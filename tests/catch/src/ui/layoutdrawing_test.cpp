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
#include "../../../../sources/editor/editorcommands.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/partimage.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partrectangle.h"
#include "../../../../sources/editor/graphicspart/partterminal.h"
#include "../../../../sources/editor/ui/rectangleeditor.h"
#include "../../../../sources/editor/ui/imageeditor.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/factory/elementpicturefactory.h"
#include <catch2/catch.hpp>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QDialog>
#include <QDomDocument>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

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

TEST_CASE("CU-44.15 — the fields of the 2D tab say mm",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	editor.elementScene()->fromXml(symbol(QString()));
	ElementScene *symbol_scene = editor.elementScene();

		//The editor of a part is shown in the tools dock only once a part
		//of its kind is selected, and it is the same editor on both tabs
	auto fieldsShownFor = [&editor](ElementScene *scene) {
		QList<PartRectangle *> rectangles = partsOf<PartRectangle>(scene);
		REQUIRE(rectangles.size() == 1);
		scene->clearSelection();
		rectangles.first()->setSelected(true);
		QList<QDoubleSpinBox *> fields;
		for (RectangleEditor *shown : editor.findChildren<RectangleEditor *>()) {
			fields << shown->findChildren<QDoubleSpinBox *>();
		}
		return fields;
	};

	editor.setLayoutDrawingActive(true);
	drawWith(editor, add_rectangle, QPointF(0, 0), QPointF(45, 90));
	const QList<QDoubleSpinBox *> in_2d = fieldsShownFor(editor.layoutDrawingScene());
		//x, y, width, height and the two radii
	REQUIRE(in_2d.size() == 6);
	for (QDoubleSpinBox *field : in_2d) {
		CHECK(field->suffix() == QStringLiteral(" mm"));
	}

		//Back on the symbol the same fields say nothing: its unit is the
		//folio's, and a millimetre there would be a lie
	editor.setLayoutDrawingActive(false);
	drawWith(editor, add_rectangle, QPointF(-5, -5), QPointF(5, 5));
	const QList<QDoubleSpinBox *> on_symbol = fieldsShownFor(symbol_scene);
	REQUIRE(on_symbol.size() == 6);
	for (QDoubleSpinBox *field : on_symbol) {
		CHECK(field->suffix().isEmpty());
	}
}

/*
	The image of the 2D tab: the photo or the datasheet drawing of the part,
	inside the symbol, at the size the person says the part has.
*/

namespace {
	/// A raster file of @a size pixels, all of @a colour, written at
	/// @a per_metre dots a metre so that its size in millimetre is known
	QByteArray rasterFile(const char *format, const QSize &size, const QColor &colour,
			      int per_metre = 10000)
	{
		QImage image(size, QImage::Format_RGB32);
		image.fill(colour);
		image.setDotsPerMeterX(per_metre);
		image.setDotsPerMeterY(per_metre);
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, format);
		return bytes;
	}

	/// An SVG drawn in millimetre: a red box of 45 by 90
	QByteArray svgFile()
	{
		return QByteArrayLiteral(
			"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"45mm\" height=\"90mm\""
			" viewBox=\"0 0 45 90\">"
			"<rect x=\"0\" y=\"0\" width=\"45\" height=\"90\" fill=\"#ff0000\"/></svg>");
	}

	QString writeBytes(const QTemporaryDir &dir, const QString &name, const QByteArray &bytes)
	{
		const QString path = dir.filePath(name);
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write(bytes);
		return path;
	}

	QDomElement imageElement(QDomDocument &doc, const QByteArray &bytes, const QString &format,
				 const QRectF &rect)
	{
		QDomElement image = doc.createElement(QStringLiteral("image"));
		image.setAttribute(QStringLiteral("x"), rect.x());
		image.setAttribute(QStringLiteral("y"), rect.y());
		image.setAttribute(QStringLiteral("width"), rect.width());
		image.setAttribute(QStringLiteral("height"), rect.height());
		image.setAttribute(QStringLiteral("format"), format);
		image.appendChild(doc.createTextNode(QString::fromLatin1(bytes.toBase64())));
		return image;
	}

	const QString insert_image = QStringLiteral("Insérer une image");
}

