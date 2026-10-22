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

#include "../../../../sources/conductorsegment.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/elementsmover.h"
#include "../../../../sources/foliogrid.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/conductortextitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/wirearrangement.h"
#include "../../../../sources/wirepath.h"

#include <catch2/catch.hpp>

#include <QElapsedTimer>
#include <QGraphicsSceneMouseEvent>
#include <QSettings>
#include <QTextDocument>
#include <QUndoStack>

#include <algorithm>
#include <cmath>

/*
	A new conductor arranges itself, and its group with it (T67). The
	bench is the folio the request was measured on: two sensors with three
	terminals each, inside a frame drawn up to x = 215 with a pen 1 wide,
	wired to six terminals whose ends are at x = 270, the terminals being
	in a frame from 282. The automatic path puts every bridge at 225, half
	way between the terminal ends, on top of each other.

	The conductors are drawn as the user draws them: a press on a terminal
	and a release on another, sent to the terminal, which is what makes
	Terminal::mouseReleaseEvent() add the conductor.
*/
namespace
{
	QString definition(const char *name, const QString &terminals, int height)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"%3\" hotspot_x=\"10\" hotspot_y=\"%4\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-%5\" width=\"16\" height=\"%6\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), terminals)
			.arg(height).arg(height / 2).arg(height / 2 - 2).arg(height - 4);
	}

		/// The label of the element, drawn from (@a x, @a y) of the symbol
	QString labelAt(int x, int y)
	{
		return QStringLiteral(
			"<dynamic_text text_from=\"ElementInfo\" x=\"%1\" y=\"%2\" z=\"3\" rotation=\"0\""
			" frame=\"false\" text_width=\"-1\" font=\"Sans Serif,9,-1,5,50,0,0,0,0,0\""
			" uuid=\"{67000000-0000-4000-8000-0000000000aa}\" Halignment=\"AlignLeft\""
			" Valignment=\"AlignTop\" keep_visual_rotation=\"false\""
			" rotation_point_center=\"false\"><text/><info_name>label</info_name></dynamic_text>")
			.arg(x).arg(y);
	}

		/// A component 60 wide and 80 high, its terminal @a terminal
	QString wideDefinition(const char *name, const QString &terminal)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"60\" height=\"80\" hotspot_x=\"30\" hotspot_y=\"40\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-28\" y=\"-38\" width=\"56\" height=\"76\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), terminal);
	}

		/// A component 50 long with a terminal at each end, 5 inside its
		/// box, as the valves of the request: standing, its terminals
		/// pointing up and down, or lying, pointing left and right
	QString stackDefinition(const char *name, bool lying)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"%2\" height=\"%3\" hotspot_x=\"%4\" hotspot_y=\"%5\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"%6\" y=\"%7\" width=\"%8\" height=\"%9\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%10"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name))
			.arg(lying ? 50 : 20).arg(lying ? 20 : 50).arg(lying ? 25 : 10).arg(lying ? 10 : 25)
			.arg(lying ? -15 : -8).arg(lying ? -8 : -15).arg(lying ? 30 : 16).arg(lying ? 16 : 30)
			.arg(lying ? QStringLiteral("<terminal x=\"-20\" y=\"0\" orientation=\"w\"/>"
						   "<terminal x=\"20\" y=\"0\" orientation=\"e\"/>")
				   : QStringLiteral("<terminal x=\"0\" y=\"-20\" orientation=\"n\"/>"
						   "<terminal x=\"0\" y=\"20\" orientation=\"s\"/>"));
	}

		/// A terminal of a strip 40 long whose end is 8 from its middle,
		/// pointing @a way (e, w, n or s): the body goes 12 past the end,
		/// as the body of a terminal of a strip on the folio goes past it
	QString deepDefinition(const char *name, char way)
	{
		const bool lying = way == 'e' || way == 'w';
		const int x = way == 'e' ? 8 : way == 'w' ? -8 : 0;
		const int y = way == 's' ? 8 : way == 'n' ? -8 : 0;
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"%2\" height=\"%3\" hotspot_x=\"%4\" hotspot_y=\"%5\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"%6\" y=\"%7\" width=\"%8\" height=\"%9\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"%10\" y=\"%11\" orientation=\"%12\"/>"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name))
			.arg(lying ? 40 : 20).arg(lying ? 20 : 40).arg(lying ? 20 : 10).arg(lying ? 10 : 20)
			.arg(lying ? -18 : -8).arg(lying ? -8 : -18).arg(lying ? 36 : 16).arg(lying ? 16 : 36)
			.arg(x).arg(y).arg(QLatin1Char(way));
	}

	QString definitions()
	{
		return definition("sensor.elmt",
				  QStringLiteral("<terminal x=\"10\" y=\"-13\" orientation=\"e\"/>"
						 "<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"
						 "<terminal x=\"10\" y=\"13\" orientation=\"e\"/>"),
				  40)
			+ definition("terminal.elmt",
				     QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>"), 20)
			+ definition("south.elmt",
				     QStringLiteral("<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"), 20)
			+ definition("north.elmt",
				     QStringLiteral("<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"), 20)
			+ definition("east.elmt",
				     QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"), 20)
				//A terminal that sticks out of its component by 10
			+ definition("pin.elmt",
				     QStringLiteral("<terminal x=\"20\" y=\"0\" orientation=\"e\"/>"), 20)
			+ definition("door.elmt",
				     QStringLiteral("<terminal x=\"0\" y=\"-30\" orientation=\"n\"/>"
						    "<terminal x=\"0\" y=\"30\" orientation=\"s\"/>"), 60)
			+ definition("block.elmt", QString(), 20)
				//A component pointing down whose label is drawn beside it,
				//down to the left, as the valves of the request show theirs;
				//and a component with no terminal that draws its label there
			+ definition("valve.elmt",
				     QStringLiteral("<terminal x=\"0\" y=\"10\" orientation=\"s\"/>") + labelAt(-130, 52), 20)
			+ definition("note.elmt", labelAt(-130, 92), 20)
				//A component as wide as the signal of the request, its
				//terminal on its edge pointing out: to the right, to the
				//left, and up
			+ wideDefinition("wide_e.elmt", QStringLiteral("<terminal x=\"30\" y=\"-20\" orientation=\"e\"/>"))
			+ wideDefinition("wide_w.elmt", QStringLiteral("<terminal x=\"-30\" y=\"-20\" orientation=\"w\"/>"))
			+ wideDefinition("wide_n.elmt", QStringLiteral("<terminal x=\"10\" y=\"-40\" orientation=\"n\"/>"))
			+ stackDefinition("stack_v.elmt", false)
			+ stackDefinition("stack_h.elmt", true)
			+ deepDefinition("deep_e.elmt", 'e')
			+ deepDefinition("deep_w.elmt", 'w')
			+ deepDefinition("deep_n.elmt", 'n')
			+ deepDefinition("deep_s.elmt", 's');
	}

	struct Instance
	{
		const char *definition;
		int x;
		int y;
		const char *label;
	};

	QString fixtureXml(const QList<Instance> &instances, const QString &shapes);

	QString fixtureXml(const QList<Instance> &instances, bool frames)
	{
		return fixtureXml(instances, frames
			? QStringLiteral("<shapes>"
					 "<shape type=\"Rectangle\" x1=\"75\" y1=\"120\" x2=\"215\" y2=\"615\""
					 " is_movable=\"1\" closed=\"0\" z=\"5\">"
					 "<pen color=\"#000000\" style=\"DashLine\" widthF=\"1\"/></shape>"
					 "<shape type=\"Rectangle\" x1=\"282\" y1=\"122\" x2=\"292\" y2=\"706\""
					 " is_movable=\"1\" closed=\"0\" z=\"5\">"
					 "<pen color=\"#000000\" style=\"DashLine\" widthF=\"1\"/></shape>"
					 "</shapes>")
			: QString());
	}

	QString fixtureXml(const QList<Instance> &instances, const QString &shapes)
	{
		QString xml;
		for (int i = 0 ; i < instances.size() ; ++i)
		{
			const Instance &instance = instances.at(i);
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"0\" type=\"embed://bench/%3\""
				" uuid=\"{67000000-0000-4000-8000-0000000000%4}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(instance.x).arg(instance.y)
				.arg(QLatin1String(instance.definition))
				.arg(i, 2, 10, QLatin1Char('0'))
				.arg(QLatin1String(instance.label));
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"800\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"10\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>%3"
			"</diagram>"
			"</project>")
			.arg(definitions(), xml, shapes);
	}

		/// The two sensors and the six terminals of the folio
	QList<Instance> folio()
	{
		return {{"sensor.elmt", 157, 168, "S1"},
			{"sensor.elmt", 157, 218, "S2"},
			{"terminal.elmt", 287, 155, "T0"},
			{"terminal.elmt", 287, 178, "T1"},
			{"terminal.elmt", 287, 201, "T2"},
			{"terminal.elmt", 287, 224, "T3"},
			{"terminal.elmt", 287, 247, "T4"},
			{"terminal.elmt", 287, 270, "T5"}};
	}

	Element *tagged(Diagram *sheet, const char *label)
	{
		const auto elements = sheet->elements();
		for (Element *element : elements) {
			if (element->actualLabel() == QLatin1String(label)) return element;
		}
		return nullptr;
	}

		/// The grid of the folio and the setting, as on the machine the
		/// request came from, put back at the end
	class Setup
	{
		public:
			explicit Setup(int grid = 1, bool arrange = true) :
				m_x(Diagram::xGrid), m_y(Diagram::yGrid),
				m_arrange(WireArrangement::arrangesAutomatically())
			{
				Diagram::xGrid = grid;
				Diagram::yGrid = grid;
				WireArrangement::setArrangesAutomatically(arrange);
			}
			~Setup()
			{
				Diagram::xGrid = m_x;
				Diagram::yGrid = m_y;
				WireArrangement::setArrangesAutomatically(m_arrange);
			}
		private:
			qreal m_x;
			qreal m_y;
			bool m_arrange;
	};

		/// A press on @a from and a release on @a to, as the user draws
	Conductor *draw(Diagram *sheet, Terminal *from, Terminal *to)
	{
		const QList<Conductor *> before = sheet->conductors();
		QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
		press.setScenePos(from->dockConductor());
		press.setButton(Qt::LeftButton);
		press.setButtons(Qt::LeftButton);
		sheet->sendEvent(from, &press);
		QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
		release.setScenePos(to->dockConductor());
		release.setButton(Qt::LeftButton);
		sheet->sendEvent(from, &release);
		const QList<Conductor *> after = sheet->conductors();
		for (Conductor *c : after) {
			if (!before.contains(c)) return c;
		}
		return nullptr;
	}

	QList<QPointF> points(const Conductor *c)
	{
		QList<QPointF> list;
		const auto segments = c->segmentsList();
		for (ConductorSegment *s : segments)
		{
			if (list.isEmpty()) list << c->mapToScene(s->firstPoint());
			list << c->mapToScene(s->secondPoint());
		}
		return list;
	}

		/// The coordinate of the bridge: the x of the segment that runs up
		/// or down, or the y of the one that runs across, between the two
		/// runs of a path of five segments; NaN for any other path
	double bridge(const Conductor *c)
	{
		const QList<QPointF> p = points(c);
		if (p.size() != 6) return std::nan("");
		if (std::abs(p.at(2).x() - p.at(3).x()) < 0.01) return p.at(2).x();
		if (std::abs(p.at(2).y() - p.at(3).y()) < 0.01) return p.at(2).y();
		return std::nan("");
	}

		/// The bench open, its six pairs of terminals at hand
	struct Bench
	{
		explicit Bench(bool frames = true) : project(fixtureXml(folio(), frames)) {}
		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			sensor.clear();
			terminal.clear();
			Element *s1 = tagged(sheet, "S1");
			Element *s2 = tagged(sheet, "S2");
			if (!s1 || !s2) return false;
			for (int i = 0 ; i < 6 ; ++i)
			{
				Element *t = tagged(sheet, QStringLiteral("T%1").arg(i).toLatin1().constData());
				if (!t) return false;
				sensor << (i < 3 ? s1 : s2)->terminals().at(i % 3);
				terminal << t->terminals().at(0);
			}
			return true;
		}
		Conductor *draw(int i) { return ::draw(sheet, sensor.at(i), terminal.at(i)); }

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		QList<Terminal *> sensor;
		QList<Terminal *> terminal;
	};

}

