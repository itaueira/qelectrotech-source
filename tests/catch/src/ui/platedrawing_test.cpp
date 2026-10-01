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
#include "../../../../sources/factory/elementpicturefactory.h"
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountinglayouteditor.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/mountinglayoutviewitem.h"
#include "../../../../sources/qetproject.h"
#include <catch2/catch.hpp>
#include <QDomDocument>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>

/*
	The mounting plate drawing the 2D drawing of each symbol.

	Two levels, and both are needed. The part on its own decides the
	geometry - where the drawing goes in the box of the catalogue, when its
	proportion is marked, what happens without a box - and that is read
	off the item directly, in millimetre, with no window. The project end
	is the path the drawing takes to get there: from the definition the
	project embeds, through the element of the folio the part is, to the
	part on the plate window and on the view of the plate on a folio - and
	through a save, because the drawing travels inside the project.
*/

namespace
{
	const QString shape_style = QStringLiteral(
		"antialias=\"false\" "
		"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"");

	/// The 2D drawing of a part @a width by @a height mm: its outline only,
	/// so that the inside of it is empty paper.
	QString outlineDrawing(qreal width, qreal height)
	{
		return QStringLiteral(
			"<layout_drawing>"
			"<rect x=\"0\" y=\"0\" width=\"%2\" height=\"%3\" rx=\"0\" ry=\"0\" %1/>"
			"</layout_drawing>").arg(shape_style).arg(width).arg(height);
	}

	QString drawing45x90()
	{
		return outlineDrawing(45, 90);
	}

	ElementPictureFactory::LayoutDrawing drawingOf(const QString &block)
	{
		QDomDocument doc;
		REQUIRE(doc.setContent(QStringLiteral(
			"<definition type=\"element\" width=\"20\" height=\"20\""
			" hotspot_x=\"10\" hotspot_y=\"10\">"
			"<description/>%1</definition>").arg(block)));
		return ElementPictureFactory::layoutDrawing(doc.documentElement());
	}

	MountedItem part(const QSizeF &size)
	{
		MountedItem item(QStringLiteral("-Q1"), QPointF(10, 20), size);
		item.uuid = QStringLiteral("q1");
		return item;
	}

	/// The part painted alone at 4 px a millimetre, its box filling the image
	QImage paintedAlone(MountedPartItem *item)
	{
		QGraphicsScene scene;
		scene.addItem(item);
		const QRectF body = item->mapRectToScene(item->drawnRect());
		QImage image(QSize(qRound(body.width() * 4), qRound(body.height() * 4)),
			     QImage::Format_ARGB32);
		image.fill(Qt::black);
		QPainter painter(&image);
		scene.render(&painter, QRectF(QPointF(0, 0), image.size()), body);
		painter.end();
		scene.removeItem(item);
		return image;
	}

	/// The colour of the part on its middle column, a quarter of the way
	/// down: inside the outline, and clear of the name written at its centre
	QColor middleOf(MountedPartItem *item)
	{
		const QImage image = paintedAlone(item);
		return image.pixelColor(image.width() / 2, image.height() / 4);
	}

	/// The dark pixels of @a image inside @a area: ink on paper
	int inkIn(const QImage &image, const QRect &area)
	{
		int ink = 0;
		for (int y = area.top(); y <= area.bottom(); ++y) {
			for (int x = area.left(); x <= area.right(); ++x) {
				if (qGray(image.pixel(x, y)) < 128) {
					++ink;
				}
			}
		}
		return ink;
	}

	/// Whether this platform draws letters at all when there is no screen.
	/// A build of Qt that rasterises no glyph offscreen would fail the test
	/// of the name for a reason that is not this code.
	bool glyphsAreDrawn()
	{
		QImage probe(QSize(80, 30), QImage::Format_ARGB32);
		probe.fill(Qt::white);
		QPainter painter(&probe);
		QFont font = painter.font();
		font.setPixelSize(20);
		painter.setFont(font);
		painter.setPen(Qt::black);
		painter.drawText(probe.rect(), Qt::AlignCenter, QStringLiteral("Q1"));
		painter.end();
		return inkIn(probe, probe.rect()) > 0;
	}

	/// One sheet, one embedded symbol carrying @a layout_drawing, and one
	/// element of it on the sheet, of uuid elementUuid().
	QString projectXml(const QString &layout_drawing)
	{
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">"
			"<element name=\"part.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"30\" height=\"20\" hotspot_x=\"15\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<uuid uuid=\"{7a1d0000-0000-4000-8000-000000000044}\"/>"
			"<names><name lang=\"en\">part</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" %1/>"
			"</description>%2"
			"</definition>"
			"</element>"
			"</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\""
			" cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>"
			"<element x=\"200\" y=\"200\" z=\"10\" prefix=\"\""
			" freezeLabel=\"false\" orientation=\"0\""
			" type=\"embed://bench/part.elmt\""
			" uuid=\"{c0ffee00-0000-4000-8000-000000000044}\">"
			"<terminals/><inputs/>"
			"<elementInformations>"
			"<elementInformation show=\"1\" name=\"label\">Q1</elementInformation>"
			"</elementInformations>"
			"<dynamic_texts/><texts_groups/>"
			"</element>"
			"</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>").arg(shape_style, layout_drawing);
	}

