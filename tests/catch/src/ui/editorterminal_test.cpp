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

#include "../../../../sources/QPropertyUndoCommand/qpropertyundocommand.h"
#include "../../../../sources/editor/UndoCommand/addpartcommand.h"
#include "../../../../sources/editor/editorcommands.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partterminal.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"

#include <catch2/catch.hpp>

#include <QApplication>
#include <QByteArray>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QIODevice>
#include <QKeyEvent>
#include <QLineF>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>

/*
	T72: a terminal of a symbol stands on whole numbers, whatever the
	symbol editor does with it.

	A terminal off the whole number is aligned by no grid of a folio. An
	input module drawn with its terminals at x -21.92 and y -15.83, -4.83,
	6.17 and 17.17, placed at a whole place of a folio, has a terminal at
	y 628.17: a wire from an arrow at 628 keeps a step of 0.17, and neither
	a grid of 1 nor one of 0.1 can take it out.

	The editor now keeps the terminal on whole numbers when it moves
	(PartTerminal::itemChange()) and writes it so (PartTerminal::toXml()).
	The drawing is not touched: only the terminal moves, by half a unit
	at most.

	A place is compared as the text "x, y", which says where the terminal
	is when a check fails; in a saved file, as its attributes are written,
	so that "22.0" or "21.92" cannot pass for "22".
*/

namespace
{
	/// @return @a point as "x, y"
	QString placeOf(const QPointF &point)
	{
		return QStringLiteral("%1, %2").arg(point.x()).arg(point.y());
	}

	/// @return the places of the terminals of @a scene, sorted
	QStringList scenePlaces(ElementScene *scene)
	{
		QStringList places;
		const QList<QGraphicsItem *> items = scene->items();
		for (QGraphicsItem *item : items) {
			if (auto terminal = qgraphicsitem_cast<PartTerminal *>(item)) {
				places << placeOf(terminal->pos());
			}
		}
		places.sort();
		return places;
	}

	/// @return the places of the terminals of the symbol saved at @a path,
	/// as its attributes are written, sorted
	QStringList savedPlaces(const QString &path)
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		QDomDocument document;
		REQUIRE(document.setContent(&file));
		QStringList places;
		for (QDomElement terminal = document.documentElement()
						    .firstChildElement(QStringLiteral("description"))
						    .firstChildElement(QStringLiteral("terminal"));
		     !terminal.isNull();
		     terminal = terminal.nextSiblingElement(QStringLiteral("terminal")))
		{
			places << terminal.attribute(QStringLiteral("x")) + QStringLiteral(", ")
					  + terminal.attribute(QStringLiteral("y"));
		}
		places.sort();
		return places;
	}

	/// @return the places of the terminals of @a editor, once saved in @a dir
	QStringList savedBy(QETElementEditor &editor, const QTemporaryDir &dir)
	{
		const QString path = dir.filePath(QStringLiteral("saved.elmt"));
		REQUIRE(editor.toFile(path));
		return savedPlaces(path);
	}

	/**
		A terminal put on the symbol of @a editor at @a place, the way the
		editor adds one (ESEventAddTerminal), after a line across the
		insertion point: the symbol then holds the point (0, 0), and saving
		it does not move it to the origin (ElementScene::toXml()) - the
		terminal is written where it stands.
	*/
	PartTerminal *terminalPutAt(QETElementEditor &editor, const QPointF &place)
	{
		ElementScene *scene = editor.elementScene();
		REQUIRE(scene != nullptr);

		auto *line = new PartLine(&editor);
		line->setLine(QLineF(QPointF(-30, -30), QPointF(30, 30)));
		scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, line));

		auto *terminal = new PartTerminal(&editor);
		terminal->setPos(place);
		scene->undoStack().push(new AddPartCommand(QStringLiteral("terminal"), scene, terminal));
		return terminal;
	}
}

	//Kept short: Catch2 v2 wraps a long name to print a failure, and with a
	//multi-byte character in it the wrap can end the run without a summary
TEST_CASE("CU-72.5 — The editor puts the terminal on whole numbers", "[T72][editor]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);

		//Put where the module of the defect has one: off the grid of the
		//editor, as Ctrl held or a typed place puts it
	PartTerminal *terminal = terminalPutAt(editor, QPointF(21.92, 17.17));
	CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));

	SECTION("saved, the file has it on the whole number")
	{
		CHECK(savedBy(editor, dir) == QStringList{QStringLiteral("22, 17")});
	}

	SECTION("moved by 0.4, it stays on the whole number")
	{
		terminal->moveBy(0.4, 0.4);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));
		terminal->moveBy(-0.4, -0.4);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));
		CHECK(savedBy(editor, dir) == QStringList{QStringLiteral("22, 17")});
	}

	SECTION("a place typed with decimals lands on the whole number")
	{
			//As the properties of a terminal push it (TerminalEditor::posEdited())
		auto *typed = new QPropertyUndoCommand(terminal, "pos", terminal->property("pos"),
						       QPointF(-4.83, 6.17));
		scene->undoStack().push(typed);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("-5, 6"));
		CHECK(savedBy(editor, dir) == QStringList{QStringLiteral("-5, 6")});

		scene->undoStack().undo();
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));
	}

	SECTION("Ctrl and an arrow move it by a whole step, not a tenth")
	{
			//A tenth would be rounded back where it was, and the key would
			//do nothing
		scene->clearSelection();
		terminal->setSelected(true);
		REQUIRE(scene->selectedItems().size() == 1);

		QKeyEvent press(QEvent::KeyPress, Qt::Key_Right, Qt::ControlModifier);
		QApplication::sendEvent(scene, &press);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("23, 17"));

		QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::ControlModifier);
		QApplication::sendEvent(scene, &down);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("23, 18"));
	}
}

