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
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/conductorsegment.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/conductortextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/arrangeconductorscommand.h"
#include "../../../../sources/wirearrangement.h"

#include <catch2/catch.hpp>

#include <QFontMetricsF>
#include <QPainterPathStroker>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QUndoStack>

#include <algorithm>
#include <cmath>

/*
	The name of a wire on the longest run it fits without touching anything
	- another wire, the name of another wire, the side of a frame -, and on
	the longest run when there is none (T71).

	The folio has, from top to bottom:

	- a staircase: four wires from components on the left, 20 apart, to
	  components on the right, 20 apart and 200 lower; their vertical runs,
	  200 long and the longest of each wire, 12 apart; their horizontal
	  runs 110 to 170 long. The letters of a name start some 7 off its own
	  wire - the margin of the text document, and the push off the wire -
	  so the wire 12 away runs through them: closer, the next name would
	  touch first, and which one moves would depend on which is placed
	  first;
	- a wire alone, its longest run free;
	- seven wires 4 apart on every run, so that no run of the middle one
	  is free on the side its name is written;
	- a wire whose longest run is crossed by the side of a frame, and one
	  that lies whole inside another frame;
	- two wires whose longest runs are 26 apart, the name of the upper one
	  written below it and the one of the lower one above it: the letters
	  of the two names cover each other, and no wire runs through them.

	Whether a name touches is measured here, on the letters of the name -
	the ink of each line from its baseline - against the strokes of the
	other wires and the letters of the other names: not by asking the code
	that decides, which is the thing measured.
*/
namespace
{
	QString definition(const char *name, const QString &terminal)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), terminal);
	}

	struct Instance
	{
		const char *definition;
		int x;
		int y;
		QString label;
	};

		/// The dock of a terminal is the component's position plus 10
		/// towards the side it faces
	QList<Instance> instances()
	{
		QList<Instance> list;
		for (int k = 0 ; k < 4 ; ++k)
		{
			list << Instance{"east.elmt", 100, 100 + 20 * k, QStringLiteral("S%1").arg(k)}
			     << Instance{"west.elmt", 400, 300 + 20 * k, QStringLiteral("R%1").arg(k)};
		}
		list << Instance{"east.elmt", 100, 600, QStringLiteral("L0")}
		     << Instance{"west.elmt", 300, 640, QStringLiteral("L1")};
		for (int i = 0 ; i < 7 ; ++i)
		{
			list << Instance{"east.elmt", 100, 800 + 4 * i, QStringLiteral("P%1").arg(i)}
			     << Instance{"west.elmt", 400, 1000 + 4 * i, QStringLiteral("Q%1").arg(i)};
		}
		list << Instance{"east.elmt", 100, 1200, QStringLiteral("B0")}
		     << Instance{"west.elmt", 400, 1210, QStringLiteral("B1")}
		     << Instance{"east.elmt", 100, 1420, QStringLiteral("C0")}
		     << Instance{"west.elmt", 400, 1440, QStringLiteral("C1")}
		     << Instance{"east.elmt", 100, 1600, QStringLiteral("D0")}
		     << Instance{"west.elmt", 400, 1640, QStringLiteral("E0")}
		     << Instance{"east.elmt", 100, 1626, QStringLiteral("D1")}
		     << Instance{"west.elmt", 400, 1700, QStringLiteral("E1")}
		     << Instance{"east.elmt", 100, 1880, QStringLiteral("Y0")}
		     << Instance{"west.elmt", 400, 2120, QStringLiteral("Y1")}
		     << Instance{"east.elmt", 100, 1900, QStringLiteral("X0")}
		     << Instance{"west.elmt", 400, 2100, QStringLiteral("X1")};
		return list;
	}

	QString fixtureXml()
	{
		QString xml;
		const QList<Instance> list = instances();
		for (int i = 0 ; i < list.size() ; ++i)
		{
			const Instance &instance = list.at(i);
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"0\" type=\"embed://bench/%3\""
				" uuid=\"{71000000-0000-4000-8000-0000000000%4}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(instance.x).arg(instance.y)
				.arg(QLatin1String(instance.definition))
				.arg(i, 2, 10, QLatin1Char('0'))
				.arg(instance.label);
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"2240\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"28\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definition("east.elmt",
					QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"))
				 + definition("west.elmt",
					      QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>")),
			     xml);
	}

	Terminal *terminalOf(Diagram *sheet, const QString &label)
	{
		const auto elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->actualLabel() == label && !element->terminals().isEmpty()) {
				return element->terminals().first();
			}
		}
		return nullptr;
	}

		/// A conductor between @a from and @a to, added to the folio. Not
		/// drawn by mouse: the terminals of the case are 4 apart, and the
		/// release would find the neighbour
	Conductor *draw(Diagram *sheet, Terminal *from, Terminal *to)
	{
		auto *c = new Conductor(from, to);
		sheet->addItem(c);
		return c;
	}

		/// A name on @a c, at the size 9, written along its runs, below a
		/// horizontal one and to the right of a vertical one
	void name(Conductor *c, const QString &text)
	{
		ConductorProperties properties = c->properties();
		properties.type = ConductorProperties::Multi;
		properties.m_formula.clear();
		properties.text = text;
		properties.text_size = 9;
		properties.m_show_text = true;
		properties.m_one_text_per_folio = false;
		properties.horiz_rotate_text = 0;
		properties.verti_rotate_text = 270;
		properties.m_horizontal_alignment = Qt::AlignBottom;
		properties.m_vertical_alignment = Qt::AlignRight;
		c->setProperties(properties);
	}

		/// The conductor drawn from @a from to @a to, given @a path in scene
		/// coordinates when there is one, and named @a text
	Conductor *wire(Diagram *sheet, const QString &from, const QString &to,
			const QList<QPointF> &path, const QString &text)
	{
		Terminal *start = terminalOf(sheet, from);
		Terminal *end = terminalOf(sheet, to);
		if (!start || !end) return nullptr;
		Conductor *c = draw(sheet, start, end);
		if (!c) return nullptr;
		if (!path.isEmpty() && !c->setPathPoints(path)) return nullptr;
		name(c, text);
		return c;
	}

		/// The letters of the name of @a c, in scene coordinates: the ink
		/// of each line, from its baseline
	QRectF ink(const Conductor *c)
	{
		const ConductorTextItem *text = c->textItem();
		const QTextDocument *document = text->document();
		const QFontMetricsF metrics(document->defaultFont());
		QRectF letters;
		for (QTextBlock block = document->begin() ; block.isValid() ; block = block.next())
		{
			const QTextLayout *layout = block.layout();
			for (int i = 0 ; layout && i < layout->lineCount() ; ++i)
			{
				const QTextLine line = layout->lineAt(i);
				const QString part = block.text().mid(line.textStart(), line.textLength());
				if (part.trimmed().isEmpty()) continue;
				const QRectF one = metrics.tightBoundingRect(part).translated(
							layout->position() + QPointF(line.x(), line.y() + line.ascent()));
				letters = letters.isNull() ? one : letters.united(one);
			}
		}
		return text->mapRectToScene(letters);
	}

		/// Whether the name of @a c touches another wire or another name of @a all
	bool touches(const Conductor *c, const QList<Conductor *> &all)
	{
		const QRectF letters = ink(c).adjusted(-0.5, -0.5, 0.5, 0.5);
		for (const Conductor *other : all)
		{
			if (other == c) continue;
			QPainterPathStroker stroker;
			stroker.setWidth(1);
			if (stroker.createStroke(other->mapToScene(other->path())).intersects(letters))
				return true;
			if (ink(other).intersects(letters))
				return true;
		}
		return false;
	}

		/// Whether the name of @a c is written along a horizontal run
	bool horizontal(const Conductor *c)
	{
		return std::fmod(std::abs(c->textItem()->rotation()), 180.) < 1;
	}

		/// The length of the longest segment of @a c, and whether it is vertical
	qreal longestRun(const Conductor *c, bool *vertical = nullptr)
	{
		qreal longest = 0;
		const auto segments = c->segmentsList();
		for (ConductorSegment *s : segments)
		{
			if (std::abs(s->length()) > longest)
			{
				longest = std::abs(s->length());
				if (vertical) *vertical = s->isVertical();
			}
		}
		return longest;
	}

		/// Every name of @a sheet placed again, as the folio does once built
	void placeAll(Diagram *sheet)
	{
		const QList<Conductor *> all = sheet->conductors();
		for (Conductor *c : all) {
			c->calculateTextItemPosition();
		}
	}

	struct Folio
	{
		Folio() : project(fixtureXml(), QStringLiteral("wirenameroom.qet")) {}

		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			if (!sheet) return false;
				//From the last wire to the first: the wire that decides where
				//the name of the next one goes - the one on the side it is
				//written - comes after it in the file, and a folio opened
				//places that name before the wire is there
			for (int k = 3 ; k >= 0 ; --k)
			{
				const qreal bridge = 280 - 12 * k;
				const qreal from = 100 + 20 * k;
				const qreal to = 300 + 20 * k;
				Conductor *c = wire(sheet, QStringLiteral("S%1").arg(k), QStringLiteral("R%1").arg(k),
						    {{110, from}, {bridge, from}, {bridge, to}, {390, to}},
						    QStringLiteral("F1-%1").arg(k + 1));
				if (!c) return false;
				staircase.prepend(c);
			}
			alone = wire(sheet, QStringLiteral("L0"), QStringLiteral("L1"),
				     {{110, 600}, {150, 600}, {150, 640}, {290, 640}}, QStringLiteral("F1-5"));
			for (int i = 0 ; i < 7 ; ++i)
			{
				const qreal from = 800 + 4 * i;
				const qreal to = 1000 + 4 * i;
				const qreal bridge = 250 - 4 * i;
				Conductor *c = wire(sheet, QStringLiteral("P%1").arg(i), QStringLiteral("Q%1").arg(i),
						    {{110, from}, {bridge, from}, {bridge, to}, {390, to}},
						    QStringLiteral("F2-%1").arg(i + 1));
				if (!c) return false;
				nested << c;
			}
			crossed = wire(sheet, QStringLiteral("B0"), QStringLiteral("B1"),
				       {{110, 1200}, {300, 1200}, {300, 1210}, {390, 1210}}, QStringLiteral("F1-8"));
			framed = wire(sheet, QStringLiteral("C0"), QStringLiteral("C1"),
				      {{110, 1420}, {300, 1420}, {300, 1440}, {390, 1440}}, QStringLiteral("F1-9"));
			upper = wire(sheet, QStringLiteral("D0"), QStringLiteral("E0"),
				     {{110, 1600}, {300, 1600}, {300, 1640}, {390, 1640}}, QStringLiteral("F1-11"));
			lower = wire(sheet, QStringLiteral("D1"), QStringLiteral("E1"),
				     {{110, 1626}, {280, 1626}, {280, 1700}, {390, 1700}}, QStringLiteral("F1-12"));
			if (!alone || !crossed || !framed || !upper || !lower) return false;
				//The name of the lower one above its runs
			ConductorProperties above = lower->properties();
			above.m_horizontal_alignment = Qt::AlignTop;
			lower->setProperties(above);

				//A frame whose left side crosses the longest run of the
				//crossed wire, and one around the whole framed wire
			sheet->addItem(new QetShapeItem(QPointF(200, 1150), QPointF(500, 1300),
							QetShapeItem::Rectangle));
			sheet->addItem(new QetShapeItem(QPointF(50, 1380), QPointF(500, 1480),
							QetShapeItem::Rectangle));
			placeAll(sheet);
			return true;
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		QList<Conductor *> staircase;
		Conductor *alone = nullptr;
		QList<Conductor *> nested;
		Conductor *crossed = nullptr;
		Conductor *framed = nullptr;
		Conductor *upper = nullptr;
		Conductor *lower = nullptr;
	};

		/// Drawing and arranging set the way the request was measured, and
		/// put back at the end
	class NoAutomaticArrangement
	{
		public:
			NoAutomaticArrangement() : m_arrange(WireArrangement::arrangesAutomatically())
			{
				WireArrangement::setArrangesAutomatically(false);
			}
			~NoAutomaticArrangement() {WireArrangement::setArrangesAutomatically(m_arrange);}
		private:
			bool m_arrange;
	};
}

