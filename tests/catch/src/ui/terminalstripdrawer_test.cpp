/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "../qt_catch_tostring.h"

#include "../../../../sources/TerminalStrip/GraphicsItem/properties/terminalstriplayoutpattern.h"
#include "../../../../sources/TerminalStrip/GraphicsItem/terminalstripdrawer.h"

#include <catch2/catch.hpp>

#include <QColor>
#include <QDomDocument>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QSharedPointer>
#include <QStringList>

/*
	The drawer of a terminal strip has four places for the levels of one
	terminal, and right-aligned the levels in them by an offset of 4 minus
	the number of levels. A terminal of five levels or more made that offset
	negative, and the first level was drawn in a place read from before the
	start of the four: out of bounds. Grouping terminals has no ceiling, so
	the case is reachable from the terminal strip editor.

	Each level here counts how many times its label is asked for, which
	tells which levels get drawn. The width needs more: with the default
	pattern, the bytes before the start read as a width of almost zero, and
	the read did not show. The pattern used here keeps four wide places
	just before its first one, so the same read shows in the width too.
	The strip handed to the drawer is made of the drawer's own interfaces,
	with no project behind it.
*/

namespace
{
	using namespace TerminalStripDrawer;

	class FakeLevel : public AbstractRealTerminalInterface
	{
		public:
			explicit FakeLevel(int *drawn) : m_drawn(drawn) {}
			QString label() const override {++*m_drawn; return QStringLiteral("1");}
			bool isBridged() const override {return false;}
			AbstractBridgeInterface *bridge() const override {return nullptr;}
			QString xref() const override {return QString();}
		private:
			int *m_drawn;
	};

	class FakeTerminal : public AbstractPhysicalTerminalInterface
	{
		public:
			explicit FakeTerminal(QVector<int> &drawn) {
				for (int &count : drawn) {
					m_levels << QSharedPointer<AbstractRealTerminalInterface>(new FakeLevel(&count));
				}
			}
			QVector<QSharedPointer<AbstractRealTerminalInterface>> realTerminals() const override {
				return m_levels;
			}
		private:
			QVector<QSharedPointer<AbstractRealTerminalInterface>> m_levels;
	};

	class FakeStrip : public AbstractTerminalStripInterface
	{
		public:
			explicit FakeStrip(QVector<int> &drawn) {
				m_terminals << QSharedPointer<AbstractPhysicalTerminalInterface>(new FakeTerminal(drawn));
			}
			QString installation() const override {return QString();}
			QString location() const override {return QString();}
			QString name() const override {return QStringLiteral("X1");}
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> physicalTerminal() const override {
				return m_terminals;
			}
		private:
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> m_terminals;
	};

		//The default pattern, with four wide places left in the storage of
		//its vector just before the first one: removing from the front of
		//a QList only moves its begin, so a read before the start finds them
		//instead of bytes that happen to read as almost nothing.
	QSharedPointer<TerminalStripLayoutPattern> pattern()
	{
		auto pattern_ = QSharedPointer<TerminalStripLayoutPattern>::create();
		QVector<QRectF> rects(4, QRectF(0, 0, 1000, 1000));
		rects << pattern_->m_terminal_rect;
		rects.remove(0, 4);
		pattern_->m_terminal_rect = rects;
		return pattern_;
	}

