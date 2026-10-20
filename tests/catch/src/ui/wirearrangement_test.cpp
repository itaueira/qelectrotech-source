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
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/wirearrangement.h"
#include "../../../../sources/wirepath.h"

#include <catch2/catch.hpp>

#include <QGraphicsSceneMouseEvent>
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
			+ definition("block.elmt", QString(), 20);
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
			int m_x;
			int m_y;
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

		//The four from 1 selected; the last one stays at 225, in the way
	const QList<Conductor *> selected = drawn.mid(1, 4);
	CHECK(WireArrangement::arrange(bench.sheet, selected, WireArrangement::Scope::Selection) == 4);
	CHECK(bridge(drawn.at(5)) == Approx(225));
	CHECK(bridge(drawn.at(3)) != Approx(230));
	for (int i = 1 ; i < 5 ; ++i)
	{
		CHECK(std::abs(bridge(drawn.at(i)) - 225) >= 4);
		for (int j = i + 1 ; j < 5 ; ++j) {
			CHECK(std::abs(bridge(drawn.at(i)) - bridge(drawn.at(j))) >= 4);
		}
	}

	bench.sheet->undoStack().undo();
	CHECK(bridge(drawn.at(1)) == Approx(225));
	CHECK(bridge(drawn.at(2)) == Approx(225));
	CHECK(bridge(drawn.at(3)) == Approx(230));
	CHECK(bridge(drawn.at(4)) == Approx(225));
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