TEST_CASE("CU-71.1 — The name leaves the staircase", "[T71][wirename]")
{
	NoAutomaticArrangement setting;
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());
	const QList<Conductor *> all = folio.sheet->conductors();

	for (int k = 0 ; k < folio.staircase.size() ; ++k)
	{
		Conductor *c = folio.staircase.at(k);
		INFO(c->properties().text.toStdString());
			//The fixture is what it says: the longest run is the vertical
			//one, 200 long, where the name went until now
		bool vertical = false;
		REQUIRE(longestRun(c, &vertical) == Approx(200));
		REQUIRE(vertical);
		CHECK_FALSE(touches(c, all));
			//The name of a vertical run is written on its right: the
			//first wire has no neighbour there, and its name stays where
			//it was; the other three left the run for a horizontal one
		if (k == 0) {
			CHECK_FALSE(horizontal(c));
		} else {
			CHECK(horizontal(c));
		}
	}
}

TEST_CASE("CU-71.2 — Where nothing touches, nothing moves", "[T71][wirename]")
{
	NoAutomaticArrangement setting;
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());
	Conductor *c = folio.alone;

		//On the longest run, the last one, 140 long and horizontal, at
		//its middle as before
	CHECK(horizontal(c));
	const QRectF letters = ink(c);
	CHECK(letters.center().x() == Approx(220).margin(2));
	CHECK(letters.top() >= 640);
	CHECK(letters.top() < 660);
	CHECK_FALSE(touches(c, folio.sheet->conductors()));
}

