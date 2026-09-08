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
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QDir>
#include <QFile>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPointF>
#include <QRect>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

/*
	What comes out of the emitted drawing, read back from the file.

	The export path itself is not new: the command line already renders every
	sheet into one PDF and already turns the cross-references into internal
	jumps. What was never checked is whether the result keeps the two promises
	an emitted drawing makes to whoever receives it - that a tag written on a
	sheet can be found in the file, and that a cross-reference lands on the
	right sheet at the right place.

	Neither can be seen from the code that writes them. Text is handed to the
	engine as a string and may leave as a curve; a jump is handed over as a
	rectangle in one coordinate system and may land in another. So the file is
	opened again and read, in src/ui/pdfprobe.cpp, and what is asserted here
	is what the bytes say.

	Two fixtures, on purpose. The searchable-text case builds its own two-sheet
	project, so the words looked for are the words this file wrote and nothing
	depends on what an example happens to contain. The cross-reference case
	needs coils and contacts wired to one another across sheets, which is a
	drawing nobody writes by hand: it takes an example that ships with the
	program, and derives what it expects from the project it opened rather
	than from a number typed here.

	One thing here is not provable on every build, and it is measured rather
	than assumed: see putsTextInPdf(). The cross-reference case does not
	depend on it and runs everywhere.
*/

namespace
{
	/// The word looked for on the first sheet, written only there.
	const char *first_anchor = "SEARCH-ANCHOR-ALPHA";
	/// The word looked for on the second sheet, written only there.
	const char *second_anchor = "SEARCH-ANCHOR-BETA";

	/**
		The example the cross-reference case reads.

		Five sheets, sixteen masters and nine slaves wired across them: the
		smallest example that ships with a cross-reference on more than one
		sheet, which is what the case is about. A bigger one would prove the
		same thing and cost a minute per run.
	*/
	const char *reference_example = "tableau_domestique.qet";

	/// A two-sheet project, each sheet carrying one word of its own.
	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"First\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/>"
			       "<inputs>"
			       "<input x=\"150\" y=\"150\" text=\"%1\" rotation=\"0\"/>"
			       "</inputs>"
			       "<conductors/>"
			       "</diagram>"
			       "<diagram title=\"Second\" order=\"2\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/>"
			       "<inputs>"
			       "<input x=\"150\" y=\"150\" text=\"%2\" rotation=\"0\"/>"
			       "</inputs>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(QLatin1String(first_anchor),
			    QLatin1String(second_anchor));
	}

	/**
		@return whether this build can put searchable text into a PDF at all.

		Measured, and not assumed. The Qt5 build of this machine rasterises no
		glyph without a screen, and the PDF it writes then carries no embedded
		font either - the words are simply not in the file. An assertion over
		the text of an exported sheet would fail there for a reason that has
		nothing to do with the export path, which is the same trap the ink
		comparisons of titleblockeditor_test.cpp calibrate against.

		The reference is written with the same QPdfWriter the export path uses
		and read with the same probe the assertions use, so a yes here means
		the two sides of the measurement agree about this machine.
	*/
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
			painter.drawText(QRect(0, 0, 400, 60),
					 Qt::AlignLeft | Qt::AlignVCenter,
					 QStringLiteral("mmm"));
		}

		PdfProbe::Document reference(path);
		return reference.isValid() && reference.hasMappedFont(1);
	}

	/// What a section says when it steps aside for the reason above.
	const char *no_text_reason =
		"this build writes no embedded font into a PDF without a screen;"
		" the text of an exported sheet would measure the platform and"
		" not the export path";

	/// @return false when @a content could not be written to @a path.
	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return false;
		}
		QTextStream stream(&file);
		stream << content;
		file.close();
		return true;
	}

	/// 1-based position of @a sheet among the project's sheets, 0 when absent.
	int folioOf(QETProject *project, Diagram *sheet)
	{
		if (!project || !sheet) {
			return 0;
		}
		const QList<Diagram *> sheets = project->diagrams();
		const int at = int(sheets.indexOf(sheet));
		return at < 0 ? 0 : at + 1;
	}

	/**
		Every pair of sheets a master and one of its contacts sit on, when the
		two are not the same sheet.

		This is what a cross-reference in the file has to account for: the
		reader clicks the coil and has to arrive at the sheet the contact is
		drawn on. Pairs on one sheet are left out - there is nothing to jump
		to - and the direction is not recorded, because the drawing carries
		the reference at both ends and either one satisfies the promise.
	*/
	QSet<QPair<int, int>> linkedFolioPairs(QETProject *project)
	{
		QSet<QPair<int, int>> pairs;
		if (!project) {
			return pairs;
		}

		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets)
		{
			const QList<Element *> elements = sheet->elements();
			for (Element *element : elements)
			{
				if (element->linkType() != Element::Master) {
					continue;
				}
				const int from = folioOf(project, sheet);
				const QList<Element *> linked =
					element->linkedElements();
				for (Element *contact : linked)
				{
					const int to =
						folioOf(project, contact->diagram());
					if (from == 0 || to == 0 || from == to) {
						continue;
					}
					pairs.insert(qMakePair(qMin(from, to),
							       qMax(from, to)));
				}
			}
		}
		return pairs;
	}
}