	QString elementUuid()
	{
		return QStringLiteral("{c0ffee00-0000-4000-8000-000000000044}");
	}

	MountingLayout plateWith(const MountedItem &item, QString *face)
	{
		MountingLayout layout;
		*face = layout.appendSurface(MountingSurface(QStringLiteral("QCM1"),
							     QStringLiteral("plate"),
							     MountingArea(600.0, 800.0)));
		REQUIRE_FALSE(layout.mountItem(*face, item).isEmpty());
		return layout;
	}
}

TEST_CASE("CU-44.7 — the plate draws the 2D drawing inside the box of the catalogue",
	  "[t44][calepinagem]")
{
	auto *item = new MountedPartItem(part(QSizeF(45, 90)));
	item->setLayoutDrawing(drawingOf(drawing45x90()));

	REQUIRE(item->hasLayoutDrawing());
	CHECK(item->hasDeclaredSize());
	CHECK(item->drawnRect() == QRectF(0, 0, 45, 90));
		//The whole box, and no mark: drawing and catalogue agree
	CHECK(item->layoutDrawingRect() == QRectF(0, 0, 45, 90));
	CHECK_FALSE(item->drawingSizeDiffers());
	CHECK(item->toolTip() == QStringLiteral("-Q1"));

		//And it is painted, not just computed: the middle of the outline
		//is paper, where the plain box is the grey of a part with no
		//drawing.
	const QColor with_drawing = middleOf(item);
	CHECK(with_drawing == QColor(255, 255, 255));
	delete item;

	auto *plain = new MountedPartItem(part(QSizeF(45, 90)));
	CHECK(middleOf(plain) == QColor(236, 236, 236));
	delete plain;

		//And the name is written over the drawing. Found on the screen on
		//2026-10-01: with the drawing in place of the box, three breakers
		//drawn alike were three parts nobody could tell apart. The inside
		//of the outline is paper, so ink there, away from the outline, is
		//the name.
	if (!glyphsAreDrawn()) {
		WARN("this Qt draws no glyph offscreen: the name is not checked");
		return;
	}
	auto *named = new MountedPartItem(part(QSizeF(45, 90)));
	named->setLayoutDrawing(drawingOf(drawing45x90()));
	const QImage image = paintedAlone(named);
	delete named;
	const QRect inside = image.rect().adjusted(8, 8, -8, -8);
	CHECK(inkIn(image, inside) > 0);
}

