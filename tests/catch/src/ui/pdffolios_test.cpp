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

#include "../qt_catch_tostring.h"

#include "../../../../sources/cli_export.h"

#include <catch2/catch.hpp>

#include <QFile>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>

/*
	Which sheets the command line puts into the PDF, and in which order.

	Drawing every sheet of a project is what the export path already did;
	drawing three of them is what it could not. Whoever emits a revision
	sends the sheets that changed, and that choice lived only in the print
	window, behind a list of check boxes no script can tick.

	What is asserted here is the document and not the call. The file is
	opened again and every page is identified by its own size, which is why
	the three sheets of the fixture are deliberately given different column
	and row counts: a sheet becomes a page as wide as itself and as wide as
	no other. Counting pages would pass with the wrong sheets in the wrong
	order, and that is precisely the failure worth catching - an emitted
	document is looked at by someone who was not there when it was asked
	for.

	The refusals carry the same weight as the exports. A folio that does not
	exist, a folio asked for twice and a misspelt option each have an
	obvious quiet answer - drop it, or fall back to everything - and every
	one of those answers hands back a file that looks complete and is not.
*/

namespace
{
	/// The word written on folio @a folio of the fixture, and on no other.
	QString anchorOf(int folio)
	{
		static const QStringList words {
			QStringLiteral("FOLIOALPHA"),
			QStringLiteral("FOLIOBETA"),
			QStringLiteral("FOLIOGAMMA")};
		return words.value(folio - 1);
	}

	/**
		One sheet of the fixture, sized so that no other sheet shares its
		size.

		The page a sheet becomes is as big as its frame, and the frame comes
		out of the column and row counts - so giving each sheet its own
		counts is what lets a page of the emitted file say which sheet it
		is.
	*/
	QString sheetXml(int order, int columns, int rows)
	{
		return QStringLiteral(
				   "<diagram title=\"Sheet %1\" order=\"%1\""
				   " height=\"600\""
				   " cols=\"%2\" colsize=\"60\""
				   " rows=\"%3\" rowsize=\"80\""
				   " displaycols=\"true\" displayrows=\"true\">"
				   "<elements/>"
				   "<inputs>"
				   "<input x=\"150\" y=\"150\" text=\"%4\""
				   " rotation=\"0\"/>"
				   "</inputs>"
				   "<conductors/>"
				   "</diagram>")
			   .arg(order).arg(columns).arg(rows).arg(anchorOf(order));
	}

	/// A three-sheet project, each sheet with its own size and its own word.
	QString projectXml()
	{
		return QStringLiteral(
				   "<project title=\"bench\" version=\"0.80\">"
				   "<collection><category name=\"import\"/></collection>"
				   "%1%2%3"
				   "</project>")
			   .arg(sheetXml(1, 17, 8),
					sheetXml(2, 11, 5),
					sheetXml(3, 23, 11));
	}

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

	/// Ask the command line for @a pdf_path, with @a options after the paths.
	int exportPdf(const QString &project_path, const QString &pdf_path,
				  const QStringList &options)
	{
		QStringList args;
		args << QStringLiteral("--export-pdf") << project_path << pdf_path;
		args << options;
		return CLIExport::run(args);
	}

	/// The same, for the single option most cases below pass.
	int exportPdf(const QString &project_path, const QString &pdf_path,
				  const QString &option)
	{
		return exportPdf(project_path, pdf_path, QStringList() << option);
	}

	/**
		A page size as something a failure message can print: "1020.0x640.0".

		Compared as text on purpose. Catch2 has no way of showing a QSizeF,
		so a mismatch would read as two sizes it cannot print, and the case
		would say that a page is the wrong one without saying which one it
		is.
	*/
	QString sizeKey(const QSizeF &size)
	{
		return QStringLiteral("%1x%2")
			   .arg(size.width(), 0, 'f', 1)
			   .arg(size.height(), 0, 'f', 1);
	}

	/**
		Whether this build put a searchable font into @a pdf at all.

		Measured on the document at hand rather than assumed: the Qt5 build
		of this machine rasterises no glyph without a screen and embeds no
		font, so asserting the text of a page there would measure the
		platform and not the selection. Nothing is lost where this returns
		false - the page sizes prove the same order, and they are asserted
		everywhere.
	*/
	bool carriesText(const PdfProbe::Document &pdf)
	{
		for (int page = 1 ; page <= pdf.pageCount() ; ++page) {
			if (pdf.hasMappedFont(page)) {
				return true;
			}
		}
		return false;
	}

	/// What a section says when it steps aside for the reason above.
	const char *no_text_reason =
		"this build writes no embedded font into a PDF without a screen;"
		" the word of a sheet would measure the platform and not the"
		" folio selection";

	/// Whether page @a page of @a pdf shows the word of folio @a folio.
	bool shows(const PdfProbe::Document &pdf, int page, int folio)
	{
		return PdfProbe::squeezed(pdf.text(page))
			   .contains(PdfProbe::squeezed(anchorOf(folio)));
	}
}

