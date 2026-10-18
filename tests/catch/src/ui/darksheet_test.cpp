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
#include "../../../../sources/diagramview.h"
#include "../../../../sources/palettegraphicsview.h"
#include "../../../../sources/print/projectprintwindow.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/diagramimageitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetpalette.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/diagrambgcolorbutton.h"

#include <catch2/catch.hpp>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QLineF>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMetaObject>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPointF>
#include <QPrinter>
#include <QSettings>
#include <QTemporaryDir>
#include <QTransform>
#include <QVariant>

#include <string>

#ifdef QET_HAS_QTPDF
#include <QPdfDocument>
#endif

/*
	A sheet color of the user's own, on a dark palette (T63, step 1).

	The rule being proved is the one decided for it: a chosen sheet color
	on which the palette's light ink reads better than black makes the
	black ink light, the view inverting the folio onto that color as it
	does onto the palette's own sheet; on any other the folio is left as
	drawn, black on it. And the screen only: printing and the PDF still
	come out black on white.

	Everything is measured on what the view itself paints, through its own
	paint event, the inversion included: the viewport of a DiagramView is
	rendered into an image, at a zoom where a line one unit thick covers
	four pixels across, so the middle of a stroke is the stroke's own color
	and not a blend with the sheet. The view is never shown (see layOut()).

	Proved by pixel and by count, never by glyph: the text of a sheet is
	drawn with whatever font the machine has. What does not move is the
	color of a stroke, of a picture and of the sheet, and those are what
	is compared - exactly, since the view's mapping is applied here to the
	same colors with the same function (shownInverted()).
*/

namespace
{
	/// One sheet; its symbols and its 69 wires are drawn in black.
	const QString example = QStringLiteral("741.qet");

	/// The zoom the strokes are looked at.
	const qreal stroke_zoom = 4.;

	/// The viewport the shots are taken through.
	const QSize view_size(640, 480);

	/// The color of the pictures pasted on the sheet: far from gray, so a
	/// picture that was inverted cannot pass for one that was not.
	const QColor picture_color(200, 60, 30);

	/// The settings the sheet color picker writes when a color is chosen.
	const QStringList sheet_keys {
		QStringLiteral("diagrameditor/sheet_background_color"),
		QStringLiteral("diagrameditor/sheet_background_custom")};

	/**
		Puts back what the cases change in the whole process, whatever they
		did: the application palette, the sheet color every folio shares and
		its "custom" flag (both static), and the two settings the picker
		writes.
	*/
	class KeepSheetState
	{
		public:
			KeepSheetState() :
				m_palette(QApplication::palette()),
				m_custom(PaletteGraphicsView::customBackgroundColor()),
				m_color(Diagram::background_color)
			{
				QSettings settings;
				for (const QString &key : sheet_keys)
				{
					if (settings.contains(key))
					{
						m_settings.insert(key, settings.value(key));
					}
				}
			}
			~KeepSheetState()
			{
				QSettings settings;
				for (const QString &key : sheet_keys)
				{
					if (m_settings.contains(key))
					{
						settings.setValue(key, m_settings.value(key));
					}
					else
					{
						settings.remove(key);
					}
				}
				PaletteGraphicsView::setCustomBackgroundColor(m_custom);
				Diagram::background_color = m_color;
				QApplication::setPalette(m_palette);
			}

			KeepSheetState(const KeepSheetState &) = delete;
			KeepSheetState &operator=(const KeepSheetState &) = delete;

		private:
			QPalette m_palette;
			bool m_custom;
			QColor m_color;
			QMap<QString, QVariant> m_settings;
	};

	/// Light mode, the sheet in the system color: as the program starts.
	void lightSystemSheet()
	{
		QApplication::setPalette(QET::Palette::fusionLight());
		PaletteGraphicsView::setCustomBackgroundColor(false);
		Diagram::background_color = Qt::white;
	}

	/**
		Chooses @a name in the sheet color picker of the toolbar, the way a
		click on that line of its menu does. The picker is built without a
		diagram editor: it then sets the color every folio is drawn with
		and stores it, and has no window to repaint.
		@return false when its menu has no line of that name
	*/
	bool pick(const std::string &name)
	{
		DiagramBgColorToolButton picker(nullptr);
		const QString text = QCoreApplication::translate(
					"DiagramBgColorToolButton", name.c_str());
		const QList<QAction *> actions = picker.menu()->actions();
		for (QAction *action : actions)
		{
			if (action->text() == text)
			{
				action->trigger();
				return true;
			}
		}
		return false;
	}