		//How many times each level of a terminal of @levels levels is drawn
	QVector<int> drawnLevels(int levels)
	{
		QVector<int> drawn(levels, 0);
		TerminalStripDrawer::TerminalStripDrawer drawer(
					QSharedPointer<AbstractTerminalStripInterface>(new FakeStrip(drawn)),
					pattern());
		QImage image(800, 400, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		drawer.paint(&painter);
		return drawn;
	}

	qreal widthOf(int levels)
	{
		QVector<int> drawn(levels, 0);
		TerminalStripDrawer::TerminalStripDrawer drawer(
					QSharedPointer<AbstractTerminalStripInterface>(new FakeStrip(drawn)),
					pattern());
		return drawer.boundingRect().width();
	}
}

TEST_CASE("T33 — A terminal of five levels or more draws its first four, one in each place",
		  "[terminalstripdrawer]")
{
	const int levels = GENERATE(5, 6, 8);
	CAPTURE(levels);

		//Without the wide places before the start, the width check below
		//would hold even with the read before the start: say so, do not fail
	if (pattern()->m_terminal_rect.constData()[-1].width() != 1000) {
		WARN("This Qt does not keep the room before the start of a QList: "
			 "the width check does not see a read before the start");
	}

		//Every label is also asked once to measure the strip - the marker
		//tags are as long as its longest number -, drawn or not: a level
		//drawn is one asked more often than one that is not
	const int asked_when_drawn = drawnLevels(1).first();
	const QVector<int> drawn = drawnLevels(levels);
	for (int i = 0 ; i < levels ; ++i)
	{
		CAPTURE(i);
		if (i < 4) {
			CHECK(drawn.at(i) == asked_when_drawn);
		} else {
			CHECK(drawn.at(i) < asked_when_drawn);
		}
	}
	CHECK(widthOf(levels) == widthOf(4));
}

TEST_CASE("T33 — Up to four levels, every level is drawn, as before",
		  "[terminalstripdrawer]")
{
	const int levels = GENERATE(1, 2, 3, 4);
	CAPTURE(levels);

	const int asked_when_drawn = drawnLevels(1).first();
	CHECK(asked_when_drawn > 0);
	CHECK(drawnLevels(levels) == QVector<int>(levels, asked_when_drawn));

		//The header and the spacer come first: a level is the difference
	const qreal level = widthOf(2) - widthOf(1);
	CHECK(level > 0);
	CHECK(widthOf(levels) - widthOf(1) == level * (levels - 1));
}

/*
	How a terminal looks in the schematic style.

	The number of a terminal is written on a marker tag, and the tag takes
	its place in the text zone from the alignment of the layout. The number
	took the same alignment inside the tag: a short one, such as "1", sat at
	one end of it. It is now always in the middle of its tag.

	A terminal of several levels draws one column per level, side by side,
	each one shorter than the one before - the step of the levels. The
	clamping parts fill the free room of the body, and the step gave the
	first level room the others do not have: a second screw, under its xref.
	Every level now draws its parts in the band of the last level.

	Both are read on the drawn image, with the default pattern.
*/
namespace
{
	/// A strip of one terminal of @a levels levels, drawn with the default pattern
	QImage drawnTerminal(int levels)
	{
		QVector<int> drawn(levels, 0);
		TerminalStripDrawer::TerminalStripDrawer drawer(
					QSharedPointer<AbstractTerminalStripInterface>(new FakeStrip(drawn)),
					QSharedPointer<TerminalStripLayoutPattern>::create());
		QImage image(400, 320, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		drawer.paint(&painter);
		painter.end();
		return image;
	}

	/// The pixels of @a image from (@a x0, @a y0) to (@a x1, @a y1), both included, darker than @a gray
	int pixelsDarkerThan(const QImage &image, int gray, int x0, int y0, int x1, int y1)
	{
		int count = 0;
		for (int y = y0 ; y <= y1 ; ++y) {
			for (int x = x0 ; x <= x1 ; ++x) {
				if (qGray(image.pixel(x, y)) < gray) {
					++count;
				}
			}
		}
		return count;
	}

		//Where the levels begin: after the header and the spacer
	int firstLevelX(const TerminalStripLayoutPattern &pattern)
	{
		return qRound(pattern.m_header_rect.width() + pattern.m_spacer_rect.width());
	}
}

TEST_CASE("T33 — The number of a terminal is written in the middle of its marker tag",
		  "[terminalstripdrawer]")
{
	const TerminalStripLayoutPattern pattern;
	REQUIRE(pattern.m_terminals_text_orientation == Qt::Vertical);
	const QRectF rect = pattern.m_terminal_rect.at(3);
	const int x0 = firstLevelX(pattern);
	const int x1 = x0 + qRound(rect.width()) - 1;
	const QImage image = drawnTerminal(1);

		//Along the vertical text, the rows of the white tag - read near the
		//sides of the column, where the number never reaches -, then the
		//rows of the black number inside it (the outline of the tag left out)
	const int zone_top = qRound(pattern.m_terminals_text_y);
	const int zone_bottom = qRound(pattern.m_terminals_text_y + pattern.m_terminals_text_height);
	int tag_first = -1, tag_last = -1;
	for (int y = zone_top ; y <= zone_bottom ; ++y)
	{
		if (qGray(image.pixel(x0 + 3, y)) > 240 && qGray(image.pixel(x1 - 3, y)) > 240) {
			if (tag_first < 0) {
				tag_first = y;
			}
			tag_last = y;
		}
	}
	REQUIRE(tag_first >= 0);
	REQUIRE(tag_last - tag_first >= 8);

	int text_first = -1, text_last = -1;
	for (int y = tag_first ; y <= tag_last ; ++y)
	{
		if (pixelsDarkerThan(image, 100, x0 + 5, y, x1 - 5, y) > 0) {
			if (text_first < 0) {
				text_first = y;
			}
			text_last = y;
		}
	}
	REQUIRE(text_first >= 0);
	CAPTURE(tag_first, tag_last, text_first, text_last);

		//The number "1" is shorter than the tag: in the middle, the room left
		//on each side of it is the same, within a pixel or two of rounding
	const int before = text_first - tag_first;
	const int after = tag_last - text_last;
	CHECK(qAbs(before - after) <= 2);
}

TEST_CASE("T33 — Every level draws the clamping parts of the last one, not one more in its step",
		  "[terminalstripdrawer]")
{
	const int levels = GENERATE(2, 3);
	CAPTURE(levels);

	const TerminalStripLayoutPattern pattern;
	const int x0 = firstLevelX(pattern);
	const QImage image = drawnTerminal(levels);
	const qreal under_xref = pattern.m_xref_text_y + pattern.m_xref_text_height;

	int x = x0;
	for (int i = 0 ; i < levels ; ++i)
	{
		CAPTURE(i);
		const QRectF rect = pattern.m_terminal_rect.at(4 - levels + i);
		const int width = qRound(rect.width());
		const int body_bottom = qRound(rect.bottom() - qMin(6.0, rect.height() * 0.06)
									   - qMin(2.0, rect.height() * 0.03));

			//The column is the one of this level: its body has its drawing
		REQUIRE(pixelsDarkerThan(image, 100, x + 3, qRound(rect.top()) + 10, x + width - 4, body_bottom - 2) > 0);

			//Between the xref and the end of the body, nothing: the room the
			//step gives a taller level is not filled with one more screw
		CHECK(pixelsDarkerThan(image, 100, x + 3, qRound(under_xref) + 4, x + width - 4, body_bottom - 2) == 0);

		x += width;
	}
}

/*
	Where the wires of a terminal go (T64): the destination of its entry is
	written above it and the one of its exit under it, in the zone at the
	end of the connections, next to the connection. An empty destination
	writes nothing. Read on the image of one terminal of one level.
*/
namespace
{
	class DestinationLevel : public AbstractRealTerminalInterface
	{
		public:
			DestinationLevel(const QString &entry, const QString &exit) :
				m_entry(entry), m_exit(exit) {}
			QString label() const override {return QStringLiteral("1");}
			bool isBridged() const override {return false;}
			AbstractBridgeInterface *bridge() const override {return nullptr;}
			QString xref() const override {return QString();}
			QString destination(int side, const QString &format) const override {
				Q_UNUSED(format)
				return side == 0 ? m_entry : (side == 1 ? m_exit : QString());
			}
		private:
			QString m_entry;
			QString m_exit;
	};

