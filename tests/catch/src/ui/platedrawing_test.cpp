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
#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/catalog/catalogpart.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/factory/elementpicturefactory.h"
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountinglayouteditor.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/mountinglayoutviewitem.h"
#include "../../../../sources/qetapp.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetresult.h"
#include <catch2/catch.hpp>
#include <QBuffer>
#include <QAction>
#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QSettings>
#include <QTemporaryDir>
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
		const auto drawing = ElementPictureFactory::layoutDrawingOf(block);
		REQUIRE_FALSE(drawing.isNull());
		return drawing;
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

	/// Whether @a colour is paper or ink seen through the orange veil of a
	/// drawing that does not fit its box: far more red than blue, which
	/// white, black and the greys of the plate never are
	bool isVeiled(const QColor &colour)
	{
		return colour.red() - colour.blue() > 60;
	}

	/// The pixels of @a image inside @a area that are under the veil
	int veiledIn(const QImage &image, const QRect &area)
	{
		int veiled = 0;
		for (int y = area.top(); y <= area.bottom(); ++y) {
			for (int x = area.left(); x <= area.right(); ++x) {
				if (isVeiled(image.pixelColor(x, y))) {
					++veiled;
				}
			}
		}
		return veiled;
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

	SECTION("the warning is a veil over the whole drawing, and only when asked")
	{
			//Decided on 2026-10-01: a mark on a corner shrank with the part,
			//and on a small one nobody saw it. The veil is as large as the
			//drawing, whatever its size.
		auto *item = new MountedPartItem(part(QSizeF(45, 100)));
		item->setLayoutDrawing(drawingOf(drawing45x90()));
		REQUIRE(item->drawingSizeDiffers());
			//Off by default: whoever does not ask, the folio, never shows it
		CHECK_FALSE(item->sizeWarningShown());
		const QImage plain = paintedAlone(item);
		CHECK(veiledIn(plain, plain.rect()) == 0);

		item->setSizeWarningShown(true);
		const QImage veiled = paintedAlone(item);
			//4 px a millimetre, the drawing at (0, 5) and 45 by 90: its four
			//corners, two millimetres in, are all under the veil
		CHECK(isVeiled(veiled.pixelColor(8, 28)));
		CHECK(isVeiled(veiled.pixelColor(172, 28)));
		CHECK(isVeiled(veiled.pixelColor(8, 372)));
		CHECK(isVeiled(veiled.pixelColor(172, 372)));
			//and the strip of box above the drawing is not: it is the
			//drawing that is in question, not the room around it
		CHECK_FALSE(isVeiled(veiled.pixelColor(90, 8)));
		delete item;
	}

	SECTION("a drawing that fits its box is never veiled, even when asked")
	{
		auto *item = new MountedPartItem(part(QSizeF(45, 90)));
		item->setLayoutDrawing(drawingOf(drawing45x90()));
		item->setSizeWarningShown(true);
		const QImage image = paintedAlone(item);
		CHECK(veiledIn(image, image.rect()) == 0);
		delete item;
	}
}

