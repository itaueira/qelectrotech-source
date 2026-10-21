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
#include "pdfprobe.h"
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/cli_export.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/exportproperties.h"
#include "../../../../sources/print/projectprintwindow.h"
#include "../../../../sources/qetgraphicsitem/diagramimageitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QMap>
#include <QMarginsF>
#include <QMetaObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QPixmap>
#include <QPrinter>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariant>

#ifdef QET_HAS_QTPDF
#include <QPdfDocument>
#endif

/*
	Black and white output (T37, step 3; ExportProperties::monochrome).

	The rule being proved is the one decided for it: a stroke - line,
	outline, text - prints pure black; a fill and a raster picture print in
	the gray of their luminance; pure white stays white. And the switch is
	one flag of the folio, set by applyProperties() like the other export
	options, which every way out of the program reaches through
	Diagram::render().

	Proved by pixel, never by glyph. The text of a sheet is drawn with
	whatever font the machine has, and some Qt builds of this project
	rasterise no glyph at all without a screen (see CLAUDE.md and
	titleblockeditor_test.cpp). What does not depend on the font is:
	  - the colour at the middle of a thick stroke, of a filled area and of
	    a picture, which are drawn from coordinates this file chooses;
	  - whether any pixel of the page has a hue at all. Text in black has
	    none, whether a glyph was drawn or not, so "no pixel has a hue"
	    holds on every machine - and it is the assertion that covers what
	    this file did not draw itself: the title block, the border, the
	    symbol taken from the project.

	A pixel "has a hue" when its three channels are not equal. The raster
	of this program's own painter is compared exactly; a page rasterised
	back from a PDF by another library is given a tolerance of two levels,
	said where it is used.
*/

namespace
{
	/// The word written on the sheet, looked for in the emitted PDF.
	const char *anchor = "MONO-ANCHOR-GAMMA";

	// The colours of the fixture. Each one is far from gray, so that a
	// render that forgot to convert it cannot pass by accident.
	const QColor stroke_color(255, 0, 0);
	const QColor fill_color(40, 160, 90);
	const QColor picture_color(200, 60, 30);

	/// Pixels across the page: about 1.7 per unit of the sheet, so that the
	/// middle of a line 8 units thick is a pixel the line covers whole.
	const int render_width = 1800;

