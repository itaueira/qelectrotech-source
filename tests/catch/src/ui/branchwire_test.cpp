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