TEST_CASE("CU-67.1 — The bend in the middle between two frames", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());

		//The bench is the folio: the terminal ends at 180 and 270
	Conductor *c = bench.draw(1);
	REQUIRE(c);
	const QList<QPointF> automatic = c->automaticPathPoints();
	REQUIRE(automatic.size() == 6);
	CHECK(automatic.at(1).x() == Approx(180));
	CHECK(automatic.at(4).x() == Approx(270));
	CHECK(automatic.at(2).x() == Approx(225));

		//The frame drawn up to 215, its pen half a unit beyond: in the middle
		//of 215.5 and 270
	CHECK(bridge(c) == Approx(243));
	CHECK(c->isAutoArranged());
}

TEST_CASE("CU-67.3 — The new conductor re-arranges the ones the application placed", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());

	QList<Conductor *> drawn;
	for (int i = 0 ; i < 6 ; ++i)
	{
		drawn << bench.draw(i);
		REQUIRE(drawn.last());
	}
		//SP1 + of the folio, straight: left as it is
	CHECK_FALSE(drawn.at(0)->isAutoArranged());
	CHECK_FALSE(drawn.at(0)->hasSavedPath());
		//The five others in a staircase, the upper one on the right, as
		//WireLanes places them in one go
	CHECK(bridge(drawn.at(1)) == Approx(261));
	CHECK(bridge(drawn.at(2)) == Approx(252));
	CHECK(bridge(drawn.at(3)) == Approx(243));
	CHECK(bridge(drawn.at(4)) == Approx(234));
	CHECK(bridge(drawn.at(5)) == Approx(225));

	SECTION("the first one moved when the others came")
	{
		Bench again;
		REQUIRE(again.open());
		Conductor *first = again.draw(1);
		const double alone = bridge(first);
		again.draw(2);
		CHECK(bridge(first) != Approx(alone));
	}
}

TEST_CASE("CU-67.4 — A conductor dragged by hand does not move", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());

	Conductor *first = bench.draw(2);
	REQUIRE(first);
		//Dragged by hand, three to the left
	REQUIRE(first->moveSegment(2, -3, 0));
	CHECK_FALSE(first->isAutoArranged());
	const QList<QPointF> by_hand = points(first);

	Conductor *second = bench.draw(3);
	REQUIRE(second);
	CHECK(points(first) == by_hand);
		//The new one keeps clear of it
	CHECK(std::abs(bridge(second) - bridge(first)) >= 4);
}

TEST_CASE("CU-67.5 — One undo removes the conductor and puts the others back", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());

	Conductor *first = bench.draw(1);
	Conductor *second = bench.draw(2);
	REQUIRE((first && second));
	const QList<QPointF> first_before = points(first);
	const QList<QPointF> second_before = points(second);

	Conductor *third = bench.draw(3);
	REQUIRE(third);
	REQUIRE(points(first) != first_before);

	bench.sheet->undoStack().undo();
	CHECK_FALSE(bench.sheet->conductors().contains(third));
	CHECK(points(first) == first_before);
	CHECK(points(second) == second_before);
}

TEST_CASE("CU-67.7 — Saved and opened again, the paths and the mark come back", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	for (int i = 1 ; i <= 3 ; ++i) REQUIRE(bench.draw(i));

	QList<QList<QPointF>> before;
	for (int i = 1 ; i <= 3 ; ++i) before << points(bench.terminal.at(i)->conductors().first());

	REQUIRE(bench.project.saveAndReopen());
	REQUIRE(bench.open());
	for (int i = 1 ; i <= 3 ; ++i)
	{
		const QList<Conductor *> on = bench.terminal.at(i)->conductors();
		REQUIRE(on.size() == 1);
		CHECK(points(on.first()) == before.at(i - 1));
		CHECK(on.first()->isAutoArranged());
	}

		//A new one in the same group still moves them
	Conductor *third = bench.terminal.at(3)->conductors().first();
	const double was = bridge(third);
	REQUIRE(bench.draw(4));
	CHECK(bridge(third) != Approx(was));
}

TEST_CASE("CU-67.8 — Dragging takes the mark away, and undo gives it back", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());

	Conductor *first = bench.draw(2);
	REQUIRE(first);
	REQUIRE(first->isAutoArranged());
	REQUIRE(first->moveSegment(2, -3, 0));
	CHECK_FALSE(first->isAutoArranged());
	const QList<QPointF> by_hand = points(first);

	REQUIRE(bench.draw(3));
	CHECK(points(first) == by_hand);

		//Back to before the second conductor and the drag
	bench.sheet->undoStack().undo();
	bench.sheet->undoStack().undo();
	CHECK(first->isAutoArranged());
	const QList<QPointF> arranged = points(first);
	REQUIRE(bench.draw(3));
	CHECK(points(first) != arranged);
}

TEST_CASE("CU-67.9 — Arrange the selected conductors, even one dragged by hand", "[T67][wirearrangement]")
{
	Setup setup(1, false);
	Bench bench;
	REQUIRE(bench.open());

		//Drawn with the arranging switched off: all at 225, as on the folio
	QList<Conductor *> drawn;
	for (int i = 0 ; i < 6 ; ++i) drawn << bench.draw(i);
	for (int i = 1 ; i < 6 ; ++i) REQUIRE(bridge(drawn.at(i)) == Approx(225));
	REQUIRE(drawn.at(3)->moveSegment(2, 5, 0));
	REQUIRE(drawn.at(5)->moveSegment(2, -5, 0));

		//Three from 1 selected, the one dragged by hand among them. The
		//fourth, in their group, is arranged with them (eighth adjustment);
		//the last one, dragged by hand and not selected, stays where it was
		//dragged, in the way
	const QList<Conductor *> selected = drawn.mid(1, 3);
	CHECK(WireArrangement::arrange(bench.sheet, selected, WireArrangement::Scope::Selection) == 4);
	CHECK(bridge(drawn.at(5)) == Approx(220));
	CHECK(bridge(drawn.at(3)) != Approx(230));
	CHECK(bridge(drawn.at(4)) != Approx(225));
	for (int i = 1 ; i < 5 ; ++i)
	{
		CHECK(std::abs(bridge(drawn.at(i)) - 220) >= 4);
		for (int j = i + 1 ; j < 5 ; ++j) {
			CHECK(std::abs(bridge(drawn.at(i)) - bridge(drawn.at(j))) >= 4);
		}
	}

	bench.sheet->undoStack().undo();
	CHECK(bridge(drawn.at(1)) == Approx(225));
	CHECK(bridge(drawn.at(2)) == Approx(225));
	CHECK(bridge(drawn.at(3)) == Approx(230));
	CHECK(bridge(drawn.at(4)) == Approx(225));
	CHECK(bridge(drawn.at(5)) == Approx(220));
}

TEST_CASE("CU-67.10 — Arrange the conductors of the folio, but not those dragged by hand", "[T67][wirearrangement]")
{
	Setup setup(1, false);
	Bench bench;
	REQUIRE(bench.open());

	QList<Conductor *> drawn;
	for (int i = 0 ; i < 6 ; ++i) drawn << bench.draw(i);
	REQUIRE(drawn.at(5)->moveSegment(2, -20, 0));
	const QList<QPointF> by_hand = points(drawn.at(5));

	CHECK(WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio) == 4);
	CHECK(points(drawn.at(5)) == by_hand);
	for (int i = 1 ; i < 5 ; ++i)
	{
		CHECK(bridge(drawn.at(i)) != Approx(225));
		CHECK(std::abs(bridge(drawn.at(i)) - bridge(drawn.at(5))) >= 4);
	}

	bench.sheet->undoStack().undo();
	for (int i = 1 ; i < 5 ; ++i) CHECK(bridge(drawn.at(i)) == Approx(225));
}

TEST_CASE("CU-67.11 — Switched off, the new conductor is drawn as before", "[T67][wirearrangement]")
{
	Setup setup(1, false);
	Bench bench;
	REQUIRE(bench.open());

	Conductor *first = bench.draw(1);
	REQUIRE(first);
	CHECK(bridge(first) == Approx(225));
	Conductor *second = bench.draw(2);
	REQUIRE(second);
	CHECK(bridge(second) == Approx(225));
	CHECK(bridge(first) == Approx(225));
	CHECK_FALSE(first->isAutoArranged());
	CHECK_FALSE(first->hasSavedPath());
}

TEST_CASE("CU-67.12 — Up and down: the same rule, the axes swapped", "[T67][wirearrangement]")
{
	Setup setup;
	UiBench::ScratchProject project(fixtureXml({{"south.elmt", 100, 90, "A"},
						    {"south.elmt", 113, 90, "B"},
						    {"north.elmt", 110, 210, "C"},
						    {"north.elmt", 133, 210, "D"}}, false));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Conductor *first = draw(sheet, tagged(sheet, "A")->terminals().at(0),
				tagged(sheet, "C")->terminals().at(0));
	Conductor *second = draw(sheet, tagged(sheet, "B")->terminals().at(0),
				 tagged(sheet, "D")->terminals().at(0));
	REQUIRE((first && second));
	REQUIRE(first->automaticPathPoints().size() == 6);

		//The terminal ends at 110 and 190: the two bridges spread over the
		//80 between them, 27 apart, centered on 150, the one on the left
		//lower
	const double a = bridge(first);
	const double b = bridge(second);
	CHECK(a > b);
	CHECK(a - b == Approx(27));
	CHECK((a + b) / 2 == Approx(150).margin(1));
}

TEST_CASE("CU-69.4 — Up and down on a grid of 0.5", "[T69][wirearrangement]")
{
		//The same two conductors, with the grid of the folio set to 0.5 in
		//the settings: 80 / 3 is 26.67, 26.5 apart on halves where it was
		//27 on whole units
	Setup setup;
	QSettings settings;
	const QVariant had_x = settings.value(FolioGrid::x_key);
	const QVariant had_y = settings.value(FolioGrid::y_key);
	settings.setValue(FolioGrid::x_key, 0.5);
	settings.setValue(FolioGrid::y_key, 0.5);

	UiBench::ScratchProject project(fixtureXml({{"south.elmt", 100, 90, "A"},
						    {"south.elmt", 113, 90, "B"},
						    {"north.elmt", 110, 210, "C"},
						    {"north.elmt", 133, 210, "D"}}, false));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Conductor *first = draw(sheet, tagged(sheet, "A")->terminals().at(0),
				tagged(sheet, "C")->terminals().at(0));
	Conductor *second = draw(sheet, tagged(sheet, "B")->terminals().at(0),
				 tagged(sheet, "D")->terminals().at(0));
	const double a = first ? bridge(first) : 0;
	const double b = second ? bridge(second) : 0;

	for (const auto &kept : {qMakePair(FolioGrid::x_key, had_x), qMakePair(FolioGrid::y_key, had_y)}) {
		if (kept.second.isValid()) settings.setValue(kept.first, kept.second);
		else settings.remove(kept.first);
	}
	REQUIRE((first && second));
	CHECK(a - b == Approx(26.5));
	CHECK((a + b) / 2 == Approx(150).margin(1));
}

TEST_CASE("CU-67.13 — A symbol in the space is not crossed", "[T67][wirearrangement]")
{
	Setup setup;
	QList<Instance> instances = folio();
	instances << Instance{"block.elmt", 245, 173, "K"};
	UiBench::ScratchProject project(fixtureXml(instances, true));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *k = tagged(sheet, "K");
	REQUIRE(k);
	const QRectF body = k->mapRectToScene(k->boundingRect());

	Conductor *c = draw(sheet, tagged(sheet, "S1")->terminals().at(1),
			    tagged(sheet, "T1")->terminals().at(0));
	REQUIRE(c);
	const double at = bridge(c);
	CHECK((at < body.left() - 5 || at > body.right() + 5));
}

namespace
{
		/// The elements of @a terminals dragged @a dx to the right, in two
		/// steps, as the mouse does - and with them the frame drawn around
		/// the terminals, as on the folio of the request: left behind, it
		/// would stand in the room the move opened
	void dragTerminals(Diagram *sheet, const QList<Terminal *> &terminals, qreal dx)
	{
		sheet->clearSelection();
		for (Terminal *t : terminals) {
			t->parentElement()->setSelected(true);
		}
		const auto items = sheet->items();
		for (QGraphicsItem *item : items) {
			auto *shape = qgraphicsitem_cast<QetShapeItem *>(item);
			if (shape && shape->mapRectToScene(shape->rect()).left() > 250) {
				shape->setSelected(true);
			}
		}
		ElementsMover &mover = sheet->elementsMover();
		REQUIRE(mover.beginMovement(sheet) > 0);
		mover.continueMovement(QPointF(dx / 2, 0));
		mover.continueMovement(QPointF(dx / 2, 0));
		mover.endMovement();
	}
}