	/// One sheet with a symbol drawn in red and a free text.
	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"mark.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"50\" height=\"50\""
			       " hotspot_x=\"25\" hotspot_y=\"25\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Mark</name></names>"
			       "<description>"
			       "<rect x=\"-20\" y=\"-20\" width=\"40\" height=\"40\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:hight;"
			       "filling:none;color:red\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>"
			       "<element x=\"150\" y=\"150\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/mark.elmt\""
			       " uuid=\"{0b1ac000-0000-4000-8000-000000000001}\">"
			       "<terminals/><inputs/><elementInformations/>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>"
			       "</elements>"
			       "<inputs>"
			       "<input x=\"150\" y=\"300\" text=\"%1\" rotation=\"0\"/>"
			       "</inputs>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(QLatin1String(anchor));
	}

	/// What the case adds to the sheet, and where to look for each.
	struct Dressing
	{
		/// The middle of a red line, 8 units thick.
		QPointF stroke;
		/// The inside of a green rectangle without outline.
		QPointF fill;
		/// The middle of a white line drawn across a black rectangle.
		QPointF white_stroke;
		/// The inside of that black rectangle, away from the white line.
		QPointF black_fill;
		/// The middle of a picture of one single colour.
		QPointF picture;
		/// A place of the sheet where nothing is drawn.
		QPointF paper;
		/// The symbol of the project, drawn in red.
		QRectF symbol;
	};

	/**
		Adds to @a sheet a line, two filled rectangles, a white line and a
		picture, at coordinates chosen here, so that their middle can be
		looked at in a render whatever font the machine has.
	*/
	Dressing dress(Diagram *sheet)
	{
		Dressing dressing;

		auto *line = new QetShapeItem(QPointF(400, 150), QPointF(600, 150),
									  QetShapeItem::Line);
		line->setPen(QPen(stroke_color, 8));
		sheet->addItem(line);
		dressing.stroke = QPointF(500, 150);

		auto *green = new QetShapeItem(QPointF(400, 250), QPointF(500, 330),
									   QetShapeItem::Rectangle);
		green->setPen(Qt::NoPen);
		green->setBrush(QBrush(fill_color));
		sheet->addItem(green);
		dressing.fill = QPointF(450, 290);

		auto *black = new QetShapeItem(QPointF(550, 250), QPointF(700, 330),
									   QetShapeItem::Rectangle);
		black->setPen(Qt::NoPen);
		black->setBrush(QBrush(Qt::black));
		black->setZValue(1);
		sheet->addItem(black);
		dressing.black_fill = QPointF(625, 265);

		auto *white = new QetShapeItem(QPointF(550, 290), QPointF(700, 290),
									   QetShapeItem::Line);
		white->setPen(QPen(QColor(Qt::white), 8));
		white->setZValue(2);
		sheet->addItem(white);
		dressing.white_stroke = QPointF(625, 290);

		QImage picture(40, 40, QImage::Format_ARGB32);
		picture.fill(picture_color);
		auto *image_item = new DiagramImageItem(QPixmap::fromImage(picture));
		image_item->setPos(800, 150);
		sheet->addItem(image_item);
		dressing.picture = image_item->sceneBoundingRect().center();

		dressing.paper = QPointF(300, 500);

		const QList<Element *> elements = sheet->elements();
		if (!elements.isEmpty())
		{
			dressing.symbol = elements.first()->sceneBoundingRect();
		}
		return dressing;
	}

	/// The colour of the pixel under @a scene_point, as #rrggbb.
	QString colourAt(const UiBench::Rendering &rendering, const QPointF &scene_point)
	{
		const QRect area = rendering.map(
					QRectF(scene_point - QPointF(0.5, 0.5), QSizeF(1., 1.)));
		if (area.isEmpty())
		{
			return QStringLiteral("(outside the image)");
		}
		return rendering.image().pixelColor(area.center()).name();
	}

	/// The gray qGray() gives @a colour, as #rrggbb.
	QString grayOf(const QColor &colour)
	{
		const int gray = qGray(colour.rgb());
		return QColor(gray, gray, gray).name();
	}

	/// Pixels of @a area whose three channels differ by more than @a tolerance.
	int pixelsWithHue(const QImage &image, const QRect &area, int tolerance = 0)
	{
		const QRect inside = area.intersected(image.rect());
		int count = 0;
		for (int y = inside.top() ; y <= inside.bottom() ; ++y)
		{
			for (int x = inside.left() ; x <= inside.right() ; ++x)
			{
				const QRgb pixel = image.pixel(x, y);
				const int r = qRed(pixel);
				const int g = qGreen(pixel);
				const int b = qBlue(pixel);
				const int spread = qMax(qAbs(r - g), qMax(qAbs(g - b), qAbs(r - b)));
				if (spread > tolerance)
				{
					++count;
				}
			}
		}
		return count;
	}

	int pixelsWithHue(const QImage &image, int tolerance = 0)
	{
		return pixelsWithHue(image, image.rect(), tolerance);
	}

	/// The export options of the case, with only the switch under test set.
	ExportProperties options(bool monochrome)
	{
		ExportProperties properties;
		properties.monochrome = monochrome;
		return properties;
	}

	/**
		@a sheet drawn into a PDF at @a path the way the command line draws
		it: a QPdfWriter at 96 dpi, one page the size of the sheet, and
		Diagram::render() with the page rectangle of the sheet as source.
	*/
	bool writePdf(Diagram *sheet, const QString &path)
	{
		const QRectF page = UiBench::pageRect(sheet);
		QPdfWriter writer(path);
		writer.setResolution(96);
		writer.setPageLayout(QPageLayout(
					     QPageSize(QSizeF(page.width() * 72. / 96.,
							      page.height() * 72. / 96.),
						       QPageSize::Point),
					     QPageLayout::Portrait,
					     QMarginsF(0, 0, 0, 0)));

		QPainter painter;
		if (!painter.begin(&writer))
		{
			return false;
		}
		sheet->render(&painter,
					  QRectF(0, 0, writer.width(), writer.height()),
					  page,
					  Qt::KeepAspectRatio);
		return painter.end();
	}

	/**
		@return whether this build writes searchable text into a PDF at all.
		Measured, as pdfexport_test.cpp does and for the same reason: some
		builds embed no font without a screen, and the searchability of an
		exported sheet would then measure the platform.
	*/
	bool putsTextInPdf()
	{
		QTemporaryDir dir;
		if (!dir.isValid())
		{
			return false;
		}
		const QString path = dir.filePath(QStringLiteral("calibre.pdf"));
		{
			QPdfWriter writer(path);
			writer.setResolution(96);
			writer.setPageSize(QPageSize(QPageSize::A4));
			QPainter painter(&writer);
			if (!painter.isActive())
			{
				return false;
			}
			painter.drawText(QRect(0, 0, 400, 60),
					 Qt::AlignLeft | Qt::AlignVCenter,
					 QStringLiteral("mmm"));
		}
		PdfProbe::Document reference(path);
		return reference.isValid() && reference.hasMappedFont(1);
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

	/// What another library may round a gray to when it rasterises a PDF.
	const int pdf_tolerance = 2;

	/// Pixels of @a image within @a tolerance of @a colour on every channel.
	int pixelsOf(const QImage &image, const QColor &colour, int tolerance)
	{
		int count = 0;
		for (int y = 0 ; y < image.height() ; ++y)
		{
			for (int x = 0 ; x < image.width() ; ++x)
			{
				const QRgb pixel = image.pixel(x, y);
				if (qAbs(qRed(pixel) - colour.red()) <= tolerance
				    && qAbs(qGreen(pixel) - colour.green()) <= tolerance
				    && qAbs(qBlue(pixel) - colour.blue()) <= tolerance)
				{
					++count;
				}
			}
		}
		return count;
	}
#endif

	/**
		Puts back every print setting the way it was, whatever the case did:
		the print window writes a dozen keys under print/ and printers/ each
		time it prints, and the next case to open it must not inherit them.
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

		private:
			static bool isPrintKey(const QString &key)
			{
				return key.startsWith(QLatin1String("print/"))
					|| key.startsWith(QLatin1String("printers/"));
			}

			QMap<QString, QVariant> m_values;
	};

	/// The black and white box of @a window, nullptr when it has none.
	QCheckBox *monochromeBox(ProjectPrintWindow &window)
	{
		return window.findChild<QCheckBox *>(QStringLiteral("m_monochrome_cb"));
	}

	/**
		Exports @a project to @a path with the PDF button of the print window,
		after ticking or clearing its black and white box when @a tick says so.
		@return the state of the box when the window opened, before @a tick
		touched it; false when the box is missing (the case checks that first).
	*/
	bool printWithWindow(QETProject *project, const QString &path,
						 int tick, bool *box_found)
	{
		auto *printer = new QPrinter();
		printer->setOutputFormat(QPrinter::PdfFormat);
		printer->setOutputFileName(path);

		ProjectPrintWindow window(project, printer);
		QCheckBox *box = monochromeBox(window);
		*box_found = box != nullptr;
		if (!box)
		{
			return false;
		}
		const bool on_opening = box->isChecked();
		if (tick >= 0)
		{
			box->setChecked(tick > 0);
		}
		QMetaObject::invokeMethod(&window, "print");
		// The PDF is post-processed on the next turn of the event loop.
		QCoreApplication::processEvents();
		return on_opening;
	}
}