TEST_CASE("T44 — an image file says its size, from its resolution or its units",
	  "[t44][layoutdrawing]")
{
		//400 by 800 pixels at 10 000 a metre are 40 by 80 mm
	const QSizeF png = PartImage::fileSizeMm(rasterFile("PNG", QSize(400, 800), Qt::red), QStringLiteral("png"));
	CHECK(png.width()  == Approx(40.0));
	CHECK(png.height() == Approx(80.0));
	const QSizeF jpg = PartImage::fileSizeMm(rasterFile("JPG", QSize(400, 800), Qt::red), QStringLiteral("jpg"));
	CHECK(jpg.width()  == Approx(40.0).margin(0.1));
	CHECK(jpg.height() == Approx(80.0).margin(0.1));
		//And an SVG written in millimetre comes back at its size
	const QSizeF svg = PartImage::fileSizeMm(svgFile(), QStringLiteral("svg"));
	CHECK(svg.width()  == Approx(45.0).margin(0.05));
	CHECK(svg.height() == Approx(90.0).margin(0.05));

		//Bytes that are no such file, or a format that is not kept, are nothing
	CHECK(PartImage::fileSizeMm(QByteArrayLiteral("not an image"), QStringLiteral("png")).isEmpty());
	CHECK(PartImage::fileSizeMm(rasterFile("PNG", QSize(4, 4), Qt::red), QStringLiteral("bmp")).isEmpty());

	CHECK(PartImage::formatOfFile(QStringLiteral("/a/datasheet.PNG")) == QStringLiteral("png"));
	CHECK(PartImage::formatOfFile(QStringLiteral("photo.jpeg")) == QStringLiteral("jpg"));
	CHECK(PartImage::formatOfFile(QStringLiteral("drawing.svg")) == QStringLiteral("svg"));
	CHECK(PartImage::formatOfFile(QStringLiteral("scan.bmp")).isEmpty());
}

TEST_CASE("CU-44.12 — inserting an image asks the real size of the part",
	  "[t44][layoutdrawing]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString png = writeBytes(dir, QStringLiteral("datasheet.png"),
				       rasterFile("PNG", QSize(400, 800), Qt::red));

	QETElementEditor editor;
	editor.elementScene()->fromXml(symbol(QString()));
	QAction *insert = editorAction(editor, insert_image);
		//On the symbol there is nothing to insert an image into
	CHECK_FALSE(insert->isEnabled());
	CHECK_FALSE(editor.insertImageFile(png));
	CHECK(partsOf<PartImage>(editor.elementScene()).isEmpty());

	editor.setLayoutDrawingActive(true);
	CHECK(insert->isEnabled());

		//The question answered the way a person answers it: the width typed
		//in, the height following it in proportion, then the proportion let
		//go and the height typed in apart
	double asked_width = 0.0, asked_height = 0.0, followed_height = 0.0;
	bool answered = false;
	QTimer answer;
	answer.setInterval(20);
	int tries = 200;
	QObject::connect(&answer, &QTimer::timeout, [&]() {
		auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
		if (!dialog || dialog->objectName() != QStringLiteral("image_size_dialog")) {
			if (--tries <= 0) {
				answer.stop();
			}
			return;
		}
		answer.stop();
		auto *width  = dialog->findChild<QDoubleSpinBox *>(QStringLiteral("image_width"));
		auto *height = dialog->findChild<QDoubleSpinBox *>(QStringLiteral("image_height"));
		auto *keep   = dialog->findChild<QCheckBox *>(QStringLiteral("image_keep_proportion"));
		if (!width || !height || !keep) {
			dialog->reject();
			return;
		}
		asked_width  = width->value();
		asked_height = height->value();
		width->setValue(22.5);
		followed_height = height->value();
		keep->setChecked(false);
		height->setValue(90.0);
		answered = true;
		dialog->accept();
	});
	answer.start();
	REQUIRE(editor.insertImageFile(png));
	REQUIRE(answered);
		//It started at the size the file says, in its proportion
	CHECK(asked_width  == Approx(40.0));
	CHECK(asked_height == Approx(80.0));
	CHECK(followed_height == Approx(45.0));

	const auto images = partsOf<PartImage>(editor.layoutDrawingScene());
	REQUIRE(images.size() == 1);
	CHECK(images.first()->sceneGeometricRect() == QRectF(-11.25, -45, 22.5, 90));
	CHECK(images.first()->isSelected());
	CHECK_FALSE(images.first()->isNull());
		//Grabbed anywhere on it, where a rectangle is grabbed by its outline
	CHECK(images.first()->shape().contains(images.first()->rect().center()));

		//One step of undo takes it out again
	editor.layoutDrawingScene()->undoStack().undo();
	CHECK(partsOf<PartImage>(editor.layoutDrawingScene()).isEmpty());

		//And back on the symbol the action is off again: off at first is
		//only how it is made, off after the 2D tab is the rule
	editor.setLayoutDrawingActive(false);
	CHECK_FALSE(insert->isEnabled());
}