TEST_CASE("CU-67.14 — Moving the terminals away arranges the group again", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	QList<Conductor *> drawn;
	for (int i = 0 ; i < 6 ; ++i) {
		drawn << bench.draw(i);
		REQUIRE(drawn.last());
	}
		//Five lanes in the 54.5 between the frame and the terminal ends: 9
		//apart, as much room left at each end
	QList<double> before;
	for (int i = 1 ; i < 6 ; ++i) before << bridge(drawn.at(i));
	for (int i = 1 ; i < 5 ; ++i) {
		REQUIRE(before.at(i - 1) - before.at(i) == Approx(9));
	}

		//The terminals 80 further: 134.5 of room, and the lanes take all of
		//it, 22 apart. A conductor following the move as one dragged by
		//hand does would keep 9 - each bend pushed by the same 40
	dragTerminals(bench.sheet, bench.terminal, 80);
	QList<double> after;
	for (int i = 1 ; i < 6 ; ++i) {
		CHECK(drawn.at(i)->isAutoArranged());
		after << bridge(drawn.at(i));
	}
	CAPTURE(before, after);
	for (int i = 1 ; i < 5 ; ++i) {
		CHECK(after.at(i - 1) - after.at(i) == Approx(22));
	}
		//Centered between the frame and the new terminal ends
	CHECK((after.first() + after.last()) / 2 == Approx((215.5 + 350) / 2).margin(1));

		//One undo: the terminals and the bends back
	bench.sheet->undoStack().undo();
	for (int i = 1 ; i < 6 ; ++i) {
		CAPTURE(i);
		CHECK(bridge(drawn.at(i)) == Approx(before.at(i - 1)));
	}
}

TEST_CASE("CU-67.14 — A conductor dragged by hand is not arranged by a move", "[T67][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *by_hand = bench.draw(2);
	REQUIRE(by_hand);
	REQUIRE(by_hand->moveSegment(2, -6, 0));
	const double was = bridge(by_hand);

	dragTerminals(bench.sheet, {bench.terminal.at(2)}, 80);
		//It follows the move as before: the 80 shared among its four runs,
		//40 of it before the bend
	CHECK_FALSE(by_hand->isAutoArranged());
	CHECK(bridge(by_hand) == Approx(was + 40));
}

namespace
{
		/// A rectangle drawn with a pen 1 wide, from (@a x1, @a y1) to
		/// (@a x2, @a y2)
	QString frame(int x1, int y1, int x2, int y2)
	{
		return QStringLiteral("<shape type=\"Rectangle\" x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\""
				      " is_movable=\"1\" closed=\"0\" z=\"5\">"
				      "<pen color=\"#000000\" style=\"DashLine\" widthF=\"1\"/></shape>")
			.arg(x1).arg(y1).arg(x2).arg(y2);
	}

	/*
		The foot of the folio of the request (T67, third adjustment): three
		sensors one above the other, each with a terminal that points up
		and one that points down, each in its frame, wired to the terminals
		11 to 16 of a strip whose ends are at x = 360. A component whose
		terminals point to the side, as the encoder of the folio, has wires
		to the terminals 6 and 9 above them: the line of the 6 is one unit
		from the upper terminal of the first sensor, and the lower terminal
		of the middle one is on the line of the 15.
	*/
	struct Foot
	{
		Foot() : project(fixtureXml(
			{{"door.elmt", 166, 468, "D1"},
			 {"door.elmt", 167, 578, "D2"},
			 {"door.elmt", 167, 675, "D3"},
			 {"sensor.elmt", 156, 393, "E"},
			 {"terminal.elmt", 370, 437, "R6"},
			 {"terminal.elmt", 370, 494, "R9"},
			 {"terminal.elmt", 370, 532, "R11"},
			 {"terminal.elmt", 370, 551, "R12"},
			 {"terminal.elmt", 370, 570, "R13"},
			 {"terminal.elmt", 370, 589, "R14"},
			 {"terminal.elmt", 370, 608, "R15"},
			 {"terminal.elmt", 370, 627, "R16"}},
			QStringLiteral("<shapes>%1%2%3</shapes>")
				.arg(frame(75, 34, 215, 521), frame(75, 525, 215, 621), frame(75, 624, 215, 720))))
		{}

		Terminal *at(const char *label, int i) const
		{
			Element *e = tagged(sheet, label);
			return e ? e->terminals().at(i) : nullptr;
		}
		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			pairs = {{at("D1", 0), at("R11", 0)}, {at("D1", 1), at("R12", 0)},
				 {at("D2", 0), at("R13", 0)}, {at("D2", 1), at("R14", 0)},
				 {at("D3", 0), at("R15", 0)}, {at("D3", 1), at("R16", 0)},
				 {at("E", 0), at("R6", 0)}, {at("E", 1), at("R9", 0)}};
			for (const auto &p : pairs) {
				if (!p.first || !p.second) return false;
			}
			return true;
		}
		bool drawAll()
		{
			for (const auto &p : pairs) {
				if (!::draw(sheet, p.first, p.second)) return false;
			}
			return true;
		}
			/// The path of the wire @a i, from the component's terminal
		QList<QPointF> path(int i) const
		{
			const QList<Conductor *> on = pairs.at(i).first->conductors();
			if (on.isEmpty()) return {};
			QList<QPointF> p = points(on.first());
			if ((p.first() - pairs.at(i).first->dockConductor()).manhattanLength() > 0.5) {
				std::reverse(p.begin(), p.end());
			}
			return p;
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		QList<QPair<Terminal *, Terminal *>> pairs;
	};

	struct Piece
	{
		QPointF a;
		QPointF b;
		const Conductor *owner;
	};

	QList<Piece> pieces(Diagram *sheet)
	{
		QList<Piece> out;
		const auto conductors = sheet->conductors();
		for (Conductor *c : conductors)
		{
			const QList<QPointF> p = points(c);
			for (int i = 0 ; i + 1 < p.size() ; ++i) out << Piece{p.at(i), p.at(i + 1), c};
		}
		return out;
	}

	bool runsAcross(const Piece &p) { return std::abs(p.a.y() - p.b.y()) < 0.01; }

	QString text(const QList<QPointF> &points)
	{
		QStringList out;
		for (const QPointF &p : points) out << QStringLiteral("(%1,%2)").arg(p.x()).arg(p.y());
		return out.join(QLatin1Char(' '));
	}

		/// Where two conductors of @a sheet cross, inside both pieces
	QList<QPointF> crossings(Diagram *sheet)
	{
		QList<QPointF> out;
		const QList<Piece> all = pieces(sheet);
		for (const Piece &h : all)
		{
			if (!runsAcross(h)) continue;
			for (const Piece &v : all)
			{
				if (v.owner == h.owner || runsAcross(v)) continue;
				const double x = v.a.x(), y = h.a.y();
				if (x > std::min(h.a.x(), h.b.x()) + 0.5 && x < std::max(h.a.x(), h.b.x()) - 0.5
					&& y > std::min(v.a.y(), v.b.y()) + 0.5 && y < std::max(v.a.y(), v.b.y()) - 0.5) {
					out << QPointF(x, y);
				}
			}
		}
		return out;
	}

		/// The terminals of @a sheet that a conductor not joined to them
		/// passes closer than @a distance to
	QList<QPointF> passedTerminals(Diagram *sheet, double distance)
	{
		QList<QPointF> out;
		const QList<Piece> all = pieces(sheet);
		const auto elements = sheet->elements();
		for (Element *e : elements)
		{
			const auto terminals = e->terminals();
			for (Terminal *t : terminals)
			{
				const QPointF d = t->dockConductor();
				for (const Piece &p : all)
				{
					if (p.owner->terminal1 == t || p.owner->terminal2 == t) continue;
					const double dx = std::max({std::min(p.a.x(), p.b.x()) - d.x(), 0.0,
								    d.x() - std::max(p.a.x(), p.b.x())});
					const double dy = std::max({std::min(p.a.y(), p.b.y()) - d.y(), 0.0,
								    d.y() - std::max(p.a.y(), p.b.y())});
					if (std::hypot(dx, dy) < distance - 1e-6) {
						out << d;
						break;
					}
				}
			}
		}
		return out;
	}

		/// How many pieces of conductors run along the side of a frame,
		/// closer than @a distance
	int alongFrames(Diagram *sheet, double distance)
	{
		int n = 0;
		const QList<Piece> all = pieces(sheet);
		const auto items = sheet->items();
		for (QGraphicsItem *item : items)
		{
			auto *shape = qgraphicsitem_cast<QetShapeItem *>(item);
			if (!shape) continue;
			const QRectF r = shape->mapRectToScene(shape->rect()).adjusted(-0.5, -0.5, 0.5, 0.5);
			for (const Piece &p : all)
			{
				const double lo_x = std::min(p.a.x(), p.b.x()), hi_x = std::max(p.a.x(), p.b.x());
				const double lo_y = std::min(p.a.y(), p.b.y()), hi_y = std::max(p.a.y(), p.b.y());
				if (runsAcross(p)) {
					for (double side : {r.top(), r.bottom()}) {
						if (std::abs(p.a.y() - side) < distance
							&& std::min(hi_x, r.right()) - std::max(lo_x, r.left()) > 0.5) ++n;
					}
				} else {
					for (double side : {r.left(), r.right()}) {
						if (std::abs(p.a.x() - side) < distance
							&& std::min(hi_y, r.bottom()) - std::max(lo_y, r.top()) > 0.5) ++n;
					}
				}
			}
		}
		return n;
	}

	int bends(const QList<QPointF> &p)
	{
		int n = 0;
		for (int i = 1 ; i + 1 < p.size() ; ++i)
		{
			const bool first = std::abs(p.at(i).y() - p.at(i - 1).y()) < 0.01;
			const bool second = std::abs(p.at(i + 1).y() - p.at(i).y()) < 0.01;
			if (first != second) ++n;
		}
		return n;
	}
}

TEST_CASE("CU-67.17 — The foot of the folio, without crossing", "[T67][wirearrangement]")
{
	Setup setup(1, false);
	Foot foot;
	REQUIRE(foot.open());
	REQUIRE(foot.drawAll());

		//As the automatic paths draw it: the wire to the 12 down over the
		//upper terminal of the middle sensor, the one to the 15 up to its
		//lower one, the bridge of the 13 across the line of the 12, and
		//the runs of the 14 and the 16 on the bottoms of the frames
	CAPTURE(text(crossings(foot.sheet)), text(passedTerminals(foot.sheet, 5)));
	REQUIRE(!crossings(foot.sheet).isEmpty());
	REQUIRE(passedTerminals(foot.sheet, 5).size() >= 2);
	REQUIRE(alongFrames(foot.sheet, 3) >= 2);

	CHECK(WireArrangement::arrange(foot.sheet, {}, WireArrangement::Scope::Folio) > 0);
	for (int i = 0 ; i < foot.pairs.size() ; ++i) {
		UNSCOPED_INFO("wire " << i << ": " << text(foot.path(i)).toStdString());
	}
	CHECK(crossings(foot.sheet).isEmpty());
	CHECK(passedTerminals(foot.sheet, 5).isEmpty());
	CHECK(alongFrames(foot.sheet, 3) == 0);

		//One undo: the automatic paths back, the defects with them
	foot.sheet->undoStack().undo();
	CHECK(!crossings(foot.sheet).isEmpty());
}

TEST_CASE("CU-67.17 — The foot of the folio drawn wire by wire", "[T67][wirearrangement]")
{
	Setup setup;
	Foot foot;
	REQUIRE(foot.open());
	REQUIRE(foot.drawAll());
	for (int i = 0 ; i < foot.pairs.size() ; ++i) {
		UNSCOPED_INFO("wire " << i << ": " << text(foot.path(i)).toStdString());
	}
	CHECK(crossings(foot.sheet).isEmpty());
	CHECK(passedTerminals(foot.sheet, 5).isEmpty());
	CHECK(alongFrames(foot.sheet, 3) == 0);
}