TEST_CASE("T37 — a linha de comando exporta as folhas pedidas, na ordem pedida",
		  "[uibench][impressao]")
{
	QTemporaryDir work;
	REQUIRE(work.isValid());

	const QString project_path = work.filePath(QStringLiteral("bench.qet"));
	REQUIRE(writeFile(project_path, projectXml()));

	// The whole project comes out first, and it is two things at once: the
	// behaviour that has to stay exactly as it was for every script that
	// already calls this, and the ruler for everything below - it is where
	// each sheet says how big its own page is.
	const QString whole_path = work.filePath(QStringLiteral("whole.pdf"));
	REQUIRE(exportPdf(project_path, whole_path, QStringList()) == 0);
	PdfProbe::Document whole(whole_path);
	INFO(whole.error().toStdString());
	REQUIRE(whole.isValid());
	REQUIRE(whole.pageCount() == 3);

	QVector<QString> sheet_size;   // sheet_size.at(f - 1) belongs to folio f
	for (int page = 1 ; page <= whole.pageCount() ; ++page) {
		sheet_size << sizeKey(whole.pageSize(page));
	}
	// Every identification below rests on the three differing. A fixture
	// that stopped giving them different sizes would let any order pass.
	INFO("sheet sizes: " << sheet_size.at(0).toStdString() << ", "
		 << sheet_size.at(1).toStdString() << ", "
		 << sheet_size.at(2).toStdString());
	REQUIRE(sheet_size.at(0) != sheet_size.at(1));
	REQUIRE(sheet_size.at(1) != sheet_size.at(2));
	REQUIRE(sheet_size.at(0) != sheet_size.at(2));

	const QString cut_path = work.filePath(QStringLiteral("cut.pdf"));

	SECTION("sem o argumento, saem todas as folhas na ordem do projeto")
	{
		// The default is what every script written before the option
		// depends on, so it is held by the word each sheet carries and not
		// only by the count of pages.
		if (!carriesText(whole)) {
			SUCCEED(no_text_reason);
			return;
		}
		INFO("page 1: "
				 << PdfProbe::squeezed(whole.text(1)).toStdString());
		INFO("page 3: "
				 << PdfProbe::squeezed(whole.text(3)).toStdString());
		REQUIRE(shows(whole, 1, 1));
		REQUIRE(shows(whole, 2, 2));
		REQUIRE(shows(whole, 3, 3));
	}

	SECTION("folios=3,1 traz duas paginas, na ordem pedida e nao na natural")
	{
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=3,1")) == 0);

		PdfProbe::Document cut(cut_path);
		INFO(cut.error().toStdString());
		REQUIRE(cut.isValid());
		REQUIRE(cut.pageCount() == 2);
		REQUIRE(sizeKey(cut.pageSize(1)) == sheet_size.at(2));
		REQUIRE(sizeKey(cut.pageSize(2)) == sheet_size.at(0));

		if (carriesText(cut)) {
			INFO("page 1: "
					 << PdfProbe::squeezed(cut.text(1)).toStdString());
			INFO("page 2: "
					 << PdfProbe::squeezed(cut.text(2)).toStdString());
			REQUIRE(shows(cut, 1, 3));
			REQUIRE(shows(cut, 2, 1));
			REQUIRE_FALSE(shows(cut, 1, 2));
			REQUIRE_FALSE(shows(cut, 2, 2));
		}
	}

	SECTION("folios=2 traz so a folha do meio")
	{
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=2")) == 0);

		PdfProbe::Document cut(cut_path);
		INFO(cut.error().toStdString());
		REQUIRE(cut.isValid());
		REQUIRE(cut.pageCount() == 1);
		REQUIRE(sizeKey(cut.pageSize(1)) == sheet_size.at(1));

		if (carriesText(cut)) {
			INFO("page 1: "
					 << PdfProbe::squeezed(cut.text(1)).toStdString());
			REQUIRE(shows(cut, 1, 2));
			REQUIRE_FALSE(shows(cut, 1, 1));
			REQUIRE_FALSE(shows(cut, 1, 3));
		}
	}

	SECTION("a folha pedida duas vezes e recusada, e nada e escrito")
	{
		// One sheet is one page here, and the page map is keyed by sheet:
		// honouring the repetition would print a page no cross-reference
		// could ever reach. Refusing says so; printing it once would answer
		// a request that was not the one made.
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=3,3")) == 2);
		REQUIRE_FALSE(QFile::exists(cut_path));
	}

	SECTION("a folha que nao existe e recusada, e nada e escrito")
	{
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=4")) == 2);
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=0")) == 2);
		// And the one that matters most: a good folio beside a bad one.
		// Exporting the good half of the request would be a document of one
		// page where two were asked for.
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=2,9")) == 2);
		REQUIRE_FALSE(QFile::exists(cut_path));
	}

	SECTION("a opcao escrita errado e recusada, em vez de exportar tudo")
	{
		// The quiet one. An option the export does not know is a request it
		// cannot honour, and answering it with the whole project hands back
		// eighty sheets to whoever asked for three.
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folio=2")) == 2);
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=")) == 2);
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringLiteral("folios=two")) == 2);
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringList() << QStringLiteral("folios=1")
										<< QStringLiteral("folios=2")) == 2);
		REQUIRE_FALSE(QFile::exists(cut_path));
	}
}