TEST_CASE("CU-44.8 — a size that disagrees with the catalogue is marked, not corrected",
	  "[t44][calepinagem]")
{
	SECTION("taller box: the drawing keeps its real size, centred on the box")
	{
		MountedPartItem item(part(QSizeF(45, 100)));
		item.setLayoutDrawing(drawingOf(drawing45x90()));
		CHECK(item.drawnRect() == QRectF(0, 0, 45, 100));
		CHECK(item.layoutDrawingRect() == QRectF(0, 5, 45, 90));
		CHECK(item.drawingSizeDiffers());
		CHECK(item.toolTip().contains(QStringLiteral("taille")));
	}

	SECTION("a drawing larger than its box is not shrunk: it overflows it, and says so")
	{
			//The rule that was replaced would have scaled it down to fit.
			//At real size it sticks out of the box on every side, and what
			//is painted has to stay inside what the item declares.
		MountedPartItem item(part(QSizeF(20, 20)));
		item.setLayoutDrawing(drawingOf(drawing45x90()));
		CHECK(item.layoutDrawingRect() == QRectF(-12.5, -35, 45, 90));
		CHECK(item.boundingRect().contains(item.layoutDrawingRect()));
		CHECK(item.drawingSizeDiffers());
			//The fit check reads the box, not the drawing
		CHECK(item.mountedItem().footprint().size() == QSizeF(20, 20));
	}

	SECTION("within the tolerance there is nothing to say")
	{
			//1 mm on 91: over 0.5 mm, and under 2 % of it - the share
			//is what lets it through
		MountedPartItem taller(part(QSizeF(45, 91)));
		taller.setLayoutDrawing(drawingOf(drawing45x90()));
		CHECK_FALSE(taller.drawingSizeDiffers());
			//0.4 mm on a part of 10: over 2 % of it, which is 0.2 mm, and
			//under 0.5 mm - the floor is what lets it through. A small part
			//drawn from a datasheet is not wrong by the width of a line.
		MountedPartItem small(part(QSizeF(10.4, 20)));
		small.setLayoutDrawing(drawingOf(outlineDrawing(10, 20)));
		CHECK_FALSE(small.drawingSizeDiffers());
		MountedPartItem small_off(part(QSizeF(10.6, 20)));
		small_off.setLayoutDrawing(drawingOf(outlineDrawing(10, 20)));
		CHECK(small_off.drawingSizeDiffers());
			//And 1 mm on 46, which is more than both: 0.5 mm and 0.92 mm
		MountedPartItem narrow(part(QSizeF(46, 90)));
		narrow.setLayoutDrawing(drawingOf(drawing45x90()));
		CHECK(narrow.drawingSizeDiffers());
	}
}

TEST_CASE("CU-44.9 — with no measure in the catalogue the drawing shows, and the part stays unmeasured",
	  "[t44][calepinagem]")
{
	MountedPartItem item(part(QSizeF()));
	item.setLayoutDrawing(drawingOf(drawing45x90()));

	CHECK_FALSE(item.hasDeclaredSize());
	CHECK(item.drawnRect() == QRectF(0, 0, 45, 90));
	CHECK(item.layoutDrawingRect() == QRectF(0, 0, 45, 90));
	CHECK_FALSE(item.drawingSizeDiffers());
		//What is on the screen never becomes what is in the file
	CHECK_FALSE(item.mountedItem().hasDeclaredSize());
	CHECK(item.mountedItem().declaredSize() == QSizeF(0, 0));
}

TEST_CASE("CU-44.10 — with no 2D drawing the part stays the box with its name",
	  "[t44][calepinagem]")
{
	MountedPartItem item(part(QSizeF(45, 90)));
	CHECK_FALSE(item.hasLayoutDrawing());
	CHECK(item.layoutDrawingRect().isNull());
	CHECK(item.drawnRect() == QRectF(0, 0, 45, 90));

	MountedPartItem unmeasured(part(QSizeF()));
	CHECK(unmeasured.drawnRect().width() == Approx(MountedPartItem::unmeasuredMarkerSide()));

		//And a drawing taken away gives the plain part back
	item.setLayoutDrawing(drawingOf(drawing45x90()));
	REQUIRE(item.hasLayoutDrawing());
	item.setLayoutDrawing(ElementPictureFactory::LayoutDrawing());
	CHECK_FALSE(item.hasLayoutDrawing());
}

