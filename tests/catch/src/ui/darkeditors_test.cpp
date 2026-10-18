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

#include "../../../../sources/diagram.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/elementview.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partrectangle.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/layout/mountingview.h"
#include "../../../../sources/qetpalette.h"
#include "../../../../sources/titleblock/qettemplateeditor.h"
#include "../../../../sources/titleblock/templateview.h"
#include "../../../../sources/titleblock/templatevisualcell.h"
#include "../../../../sources/titleblockcell.h"
#include "../../../../sources/titleblocktemplate.h"

#include <catch2/catch.hpp>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QHash>
#include <QIODevice>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <string>

/*
	The three drawing areas that stayed light on a dark palette: the symbol
	editor (white, with a black grid), the title block template editor
	(light yellow) and the mounting plate (light grey). Each of them is a
	PaperGraphicsView now, and shows its scene with the lightness inverted
	the way the folio does, while the scene keeps drawing for white paper.

	So two things are proved, and they pull in opposite directions. What is
	seen changes: the area dark, the strokes, lines and texts light, the
	selection visible. What is saved does not: the same symbol, the same
	template, the same plate as on a light palette. A fix that recolored the
	primitives would pass the first half and fail the second.

	Everything is measured on the viewport as Qt renders it, grabbed under
	the offscreen platform. Contrast is the WCAG 2 ratio of
	QET::Palette::contrastRatio(): 4.5 for text and strokes, 3 for the
	outline of a part. The dark palette is QET::Palette::fusionDark(), Base
	(30,30,30) and Text (220,220,220), the one the program builds for a
	dark session under Fusion.
*/

namespace
{
	/**
		Applies a palette to the application for as long as it lives, and
		puts the previous one back: the suite is one process, and every case
		after this one expects the palette it started with.
	*/
	class ApplicationPalette
	{
		public:
			explicit ApplicationPalette(const QPalette &palette) :
				m_previous(QApplication::palette())
			{
				QApplication::setPalette(palette);
				QCoreApplication::processEvents();
			}
			~ApplicationPalette()
			{
				QApplication::setPalette(m_previous);
				QCoreApplication::processEvents();
			}
			ApplicationPalette(const ApplicationPalette &) = delete;
			ApplicationPalette &operator=(const ApplicationPalette &) = delete;

		private:
			const QPalette m_previous;
	};

	std::string describe(const QColor &color)
	{
		return QStringLiteral("(%1,%2,%3)")
			.arg(color.red()).arg(color.green()).arg(color.blue())
			.toStdString();
	}