	class OneLevelTerminal : public AbstractPhysicalTerminalInterface
	{
		public:
			explicit OneLevelTerminal(QSharedPointer<AbstractRealTerminalInterface> level) :
				m_level(level) {}
			QVector<QSharedPointer<AbstractRealTerminalInterface>> realTerminals() const override {
				return {m_level};
			}
		private:
			QSharedPointer<AbstractRealTerminalInterface> m_level;
	};

	class OneTerminalStrip : public AbstractTerminalStripInterface
	{
		public:
			OneTerminalStrip(const QString &entry, const QString &exit) {
				m_terminals << QSharedPointer<AbstractPhysicalTerminalInterface>(
								   new OneLevelTerminal(QSharedPointer<AbstractRealTerminalInterface>(
															new DestinationLevel(entry, exit))));
			}
			QString installation() const override {return QString();}
			QString location() const override {return QString();}
			QString name() const override {return QStringLiteral("X1");}
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> physicalTerminal() const override {
				return m_terminals;
			}
		private:
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> m_terminals;
	};

		//Room above the strip in the image: the entry is written above y = 0
	const int image_shift = 120;

	QImage drawnDestinations(const QString &entry, const QString &exit)
	{
		TerminalStripDrawer::TerminalStripDrawer drawer(
					QSharedPointer<AbstractTerminalStripInterface>(new OneTerminalStrip(entry, exit)),
					QSharedPointer<TerminalStripLayoutPattern>::create());
		QImage image(400, 520, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		painter.translate(0, image_shift);
		drawer.paint(&painter);
		painter.end();
		return image;
	}
}

TEST_CASE("T64 — The destination of the entry is written above the terminal, the one of the exit under it",
		  "[terminalstripdrawer]")
{
	const TerminalStripLayoutPattern pattern;
	REQUIRE(pattern.m_destination_text_length > 0);
	const QRectF rect = pattern.m_terminal_rect.at(3);
	const int x0 = firstLevelX(pattern) + 3;
	const int x1 = firstLevelX(pattern) + qRound(rect.width()) - 4;
	const int stub = qRound(pattern.m_connection_length);
	const int length = qRound(pattern.m_destination_text_length);
	const int top = image_shift + qRound(rect.top()) - stub;	//End of the connection above
	const int bottom = image_shift + qRound(rect.bottom()) + stub;	//End of the connection under

	const QImage both = drawnDestinations(QStringLiteral("K1:24"), QStringLiteral("M1:A1"));
	const QImage entry_only = drawnDestinations(QStringLiteral("K1:24"), QString());

		//Above: written next to the connection, and short enough to leave
		//the far end of the zone empty
	CHECK(pixelsDarkerThan(both, 100, x0, top - length / 2, x1, top - 2) > 0);
	CHECK(pixelsDarkerThan(both, 100, x0, top - length + 2, x1, top - length + 12) == 0);

		//Under: written next to the connection too
	CHECK(pixelsDarkerThan(both, 100, x0, bottom + 2, x1, bottom + length / 2) > 0);
	CHECK(pixelsDarkerThan(both, 100, x0, bottom + length - 12, x1, bottom + length - 2) == 0);

		//No destination, nothing written
	CHECK(pixelsDarkerThan(entry_only, 100, x0, bottom + 2, x1, bottom + length - 2) == 0);
}

/*
	The pattern of the destinations is the project's, kept in the layout of
	the plan: the drawer hands it to every level, and it survives saving.
*/
namespace
{
	class FormatSpyLevel : public DestinationLevel
	{
		public:
			explicit FormatSpyLevel(QString *seen) :
				DestinationLevel(QString(), QString()), m_seen(seen) {}
			QString destination(int side, const QString &format) const override {
				Q_UNUSED(side)
				*m_seen = format;
				return QString();
			}
		private:
			QString *m_seen;
	};