TEST_CASE("CU-67.18 — To the side, out of the terminal only as needed", "[T67][wirearrangement]")
{
	Setup setup(1, false);
	Foot foot;
	REQUIRE(foot.open());
	REQUIRE(foot.drawAll());
	REQUIRE(WireArrangement::arrange(foot.sheet, {}, WireArrangement::Scope::Folio) > 0);

		//The four whose terminal is on no other line leave it to the side
	for (int i : {1, 2, 4, 5})
	{
		const QList<QPointF> p = foot.path(i);
		CAPTURE(i, text(p));
		REQUIRE(p.size() >= 3);
		CHECK(p.at(1).y() == Approx(p.at(0).y()));
	}
		//The lower terminal of the middle sensor is on the line of the 15:
		//down 3, the least a line keeps from another, and to the side
	const QList<QPointF> fourteen = foot.path(3);
	CAPTURE(text(fourteen));
	REQUIRE(fourteen.size() >= 3);
	CHECK(fourteen.at(1).x() == Approx(167));
	CHECK(fourteen.at(1).y() == Approx(611));
	CHECK(fourteen.at(2).y() == Approx(611));
		//The upper one of the first sensor is one unit from the line of
		//the 6: up 4
	const QList<QPointF> eleven = foot.path(0);
	CAPTURE(text(eleven));
	REQUIRE(eleven.size() >= 3);
	CHECK(eleven.at(1).y() == Approx(434));
	CHECK(eleven.at(2).y() == Approx(434));

	SECTION("saved and opened again, the shapes come back")
	{
		QList<QList<QPointF>> before;
		for (int i = 0 ; i < foot.pairs.size() ; ++i) before << foot.path(i);
		REQUIRE(foot.project.saveAndReopen());
		REQUIRE(foot.open());
		for (int i = 0 ; i < foot.pairs.size() ; ++i)
		{
			CAPTURE(i);
			CHECK(text(foot.path(i)) == text(before.at(i)));
		}
	}
}

TEST_CASE("CU-67.19 — One bend, when the straight way is free", "[T67][wirearrangement]")
{
	Setup setup;
	UiBench::ScratchProject project(fixtureXml({{"door.elmt", 167, 578, "D"},
						    {"terminal.elmt", 370, 650, "R"}}, QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Conductor *c = draw(sheet, tagged(sheet, "D")->terminals().at(1),
			    tagged(sheet, "R")->terminals().at(0));
	REQUIRE(c);
	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);
	const QList<QPointF> p = points(c);
	CAPTURE(text(p));
	CHECK(bends(p) == 1);
	CHECK(text(p).contains(QStringLiteral("(167,650)")));

	SECTION("with the terminal of another component on the way, it leaves to the side")
	{
		UiBench::ScratchProject other(fixtureXml({{"door.elmt", 167, 578, "D"},
							  {"door.elmt", 167, 668, "F"},
							  {"terminal.elmt", 370, 650, "R"}}, QString()));
		REQUIRE(other.isOpen());
		Diagram *s = other.diagram(0);
		Conductor *w = draw(s, tagged(s, "D")->terminals().at(1),
				    tagged(s, "R")->terminals().at(0));
		REQUIRE(w);
		WireArrangement::arrange(s, {}, WireArrangement::Scope::Folio);
		const QList<QPointF> q = points(w);
		CAPTURE(text(q));
		CHECK(bends(q) == 2);
		CHECK(passedTerminals(s, 5).isEmpty());
	}
	SECTION("with only the end of a terminal on the way, it leaves to the side too")
	{
			//The end of the terminal at (167, 630), on the way down; the
			//body of its component too far for its margin to stand in
		UiBench::ScratchProject other(fixtureXml({{"door.elmt", 167, 578, "D"},
							  {"pin.elmt", 147, 630, "P"},
							  {"terminal.elmt", 370, 650, "R"}}, QString()));
		REQUIRE(other.isOpen());
		Diagram *s = other.diagram(0);
		REQUIRE(tagged(s, "P")->terminals().at(0)->dockConductor() == QPointF(167, 630));
		Conductor *w = draw(s, tagged(s, "D")->terminals().at(1),
				    tagged(s, "R")->terminals().at(0));
		REQUIRE(w);
		WireArrangement::arrange(s, {}, WireArrangement::Scope::Folio);
		const QList<QPointF> q = points(w);
		CAPTURE(text(q));
		CHECK(bends(q) == 2);
		CHECK(passedTerminals(s, 5).isEmpty());
	}
	SECTION("with a wire that would have to cross its line, it leaves to the side")
	{
			//A component further right, its terminal at 625, has a wire to
			//a terminal at 700: on its way down, its bridge would cross the
			//line at 650 that the one bend would draw
		UiBench::ScratchProject other(fixtureXml({{"door.elmt", 167, 578, "D"},
							  {"east.elmt", 190, 625, "E"},
							  {"terminal.elmt", 370, 650, "R"},
							  {"terminal.elmt", 370, 700, "S"}}, QString()));
		REQUIRE(other.isOpen());
		Diagram *s = other.diagram(0);
		REQUIRE(draw(s, tagged(s, "E")->terminals().at(0), tagged(s, "S")->terminals().at(0)));
		Conductor *w = draw(s, tagged(s, "D")->terminals().at(1),
				    tagged(s, "R")->terminals().at(0));
		REQUIRE(w);
		WireArrangement::arrange(s, {}, WireArrangement::Scope::Folio);
		const QList<QPointF> q = points(w);
		CAPTURE(text(q));
		CHECK(bends(q) == 2);
		CHECK(crossings(s).isEmpty());
	}
}

TEST_CASE("CU-67.18 — Not along the side of a frame", "[T67][wirearrangement]")
{
		//The bottom of the frame drawn at the height of the lower terminal,
		//608, with a pen 1 wide: to the side there, the run would follow
		//it half a unit off; down until it is 3 away
	Setup setup;
	UiBench::ScratchProject project(fixtureXml({{"door.elmt", 167, 578, "D"},
						    {"terminal.elmt", 370, 570, "R"}},
						   QStringLiteral("<shapes>%1</shapes>").arg(frame(75, 525, 215, 608))));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Conductor *c = draw(sheet, tagged(sheet, "D")->terminals().at(1),
			    tagged(sheet, "R")->terminals().at(0));
	REQUIRE(c);
	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);
	QList<QPointF> p = points(c);
	if (p.first().y() > 608.5 || p.first().y() < 607.5) std::reverse(p.begin(), p.end());
	CAPTURE(text(p));
	REQUIRE(p.size() >= 3);
	CHECK(text(p).startsWith(QStringLiteral("(167,608) (167,612) ")));
	CHECK(alongFrames(sheet, 3) == 0);
}

/*
	Dragging the segment next to the one joined to a terminal (T68): the
	conductor from S1, whose terminal points right at (167, 168), to T1 at
	(277, 178), with the segment of the terminal of S1 long, to 200 - as a
	move of a component leaves it.
*/
namespace
{
	Conductor *longStub(Bench &bench)
	{
		Conductor *c = bench.draw(1);
		if (!c) return nullptr;
		if (!c->setPathPoints({{167, 168}, {200, 168}, {200, 178}, {277, 178}})) return nullptr;
		return c;
	}
}

TEST_CASE("CU-68.1 — Dragging the long arm shortens the short one", "[T68][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *c = longStub(bench);
	REQUIRE(c);
		//The upright segment dragged 25 towards S1: the bend where it was let
		//go, and nothing beyond it
	REQUIRE(c->moveSegment(1, -25, 0));
	const QList<QPointF> bent{{167, 168}, {175, 168}, {175, 178}, {277, 178}};
	CHECK(points(c) == bent);

	SECTION("CU-68.4 — one undo")
	{
		bench.sheet->undoStack().undo();
		const QList<QPointF> before{{167, 168}, {200, 168}, {200, 178}, {277, 178}};
		CHECK(points(c) == before);
	}
}

TEST_CASE("CU-68.2 — The bend right over the terminal", "[T68][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *c = longStub(bench);
	REQUIRE(c);
		//Dragged all the way to the terminal: one unit off it, and the
		//segment dragged still a middle one, which drags
	REQUIRE(c->moveSegment(1, -40, 0));
	const QList<QPointF> bent{{167, 168}, {168, 168}, {168, 178}, {277, 178}};
	CHECK(points(c) == bent);
	REQUIRE(c->moveSegment(1, 10, 0));
	const QList<QPointF> dragged{{167, 168}, {178, 168}, {178, 178}, {277, 178}};
	CHECK(points(c) == dragged);
}

TEST_CASE("CU-68.3 — Dragging away draws as before", "[T68][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *c = longStub(bench);
	REQUIRE(c);
	REQUIRE(c->moveSegment(1, 20, 0));
	const QList<QPointF> bent{{167, 168}, {220, 168}, {220, 178}, {277, 178}};
	CHECK(points(c) == bent);
}

TEST_CASE("CU-68.6 — Dragging by the handle, past the terminal", "[T68][wirearrangement]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *c = longStub(bench);
	REQUIRE(c);
	const QList<QPointF> before = points(c);

		//The handle of the upright segment, the one the mouse takes
	c->setSelected(true);
	QetGraphicsHandlerItem *handle = nullptr;
	const auto items = bench.sheet->items(QPointF(200, 173));
	for (QGraphicsItem *item : items) {
		if (item->type() == QetGraphicsHandlerItem::Type) {
			handle = qgraphicsitem_cast<QetGraphicsHandlerItem *>(item);
		}
	}
	REQUIRE(handle);

	const auto send = [&](QEvent::Type type, qreal x) {
		QGraphicsSceneMouseEvent event(type);
		event.setScenePos(QPointF(x, 173));
		event.setButton(Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
		bench.sheet->sendEvent(handle, &event);
	};
	send(QEvent::GraphicsSceneMousePress, 200);
		//Step by step towards S1, and on past its terminal at 167: at each
		//step the bend is where the mouse is, or one unit from the
		//terminal when the mouse is past it, and no segment runs back over
		//another
	for (qreal x : {190.0, 180.0, 170.0, 160.0, 150.0, 172.0, 140.0})
	{
		CAPTURE(x);
		send(QEvent::GraphicsSceneMouseMove, x);
		const QList<QPointF> now = points(c);
		CAPTURE(now);
		CHECK(WirePath::simplified(now) == now);
		CHECK(now.at(1).x() == Approx(std::max(168.0, x)));
		CHECK(now.at(1).y() == Approx(168));
	}
	send(QEvent::GraphicsSceneMouseRelease, 140);
	const QList<QPointF> bent{{167, 168}, {168, 168}, {168, 178}, {277, 178}};
	CHECK(points(c) == bent);

		//One undo for the whole drag
	bench.sheet->undoStack().undo();
	CHECK(points(c) == before);
}

namespace
{
		/// The x of the longest piece of @a p that runs up or down, and
		/// the y of its first piece that runs to the side
	QPointF laneAndRun(const QList<QPointF> &p)
	{
		qreal lane = 0;
		qreal longest = -1;
		qreal run = 0;
		bool run_found = false;
		for (int i = 0 ; i + 1 < p.size() ; ++i)
		{
			const QPointF a = p.at(i), b = p.at(i + 1);
			if (std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) > longest) {
				longest = std::abs(a.y() - b.y());
				lane = a.x();
			}
			if (!run_found && std::abs(a.y() - b.y()) < 0.01 && std::abs(a.x() - b.x()) > 0.01) {
				run = a.y();
				run_found = true;
			}
		}
		return QPointF(lane, run);
	}
}

