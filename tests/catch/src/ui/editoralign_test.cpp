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

#include "../../../../sources/editor/UndoCommand/addpartcommand.h"
#include "../../../../sources/editor/UndoCommand/alignpartscommand.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/elementview.h"
#include "../../../../sources/editor/graphicspart/partdynamictextfield.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partterminal.h"
#include "../../../../sources/editor/partletters.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QUndoStack>

#include <cmath>

/*
	Align and distribute in the symbol editor, by the letters of a text and
	on a grid of 1 (T73).
*/
namespace
{
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

	PartDynamicTextField *textAt(QETElementEditor &editor, const QString &text,
				     qreal size, const QPointF &place)
	{
		ElementScene *scene = editor.elementScene();
		auto *part = new PartDynamicTextField(&editor);
		QFont font = part->font();
		font.setPointSizeF(size);
		part->setFont(font);
		part->setText(text);
		part->setPos(place);
		scene->undoStack().push(new AddPartCommand(QStringLiteral("text"), scene, part));
		return part;
	}

	PartLine *lineAt(QETElementEditor &editor, const QLineF &line)
	{
		ElementScene *scene = editor.elementScene();
		auto *part = new PartLine(&editor);
		part->setLine(line);
		scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, part));
		return part;
	}

		/// Pushes @a mode on the selection of @a scene, as the menu does
	void align(ElementScene *scene, AlignPartsCommand::Mode mode)
	{
		auto *command = new AlignPartsCommand(scene, mode);
		if (command->childCount()) {
			scene->undoStack().push(command);
		} else {
			delete command;
		}
	}

	QRectF letters(const PartDynamicTextField *text)
	{
		return PartLetters::lettersInScene(text);
	}
}

TEST_CASE("CU-73.1 — The grid of 1 at the usual zoom", "[T73][editor]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	PartLine *line = lineAt(editor, QLineF(QPointF(-40, -20), QPointF(40, -20)));

		//The folio drawn at the usual zoom, which drew the grid of 10 and
		//made the parts land on it
	ElementView *view = editor.elementView();
	REQUIRE(view != nullptr);
	view->resize(400, 300);
	view->resetTransform();
	view->viewport()->grab();

	mouse(scene, QEvent::GraphicsSceneMousePress, QPointF(0, -20), QPointF(0, -20));
	mouse(scene, QEvent::GraphicsSceneMouseMove, QPointF(4, -20), QPointF(0, -20));
	mouse(scene, QEvent::GraphicsSceneMouseMove, QPointF(7, -17), QPointF(0, -20));
	mouse(scene, QEvent::GraphicsSceneMouseRelease, QPointF(7, -17), QPointF(0, -20));
	CHECK(line->pos() == QPointF(7, 3));
}

TEST_CASE("CU-73.2 — Two texts aligned by their letters", "[T73][editor]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	PartDynamicTextField *small = textAt(editor, QStringLiteral("RF2"), 5, QPointF(10.3, 10.6));
	PartDynamicTextField *large = textAt(editor, QStringLiteral("Folha:4-B11"), 9, QPointF(42.7, 31.2));
	scene->clearSelection();
	small->setSelected(true);
	large->setSelected(true);

	align(scene, AlignPartsCommand::AlignLeft);
	CHECK(letters(small).left() == Approx(letters(large).left()).margin(0.01));

	align(scene, AlignPartsCommand::AlignTop);
	CHECK(letters(small).top() == Approx(letters(large).top()).margin(0.01));
		//By the letters, and not by the box: the boxes of fonts 5 and 9
		//do not start at one height when their capitals do
	CHECK(std::abs(small->sceneBoundingRect().top() - large->sceneBoundingRect().top()) > 0.5);

	align(scene, AlignPartsCommand::AlignBottom);
	CHECK(letters(small).bottom() == Approx(letters(large).bottom()).margin(0.01));
}

TEST_CASE("CU-73.3 — The tag in the middle of the arrow", "[T73][editor]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	PartDynamicTextField *tag = textAt(editor, QStringLiteral("RF2"), 5, QPointF(13.3, -20));
	scene->clearSelection();
	tag->setSelected(true);

	align(scene, AlignPartsCommand::CentreOnVerticalAxis);
	CHECK(letters(tag).center().x() == Approx(0).margin(0.01));
	CHECK(tag->pos().y() == Approx(-20));

	SECTION("saved and read again, it stays")
	{
		QDomDocument document;
		const QDomElement saved = tag->toXml(document);
		auto *again = new PartDynamicTextField(&editor);
		scene->addItem(again);
		again->fromXml(saved);
		CHECK(letters(again).center().x() == Approx(0).margin(0.01));
	}
}

TEST_CASE("CU-73.4 — Lines and a terminal aligned", "[T73][editor]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	PartLine *a = lineAt(editor, QLineF(QPointF(-40, -20), QPointF(40, -20)));
	PartLine *b = lineAt(editor, QLineF(QPointF(-25, 10), QPointF(30, 10)));
	auto *terminal = new PartTerminal(&editor);
	terminal->setPos(QPointF(-33, 30));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("terminal"), scene, terminal));
	scene->clearSelection();
	a->setSelected(true);
	b->setSelected(true);
	terminal->setSelected(true);

	align(scene, AlignPartsCommand::AlignLeft);
	CHECK(a->sceneGeometricRect().left() == Approx(b->sceneGeometricRect().left()));
	CHECK(terminal->scenePos().x() == Approx(a->sceneGeometricRect().left()));
	CHECK(terminal->pos().x() == std::round(terminal->pos().x()));
}

TEST_CASE("CU-73.5 — Three texts distributed", "[T73][editor]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	PartDynamicTextField *one = textAt(editor, QStringLiteral("A1"), 5, QPointF(0, 0));
	PartDynamicTextField *two = textAt(editor, QStringLiteral("B2"), 9, QPointF(0, 3));
	PartDynamicTextField *three = textAt(editor, QStringLiteral("C3"), 7, QPointF(0, 60));
	scene->clearSelection();
	one->setSelected(true);
	two->setSelected(true);
	three->setSelected(true);
	const qreal first = letters(one).center().y();

	align(scene, AlignPartsCommand::DistributeVertical);
	const qreal pitch = letters(two).center().y() - letters(one).center().y();
	CHECK(pitch > 0);
	CHECK(letters(three).center().y() - letters(two).center().y() == Approx(pitch).margin(0.01));
		//The top one stays
	CHECK(letters(one).center().y() == Approx(first));
		//And none touches the next
	CHECK(letters(two).top() >= letters(one).bottom() - 0.01);
	CHECK(letters(three).top() >= letters(two).bottom() - 0.01);
}

TEST_CASE("CU-73.6 — Undone in one step", "[T73][editor]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	PartDynamicTextField *small = textAt(editor, QStringLiteral("RF2"), 5, QPointF(10.3, 10.6));
	PartDynamicTextField *large = textAt(editor, QStringLiteral("Folha:4-B11"), 9, QPointF(42.7, 31.2));
	scene->clearSelection();
	small->setSelected(true);
	large->setSelected(true);
	const QPointF small_start = small->pos();
	const QPointF large_start = large->pos();
	const int steps = scene->undoStack().count();

	align(scene, AlignPartsCommand::AlignRight);
	REQUIRE(scene->undoStack().count() == steps + 1);
	scene->undoStack().undo();
	CHECK(small->pos() == small_start);
	CHECK(large->pos() == large_start);
}