TEST_CASE("T37 — o PDF emitido carrega texto que se procura, e as folhas em ordem",
	  "[uibench][impressao]")
{
	QTemporaryDir work;
	REQUIRE(work.isValid());

	const QString project_path = work.filePath(QStringLiteral("bench.qet"));
	const QString pdf_path     = work.filePath(QStringLiteral("bench.pdf"));
	REQUIRE(writeFile(project_path, projectXml()));

	const int code = CLIExport::run(QStringList()
					<< QStringLiteral("--export-pdf")
					<< project_path
					<< pdf_path);
	REQUIRE(code == 0);
	REQUIRE(QFile::exists(pdf_path));

	PdfProbe::Document pdf(pdf_path);
	INFO(pdf.error().toStdString());
	REQUIRE(pdf.isValid());

	SECTION("as duas folhas viram duas páginas do mesmo arquivo")
	{
		// "One file, sheets in order" is the whole of the first promise:
		// a second file, or a sheet dropped, is what the receiving side
		// notices first and what nothing else in the suite would see.
		REQUIRE(pdf.pageCount() == 2);
	}

	SECTION("o texto da folha sai como texto, e não como desenho de letra")
	{
		// A page that drew its words as curves has no font carrying a
		// glyph-to-character map, and the reader's search box has nothing
		// to look into. That is the difference this asserts - it is not
		// about how the page looks, which is identical either way.
		if (!putsTextInPdf()) {
			SUCCEED(no_text_reason);
			return;
		}
		REQUIRE(pdf.hasMappedFont(1));
		REQUIRE(pdf.hasMappedFont(2));
	}

	SECTION("a palavra escrita na folha é achada na página dela")
	{
		if (!putsTextInPdf()) {
			SUCCEED(no_text_reason);
			return;
		}

		const QString first  = PdfProbe::squeezed(pdf.text(1));
		const QString second = PdfProbe::squeezed(pdf.text(2));

		INFO("page 1: " << first.toStdString());
		INFO("page 2: " << second.toStdString());

		REQUIRE(first.contains(
				PdfProbe::squeezed(QLatin1String(first_anchor))));
		REQUIRE(second.contains(
				PdfProbe::squeezed(QLatin1String(second_anchor))));

		// And each on its own page, which is what says the sheets came
		// out in order rather than merely both coming out.
		REQUIRE_FALSE(first.contains(
				      PdfProbe::squeezed(
					      QLatin1String(second_anchor))));
		REQUIRE_FALSE(second.contains(
				      PdfProbe::squeezed(
					      QLatin1String(first_anchor))));
	}
}