	class FormatSpyStrip : public OneTerminalStrip
	{
		public:
			explicit FormatSpyStrip(QString *seen) : OneTerminalStrip(QString(), QString()) {
				m_spy << QSharedPointer<AbstractPhysicalTerminalInterface>(
							 new OneLevelTerminal(QSharedPointer<AbstractRealTerminalInterface>(
													  new FormatSpyLevel(seen))));
			}
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> physicalTerminal() const override {
				return m_spy;
			}
		private:
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> m_spy;
	};
}

TEST_CASE("T64 — The plan writes the destinations with the pattern of the project, which is saved with it",
		  "[terminalstripdrawer]")
{
	auto pattern = QSharedPointer<TerminalStripLayoutPattern>::create();
	pattern->m_destination_format = QStringLiteral("%tag (%folio) %wire");
	pattern->m_destination_text_length = 90;

	SECTION("the drawer hands the pattern to the levels")
	{
		QString seen;
		TerminalStripDrawer::TerminalStripDrawer drawer(
					QSharedPointer<AbstractTerminalStripInterface>(new FormatSpyStrip(&seen)), pattern);
		QImage image(400, 520, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		drawer.paint(&painter);
		painter.end();
		CHECK(seen == QStringLiteral("%tag (%folio) %wire"));
	}

	SECTION("the pattern and the length survive the project file")
	{
		QDomDocument document;
		document.appendChild(pattern->toXml(document));
		TerminalStripLayoutPattern read;
		read.fromXml(document.documentElement());
		CHECK(read.m_destination_format == QStringLiteral("%tag (%folio) %wire"));
		CHECK(read.m_destination_text_length == 90);
	}
}

/*
	Every destination of a strip is written in the same size, the one that
	lets the longest fit in its zone, like the numbers on the marker tags.
	Each one used to be reduced alone: "Free" came out bigger than the
	destinations with a folio, and on screen one that just fitted lost its
	end - its ")". A destination much longer than the others is reduced alone, so
	it does not take the whole strip down with it.
	Read on the height of the first letter of each destination, which lies
	across the column of its terminal: the same letter in all of them, and
	the light gray of its edges counted, because the destinations are not
	hinted and a thin stem is gray on the pixels it straddles.
*/
namespace
{
	class EntriesStrip : public AbstractTerminalStripInterface
	{
		public:
			explicit EntriesStrip(const QStringList &entries) {
				for (const auto &entry : entries) {
					m_terminals << QSharedPointer<AbstractPhysicalTerminalInterface>(
									   new OneLevelTerminal(QSharedPointer<AbstractRealTerminalInterface>(
																new DestinationLevel(entry, QString()))));
				}
			}
			QString installation() const override {return QString();}
			QString location() const override {return QString();}
			QString name() const override {return QStringLiteral("X1");}
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> physicalTerminal() const override {
				return m_terminals;
			}
		private:
			QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> m_terminals;
	};

	QImage drawnEntries(const QStringList &entries)
	{
		TerminalStripDrawer::TerminalStripDrawer drawer(
					QSharedPointer<AbstractTerminalStripInterface>(new EntriesStrip(entries)),
					QSharedPointer<TerminalStripLayoutPattern>::create());
		QImage image(400, 520, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		painter.translate(0, image_shift);
		drawer.paint(&painter);
		painter.end();
		return image;
	}

		/// Across the column of terminal @a index, the width of the ink of the
		/// first letter of its entry, next to the end of its connection
	int firstLetterHeight(const QImage &image, int index)
	{
		const TerminalStripLayoutPattern pattern;
		const QRectF rect = pattern.m_terminal_rect.at(3);
		const int x0 = firstLevelX(pattern) + index * qRound(rect.width()) + 1;
		const int x1 = x0 + qRound(rect.width()) - 3;
		const int end = image_shift + qRound(rect.top() - pattern.m_connection_length);
		int first = -1, last = -1;
		for (int y = end - 5 ; y <= end - 2 ; ++y) {
			for (int x = x0 ; x <= x1 ; ++x) {
				if (qGray(image.pixel(x, y)) < 200) {
					first = first < 0 ? x : qMin(first, x);
					last = qMax(last, x);
				}
			}
		}
		return first < 0 ? 0 : last - first + 1;
	}
}

TEST_CASE("T64 — One size for the destinations of a strip", "[terminalstripdrawer]")
{
	const TerminalStripLayoutPattern pattern;
	const QString free_ = QStringLiteral("Livre");
	const QString folio_ = QStringLiteral("L1:24 (Folha 1) 105");
	const QString long_ = QStringLiteral("L1:24 (Folha 1) 105 / L2:13 (Folha 2) 106 / L3:13 (Folha 3) 107");

		//The destination with the folio has to be reduced, the free one not:
		//the zone keeps one unit at each end, and two more as a slack
	const QFontMetricsF metrics(pattern.font());
	const qreal room = pattern.m_destination_text_length - 4;
	CAPTURE(metrics.horizontalAdvance(folio_), metrics.horizontalAdvance(free_), room);
	REQUIRE(metrics.horizontalAdvance(folio_) > room + 4);
	REQUIRE(metrics.horizontalAdvance(free_) < room - 4);

	SECTION("the free terminal is written in the size of the reduced one")
	{
		const QImage image = drawnEntries({free_, folio_});
		const int free_height = firstLetterHeight(image, 0);
		const int folio_height = firstLetterHeight(image, 1);
		CAPTURE(free_height, folio_height);
		REQUIRE(folio_height >= 4);
		CHECK(qAbs(free_height - folio_height) <= 1);
	}

	SECTION("a much longer destination is reduced alone")
	{
		const QImage image = drawnEntries({free_, folio_, long_});
		const int free_height = firstLetterHeight(image, 0);
		const int folio_height = firstLetterHeight(image, 1);
		const int long_height = firstLetterHeight(image, 2);
		CAPTURE(free_height, folio_height, long_height);
		REQUIRE(folio_height >= 4);
		CHECK(qAbs(free_height - folio_height) <= 1);
		CHECK(long_height < folio_height - 1);
	}
}