	/**
		Sizes @a view and lays it out without showing it.

		Not shown on purpose: a folio view takes its window for the diagram
		editor when it is first shown (DiagramView::event() zooms to fit,
		and that asks the editor whether to draw the grid), and this suite
		builds no editor. grab() sends a hidden widget the resize events it
		is owed, which gives the viewport its size; centerOn() needs it.
	*/
	void layOut(DiagramView &view)
	{
		view.setFrameShape(QFrame::NoFrame);
		view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		view.resize(view_size);
		const QPixmap laid_out = view.grab();
		Q_UNUSED(laid_out)
	}

	/// What the screen gets from a view, and where the scene went in it.
	struct Shot
	{
		QImage image;
		QTransform scene_to_image;

		/// Where a rectangle of the scene lands in the image, cut to it.
		QRect map(const QRectF &scene_rect) const
		{
			return scene_to_image.mapRect(scene_rect).toAlignedRect()
					.intersected(image.rect());
		}

		/// The color of the pixel under a point of the scene, as #rrggbb.
		QString colourAt(const QPointF &scene_point) const
		{
			const QPoint pixel = scene_to_image.map(scene_point).toPoint();
			if (!image.rect().contains(pixel))
			{
				return QStringLiteral("(outside the image)");
			}
			return image.pixelColor(pixel).name();
		}
	};

	/**
		The viewport of @a view at @a zoom, with @a center in its middle,
		painted the way the screen gets it: through the view's own paint
		event, inversion included. The children of the viewport (the
		context toolbar, the gesture overlay) are left out.
	*/
	Shot shoot(DiagramView &view, const QPointF &center, qreal zoom)
	{
		view.setTransform(QTransform::fromScale(zoom, zoom));
		view.centerOn(center);

		Shot shot;
		shot.scene_to_image = view.viewportTransform();
		QImage image(view.viewport()->size(), QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::transparent);
		view.viewport()->render(&image, QPoint(), QRegion(),
					QWidget::DrawWindowBackground);
		shot.image = image.convertToFormat(QImage::Format_RGB32);
		return shot;
	}

	/// The color found most often in @a area: the sheet.
	QColor mostFrequent(const QImage &image, const QRect &area)
	{
		QHash<QRgb, int> counts;
		const QRect inside = area.intersected(image.rect());
		for (int y = inside.top() ; y <= inside.bottom() ; ++y)
		{
			for (int x = inside.left() ; x <= inside.right() ; ++x)
			{
				++counts[image.pixel(x, y)];
			}
		}
		QRgb best = 0;
		int best_count = -1;
		for (auto it = counts.cbegin() ; it != counts.cend() ; ++it)
		{
			if (it.value() > best_count)
			{
				best_count = it.value();
				best = it.key();
			}
		}
		return QColor(best);
	}

	QColor mostFrequent(const QImage &image)
	{
		return mostFrequent(image, image.rect());
	}

	/// How many pixels of @a area are exactly @a color.
	int countOf(const QImage &image, const QRect &area, const QColor &color)
	{
		const QRgb wanted = color.rgb();
		const QRect inside = area.intersected(image.rect());
		int count = 0;
		for (int y = inside.top() ; y <= inside.bottom() ; ++y)
		{
			for (int x = inside.left() ; x <= inside.right() ; ++x)
			{
				if (image.pixel(x, y) == wanted)
				{
					++count;
				}
			}
		}
		return count;
	}

	/// The pixel of @a area that contrasts most with @a sheet: the ink.
	QColor strongestInk(const QImage &image, const QRect &area, const QColor &sheet)
	{
		QColor strongest = sheet;
		double best = 1.;
		const QRect inside = area.intersected(image.rect());
		for (int y = inside.top() ; y <= inside.bottom() ; ++y)
		{
			for (int x = inside.left() ; x <= inside.right() ; ++x)
			{
				const QColor color(image.pixel(x, y));
				const double contrast = QET::Palette::contrastRatio(color, sheet);
				if (contrast > best)
				{
					best = contrast;
					strongest = color;
				}
			}
		}
		return strongest;
	}

	/**
		What the view shows for @a drawn on a sheet inverted onto @a sheet
		with @a ink: the view's own mapping, applied to one pixel.
	*/
	QColor shownInverted(const QColor &drawn, const QColor &sheet, const QColor &ink)
	{
		QImage pixel(1, 1, QImage::Format_RGB32);
		pixel.fill(drawn);
		QET::Palette::invertLightness(pixel, sheet, ink);
		return pixel.pixelColor(0, 0);
	}