	std::string describe(const QRect &rect)
	{
		return QStringLiteral("[%1,%2 %3x%4]")
			.arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height())
			.toStdString();
	}

	/** Show @a window as a person would see it, on nobody's screen. */
	void showWindow(QWidget &window)
	{
		window.resize(1280, 800);
		window.show();
		REQUIRE(QTest::qWaitForWindowExposed(&window));
		QCoreApplication::processEvents();
	}

	/** @return what the viewport of @a view shows, as Qt renders it. */
	QImage grabbed(QGraphicsView *view)
	{
		QCoreApplication::processEvents();
		return view->viewport()->grab().toImage();
	}

	/** @return @a scene_rect in the coordinates of the viewport of @a view */
	QRect onViewport(const QGraphicsView *view, const QRectF &scene_rect)
	{
		return view->mapFromScene(scene_rect).boundingRect();
	}

	/**
		@return the color most pixels of @a area have: the ground of a
		drawing made of lines, since the lines are the minority.
	*/
	QColor modeColor(const QImage &image, const QRect &area)
	{
		const QRect clipped = area.intersected(image.rect());
		QHash<QRgb, int> counts;
		for (int y = clipped.top() ; y <= clipped.bottom() ; ++ y) {
			for (int x = clipped.left() ; x <= clipped.right() ; ++ x) {
				++ counts[image.pixel(x, y) & 0x00ffffffu];
			}
		}
		QRgb mode = 0;
		int most = -1;
		for (auto it = counts.cbegin() ; it != counts.cend() ; ++ it) {
			if (it.value() > most) {
				most = it.value();
				mode = it.key();
			}
		}
		return QColor(mode);
	}

	/**
		Calls @a visit with each pixel of @a outer that is not in @a inner
		(an empty @a inner leaves the whole of @a outer).
	*/
	template <typename Visit>
	void forEachPixel(const QImage &image, const QRect &outer, const QRect &inner, Visit visit)
	{
		const QRect clipped = outer.intersected(image.rect());
		for (int y = clipped.top() ; y <= clipped.bottom() ; ++ y) {
			for (int x = clipped.left() ; x <= clipped.right() ; ++ x) {
				if (!inner.isEmpty() && inner.contains(x, y)) {
					continue;
				}
				visit(QColor(image.pixel(x, y)));
			}
		}
	}

	/** @return the pixel of @a outer, out of @a inner, that contrasts most with @a ground */
	QColor mostContrasting(const QImage &image, const QRect &outer, const QRect &inner,
			       const QColor &ground)
	{
		QColor best = ground;
		double best_ratio = 1.0;
		forEachPixel(image, outer, inner, [&](const QColor &pixel) {
			const double ratio = QET::Palette::contrastRatio(pixel, ground);
			if (ratio > best_ratio) {
				best_ratio = ratio;
				best = pixel;
			}
		});
		return best;
	}

	/** @return how many pixels of @a outer, out of @a inner, reach @a ratio against @a ground */
	int countContrasting(const QImage &image, const QRect &outer, const QRect &inner,
			     const QColor &ground, double ratio)
	{
		int count = 0;
		forEachPixel(image, outer, inner, [&](const QColor &pixel) {
			if (QET::Palette::contrastRatio(pixel, ground) >= ratio) {
				++ count;
			}
		});
		return count;
	}

	/** @return the most red pixel of @a area: red minus the larger other channel */
	QColor reddest(const QImage &image, const QRect &area)
	{
		QColor best(Qt::black);
		int best_score = -256;
		forEachPixel(image, area, QRect(), [&](const QColor &pixel) {
			const int score = pixel.red() - qMax(pixel.green(), pixel.blue());
			if (score > best_score) {
				best_score = score;
				best = pixel;
			}
		});
		return best;
	}

	/**
		@return whether this build draws letters at all. Measured, as in
		titleblockeditor_test.cpp: a build that rasterises no glyph without
		a screen would make every assertion on text measure the platform.
	*/
	bool rasterisesGlyphs()
	{
		QImage image(60, 20, QImage::Format_ARGB32);
		image.fill(Qt::white);
		{
			QPainter painter(&image);
			painter.setPen(Qt::black);
			painter.drawText(QRect(0, 0, 60, 20), Qt::AlignLeft | Qt::AlignVCenter,
					 QStringLiteral("mmm"));
		}
		return mostContrasting(image, image.rect(), QRect(), Qt::white) != QColor(Qt::white);
	}

	/**
		Sets the background of the folio for as long as it lives, and puts
		the previous one back. Diagram::background_color is one value for
		the whole program, and the cells of a title block choose their ink
		from it.
	*/
	class FolioBackground
	{
		public:
			explicit FolioBackground(const QColor &color) :
				m_previous(Diagram::background_color)
			{
				Diagram::background_color = color;
			}
			~FolioBackground()
			{
				Diagram::background_color = m_previous;
			}
			FolioBackground(const FolioBackground &) = delete;
			FolioBackground &operator=(const FolioBackground &) = delete;

		private:
			const QColor m_previous;
	};

	QByteArray fileContent(const QString &path)
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		return file.readAll();
	}
}

/*
	The symbol editor.
*/
namespace
{
	/**
		@return a writable copy, in @a dir, of a coil of the collection the
		program ships with: a rectangle and two lines, all black. Copied
		rather than opened in place, so that nothing can write to the
		source tree - and written rather than QFile::copy'd, because a
		read-only copy would open the editor's read-only warning, a modal
		box nothing here could close.
	*/
	QString coilSymbol(const QTemporaryDir &dir)
	{
		const QByteArray bytes = fileContent(QStringLiteral(
			QET_EXAMPLES_DIR "/../elements/10_electric/10_allpole/"
			"310_relays_contactors_contacts/01_coils/bobine3.elmt"));
		REQUIRE_FALSE(bytes.isEmpty());
		const QString path = dir.filePath(QStringLiteral("bobine.elmt"));
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		REQUIRE(file.write(bytes) == bytes.size());
		return path;
	}

