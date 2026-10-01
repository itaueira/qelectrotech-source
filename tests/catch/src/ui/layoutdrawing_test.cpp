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
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/factory/elementpicturefactory.h"
#include <catch2/catch.hpp>
#include <QDomDocument>
#include <QString>
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
	/// appended to the definition as it is given.
	QDomDocument symbol(const QString &layout_drawing)
	{
		const QString xml = QStringLiteral(
			"<definition type=\"element\" width=\"40\" height=\"20\""
			" hotspot_x=\"20\" hotspot_y=\"10\" link_type=\"simple\">"
			"<uuid uuid=\"{0b3c2c8e-0c4f-4a43-9a52-1d1f5f2b7a10}\"/>"
			"<names><name lang=\"fr\">essai</name></names>"
			"<elementInformations/>"
			"<description>"
			"<line x1=\"-10\" y1=\"0\" x2=\"10\" y2=\"0\" end1=\"none\" end2=\"none\""
			" length1=\"1.5\" length2=\"1.5\" %1/>"
			"<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"1\"/>"
			"</description>%2</definition>").arg(shape_style, layout_drawing);
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