	/// The pixels where two shots of the same place differ.
	QList<QPoint> differences(const QImage &a, const QImage &b)
	{
		QList<QPoint> points;
		if (a.size() != b.size())
		{
			return points;
		}
		for (int y = 0 ; y < a.height() ; ++y)
		{
			for (int x = 0 ; x < a.width() ; ++x)
			{
				if (a.pixel(x, y) != b.pixel(x, y))
				{
					points << QPoint(x, y);
				}
			}
		}
		return points;
	}

	/// How many pixels of @a image are darker than mid gray: ink, on paper.
	int darkPixels(const QImage &image)
	{
		int count = 0;
		for (int y = 0 ; y < image.height() ; ++y)
		{
			for (int x = 0 ; x < image.width() ; ++x)
			{
				if (qGray(image.pixel(x, y)) < 128)
				{
					++count;
				}
			}
		}
		return count;
	}

	/// How many of @a points are exactly @a color in @a image.
	int countAt(const QImage &image, const QList<QPoint> &points, const QColor &color)
	{
		const QRgb wanted = color.rgb();
		int count = 0;
		for (const QPoint &point : points)
		{
			if (image.pixel(point) == wanted)
			{
				++count;
			}
		}
		return count;
	}

	/**
		The pixels of @a wire its stroke covers whole: those that are pure
		black in @a reference, a shot of the same place, through the same
		pixels, with the wire black on the white sheet of the light palette.

		The others are the rim of its junction dots, which no shot draws in
		the wire's color: Conductor::paint() draws the wire itself without
		antialiasing and turns it on for the dots, so the edge of a dot is a
		blend of the wire and the sheet, on any sheet and either palette.
		Measured on the wire of the example at stroke_zoom: 1360 pixels
		differ between the black and the red wire in a dark shot and 1376 in
		a light one, and 1280 of them are pure black in the reference - the
		same 1280 that come out exactly the expected color in every shot.
	*/
	QList<QPoint> coveredWhole(const QList<QPoint> &wire, const QImage &reference)
	{
		const QRgb black = QColor(Qt::black).rgb();
		QList<QPoint> covered;
		for (const QPoint &point : wire)
		{
			if (reference.pixel(point) == black)
			{
				covered << point;
			}
		}
		return covered;
	}

	/**
		How many pixels of @a wire coveredWhole() leaves out for another
		reason than being on the edge of a junction dot of @a conductor:
		what would go unchecked without being the rim, and it has to be
		none.

		A dot is 3 units across and drawn with the wire's pen, 1 unit wide
		(Conductor::paint()), so its edge is 2 units from its middle: 8 px
		at stroke_zoom. A pixel is on it when its middle is within one
		pixel of it; measured, those of the example lie 7.4 to 8.5 px from
		the middle of a dot.
	*/
	int leftOutOffTheDots(const QList<QPoint> &wire, const QImage &reference,
			      const Conductor *conductor, const QTransform &scene_to_image)
	{
		REQUIRE(conductor->properties().cond_size == Approx(1.));
		const qreal dot_edge = 2. * stroke_zoom;
		QList<QPointF> dots;
		const QList<QPointF> junctions = conductor->junctions();
		for (const QPointF &junction : junctions)
		{
			dots << scene_to_image.map(conductor->mapToScene(junction));
		}

		const QRgb black = QColor(Qt::black).rgb();
		int count = 0;
		for (const QPoint &point : wire)
		{
			if (reference.pixel(point) == black)
			{
				continue;
			}
			const QPointF middle = QPointF(point) + QPointF(0.5, 0.5);
			bool on_a_dot = false;
			for (const QPointF &dot : dots)
			{
				if (qAbs(QLineF(middle, dot).length() - dot_edge) <= 1.)
				{
					on_a_dot = true;
				}
			}
			if (!on_a_dot)
			{
				++count;
			}
		}
		return count;
	}

	/// A picture of one single color, so that its middle is that color.
	QPixmap plainPicture()
	{
		QImage picture(40, 40, QImage::Format_ARGB32);
		picture.fill(picture_color);
		return QPixmap::fromImage(picture);
	}

	/// Wire @a wire in @a color, the way its properties dialog does.
	void paintWire(Conductor *wire, const QColor &color)
	{
		ConductorProperties properties = wire->properties();
		properties.color = color;
		wire->setProperties(properties);
	}

	/**
		The folio of the example, and what the cases look at on it. The
		grid is turned off: its dots are strokes too, and would count as
		ink where the case looks for the symbol's.
	*/
	struct Folio
	{
		Diagram *sheet = nullptr;
		/// The largest symbol a shot at stroke_zoom takes whole.
		Element *symbol = nullptr;
		/// A wire a shot at stroke_zoom takes whole, black in the file.
		Conductor *wire = nullptr;
		/// A picture pasted as it is: keeps its colors on a dark sheet.
		DiagramImageItem *photo = nullptr;
		/// The same picture, set to adapt to the dark theme.
		DiagramImageItem *adapted = nullptr;
	};