TEST_CASE("CU-44.11 — the drawing comes from the project, and travels with it",
	  "[t44][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(drawing45x90()),
					QStringLiteral("plate-drawing.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagram(0)->elements().size() == 1);
	REQUIRE(scratch.diagram(0)->elements().first()->uuid().toString() == elementUuid());

	MountedItem linked = part(QSizeF(45, 100));
	linked.element = elementUuid();
	QString face;
	scratch->setMountingLayout(plateWith(linked, &face));

	auto checkPlate = [&face](QETProject *project) {
		MountingLayoutEditor editor(project);
		REQUIRE(editor.shownSurface() == face);
		MountedPartItem *on_plate = editor.scene()->partItem(QStringLiteral("q1"));
		REQUIRE(on_plate != nullptr);
		CHECK(on_plate->hasLayoutDrawing());
		CHECK(on_plate->layoutDrawingRect() == QRectF(0, 5, 45, 90));
		CHECK(on_plate->drawingSizeDiffers());
	};

	SECTION("on the plate window, read through the element of the folio")
	{
		checkPlate(scratch.project());
	}

	SECTION("on the view of the plate posed on a folio")
	{
		auto *view = new MountingLayoutViewItem();
		scratch.diagram(0)->addItem(view);
		view->setProject(scratch.project());
		view->setSurfaceUuid(face);
		MountedPartItem *on_folio = view->partItem(QStringLiteral("q1"));
		REQUIRE(on_folio != nullptr);
		CHECK(on_folio->hasLayoutDrawing());
		CHECK(on_folio->layoutDrawingRect() == QRectF(0, 5, 45, 90));
	}

	SECTION("after a save and a reopening, from the copy the project embeds")
	{
		REQUIRE(scratch.saveAndReopen());
		checkPlate(scratch.project());
	}
}

TEST_CASE("T44 — a part whose symbol has no 2D drawing, or that is no symbol, gets none",
	  "[t44][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(QString()),
					QStringLiteral("plate-nodrawing.qet"));
	REQUIRE(scratch.isOpen());

	MountedItem linked = part(QSizeF(45, 90));
	linked.element = elementUuid();
	MountedItem loose(QStringLiteral("-X9"), QPointF(100, 20), QSizeF(6, 45));
	loose.uuid = QStringLiteral("x9");
	QString face;
	MountingLayout layout = plateWith(linked, &face);
	REQUIRE_FALSE(layout.mountItem(face, loose).isEmpty());
	scratch->setMountingLayout(layout);

	const auto drawings = MountedPartItem::layoutDrawingsOf(
				scratch.project(), scratch->mountingLayout().surface(face).items);
	CHECK(drawings.isEmpty());

	MountingLayoutEditor editor(scratch.project());
	CHECK_FALSE(editor.scene()->partItem(QStringLiteral("q1"))->hasLayoutDrawing());
	CHECK_FALSE(editor.scene()->partItem(QStringLiteral("x9"))->hasLayoutDrawing());
}

TEST_CASE("T44 — the scene keeps the drawings it is given, for the parts it draws afterwards",
	  "[t44][calepinagem]")
{
		//The window hands the drawings over after every step, and that
		//covers the plate window. This is the scene on its own: given the
		//drawings first, rebuilt afterwards - by a new face, by an undo of
		//its own - it must give each part its drawing back unaided.
	MountedItem linked = part(QSizeF(45, 90));
	linked.element = elementUuid();
	MountingSurface surface(QStringLiteral("QCM1"), QStringLiteral("plate"),
				MountingArea(600.0, 800.0));
	surface.items << linked;

	MountingScene scene;
	QHash<QString, ElementPictureFactory::LayoutDrawing> drawings;
	drawings.insert(elementUuid(), drawingOf(drawing45x90()));
	scene.setLayoutDrawings(drawings);
	REQUIRE(scene.partItem(QStringLiteral("q1")) == nullptr);

	scene.setSurface(surface);
	MountedPartItem *drawn = scene.partItem(QStringLiteral("q1"));
	REQUIRE(drawn != nullptr);
	CHECK(drawn->hasLayoutDrawing());
	CHECK(drawn->layoutDrawingRect() == QRectF(0, 0, 45, 90));
}
