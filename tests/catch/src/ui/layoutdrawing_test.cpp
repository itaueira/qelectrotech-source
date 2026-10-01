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
#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partrectangle.h"
#include "../../../../sources/editor/graphicspart/partterminal.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/factory/elementpicturefactory.h"
#include <catch2/catch.hpp>
#include <QAction>
#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>

/*
	The layout_drawing block of an element definition: the drawing of the
	part as it sits on a mounting plate, in millimetre, beside the
	description that draws the symbol.

	Two things are proved here, and they live in two places. The reading
	needs no scene: ElementPictureFactory::layoutDrawing() turns the block
	into a picture and a rectangle, and that is what the plate will use.
	The keeping needs the element editor, because the trap is there:
	ElementScene::toXml() builds its document from nothing on every save,
	so a block the scene does not keep is lost the first time anybody
	saves the symbol - with the symbol itself perfectly fine, which is why
	nobody would notice.

	The editor is a real one. Its parts reach the scene through
	element_editor->elementScene() without a null check, so a scene built
	without an editor would test a configuration the program never runs.
*/

namespace {
	const QString shape_style = QStringLiteral(
		"antialias=\"false\" "
		"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"");

	/// A symbol of one line and one terminal, with @a layout_drawing
	/// appended to the definition as it is given. The uuid is a parameter
	/// because ElementPictureFactory caches by it: two files sharing one
	/// would be drawn from whichever was read first.
	QDomDocument symbol(const QString &layout_drawing,
						const QString &uuid = QStringLiteral("{0b3c2c8e-0c4f-4a43-9a52-1d1f5f2b7a10}"))
	{
		const QString xml = QStringLiteral(
			"<definition type=\"element\" width=\"40\" height=\"20\""
			" hotspot_x=\"20\" hotspot_y=\"10\" link_type=\"simple\">"
			"<uuid uuid=\"%3\"/>"
			"<names><name lang=\"fr\">essai</name></names>"
			"<elementInformations/>"
			"<description>"
			"<line x1=\"-10\" y1=\"0\" x2=\"10\" y2=\"0\" end1=\"none\" end2=\"none\""
			" length1=\"1.5\" length2=\"1.5\" %1/>"
			"<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"1\"/>"
			"</description>%2</definition>").arg(shape_style, layout_drawing, uuid);
		QDomDocument doc;
		REQUIRE(doc.setContent(xml));
		return doc;
	}

	/// A rectangle of 45 x 90 mm cut by a line, plus a terminal and a
	/// dynamic text, which are not shapes and must draw nothing.
	QString drawing45x90()
	{
		return QStringLiteral(
			"<layout_drawing>"
			"<rect x=\"0\" y=\"0\" width=\"45\" height=\"90\" rx=\"0\" ry=\"0\" %1/>"
			"<line x1=\"0\" y1=\"45\" x2=\"45\" y2=\"45\" end1=\"none\" end2=\"none\""
			" length1=\"1.5\" length2=\"1.5\" %1/>"
			"<terminal x=\"0\" y=\"0\" orientation=\"n\" name=\"9\"/>"
			"<dynamic_text x=\"0\" y=\"0\" z=\"1\" text_from=\"UserText\""
			" uuid=\"{11111111-2222-3333-4444-555555555555}\" frame=\"false\""
			" text_width=\"-1\"><text>X</text></dynamic_text>"
			"</layout_drawing>").arg(shape_style);
	}

	QString serialized(const QDomElement &element)
	{
		QString out;
		QTextStream stream(&out);
		element.save(stream, -1);
		return out;
	}

	QDomElement layoutBlock(const QDomDocument &doc)
	{
		return doc.documentElement().firstChildElement(
					ElementPictureFactory::layoutDrawingTagName());
	}
}

TEST_CASE("T44 — the layout drawing is read in millimetre, and only its shapes count",
	  "[t44][layoutdrawing]")
{
	const QDomDocument doc = symbol(drawing45x90());
	const auto drawing = ElementPictureFactory::layoutDrawing(doc.documentElement());

		//The rectangle and the line. The terminal and the dynamic text are
		//in the block too, and draw nothing: that is the count proving it.
	CHECK(drawing.parts == 2);
	CHECK(drawing.bounds == QRectF(0, 0, 45, 90));
	CHECK_FALSE(drawing.isNull());
	CHECK_FALSE(drawing.picture.isNull());

	SECTION("no block is no drawing")
	{
		const auto none = ElementPictureFactory::layoutDrawing(
					symbol(QString()).documentElement());
		CHECK(none.isNull());
		CHECK(none.bounds.isNull());
	}

	SECTION("a block holding nothing but what is not a shape is no drawing")
	{
		const auto none = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing>"
						   "<terminal x=\"0\" y=\"0\" orientation=\"n\" name=\"9\"/>"
						   "</layout_drawing>")).documentElement());
		CHECK(none.isNull());
	}

	SECTION("a shape its parser refuses is not counted")
	{
			//A rectangle with no height: parseRect() returns before
			//drawing, and a count by tag would call this a drawing.
		const auto none = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing><rect x=\"0\" y=\"0\" width=\"45\" %1/>"
						   "</layout_drawing>").arg(shape_style)).documentElement());
		CHECK(none.parts == 0);
		CHECK(none.isNull());
	}

	SECTION("the fraction of a millimetre is kept")
	{
			//22.5 mm is the width of a common modular device, and a whole
			//unit off is the size of the gap the plate warns about.
		const auto narrow = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing><rect x=\"0\" y=\"0\" width=\"22.5\""
						   " height=\"90\" rx=\"0\" ry=\"0\" %1/></layout_drawing>")
						   .arg(shape_style)).documentElement());
		CHECK(narrow.bounds.width() == Approx(22.5));
		CHECK(narrow.bounds.height() == Approx(90));
	}

	SECTION("a half arc is bounded by what it draws, not by its whole ellipse")
	{
			//From 0 to 180 degrees, counterclockwise from three o'clock:
			//the top half of a circle 40 mm across.
		const auto arc = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing><arc x=\"0\" y=\"0\" width=\"40\" height=\"40\""
						   " start=\"0\" angle=\"180\" %1/></layout_drawing>")
						   .arg(shape_style)).documentElement());
		REQUIRE(arc.parts == 1);
		CHECK(arc.bounds.width() == Approx(40));
		CHECK(arc.bounds.height() == Approx(20));
		CHECK(arc.bounds.top() == Approx(0).margin(1e-9));
	}
}

