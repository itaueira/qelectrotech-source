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
#include "../../../../sources/pdf_links.h"

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QChar>
#include <QFile>
#include <QList>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

/*
	The navigation panel of an emitted drawing.

	A project of fourteen sheets leaves this program as fourteen pages and
	nothing else: a reader opens it on page one, and the only way to the sheet
	somebody is asking about is to scroll. The jumps inside the document
	already work - a cross-reference lands on its target - but a jump only
	serves whoever has already found the reference to click on.

	What is asserted is the file and not the call, for the same reason the
	folio selection is: nobody who opens the document was there when it was
	written. And it is asserted over the raw bytes rather than through a PDF
	library, because none is linked here and bringing one in to prove a tree
	this small would cost more than the tree.

	Two of the assertions carry the weight. A tree whose entries all pointed
	at page one would list the right sheets and open the wrong one; a tree
	whose /Next chain broke halfway would show two of the three. Both are what
	a reader follows, and neither is visible to "the file contains the word
	/Outlines" - which is all the absence was measured by.
*/

namespace
{
	/// The title written on sheet @a folio of the fixture, and on no other.
	QString titleOf(int folio)
	{
		switch (folio) {
			case 1:
				return QStringLiteral("Alimentation");
			case 2:
				// Written as explicit UTF-8 bytes rather than as an accented
				// literal: what is proved here is the encoding the bookmark
				// is written in, and that proof must not rest on how the
				// compiler read this file.
				return QString::fromUtf8("Commande g\xC3\xA9n\xC3\xA9rale");
			case 3:
				// The parentheses are the point: they delimit a plain PDF
				// string, so a title carrying them has to be written some
				// other way or it closes the string early.
				return QStringLiteral("Puissance (400 V)");
			default:
				return QString();
		}
	}

	/**
		One sheet of the fixture, sized so that no other sheet shares its size.

		Same reason as in the folio-selection bench: the page a sheet becomes
		is as big as its frame, so distinct column and row counts are what
		lets a page of the emitted file say which sheet it is.
	*/
	QString sheetXml(int order, int columns, int rows)
	{
		return QStringLiteral(
				   "<diagram title=\"%4\" order=\"%1\""
				   " height=\"600\""
				   " cols=\"%2\" colsize=\"60\""
				   " rows=\"%3\" rowsize=\"80\""
				   " displaycols=\"true\" displayrows=\"true\">"
				   "<elements/>"
				   "<inputs/>"
				   "<conductors/>"
				   "</diagram>")
			   .arg(order).arg(columns).arg(rows).arg(titleOf(order));
	}

	/// A three-sheet project, each sheet with its own size and its own title.
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

	/**
		@return false when @a content could not be written to @a path.

		Written as UTF-8 bytes and not through a text stream: one of the three
		titles is accented, and a stream would encode it in whatever the
		locale of the machine happens to be, while the reader of the file
		assumes UTF-8.
	*/
	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		const QByteArray utf8 = content.toUtf8();
		const bool written = (file.write(utf8) == utf8.size());
		file.close();
		return written;
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