TEST_CASE("T37 — preto e branco: traço sai preto puro, preenchimento e figura em cinza da luminância",
	  "[uibench][impressao]")
{
	UiBench::ScratchProject scratch(projectXml());
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().size() == 1);

	const Dressing dressing = dress(sheet);
	REQUIRE_FALSE(dressing.symbol.isEmpty());

	// The same options on both sides, the switch excepted: what differs
	// between the two images is the switch and nothing else.
	ExportProperties before = sheet->applyProperties(options(false));
	{
		// One render thrown away first: whatever an item lays out on its
		// first paint is then done before the image the last section
		// compares against is taken.
		const UiBench::Rendering warm_up(sheet, render_width);
		Q_UNUSED(warm_up)
	}
	const UiBench::Rendering colour(sheet, render_width);
	sheet->applyProperties(before);

	before = sheet->applyProperties(options(true));
	REQUIRE(sheet->monochrome());
	const UiBench::Rendering mono(sheet, render_width);
	sheet->applyProperties(before);

	REQUIRE_FALSE(colour.isNull());
	REQUIRE_FALSE(mono.isNull());

	SECTION("sem a opção, o desenho sai nas cores dele")
	{
		// The fixture is what it says it is: without this, a renderer that
		// drew everything black would pass every section below.
		CHECK(colourAt(colour, dressing.stroke) == stroke_color.name());
		CHECK(colourAt(colour, dressing.fill) == fill_color.name());
		CHECK(colourAt(colour, dressing.picture) == picture_color.name());
		CHECK(pixelsWithHue(colour.image(), colour.map(dressing.symbol)) > 0);
	}

	SECTION("o traço colorido sai preto puro")
	{
		CHECK(colourAt(mono, dressing.stroke) == QColor(Qt::black).name());
	}

	SECTION("o preenchimento e a figura saem no cinza da luminância deles")
	{
		// qGray(40, 160, 90) = 107, qGray(200, 60, 30) = 103: grays, not
		// black - a filled area keeps its shape instead of becoming a blot.
		CHECK(colourAt(mono, dressing.fill) == grayOf(fill_color));
		CHECK(colourAt(mono, dressing.picture) == grayOf(picture_color));
	}

	SECTION("o branco continua branco, e o preto continua preto")
	{
		// A white line cut through a black shape is the one stroke that must
		// not turn black: on paper it is no ink, and in black it would print
		// a line nobody sees on the screen.
		CHECK(colourAt(mono, dressing.white_stroke) == QColor(Qt::white).name());
		CHECK(colourAt(mono, dressing.black_fill) == QColor(Qt::black).name());
		CHECK(colourAt(mono, dressing.paper) == QColor(Qt::white).name());
	}

	SECTION("nenhum ponto da folha sai com cor, nem o que o caso não desenhou")
	{
		// The symbol, the border, the title block and the text come from
		// the project and not from this file: counting the pixels with a hue
		// over the whole page is what covers them, whatever the font.
		INFO("pixels with a hue, in colour: " << pixelsWithHue(colour.image()));
		CHECK(pixelsWithHue(colour.image()) > 0);
		CHECK(pixelsWithHue(mono.image()) == 0);

		// And the red symbol is still drawn, in black: converted, not lost.
		CHECK(mono.ink(dressing.symbol) > 0);
	}

	SECTION("a opção vale para uma saída só")
	{
		// applyProperties() put the flag back down...
		REQUIRE_FALSE(sheet->monochrome());

		// ...and a render with whatever the folio now holds is the colour
		// one again, pixel for pixel: nothing of the relay stayed behind.
		// The other options are those of the first render, so that the
		// flag is the only thing that can make the two differ.
		ExportProperties as_left = options(false);
		as_left.monochrome = sheet->monochrome();
		const ExportProperties previous = sheet->applyProperties(as_left);
		const UiBench::Rendering again(sheet, render_width);
		sheet->applyProperties(previous);
		CHECK(again.image() == colour.image());
	}
}