	Folio dress(Diagram *sheet)
	{
		Folio folio;
		folio.sheet = sheet;
		if (!sheet)
		{
			return folio;
		}
		sheet->setDisplayGrid(false);

		const QSizeF room = QSizeF(view_size) / stroke_zoom * 0.9;
		auto fits = [&room](const QRectF &rect) {
			return rect.width() <= room.width() && rect.height() <= room.height();
		};

		qreal best_area = 0.;
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			const QRectF rect = element->sceneBoundingRect();
			const qreal area = rect.width() * rect.height();
			if (fits(rect) && area > best_area)
			{
				best_area = area;
				folio.symbol = element;
			}
		}

		best_area = 0.;
		const QList<Conductor *> wires = sheet->conductors();
		for (Conductor *wire : wires)
		{
			const QRectF rect = wire->sceneBoundingRect();
			const qreal area = rect.width() * rect.height();
			if (wire->properties().color == QColor(Qt::black)
			    && fits(rect) && area > best_area)
			{
				best_area = area;
				folio.wire = wire;
			}
		}

		// Side by side, above everything, inside the drawing area.
		const QPointF corner = UiBench::drawingRect(sheet).topLeft() + QPointF(20., 20.);
		folio.photo = new DiagramImageItem(plainPicture());
		folio.photo->setPos(corner);
		folio.photo->setZValue(1000.);
		sheet->addItem(folio.photo);
		folio.adapted = new DiagramImageItem(plainPicture());
		folio.adapted->setAdaptToDarkTheme(true);
		folio.adapted->setPos(corner + QPointF(60., 0.));
		folio.adapted->setZValue(1000.);
		sheet->addItem(folio.adapted);
		return folio;
	}

	/// The middle of the two pictures, where one shot takes both.
	QPointF picturesCenter(const Folio &folio)
	{
		return folio.photo->sceneBoundingRect()
				.united(folio.adapted->sceneBoundingRect()).center();
	}

	/**
		Puts back every print setting the way it was, whatever the case did:
		the print window writes a dozen keys under print/ and printers/ each
		time it prints. The same guard as monochrome_test.cpp.
	*/
	class KeepPrintSettings
	{
		public:
			KeepPrintSettings()
			{
				QSettings settings;
				const QStringList keys = settings.allKeys();
				for (const QString &key : keys)
				{
					if (isPrintKey(key))
					{
						m_values.insert(key, settings.value(key));
					}
				}
			}
			~KeepPrintSettings()
			{
				QSettings settings;
				const QStringList keys = settings.allKeys();
				for (const QString &key : keys)
				{
					if (isPrintKey(key) && !m_values.contains(key))
					{
						settings.remove(key);
					}
				}
				for (auto it = m_values.cbegin() ; it != m_values.cend() ; ++it)
				{
					settings.setValue(it.key(), it.value());
				}
			}

			KeepPrintSettings(const KeepPrintSettings &) = delete;
			KeepPrintSettings &operator=(const KeepPrintSettings &) = delete;

		private:
			static bool isPrintKey(const QString &key)
			{
				return key.startsWith(QLatin1String("print/"))
					|| key.startsWith(QLatin1String("printers/"));
			}

			QMap<QString, QVariant> m_values;
	};

	/**
		@a project exported to @a path with the PDF button of the print
		window, the way the program exports it.
		@return whether the file was written
	*/
	bool printToPdf(QETProject *project, const QString &path)
	{
		auto *printer = new QPrinter();
		printer->setOutputFormat(QPrinter::PdfFormat);
		printer->setOutputFileName(path);
		{
			ProjectPrintWindow window(project, printer);
			QMetaObject::invokeMethod(&window, "print");
			// The PDF is post-processed on the next turn of the event loop.
			QCoreApplication::processEvents();
		}
		return QFile::exists(path);
	}

#ifdef QET_HAS_QTPDF
	/// The first page of the PDF at @a path, at 144 dpi, over white.
	QImage rasterise(const QString &path)
	{
		QPdfDocument document;
		document.load(path);
		if (document.status() != QPdfDocument::Status::Ready
		    || document.pageCount() < 1)
		{
			return QImage();
		}
		const QSize size = (document.pagePointSize(0) * 2.).toSize();
		const QImage page = document.render(0, size);
		if (page.isNull())
		{
			return QImage();
		}
		QImage over_white(page.size(), QImage::Format_RGB32);
		over_white.fill(Qt::white);
		QPainter painter(&over_white);
		painter.drawImage(0, 0, page);
		painter.end();
		return over_white;
	}