	/// The whole of @a path, or an empty array when it cannot be read.
	QByteArray bytesOf(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QByteArray();
		}
		const QByteArray data = file.readAll();
		file.close();
		return data;
	}

	/// A PDF of @a pages drawn pages carrying no outline at all.
	bool writeBlankPdf(const QString &path, int pages)
	{
		QPdfWriter writer(path);
		writer.setPageSize(QPageSize(QPageSize::A4));
		QPainter painter;
		if (!painter.begin(&writer)) {
			return false;
		}
		for (int i = 0 ; i < pages ; ++i) {
			if (i > 0) {
				writer.newPage();
			}
			// Something has to be drawn: an untouched page is a page the
			// engine may decide it has nothing to write about.
			painter.drawLine(10, 10, 200, 200);
		}
		painter.end();
		return true;
	}

	/// The object number of every page of @a data, in document order.
	QVector<int> pageObjectsOf(const QByteArray &data)
	{
		QVector<int> pages;
		const int at = data.indexOf("/Type /Pages");
		const int kids = (at == -1) ? -1 : data.indexOf("/Kids", at);
		const int lb = (kids == -1) ? -1 : data.indexOf('[', kids);
		const int rb = (lb == -1) ? -1 : data.indexOf(']', lb);
		if (lb == -1 || rb == -1 || rb <= lb) {
			return pages;
		}
		const QList<QByteArray> words =
			data.mid(lb + 1, rb - lb - 1).simplified().split(' ');
		for (int i = 0 ; i + 2 < words.size() ; ++i) {
			if (words.at(i + 2) == "R") {
				pages << words.at(i).toInt();
			}
		}
		return pages;
	}

	/// The bytes of object @a number, between its header and its "endobj".
	QByteArray objectBody(const QByteArray &data, int number)
	{
		const QByteArray head =
			"\n" + QByteArray::number(number) + " 0 obj";
		const int at = data.indexOf(head);
		if (at == -1) {
			return QByteArray();
		}
		const int end = data.indexOf("endobj", at);
		if (end == -1) {
			return QByteArray();
		}
		return data.mid(at + head.size(), end - at - head.size());
	}

	/// The object number "/<key> N 0 R" names in @a dict, or 0.
	int referenceOf(const QByteArray &dict, const QByteArray &key)
	{
		const QByteArray needle = "/" + key + " ";
		const int at = dict.indexOf(needle);
		if (at == -1) {
			return 0;
		}
		return dict.mid(at + needle.size(), 24).simplified()
			   .split(' ').value(0).toInt();
	}

	/// The page object "/Dest [N 0 R /Fit]" opens in @a dict, or 0.
	int destinationOf(const QByteArray &dict)
	{
		const QByteArray needle = "/Dest [";
		const int at = dict.indexOf(needle);
		if (at == -1) {
			return 0;
		}
		return dict.mid(at + needle.size(), 24).simplified()
			   .split(' ').value(0).toInt();
	}

	/**
		The text of "/Title <FEFF...>" in @a dict, decoded.

		Decoded and not compared as hexadecimal on purpose: what has to be
		true is that the title a person typed comes back out of the file, and
		comparing two hexadecimal strings would say nothing about which of the
		two is the wrong one when they differ.
	*/
	QString titleIn(const QByteArray &dict)
	{
		const QByteArray needle = "/Title <";
		const int at = dict.indexOf(needle);
		if (at == -1) {
			return QString();
		}
		const int end = dict.indexOf('>', at);
		if (end == -1) {
			return QString();
		}
		const QByteArray raw =
			QByteArray::fromHex(dict.mid(at + needle.size(),
										 end - at - needle.size()));
		// Without the byte-order mark a reader decodes the bytes as
		// PDFDocEncoding, and every accented title comes out mangled.
		if (raw.size() < 2
			|| static_cast<uchar>(raw.at(0)) != 0xFE
			|| static_cast<uchar>(raw.at(1)) != 0xFF) {
			return QString();
		}
		QString text;
		for (int i = 2 ; i + 1 < raw.size() ; i += 2) {
			text.append(QChar(static_cast<ushort>(
								  (static_cast<uchar>(raw.at(i)) << 8)
								  | static_cast<uchar>(raw.at(i + 1)))));
		}
		return text;
	}

	/// One line of the bookmark panel, as read back out of the file.
	struct Bookmark
	{
		QString title;
		/// The object number of the page the entry opens.
		int destination = 0;
	};

	/// The object number of the outline root of @a data, or 0.
	int outlineRootOf(const QByteArray &data)
	{
		const int marker = data.indexOf("/Type /Outlines");
		if (marker == -1) {
			return 0;
		}
		const int head = data.lastIndexOf(" 0 obj", marker);
		if (head == -1) {
			return 0;
		}
		int start = head;
		while (start > 0 && data.at(start - 1) >= '0'
			   && data.at(start - 1) <= '9') {
			--start;
		}
		return data.mid(start, head - start).toInt();
	}

	/**
		The bookmarks of @a data, in the order the tree chains them.

		Followed along /First and /Next rather than collected by scanning the
		file for "/Title": the chain is what a reader walks, so a tree whose
		links are wrong has to come back short here instead of coming back
		complete in the order the objects happen to sit in.
	*/
	QList<Bookmark> bookmarksOf(const QByteArray &data)
	{
		QList<Bookmark> found;
		const int root = outlineRootOf(data);
		if (root <= 0) {
			return found;
		}

		int current = referenceOf(objectBody(data, root), "First");
		// Bounded so that a tree pointing back at itself fails by count
		// instead of hanging the suite.
		for (int guard = 0 ; current > 0 && guard < 1000 ; ++guard) {
			const QByteArray item = objectBody(data, current);
			if (item.isEmpty()) {
				break;
			}
			Bookmark entry;
			entry.title = titleIn(item);
			entry.destination = destinationOf(item);
			found << entry;
			current = referenceOf(item, "Next");
		}
		return found;
	}

	/// Where the last "startxref" of @a data says the table is, or -1.
	int startxrefOf(const QByteArray &data)
	{
		const int at = data.lastIndexOf("startxref");
		if (at == -1) {
			return -1;
		}
		return data.mid(at + 9, 32).simplified().split(' ').value(0).toInt();
	}

	/**
		The offset the table at @a table gives for object @a number, or -1.

		The table is positional: "xref", then a line saying which object the
		section starts at and how many it covers, then exactly twenty bytes
		per object. That fixed width is the whole reason a table can be read
		at all, and it is also what makes a missing line shift every object
		after it.
	*/
	int xrefOffsetOf(const QByteArray &data, int table, int number)
	{
		const int first_line = data.indexOf('\n', table);
		if (first_line == -1) {
			return -1;
		}
		const int second_line = data.indexOf('\n', first_line + 1);
		if (second_line == -1) {
			return -1;
		}
		const int entry = second_line + 1 + number * 20;
		if (entry + 10 > data.size()) {
			return -1;
		}
		return data.mid(entry, 10).toInt();
	}
}