	template <typename Part>
	QList<Part *> partsOf(ElementScene *scene)
	{
		QList<Part *> found;
		const QList<QGraphicsItem *> items = scene->items();
		for (QGraphicsItem *item : items) {
			if (auto part = dynamic_cast<Part *>(item)) {
				found << part;
			}
		}
		return found;
	}

	QAction *editorAction(QETElementEditor &editor, const QString &text)
	{
		const QList<QAction *> actions = editor.findChildren<QAction *>();
		for (QAction *action : actions) {
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

	/**
		The edit of the case: a line drawn under the coil with the tool of
		the bar, the way a person draws one. The same on both palettes.
	*/
	void addLineUnderTheCoil(QETElementEditor &editor)
	{
		ElementScene *scene = editor.elementScene();
		const int lines = partsOf<PartLine>(scene).size();
		scene->clearSelection();
		editorAction(editor, QStringLiteral("Add a line"))->trigger();
		clickAt(scene, QPointF(-20, 30));
		sendMouse(scene, QEvent::GraphicsSceneMouseMove, QPointF(20, 30));
		clickAt(scene, QPointF(20, 30));
		REQUIRE(partsOf<PartLine>(scene).size() == lines + 1);
	}

	/**
		@return @a saved in the form two saves of the same edit share, with
		@a blanked set to the number of uuids it blanked.

		Two things in a saved symbol do not depend on the edit, and both
		were measured to differ between two saves on the same palette:
		- the uuids @a source does not have. ElementScene::toXml() gives the
		  definition a new one on every save, so that a folio knows its
		  symbol changed; and a graphic part read from a file written before
		  parts carried one keeps the fresh uuid it was built with
		  (CustomElementGraphicPart::stylesFromXml()), so two editors that
		  open the coil give its rectangle and lines different ones. These
		  are blanked; the uuids of the terminals and texts come from the
		  file, and are compared.
		- the order of the <terminal> lines. ElementScene::zItems() sorts
		  the drawing by z value and appends the terminals in the order the
		  scene lists its items, and a scene without an index lists them
		  sorted by address once any item has been removed
		  (QGraphicsSceneLinearIndex::removeItem()): over five runs the two
		  terminals of the coil came out A1, A2 in some saves and A2, A1 in
		  others, on either palette. They are put in one order here; every
		  other line keeps its own, the drawing's included.
	*/
	QByteArray comparable(const QByteArray &saved, const QByteArray &source, int *blanked)
	{
		static const QRegularExpression uuid(
			QStringLiteral("uuid=\"(\\{[0-9a-fA-F-]+\\})\""));
		QSet<QString> in_source;
		QRegularExpressionMatchIterator source_uuids = uuid.globalMatch(QString::fromUtf8(source));
		while (source_uuids.hasNext()) {
			in_source.insert(source_uuids.next().captured(1));
		}

		*blanked = 0;
		QStringList lines = QString::fromUtf8(saved).split(QLatin1Char('\n'));
		QList<int> terminal_rows;
		QStringList terminals;
		for (int row = 0 ; row < lines.size() ; ++ row) {
			const QString &line = lines.at(row);
			QString kept;
			int from = 0;
			QRegularExpressionMatchIterator matches = uuid.globalMatch(line);
			while (matches.hasNext()) {
				const QRegularExpressionMatch match = matches.next();
				if (in_source.contains(match.captured(1))) {
					continue;
				}
				kept += line.mid(from, match.capturedStart(1) - from);
				kept += QStringLiteral("{}");
				from = match.capturedEnd(1);
				++ *blanked;
			}
			kept += line.mid(from);
			lines[row] = kept;
			if (kept.trimmed().startsWith(QLatin1String("<terminal "))) {
				terminal_rows << row;
				terminals << kept;
			}
		}
		terminals.sort();
		for (int i = 0 ; i < terminal_rows.size() ; ++ i) {
			lines[terminal_rows.at(i)] = terminals.at(i);
		}
		return lines.join(QLatin1Char('\n')).toUtf8();
	}
}

TEST_CASE("CU-63.8 — o editor de símbolos em modo escuro: área escura, traço claro, "
	  "e o arquivo salvo é o do modo claro",
	  "[t63][darkmode][ui]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString symbol = coilSymbol(dir);
	const QByteArray source = fileContent(symbol);
	const QString saved_dark  = dir.filePath(QStringLiteral("escuro.elmt"));
	const QString saved_light = dir.filePath(QStringLiteral("claro.elmt"));

	{
		ApplicationPalette dark(QET::Palette::fusionDark());
		const QColor base = QApplication::palette().color(QPalette::Base);

		QETElementEditor editor;
		editor.fromFile(symbol);
		showWindow(editor);
		ElementView *view = editor.elementView();
		REQUIRE(view != nullptr);
		view->zoomReset();
		view->centerOn(0, 0);

		const QList<PartRectangle *> rectangles = partsOf<PartRectangle>(editor.elementScene());
		REQUIRE(rectangles.size() == 1);
		PartRectangle *coil = rectangles.first();
			// A piece of the top edge of the coil's rectangle, on the left:
			// away from the middle, where the hotspot cross is drawn in red
			// and the selection handles sit. The geometric rectangle, and
			// not the bounding one, which reaches 2.5 units further out to
			// take in the hover shadow.
		const QRectF outline = coil->sceneGeometricRect();
		const QRect top_edge = onViewport(view, QRectF(outline.left() + 3, outline.top() - 1, 6, 2));

		const QImage image = grabbed(view);
		REQUIRE(image.rect().contains(onViewport(view, coil->sceneBoundingRect())));

			// The area is the paper the view inverts, and white comes out
			// as the palette's Base exactly.
		const QColor ground = modeColor(image, image.rect());
		INFO("area " << describe(ground));
		CHECK(ground.lightness() < 80);
		CHECK(ground == base);

			// The black outline of the coil comes out light, at text
			// contrast.
		const QColor stroke = mostContrasting(image, top_edge, QRect(), ground);
		INFO("stroke " << describe(stroke)
		     << ", contrast " << QET::Palette::contrastRatio(stroke, ground));
		CHECK(stroke.lightness() > ground.lightness());
		CHECK(QET::Palette::contrastRatio(stroke, ground) >= 4.5);
		const QColor unselected = reddest(image, top_edge);
		CHECK(unselected.red() - qMax(unselected.green(), unselected.blue()) < 50);

			// Selected, the primitive turns red, and the red still reads
			// on the dark area: its hue is kept by the inversion.
		coil->setSelected(true);
		const QImage selected = grabbed(view);
		const QColor red = reddest(selected, top_edge);
		INFO("selected stroke " << describe(red)
		     << ", contrast " << QET::Palette::contrastRatio(red, ground));
		CHECK(red.red() - qMax(red.green(), red.blue()) >= 100);
		CHECK(QET::Palette::contrastRatio(red, ground) >= 3.0);

		addLineUnderTheCoil(editor);
		REQUIRE(editor.toFile(saved_dark));
	}

	{
		ApplicationPalette light(QET::Palette::fusionLight());

		QETElementEditor editor;
		editor.fromFile(symbol);
		showWindow(editor);
		ElementView *view = editor.elementView();
		REQUIRE(view != nullptr);
		view->zoomReset();
		view->centerOn(0, 0);
		grabbed(view);

		addLineUnderTheCoil(editor);
		REQUIRE(editor.toFile(saved_light));
	}

	const QByteArray dark_file  = fileContent(saved_dark);
	const QByteArray light_file = fileContent(saved_light);
	REQUIRE_FALSE(dark_file.isEmpty());
		// The edit is in both: the coil's two lines and the one drawn.
	CHECK(dark_file.count("<line ") == 3);
	CHECK(light_file.count("<line ") == 3);
		// Same length - a uuid is as long on both sides, and the terminals
		// only change places - and the same bytes once what does not
		// depend on the edit is set aside (comparable()): the uuid of the
		// definition, those of the rectangle and of the three lines.
	CHECK(dark_file.size() == light_file.size());
	int dark_blanked = 0;
	int light_blanked = 0;
	const QByteArray dark_comparable = comparable(dark_file, source, &dark_blanked);
	const QByteArray light_comparable = comparable(light_file, source, &light_blanked);
	CHECK(dark_blanked == 5);
	CHECK(light_blanked == 5);
	CHECK(dark_comparable == light_comparable);
}

/*
	The title block template editor and the mounting plate.
*/
namespace
{
	/**
		Three text cells and one the file leaves out. The text is large
		and the zoom is one, so that the stems of the letters are wider
		than a pixel: a stem of one pixel is drawn half on each of two, and
		its contrast would measure the antialiasing rather than the color.
	*/
	QString darkTemplate()
	{
		return QStringLiteral(
			"<titleblocktemplate name=\"escuro\">\n"
			"  <information></information>\n"
			"  <logos/>\n"
			"  <grid cols=\"t50%;t50%;\" rows=\"40;40;\">\n"
			"    <field row=\"0\" col=\"0\" name=\"medida\""
			" displaylabel=\"false\" align=\"center\" valign=\"center\""
			" fontsize=\"16\">\n"
			"      <value><translation lang=\"en\">MMMM"
			"</translation></value>\n"
			"    </field>\n"
			"    <field row=\"0\" col=\"1\" name=\"outra\""
			" displaylabel=\"false\" align=\"center\" valign=\"center\""
			" fontsize=\"16\">\n"
			"      <value><translation lang=\"en\">HHHH"
			"</translation></value>\n"
			"    </field>\n"
			"    <field row=\"1\" col=\"0\" name=\"terceira\""
			" displaylabel=\"false\" align=\"center\" valign=\"center\""
			" fontsize=\"16\">\n"
			"      <value><translation lang=\"en\">WWWW"
			"</translation></value>\n"
			"    </field>\n"
			"  </grid>\n"
			"</titleblocktemplate>\n");
	}

	/** @return the item the view draws for @a cell, found in its scene */
	TitleBlockTemplateVisualCell *visualCellOf(QGraphicsScene *scene,
						   const TitleBlockCell *cell)
	{
		const QList<QGraphicsItem *> items = scene->items();
		for (QGraphicsItem *item : items) {
			auto *visual = dynamic_cast<TitleBlockTemplateVisualCell *>(item);
			if (visual && visual->cell() == cell) {
				return visual;
			}
		}
		return nullptr;
	}

	/// A breaker of a real width, measured.
	MountedItem breaker()
	{
		MountedItem item(QStringLiteral("-Q1"),
				 QPointF(10.5, 20.25),
				 QSizeF(22.5, 85.0));
		item.uuid = QStringLiteral("q1");
		item.part_code = QStringLiteral("A9F74210");
		return item;
	}

	/// A part nobody measured yet, off to the side.
	MountedItem unmeasuredRelay()
	{
		MountedItem item(QStringLiteral("-KA1"),
				 QPointF(200.0, 40.0),
				 QSizeF());
		item.uuid = QStringLiteral("ka1");
		item.part_code = QStringLiteral("RXM4AB2BD");
		return item;
	}

	/// A plate of 600 by 800, with the two of them on it.
	MountingSurface plate()
	{
		MountingSurface surface(QStringLiteral("QCM1"),
					QStringLiteral("plate"),
					MountingArea(600.0, 800.0));
		surface.uuid = QStringLiteral("face");
		surface.name = QStringLiteral("Platine");
		surface.items << breaker() << unmeasuredRelay();
		return surface;
	}

	/** @return the plate as it is written into the project */
	QString xmlOf(const MountingSurface &surface)
	{
		QDomDocument document;
		document.appendChild(surface.toXml(document));
		return document.toString();
	}

	/// Bare plate, right of the breaker, in millimetres.
	const QRectF bare_plate(50., 30., 70., 70.);
	/// The room around the plate, left of its edge, in millimetres.
	const QRectF around_plate(-18., 30., 16., 70.);

	/**
		Show @a view at eight pixels per millimetre, on the breaker: wide
		enough for the name of the part to be drawn in letters whose stems
		are several pixels wide, as the zoom of the template is chosen.

		The events are let through between the zoom and the centring, and
		that order is measured, not taste. At eight pixels per millimetre
		the horizontal scroll bar appears, and on the next turn of the
		event loop the viewport shrinks from 1264x798 to 1264x784; the view
		then keeps the point under the mouse where it was (its resize
		anchor is AnchorUnderMouse), and the offscreen platform puts the
		mouse at (10,10), inside the window. Centred before that turn, the
		view ended scrolled to -7,-7 instead of -160,110, the part ran off
		the bottom of the image and the room left of the plate was off its
		left edge.
	*/
	QRect showBreaker(MountingView &view, MountedPartItem *part)
	{
		showWindow(view);
		view.resetTransform();
		view.scale(8.0, 8.0);
		QCoreApplication::processEvents();
		const QRectF body = part->mapRectToScene(part->drawnRect());
		view.centerOn(body.center());
		QCoreApplication::processEvents();
		return onViewport(&view, body);
	}
}

TEST_CASE("CU-63.9 — o editor de carimbo e a vista da placa em modo escuro: fundo escuro, "
	  "linhas e textos claros, a seleção visível, e o que se salva é o do modo claro",
	  "[t63][darkmode][ui]")
{
	const bool glyphs = rasterisesGlyphs();

	SECTION("o editor de carimbo")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString path = dir.filePath(QStringLiteral("escuro.titleblock"));
		{
			QFile file(path);
			REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
			REQUIRE(file.write(darkTemplate().toUtf8()) > 0);
		}

			// Owned by the case and declared before the editor, which
			// never deletes the template it edits.
		std::unique_ptr<TitleBlockTemplate> tbt(new TitleBlockTemplate());
		REQUIRE(tbt->loadFromXmlFile(path));
		TitleBlockCell *measured = tbt->cell(0, 0);
		REQUIRE(measured != nullptr);
		REQUIRE(measured->type() == TitleBlockCell::TextCell);

		const QString saved_dark  = dir.filePath(QStringLiteral("salvo-escuro.titleblock"));
		const QString saved_light = dir.filePath(QStringLiteral("salvo-claro.titleblock"));

		ApplicationPalette dark(QET::Palette::fusionDark());
			// The hardest folio a user can have chosen. On a black sheet
			// the cells draw white ink (TitleBlockTemplate::renderCell()),
			// which neither the editor's light paper nor its inverted dark
			// one can show: the view paints the preview as for a white
			// sheet, and leaves the folio's color as it found it.
		FolioBackground black_sheet(Qt::black);

		QETTitleBlockTemplateEditor editor;
		REQUIRE(editor.edit(tbt.get()));
		showWindow(editor);
		auto *view = qobject_cast<TitleBlockTemplateView *>(editor.centralWidget());
		REQUIRE(view != nullptr);

		TitleBlockTemplateVisualCell *visual = visualCellOf(view->scene(), measured);
		REQUIRE(visual != nullptr);
		view->zoomReset();
		view->centerOn(visual);

		const QRect cell_px = onViewport(view, visual->sceneBoundingRect());
		const QRect border  = cell_px.adjusted(-2, -2, 2, 2);
		const QRect inside  = cell_px.adjusted(4, 4, -4, -4);

		const QImage image = grabbed(view);
		REQUIRE(image.rect().contains(border));
		CHECK(Diagram::background_color == QColor(Qt::black));

			// The ground of the editor: dark, and not the saturated olive
			// the yellow itself would have turned into.
		const QColor paper = modeColor(image, image.rect());
		INFO("ground " << describe(paper));
		CHECK(paper.lightness() < 80);

			// The lines of the cell: drawn black for the folio, light here.
		const QColor line = mostContrasting(image, border, inside, paper);
		INFO("cell line " << describe(line)
		     << ", contrast " << QET::Palette::contrastRatio(line, paper));
		CHECK(line.lightness() > paper.lightness());
		CHECK(QET::Palette::contrastRatio(line, paper) >= 4.5);

		if (glyphs) {
			const QColor text = mostContrasting(image, inside, QRect(), paper);
			INFO("cell text " << describe(text)
			     << ", contrast " << QET::Palette::contrastRatio(text, paper));
			CHECK(text.lightness() > paper.lightness());
			CHECK(QET::Palette::contrastRatio(text, paper) >= 4.5);
		}

			// Selected, the cell takes the palette's highlight, inverted
			// with the rest: a blue that stands apart from the ground, and
			// the text of the cell still reads on it.
		visual->setSelected(true);
		REQUIRE(view->selectedCells().count() == 1);
		const QImage selected = grabbed(view);
		const QColor highlight = modeColor(selected, inside);
		INFO("selected cell " << describe(highlight)
		     << ", against the ground " << QET::Palette::contrastRatio(highlight, paper));
		CHECK(QET::Palette::contrastRatio(highlight, paper) >= 1.5);
		CHECK(highlight.blue() > highlight.red() + 20);
		if (glyphs) {
			const QColor text = mostContrasting(selected, inside, QRect(), highlight);
			INFO("text on the selection " << describe(text)
			     << ", contrast " << QET::Palette::contrastRatio(text, highlight));
			CHECK(text.lightness() > highlight.lightness());
			CHECK(QET::Palette::contrastRatio(text, highlight) >= 4.5);
		}

		REQUIRE(tbt->saveToXmlFile(saved_dark));
		{
			ApplicationPalette light(QET::Palette::fusionLight());
			grabbed(view);
			REQUIRE(tbt->saveToXmlFile(saved_light));
		}
		const QByteArray dark_file = fileContent(saved_dark);
		REQUIRE_FALSE(dark_file.isEmpty());
		CHECK(dark_file == fileContent(saved_light));
	}

	SECTION("a vista da placa")
	{
		MountingScene scene;
		scene.setSurface(plate());
		const QString before = xmlOf(scene.surface());
		MountedPartItem *part = scene.partItem(QStringLiteral("q1"));
		REQUIRE(part != nullptr);

		ApplicationPalette dark(QET::Palette::fusionDark());

		MountingView view(&scene);
		const QRect body = showBreaker(view, part);
		const QRect band_out = body.adjusted(-2, -2, 2, 2);
		const QRect band_in  = body.adjusted(3, 3, -3, -3);
		const QRect inside   = body.adjusted(4, 4, -4, -4);

		const QImage image = grabbed(&view);
		const QRect plate_px  = onViewport(&view, bare_plate);
		const QRect around_px = onViewport(&view, around_plate);
		INFO("image " << describe(image.rect()) << ", part " << describe(band_out)
		     << ", plate " << describe(plate_px) << ", around " << describe(around_px));
		REQUIRE(image.rect().contains(band_out));
		REQUIRE(image.rect().contains(plate_px));
		REQUIRE(image.rect().contains(around_px));

			// The plate, where no part is, and the room around it: both
			// dark now, as both were light.
		const QColor plate_ground = modeColor(image, plate_px);
		const QColor around = modeColor(image, around_px);
		INFO("plate " << describe(plate_ground) << ", around it " << describe(around));
		CHECK(plate_ground.lightness() < 80);
		CHECK(around.lightness() < 80);

			// The outline of the part: light, and at 3:1 at least on the
			// plate, the contrast a control's edge needs.
		const QColor outline = mostContrasting(image, band_out, band_in, plate_ground);
		INFO("outline " << describe(outline)
		     << ", contrast " << QET::Palette::contrastRatio(outline, plate_ground));
		CHECK(outline.lightness() > plate_ground.lightness());
		CHECK(QET::Palette::contrastRatio(outline, plate_ground) >= 3.0);

			// The name of the part, on the part.
		if (glyphs) {
			const QColor body_ground = modeColor(image, inside);
			const QColor name = mostContrasting(image, inside, QRect(), body_ground);
			INFO("name " << describe(name) << " on the part " << describe(body_ground)
			     << ", contrast " << QET::Palette::contrastRatio(name, body_ground));
			CHECK(name.lightness() > body_ground.lightness());
			CHECK(QET::Palette::contrastRatio(name, body_ground) >= 4.5);
		}

			// Selected, the outline is drawn twice as wide: the pixels at
			// 3:1 on the plate, around the part, grow by about as much.
		const int outline_before = countContrasting(image, band_out, band_in, plate_ground, 3.0);
		part->setSelected(true);
		const QImage selected = grabbed(&view);
		const int outline_after = countContrasting(selected, band_out, band_in, plate_ground, 3.0);
		INFO("outline pixels: " << outline_before << " before the selection, "
		     << outline_after << " after");
		CHECK(outline_before > 0);
		CHECK(outline_after > outline_before * 3 / 2);

			// Nothing of it reaches the plate that is saved.
		CHECK(xmlOf(scene.surface()) == before);
		{
			ApplicationPalette light(QET::Palette::fusionLight());
			grabbed(&view);
			CHECK(xmlOf(scene.surface()) == before);
		}
	}
}

TEST_CASE("T63 — na paleta clara, os três editores desenham como antes",
	  "[t63][darkmode][ui]")
{
	ApplicationPalette light(QET::Palette::fusionLight());

	SECTION("o editor de símbolos: fundo branco, traço preto")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		QETElementEditor editor;
		editor.fromFile(coilSymbol(dir));
		showWindow(editor);
		ElementView *view = editor.elementView();
		REQUIRE(view != nullptr);
		view->zoomReset();
		view->centerOn(0, 0);

		const QList<PartRectangle *> rectangles = partsOf<PartRectangle>(editor.elementScene());
		REQUIRE(rectangles.size() == 1);
		const QRect around = onViewport(view, rectangles.first()->sceneBoundingRect())
				     .adjusted(-4, -4, 4, 4);

		const QImage image = grabbed(view);
		const QColor ground = modeColor(image, image.rect());
		INFO("area " << describe(ground));
		CHECK(ground == QColor(Qt::white));
		const QColor stroke = mostContrasting(image, around, QRect(), ground);
		INFO("stroke " << describe(stroke));
		CHECK(stroke == QColor(Qt::black));
	}