TEST_CASE("T44 — the element editor writes back the layout drawing it read",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene);

	const QDomDocument read = symbol(drawing45x90());
	scene->fromXml(read);

	const QDomDocument saved = scene->toXml();
	const QDomElement block = layoutBlock(saved);
	REQUIRE_FALSE(block.isNull());

		//Byte for byte what was read - terminal and dynamic text included:
		//this scene keeps the block, it does not judge it. Filtering is
		//the reader's business, and the reader of the saved file still
		//finds the same drawing.
	CHECK(serialized(block) == serialized(layoutBlock(read)));
	const auto drawing = ElementPictureFactory::layoutDrawing(saved.documentElement());
	CHECK(drawing.parts == 2);
	CHECK(drawing.bounds == QRectF(0, 0, 45, 90));

		//And the symbol is still the symbol: the description was not
		//touched by the block written beside it.
	const QDomElement description = saved.documentElement().firstChildElement("description");
	CHECK(description.elementsByTagName("line").size() == 1);
	CHECK(description.elementsByTagName("terminal").size() == 1);

	SECTION("a selection export does not carry the drawing along")
	{
		scene->slot_selectAll();
		CHECK(layoutBlock(scene->toXml(false)).isNull());
	}

	SECTION("reset forgets it, so the next element does not inherit it")
	{
		scene->reset();
		CHECK(scene->layoutDrawingXml().isNull());
		CHECK(layoutBlock(scene->toXml()).isNull());
	}
}