TEST_CASE("CU-44.13 — the image is scaled freely, and travels inside the symbol",
	  "[t44][layoutdrawing]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = writeSymbol(dir, QStringLiteral("piece.elmt"),
					 symbol(QString(), QStringLiteral("{0b3c2c8e-0c4f-4a43-9a52-1d1f5f2b7a13}")));
	const QByteArray png = rasterFile("PNG", QSize(40, 80), Qt::red);

	{
		QETElementEditor editor;
		editor.fromFile(path);
		editor.setLayoutDrawingActive(true);
		PartImage *image = editor.addLayoutImage(png, QStringLiteral("png"), QSizeF(22.5, 90));
		REQUIRE(image);

			//The fields of the part, shown because the image is selected
		ImageEditor *fields = nullptr;
		for (ImageEditor *shown : editor.findChildren<ImageEditor *>()) {
			fields = shown;
		}
		REQUIRE(fields);
		auto *width  = fields->findChild<QDoubleSpinBox *>(QStringLiteral("m_width_sb"));
		auto *height = fields->findChild<QDoubleSpinBox *>(QStringLiteral("m_height_sb"));
		REQUIRE(width);
		REQUIRE(height);
		CHECK(width->suffix() == QStringLiteral(" mm"));

			//25 by 85: not the proportion of the file, and kept as asked
		width->setValue(25.0);
		emit width->editingFinished();
		height->setValue(85.0);
		emit height->editingFinished();
		CHECK(image->rect().size() == QSizeF(25, 85));
		REQUIRE(editor.toFile(path));
	}

		//The file holds the image itself, and the description the folio
		//draws holds nothing of it
	const QDomDocument saved = readFile(path);
	const QDomElement written = layoutBlock(saved).firstChildElement(QStringLiteral("image"));
	REQUIRE_FALSE(written.isNull());
	CHECK(written.attribute(QStringLiteral("format")) == QStringLiteral("png"));
	CHECK(written.attribute(QStringLiteral("width"))  == QStringLiteral("25"));
	CHECK(written.attribute(QStringLiteral("height")) == QStringLiteral("85"));
	CHECK(QByteArray::fromBase64(written.text().toLatin1()) == png);
	CHECK(saved.documentElement().firstChildElement(QStringLiteral("description"))
	      .elementsByTagName(QStringLiteral("image")).isEmpty());

		//Opened again with no file of it anywhere on the disk
	QETElementEditor reopened;
	reopened.fromFile(path);
	const auto images = partsOf<PartImage>(reopened.layoutDrawingScene());
	REQUIRE(images.size() == 1);
	CHECK(images.first()->rect().size() == QSizeF(25, 85));
	CHECK_FALSE(images.first()->isNull());
	CHECK(images.first()->fileData() == png);
	CHECK(partsOf<PartImage>(reopened.elementScene()).isEmpty());
}