TEST_CASE("T37 — o PDF em preto e branco continua pesquisável, e sem cor",
	  "[uibench][impressao]")
{
	UiBench::ScratchProject scratch(projectXml());
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	dress(sheet);

	QTemporaryDir work;
	REQUIRE(work.isValid());
	const QString colour_path = work.filePath(QStringLiteral("colour.pdf"));
	const QString mono_path = work.filePath(QStringLiteral("mono.pdf"));

	ExportProperties before = sheet->applyProperties(options(false));
	const bool colour_written = writePdf(sheet, colour_path);
	sheet->applyProperties(before);

	before = sheet->applyProperties(options(true));
	const bool mono_written = writePdf(sheet, mono_path);
	sheet->applyProperties(before);

	REQUIRE(colour_written);
	REQUIRE(mono_written);

	SECTION("o texto sai como texto, o mesmo das duas saídas")
	{
		// The relay hands each text item to the PDF engine whole. Had it
		// drawn the letters as curves, the file would look the same and the
		// search box of a reader would find nothing.
		if (!putsTextInPdf())
		{
			SUCCEED("this build writes no embedded font into a PDF without a screen;"
				" the searchability of the export would measure the platform");
			return;
		}

		PdfProbe::Document colour(colour_path);
		PdfProbe::Document mono(mono_path);
		INFO(colour.error().toStdString() << " / " << mono.error().toStdString());
		REQUIRE(colour.isValid());
		REQUIRE(mono.isValid());
		REQUIRE(mono.pageCount() == 1);

		CHECK(mono.hasMappedFont(1));
		const QString colour_text = PdfProbe::squeezed(colour.text(1));
		const QString mono_text = PdfProbe::squeezed(mono.text(1));
		CHECK(mono_text.contains(PdfProbe::squeezed(QLatin1String(anchor))));
		CHECK(mono_text == colour_text);
	}

	SECTION("a página do PDF não tem cor nenhuma")
	{
#ifdef QET_HAS_QTPDF
		const QImage colour = rasterise(colour_path);
		const QImage mono = rasterise(mono_path);
		REQUIRE_FALSE(colour.isNull());
		REQUIRE_FALSE(mono.isNull());
		INFO("pixels with a hue, colour PDF: " << pixelsWithHue(colour, pdf_tolerance));
		CHECK(pixelsWithHue(colour, pdf_tolerance) > 0);
		CHECK(pixelsWithHue(mono, pdf_tolerance) == 0);
#else
		SUCCEED("built without QtPdf: no library here reads the page of a PDF back as pixels");
#endif
	}
}