TEST_CASE("T44 — a symbol without a layout drawing is saved without one",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	scene->fromXml(symbol(QString()));

		//Not even an empty block: an old symbol saved by this version must
		//come out the way it went in.
	CHECK(scene->layoutDrawingXml().isNull());
	CHECK(layoutBlock(scene->toXml()).isNull());
}

/*
	From here on the editor is driven the way the person driving it does:
	opened from a file, the tab switched, a shape drawn with the real tool
	of the tool bar, saved through toFile(), read back. A file and not a
	location: a file outside every collection is no element to
	ElementsLocation::isElement(), and fromLocation() would stop on a
	modal box nobody can close here. The mouse goes to
	the scene as scene events, which is where the tools read it: going
	through the view would only add the geometry of a window nobody sees.
*/
namespace {
	QString writeSymbol(const QTemporaryDir &dir, const QString &name, const QDomDocument &doc)
	{
		const QString path = dir.filePath(name);
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write(doc.toByteArray());
		file.close();
		return path;
	}

	QDomDocument readFile(const QString &path)
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		QDomDocument doc;
		REQUIRE(doc.setContent(file.readAll()));
		return doc;
	}

	QAction *editorAction(QETElementEditor &editor, const QString &text)
	{
		for (QAction *action : editor.findChildren<QAction *>()) {
			if (action->text() == text) {
				return action;
			}
		}
		FAIL("no action named " << text.toStdString());
		return nullptr;
	}

	void sendMouse(ElementScene *scene, QEvent::Type type, const QPointF &pos)
	{
		QGraphicsSceneMouseEvent event(type);
		event.setScenePos(pos);
		event.setButton(type == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMousePress ? Qt::LeftButton : Qt::NoButton);
		QApplication::sendEvent(scene, &event);
	}

	void clickAt(ElementScene *scene, const QPointF &pos)
	{
		sendMouse(scene, QEvent::GraphicsSceneMousePress, pos);
		sendMouse(scene, QEvent::GraphicsSceneMouseRelease, pos);
	}

	/// What a person does with a two-click tool of the bar: pick the
	/// tool, click the first corner, move, click the second.
	void drawWith(QETElementEditor &editor, const QString &tool, const QPointF &from, const QPointF &to)
	{
		editorAction(editor, tool)->trigger();
		ElementScene *scene = editor.elementScene();
		clickAt(scene, from);
		sendMouse(scene, QEvent::GraphicsSceneMouseMove, to);
		clickAt(scene, to);
	}

	template <typename Part>
	QList<Part *> partsOf(ElementScene *scene)
	{
		QList<Part *> found;
		for (QGraphicsItem *item : scene->items()) {
			if (auto part = dynamic_cast<Part *>(item)) {
				found << part;
			}
		}
		return found;
	}

	const QString add_rectangle = QStringLiteral("Ajouter un rectangle");
	const QString add_line      = QStringLiteral("Ajouter une ligne");
}