TEST_CASE("CU-67.20 — Two wires turn round a column of terminals", "[T67][wirearrangement]")
{
	/*
		The case of folio 5: two terminals of a strip pointing down, at x
		497 and 527, wired to two terminals pointing left at x 499 - the
		left one to the lower, the right one to the upper. Both components
		are on this side of the end of the terminals they go to: each wire
		has to go past it and come back.
	*/
	Setup setup;
	UiBench::ScratchProject project(fixtureXml({{"south.elmt", 497, 89, "X1"},
						    {"south.elmt", 527, 89, "X3"},
						    {"terminal.elmt", 509, 172, "VP"},
						    {"terminal.elmt", 509, 183, "VM"}}, QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	REQUIRE(tagged(sheet, "X1")->terminals().at(0)->dockConductor() == QPointF(497, 99));
	REQUIRE(tagged(sheet, "VM")->terminals().at(0)->dockConductor() == QPointF(499, 183));
	Conductor *outer = draw(sheet, tagged(sheet, "X1")->terminals().at(0),
				tagged(sheet, "VM")->terminals().at(0));
	Conductor *inner = draw(sheet, tagged(sheet, "X3")->terminals().at(0),
				tagged(sheet, "VP")->terminals().at(0));
	REQUIRE((outer && inner));

	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);
	const QList<QPointF> o = points(outer);
	const QList<QPointF> i = points(inner);
	CAPTURE(text(o), text(i), text(crossings(sheet)));
	const QPointF outer_way = laneAndRun(o);
	const QPointF inner_way = laneAndRun(i);

		//No crossing, and both lanes past the end of the terminals
	CHECK(crossings(sheet).isEmpty());
	CHECK(outer_way.x() < 495);
	CHECK(inner_way.x() < 495);
		//The wire of the left terminal outside, a pitch from the other
	CHECK(outer_way.x() < inner_way.x());
	CHECK(inner_way.x() - outer_way.x() == Approx(15));
		//And it turns first
	CHECK(outer_way.y() < inner_way.y());
		//Each into its terminal, from its lane
	CHECK(o.last() == QPointF(499, 183));
	CHECK(i.last() == QPointF(499, 172));
}

namespace
{
		/// One of the eight ways a drawing can lie on the folio
	struct Way
	{
		bool mirrored;
		bool upside_down;
		bool turned;
	};

	std::ostream &operator<<(std::ostream &out, const Way &way)
	{
		return out << (way.mirrored ? "mirrored " : "") << (way.upside_down ? "upside down " : "")
			   << (way.turned ? "turned" : "as drawn");
	}

	QList<Way> eightWays()
	{
		QList<Way> ways;
		for (bool turned : {false, true}) {
			for (bool mirrored : {false, true}) {
				for (bool upside_down : {false, true}) ways << Way{mirrored, upside_down, turned};
			}
		}
		return ways;
	}

	/*
		A point of several wires (T67, fifth adjustment), in any of the
		eight ways it can lie on the folio: as drawn, mirrored left to right (about x 700),
		upside down, x and y swapped, and their combinations. The checks are
		made on the drawing as drawn - every point of the folio taken back
		to it -, so that one rule is asked of all eight.

		As drawn: the common terminal points down, at (common_x, 321);
		@a arms terminals of a strip on its right point left, their ends at
		x 775, the first at y 352 and the next ones every @a step. With
		others, the wires of other points come from terminals on the left,
		at x 635, to terminals of the strip between the arms - the folio of
		the request, whose arms are 352, 421, 467, 513 and 559.
	*/
	struct Point
	{
		static constexpr int request_arms[] = {352, 421, 467, 513, 559};

		Point(int arms, int step, bool others, Way way, int common_x = 735,
		      const QList<QPointF> &blocks = {}, int left_x = 635,
		      const QList<Instance> &more = {}) :
			way(way),
			project(fixtureXml(instances(arms, step, others, common_x, blocks, left_x) + more, QString()))
		{
			for (int i = 0 ; i < arms ; ++i) arm_y << armY(i, step);
		}

		static double armY(int i, int step)
		{
			return step > 0 ? 352 + i * step : request_arms[i];
		}

			/// A point as drawn, where it lies on the folio
		QPointF place(QPointF p) const
		{
			if (way.mirrored) p.setX(1400 - p.x());
			if (way.upside_down) p.setY(880 - p.y());
			if (way.turned) p = QPointF(p.y(), p.x());
			return p;
		}
			/// A point of the folio, back where it is as drawn
		QPointF drawn(QPointF p) const
		{
			if (way.turned) p = QPointF(p.y(), p.x());
			if (way.upside_down) p.setY(880 - p.y());
			if (way.mirrored) p.setX(1400 - p.x());
			return p;
		}
		QPointF turn(QPointF towards) const
		{
			if (way.mirrored) towards.setX(-towards.x());
			if (way.upside_down) towards.setY(-towards.y());
			if (way.turned) towards = QPointF(towards.y(), towards.x());
			return towards;
		}
			/// The component whose terminal ends at @a end, pointing
			/// @a towards, as drawn
		Instance component(QPointF end, QPointF towards, const char *label) const
		{
			end = place(end);
			towards = turn(towards);
			const char *definition = towards.y() > 0 ? "south.elmt"
					       : towards.y() < 0 ? "north.elmt"
					       : towards.x() > 0 ? "east.elmt" : "terminal.elmt";
			return Instance{definition, qRound(end.x() - 10 * towards.x()),
					qRound(end.y() - 10 * towards.y()), label};
		}

		QList<Instance> instances(int arms, int step, bool others, int common_x,
					  const QList<QPointF> &blocks, int left_x) const
		{
			const int left_y[] = {375, 420, 465, 510, 555, 600};
			const int other_y[] = {375, 398, 444, 490, 536, 582};
			const QPointF down(0, 1), left(-1, 0), right(1, 0);
			QList<Instance> out;
			out << component(QPointF(common_x, 321), down, "C");
			for (int i = 0 ; i < arms ; ++i) {
				out << component(QPointF(775, armY(i, step)), left, arm_labels[i]);
			}
			for (int i = 0 ; others && i < 6 ; ++i)
			{
				out << component(QPointF(left_x, left_y[i]), right, left_labels[i]);
				out << component(QPointF(775, other_y[i]), left, other_labels[i]);
			}
			for (int i = 0 ; i < blocks.size() ; ++i)
			{
				const QPointF p = place(blocks.at(i));
				out << Instance{"block.elmt", qRound(p.x()), qRound(p.y()), block_labels[i]};
			}
			return out;
		}

		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			return terminal("C") != nullptr;
		}
		Terminal *terminal(const char *label) const
		{
			Element *e = tagged(sheet, label);
			return e ? e->terminals().at(0) : nullptr;
		}
			/// The wires of the point, then the others, drawn
		bool drawAll(bool others)
		{
			for (int i = 0 ; i < arm_y.size() ; ++i)
			{
				Conductor *c = draw(sheet, terminal("C"), terminal(arm_labels[i]));
				if (!c) return false;
				point << c;
			}
			for (int i = 0 ; others && i < 6 ; ++i)
			{
				Conductor *c = draw(sheet, terminal(left_labels[i]), terminal(other_labels[i]));
				if (!c) return false;
				rest << c;
			}
			return true;
		}
			/// The path of @a c as drawn, from its end on @a from
		QList<QPointF> path(const Conductor *c, Terminal *from) const
		{
			QList<QPointF> p = points(c);
			if (!p.isEmpty() && (p.first() - from->dockConductor()).manhattanLength() > 0.5) {
				std::reverse(p.begin(), p.end());
			}
			for (QPointF &q : p) q = drawn(q);
			return p;
		}
			/// The x of the common line, as drawn: the leftmost piece running
			/// up or down of the wires of the point
		double trunk() const
		{
			double x = std::nan("");
			for (const Conductor *c : point)
			{
				const QList<QPointF> p = path(c, terminal("C"));
				for (int i = 0 ; i + 1 < p.size() ; ++i)
				{
					if (std::abs(p.at(i).x() - p.at(i + 1).x()) < 0.01
					    && std::abs(p.at(i).y() - p.at(i + 1).y()) > 0.01
					    && (std::isnan(x) || p.at(i).x() < x)) {
						x = p.at(i).x();
					}
				}
			}
			return x;
		}
		static void name(Conductor *c, const QString &text, int size = 0)
		{
			ConductorProperties properties = c->properties();
			properties.text = text;
			if (size > 0) properties.text_size = size;
			c->setProperties(properties);
		}
			/// The box of the name of @a c, as drawn
		QRectF nameBox(const Conductor *c) const
		{
			const QRectF r = c->textItem()->sceneBoundingRect();
			return QRectF(drawn(r.topLeft()), drawn(r.bottomRight())).normalized();
		}
			/// The box of the letters of the name of @a c, as drawn: its box
			/// less the blank margin of the text
		QRectF letters(const Conductor *c) const
		{
			const qreal m = c->textItem()->document()->documentMargin();
			return nameBox(c).adjusted(m, m, -m, -m);
		}
			/// The run of a wire as drawn, on the folio
		Qt::Orientations armRun() const { return way.turned ? Qt::Vertical : Qt::Horizontal; }

		static constexpr const char *arm_labels[] = {"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7"};
		static constexpr const char *left_labels[] = {"L0", "L1", "L2", "L3", "L4", "L5"};
		static constexpr const char *other_labels[] = {"B0", "B1", "B2", "B3", "B4", "B5"};
		static constexpr const char *block_labels[] = {"K0", "K1"};

		Way way;
		UiBench::ScratchProject project;
		QList<double> arm_y;
		Diagram *sheet = nullptr;
		QList<Conductor *> point;
		QList<Conductor *> rest;
	};
}

TEST_CASE("CU-67.21 — A point of N wires becomes one common line", "[T67][wirearrangement]")
{
	const int arms = GENERATE(2, 3, 5, 8);
	const int step = GENERATE(23, 46);
	const bool wire_by_wire = GENERATE(false, true);
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(arms, step, wire_by_wire, way);
	Setup setup(1, wire_by_wire);
	Point bench(arms, step, false, way);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	if (!wire_by_wire) {
		WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	}

		//Each wire straight on from the common terminal, on one line, to
		//the height of its own terminal: no lane of its own
	for (int i = 0 ; i < arms ; ++i)
	{
		const QList<QPointF> p = bench.path(bench.point.at(i), bench.terminal("C"));
		CAPTURE(i, text(p));
		REQUIRE(p.size() >= 3);
		CHECK(p.at(0) == QPointF(735, 321));
		CHECK(p.at(1) == QPointF(735, bench.arm_y.at(i)));
		CHECK(p.last() == QPointF(775, bench.arm_y.at(i)));
	}
}

TEST_CASE("CU-67.22 — The wires of other points around the common line", "[T67][wirearrangement]")
{
		//The other points from far on the left, as on the folio of the
		//request, and from near the common line - where the widest room for
		//their bridges is between it and the strip, along the arms; and a
		//wire placed by hand straight across the common line, which stays
	const int left_x = GENERATE(635, 705);
	const bool by_hand = GENERATE(false, true);
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(left_x, by_hand, way);
	Setup setup(1, false);
	QList<Instance> more;
	if (by_hand)
	{
		const Point layout(0, 0, false, way);
		more << layout.component(QPointF(605, 440), QPointF(1, 0), "H0")
		     << layout.component(QPointF(750, 440), QPointF(-1, 0), "H1");
	}
	Point bench(5, 0, true, way, 735, {}, left_x, more);
	REQUIRE(bench.open());
		//Drawn first, on the empty folio, where nothing lies over its ends
	if (by_hand)
	{
		Conductor *across = draw(bench.sheet, bench.terminal("H0"), bench.terminal("H1"));
		REQUIRE(across != nullptr);
		REQUIRE(across->setArrangedPath(points(across)));
		across->setAutoArranged(false);
	}
	REQUIRE(bench.drawAll(true));
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);

	const double trunk = bench.trunk();
	CAPTURE(trunk);
	CHECK(trunk == Approx(735));
	for (int i = 0 ; i < 5 ; ++i)
	{
		const QList<QPointF> p = bench.path(bench.point.at(i), bench.terminal("C"));
		CAPTURE(i, text(p));
		REQUIRE(p.size() >= 2);
		CHECK(p.at(1).x() == Approx(trunk));
	}

		//The pieces of the other wires, as drawn
	QList<Piece> others;
	for (int k = 0 ; k < bench.rest.size() ; ++k)
	{
		const QList<QPointF> p = bench.path(bench.rest.at(k), bench.terminal(Point::left_labels[k]));
		UNSCOPED_INFO(text(p).toStdString());
		for (int i = 0 ; i + 1 < p.size() ; ++i) others << Piece{p.at(i), p.at(i + 1), bench.rest.at(k)};
	}
		//No two of them cross
	int crossed = 0;
	for (const Piece &h : std::as_const(others))
	{
		if (!runsAcross(h)) continue;
		for (const Piece &v : std::as_const(others))
		{
			if (v.owner == h.owner || runsAcross(v)) continue;
			const double x = v.a.x(), y = h.a.y();
			if (x > std::min(h.a.x(), h.b.x()) + 0.5 && x < std::max(h.a.x(), h.b.x()) - 0.5
			    && y > std::min(v.a.y(), v.b.y()) + 0.5 && y < std::max(v.a.y(), v.b.y()) - 0.5) {
				++crossed;
			}
		}
	}
	CHECK(crossed == 0);
		//Each crosses the common line once at most, and none runs closer
		//than 3 to an arm of the point
	for (const Conductor *owner : std::as_const(bench.rest))
	{
		int through = 0;
		int along_an_arm = 0;
		for (const Piece &piece : std::as_const(others))
		{
			if (piece.owner != owner || !runsAcross(piece)) continue;
			const double lo = std::min(piece.a.x(), piece.b.x());
			const double hi = std::max(piece.a.x(), piece.b.x());
			if (lo < trunk - 0.5 && hi > trunk + 0.5 && piece.a.y() > 321 && piece.a.y() < 559) ++through;
			for (double y : bench.arm_y)
			{
				if (std::abs(piece.a.y() - y) < 3 && std::min(hi, 765.0) - std::max(lo, trunk) > 0.5) {
					++along_an_arm;
				}
			}
		}
		CHECK(through <= 1);
		CHECK(along_an_arm == 0);
	}
}