TEST_CASE("CU-71.3 — No run will do", "[T71][wirename]")
{
	NoAutomaticArrangement setting;
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());

		//Every run of the middle wire has neighbours 4, 8 and 12 away on
		//the side its name is written: the name stays on its longest run,
		//the vertical one, as until now
	Conductor *c = folio.nested.at(3);
	REQUIRE(c->properties().text == QStringLiteral("F2-4"));
	CHECK(touches(c, folio.sheet->conductors()));
	CHECK_FALSE(horizontal(c));
	const QRectF letters = ink(c);
	CHECK(letters.center().y() > 812);
	CHECK(letters.center().y() < 1012);
	CHECK(letters.left() > 238);
}

TEST_CASE("CU-71.4 — The side of a frame", "[T71][wirename]")
{
	NoAutomaticArrangement setting;
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());

		//The longest run, 190 long, crosses the left side of the frame at
		//x 200, where the name would sit: it goes to the next run, 90
		//long, inside the frame
	CHECK(horizontal(folio.crossed));
	const QRectF crossed = ink(folio.crossed);
	CHECK(crossed.top() >= 1210);
	CHECK(crossed.left() > 300);

		//Inside a frame, crossing no side: the longest run, as until now
	CHECK(horizontal(folio.framed));
	const QRectF framed = ink(folio.framed);
	CHECK(framed.top() >= 1420);
	CHECK(framed.top() < 1440);
	CHECK(framed.center().x() == Approx(205).margin(2));
}