TEST_CASE("T37 — a janela de impressão escolhe o preto e branco, leva-o ao PDF e o lembra",
	  "[uibench][impressao]")
{
	const KeepPrintSettings keep;
	// The key toSettings() writes under the prefix the window uses.
	QSettings().setValue(QStringLiteral("print/defaultmonochrome"), false);

	UiBench::ScratchProject scratch(projectXml());
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagram(0) != nullptr);
	dress(scratch.diagram(0));

	QTemporaryDir work;
	REQUIRE(work.isValid());
	const QString colour_path = work.filePath(QStringLiteral("window-colour.pdf"));
	const QString mono_path = work.filePath(QStringLiteral("window-mono.pdf"));

	bool box_found = false;

	// Left as the setting says: clear.
	const bool first_opening = printWithWindow(scratch.project(), colour_path, -1, &box_found);
	REQUIRE(box_found);
	CHECK_FALSE(first_opening);
	CHECK_FALSE(QSettings().value(QStringLiteral("print/defaultmonochrome")).toBool());

	// Ticked in the window, then printed: the window saves its options when
	// it prints, and this one with them.
	printWithWindow(scratch.project(), mono_path, 1, &box_found);
	REQUIRE(box_found);
	CHECK(QSettings().value(QStringLiteral("print/defaultmonochrome")).toBool());

	// So the next window opens with the box ticked.
	const QString again_path = work.filePath(QStringLiteral("window-again.pdf"));
	const bool third_opening = printWithWindow(scratch.project(), again_path, -1, &box_found);
	REQUIRE(box_found);
	CHECK(third_opening);

	// The folio was given its own options back after each print.
	CHECK_FALSE(scratch.diagram(0)->monochrome());

	REQUIRE(QFile::exists(colour_path));
	REQUIRE(QFile::exists(mono_path));

#ifdef QET_HAS_QTPDF
	// And the box reached the file: the same project, the same window, one
	// page with colour and one without.
	const QImage colour = rasterise(colour_path);
	const QImage mono = rasterise(mono_path);
	REQUIRE_FALSE(colour.isNull());
	REQUIRE_FALSE(mono.isNull());
	INFO("pixels with a hue, colour PDF: " << pixelsWithHue(colour, pdf_tolerance));
	CHECK(pixelsWithHue(colour, pdf_tolerance) > 0);
	CHECK(pixelsWithHue(mono, pdf_tolerance) == 0);