TEST_CASE("CU-67.23 — The name on the piece no other wire crosses", "[T67][wirearrangement]")
{
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(way);
	Setup setup(1, false);
	Point bench(5, 0, true, way);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(true));
		//Names of the folio of the request, at its size, the smallest one:
		//they cannot be drawn smaller to fit. Those of the other points
		//longer, so that, as on that folio, none of their runs holds them
		//while their bridge is in the middle of its room
	for (Conductor *c : std::as_const(bench.point)) Point::name(c, QStringLiteral("F5-21 (■)"), 5);
	for (int i = 0 ; i < bench.rest.size() ; ++i) {
		Point::name(bench.rest.at(i), QStringLiteral("F5-1%1 (■ 1mm²) X4:2.2 A").arg(i + 3), 5);
	}
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	const double trunk = bench.trunk();
	REQUIRE(!std::isnan(trunk));

		//The name of each wire of the point lies along its arm, between the
		//common line and the strip
	const double along_the_arm = bench.armRun() == Qt::Vertical ? 270 : 0;
	for (int i = 0 ; i < 5 ; ++i)
	{
		const QList<QPointF> p = bench.path(bench.point.at(i), bench.terminal("C"));
		const double stub = p.at(p.size() - 2).x();
		const QRectF box = bench.letters(bench.point.at(i));
		CAPTURE(i, trunk, stub, box, text(p));
		CHECK(bench.point.at(i)->textItem()->rotation() == Approx(along_the_arm));
			//Beside its arm, nearer it than any other line of the strip,
			//its letters 3 from each end of the arm - the line as it is
			//seen, up to the terminal, the strip's own piece in line with it
			//(CU-71.7)
		CHECK(std::abs(box.center().y() - bench.arm_y.at(i)) < 15);
		CHECK(box.left() >= trunk + 2.5);
		CHECK(box.right() <= p.last().x() - 2.5);
	}
		//The letters of no name on a line of another wire: not on the
		//common line, and not on a wire that crosses it - each name on a
		//piece of its own wire that holds it
	const QList<Piece> all = pieces(bench.sheet);
	QList<Conductor *> named = bench.point;
	named << bench.rest;
	for (const Conductor *c : std::as_const(named))
	{
		const QRectF box = bench.letters(c);
		int touched = 0;
		for (const Piece &piece : all)
		{
			if (piece.owner == c) continue;
			const QRectF line = QRectF(bench.drawn(piece.a), bench.drawn(piece.b))
					.normalized().adjusted(-0.5, -0.5, 0.5, 0.5);
			if (line.intersects(box)) ++touched;
		}
		CAPTURE(box, text(bench.path(c, c->terminal1)));
		CHECK(touched == 0);
	}
}

TEST_CASE("CU-67.24 — The common line moves away when a name does not fit", "[T67][wirearrangement]")
{
	const bool in_the_way = GENERATE(false, true);
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(in_the_way, way);
	Setup setup(1, false);
	QList<QPointF> blocks;
	if (in_the_way) blocks << QPointF(735, 390);
	Point bench(2, 0, false, way, 755, blocks);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	for (Conductor *c : std::as_const(bench.point)) Point::name(c, QStringLiteral("LONG-NAME-0001"));
	REQUIRE(WireArrangement::nameLength(bench.point.first(), bench.armRun()) > 30);
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);

	const double trunk = bench.trunk();
	CAPTURE(trunk, text(bench.path(bench.point.at(0), bench.terminal("C"))),
		text(bench.path(bench.point.at(1), bench.terminal("C"))));
	CHECK(trunk < 755);
	if (!in_the_way)
	{
			//Far enough for the letters of the name, at its own size, with
			//3 to spare at each end of the arm - and the name on it
		for (Conductor *c : std::as_const(bench.point))
		{
			const QList<QPointF> p = bench.path(c, bench.terminal("C"));
			const double arm = p.at(p.size() - 2).x() - trunk;
			const QRectF box = bench.letters(c);
			CAPTURE(text(p), arm, box);
			CHECK(arm >= WireArrangement::nameLength(c, bench.armRun()) + 6 - 0.01);
			CHECK(box.left() >= trunk + 2.5);
				//On the line as it is seen, up to the terminal (CU-71.7)
			CHECK(box.right() <= p.last().x() - 2.5);
		}
	}
	else
	{
			//Stopped before the component in its way. The name is then shown
			//once, on the line the point shares (CU-67.34): it was drawn
			//smaller on each arm, until the tenth adjustment
		const QRectF body = tagged(bench.sheet, "K0")->sceneBoundingRect();
		const double body_right = QRectF(bench.drawn(body.topLeft()),
						 bench.drawn(body.bottomRight())).normalized().right();
		CAPTURE(body_right);
		CHECK(trunk > body_right);
	}
}

TEST_CASE("CU-67.25 — Turned, the same common line", "[T67][wirearrangement]")
{
		//CU-67.21 asks every way of the eight; this one the way of the
		//request turned: the common terminal pointing right, the strip
		//below with its terminals pointing up
	Setup setup(1, false);
	Point bench(5, 0, false, Way{false, false, true});
	REQUIRE(bench.open());
	REQUIRE(bench.terminal("C")->dockConductor() == QPointF(321, 735));
	REQUIRE(bench.terminal("A0")->orientation() == Qet::North);
	REQUIRE(bench.drawAll(false));
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);

		//On the folio: out of the common terminal to the right on one line,
		//and straight down from it into each terminal of the strip
	for (int i = 0 ; i < 5 ; ++i)
	{
		QList<QPointF> p = points(bench.point.at(i));
		if ((p.first() - QPointF(321, 735)).manhattanLength() > 0.5) std::reverse(p.begin(), p.end());
		CAPTURE(i, text(p));
		REQUIRE(p.size() >= 3);
		CHECK(p.at(1) == QPointF(bench.arm_y.at(i), 735));
		CHECK(p.last() == QPointF(bench.arm_y.at(i), 775));
	}
}