TEST_CASE("T37 — o PDF emitido leva um sumario, um marcador por folha",
		  "[uibench][impressao]")
{
	QTemporaryDir work;
	REQUIRE(work.isValid());

	const QString project_path = work.filePath(QStringLiteral("bench.qet"));
	REQUIRE(writeFile(project_path, projectXml()));

	const QString whole_path = work.filePath(QStringLiteral("whole.pdf"));
	REQUIRE(exportPdf(project_path, whole_path, QStringList()) == 0);

	const QByteArray whole = bytesOf(whole_path);
	REQUIRE_FALSE(whole.isEmpty());

	SECTION("as tres folhas viram tres marcadores, na ordem do projeto")
	{
		// The measurement that opened this step: the emitted file had
		// /Annots and /GoTo, so the jumps worked, and no /Outlines at all.
		REQUIRE(whole.contains("/Type /Outlines"));

		const QVector<int> pages = pageObjectsOf(whole);
		REQUIRE(pages.size() == 3);

		const QList<Bookmark> marks = bookmarksOf(whole);
		REQUIRE(marks.size() == 3);

		for (int i = 0 ; i < marks.size() ; ++i) {
			INFO("bookmark " << (i + 1) << ": "
				 << marks.at(i).title.toStdString());
			REQUIRE(marks.at(i).title == titleOf(i + 1));
			// The one that matters: a tree naming the right sheets and
			// opening the wrong pages reads as correct until it is clicked.
			REQUIRE(marks.at(i).destination == pages.at(i));
		}
	}

	SECTION("o titulo acentuado e o que tem parenteses chegam inteiros")
	{
		const QList<Bookmark> marks = bookmarksOf(whole);
		REQUIRE(marks.size() == 3);

		// A plain PDF string is delimited by parentheses, so sheet 3 is what
		// says the title is not being written as one; and sheet 2 is what
		// says the bytes carry a byte-order mark, since titleIn() refuses to
		// decode without one.
		REQUIRE(marks.at(1).title == titleOf(2));
		REQUIRE(marks.at(2).title == titleOf(3));
		REQUIRE(marks.at(2).title.contains(QLatin1Char(')')));
	}

	SECTION("o leitor e mandado abrir o painel, em vez de deixa-lo escondido")
	{
		// A tree nobody sees is a tree nobody has. If this ever fails
		// because the engine started writing a /PageMode of its own, the
		// answer is to replace that key and not to leave it alone.
		REQUIRE(whole.contains("/PageMode /UseOutlines"));

		const int root = outlineRootOf(whole);
		REQUIRE(root > 0);
		REQUIRE(whole.contains("/Outlines " + QByteArray::number(root)
							   + " 0 R"));
		REQUIRE(objectBody(whole, root).contains("/Count 3"));
	}

	SECTION("a tabela reconstruida endereca os objetos novos corretamente")
	{
		// The probe reads the objects and deliberately ignores the table, so
		// nothing else in this suite would notice a table off by one entry -
		// and the table is the only thing a real reader uses.
		const int table = startxrefOf(whole);
		REQUIRE(table > 0);
		REQUIRE(whole.mid(table, 4) == QByteArray("xref"));

		const int root = outlineRootOf(whole);
		REQUIRE(root > 0);
		const int offset = xrefOffsetOf(whole, table, root);
		REQUIRE(offset > 0);
		REQUIRE(whole.mid(offset, 32)
				.startsWith(QByteArray::number(root) + " 0 obj"));
	}

	SECTION("o documento continua legivel depois de reescrito")
	{
		PdfProbe::Document pdf(whole_path);
		INFO(pdf.error().toStdString());
		REQUIRE(pdf.isValid());
		REQUIRE(pdf.pageCount() == 3);
	}

	SECTION("folios=3,1 leva dois marcadores, na ordem pedida")
	{
		const QString cut_path = work.filePath(QStringLiteral("cut.pdf"));
		REQUIRE(exportPdf(project_path, cut_path,
						  QStringList()
							  << QStringLiteral("folios=3,1")) == 0);

		const QByteArray cut = bytesOf(cut_path);
		REQUIRE_FALSE(cut.isEmpty());

		const QVector<int> pages = pageObjectsOf(cut);
		REQUIRE(pages.size() == 2);

		const QList<Bookmark> marks = bookmarksOf(cut);
		REQUIRE(marks.size() == 2);
		// Narrowed and reordered: the panel has to say what the document
		// holds, not what the project holds.
		REQUIRE(marks.at(0).title == titleOf(3));
		REQUIRE(marks.at(1).title == titleOf(1));
		REQUIRE(marks.at(0).destination == pages.at(0));
		REQUIRE(marks.at(1).destination == pages.at(1));
	}
}

