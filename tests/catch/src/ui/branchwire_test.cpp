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

#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"
#include "../../../../sources/wirearrangement.h"

#include <catch2/catch.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QUndoStack>

/*
	A wire released over a wire branches off it (T86): the person draws the
	arm of a point once, from the terminal to the last of its components,
	and the others come off the arm. In the panel the wires of a point are
	joined at the terminal by a jumper, so a branch is still a wire of that
	terminal: only its drawing comes off the arm.

	The bench: a component B whose terminal points right, at (110, 100),
	and three components S1, S2 and S3 whose terminals point left, at
	(290, 160), (290, 220) and (290, 280). The automatic path of B to S3
	runs down at x = 200.
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

	QString fixture()
	{
		const QString definitions =
			definition("east.elmt", QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"))
			+ definition("west.elmt", QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>"));
		struct Instance { const char *definition; int x; int y; const char *label; };
		const QList<Instance> instances {
			{"east.elmt", 100, 100, "B"},
			{"west.elmt", 300, 160, "S1"},
			{"west.elmt", 300, 220, "S2"},
			{"west.elmt", 300, 280, "S3"}};
		QString xml;
		for (int i = 0 ; i < instances.size() ; ++i)
		{
			const Instance &instance = instances.at(i);
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"0\" type=\"embed://bench/%3\""
				" uuid=\"{86000000-0000-4000-8000-0000000000%4}\">"
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
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definitions, xml);
	}

		/// The grid of 1 of the machine the request came from, and no
		/// arranging of new wires, so that the arm is the automatic path
	class Setup
	{
		public:
			Setup() :
				m_x(Diagram::xGrid), m_y(Diagram::yGrid),
				m_arrange(WireArrangement::arrangesAutomatically())
			{
				Diagram::xGrid = 1;
				Diagram::yGrid = 1;
				WireArrangement::setArrangesAutomatically(false);
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

		/// A press on @a from and a release at @a at, as the user draws
	Conductor *drawTo(Diagram *sheet, Terminal *from, const QPointF &at)
	{
		const QList<Conductor *> before = sheet->conductors();
		QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
		press.setScenePos(from->dockConductor());
		press.setButton(Qt::LeftButton);
		press.setButtons(Qt::LeftButton);
		sheet->sendEvent(from, &press);
		QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
		release.setScenePos(at);
		release.setButton(Qt::LeftButton);
		sheet->sendEvent(from, &release);
		const QList<Conductor *> after = sheet->conductors();
		for (Conductor *c : after) {
			if (!before.contains(c)) return c;
		}
		return nullptr;
	}

	struct Bench
	{
		Bench() : project(fixture()) {}
		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			for (Element *e : sheet->elements())
			{
				const QString label = e->actualLabel();
				if (label == QLatin1String("B")) b = e->terminals().at(0);
				else if (label == QLatin1String("S1")) s1 = e->terminals().at(0);
				else if (label == QLatin1String("S2")) s2 = e->terminals().at(0);
				else if (label == QLatin1String("S3")) s3 = e->terminals().at(0);
			}
			return b && s1 && s2 && s3;
		}
		Conductor *byUuid(const QUuid &uuid) const
		{
			for (Conductor *c : sheet->conductors()) {
				if (c->uuid() == uuid) return c;
			}
			return nullptr;
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		Terminal *b = nullptr;
		Terminal *s1 = nullptr;
		Terminal *s2 = nullptr;
		Terminal *s3 = nullptr;
	};

	qreal distanceToPath(const Conductor *c, const QPointF &point)
	{
		return QLineF(Conductor::nearestOnPath(c->scenePathPoints(), point), point).length();
	}

	bool sameAs(const QPointF &a, const QPointF &b)
	{
		return (a - b).manhattanLength() < 0.01;
	}

		/// Whether the path of @a branch is the path of @a arm from the
		/// terminal they share to the point of the branch
	bool sharesThePathOf(const Conductor *branch, const Conductor *arm)
	{
		QList<QPointF> along = arm->scenePathPoints();
		QList<QPointF> own = branch->scenePathPoints();
		Terminal *shared = Conductor::sharedTerminal(branch, arm);
		if (!shared) return false;
		if (shared != arm->terminal1) std::reverse(along.begin(), along.end());
		if (shared != branch->terminal1) std::reverse(own.begin(), own.end());
		const QPointF point = Conductor::nearestOnPath(along, branch->armPoint());
			//Every point of the branch up to the point lies on the arm
		for (const QPointF &p : std::as_const(own))
		{
			if (QLineF(Conductor::nearestOnPath(along, p), p).length() > 0.01) return false;
			if (sameAs(p, point)) return true;
		}
		return false;
	}
}

TEST_CASE("CU-86.1 - A wire released over a wire branches off it", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());

	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);
	REQUIRE(distanceToPath(arm, QPointF(200, 160)) < 0.01);

	const int steps = bench.sheet->undoStack().count();
	Conductor *branch = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	REQUIRE(branch);

		//A wire of the end of the arm nearer to the point, B, off the arm
	CHECK(branch->arm() == arm);
	CHECK(Conductor::sharedTerminal(branch, arm) == bench.b);
	CHECK((branch->terminal1 == bench.s1 || branch->terminal2 == bench.s1));
	CHECK(branch->relatedPotentialConductors(false).contains(arm));

		//Its path is the arm up to the point, then its own piece
	CHECK(sharesThePathOf(branch, arm));
	const QList<QPointF> path = branch->scenePathPoints();
	REQUIRE(branch->ownPieceStart() >= 0);
	REQUIRE(branch->ownPieceStart() < path.size());
	CHECK(sameAs(path.at(branch->ownPieceStart()), QPointF(200, 160)));

		//The dot where it comes off the arm
	bool dot = false;
	for (const QPointF &junction : branch->junctions()) {
		if (sameAs(branch->mapToScene(junction), QPointF(200, 160))) dot = true;
	}
	CHECK(dot);

		//One undo step
	CHECK(bench.sheet->undoStack().count() == steps + 1);
	bench.sheet->undoStack().undo();
	CHECK_FALSE(bench.sheet->conductors().contains(branch));
	bench.sheet->undoStack().redo();
	REQUIRE(bench.sheet->conductors().contains(branch));
	CHECK(branch->arm() == arm);
	CHECK(sharesThePathOf(branch, arm));
}

TEST_CASE("CU-86.2 - The branches follow the arm", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	Conductor *two = drawTo(bench.sheet, bench.s2, QPointF(199, 220));
	REQUIRE(one);
	REQUIRE(two);
	REQUIRE(two->arm() == arm);

		//The run of the arm that goes down, 20 to the right, by hand
	QList<QPointF> moved = arm->scenePathPoints();
	for (QPointF &p : moved) {
		if (qAbs(p.x() - 200) < 0.01) p.setX(220);
	}
	REQUIRE(arm->setPathPoints(moved));

	CHECK(distanceToPath(arm, QPointF(220, 160)) < 0.01);
	CHECK(sameAs(one->scenePathPoints().at(one->ownPieceStart()), QPointF(220, 160)));
	CHECK(sameAs(two->scenePathPoints().at(two->ownPieceStart()), QPointF(220, 220)));
	CHECK(sharesThePathOf(one, arm));
	CHECK(sharesThePathOf(two, arm));

		//And back with the undo
	bench.sheet->undoStack().undo();
	CHECK(sameAs(one->scenePathPoints().at(one->ownPieceStart()), QPointF(200, 160)));
	CHECK(sameAs(two->scenePathPoints().at(two->ownPieceStart()), QPointF(200, 220)));
}

TEST_CASE("CU-86.3 - Deleting the arm leaves the branches as they are", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	REQUIRE(one);
	REQUIRE(one->arm() == arm);
	const QList<QPointF> before = one->scenePathPoints();

	bench.sheet->clearSelection();
	arm->setSelected(true);
	DiagramContent selection(bench.sheet);
	bench.sheet->clearSelection();
	REQUIRE(selection.m_conductors_to_move.size() + selection.m_conductors_to_update.size()
			+ selection.m_other_conductors.size() >= 1);
	bench.sheet->undoStack().push(new DeleteQGraphicsItemCommand(bench.sheet, selection));

	REQUIRE_FALSE(bench.sheet->conductors().contains(arm));
	CHECK(one->arm() == nullptr);
	CHECK(one->scenePathPoints() == before);
	CHECK((one->terminal1 == bench.b || one->terminal2 == bench.b));

		//Back with the undo, the branch follows it again
	bench.sheet->undoStack().undo();
	REQUIRE(bench.sheet->conductors().contains(arm));
	CHECK(one->arm() == arm);
	CHECK(sharesThePathOf(one, arm));
}

TEST_CASE("CU-86.4 - The branch saved and opened again", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	REQUIRE(one);
	const QUuid arm_uuid = arm->uuid();
	const QUuid one_uuid = one->uuid();
	const QList<QPointF> path = one->scenePathPoints();

	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.saveAndReopen());
	REQUIRE(bench.open());
	Conductor *arm_again = bench.byUuid(arm_uuid);
	Conductor *one_again = bench.byUuid(one_uuid);
	REQUIRE(arm_again);
	REQUIRE(one_again);
	CHECK(one_again->arm() == arm_again);
	CHECK(one_again->scenePathPoints() == path);

		//And it still follows the arm
	QList<QPointF> moved = arm_again->scenePathPoints();
	for (QPointF &p : moved) {
		if (qAbs(p.x() - 200) < 0.01) p.setX(220);
	}
	REQUIRE(arm_again->setPathPoints(moved));
	CHECK(sameAs(one_again->scenePathPoints().at(one_again->ownPieceStart()), QPointF(220, 160)));
}

TEST_CASE("CU-86.5 - Arranging the folio keeps the branches on the arm", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	Conductor *two = drawTo(bench.sheet, bench.s2, QPointF(201, 220));
	REQUIRE(one);
	REQUIRE(two);

	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);

	CHECK(one->arm() == arm);
	CHECK(two->arm() == arm);
	CHECK(sharesThePathOf(one, arm));
	CHECK(sharesThePathOf(two, arm));
	CHECK(distanceToPath(arm, one->scenePathPoints().at(one->ownPieceStart())) < 0.01);
	CHECK(distanceToPath(arm, two->scenePathPoints().at(two->ownPieceStart())) < 0.01);
}

TEST_CASE("CU-86.6 - A branch off a branch", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	REQUIRE(one);
	Conductor *sub = drawTo(bench.sheet, bench.s2, QPointF(250, 161));
	REQUIRE(sub);
	CHECK(sub->arm() == one);
	CHECK(sharesThePathOf(sub, one));

		//The arm moved, the branch and its branch go with it
	QList<QPointF> moved = arm->scenePathPoints();
	for (QPointF &p : moved) {
		if (qAbs(p.x() - 200) < 0.01) p.setX(220);
	}
	REQUIRE(arm->setPathPoints(moved));
	CHECK(sharesThePathOf(one, arm));
	CHECK(sharesThePathOf(sub, one));

		//And no branch makes its arm a branch of itself
	CHECK_FALSE(arm->canBranchOff(one));
	CHECK_FALSE(one->canBranchOff(sub));
}

namespace
{
		/// Whether every segment of @a points runs along x or along y
	bool upright(const QList<QPointF> &points)
	{
		for (int i = 0 ; i + 1 < points.size() ; ++i)
		{
			if (qAbs(points.at(i).x() - points.at(i + 1).x()) > 0.01
			    && qAbs(points.at(i).y() - points.at(i + 1).y()) > 0.01) return false;
		}
		return true;
	}

		/// The handlers on the folio, the ones a hand can take
	QList<QetGraphicsHandlerItem *> handlers(Diagram *sheet)
	{
		QList<QetGraphicsHandlerItem *> list;
		for (QGraphicsItem *item : sheet->items()) {
			if (auto *h = qgraphicsitem_cast<QetGraphicsHandlerItem *>(item)) list << h;
		}
		return list;
	}

		/// A handler taken and moved by @a movement, as a hand does
	void drag(Diagram *sheet, QetGraphicsHandlerItem *handler, const QPointF &movement)
	{
		const QPointF from = handler->scenePos();
		QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
		press.setScenePos(from);
		press.setButton(Qt::LeftButton);
		press.setButtons(Qt::LeftButton);
		sheet->sendEvent(handler, &press);
		QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
		move.setScenePos(from + movement);
		move.setButtons(Qt::LeftButton);
		sheet->sendEvent(handler, &move);
		QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
		release.setScenePos(from + movement);
		release.setButton(Qt::LeftButton);
		sheet->sendEvent(handler, &release);
	}
}

TEST_CASE("T86 - The branch leaves its terminal straight to the arm", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);

		//Dropped 30 below the line of S1, on the arm: it comes off the arm
		//where the line of S1 meets it, and runs straight (23/10/2026)
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 190));
	REQUIRE(one);
	REQUIRE(one->arm() == arm);
	CHECK(sameAs(one->scenePathPoints().at(one->ownPieceStart()), QPointF(200, 160)));
	const QList<QPointF> own = one->ownPiecePoints();
	REQUIRE(own.size() >= 2);
	CHECK(sameAs(own.first(), QPointF(200, 160)));
	CHECK(sameAs(own.last(), bench.s1->dockConductor()));
	for (const QPointF &p : own) {
		CHECK(qAbs(p.y() - 160) < 0.01);
	}
	CHECK(one->ownShape().isEmpty());
}

TEST_CASE("T86 - The own piece of a branch dragged like a wire", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	REQUIRE(arm);
	REQUIRE(one);
	REQUIRE(one->arm() == arm);

		//Selected, it offers handlers on its own piece only, right of the arm
	bench.sheet->clearSelection();
	one->setSelected(true);
	const QList<QetGraphicsHandlerItem *> offered = handlers(bench.sheet);
	REQUIRE_FALSE(offered.isEmpty());
	QetGraphicsHandlerItem *middle = nullptr;
	for (QetGraphicsHandlerItem *h : offered)
	{
		CHECK(h->scenePos().x() > 200);
		if (!middle || qAbs(h->scenePos().x() - 245) < qAbs(middle->scenePos().x() - 245)) middle = h;
	}

		//Its long segment taken down 20, as a wire between two components
	const int steps = bench.sheet->undoStack().count();
	drag(bench.sheet, middle, QPointF(0, 20));
	CHECK(bench.sheet->undoStack().count() == steps + 1);
	CHECK(one->arm() == arm);
	const QList<QPointF> shaped = one->ownPiecePoints();
	CHECK_FALSE(one->ownShape().isEmpty());
	CHECK(upright(one->scenePathPoints()));
	CHECK(sameAs(shaped.last(), bench.s1->dockConductor()));
	bool lowered = false;
	for (const QPointF &p : shaped) {
		if (qAbs(p.y() - 180) < 0.01) lowered = true;
	}
	CHECK(lowered);
	CHECK(sharesThePathOf(one, arm));

		//The shape kept when the component moves: the wire still upright,
		//still ending at its terminal
	Element *s1 = bench.s1->parentElement();
	s1->setPos(s1->pos() + QPointF(10, 10));
	one->updatePath();
	CHECK(upright(one->scenePathPoints()));
	CHECK(sameAs(one->ownPiecePoints().last(), bench.s1->dockConductor()));
	s1->setPos(s1->pos() - QPointF(10, 10));
	one->updatePath();

		//Saved and opened again with its shape
	const QUuid one_uuid = one->uuid();
	const QList<QPointF> path = one->scenePathPoints();
	REQUIRE(bench.project.saveAndReopen());
	REQUIRE(bench.open());
	Conductor *again = bench.byUuid(one_uuid);
	REQUIRE(again);
	CHECK(again->arm() != nullptr);
	CHECK(again->scenePathPoints() == path);
	CHECK_FALSE(again->ownShape().isEmpty());
}

TEST_CASE("T86 - Undoing the shape of a branch straightens it again", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(201, 160));
	REQUIRE(arm);
	REQUIRE(one);
	const QList<QPointF> straight = one->scenePathPoints();
	bench.sheet->clearSelection();
	one->setSelected(true);
	QetGraphicsHandlerItem *middle = nullptr;
	for (QetGraphicsHandlerItem *h : handlers(bench.sheet)) {
		if (!middle || qAbs(h->scenePos().x() - 245) < qAbs(middle->scenePos().x() - 245)) middle = h;
	}
	REQUIRE(middle);
	drag(bench.sheet, middle, QPointF(0, 20));
	REQUIRE_FALSE(one->ownShape().isEmpty());

	bench.sheet->undoStack().undo();
	CHECK(one->ownShape().isEmpty());
	CHECK(one->scenePathPoints() == straight);
	bench.sheet->undoStack().redo();
	CHECK_FALSE(one->ownShape().isEmpty());
}

TEST_CASE("T86 - An arm the arrangement cannot place stays as a wire alone", "[T86][branch]")
{
	Setup setup;
	Bench bench;
	REQUIRE(bench.open());
	Conductor *arm = drawTo(bench.sheet, bench.b, bench.s3->dockConductor());
	REQUIRE(arm);

		//Alone, the arm is a wire like any other: its run put at x = 150 by
		//an arrangement, arranging the folio gives it its lane again
	QList<QPointF> shifted = arm->scenePathPoints();
	for (QPointF &p : shifted) {
		if (qAbs(p.x() - 200) < 0.01) p.setX(150);
	}
	REQUIRE(arm->setArrangedPath(shifted));
	const auto planned = [&bench](const Conductor *c) {
		const QList<WireArrangement::Change> changes
				= WireArrangement::plan(bench.sheet, {}, WireArrangement::Scope::Folio);
		for (const WireArrangement::Change &change : changes) {
			if (change.conductor == c) return true;
		}
		return false;
	};
	REQUIRE(planned(arm));

		//With branches, it is never arranged as a wire alone (23/10/2026:
		//so arranged, it went to the middle of the corridor and its branches
		//crossed every wire on the way to it). Between two terminals that
		//point to the side, it is no arm the arrangement places next to a
		//column either: it stays where it is
	Conductor *one = drawTo(bench.sheet, bench.s1, QPointF(151, 160));
	REQUIRE(one);
	REQUIRE(one->arm() == arm);
	CHECK_FALSE(planned(arm));
	const QList<QPointF> path = arm->scenePathPoints();
	WireArrangement::arrange(bench.sheet, {}, WireArrangement::Scope::Folio);
	CHECK(arm->scenePathPoints() == path);
	CHECK(sharesThePathOf(one, arm));
}

namespace
{
		/// Two components at the top, their terminal pointing down, at x 480
		/// and 520; a column of eight terminals of a strip pointing right, at
		/// x 350, from y 200 to 480 - the corridor of a folio of a panel
	QString columnFixture()
	{
		const QString definitions =
			definition("south.elmt", QStringLiteral("<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"))
			+ definition("east.elmt", QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"));
		QString xml;
		const auto add = [&xml](const char *def, int x, int y, const QString &label, int n) {
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"0\" type=\"embed://bench/%3\""
				" uuid=\"{86100000-0000-4000-8000-0000000000%4}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(x).arg(y).arg(QLatin1String(def)).arg(n, 2, 10, QLatin1Char('0')).arg(label);
		};
		add("south.elmt", 480, 100, QStringLiteral("T1"), 0);
		add("south.elmt", 520, 100, QStringLiteral("T2"), 1);
		for (int i = 0 ; i < 8 ; ++i) {
			add("east.elmt", 350, 200 + 40 * i, QStringLiteral("X%1").arg(i + 1), 10 + i);
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"800\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"10\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definitions, xml);
	}

	Terminal *terminalOf(Diagram *sheet, const QString &label)
	{
		for (Element *e : sheet->elements()) {
			if (e->actualLabel() == label) return e->terminals().value(0);
		}
		return nullptr;
	}

		/// The x of the run of @a arm that goes down, the longest one
	qreal laneOf(const Conductor *arm)
	{
		const QList<QPointF> path = arm->scenePathPoints();
		qreal lane = std::nan("");
		qreal longest = 0;
		for (int i = 0 ; i + 1 < path.size() ; ++i)
		{
			const qreal length = qAbs(path.at(i + 1).y() - path.at(i).y());
			if (qAbs(path.at(i + 1).x() - path.at(i).x()) < 0.01 && length > longest)
			{
				longest = length;
				lane = path.at(i).x();
			}
		}
		return lane;
	}
}

TEST_CASE("T86 - Arranging puts each arm next to the column of its branches", "[T86][branch]")
{
	Setup setup;
	UiBench::ScratchProject project(columnFixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Terminal *t1 = terminalOf(sheet, QStringLiteral("T1"));
	Terminal *t2 = terminalOf(sheet, QStringLiteral("T2"));
	QList<Terminal *> x;
	for (int i = 1 ; i <= 8 ; ++i) x << terminalOf(sheet, QStringLiteral("X%1").arg(i));
	REQUIRE(t1);
	REQUIRE(t2);
	REQUIRE_FALSE(x.contains(nullptr));

		//Two arms down the middle of the corridor, as drawing them gives,
		//and their branches off them
	Conductor *arm1 = drawTo(sheet, t1, x.at(3)->dockConductor());
	Conductor *arm2 = drawTo(sheet, t2, x.at(7)->dockConductor());
	REQUIRE(arm1);
	REQUIRE(arm2);
	QList<Conductor *> branches1, branches2;
	for (int i : {0, 1, 2}) {
		branches1 << drawTo(sheet, x.at(i), QPointF(laneOf(arm1) + 1, x.at(i)->dockConductor().y()));
	}
	for (int i : {4, 5, 6}) {
		branches2 << drawTo(sheet, x.at(i), QPointF(laneOf(arm2) + 1, x.at(i)->dockConductor().y()));
	}
	for (Conductor *b : branches1) {
		REQUIRE(b);
		REQUIRE(b->arm() == arm1);
	}
	for (Conductor *b : branches2) {
		REQUIRE(b);
		REQUIRE(b->arm() == arm2);
	}
	const qreal column = x.at(0)->dockConductor().x();
	REQUIRE(laneOf(arm1) > column + 100);

	const int steps = sheet->undoStack().count();
	WireArrangement::arrange(sheet, {}, WireArrangement::Scope::Folio);
	CHECK(sheet->undoStack().count() == steps + 1);

		//Each arm next to the column, beyond the room of a name, and a pitch
		//from the other one
	const qreal lane1 = laneOf(arm1);
	const qreal lane2 = laneOf(arm2);
	CHECK(lane1 > column + 20);
	CHECK(lane1 < column + 100);
	CHECK(lane2 > column + 20);
	CHECK(lane2 < column + 100);
	CHECK(qAbs(lane1 - lane2) >= 15 - 0.01);
		//Nowhere one over the other: from terminals at one height, their
		//runs to their lanes were on one line (23/10/2026)
	const QList<QPointF> p1 = arm1->scenePathPoints();
	const QList<QPointF> p2 = arm2->scenePathPoints();
	for (int i = 0 ; i + 1 < p1.size() ; ++i)
	{
		for (int j = 0 ; j + 1 < p2.size() ; ++j)
		{
			const QPointF a = p1.at(i), b = p1.at(i + 1), c = p2.at(j), d = p2.at(j + 1);
			if (qAbs(a.y() - b.y()) < 0.01 && qAbs(c.y() - d.y()) < 0.01 && qAbs(a.y() - c.y()) < 2)
			{
				const qreal lo = std::max(std::min(a.x(), b.x()), std::min(c.x(), d.x()));
				const qreal hi = std::min(std::max(a.x(), b.x()), std::max(c.x(), d.x()));
				CHECK(hi - lo <= 1);
			}
			if (qAbs(a.x() - b.x()) < 0.01 && qAbs(c.x() - d.x()) < 0.01 && qAbs(a.x() - c.x()) < 2)
			{
				const qreal lo = std::max(std::min(a.y(), b.y()), std::min(c.y(), d.y()));
				const qreal hi = std::min(std::max(a.y(), b.y()), std::max(c.y(), d.y()));
				CHECK(hi - lo <= 1);
			}
		}
	}

		//And every branch straight from its terminal to its arm
	for (Conductor *b : branches1 + branches2)
	{
		const QList<QPointF> own = b->ownPiecePoints();
		REQUIRE(own.size() >= 2);
		const qreal lane = b->arm() == arm1 ? lane1 : lane2;
		CHECK(qAbs(own.first().x() - lane) < 0.01);
		for (const QPointF &p : own) {
			CHECK(qAbs(p.y() - own.last().y()) < 0.01);
		}
		CHECK(b->ownShape().isEmpty());
	}

		//One undo puts it all back
	sheet->undoStack().undo();
	CHECK(laneOf(arm1) > column + 100);
}