TEST_CASE("T44 — the warning of size is on the plate window, and never on the folio",
	  "[t44][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(QString()), QStringLiteral("plate-warning.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

		//A box of 45 by 100 for a drawing of 45 by 90: one that warns
	MountedItem item = part(QSizeF(45, 100));
	item.part_code = QStringLiteral("PART-WARNED");
	QString face;
	MountingLayout layout = plateWith(item, &face);
	layout.setPartDrawing(item.part_code, drawing45x90());
	scratch->setMountingLayout(layout);

		//On the plate window, where the part is placed, the warning is shown
	MountingLayoutEditor editor(scratch.project());
	MountedPartItem *on_plate = editor.scene()->partItem(QStringLiteral("q1"));
	REQUIRE(on_plate != nullptr);
	REQUIRE(on_plate->drawingSizeDiffers());
	CHECK(on_plate->sizeWarningShown());

		//On the folio it is not, and the folio is what every print, PDF
		//and image is drawn from: the plate painted there has no veil
	auto *view = new MountingLayoutViewItem();
	scratch.diagram(0)->addItem(view);
	view->setProject(scratch.project());
	view->setSurfaceUuid(face);
	MountedPartItem *on_folio = view->partItem(QStringLiteral("q1"));
	REQUIRE(on_folio != nullptr);
	REQUIRE(on_folio->drawingSizeDiffers());
	CHECK_FALSE(on_folio->sizeWarningShown());

	auto folioImage = [&]() {
		const QRectF area = on_folio->sceneBoundingRect();
		QImage image(QSize(qMax(1, qRound(area.width() * 4)), qMax(1, qRound(area.height() * 4))),
			     QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		scratch.diagram(0)->render(&painter, QRectF(QPointF(0, 0), image.size()), area);
		painter.end();
		return image;
	};
	const QImage folio = folioImage();
	CHECK(veiledIn(folio, folio.rect()) == 0);
		//And the zero means something: the same picture of the folio sees
		//the veil the moment the part is asked to show it
	on_folio->setSizeWarningShown(true);
	const QImage control = folioImage();
	CHECK(veiledIn(control, control.rect()) > 0);
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

/*
	The plate at the scale of a project: the drawing of each part comes
	from the copy the project keeps by part code, renewed from the
	catalogue, and not from the symbol of the folio, which serves a whole
	family of parts and carries no drawing.
*/
namespace {
	const QString part_code = QStringLiteral("LC1D09");

	MountedItem coded(const QSizeF &size, const QString &code = part_code)
	{
		MountedItem item = part(size);
		item.part_code = code;
		return item;
	}

	/// The catalogue of the program, pointed at a file of its own for the
	/// time of a test and given back afterwards: opening a project renews
	/// its copies from that catalogue, and a test of it must not write in
	/// anybody's
	class ProgramCatalogue
	{
		public:
			explicit ProgramCatalogue(const QString &file) :
				m_before(QSettings().value(QStringLiteral("catalog/path")).toString())
			{
				QETApp::setCatalogPath(file);
			}
			~ProgramCatalogue() {QETApp::setCatalogPath(m_before);}
			Catalog &catalog() {return *QETApp::catalog();}

		private:
			QString m_before;
	};

	void saveDrawing(Catalog &catalog, const QString &code, const QString &block)
	{
		CatalogPart part = catalog.partByCode(code);
		if (part.isNull()) {
			part = CatalogPart(code, catalog.classByKey(QStringLiteral("contactor")).id);
		}
		part.layout_drawing = block;
		QString error;
		REQUIRE(catalog.savePart(part, &error));
	}

	void sendMouse(ElementScene *scene, QEvent::Type type, const QPointF &pos)
	{
		QGraphicsSceneMouseEvent event(type);
		event.setScenePos(pos);
		event.setButton(type == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMousePress ? Qt::LeftButton : Qt::NoButton);
		QApplication::sendEvent(scene, &event);
	}

	QAction *actionNamed(QWidget &window, const QString &text)
	{
		for (QAction *action : window.findChildren<QAction *>()) {
			if (action->text() == text) {
				return action;
			}
		}
		FAIL("no action named " << text.toStdString());
		return nullptr;
	}
}

TEST_CASE("CU-44.11 — the drawing comes from the copy the project keeps, and travels with it",
	  "[t44][calepinagem]")
{
		//The symbol of the folio carries no drawing: the part is drawn by
		//its code, and the copy is in the project
	UiBench::ScratchProject scratch(projectXml(QString()), QStringLiteral("plate-drawing.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	MountedItem linked = coded(QSizeF(45, 100));
	linked.element = elementUuid();
	QString face;
	MountingLayout layout = plateWith(linked, &face);
	layout.setPartDrawing(part_code, drawing45x90());
		//And a part with no symbol behind it: the same code, the same drawing
	MountedItem loose = coded(QSizeF(45, 90));
	loose.uuid = QStringLiteral("q2");
	loose.position = QPointF(200, 20);
	REQUIRE_FALSE(layout.mountItem(face, loose).isEmpty());
	scratch->setMountingLayout(layout);

	auto checkPlate = [&face](QETProject *project) {
		MountingLayoutEditor editor(project);
		REQUIRE(editor.shownSurface() == face);
		MountedPartItem *on_plate = editor.scene()->partItem(QStringLiteral("q1"));
		REQUIRE(on_plate != nullptr);
		CHECK(on_plate->hasLayoutDrawing());
		CHECK(on_plate->layoutDrawingRect() == QRectF(0, 5, 45, 90));
		CHECK(on_plate->drawingSizeDiffers());
		MountedPartItem *without_symbol = editor.scene()->partItem(QStringLiteral("q2"));
		REQUIRE(without_symbol != nullptr);
		CHECK(without_symbol->hasLayoutDrawing());
	};

	SECTION("on the plate window")
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

	SECTION("after a save and a reopening with a catalogue that does not know the code")
	{
			//The catalogue of the bench is empty: the code is unknown, and
			//the copy is what draws the plate
		REQUIRE(QETApp::catalog()->partByCode(part_code).isNull());
		REQUIRE(scratch.saveAndReopen());
		checkPlate(scratch.project());
	}
}

TEST_CASE("CU-44.16 — the catalogue is the source of the copy, at opening and when it changes",
	  "[t44][calepinagem]")
{
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	ProgramCatalogue program(directory.filePath(QStringLiteral("catalogue.sqlite")));
	REQUIRE(program.catalog().isOpen());
	saveDrawing(program.catalog(), part_code, outlineDrawing(45, 100));

		//Written with the old drawing, 45 by 90
	UiBench::ScratchProject scratch(projectXml(QString()), QStringLiteral("plate-source.qet"));
	REQUIRE(scratch.isOpen());
	QString face;
	MountingLayout layout = plateWith(coded(QSizeF(45, 100)), &face);
	layout.setPartDrawing(part_code, drawing45x90());
	scratch->setMountingLayout(layout);

		//Opened again: the catalogue says 45 by 100, and the plate follows
	REQUIRE(scratch.saveAndReopen());
	CHECK(ElementPictureFactory::layoutDrawingOf(
		      scratch->mountingLayout().partDrawing(part_code)).bounds == QRectF(0, 0, 45, 100));
	CHECK_FALSE(scratch->projectWasModified());
	{
		MountingLayoutEditor editor(scratch.project());
		MountedPartItem *drawn = editor.scene()->partItem(QStringLiteral("q1"));
		REQUIRE(drawn != nullptr);
		CHECK(drawn->layoutDrawingRect() == QRectF(0, 0, 45, 100));
		CHECK_FALSE(drawn->drawingSizeDiffers());
	}

		//The drawing taken off the part, with the project open: the copy
		//goes, and the part is a box with its name again - and renewing is
		//no edit of anybody's
	auto *view = new MountingLayoutViewItem();
	scratch.diagram(0)->addItem(view);
	view->setProject(scratch.project());
	view->setSurfaceUuid(face);
	REQUIRE(view->partItem(QStringLiteral("q1"))->hasLayoutDrawing());
	saveDrawing(program.catalog(), part_code, QString());
	CHECK(scratch->mountingLayout().partDrawing(part_code).isEmpty());
	CHECK_FALSE(view->partItem(QStringLiteral("q1"))->hasLayoutDrawing());
	CHECK_FALSE(scratch->projectWasModified());
}

TEST_CASE("T44 — opening renews the copy from the catalogue, and so does saving, each on its own",
	  "[t44][calepinagem]")
{
		//Saving and reopening hides one behind the other: a save that
		//renews writes the new drawing, and the opening that should renew
		//finds nothing left to do - planted, either went unnoticed. So each
		//is asked alone here.
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	const QString file = directory.filePath(QStringLiteral("catalogue.sqlite"));
	ProgramCatalogue program(file);
	REQUIRE(program.catalog().isOpen());
	saveDrawing(program.catalog(), part_code, outlineDrawing(45, 100));

	QString face;
	MountingLayout layout = plateWith(coded(QSizeF(45, 100)), &face);
	layout.setPartDrawing(part_code, drawing45x90());
	QDomDocument written;
	written.appendChild(layout.toXml(written));
	QString project_xml = projectXml(QString());
	project_xml.replace(QStringLiteral("</project>"), written.toString(-1) + QStringLiteral("</project>"));

	SECTION("opening: a file written with the old drawing comes up with the new one")
	{
		UiBench::ScratchProject scratch(project_xml, QStringLiteral("plate-open.qet"));
		REQUIRE(scratch.isOpen());
		CHECK(ElementPictureFactory::layoutDrawingOf(
			      scratch->mountingLayout().partDrawing(part_code)).bounds == QRectF(0, 0, 45, 100));
		CHECK_FALSE(scratch->projectWasModified());
	}

	SECTION("saving: a change made elsewhere, that this program was not told of, is written")
	{
		UiBench::ScratchProject scratch(projectXml(QString()), QStringLiteral("plate-save.qet"));
		REQUIRE(scratch.isOpen());
		scratch->setMountingLayout(layout);
			//Another workstation on the same catalogue: no signal reaches
			//this program, and the next save is what carries the change
		{
			Catalog elsewhere;
			QString error;
			REQUIRE(elsewhere.open(file, &error));
			saveDrawing(elsewhere, part_code, outlineDrawing(45, 110));
		}
		REQUIRE(scratch->write().isOk());

		QFile saved(scratch.filePath());
		REQUIRE(saved.open(QIODevice::ReadOnly));
		QDomDocument document;
		REQUIRE(document.setContent(saved.readAll()));
		const QDomElement copy = document.documentElement()
				.firstChildElement(MountingLayout::tagName())
				.firstChildElement(QStringLiteral("part_drawings"))
				.firstChildElement(QStringLiteral("part_drawing"));
		REQUIRE(copy.attribute(QStringLiteral("code")) == part_code);
		CHECK(ElementPictureFactory::layoutDrawingOf(copy.firstChildElement()).bounds
		      == QRectF(0, 0, 45, 110));
	}
}

TEST_CASE("T44 — a part with no code, or a code with no copy, gets no drawing",
	  "[t44][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(QString()), QStringLiteral("plate-nodrawing.qet"));
	REQUIRE(scratch.isOpen());

	MountedItem linked = coded(QSizeF(45, 90), QStringLiteral("NO-COPY"));
	linked.element = elementUuid();
	MountedItem loose(QStringLiteral("-X9"), QPointF(100, 20), QSizeF(6, 45));
	loose.uuid = QStringLiteral("x9");
	QString face;
	MountingLayout layout = plateWith(linked, &face);
	REQUIRE_FALSE(layout.mountItem(face, loose).isEmpty());
	scratch->setMountingLayout(layout);

	const auto drawings = MountedPartItem::layoutDrawingsOf(
				scratch->mountingLayout(), scratch->mountingLayout().surface(face).items);
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
	MountingSurface surface(QStringLiteral("QCM1"), QStringLiteral("plate"),
				MountingArea(600.0, 800.0));
	surface.items << coded(QSizeF(45, 90));

	MountingScene scene;
	QHash<QString, ElementPictureFactory::LayoutDrawing> drawings;
	drawings.insert(part_code, drawingOf(drawing45x90()));
	scene.setLayoutDrawings(drawings);
	REQUIRE(scene.partItem(QStringLiteral("q1")) == nullptr);

	scene.setSurface(surface);
	MountedPartItem *drawn = scene.partItem(QStringLiteral("q1"));
	REQUIRE(drawn != nullptr);
	CHECK(drawn->hasLayoutDrawing());
	CHECK(drawn->layoutDrawingRect() == QRectF(0, 0, 45, 90));
}

TEST_CASE("CU-44.19 — the drawing of a part is modified from the plate, into the catalogue",
	  "[t44][calepinagem]")
{
	Catalog catalog;
	QString error;
	REQUIRE(catalog.openInMemory(&error));
	saveDrawing(catalog, part_code, drawing45x90());

	UiBench::ScratchProject scratch(projectXml(QString()), QStringLiteral("plate-edit.qet"));
	REQUIRE(scratch.isOpen());
	QString face;
	MountingLayout layout = plateWith(coded(QSizeF(45, 90)), &face);
	MountedItem loose(QStringLiteral("-X9"), QPointF(100, 20), QSizeF(6, 45));
	loose.uuid = QStringLiteral("x9");
	REQUIRE_FALSE(layout.mountItem(face, loose).isEmpty());
	scratch->setMountingLayout(layout);
	REQUIRE(scratch->refreshPartDrawings(catalog));

	MountingLayoutEditor editor(scratch.project());
	editor.setCatalog(&catalog);
	editor.show();
	QAction *modify = actionNamed(editor, QStringLiteral("Modifier le dessin 2D de la pièce…"));

		//A part with no code has no part to draw
	editor.scene()->partItem(QStringLiteral("x9"))->setSelected(true);
	CHECK_FALSE(modify->isEnabled());
	editor.scene()->clearSelection();
	MountedPartItem *on_plate = editor.scene()->partItem(QStringLiteral("q1"));
	on_plate->setSelected(true);
	REQUIRE(modify->isEnabled());
	modify->trigger();

	QETElementEditor *drawing = nullptr;
	for (QETElementEditor *shown : editor.findChildren<QETElementEditor *>()) {
		if (shown->isVisible() && shown->isPartDrawingMode()) {
			drawing = shown;
		}
	}
	REQUIRE(drawing != nullptr);
	CHECK(drawing->windowTitle().contains(part_code));
		//It opens on what the catalogue holds
	REQUIRE(drawing->layoutDrawingScene()->items().size() >= 1);

		//A line drawn with the tool, and saved
	actionNamed(*drawing, QStringLiteral("Ajouter une ligne"))->trigger();
	ElementScene *scene = drawing->elementScene();
	sendMouse(scene, QEvent::GraphicsSceneMousePress, QPointF(0, 45));
	sendMouse(scene, QEvent::GraphicsSceneMouseRelease, QPointF(0, 45));
	sendMouse(scene, QEvent::GraphicsSceneMouseMove, QPointF(45, 45));
	sendMouse(scene, QEvent::GraphicsSceneMousePress, QPointF(45, 45));
	sendMouse(scene, QEvent::GraphicsSceneMouseRelease, QPointF(45, 45));
	QAction *save = drawing->findChild<QAction *>(QStringLiteral("m_save_action"));
	REQUIRE(save);
	save->trigger();

		//In the catalogue, in the copy of the project, and on the plate,
		//without reopening anything
	CHECK(ElementPictureFactory::layoutDrawingOf(catalog.partByCode(part_code).layout_drawing).parts == 2);
	CHECK(ElementPictureFactory::layoutDrawingOf(scratch->mountingLayout().partDrawing(part_code)).parts == 2);
	CHECK(editor.scene()->partItem(QStringLiteral("q1"))->hasLayoutDrawing());
	CHECK(drawing->close());
}

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

	QString imageDrawing(const QByteArray &bytes, const QString &format)
	{
		return QStringLiteral(
			"<layout_drawing>"
			"<image x=\"0\" y=\"0\" width=\"25\" height=\"85\" format=\"%1\">%2</image>"
			"</layout_drawing>").arg(format, QString::fromLatin1(bytes.toBase64()));
	}
}

TEST_CASE("CU-44.14 — the plate shows an image at its real size",
	  "[t44][calepinagem]")
{
	const QList<QPair<QString, QByteArray>> files {
		{QStringLiteral("png"), rasterFile("PNG", QSize(40, 80), Qt::red)},
		{QStringLiteral("jpg"), rasterFile("JPG", QSize(40, 80), Qt::red)},
		{QStringLiteral("svg"), svgFile()}};
	for (const auto &file : files)
	{
		INFO("format " << file.first.toStdString());
		const auto drawing = drawingOf(imageDrawing(file.second, file.first));
		CHECK(drawing.parts == 1);
		CHECK(drawing.bounds == QRectF(0, 0, 25, 85));

			//25 by 85 in the catalogue too: no mark, the image filling the
			//box - stretched, whatever the proportion of the file
		auto *item = new MountedPartItem(part(QSizeF(25, 85)));
		item->setLayoutDrawing(drawing);
		CHECK_FALSE(item->drawingSizeDiffers());
		CHECK(item->layoutDrawingRect() == QRectF(0, 0, 25, 85));
		const QColor seen = middleOf(item);
		CHECK(seen.red() > 200);
		CHECK(seen.green() < 60);
		CHECK(seen.blue() < 60);
		delete item;
	}
}