TEST_CASE("CU-71.5 — A name moved by hand", "[T71][wirename]")
{
	NoAutomaticArrangement setting;
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());

		//Put back by hand on the vertical run of the second wire of the
		//staircase, where it touches the first one
	Conductor *c = folio.staircase.at(1);
	ConductorTextItem *text = c->textItem();
	text->setRotation(270);
	text->setPos(268, 200);
	text->forceMovedByUser(true);
	const QPointF placed = text->pos();

	placeAll(folio.sheet);
	CHECK(text->pos() == placed);
	CHECK_FALSE(horizontal(c));
}

TEST_CASE("CU-71.6 — Undoing the arrangement", "[T71][wirename]")
{
	NoAutomaticArrangement setting;
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("wirenameroom-undo.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	REQUIRE(sheet != nullptr);

		//The staircase drawn with the automatic paths: the four bridges on
		//one line, half way, which the arrangement spreads
	QList<Conductor *> wires;
	for (int k = 0 ; k < 4 ; ++k)
	{
		Conductor *c = wire(sheet, QStringLiteral("S%1").arg(k), QStringLiteral("R%1").arg(k),
				    {}, QStringLiteral("F1-%1").arg(k + 1));
		REQUIRE(c != nullptr);
		wires << c;
	}
	placeAll(sheet);
	QList<QPointF> before;
	QList<qreal> turned;
	for (Conductor *c : std::as_const(wires))
	{
		before << c->textItem()->pos();
		turned << c->textItem()->rotation();
	}

	QUndoStack &stack = sheet->undoStack();
	const int index = stack.index();
	REQUIRE(WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio) > 0);
	REQUIRE(stack.index() == index + 1);
		//Arranged: no name touches
	const QList<Conductor *> all = sheet->conductors();
	for (Conductor *c : std::as_const(wires))
	{
		INFO(c->properties().text.toStdString());
		CHECK_FALSE(touches(c, all));
	}

		//Undone: the paths and the names where they were
	stack.undo();
	for (int k = 0 ; k < wires.size() ; ++k)
	{
		INFO(wires.at(k)->properties().text.toStdString());
		CHECK(wires.at(k)->textItem()->pos() == before.at(k));
		CHECK(wires.at(k)->textItem()->rotation() == turned.at(k));
	}
}