#endif
}

TEST_CASE("CU-63.1 — fundo escuro escolhido no seletor: o traço preto clareia, o fio colorido e a foto ficam",
	  "[uibench][T63]")
{
	const std::string choice = GENERATE(as<std::string>{}, "Dark gray", "Black");
	CAPTURE(choice);

	const KeepSheetState keep;
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	const Folio folio = dress(bench.diagram(0));
	REQUIRE(folio.sheet != nullptr);
	REQUIRE(folio.symbol != nullptr);
	REQUIRE(folio.wire != nullptr);

	DiagramView view(folio.sheet);
	layOut(view);
	REQUIRE(view.viewport()->size() == view_size);

	const QRectF symbol_rect = folio.symbol->sceneBoundingRect();
	const QRectF wire_rect = folio.wire->sceneBoundingRect();

	// The fixture is what it says it is: in light mode, on the system
	// sheet, the symbol and the wire are drawn in pure black. Without
	// this, a symbol drawn in gray would pass every check below.
	lightSystemSheet();
	{
		const Shot light = shoot(view, symbol_rect.center(), stroke_zoom);
		REQUIRE(countOf(light.image, light.map(symbol_rect), Qt::black) > 0);
	}
	// Kept: it is also what tells the wire section which pixels the
	// stroke covers whole (coveredWhole()).
	const Shot light_wire = shoot(view, wire_rect.center(), stroke_zoom);
	REQUIRE(countOf(light_wire.image, light_wire.map(wire_rect), Qt::black) > 0);

	QApplication::setPalette(QET::Palette::fusionDark());
	REQUIRE(pick(choice));
	REQUIRE(PaletteGraphicsView::customBackgroundColor());
	const QColor sheet_color = Diagram::background_color;
	const QColor ink = QApplication::palette().color(QPalette::Text);
	CAPTURE(sheet_color.name(), ink.name());
	REQUIRE(sheet_color.lightness() < 128);
	REQUIRE(view.invertsScene());

	SECTION("a folha é a cor escolhida, e o traço preto do símbolo sai claro")
	{
		const Shot shot = shoot(view, symbol_rect.center(), stroke_zoom);
		const QRect symbol = shot.map(symbol_rect);
		REQUIRE_FALSE(symbol.isEmpty());

		CHECK(mostFrequent(shot.image).name() == sheet_color.name());
		// Black became the palette's text color, pixel for pixel...
		CHECK(shownInverted(Qt::black, sheet_color, ink).name() == ink.name());
		CHECK(countOf(shot.image, symbol, ink) > 0);
		// ...it reads on the chosen sheet, lighter than it...
		const QColor strongest = strongestInk(shot.image, symbol, sheet_color);
		INFO("strongest ink " << strongest.name().toStdString() << ", "
		     << QET::Palette::contrastRatio(strongest, sheet_color) << ":1");
		CHECK(strongest.lightness() > sheet_color.lightness());
		CHECK(QET::Palette::contrastRatio(strongest, sheet_color) >= 4.5);
		// ...and none of it stayed black on a sheet that is not.
		if (sheet_color != QColor(Qt::black))
		{
			CHECK(countOf(shot.image, symbol, Qt::black) == 0);
		}
	}

	SECTION("o fio preto sai claro, e o fio vermelho continua vermelho")
	{
		// The same place twice, the wire black and then red: the pixels
		// that differ are the wire, wherever nothing else covers it.
		const Shot black = shoot(view, wire_rect.center(), stroke_zoom);
		paintWire(folio.wire, Qt::red);
		const Shot red = shoot(view, wire_rect.center(), stroke_zoom);
		const QList<QPoint> wire = differences(black.image, red.image);
		INFO("wire pixels: " << wire.size());
		REQUIRE(wire.size() > 0);

		// Of those, the ones the stroke covers whole, told by the light
		// shot of the fixture through the same pixels; what is left out is
		// the antialiased rim of the junction dots, and nothing else.
		REQUIRE(black.scene_to_image == light_wire.scene_to_image);
		const QList<QPoint> stroke = coveredWhole(wire, light_wire.image);
		INFO("covered whole: " << stroke.size());
		REQUIRE(stroke.size() > 0);
		CHECK(leftOutOffTheDots(wire, light_wire.image, folio.wire, black.scene_to_image) == 0);

		// Black, it was the light ink, at text contrast.
		CHECK(countAt(black.image, stroke, ink) == stroke.size());
		CHECK(QET::Palette::contrastRatio(ink, sheet_color) >= 4.5);

		// Red, it is red still: lighter, the same hue.
		const QColor red_shown = shownInverted(Qt::red, sheet_color, ink);
		INFO("red shows as " << red_shown.name().toStdString());
		CHECK(countAt(red.image, stroke, red_shown) == stroke.size());
		CHECK(red_shown.hslHue() == 0);
		CHECK(red_shown.hslSaturationF() > 0.5);
		CHECK(red_shown.red() > red_shown.green() + 100);
	}

	SECTION("a foto colada não muda, e a figura que se adapta ao tema segue a folha")
	{
		const Shot shot = shoot(view, picturesCenter(folio), stroke_zoom);
		CHECK(shot.colourAt(folio.photo->sceneBoundingRect().center())
		      == picture_color.name());
		// Inverted with the folio, onto the chosen color and not the
		// palette's own sheet.
		CHECK(shot.colourAt(folio.adapted->sceneBoundingRect().center())
		      == shownInverted(picture_color, sheet_color, ink).name());
	}

	SECTION("a moldura e o carimbo saem claros sobre a folha escolhida")
	{
		// Drawn white on a dark sheet, as they are for an export, they
		// would come out dark here, on a sheet inverted into a light one.
		const QRectF title_block = UiBench::titleBlockRect(folio.sheet);
		REQUIRE_FALSE(title_block.isEmpty());
		const Shot shot = shoot(view, title_block.center(), 1.);
		const QRect area = shot.map(title_block);
		REQUIRE_FALSE(area.isEmpty());
		CHECK(mostFrequent(shot.image, area).name() == sheet_color.name());
		CHECK(countOf(shot.image, area, ink) > 0);
	}

	SECTION("a grade acompanha, mais fraca que o traço")
	{
		const Shot without = shoot(view, symbol_rect.center(), stroke_zoom);
		folio.sheet->setDisplayGrid(true);
		const Shot with = shoot(view, symbol_rect.center(), stroke_zoom);
		folio.sheet->setDisplayGrid(false);

		// A dot is drawn as on a white sheet shown inverted: the soft gray
		// QET::Palette::gridDotColor() gives for that, then inverted.
		const QColor dot = shownInverted(QET::Palette::gridDotColor(Qt::white, true),
						 sheet_color, ink);
		INFO("a dot shows as " << dot.name().toStdString());
		CHECK(countOf(with.image, with.image.rect(), dot)
		      > countOf(without.image, without.image.rect(), dot));
		CHECK(QET::Palette::contrastRatio(dot, sheet_color)
		      < QET::Palette::contrastRatio(ink, sheet_color));
	}

	// The view had the scene drawn on white while it painted, and gave the
	// chosen color back each time: what the rest of the program reads is
	// still the user's choice.
	CHECK(Diagram::background_color.name() == sheet_color.name());
	CHECK(PaletteGraphicsView::customBackgroundColor());
}