TEST_CASE("T37 — a referência cruzada emitida salta para a folha e o lugar certos",
	  "[uibench][impressao]")
{
	QString reason;
	const QString source = UiBench::examplePath(
		QLatin1String(reference_example));
	const bool opens = UiBench::opensWithoutDialog(source, &reason);
	INFO(reason.toStdString());
	REQUIRE(opens);

	QTemporaryDir work;
	REQUIRE(work.isValid());
	const QString pdf_path = work.filePath(QStringLiteral("xref.pdf"));

	const int code = CLIExport::run(QStringList()
					<< QStringLiteral("--export-pdf")
					<< source
					<< pdf_path);
	REQUIRE(code == 0);

	PdfProbe::Document pdf(pdf_path);
	INFO(pdf.error().toStdString());
	REQUIRE(pdf.isValid());

	const QList<PdfProbe::Link> links = pdf.links();

	SECTION("o desenho tem referência cruzada, e ela chega ao arquivo")
	{
		const QString name = QLatin1String(reference_example);
		UiBench::Project project(name);
		INFO(project.error().toStdString());
		REQUIRE(project.isOpen());
		REQUIRE(pdf.pageCount() == project.diagramCount());

		const QSet<QPair<int, int>> expected =
			linkedFolioPairs(project.project());
		// The example is chosen for having these; a fixture that stopped
		// having them would make every assertion below vacuous.
		REQUIRE_FALSE(expected.isEmpty());

		QSet<QPair<int, int>> emitted;
		for (const PdfProbe::Link &link : links) {
			if (link.target_page > 0
			    && link.target_page != link.source_page) {
				emitted.insert(
					qMakePair(qMin(link.source_page,
						       link.target_page),
						  qMax(link.source_page,
						       link.target_page)));
			}
		}

		QStringList missing;
		for (const QPair<int, int> &pair : expected) {
			if (!emitted.contains(pair)) {
				missing << QStringLiteral("%1<->%2")
					   .arg(pair.first).arg(pair.second);
			}
		}
		INFO("folio pairs without a link in the file: "
		     << missing.join(QStringLiteral(", ")).toStdString());
		REQUIRE(missing.isEmpty());
	}

	SECTION("nenhum salto aponta para página que não existe")
	{
		REQUIRE_FALSE(links.isEmpty());

		QStringList dangling;
		for (const PdfProbe::Link &link : links) {
			if (link.target_page < 1
			    || link.target_page > pdf.pageCount()) {
				dangling << QStringLiteral("page %1 -> %2")
					    .arg(link.source_page)
					    .arg(link.target_page);
			}
		}
		INFO("links out of the document: "
		     << dangling.join(QStringLiteral(", ")).toStdString());
		REQUIRE(dangling.isEmpty());
	}

	SECTION("o lugar apontado cai dentro da página de destino")
	{
		// The sheet the jump lands on is the easy half; where on the
		// sheet is the half that goes wrong quietly. The framed region
		// is computed while another page is being drawn, and the two
		// pages of a project are not the same size - a sheet is as wide
		// as its own frame. So the check is the one thing that cannot be
		// true by accident: the middle of what the reader is taken to
		// has to be a point of the sheet it was taken to.
		QStringList off_page;
		for (const PdfProbe::Link &link : links) {
			if (link.destination.isEmpty()) {
				continue;   // frames the whole page, nothing to check
			}
			const QSizeF page = pdf.pageSize(link.target_page);
			const QPointF middle = link.destination.center();
			if (middle.x() < 0 || middle.y() < 0
			    || middle.x() > page.width()
			    || middle.y() > page.height()) {
				off_page << QStringLiteral(
						"page %1 -> %2 at (%3, %4),"
						" page is %5 x %6")
					    .arg(link.source_page)
					    .arg(link.target_page)
					    .arg(middle.x()).arg(middle.y())
					    .arg(page.width())
					    .arg(page.height());
			}
		}
		INFO("destinations outside their own page: "
		     << off_page.join(QStringLiteral(" | ")).toStdString());
		REQUIRE(off_page.isEmpty());
	}
}
