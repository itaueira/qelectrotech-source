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
#include "../../../../sources/editor/UndoCommand/addpartcommand.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/elementsmover.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/QPropertyUndoCommand/qpropertyundocommand.h"

#include <catch2/catch.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QTest>
#include <QUndoStack>

#include <cmath>

/*
	Undoing a move undoes that move (T75). The defect: a move made while an
	earlier gesture was left open - its release lost to a menu, a dialog or
	another window - went unrecorded, and the earlier gesture was recorded
	with its movement: Ctrl+Z moved back the line moved before, and left
	the last one where it was.

	The events go through the scene, the way a view delivers them, so that
	the scene picks the item under the mouse, selects it and lets it grab
	the mouse - none of which a press sent straight to the item does.
*/
namespace
{
	const char *empty_folio =
		"<project title=\"bench\" version=\"0.80\">"
		"<diagram title=\"Bench\" order=\"1\" height=\"600\" folio=\"1\""
		" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
		" displaycols=\"true\" displayrows=\"true\">"
		"<elements/><inputs/><conductors/>"
		"</diagram>"
		"</project>";

		/// A mouse event of @a type at @a at, the left button pressed at
		/// @a from; the screen is the scene four times over, so that a
		/// move of a few units is past the distance a drag starts at
	void mouse(QGraphicsScene *scene, QEvent::Type type, const QPointF &at, const QPointF &from)
	{
		QGraphicsSceneMouseEvent event(type);
		event.setButton(Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
									    : Qt::LeftButton);
		event.setScenePos(at);
		event.setScreenPos((at * 4).toPoint());
		event.setLastScenePos(at);
		event.setLastScreenPos((at * 4).toPoint());
		event.setButtonDownScenePos(Qt::LeftButton, from);
		event.setButtonDownScreenPos(Qt::LeftButton, (from * 4).toPoint());
		QCoreApplication::sendEvent(scene, &event);
	}

		/// A drag from @a from to @a to, in two moves, released when
		/// @a release
	void drag(QGraphicsScene *scene, const QPointF &from, const QPointF &to, bool release = true)
	{
		mouse(scene, QEvent::GraphicsSceneMousePress, from, from);
		mouse(scene, QEvent::GraphicsSceneMouseMove, (from + to) / 2, from);
		mouse(scene, QEvent::GraphicsSceneMouseMove, to, from);
		if (release) {
			mouse(scene, QEvent::GraphicsSceneMouseRelease, to, from);
		}
	}

		/// What a menu or a dialog opened over the folio does to a drag:
		/// the item loses the mouse, and its release never comes
	void loseTheRelease(QGraphicsScene *scene)
	{
		if (QGraphicsItem *grabber = scene->mouseGrabberItem()) {
			grabber->ungrabMouse();
		}
	}

	void key(QGraphicsScene *scene, QEvent::Type type, Qt::Key k)
	{
		QKeyEvent event(type, k, Qt::NoModifier);
		QCoreApplication::sendEvent(scene, &event);
	}

	bool same(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
	}

	struct Folio
	{
		Folio() : project(QString::fromLatin1(empty_folio), QStringLiteral("undomove.qet")) {}

		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			if (!sheet) return false;
				//Lines on the grid of 10, so that the grid of this
				//machine does not move them by itself
			a = new QetShapeItem(QPointF(400, 150), QPointF(600, 150), QetShapeItem::Line);
			b = new QetShapeItem(QPointF(400, 300), QPointF(600, 300), QetShapeItem::Line);
			sheet->addItem(a);
			sheet->addItem(b);
			return true;
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		QetShapeItem *a = nullptr;
		QetShapeItem *b = nullptr;
	};
}

TEST_CASE("CU-75.1 — A line moved after a lost release is undone", "[T75][undo]")
{
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());
	QUndoStack &stack = folio.sheet->undoStack();
	const QPointF a_start = folio.a->pos();
	const QPointF b_start = folio.b->pos();

		//The fixture is what it says: a whole drag of the line is one
		//step, and undoing it puts the line back
	drag(folio.sheet, QPointF(500, 150), QPointF(500, 170));
	REQUIRE(same(folio.a->pos(), a_start + QPointF(0, 20)));
	stack.undo();
	QCoreApplication::processEvents();
	REQUIRE(same(folio.a->pos(), a_start));

		//A drag of A whose release is lost, then a whole drag of B
	drag(folio.sheet, QPointF(500, 150), QPointF(500, 170), false);
	loseTheRelease(folio.sheet);
	REQUIRE(same(folio.a->pos(), a_start + QPointF(0, 20)));
	folio.sheet->clearSelection();
	drag(folio.sheet, QPointF(500, 300), QPointF(500, 340));
	REQUIRE(same(folio.b->pos(), b_start + QPointF(0, 40)));

		//Undo: B, the last moved, goes back, and A stays
	stack.undo();
	QCoreApplication::processEvents();
	CHECK(same(folio.b->pos(), b_start));
	CHECK(same(folio.a->pos(), a_start + QPointF(0, 20)));
		//And the move of A, left open, was not lost: undone next
	stack.undo();
	QCoreApplication::processEvents();
	CHECK(same(folio.a->pos(), a_start));
	CHECK(same(folio.b->pos(), b_start));
}