#else
	SUCCEED("built without QtPdf: that the box reached the file is proved by the case above, on the same render path");
#endif
}

TEST_CASE("T37 — o preto e branco é gravado com as outras opções de exportação",
	  "[uibench][impressao]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("settings.ini"));

	// Off unless asked for: an export nobody configured stays in colour.
	CHECK_FALSE(ExportProperties().monochrome);

	{
		QSettings settings(path, QSettings::IniFormat);
		ExportProperties properties;
		properties.monochrome = true;
		properties.toSettings(settings, QStringLiteral("print/default"));
		properties.monochrome = false;
		properties.toSettings(settings, QStringLiteral("export/default"));
	}

	QSettings settings(path, QSettings::IniFormat);
	// Beside the other options of the same prefix, under the name the print
	// window case above writes by hand.
	CHECK(settings.contains(QStringLiteral("print/defaultmonochrome")));

	ExportProperties printing;
	printing.fromSettings(settings, QStringLiteral("print/default"));
	CHECK(printing.monochrome);

	// Printing and exporting keep their own answer.
	ExportProperties exported;
	exported.monochrome = true;
	exported.fromSettings(settings, QStringLiteral("export/default"));
	CHECK_FALSE(exported.monochrome);

	// Settings written before the option existed read as colour.
	ExportProperties old;
	old.monochrome = true;
	old.fromSettings(settings, QStringLiteral("nothing/here"));
	CHECK_FALSE(old.monochrome);
}

TEST_CASE("T37 — o fundo branco é desligado por padrão, e gravado com chave própria",
	  "[uibench][impressao]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("settings.ini"));

	// Without a choice, a PNG or an SVG keeps no sheet (P154).
	CHECK_FALSE(ExportProperties().draw_white_background);

	{
		QSettings settings(path, QSettings::IniFormat);
		ExportProperties properties;
		properties.draw_white_background = true;
		properties.toSettings(settings, QStringLiteral("export/default"));
	}
	{
		QSettings settings(path, QSettings::IniFormat);
		CHECK(settings.contains(QStringLiteral("export/defaultwhitebackground")));
		ExportProperties read;
		read.fromSettings(settings, QStringLiteral("export/default"));
		CHECK(read.draw_white_background);
	}

	// The transparency of the SVG, which every export saved false until now,
	// does not turn the white sheet on: the key is another one.
	{
		QSettings settings(path, QSettings::IniFormat);
		settings.setValue(QStringLiteral("before/drawbgtransparent"), false);
	}
	QSettings settings(path, QSettings::IniFormat);
	ExportProperties before;
	before.fromSettings(settings, QStringLiteral("before/"));
	CHECK_FALSE(before.draw_white_background);
}

TEST_CASE("T37 — exportar o PDF não muda a cor da folha na tela",
	  "[uibench][impressao]")
{
	const KeepPrintSettings keep;
	// The color of the sheets is one for the whole program: put it back.
	struct KeepSheetColor
	{
		const QColor kept = Diagram::background_color;
		~KeepSheetColor() { Diagram::background_color = kept; }
	} keep_sheet_color;
	const QColor sheet_color(60, 60, 60);
	Diagram::background_color = sheet_color;

	UiBench::ScratchProject scratch(projectXml());
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagram(0) != nullptr);

	QTemporaryDir work;
	REQUIRE(work.isValid());
	const QString path = work.filePath(QStringLiteral("sheet-color.pdf"));

	auto *printer = new QPrinter();
	printer->setOutputFormat(QPrinter::PdfFormat);
	printer->setOutputFileName(path);
	{
		ProjectPrintWindow window(scratch.project(), printer);
		// Open, the sheets behind it keep their color.
		CHECK(Diagram::background_color.name() == sheet_color.name());

		QMetaObject::invokeMethod(&window, "print");
		// The PDF is post-processed, and the window closed, on the next turn
		// of the event loop.
		QCoreApplication::processEvents();

		// Closed and not destroyed, as in the editor, where closing only
		// hides it: the color is still the one the sheets had.
		CHECK(Diagram::background_color.name() == sheet_color.name());
	}
	CHECK(Diagram::background_color.name() == sheet_color.name());
	REQUIRE(QFile::exists(path));