TEST_CASE("T71 — The names of a folio opened again", "[T71][wirename]")
{
	/*
		Opened, a folio places each name as its wire is added, before the
		wires and the frames after it are there: the names are placed
		again once the folio is whole.
	*/
	NoAutomaticArrangement setting;
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());
	REQUIRE(folio.project.saveAndReopen());
	INFO(folio.project.error().toStdString());
	Diagram *sheet = folio.project.diagram(0);
	REQUIRE(sheet != nullptr);
	const QList<Conductor *> all = sheet->conductors();
	const auto named = [&all](const QString &text) -> Conductor * {
		for (Conductor *c : all) {
			if (c->properties().text == text) return c;
		}
		return nullptr;
	};

	for (int k = 1 ; k < 4 ; ++k)
	{
		Conductor *c = named(QStringLiteral("F1-%1").arg(k + 1));
		REQUIRE(c != nullptr);
		INFO(c->properties().text.toStdString());
		CHECK(horizontal(c));
		CHECK_FALSE(touches(c, all));
	}
	Conductor *crossed = named(QStringLiteral("F1-8"));
	REQUIRE(crossed != nullptr);
	CHECK(horizontal(crossed));
	CHECK(ink(crossed).left() > 300);
}

TEST_CASE("T71 — Two names that would cover each other", "[T71][wirename]")
{
	NoAutomaticArrangement setting;
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());

		//On their longest runs the two names would cover each other: one
		//of them went to another run, and neither touches anything
	const QList<Conductor *> all = folio.sheet->conductors();
	CHECK_FALSE(touches(folio.upper, all));
	CHECK_FALSE(touches(folio.lower, all));
	CHECK_FALSE(ink(folio.upper).intersects(ink(folio.lower)));
}

TEST_CASE("T71 — A wire arranged next to a name", "[T71][wirename]")
{
	/*
		The arrangement moves a wire next to the name of a wire it does not
		move: the name is placed again at the end of the arrangement, and
		back where it was on undo. Y keeps its path; X is given one whose
		vertical run passes through the letters of the name of Y.
	*/
	NoAutomaticArrangement setting;
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("wirenameroom-next.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	REQUIRE(sheet != nullptr);
	Conductor *y = wire(sheet, QStringLiteral("Y0"), QStringLiteral("Y1"),
			    {{110, 1880}, {200, 1880}, {200, 2120}, {390, 2120}}, QStringLiteral("F3-1"));
	Conductor *x = wire(sheet, QStringLiteral("X0"), QStringLiteral("X1"),
			    {{110, 1900}, {350, 1900}, {350, 2100}, {390, 2100}}, QStringLiteral("F3-2"));
	REQUIRE((x && y));
	placeAll(sheet);
		//The name of Y on its vertical run, the longest, nothing near it
	REQUIRE_FALSE(horizontal(y));
	const QPointF before = y->textItem()->pos();

	WireArrangement::Change change;
	change.conductor = x;
	change.points = {{110, 1900}, {212, 1900}, {212, 2100}, {390, 2100}};
	QUndoStack &stack = sheet->undoStack();
	stack.push(new ArrangeConductorsCommand(sheet, {change}));
	REQUIRE(x->segmentsList().size() >= 3);

		//X runs through the letters the name of Y had there: it left
	CHECK(horizontal(y));
	CHECK_FALSE(touches(y, sheet->conductors()));

	stack.undo();
	CHECK_FALSE(horizontal(y));
	CHECK(y->textItem()->pos() == before);
}

TEST_CASE("T71 — A folio built places its names again", "[T71][wirename]")
{
	/*
		A folio being built adds its wires one by one, and each one places
		its name as it comes, with only the wires before it there: the
		second wire of the staircase, added first, keeps its name on its
		vertical run, and the first one, added after, runs through it.
		Built, the folio places every name again.
	*/
	NoAutomaticArrangement setting;
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("wirenameroom-built.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	REQUIRE(sheet != nullptr);
	Conductor *second = wire(sheet, QStringLiteral("S1"), QStringLiteral("R1"),
				 {{110, 120}, {268, 120}, {268, 320}, {390, 320}}, QStringLiteral("F1-2"));
	Conductor *first = wire(sheet, QStringLiteral("S0"), QStringLiteral("R0"),
				{{110, 100}, {280, 100}, {280, 300}, {390, 300}}, QStringLiteral("F1-1"));
	REQUIRE((first && second));
		//The scenario is what it says: the name of the second wire was
		//placed before the first one was there, and the first one runs
		//through it now
	REQUIRE_FALSE(horizontal(second));
	REQUIRE(touches(second, sheet->conductors()));

	sheet->refreshContents();
	CHECK(horizontal(second));
	CHECK_FALSE(touches(second, sheet->conductors()));
}