TEST_CASE("CU-75.2 — An arrow key let go mid-drag leaves the drag whole", "[T75][undo]")
{
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());
	QUndoStack &stack = folio.sheet->undoStack();
	const QPointF a_start = folio.a->pos();

	mouse(folio.sheet, QEvent::GraphicsSceneMousePress, QPointF(500, 150), QPointF(500, 150));
	mouse(folio.sheet, QEvent::GraphicsSceneMouseMove, QPointF(500, 160), QPointF(500, 150));
		//The fixture is what it says: the drag is under way, the line
		//holds the mouse, and nothing is written yet
	const int steps = stack.count();
	REQUIRE(folio.sheet->mouseGrabberItem() == folio.a);
	REQUIRE_FALSE(folio.sheet->elementsMover().isReady());
		//An arrow key let go while the mouse still drags: the drag goes on
	key(folio.sheet, QEvent::KeyRelease, Qt::Key_Right);
	CHECK_FALSE(folio.sheet->elementsMover().isReady());
	CHECK(stack.count() == steps);
	mouse(folio.sheet, QEvent::GraphicsSceneMouseMove, QPointF(500, 190), QPointF(500, 150));
	mouse(folio.sheet, QEvent::GraphicsSceneMouseRelease, QPointF(500, 190), QPointF(500, 150));
	REQUIRE(same(folio.a->pos(), a_start + QPointF(0, 40)));

	stack.undo();
	QCoreApplication::processEvents();
	CHECK(same(folio.a->pos(), a_start));
}

TEST_CASE("CU-75.3 — A line moved by the arrows after a lost release", "[T75][undo]")
{
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());
	QUndoStack &stack = folio.sheet->undoStack();
	const QPointF a_start = folio.a->pos();
	const QPointF b_start = folio.b->pos();

	drag(folio.sheet, QPointF(500, 150), QPointF(500, 170), false);
	loseTheRelease(folio.sheet);
	folio.sheet->clearSelection();
	folio.b->setSelected(true);
	key(folio.sheet, QEvent::KeyPress, Qt::Key_Down);
	key(folio.sheet, QEvent::KeyRelease, Qt::Key_Down);
		//The arrow moved B, and only B
	REQUIRE(folio.b->pos().y() > b_start.y());
	CHECK(same(folio.a->pos(), a_start + QPointF(0, 20)));

	stack.undo();
	QCoreApplication::processEvents();
	CHECK(same(folio.b->pos(), b_start));
	CHECK(same(folio.a->pos(), a_start + QPointF(0, 20)));
}

TEST_CASE("CU-75.4 — In the symbol editor, the last line moved is undone", "[T75][undo]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	auto *a = new PartLine(&editor);
	a->setLine(QLineF(QPointF(-40, -20), QPointF(40, -20)));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, a));
	auto *b = new PartLine(&editor);
	b->setLine(QLineF(QPointF(-40, 20), QPointF(40, 20)));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, b));
	const QPointF a_start = a->pos();
	const QPointF b_start = b->pos();

	drag(scene, QPointF(0, -20), QPointF(0, -10));
	REQUIRE_FALSE(same(a->pos(), a_start));
	const QPointF a_moved = a->pos();
	scene->clearSelection();
	drag(scene, QPointF(0, 20), QPointF(0, 40));
	REQUIRE_FALSE(same(b->pos(), b_start));

	scene->undoStack().undo();
		//The undo of a part is animated
	QTest::qWait(400);
	CHECK(same(b->pos(), b_start));
	CHECK(same(a->pos(), a_moved));
}

TEST_CASE("T75 — A property step that changes nothing is not kept", "[T75][undo]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	auto *line = new PartLine(&editor);
	line->setLine(QLineF(QPointF(-40, -20), QPointF(40, -20)));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, line));
	const int before = scene->undoStack().count();

	scene->undoStack().push(new QPropertyUndoCommand(line, "line", QVariant(line->line()),
							QVariant(line->line())));
	CHECK(scene->undoStack().count() == before);

		//And a step that does change something is kept, of course
	scene->undoStack().push(new QPropertyUndoCommand(line, "line", QVariant(line->line()),
							QVariant(QLineF(QPointF(-40, -20), QPointF(50, -20)))));
	CHECK(scene->undoStack().count() == before + 1);
}

TEST_CASE("CU-75.5 — A handle clicked without a drag leaves no step", "[T75][undo]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	auto *line = new PartLine(&editor);
	line->setLine(QLineF(QPointF(-40, -20), QPointF(40, -20)));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, line));
	auto *other = new PartLine(&editor);
	other->setLine(QLineF(QPointF(-40, 20), QPointF(40, 20)));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, other));

		//A real step first: the other line moved
	drag(scene, QPointF(0, 20), QPointF(0, 40));
	const QPointF other_moved = other->pos();
	REQUIRE_FALSE(same(other_moved, QPointF()));
	const int before = scene->undoStack().count();

		//The line selected shows its handles; one is clicked, not dragged
	scene->clearSelection();
	line->setSelected(true);
	mouse(scene, QEvent::GraphicsSceneMousePress, QPointF(40, -20), QPointF(40, -20));
	mouse(scene, QEvent::GraphicsSceneMouseRelease, QPointF(40, -20), QPointF(40, -20));
	CHECK(scene->undoStack().count() == before);

		//So that Ctrl+Z takes back the move, the last thing done
	scene->undoStack().undo();
	QTest::qWait(400);
	CHECK(same(other->pos(), QPointF()));
}