TEST_CASE("T44 — an image pasted on the symbol is refused, and kept on the 2D tab",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	ElementScene *symbol_scene = editor.elementScene();
	symbol_scene->fromXml(symbol(QString()));

		//What the clipboard holds after copying an image of the 2D tab
	QDomDocument pasted = symbol(QString());
	pasted.documentElement().firstChildElement(QStringLiteral("description")).appendChild(
				imageElement(pasted, rasterFile("PNG", QSize(10, 10), Qt::red),
					     QStringLiteral("png"), QRectF(0, 0, 10, 10)));

	symbol_scene->fromXml(pasted, QPointF(5, 5), false);
	CHECK(partsOf<PartImage>(symbol_scene).isEmpty());

	editor.setLayoutDrawingActive(true);
	editor.elementScene()->fromXml(pasted, QPointF(5, 5), false);
	CHECK(partsOf<PartImage>(editor.layoutDrawingScene()).size() == 1);
}

TEST_CASE("T44 — an image this program cannot draw is kept as it came",
	  "[t44][layoutdrawing]")
{
		//A format of a later version, or bytes damaged on the way: drawn as
		//a crossed box, and written back untouched
	QDomDocument doc;
	QDomElement element = doc.createElement(QStringLiteral("image"));
	element.setAttribute(QStringLiteral("x"), 0);
	element.setAttribute(QStringLiteral("y"), 0);
	element.setAttribute(QStringLiteral("width"), 20);
	element.setAttribute(QStringLiteral("height"), 30);
	element.setAttribute(QStringLiteral("format"), QStringLiteral("webp"));
	element.appendChild(doc.createTextNode(QString::fromLatin1(QByteArrayLiteral("RIFF....WEBP").toBase64())));

	QETElementEditor editor;
	PartImage image(&editor);
	image.fromXml(element);
	CHECK(image.isNull());
	CHECK_FALSE(image.isUseless());
	const QDomElement back = image.toXml(doc);
	CHECK(back.attribute(QStringLiteral("format")) == QStringLiteral("webp"));
	CHECK(back.text() == element.text());

		//And the plate counts nothing it could not draw
	QDomDocument definition;
	REQUIRE(definition.setContent(QStringLiteral(
		"<definition type=\"element\" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\">"
		"<description/><layout_drawing/></definition>")));
	definition.documentElement().firstChildElement(QStringLiteral("layout_drawing"))
			.appendChild(definition.importNode(element, true));
	const auto drawing = ElementPictureFactory::layoutDrawing(definition.documentElement());
	CHECK(drawing.parts == 0);
}

TEST_CASE("T44 — rotating, flipping or mirroring leaves an image as it is drawn",
	  "[t44][layoutdrawing]")
{
		//Found on the screen on 2026-10-01: Space turned the image by its
		//transform, sideways, with the fields still saying the old width
		//and height, and nothing of the turn in the file
	QETElementEditor editor;
	editor.setLayoutDrawingActive(true);
	PartImage *image = editor.addLayoutImage(rasterFile("PNG", QSize(40, 80), Qt::red),
						 QStringLiteral("png"), QSizeF(45, 88));
	REQUIRE(image);
	const QRectF drawn = image->sceneGeometricRect();
	ElementScene *scene = editor.layoutDrawingScene();
	REQUIRE(scene->selectedItems() == QList<QGraphicsItem *>{image});

	scene->undoStack().push(new RotateElementsCommand(scene));
	CHECK(image->QGraphicsItem::rotation() == 0.0);
	CHECK(image->sceneGeometricRect() == drawn);
	scene->undoStack().undo();
	CHECK(image->QGraphicsItem::rotation() == 0.0);
	CHECK(image->sceneGeometricRect() == drawn);

	scene->undoStack().push(new FlipElementsCommand(scene));
	scene->undoStack().push(new MirrorElementsCommand(scene));
	CHECK(image->sceneGeometricRect() == drawn);
}