TEST_CASE("CU-63.2 — fundo Cinza ou Branco escolhido no seletor: o traço continua preto",
	  "[uibench][T63]")
{
	const std::string choice = GENERATE(as<std::string>{}, "Gray", "White");
	CAPTURE(choice);

	const KeepSheetState keep;
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	const Folio folio = dress(bench.diagram(0));
	REQUIRE(folio.sheet != nullptr);
	REQUIRE(folio.symbol != nullptr);
	REQUIRE(folio.wire != nullptr);

	DiagramView view(folio.sheet);
	layOut(view);
	REQUIRE(view.viewport()->size() == view_size);

	const QRectF symbol_rect = folio.symbol->sceneBoundingRect();
	const QRectF wire_rect = folio.wire->sceneBoundingRect();

	// The wire in light mode, on the system sheet, drawn in pure black:
	// what tells the wire section which pixels the stroke covers whole
	// (coveredWhole()).
	lightSystemSheet();
	const Shot light_wire = shoot(view, wire_rect.center(), stroke_zoom);
	REQUIRE(countOf(light_wire.image, light_wire.map(wire_rect), Qt::black) > 0);

	QApplication::setPalette(QET::Palette::fusionDark());
	REQUIRE(pick(choice));
	REQUIRE(PaletteGraphicsView::customBackgroundColor());
	const QColor sheet_color = Diagram::background_color;
	CAPTURE(sheet_color.name());
	REQUIRE(sheet_color.lightness() >= 128);
	CHECK_FALSE(view.invertsScene());

	SECTION("a folha é a cor escolhida, e o traço do símbolo é preto puro")
	{
		const Shot shot = shoot(view, symbol_rect.center(), stroke_zoom);
		const QRect symbol = shot.map(symbol_rect);
		REQUIRE_FALSE(symbol.isEmpty());

		CHECK(mostFrequent(shot.image).name() == sheet_color.name());
		CHECK(countOf(shot.image, symbol, Qt::black) > 0);
		const QColor strongest = strongestInk(shot.image, symbol, sheet_color);
		CHECK(strongest.name() == QColor(Qt::black).name());
		// The reason black stays on mid gray: 5.3:1, where white gives 3.9.
		const double black_contrast = QET::Palette::contrastRatio(Qt::black, sheet_color);
		INFO("black reads " << black_contrast << ":1 on the sheet");
		CHECK(black_contrast >= 5.3);
		CHECK(black_contrast > QET::Palette::contrastRatio(Qt::white, sheet_color));
	}

	SECTION("o fio preto continua preto, e o vermelho, vermelho")
	{
		const Shot black = shoot(view, wire_rect.center(), stroke_zoom);
		paintWire(folio.wire, Qt::red);
		const Shot red = shoot(view, wire_rect.center(), stroke_zoom);
		const QList<QPoint> wire = differences(black.image, red.image);
		INFO("wire pixels: " << wire.size());
		REQUIRE(wire.size() > 0);

		// Where the stroke covers the pixel whole; the rest is the rim of
		// the junction dots, a blend with the sheet on any sheet.
		REQUIRE(black.scene_to_image == light_wire.scene_to_image);
		const QList<QPoint> stroke = coveredWhole(wire, light_wire.image);
		INFO("covered whole: " << stroke.size());
		REQUIRE(stroke.size() > 0);
		CHECK(leftOutOffTheDots(wire, light_wire.image, folio.wire, black.scene_to_image) == 0);

		CHECK(countAt(black.image, stroke, Qt::black) == stroke.size());
		CHECK(countAt(red.image, stroke, Qt::red) == stroke.size());
	}

	SECTION("a foto colada não muda")
	{
		const Shot shot = shoot(view, picturesCenter(folio), stroke_zoom);
		CHECK(shot.colourAt(folio.photo->sceneBoundingRect().center())
		      == picture_color.name());
	}

	CHECK(Diagram::background_color.name() == sheet_color.name());
}