TEST_CASE("CU-44.1 — the 2D drawing survives save and reopen",
	  "[t44][layoutdrawing]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = writeSymbol(dir, QStringLiteral("piece.elmt"), symbol(QString()));

	{
		QETElementEditor editor;
		editor.fromFile(path);
		editor.setLayoutDrawingActive(true);
		REQUIRE(editor.elementScene() == editor.layoutDrawingScene());

		drawWith(editor, add_rectangle, QPointF(0, 0), QPointF(45, 90));
		drawWith(editor, add_line,      QPointF(0, 45), QPointF(45, 45));
		REQUIRE(partsOf<PartRectangle>(editor.layoutDrawingScene()).size() == 1);
		REQUIRE(partsOf<PartLine>(editor.layoutDrawingScene()).size() == 1);
		REQUIRE(editor.toFile(path));
	}

		//The file, as any reader finds it
	const QDomDocument saved = readFile(path);
	const QDomElement block = layoutBlock(saved);
	REQUIRE_FALSE(block.isNull());
	CHECK(block.elementsByTagName("rect").size() == 1);
	CHECK(block.elementsByTagName("line").size() == 1);
	const auto drawing = ElementPictureFactory::layoutDrawing(saved.documentElement());
	CHECK(drawing.parts == 2);
	CHECK(drawing.bounds == QRectF(0, 0, 45, 90));

		//And the editor, opened again
	QETElementEditor reopened;
	reopened.fromFile(path);
	CHECK_FALSE(reopened.isLayoutDrawingActive());
	const auto rects = partsOf<PartRectangle>(reopened.layoutDrawingScene());
	REQUIRE(rects.size() == 1);
	CHECK(rects.first()->sceneGeometricRect() == QRectF(0, 0, 45, 90));
	CHECK(partsOf<PartLine>(reopened.layoutDrawingScene()).size() == 1);

		//The symbol tab is as it was: the line and the terminal, nothing of
		//the 2D drawing in it
	CHECK(partsOf<PartLine>(reopened.elementScene()).size() == 1);
	CHECK(partsOf<PartTerminal>(reopened.elementScene()).size() == 1);
	CHECK(partsOf<PartRectangle>(reopened.elementScene()).isEmpty());
}

TEST_CASE("CU-44.2 — a symbol without a 2D drawing comes out as it went in",
	  "[t44][layoutdrawing]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = writeSymbol(dir, QStringLiteral("ancien.elmt"), symbol(QString()));

	QETElementEditor editor;
	editor.fromFile(path);
	drawWith(editor, add_line, QPointF(-10, 10), QPointF(10, 10));
	REQUIRE(partsOf<PartLine>(editor.elementScene()).size() == 2);
	REQUIRE(editor.toFile(path));

		//Not even an empty block
	CHECK(layoutBlock(readFile(path)).isNull());
}

TEST_CASE("CU-44.3 — editing only the symbol does not erase the 2D drawing",
	  "[t44][layoutdrawing]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = writeSymbol(dir, QStringLiteral("piece.elmt"), symbol(drawing45x90()));

	QETElementEditor editor;
	editor.fromFile(path);
		//The 2D tab is never shown
	REQUIRE_FALSE(editor.isLayoutDrawingActive());
	drawWith(editor, add_line, QPointF(-10, 10), QPointF(10, 10));
	REQUIRE(editor.toFile(path));

	const QDomDocument saved = readFile(path);
	const QDomElement block = layoutBlock(saved);
	REQUIRE_FALSE(block.isNull());
		//The shapes are all there. What the block held besides shapes - a
		//terminal and a dynamic text, which this fixture puts in on purpose
		//- the 2D tab never took, and so never writes back.
	const auto drawing = ElementPictureFactory::layoutDrawing(saved.documentElement());
	CHECK(drawing.parts == 2);
	CHECK(drawing.bounds == QRectF(0, 0, 45, 90));
	CHECK(block.elementsByTagName("terminal").isEmpty());
	CHECK(block.elementsByTagName("dynamic_text").isEmpty());
}