TEST_CASE("T72 — A symbol read with terminals off the whole number", "[T72][editor]")
{
		//The terminals of the input module of the defect, around a drawing
		//that holds the insertion point
	const QByteArray module = QByteArrayLiteral(
		"<definition type=\"element\" version=\"0.80\" width=\"50\" height=\"50\""
		" hotspot_x=\"25\" hotspot_y=\"25\" link_type=\"simple\">"
		"<names><name lang=\"en\">Input module</name></names>"
		"<description>"
		"<rect x=\"-20\" y=\"-20\" width=\"40\" height=\"40\" antialias=\"false\""
		" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
		"<terminal x=\"-21.92\" y=\"-15.83\" orientation=\"w\" name=\"1\"/>"
		"<terminal x=\"-21.92\" y=\"-4.83\" orientation=\"w\" name=\"2\"/>"
		"<terminal x=\"-21.92\" y=\"6.17\" orientation=\"w\" name=\"3\"/>"
		"<terminal x=\"-21.92\" y=\"17.17\" orientation=\"w\" name=\"4\"/>"
		"</description>"
		"</definition>");

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("module.elmt"));
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		REQUIRE(file.write(module) == module.size());
	}
		//What the case is about: a file with every terminal off the whole number
	REQUIRE(savedPlaces(path).size() == 4);
	REQUIRE(savedPlaces(path).first() == QStringLiteral("-21.92, -15.83"));

	QETElementEditor editor;
	editor.fromFile(path);
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);

	QStringList whole = {QStringLiteral("-22, -16"), QStringLiteral("-22, -5"),
			     QStringLiteral("-22, 6"), QStringLiteral("-22, 17")};
	whole.sort();
	CHECK(scenePlaces(scene) == whole);
	CHECK(savedBy(editor, dir) == whole);
}

TEST_CASE("T72 — Undone, the terminal comes back to its whole place", "[T72][editor]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	PartTerminal *terminal = terminalPutAt(editor, QPointF(22, 17));
	REQUIRE(placeOf(terminal->pos()) == QStringLiteral("22, 17"));

	SECTION("a selection moved by a fraction")
	{
			//As the selection is moved and the move pushed
			//(ElementPrimitiveDecorator): moved first, the redo of the push
			//skipped
		terminal->moveBy(0.6, -0.6);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("23, 16"));
		scene->undoStack().push(new MovePartsCommand(
			QPointF(0.6, -0.6), scene, QList<QGraphicsItem *>{terminal}));
		CHECK(placeOf(terminal->pos()) == QStringLiteral("23, 16"));

		scene->undoStack().undo();
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));
		scene->undoStack().redo();
		CHECK(placeOf(terminal->pos()) == QStringLiteral("23, 16"));
	}

	SECTION("a selection shrunk by half")
	{
			//From (22, 17) to (-4, -6.5), rounded to (-4, -6). Scaled back
			//from the rounded place, -6 would go to 18: the scaling has to
			//start from where the last one put the terminal
		const QRectF before(-30, -30, 60, 60);
		const QRectF after(-30, -30, 30, 30);
		terminal->startUserTransformation(before);
		terminal->handleUserTransformation(before, after);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("-4, -6"));

		auto *scale = new ScalePartsCommand(scene);
		scale->setScaledPrimitives(QList<CustomElementPart *>{terminal});
		scale->setTransformation(before, after);
		scene->undoStack().push(scale);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("-4, -6"));

		scene->undoStack().undo();
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));
		scene->undoStack().redo();
		CHECK(placeOf(terminal->pos()) == QStringLiteral("-4, -6"));
		scene->undoStack().undo();
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));
	}

	SECTION("mirrored and flipped twice across an axis on a quarter")
	{
			//The mirror of a selection is undone by mirroring it again
			//(MirrorElementsCommand::undo()). Across 0.25, 22 lands on
			//-21.5: rounded away from zero it would go to -22, and come
			//back on 22.5, then 23
		terminal->mirror(0.25);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("-21, 17"));
		terminal->mirror(0.25);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));

		terminal->flip(0.25);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, -16"));
		terminal->flip(0.25);
		CHECK(placeOf(terminal->pos()) == QStringLiteral("22, 17"));
	}
}