TEST_CASE("T63 — na folha escolhida, vence o traço que contrasta mais: o claro ou o preto",
	  "[uibench][T63]")
{
	const KeepSheetState keep;
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	const Folio folio = dress(bench.diagram(0));
	REQUIRE(folio.sheet != nullptr);
	REQUIRE(folio.symbol != nullptr);

	DiagramView view(folio.sheet);
	layOut(view);
	REQUIRE(view.viewport()->size() == view_size);

	SECTION("onde a régua corta")
	{
		struct Case { QColor sheet; bool dark_palette; bool custom; bool inverts; };
		const QList<Case> cases {
			// On a dark palette, the system sheet inverts, as before...
			{QColor(Qt::white), true, false, true},
			// ...a chosen one when the light ink beats black on it: dark
			// grays and dark blue do; mid gray, white and the saturated
			// colors whose lightness is 127 do not (light ink on yellow
			// would give 1.3:1, black gives 19.6:1)...
			{QColor(40, 40, 40), true, true, true},
			{QColor(100, 100, 100), true, true, true},
			{QColor(0, 0, 160), true, true, true},
			{QColor(127, 127, 127), true, true, false},
			{QColor(128, 128, 128), true, true, false},
			{QColor(Qt::yellow), true, true, false},
			{QColor(Qt::red), true, true, false},
			{QColor(Qt::white), true, true, false},
			// ...and on a light palette nothing ever does.
			{QColor(Qt::white), false, false, false},
			{QColor(Qt::black), false, true, false}};
		for (const Case &c : cases)
		{
			QApplication::setPalette(c.dark_palette ? QET::Palette::fusionDark()
								: QET::Palette::fusionLight());
			PaletteGraphicsView::setCustomBackgroundColor(c.custom);
			Diagram::background_color = c.sheet;
			INFO(c.sheet.name().toStdString()
			     << (c.dark_palette ? " on the dark palette" : " on the light palette")
			     << (c.custom ? ", chosen" : ", system"));
			CHECK(view.invertsScene() == c.inverts);
		}
	}

	SECTION("uma cor escura qualquer, (40, 40, 40), como a gravada nas preferências")
	{
		// What "Other color..." and the next start (main.cpp) both do: the
		// two statics, no picker.
		QApplication::setPalette(QET::Palette::fusionDark());
		PaletteGraphicsView::setCustomBackgroundColor(true);
		const QColor sheet_color(40, 40, 40);
		Diagram::background_color = sheet_color;
		const QColor ink = QApplication::palette().color(QPalette::Text);

		const QRectF symbol_rect = folio.symbol->sceneBoundingRect();
		const Shot shot = shoot(view, symbol_rect.center(), stroke_zoom);
		const QRect symbol = shot.map(symbol_rect);
		REQUIRE_FALSE(symbol.isEmpty());
		CHECK(mostFrequent(shot.image).name() == sheet_color.name());
		CHECK(countOf(shot.image, symbol, ink) > 0);
		CHECK(countOf(shot.image, symbol, Qt::black) == 0);
		const QColor strongest = strongestInk(shot.image, symbol, sheet_color);
		INFO("strongest ink " << strongest.name().toStdString() << ", "
		     << QET::Palette::contrastRatio(strongest, sheet_color) << ":1");
		CHECK(QET::Palette::contrastRatio(strongest, sheet_color) >= 4.5);
		CHECK(Diagram::background_color.name() == sheet_color.name());
	}

	SECTION("na cor do sistema, a folha escura continua como era")
	{
		QApplication::setPalette(QET::Palette::fusionDark());
		PaletteGraphicsView::setCustomBackgroundColor(false);
		Diagram::background_color = Qt::white;
		const QPalette palette = QApplication::palette();

		const QRectF symbol_rect = folio.symbol->sceneBoundingRect();
		const Shot shot = shoot(view, symbol_rect.center(), stroke_zoom);
		CHECK(mostFrequent(shot.image).name() == palette.color(QPalette::Base).name());
		CHECK(countOf(shot.image, shot.map(symbol_rect), palette.color(QPalette::Text)) > 0);
		CHECK(Diagram::background_color.name() == QColor(Qt::white).name());
	}
}