TEST_CASE("T37 — o sumario recusa o que tornaria o arquivo ambiguo",
		  "[uibench][impressao]")
{
	QTemporaryDir work;
	REQUIRE(work.isValid());

	const QString path = work.filePath(QStringLiteral("blank.pdf"));
	REQUIRE(writeBlankPdf(path, 2));

	SECTION("um segundo sumario nao e escrito por cima do primeiro")
	{
		PdfLinks::OutlineEntry first;
		first.title = QStringLiteral("Un");
		first.page = 1;

		PdfLinks::injectOutline(path,
								QList<PdfLinks::OutlineEntry>() << first);
		const QByteArray once = bytesOf(path);
		REQUIRE(once.contains("/Type /Outlines"));
		REQUIRE(bookmarksOf(once).size() == 1);

		PdfLinks::OutlineEntry second;
		second.title = QStringLiteral("Deux");
		second.page = 2;
		PdfLinks::injectOutline(path,
								QList<PdfLinks::OutlineEntry>() << second);

		const QByteArray twice = bytesOf(path);
		// Two trees would leave the catalog with two /Outlines keys, and a
		// reader would follow whichever it parsed last - so the second call
		// has to do nothing at all rather than half of something.
		const int first_at = twice.indexOf("/Type /Outlines");
		REQUIRE(first_at != -1);
		REQUIRE(twice.indexOf("/Type /Outlines", first_at + 1) == -1);
		REQUIRE(twice == once);
	}

	SECTION("marcador para pagina que o documento nao tem e descartado")
	{
		PdfLinks::OutlineEntry good;
		good.title = QStringLiteral("Un");
		good.page = 1;
		PdfLinks::OutlineEntry beyond;
		beyond.title = QStringLiteral("Neuf");
		beyond.page = 9;

		PdfLinks::injectOutline(path, QList<PdfLinks::OutlineEntry>()
									  << good << beyond);

		const QByteArray data = bytesOf(path);
		const QList<Bookmark> marks = bookmarksOf(data);
		// Kept quiet rather than written: an entry pointing past the last
		// page opens nothing, and a panel with a dead line in it is worse
		// than a panel with one line fewer.
		REQUIRE(marks.size() == 1);
		REQUIRE(marks.at(0).title == QStringLiteral("Un"));
	}
}