TEST_CASE("T67 — Side to side, the wires of a point share one bridge", "[T67][wirearrangement]")
{
	/*
		The same rule between two columns: a terminal pointing right, as
		drawn at (635, 400), wired to @a arms terminals of a strip on the
		right pointing left, at x 775, below it. Each wire would get a lane
		of its own; the point gets one bridge for all, in each of the eight
		ways the drawing can lie.
	*/
	const int arms = GENERATE(2, 4);
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(arms, way);
	Setup setup(1, false);
	Point layout(0, 0, false, way);
	QList<Instance> instances{layout.component(QPointF(635, 400), QPointF(1, 0), "C")};
	for (int i = 0 ; i < arms ; ++i) {
		instances << layout.component(QPointF(775, 430 + 46 * i), QPointF(-1, 0), Point::arm_labels[i]);
	}
	UiBench::ScratchProject project(fixtureXml(instances, QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	QList<Conductor *> wires;
	for (int i = 0 ; i < arms ; ++i)
	{
		Conductor *c = draw(sheet, tagged(sheet, "C")->terminals().at(0),
				    tagged(sheet, Point::arm_labels[i])->terminals().at(0));
		REQUIRE(c != nullptr);
		wires << c;
	}
	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);

		//One bridge: every wire goes across from the common terminal on
		//one line, and turns at the same place
	QList<double> turns;
	for (const Conductor *c : std::as_const(wires))
	{
		QList<QPointF> p = points(c);
		if ((p.first() - tagged(sheet, "C")->terminals().at(0)->dockConductor()).manhattanLength() > 0.5) {
			std::reverse(p.begin(), p.end());
		}
		for (QPointF &q : p) q = layout.drawn(q);
		CAPTURE(text(p));
		REQUIRE(p.size() >= 4);
		CHECK(p.at(0) == QPointF(635, 400));
		turns << p.at(2).x();
		CHECK(p.at(2).y() == Approx(400));
	}
	for (double x : std::as_const(turns)) {
		CHECK(x == Approx(turns.first()));
	}
}

TEST_CASE("CU-67.26 — The wire does not run over the text of a component", "[T67][wirearrangement]")
{
	/*
		A terminal pointing down at (300, 300), wired to the terminal of a
		strip on the left, lower, pointing right, at (100, 350); and a label
		on the line of the strip's terminal, between the two - of the
		component of the wire, as on the folio of the request, or of
		another. Straight down and across, as the reset draws it, the wire
		would run over it.
	*/
	const bool own = GENERATE(true, false);
	CAPTURE(own);
	Setup setup(1, false);
	QList<Instance> instances{{"east.elmt", 90, 350, "X"}};
	if (own) {
		instances << Instance{"valve.elmt", 300, 290, "LAMINA LONGA"};
	} else {
		instances << Instance{"south.elmt", 300, 290, "V"} << Instance{"note.elmt", 300, 250, "LAMINA LONGA"};
	}
	UiBench::ScratchProject project(fixtureXml(instances, QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *component = tagged(sheet, own ? "LAMINA LONGA" : "V");
	Element *strip = tagged(sheet, "X");
	Element *labelled = tagged(sheet, "LAMINA LONGA");
	REQUIRE((component && strip && labelled));
	REQUIRE(component->terminals().at(0)->dockConductor() == QPointF(300, 300));
	REQUIRE(strip->terminals().at(0)->dockConductor() == QPointF(100, 350));
		//A folio keeps the texts of its elements, and the bench's has none:
		//the label is given the way the folio gives it, from the symbol's
		//place for it
	auto *given = new DynamicElementTextItem(labelled);
	given->setTextFrom(DynamicElementTextItem::ElementInfo);
	given->setInfoName(QStringLiteral("label"));
	labelled->addDynamicTextItem(given);
		//Nearer the strip than the component: the widest room for the bridge
		//is between the label and the component, where the wire would run
		//over the label to reach the strip
	given->setPos(-140, own ? 52 : 92);
	REQUIRE(given->toPlainText() == QStringLiteral("LAMINA LONGA"));
	const QRectF label = given->sceneBoundingRect();
	CAPTURE(label);
		//The label is on the line of the strip's terminal, between the two
	REQUIRE(label.top() < 350);
	REQUIRE(label.bottom() > 350);
	REQUIRE(label.left() > 110);
	REQUIRE(label.right() < 300);

	Conductor *wire = draw(sheet, component->terminals().at(0), strip->terminals().at(0));
	REQUIRE(wire != nullptr);
	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);

	const QList<QPointF> p = points(wire);
	CAPTURE(text(p));
	int over = 0;
	for (int i = 0 ; i + 1 < p.size() ; ++i)
	{
		const QRectF piece = QRectF(p.at(i), p.at(i + 1)).normalized().adjusted(-0.5, -0.5, 0.5, 0.5);
		if (piece.intersects(label)) ++over;
	}
	CHECK(over == 0);
	CHECK((p.first() == QPointF(300, 300) || p.last() == QPointF(300, 300)));
	CHECK((p.first() == QPointF(100, 350) || p.last() == QPointF(100, 350)));
}

TEST_CASE("CU-67.27 — The wire goes round its own component", "[T67][wirearrangement]")
{
	/*
		The signal of the request: a component whose terminal, on its edge,
		points away from the strip it is wired to, the strip's terminal on
		the line of the component's body and pointing at it. Drawn as the
		program draws it, the wire comes back through the component. To the
		right, to the left, and up.
	*/
	struct Way67
	{
		const char *component;
		int x;
		int y;
		const char *strip;
		int strip_x;
		int strip_y;
	};
		//The fourth as on the folio of the request: the strip's terminal near
		//the top of the component, so that the wire goes round it above,
		//where its name, drawn under the run, would lie on the component
	const Way67 way = GENERATE(Way67{"wide_e.elmt", 900, 330, "east.elmt", 785, 352},
				   Way67{"wide_e.elmt", 900, 330, "east.elmt", 785, 300},
				   Way67{"wide_w.elmt", 500, 330, "terminal.elmt", 615, 352},
				   Way67{"wide_n.elmt", 600, 600, "north.elmt", 620, 760});
	CAPTURE(way.component, way.strip_y);
	Setup setup(1, false);
	UiBench::ScratchProject project(fixtureXml({{way.component, way.x, way.y, "C"},
						    {way.strip, way.strip_x, way.strip_y, "X"}}, QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *component = tagged(sheet, "C");
	Element *strip = tagged(sheet, "X");
	REQUIRE((component && strip));
	const QRectF body = component->sceneBoundingRect().adjusted(0.5, 0.5, -0.5, -0.5);
	Conductor *wire = draw(sheet, component->terminals().at(0), strip->terminals().at(0));
	REQUIRE(wire != nullptr);
		//A name as long as those of the request, which a run along the
		//component could take
	ConductorProperties properties = wire->properties();
	properties.text = QStringLiteral("F5-4 (X 1mm2)");
	wire->setProperties(properties);

	const auto inside = [&body](const QList<QPointF> &p) {
		int n = 0;
		for (int i = 0 ; i + 1 < p.size() ; ++i) {
			if (QRectF(p.at(i), p.at(i + 1)).normalized().adjusted(-0.01, -0.01, 0.01, 0.01).intersects(body)) ++n;
		}
		return n;
	};
		//As the program draws it, through the component
	CAPTURE(text(points(wire)));
	REQUIRE(inside(points(wire)) > 0);

	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);
	const QList<QPointF> p = points(wire);
	CAPTURE(text(p), body);
	CHECK(inside(p) == 0);
	const QPointF from = component->terminals().at(0)->dockConductor();
	const QPointF to = strip->terminals().at(0)->dockConductor();
	CHECK(((p.first() == from && p.last() == to) || (p.first() == to && p.last() == from)));
		//Nor its name on the component
	const qreal margin = wire->textItem()->document()->documentMargin();
	const QRectF letters = wire->textItem()->sceneBoundingRect().adjusted(margin, margin, -margin, -margin);
	const QRectF drawn = component->mapRectToScene(component->drawnRect());
	INFO("letters " << letters.left() << ".." << letters.right() << " x " << letters.top() << ".." << letters.bottom()
	     << ", component " << drawn.left() << ".." << drawn.right() << " x " << drawn.top() << ".." << drawn.bottom()
	     << ", size " << wire->textItem()->font().pointSize());
	CHECK_FALSE(letters.intersects(drawn));
}

TEST_CASE("CU-67.28 — Two points side by side", "[T67][wirearrangement]")
{
	/*
		The folio of the request: two common terminals side by side over
		one strip, their arms taking turns on its terminals - the arms of
		the one farther from the strip cross the common line of the nearer.
		As drawn: C at (735, 321) and D at (712, 321), both pointing down;
		the strip on the right, its terminals at x 775 pointing left, every
		23 from y 352. The larger point is arranged first: once the nearer,
		once the farther.
	*/
	const bool nearer_larger = GENERATE(true, false);
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(nearer_larger, way);
	Setup setup(1, false);
	const Point layout(0, 0, false, way);
	const int arms = 7;
	QList<Instance> instances{layout.component(QPointF(735, 321), QPointF(0, 1), "C"),
				  layout.component(QPointF(712, 321), QPointF(0, 1), "D")};
	for (int i = 0 ; i < arms ; ++i) {
		instances << layout.component(QPointF(775, 352 + 23 * i), QPointF(-1, 0), Point::arm_labels[i]);
	}
	UiBench::ScratchProject project(fixtureXml(instances, QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
		//Four arms for the larger point, on the even terminals; three for
		//the other, on the odd ones
	const char *larger = nearer_larger ? "C" : "D";
	const char *smaller = nearer_larger ? "D" : "C";
	struct Arm
	{
		Conductor *wire;
		const char *common;
		double x;
		double y;
	};
	QList<Arm> wires;
	for (int i = 0 ; i < arms ; ++i)
	{
		const bool of_larger = i % 2 == 0;
		const char *common = of_larger ? larger : smaller;
		Conductor *c = draw(sheet, tagged(sheet, common)->terminals().at(0),
				    tagged(sheet, Point::arm_labels[i])->terminals().at(0));
		REQUIRE(c != nullptr);
		wires << Arm{c, common, of_larger == nearer_larger ? 735.0 : 712.0, 352.0 + 23 * i};
	}
	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);

		//Each wire straight on from its common terminal, on the one line of
		//its point, to the height of its own terminal of the strip
	for (const Arm &arm : std::as_const(wires))
	{
		const QPointF from = tagged(sheet, arm.common)->terminals().at(0)->dockConductor();
		QList<QPointF> p = points(arm.wire);
		if ((p.first() - from).manhattanLength() > 0.5) std::reverse(p.begin(), p.end());
		for (QPointF &q : p) q = layout.drawn(q);
		CAPTURE(arm.common, arm.y, text(p));
		REQUIRE(p.size() >= 3);
		CHECK(p.at(0) == QPointF(arm.x, 321));
		CHECK(p.at(1) == QPointF(arm.x, arm.y));
		CHECK(p.last() == QPointF(775, arm.y));
	}
}

TEST_CASE("CU-67.29 — Bridges of other points past the common line", "[T67][wirearrangement]")
{
	/*
		The folio of the request: the wires of other points come from a
		column on the left, near the common line, to the terminals of the
		strip between the arms - each bridge across the height of one arm,
		and far enough from the next one to be alone in its room. Between
		the common line and the strip there is more room than on its left,
		but a bridge there crosses an arm. As drawn: the point of CU-67.21,
		5 arms every 46 from 352; the others from x 695, 30 above the
		terminal of the strip they go to. From x 721 the room left of the
		common line is too narrow for a bridge, and it stays between the
		common line and the strip, as before. (The automatic path gives the
		terminal on the left of the folio a longer stub than the one on the
		right: mirrored, the room on the left is 10 wider, and both rooms
		are chosen to hold that.)
	*/
	const int left_x = GENERATE(695, 721);
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(left_x, way);
	Setup setup(1, false);
	const Point layout(0, 0, false, way);
	QList<Instance> more;
	for (int k = 0 ; k < 4 ; ++k)
	{
		more << layout.component(QPointF(left_x, 345 + 46 * k), QPointF(1, 0), Point::left_labels[k])
		     << layout.component(QPointF(775, 375 + 46 * k), QPointF(-1, 0), Point::other_labels[k]);
	}
	Point bench(5, 46, false, way, 735, {}, 635, more);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	for (int k = 0 ; k < 4 ; ++k)
	{
		Conductor *c = draw(bench.sheet, bench.terminal(Point::left_labels[k]),
				    bench.terminal(Point::other_labels[k]));
		REQUIRE(c != nullptr);
		bench.rest << c;
	}
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);

	CHECK(bench.trunk() == Approx(735));
	for (int k = 0 ; k < 4 ; ++k)
	{
		const QList<QPointF> p = bench.path(bench.rest.at(k), bench.terminal(Point::left_labels[k]));
		CAPTURE(k, text(p));
		double bridge = std::nan("");
		for (int i = 0 ; i + 1 < p.size() ; ++i)
		{
			if (std::abs(p.at(i).x() - p.at(i + 1).x()) < 0.01 && std::abs(p.at(i).y() - p.at(i + 1).y()) > 0.01) {
				bridge = p.at(i).x();
			}
		}
		REQUIRE_FALSE(std::isnan(bridge));
		if (left_x == 695) {
				//Past the common line, where it crosses no arm
			CHECK(bridge < 735);
		} else {
			CHECK((bridge > 735 && bridge < 775));
		}
		CHECK(p.first() == QPointF(left_x, 345 + 46 * k));
		CHECK(p.last() == QPointF(775, 375 + 46 * k));
	}
}

TEST_CASE("CU-67.30 — Components stacked without a gap", "[T67][wirearrangement]")
{
	/*
		The folio of the request: two components stacked without a gap,
		each with a terminal on top and one below, 5 inside its box; the
		strip on the left, its terminals pointing right. As drawn: the
		components at x 900, from y 380 to 430 and from 430 to 480, their
		terminals at 385, 425, 435 and 475; the ends of the strip at x 760,
		at 421, 444, 467 and 490, as on the folio. The bodies of the strip
		took the height of the ends of the terminals, and the wires went
		past the terminal of the other component.
	*/
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(way);
	Setup setup(1, false);
	const Point layout(0, 0, false, way);
	const auto stack = [&](double y, const char *label) {
		const QPointF p = layout.place(QPointF(900, y));
		return Instance{way.turned ? "stack_h.elmt" : "stack_v.elmt", qRound(p.x()), qRound(p.y()), label};
	};
		//The terminals of the strip 8 inside a body 40 long, as a terminal
		//of a strip on the folio, so that its body reaches past the end of
		//the strip's own piece in every way
	const auto deep = [&](double y, const char *label) {
		const QPointF end = layout.place(QPointF(760, y));
		const QPointF towards = layout.turn(QPointF(1, 0));
		const char *definition = towards.y() > 0 ? "deep_s.elmt"
				       : towards.y() < 0 ? "deep_n.elmt"
				       : towards.x() > 0 ? "deep_e.elmt" : "deep_w.elmt";
		return Instance{definition, qRound(end.x() - 8 * towards.x()), qRound(end.y() - 8 * towards.y()), label};
	};
	const double rows[] = {421, 444, 467, 490};
	const double tips[] = {385, 425, 435, 475};
	const char *owners[] = {"V0", "V0", "V1", "V1"};
	QList<Instance> instances{stack(405, "V0"), stack(455, "V1")};
	for (int i = 0 ; i < 4 ; ++i) instances << deep(rows[i], Point::arm_labels[i]);
	UiBench::ScratchProject project(fixtureXml(instances, QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	QList<Conductor *> wires;
	for (int i = 0 ; i < 4 ; ++i)
	{
		Terminal *from = nullptr;
		for (Terminal *t : tagged(sheet, owners[i])->terminals()) {
			if ((layout.drawn(t->dockConductor()) - QPointF(900, tips[i])).manhattanLength() < 0.5) from = t;
		}
		REQUIRE(from != nullptr);
		Conductor *c = draw(sheet, from, tagged(sheet, Point::arm_labels[i])->terminals().at(0));
		REQUIRE(c != nullptr);
		wires << c;
	}
	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);

	QList<QList<QPointF>> paths;
	for (int i = 0 ; i < 4 ; ++i)
	{
		QList<QPointF> p = points(wires.at(i));
		for (QPointF &q : p) q = layout.drawn(q);
		if ((p.first() - QPointF(900, tips[i])).manhattanLength() > 0.5) std::reverse(p.begin(), p.end());
		paths << p;
		CAPTURE(i, text(p));
		REQUIRE(p.size() >= 3);
		CHECK(p.last() == QPointF(760, rows[i]));
			//No piece of it on the line of the terminals passes the end of
			//another one: out of its own to the side, or straight on to its
			//row of the strip when no other is in between
		for (int k = 0 ; k + 1 < p.size() ; ++k)
		{
			if (std::abs(p.at(k).x() - 900) > 0.5 || std::abs(p.at(k + 1).x() - 900) > 0.5) continue;
			const double low = std::min(p.at(k).y(), p.at(k + 1).y());
			const double high = std::max(p.at(k).y(), p.at(k + 1).y());
			for (int o = 0 ; o < 4 ; ++o) {
				if (o != i) CHECK_FALSE((tips[o] > low - 0.5 && tips[o] < high + 0.5));
			}
		}
			//The two that face each other between the components, out of
			//the very end of their terminals
		if (i == 1 || i == 2)
		{
			CHECK(p.at(1).y() == Approx(tips[i]));
			CHECK(p.at(1).x() < 899.5);
		}
	}
		//No two of them cross
	int crossed = 0;
	for (int i = 0 ; i < 4 ; ++i) for (int j = 0 ; j < 4 ; ++j)
	{
		if (i == j) continue;
		const QList<QPointF> &h = paths.at(i);
		const QList<QPointF> &v = paths.at(j);
		for (int a = 0 ; a + 1 < h.size() ; ++a) for (int b = 0 ; b + 1 < v.size() ; ++b)
		{
			if (std::abs(h.at(a).y() - h.at(a + 1).y()) > 0.01 || std::abs(v.at(b).x() - v.at(b + 1).x()) > 0.01) continue;
			const double x = v.at(b).x(), y = h.at(a).y();
			if (x > std::min(h.at(a).x(), h.at(a + 1).x()) + 0.5 && x < std::max(h.at(a).x(), h.at(a + 1).x()) - 0.5
			    && y > std::min(v.at(b).y(), v.at(b + 1).y()) + 0.5 && y < std::max(v.at(b).y(), v.at(b + 1).y()) - 0.5) {
				++crossed;
			}
		}
	}
	CAPTURE(text(paths.at(0)), text(paths.at(1)), text(paths.at(2)), text(paths.at(3)));
	CHECK(crossed == 0);
}

TEST_CASE("CU-71.7 — The name in the middle of the whole straight line", "[T71][wirearrangement]")
{
	/*
		The request: each arm of a point ends in the strip's own piece, in
		line with it - two segments of the path, one straight line to the
		eye. The name went in the middle of the longer segment, against the
		common line, with the room by the strip left empty. As drawn: the
		point of CU-67.21, its arms from the common line at 735 to the
		strip at 775; in the eight ways it can lie.
	*/
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(way);
	Setup setup(1, false);
	Point bench(3, 46, false, way);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	for (Conductor *c : std::as_const(bench.point)) Point::name(c, QStringLiteral("F5-9"), 5);
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	REQUIRE(bench.trunk() == Approx(735));

	for (int i = 0 ; i < bench.point.size() ; ++i)
	{
		const QList<QPointF> p = bench.path(bench.point.at(i), bench.terminal("C"));
		const QRectF letters = bench.letters(bench.point.at(i));
		CAPTURE(i, text(p), letters);
			//The arm is one straight line, drawn in more than one piece
		REQUIRE(p.size() >= 4);
			//On it, as far from the common line as from the strip
		CHECK(std::abs((letters.left() - 735) - (775 - letters.right())) < 1);
	}
}

namespace
{
		/// The four ways of a point that turns round its strip: as drawn,
		/// mirrored, upside down and both - the shape that turns round is
		/// never described turned (fourth adjustment)
	QList<Way> fourWays()
	{
		QList<Way> ways;
		const QList<Way> all = eightWays();
		for (const Way &way : all) {
			if (!way.turned) ways << way;
		}
		return ways;
	}

		/// Whether the piece from @a a to @a b runs into @a body
	bool runsInto(const QPointF &a, const QPointF &b, const QRectF &body)
	{
		const qreal left = std::min(a.x(), b.x()), right = std::max(a.x(), b.x());
		const qreal top = std::min(a.y(), b.y()), bottom = std::max(a.y(), b.y());
		return left < body.right() && right > body.left() && top < body.bottom() && bottom > body.top();
	}

		/// The wires of @a wires whose name is visible
	QList<Conductor *> shown(const QList<Conductor *> &wires)
	{
		QList<Conductor *> out;
		for (Conductor *c : wires) {
			if (c->textItem()->isVisible()) out << c;
		}
		return out;
	}
}

TEST_CASE("CU-67.31 — Round the strip, one trunk", "[T67][wirearrangement]")
{
	/*
		The request: the common terminal on the right of the strip, whose
		terminals point left, away from it - each wire goes down, past the
		strip, down to its terminal and into it from the left. Each took a
		lane of its own, and five lanes went over the component between the
		strip and the next column. As drawn: the common terminal at 900,
		the strip's ends at 775, a component at 720 with the variant.
	*/
	const int arms = GENERATE(2, 5);
	const bool in_the_way = GENERATE(false, true);
	const Way way = GENERATE(from_range(fourWays()));
	CAPTURE(arms, in_the_way, way);
	Setup setup(1, false);
	QList<QPointF> blocks;
	if (in_the_way) blocks << QPointF(720, 375);
	Point bench(arms, 46, false, way, 900, blocks);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);

	QList<QPointF> ways;
	for (int i = 0 ; i < arms ; ++i)
	{
		const QList<QPointF> p = bench.path(bench.point.at(i), bench.terminal("C"));
		CAPTURE(i, text(p));
		ways << laneAndRun(p);
			//Into its own terminal, from the left of the strip
		CHECK(p.last() == QPointF(775, bench.arm_y.at(i)));
		CHECK(p.at(p.size() - 2).x() < 775);
	}
		//One lane and one run for all of them
	for (int i = 1 ; i < arms ; ++i)
	{
		CAPTURE(i, ways);
		CHECK(ways.at(i).x() == Approx(ways.first().x()));
		CHECK(ways.at(i).y() == Approx(ways.first().y()));
	}
	CHECK(ways.first().x() < 775);
	if (in_the_way)
	{
			//And none of them over the component
		Element *block = tagged(bench.sheet, "K0");
		REQUIRE(block);
		const QRectF r = block->mapRectToScene(block->drawnRect());
		const QRectF body = QRectF(bench.drawn(r.topLeft()), bench.drawn(r.bottomRight())).normalized();
		for (const Conductor *c : std::as_const(bench.point))
		{
			const QList<QPointF> p = bench.path(c, bench.terminal("C"));
			CAPTURE(body, text(p));
			for (int i = 0 ; i + 1 < p.size() ; ++i) {
				CHECK_FALSE(runsInto(p.at(i), p.at(i + 1), body));
			}
		}
	}
}

TEST_CASE("CU-67.32 — One name when no arm holds it", "[T67][wirearrangement]")
{
	const Way way = GENERATE(from_range(fourWays()));
	CAPTURE(way);
	Setup setup(1, false);
	Point bench(5, 46, false, way, 900);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	for (Conductor *c : std::as_const(bench.point)) Point::name(c, QStringLiteral("F5-9 (■ 1mm²)"), 5);
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);

	const QPointF way0 = laneAndRun(bench.path(bench.point.first(), bench.terminal("C")));
	const double arm = 775 - way0.x();
	CAPTURE(way0, arm);
		//The case: the name does not fit the arm, at the smallest size
	REQUIRE(WireArrangement::nameLength(bench.point.first(), Qt::Horizontal) > arm - 6);

	const QList<Conductor *> names = shown(bench.point);
	REQUIRE(names.size() == 1);
		//Lying on the common line, between the trunk and the common terminal
	const QRectF box = bench.letters(names.first());
	CAPTURE(box);
	CHECK(names.first()->textItem()->rotation() == Approx(0));
	CHECK(std::abs(box.center().y() - way0.y()) < 15);
	CHECK(box.left() >= way0.x());
	CHECK(box.right() <= 900);
}

TEST_CASE("CU-67.33 — The trunk moved away, a name on each arm", "[T67][wirearrangement]")
{
	const Way way = GENERATE(from_range(fourWays()));
	CAPTURE(way);
	Setup setup(1, false);
	Point bench(5, 46, false, way, 900);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	for (Conductor *c : std::as_const(bench.point)) Point::name(c, QStringLiteral("F5-9 (■ 1mm²)"), 5);
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	const double lane = laneAndRun(bench.path(bench.point.first(), bench.terminal("C"))).x();
	REQUIRE(shown(bench.point).size() == 1);

		//The trunk of the wire to the farthest terminal, by its handle: its
		//longest piece running up or down, on the folio
	Conductor *c = bench.point.last();
	const QList<QPointF> p = points(c);
	QPointF middle;
	qreal longest = -1;
	for (int i = 0 ; i + 1 < p.size() ; ++i)
	{
		const QPointF a = p.at(i), b = p.at(i + 1);
		if (std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) > longest) {
			longest = std::abs(a.y() - b.y());
			middle = (a + b) / 2;
		}
	}
	c->setSelected(true);
	QetGraphicsHandlerItem *handle = nullptr;
	const auto items = bench.sheet->items(middle);
	for (QGraphicsItem *item : items) {
		if (item->type() == QetGraphicsHandlerItem::Type) {
			handle = qgraphicsitem_cast<QetGraphicsHandlerItem *>(item);
		}
	}
	REQUIRE(handle);
	const auto send = [&](QEvent::Type type, const QPointF &at) {
		QGraphicsSceneMouseEvent event(type);
		event.setScenePos(at);
		event.setButton(Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
		bench.sheet->sendEvent(handle, &event);
	};
	const QPointF away = middle + bench.turn(QPointF(-40, 0));
	send(QEvent::GraphicsSceneMousePress, middle);
	send(QEvent::GraphicsSceneMouseMove, (middle + away) / 2);
	send(QEvent::GraphicsSceneMouseMove, away);
	send(QEvent::GraphicsSceneMouseRelease, away);

		//The trunk of every wire of the point went with it, and each name
		//is on its own arm
	for (int i = 0 ; i < bench.point.size() ; ++i)
	{
		const QList<QPointF> q = bench.path(bench.point.at(i), bench.terminal("C"));
		CAPTURE(i, text(q));
		CHECK(laneAndRun(q).x() == Approx(lane - 40));
		CHECK(bench.point.at(i)->textItem()->isVisible());
		const QRectF box = bench.letters(bench.point.at(i));
		CAPTURE(box);
		CHECK(std::abs(box.center().y() - bench.arm_y.at(i)) < 15);
		CHECK(box.left() >= lane - 40 + 2.5);
		CHECK(box.right() <= 775 - 2.5);
	}

		//One undo for the whole drag: the trunk back, one name again
	bench.sheet->undoStack().undo();
	for (int i = 0 ; i < bench.point.size() ; ++i) {
		CHECK(laneAndRun(bench.path(bench.point.at(i), bench.terminal("C"))).x() == Approx(lane));
	}
	CHECK(shown(bench.point).size() == 1);
}

TEST_CASE("T67 — Moving a point decides its name once, at the end", "[T67][wirearrangement]")
{
	/*
		The request: dragging the common terminal of six wires had become
		slow - deciding at each step which wire shows the name of the point
		cost 330 ms a step on the folio. In a live move the names are
		decided once, at its end; and the point still shows its name once.
	*/
	Setup setup(1, false);
	Point bench(5, 46, false, Way{false, false, false}, 900);
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	for (Conductor *c : std::as_const(bench.point)) Point::name(c, QStringLiteral("F5-9 (■ 1mm²)"), 5);
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	REQUIRE(shown(bench.point).size() == 1);

	bench.sheet->clearSelection();
	bench.terminal("C")->parentElement()->setSelected(true);
	ElementsMover &mover = bench.sheet->elementsMover();
	REQUIRE(mover.beginMovement(bench.sheet) > 0);
	QElapsedTimer clock;
	clock.start();
	for (int i = 0 ; i < 40 ; ++i) {
		mover.continueMovement(QPointF(i % 2 ? -10 : 10, 0));
	}
	const qint64 steps = clock.elapsed();
		//During the move, nothing decided again: still one name
	CHECK(shown(bench.point).size() == 1);
	mover.endMovement();
	WARN("40 steps of a live move: " << steps << " ms");

	CHECK(shown(bench.point).size() == 1);
}

TEST_CASE("CU-67.34 — One name also where it does not turn round", "[T67][wirearrangement]")
{
	/*
		The point of CU-67.24 with a component in the way of its common
		line: the line stops before it, and no arm holds the name. The
		name was drawn smaller on each arm; it is shown once, on the line
		the point shares.
	*/
	const Way way = GENERATE(from_range(eightWays()));
	CAPTURE(way);
	Setup setup(1, false);
	Point bench(2, 0, false, way, 755, {QPointF(735, 390)});
	REQUIRE(bench.open());
	REQUIRE(bench.drawAll(false));
	for (Conductor *c : std::as_const(bench.point)) Point::name(c, QStringLiteral("LONG-NAME-0001"));
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	const double trunk = bench.trunk();
	CAPTURE(trunk);

	const QList<Conductor *> names = shown(bench.point);
	REQUIRE(names.size() == 1);
		//Not on an arm, which runs across: along the common line, beside
		//it and within it, at its own size
	const QRectF box = bench.letters(names.first());
	CAPTURE(box);
	CHECK(box.height() > box.width());
	CHECK(std::abs(box.center().x() - trunk) < 15);
	CHECK(box.top() >= 321 - 3);
	CHECK(box.bottom() <= Point::armY(1, 0) + 3);
	CHECK(names.first()->textItem()->font().pointSize() == names.first()->properties().text_size);

		//Arranged again, the name of the other one hidden: the common line
		//asks for the room of the names all the same, and stays
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	CHECK(bench.trunk() == Approx(trunk));
	CHECK(shown(bench.point).size() == 1);
}
