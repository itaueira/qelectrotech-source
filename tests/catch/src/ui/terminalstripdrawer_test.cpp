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

#include <QImage>
#include <QPainter>
#include <QSharedPointer>

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