	SECTION("o editor de carimbo: o amarelo de sempre")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString path = dir.filePath(QStringLiteral("claro.titleblock"));
		{
			QFile file(path);
			REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
			REQUIRE(file.write(darkTemplate().toUtf8()) > 0);
		}
		std::unique_ptr<TitleBlockTemplate> tbt(new TitleBlockTemplate());
		REQUIRE(tbt->loadFromXmlFile(path));

		QETTitleBlockTemplateEditor editor;
		REQUIRE(editor.edit(tbt.get()));
		showWindow(editor);
		auto *view = qobject_cast<TitleBlockTemplateView *>(editor.centralWidget());
		REQUIRE(view != nullptr);
		view->zoomReset();

		const QColor paper = modeColor(grabbed(view), view->viewport()->rect());
		INFO("ground " << describe(paper));
		CHECK(paper == QColor(248, 255, 160));
	}

	SECTION("a vista da placa: a placa branca no cinza-claro")
	{
		MountingScene scene;
		scene.setSurface(plate());
		MountedPartItem *part = scene.partItem(QStringLiteral("q1"));
		REQUIRE(part != nullptr);

		MountingView view(&scene);
		showBreaker(view, part);
		const QImage image = grabbed(&view);
		const QRect plate_px  = onViewport(&view, bare_plate);
		const QRect around_px = onViewport(&view, around_plate);
		INFO("image " << describe(image.rect()) << ", plate " << describe(plate_px)
		     << ", around " << describe(around_px));
		REQUIRE(image.rect().contains(plate_px));
		REQUIRE(image.rect().contains(around_px));
		const QColor plate_ground = modeColor(image, plate_px);
		const QColor around = modeColor(image, around_px);
		INFO("plate " << describe(plate_ground) << ", around it " << describe(around));
		CHECK(plate_ground == QColor(Qt::white));
		CHECK(around == QColor(246, 246, 246));
	}
}