TEST_CASE("T63 — o PDF da janela de impressão não muda com a folha escura escolhida na tela",
	  "[uibench][T63][impressao]")
{
	/*
		The half of CU-63.3 a suite can reach: the PDF. Printing on paper
		goes through the same window and the same drawing
		(ProjectPrintWindow::requestPaint()), into a printer no suite has.
	*/
	const KeepSheetState keep;
	const KeepPrintSettings keep_print;

	UiBench::ScratchProject scratch(UiBench::fileContent(UiBench::examplePath(example)),
					example);
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);

	QTemporaryDir work;
	REQUIRE(work.isValid());

	// One export thrown away first: the window saves its options when it
	// prints, and the two exports compared below then both open on them.
	lightSystemSheet();
	REQUIRE(printToPdf(scratch.project(), work.filePath(QStringLiteral("warm-up.pdf"))));

	// The reference: light mode, the sheet in the system color.
	const QString light_path = work.filePath(QStringLiteral("light.pdf"));
	REQUIRE(printToPdf(scratch.project(), light_path));

	// Dark mode, black chosen in the picker, and the folio painted on the
	// screen first, so that whatever the view does while it inverts has
	// been done before the export.
	QApplication::setPalette(QET::Palette::fusionDark());
	REQUIRE(pick("Black"));
	REQUIRE(Diagram::background_color == QColor(Qt::black));
	{
		DiagramView view(sheet);
		layOut(view);
		REQUIRE(view.invertsScene());
		const Shot shown = shoot(view, UiBench::drawingRect(sheet).center(), 1.);
		REQUIRE(mostFrequent(shown.image).name() == QColor(Qt::black).name());
	}
	const QString dark_path = work.filePath(QStringLiteral("dark.pdf"));
	REQUIRE(printToPdf(scratch.project(), dark_path));

	// The window gave the chosen color back when it closed.
	CHECK(Diagram::background_color.name() == QColor(Qt::black).name());
	CHECK(PaletteGraphicsView::customBackgroundColor());

#ifdef QET_HAS_QTPDF
	const QImage light = rasterise(light_path);
	const QImage dark = rasterise(dark_path);
	REQUIRE_FALSE(light.isNull());
	REQUIRE_FALSE(dark.isNull());
	REQUIRE(dark.size() == light.size());

	// White paper, and the same page as in light mode, pixel for pixel.
	CHECK(mostFrequent(dark).name() == QColor(Qt::white).name());
	const QList<QPoint> changed = differences(light, dark);
	INFO("pixels that differ from the light mode PDF: " << changed.size());
	CHECK(changed.isEmpty());
	// And there is ink on it: two blank pages would be equal too.
	CHECK(darkPixels(dark) > 0);
#else
	SUCCEED("built without QtPdf: no library here reads the page of a PDF back as pixels");
#endif
}