TEST_CASE("CU-44.4 — the 2D drawing does not show on the folio",
	  "[t44][layoutdrawing]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

		//The same symbol saved by the editor twice, with and without a 2D
		//drawing: what the folio is built from must not tell them apart.
	auto saveThrough = [&dir](const QString &name, const QString &block, const QString &uuid) {
		const QString path = writeSymbol(dir, name, symbol(block, uuid));
		QETElementEditor editor;
		editor.fromFile(path);
		return editor.elementXml();
	};
	const QDomDocument with = saveThrough(QStringLiteral("avec.elmt"), drawing45x90(),
								QStringLiteral("{9a1d2c3e-0000-4000-8000-000000000001}"));
	const QDomDocument without = saveThrough(QStringLiteral("sans.elmt"), QString(),
								QStringLiteral("{9a1d2c3e-0000-4000-8000-000000000002}"));
	REQUIRE_FALSE(layoutBlock(with).isNull());

		//The size and hotspot the folio places it by
	for (const char *attribute : {"width", "height", "hotspot_x", "hotspot_y"}) {
		INFO(attribute);
		CHECK(with.documentElement().attribute(attribute)
			  == without.documentElement().attribute(attribute));
	}

		//The terminals an element on a folio gets: those of the description
	CHECK(with.documentElement().firstChildElement("description")
			.elementsByTagName("terminal").size() == 1);

		//And what the folio draws, by the factory that draws it: one line,
		//the symbol's. The rectangle and the second line of the block would
		//show here if the folio read it.
	const QString path = dir.filePath(QStringLiteral("avec-enregistre.elmt"));
	QFile file(path);
	REQUIRE(file.open(QIODevice::WriteOnly));
	file.write(with.toByteArray());
	file.close();
	const auto primitives = ElementPictureFactory::instance()->getPrimitives(ElementsLocation(path));
	CHECK(primitives.m_lines.size() == 1);
	CHECK(primitives.m_rectangles.isEmpty());
}

TEST_CASE("CU-44.5 — the 2D tab has no terminal and no dynamic text",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	editor.elementScene()->fromXml(symbol(QString()));
	QAction *terminal = editorAction(editor, QStringLiteral("Ajouter une borne"));
	QAction *dynamic  = editorAction(editor, QStringLiteral("Ajouter un champ texte dynamique"));
	REQUIRE(terminal->isEnabled());
	REQUIRE(dynamic->isEnabled());

	editor.setLayoutDrawingActive(true);
	CHECK_FALSE(terminal->isEnabled());
	CHECK_FALSE(dynamic->isEnabled());
		//The shapes are all there
	CHECK(editorAction(editor, add_rectangle)->isEnabled());
	CHECK(editorAction(editor, add_line)->isEnabled());

	editor.setLayoutDrawingActive(false);
	CHECK(terminal->isEnabled());
	CHECK(dynamic->isEnabled());
}

TEST_CASE("CU-44.6 — undo is per tab",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	editor.elementScene()->fromXml(symbol(QString()));
	QAction *undo = editorAction(editor, QStringLiteral("Annuler"));
		//Kept before any switch: elementScene() answers for the tab shown
	ElementScene *symbol_scene = editor.elementScene();

	editor.setLayoutDrawingActive(true);
	drawWith(editor, add_line, QPointF(0, 0), QPointF(45, 0));
	REQUIRE(partsOf<PartLine>(editor.layoutDrawingScene()).size() == 1);

	editor.setLayoutDrawingActive(false);
	drawWith(editor, add_line, QPointF(-10, 10), QPointF(10, 10));
	REQUIRE(partsOf<PartLine>(symbol_scene).size() == 2);

	REQUIRE(undo->isEnabled());
	undo->trigger();
	CHECK(partsOf<PartLine>(symbol_scene).size() == 1);
	CHECK(partsOf<PartLine>(editor.layoutDrawingScene()).size() == 1);

		//And back on the 2D tab, its own undo
	editor.setLayoutDrawingActive(true);
	REQUIRE(undo->isEnabled());
	undo->trigger();
	CHECK(partsOf<PartLine>(editor.layoutDrawingScene()).isEmpty());
	CHECK(partsOf<PartLine>(symbol_scene).size() == 1);
}

TEST_CASE("T44 — the 2D tab refuses what is not a shape, pasted or loaded",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	editor.setLayoutDrawingActive(true);

		//The paste path: fromXml at a position, consider_informations off.
		//A whole symbol pasted on the plate drawing leaves its line, and
		//not its terminal.
	editor.elementScene()->fromXml(symbol(QString()), QPointF(5, 5), false);
	CHECK(partsOf<PartLine>(editor.layoutDrawingScene()).size() == 1);
	CHECK(partsOf<PartTerminal>(editor.layoutDrawingScene()).isEmpty());
}