#ifdef QET_HAS_QTPDF
	// And the paper of the PDF is white. Measured against the same sheet
	// drawn without the window, on its color: the count sees that color, so
	// a count near zero is the white paper and not a count that sees nothing.
	const QString direct_path = work.filePath(QStringLiteral("sheet-color-direct.pdf"));
	REQUIRE(writePdf(scratch.diagram(0), direct_path));
	const QImage direct = rasterise(direct_path);
	const QImage printed = rasterise(path);
	REQUIRE_FALSE(direct.isNull());
	REQUIRE_FALSE(printed.isNull());

	const int direct_pixels = direct.width() * direct.height();
	const int printed_pixels = printed.width() * printed.height();
	const int direct_sheet = pixelsOf(direct, sheet_color, pdf_tolerance);
	const int printed_sheet = pixelsOf(printed, sheet_color, pdf_tolerance);
	INFO("pixels of the sheet color, drawn on it: " << direct_sheet << " of " << direct_pixels);
	INFO("pixels of the sheet color, printed: " << printed_sheet << " of " << printed_pixels);
	REQUIRE(direct_sheet > direct_pixels / 2);
	CHECK(printed_sheet < printed_pixels / 100);
#else
	SUCCEED("built without QtPdf: no library here reads the page of a PDF back as pixels");
#endif
}

TEST_CASE("T37 — a linha de comando desenha em preto e branco quando pedida",
	  "[uibench][impressao]")
{
	QTemporaryDir work;
	REQUIRE(work.isValid());
	const QString project_path = work.filePath(QStringLiteral("cli.qet"));
	{
		QFile file(project_path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
		file.write(projectXml().toUtf8());
	}
	const QString colour_path = work.filePath(QStringLiteral("cli-colour.pdf"));
	const QString mono_path = work.filePath(QStringLiteral("cli-mono.pdf"));
	const QString pdf_verb = QStringLiteral("--export-pdf");

		//Without the option, as before; with it, the box of the window
	CHECK(CLIExport::run({pdf_verb, project_path, colour_path}) == 0);
	CHECK(CLIExport::run({pdf_verb, project_path, mono_path,
			      QStringLiteral("monochrome=true")}) == 0);
	REQUIRE(QFile::exists(colour_path));
	REQUIRE(QFile::exists(mono_path));

	SECTION("a value other than true or false, or the option twice, writes nothing")
	{
		const QString bad = work.filePath(QStringLiteral("cli-bad.pdf"));
		CHECK(CLIExport::run({pdf_verb, project_path, bad,
				      QStringLiteral("monochrome=yes")}) != 0);
		CHECK(CLIExport::run({pdf_verb, project_path, bad,
				      QStringLiteral("monochrome=true"),
				      QStringLiteral("monochrome=false")}) != 0);
		CHECK_FALSE(QFile::exists(bad));
	}

	SECTION("with folios= beside it")
	{
		const QString both = work.filePath(QStringLiteral("cli-both.pdf"));
		CHECK(CLIExport::run({pdf_verb, project_path, both,
				      QStringLiteral("folios=1"),
				      QStringLiteral("monochrome=true")}) == 0);
		CHECK(QFile::exists(both));
	}

#ifdef QET_HAS_QTPDF
		//The red symbol of the fixture has a hue in colour, and none in
		//black and white
	const QImage colour = rasterise(colour_path);
	const QImage mono = rasterise(mono_path);
	REQUIRE_FALSE(colour.isNull());
	REQUIRE_FALSE(mono.isNull());
	INFO("pixels with a hue, colour PDF: " << pixelsWithHue(colour, pdf_tolerance));
	CHECK(pixelsWithHue(colour, pdf_tolerance) > 0);
	CHECK(pixelsWithHue(mono, pdf_tolerance) == 0);
#else
	SUCCEED("built without QtPdf: no library here reads the page of a PDF back as pixels");
#endif
}
