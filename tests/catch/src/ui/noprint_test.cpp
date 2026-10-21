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

#include "../../../../sources/diagram.h"
#include "../../../../sources/dxfexport.h"
#include "../../../../sources/exportproperties.h"
#include "../../../../sources/qetgraphicsitem/independenttextitem.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/inditextpropertieswidget.h"
#include "../../../../sources/ui/noprintbox.h"
#include "../../../../sources/ui/shapegraphicsitempropertieswidget.h"

#include <catch2/catch.hpp>

#include <QFile>
#include <QGraphicsView>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QUndoStack>

/*
	A note of the drawing office - an arrow, a reminder - marked not to
	print (T37, step 5): shown on screen, left out of the paper, the PDF
	and the DXF. Only free shapes and free texts take the mark.

	The shapes are proved by pixel: a red note and a blue control, thick
	enough that their colour survives antialiasing, drawn where this file
	puts them. The texts are proved by what the PDF and the DXF carry, the
	words being searched for; a build that writes no text into a PDF is
	measured first and the PDF half is then skipped with a warning, as
	pdfexport_test.cpp does.
*/
namespace
{
	const QString printed_word = QStringLiteral("PRINTED-ANCHOR");
	const QString note_word = QStringLiteral("NOPRINT-ANCHOR");

	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\"/></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/>"
			       "<inputs>"
			       "<input x=\"150\" y=\"420\" text=\"%1\" rotation=\"0\"/>"
			       "<input x=\"500\" y=\"420\" text=\"%2\" rotation=\"0\"/>"
			       "</inputs>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(printed_word, note_word);
	}

	IndependentTextItem *textOf(Diagram *sheet, const QString &word)
	{
		const QList<QGraphicsItem *> items = sheet->items();
		for (QGraphicsItem *item : items) {
			if (auto *text = qgraphicsitem_cast<IndependentTextItem *>(item)) {
				if (text->toPlainText() == word) {
					return text;
				}
			}
		}
		return nullptr;
	}

	QetShapeItem *shapeOf(Diagram *sheet, const QColor &colour)
	{
		const QList<QGraphicsItem *> items = sheet->items();
		for (QGraphicsItem *item : items) {
			if (auto *shape = qgraphicsitem_cast<QetShapeItem *>(item)) {
				if (shape->pen().color() == colour) {
					return shape;
				}
			}
		}
		return nullptr;
	}

	QetShapeItem *addShape(Diagram *sheet, const QRectF &rect, const QColor &colour)
	{
		auto *shape = new QetShapeItem(rect.topLeft(), rect.bottomRight(),
					       QetShapeItem::Rectangle);
		shape->setPen(QPen(colour, 10));
		sheet->addItem(shape);
		return shape;
	}

		/// Pixels of @a image near @a colour, one of pure red or pure blue
	int pixelsOf(const QImage &image, const QColor &colour)
	{
		int count = 0;
		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				const QColor c = image.pixelColor(x, y);
				const bool red = c.red() > 180 && c.green() < 90 && c.blue() < 90;
				const bool blue = c.blue() > 180 && c.red() < 90 && c.green() < 90;
				if ((colour == Qt::red && red) || (colour == Qt::blue && blue)) {
					++count;
				}
			}
		}
		return count;
	}

		/// The page of @a sheet as print and export draw it: with no view
	QImage printed(Diagram *sheet)
	{
		const QRectF page = UiBench::pageRect(sheet);
		QImage image(page.size().toSize(), QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		sheet->render(&painter, QRectF(QPointF(0, 0), page.size()), page);
		painter.end();
		return image;
	}

		/// The sheet as a view on the screen shows it, whole
	QImage onScreen(Diagram *sheet)
	{
		QGraphicsView view(sheet);
		view.resize(1400, 1000);
		view.fitInView(UiBench::pageRect(sheet), Qt::KeepAspectRatio);
		return view.viewport()->grab().toImage();
	}

	bool writePdf(Diagram *sheet, const QString &path)
	{
		const QRectF page = UiBench::pageRect(sheet);
		QPdfWriter writer(path);
		writer.setResolution(96);
		writer.setPageLayout(QPageLayout(
					     QPageSize(QSizeF(page.width() * 72. / 96.,
							      page.height() * 72. / 96.),
						       QPageSize::Point),
					     QPageLayout::Portrait, QMarginsF(0, 0, 0, 0)));
		QPainter painter;
		if (!painter.begin(&writer)) {
			return false;
		}
		sheet->render(&painter, QRectF(0, 0, writer.width(), writer.height()),
			      page, Qt::KeepAspectRatio);
		return painter.end();
	}

		/// Whether this build writes searchable text into a PDF at all
	bool putsTextInPdf()
	{
		QTemporaryDir dir;
		if (!dir.isValid()) {
			return false;
		}
		const QString path = dir.filePath(QStringLiteral("calibre.pdf"));
		{
			QPdfWriter writer(path);
			writer.setResolution(96);
			writer.setPageSize(QPageSize(QPageSize::A4));
			QPainter painter(&writer);
			if (!painter.isActive()) {
				return false;
			}
			painter.drawText(QRect(0, 0, 400, 60), Qt::AlignLeft | Qt::AlignVCenter,
					 QStringLiteral("mmm"));
		}
		PdfProbe::Document reference(path);
		return reference.isValid() && reference.hasMappedFont(1);
	}

	QString dxfOf(Diagram *sheet, const QString &path)
	{
		const ExportProperties properties;
		const QSize size = DxfExport::folioSize(sheet, properties);
		DxfExport::write(sheet, size.width(), size.height(), path, properties);
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QString();
		}
		return QString::fromUtf8(file.readAll());
	}
}

TEST_CASE("CU-37.5 — auxílio de tela: na tela sim, no papel e no PDF não",
	  "[uibench][t37][noprint]")
{
	UiBench::ScratchProject bench(projectXml(), QStringLiteral("noprint.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);

	IndependentTextItem *word = textOf(sheet, printed_word);
	IndependentTextItem *note_text = textOf(sheet, note_word);
	REQUIRE(word);
	REQUIRE(note_text);
	QetShapeItem *note = addShape(sheet, QRectF(150, 150, 200, 150), Qt::red);
	QetShapeItem *control = addShape(sheet, QRectF(500, 150, 200, 150), Qt::blue);

		//Both shapes are drawn everywhere before the mark: the sonde
	REQUIRE(pixelsOf(printed(sheet), Qt::red) > 200);
	REQUIRE(pixelsOf(printed(sheet), Qt::blue) > 200);

	QUndoCommand *step = NoPrintBox::markCommand({note, note_text}, false);
	REQUIRE(step);
	sheet->undoStack().push(step);
	CHECK_FALSE(note->isPrintable());
	CHECK_FALSE(note_text->isPrintable());
	CHECK(control->isPrintable());
	CHECK(word->isPrintable());

	SECTION("on the screen the note is there")
	{
		const QImage screen = onScreen(sheet);
		CHECK(pixelsOf(screen, Qt::red) > 50);
		CHECK(pixelsOf(screen, Qt::blue) > 50);
	}

	SECTION("on the paper it is not, and the control is")
	{
		const QImage paper = printed(sheet);
		CHECK(pixelsOf(paper, Qt::red) == 0);
		CHECK(pixelsOf(paper, Qt::blue) > 200);
	}

	SECTION("the PDF carries the text that prints, and not the note")
	{
		if (!putsTextInPdf()) {
			WARN("this build writes no searchable text into a PDF: half skipped");
		} else {
			QTemporaryDir dir;
			REQUIRE(dir.isValid());
			const QString path = dir.filePath(QStringLiteral("noprint.pdf"));
			REQUIRE(writePdf(sheet, path));
			PdfProbe::Document pdf(path);
			REQUIRE(pdf.isValid());
			const QString text = PdfProbe::squeezed(pdf.text());
			CHECK(text.contains(PdfProbe::squeezed(printed_word)));
			CHECK_FALSE(text.contains(PdfProbe::squeezed(note_word)));
		}
	}

	SECTION("the DXF leaves the note out as well")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString dxf = dxfOf(sheet, dir.filePath(QStringLiteral("noprint.dxf")));
		REQUIRE_FALSE(dxf.isEmpty());
		CHECK(dxf.contains(printed_word));
		CHECK_FALSE(dxf.contains(note_word));
	}

	SECTION("undone, the note prints again")
	{
		sheet->undoStack().undo();
		CHECK(note->isPrintable());
		CHECK(note_text->isPrintable());
		CHECK(pixelsOf(printed(sheet), Qt::red) > 200);
	}
}

TEST_CASE("T37 — a marca de não imprimir sobrevive a gravar e reabrir",
	  "[uibench][t37][noprint]")
{
	UiBench::ScratchProject bench(projectXml(), QStringLiteral("noprintsave.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);

	addShape(sheet, QRectF(150, 150, 200, 150), Qt::red)->setPrintable(false);
	addShape(sheet, QRectF(500, 150, 200, 150), Qt::blue);
	textOf(sheet, note_word)->setPrintable(false);

	REQUIRE(bench.saveAndReopen());
	sheet = bench.diagram(0);
	REQUIRE(sheet);

	QFile file(bench.filePath());
	REQUIRE(file.open(QIODevice::ReadOnly));
	const QString saved = QString::fromUtf8(file.readAll());
		//Written only for what does not print: a file without the mark
		//reads as it always did
	CHECK(saved.count(QStringLiteral("printable=\"false\"")) == 2);
	CHECK(saved.count(QStringLiteral("printable=")) == 2);

	REQUIRE(shapeOf(sheet, Qt::red));
	REQUIRE(shapeOf(sheet, Qt::blue));
	REQUIRE(textOf(sheet, note_word));
	REQUIRE(textOf(sheet, printed_word));
	CHECK_FALSE(shapeOf(sheet, Qt::red)->isPrintable());
	CHECK(shapeOf(sheet, Qt::blue)->isPrintable());
	CHECK_FALSE(textOf(sheet, note_word)->isPrintable());
	CHECK(textOf(sheet, printed_word)->isPrintable());
}

TEST_CASE("T37 — a caixa Não imprimir marca a seleção num passo só",
	  "[uibench][t37][noprint]")
{
	UiBench::ScratchProject bench(projectXml(), QStringLiteral("noprintbox.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);

	QetShapeItem *note = addShape(sheet, QRectF(150, 150, 200, 150), Qt::red);
	QetShapeItem *control = addShape(sheet, QRectF(500, 150, 200, 150), Qt::blue);
	QUndoStack &stack = sheet->undoStack();

	SECTION("two shapes: partly checked when they differ, one step for both")
	{
		ShapeGraphicsItemPropertiesWidget widget(QList<QetShapeItem *>{note, control});
		auto *box = widget.findChild<NoPrintBox *>();
		REQUIRE(box);
		CHECK(box->isEnabled());
		CHECK(box->checkState() == Qt::Unchecked);

			//Marked from elsewhere, the box follows
		note->setPrintable(false);
		CHECK(box->checkState() == Qt::PartiallyChecked);

		const int before = stack.count();
		box->click();
		CHECK_FALSE(note->isPrintable());
		CHECK_FALSE(control->isPrintable());
		CHECK(stack.count() == before + 1);
		CHECK(box->checkState() == Qt::Checked);

		stack.undo();
		CHECK_FALSE(note->isPrintable());
		CHECK(control->isPrintable());
		CHECK(box->checkState() == Qt::PartiallyChecked);
	}

	SECTION("one free text, through its own panel")
	{
		IndependentTextItem *text = textOf(sheet, note_word);
		REQUIRE(text);
		IndiTextPropertiesWidget widget(text);
		auto *box = widget.findChild<NoPrintBox *>();
		REQUIRE(box);
		CHECK(box->checkState() == Qt::Unchecked);

		const int before = stack.count();
		box->click();
		CHECK_FALSE(text->isPrintable());
		CHECK(stack.count() == before + 1);

		box->click();
		CHECK(text->isPrintable());
		CHECK(stack.count() == before + 2);
	}

	SECTION("an item without the mark is left alone")
	{
			//A component, a wire, a text of a component: none has the
			//property, and asking to mark one is a step with nothing in it
		QGraphicsTextItem plain(QStringLiteral("plain"));
		CHECK(NoPrintBox::markCommand({&plain}, false) == nullptr);

		QUndoCommand *step = NoPrintBox::markCommand({&plain, note}, false);
		REQUIRE(step);
		CHECK(step->childCount() == 1);
		delete step;
	}
}
