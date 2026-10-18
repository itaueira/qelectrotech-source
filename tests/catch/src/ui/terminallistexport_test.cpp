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

#include "../../../../sources/TerminalStrip/UndoCommand/sortterminalstripcommand.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripeditorwindow.h"
#include "../../../../sources/cli_export.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QVector>

/*
	The terminal list, as the terminal strip window writes it.

	The window has an action that writes the terminals of the strip
	selected in its tree - or of every strip, when the tree points at
	anything else - to a CSV file, through the collector and the writer the
	command line uses. What is held against it here is the file: one strip
	is that strip alone, in the order of its rail; every strip is the very
	file of --export-labels <project> <file> kinds=terminal, byte for byte;
	and a file that cannot be written is not written at all.

	The window itself is not built. TerminalStripEditorWindow reaches
	QETApp as soon as it is constructed - it looks for the diagram editor of
	its project, to put the undo and redo buttons on its tool bar - and no
	case of this bench has a QETApp; terminalnavigate_test.cpp says the
	same about its double click. So the work of the action lives in a
	static function of the window, TerminalStripEditorWindow::
	exportTerminalList(), and that is what these cases call: the choice
	between one strip and every strip, the collector, the writer and the
	failure. What stays out of reach is the window around it - which node
	of the tree gives which strip, the save dialog, and the line the status
	bar shows. That is why the cases are labelled T21 and not CU-21.17: the
	case of use is done in the window, and it ends at that status bar.

	The bench is one sheet of eight terminal symbols, and two strips built
	on purpose so that a wrong order cannot pass:

	- X1 holds five terminals in the order 2, 10, 1, 4, 3 - neither the
	  order they are drawn in, nor the alphabetical one, nor the natural
	  one. An export that walked the sheet or sorted the rail fails.
	- X2 holds three, 9, 7, 8, and is created first: the project holds X2
	  before X1 while the tree of the window lists X1 first, sorted by name.
	  An export of every strip that followed the tree, and not the project,
	  is a different file from the command line's, and fails.
*/

namespace
{
		//The folio number the sheet carries in its title block.
	const char *const sheet_folio = "21";

		//The tags the sheet draws, left to right.
	QStringList drawnTags()
	{
		return QStringList {QStringLiteral("1"), QStringLiteral("2"),
				    QStringLiteral("10"), QStringLiteral("3"),
				    QStringLiteral("4"), QStringLiteral("7"),
				    QStringLiteral("8"), QStringLiteral("9")};
	}

		//The rails, as the cases mount them.
	QStringList x1Rail()
	{
		return QStringList {QStringLiteral("2"), QStringLiteral("10"),
				    QStringLiteral("1"), QStringLiteral("4"),
				    QStringLiteral("3")};
	}
	QStringList x2Rail()
	{
		return QStringList {QStringLiteral("9"), QStringLiteral("7"),
				    QStringLiteral("8")};
	}

	/**
		One terminal symbol of the bench, as labelexport_test.cpp writes
		them: @a index unique over the project, its docking points 2 *
		index (east) and 2 * index + 1 (west), and the docking coordinates
		written as the arithmetic of Terminal::terminalSize.
	*/
	QString terminalXml(int index, int x, const QString &label)
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		return QStringLiteral(
			       "<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/terminal.elmt\""
			       " uuid=\"{c0ffee21-0017-4000-8000-0000000000%2}\">"
			       "<terminals>"
			       "<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
			       "<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
			       "</terminals>"
			       "<inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%7"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(x)
			.arg(index, 2, 10, QLatin1Char('0'))
			.arg(east_dock)
			.arg(index * 2)
			.arg(west_dock)
			.arg(index * 2 + 1)
			.arg(label);
	}

	/// A project of one sheet drawing the terminals of drawnTags(), and no strip.
	QString fixtureXml()
	{
		const QStringList tags = drawnTags();
		QString elements;
		for (int i = 0 ; i < tags.size() ; ++i) {
			elements += terminalXml(i, 120 + 60 * i, tags.at(i));
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"terminal.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"terminal\">"
			       "<names><name lang=\"en\">Terminal</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Sheet one\" order=\"1\" height=\"600\""
			       " folio=\"%1\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
			.arg(QString::fromLatin1(sheet_folio), elements);
	}

	/// The terminal symbol of @a diagram tagged @a label, nullptr when none.
	Element *terminalLabelled(Diagram *diagram, const QString &label)
	{
		if (!diagram) {
			return nullptr;
		}

		const auto elements = diagram->elements();
		for (Element *element : elements)
		{
			if (element->elementData().m_type == ElementData::Terminal
			    && element->actualLabel() == label) {
				return element;
			}
		}
		return nullptr;
	}

	/**
		A new strip named @a name, with the terminals of @a sheet tagged
		@a rail mounted one after the other, in that order. Null when one
		of them is not found or not taken.
	*/
	TerminalStrip *mountStrip(QETProject *project, Diagram *sheet,
				  const QString &name, const QStringList &rail)
	{
		TerminalStrip *strip =
				project->newTerminalStrip(QString(), QString(), name);
		if (!strip) {
			return nullptr;
		}

		for (const QString &tag : rail)
		{
			Element *element = terminalLabelled(sheet, tag);
			if (!element || !strip->addTerminal(element)) {
				return nullptr;
			}
		}
		return strip;
	}

	/**
		The project @a project holds now - its strips included - written to
		a file of its own, as labelexport_test.cpp does, so that the command
		line reads the strips back from a file and not from the memory of
		the bench.
	*/
	bool writeProject(QETProject *project, const QString &path)
	{
		const QByteArray content = project->toXml().toString(4).toUtf8();
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(content) == content.size();
	}

	/// The first row of the file, written here and not asked of the writer.
	QStringList expectedHeader()
	{
		return QStringList {QStringLiteral("kind"),
				    QStringLiteral("text"),
				    QStringLiteral("lines"),
				    QStringLiteral("strip"),
				    QStringLiteral("position"),
				    QStringLiteral("level"),
				    QStringLiteral("location"),
				    QStringLiteral("folio"),
				    QStringLiteral("revision"),
				    QStringLiteral("excluded_from_bom")};
	}

	int column(const QString &key)
	{
		return int(expectedHeader().indexOf(key));
	}

	/// What one export left at a path.
	struct CsvFile
	{
		bool exists = false;
		QByteArray bytes;
		bool byte_order_mark = false;
			/// The text behind the mark.
		QString text;
			/// The text cut at each CRLF, the last, empty, piece dropped.
		QStringList lines;
		QVector<QStringList> rows;
	};

	/**
		Read the file at @a path back.

		The reader is plain - a row ends at CRLF, a cell at ';' - because
		no cell of this bench needs quotes: no tag holds a separator, and no
		terminal has a wire or a cable that would put a line feed in a
		cell. The quoting is the writer's own bench, labelexport_test.cpp.
		isPlain() says whether that still holds, and every case requires it,
		so that a bench that grows such a cell fails there and not in a
		reader too simple for it.
	*/
	CsvFile readCsv(const QString &path)
	{
		CsvFile file;
		file.exists = QFile::exists(path);

		QFile csv(path);
		if (csv.open(QIODevice::ReadOnly)) {
			file.bytes = csv.readAll();
		}

		const QByteArray mark("\xEF\xBB\xBF", 3);
		file.byte_order_mark = file.bytes.startsWith(mark);
		file.text = QString::fromUtf8(file.byte_order_mark
					      ? file.bytes.mid(mark.size())
					      : file.bytes);

		file.lines = file.text.split(QStringLiteral("\r\n"));
		if (!file.lines.isEmpty() && file.lines.last().isEmpty()) {
			file.lines.removeLast();
		}
		for (const QString &line : std::as_const(file.lines)) {
			file.rows << line.split(QLatin1Char(';'));
		}
		return file;
	}

	/// True when readCsv() was enough for @a file: no quote, no line feed
	/// that does not end a row.
	bool isPlain(const CsvFile &file)
	{
		return !file.text.contains(QLatin1Char('"'))
			&& file.text.count(QLatin1Char('\n')) == file.lines.size()
			&& file.text.count(QLatin1Char('\r')) == file.lines.size();
	}

	/// The rows under the first one.
	QVector<QStringList> labelRows(const CsvFile &file)
	{
		return file.rows.mid(1);
	}

	/// One column of the rows under the first one, in their order.
	QStringList cells(const CsvFile &file, const QString &key)
	{
		QStringList values;
		const QVector<QStringList> rows = labelRows(file);
		for (const QStringList &row : rows) {
			values << row.value(column(key));
		}
		return values;
	}

	/// @a count times @a value.
	QStringList repeated(const QString &value, int count)
	{
		QStringList values;
		for (int i = 0 ; i < count ; ++i) {
			values << value;
		}
		return values;
	}

	/// The shape every terminal list has: the mark, the first row, and
	/// every row as wide as it.
	void checkShape(const CsvFile &file)
	{
		CHECK(file.byte_order_mark);
		REQUIRE(isPlain(file));
		REQUIRE_FALSE(file.rows.isEmpty());
		CHECK(file.rows.first() == expectedHeader());
		for (const QStringList &row : file.rows)
		{
			INFO("row " << row.join(QStringLiteral("|")).toStdString());
			CHECK(row.size() == expectedHeader().size());
		}
	}

	/// --export-labels <project_path> <csv_path> kinds=terminal, run in
	/// this process, and what it wrote.
	CsvFile exportFromCommandLine(const QString &project_path,
				      const QString &csv_path, int &code)
	{
		code = CLIExport::run(QStringList()
				      << QStringLiteral("--export-labels")
				      << project_path << csv_path
				      << QStringLiteral("kinds=terminal"));
		return readCsv(csv_path);
	}

	/// The project of every case: the sheet, X2 mounted, then X1.
	struct Rails
	{
		TerminalStrip *x1 = nullptr;
		TerminalStrip *x2 = nullptr;
	};

	Rails mountRails(UiBench::ScratchProject &bench)
	{
		Rails rails;
		Diagram *sheet = bench.diagram(0);
		if (!sheet) {
			return rails;
		}
		rails.x2 = mountStrip(bench.project(), sheet,
				      QStringLiteral("X2"), x2Rail());
		rails.x1 = mountStrip(bench.project(), sheet,
				      QStringLiteral("X1"), x1Rail());
		return rails;
	}
}

TEST_CASE("T21 — the terminal list of one strip is that strip alone, in the order of its rail",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-list-one.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	const Rails rails = mountRails(bench);
	REQUIRE(rails.x1 != nullptr);
	REQUIRE(rails.x2 != nullptr);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("x1.csv"));

	int written = -1;
	const QString failure = TerminalStripEditorWindow::exportTerminalList(
				bench.project(), rails.x1, path, &written);
	INFO("failure: " << failure.toStdString());
	REQUIRE(failure.isEmpty());

	const CsvFile file = readCsv(path);
	INFO(file.text.toStdString());
	REQUIRE(file.exists);
	checkShape(file);

		//Five rows, the count the window reports with them, and the order of
		//the rail - not the order of the sheet, not a sorted one.
	REQUIRE(labelRows(file).size() == 5);
	CHECK(written == 5);
	CHECK(cells(file, QStringLiteral("text")) == x1Rail());
		//Counted from 1, as the Position column of the strip editor.
	CHECK(cells(file, QStringLiteral("position"))
	      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
			      QStringLiteral("3"), QStringLiteral("4"),
			      QStringLiteral("5")}));
	CHECK(cells(file, QStringLiteral("strip"))
	      == repeated(QStringLiteral("X1"), 5));
	CHECK(cells(file, QStringLiteral("kind"))
	      == repeated(QStringLiteral("terminal"), 5));
		//The folio as the sheet draws it, read and not assumed, and required
		//not to be empty: a cell the comparison below can see.
	const QString drawn_folio =
			bench.diagram(0)->border_and_titleblock.finalfolio();
	REQUIRE_FALSE(drawn_folio.isEmpty());
	CHECK(cells(file, QStringLiteral("folio")) == repeated(drawn_folio, 5));

		//And it is the command line's file with the rows of the other strip
		//taken out, byte for byte: the first row, then the X1 rows of that
		//file, in their order, each ended as that file ends it.
	const QString project_path = dir.filePath(QStringLiteral("one.qet"));
	REQUIRE(writeProject(bench.project(), project_path));
	int code = -1;
	const CsvFile every = exportFromCommandLine(
				project_path, dir.filePath(QStringLiteral("cli.csv")),
				code);
	INFO("command line:\n" << every.text.toStdString());
	REQUIRE(code == 0);
	checkShape(every);
	REQUIRE(labelRows(every).size() == 8);

	QString expected_text = every.lines.first() + QStringLiteral("\r\n");
	for (int i = 1 ; i < every.lines.size() ; ++i)
	{
		if (every.rows.at(i).value(column(QStringLiteral("strip")))
		    == QLatin1String("X1")) {
			expected_text += every.lines.at(i) + QStringLiteral("\r\n");
		}
	}
	const QByteArray expected = QByteArray("\xEF\xBB\xBF", 3)
			+ expected_text.toUtf8();
	CHECK(file.bytes == expected);
}

TEST_CASE("T21 — the terminal list of every strip is the file of --export-labels kinds=terminal",
	  "[t21][label][terminalstrip][cli]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-list-every.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	const Rails rails = mountRails(bench);
	REQUIRE(rails.x1 != nullptr);
	REQUIRE(rails.x2 != nullptr);

		//The precondition of the probe on the order of the strips: the
		//project holds X2 first, and a sort by name - the tree's - puts X1
		//first.
	REQUIRE(bench->terminalStrip().size() == 2);
	REQUIRE(bench->terminalStrip().first() == rails.x2);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());

		//From the window, over the project the bench holds open, with no
		//strip: every strip.
	const QString path = dir.filePath(QStringLiteral("every.csv"));
	int written = -1;
	const QString failure = TerminalStripEditorWindow::exportTerminalList(
				bench.project(), nullptr, path, &written);
	INFO("failure: " << failure.toStdString());
	REQUIRE(failure.isEmpty());

	const CsvFile file = readCsv(path);
	INFO("window:\n" << file.text.toStdString());
	REQUIRE(file.exists);
	checkShape(file);

		//The literal probes beside the comparison below, so that two empty
		//lists cannot make it trivially true: eight rows, the strips in the
		//order of the project and each in the order of its rail.
	REQUIRE(labelRows(file).size() == 8);
	CHECK(written == 8);
	CHECK(cells(file, QStringLiteral("text")) == x2Rail() + x1Rail());
	CHECK(cells(file, QStringLiteral("strip"))
	      == repeated(QStringLiteral("X2"), 3)
		 + repeated(QStringLiteral("X1"), 5));

		//And from the command line, out of the saved file alone.
	const QString project_path = dir.filePath(QStringLiteral("every.qet"));
	REQUIRE(writeProject(bench.project(), project_path));
	int code = -1;
	const CsvFile cli = exportFromCommandLine(
				project_path, dir.filePath(QStringLiteral("cli.csv")),
				code);
	INFO("command line:\n" << cli.text.toStdString());
	REQUIRE(code == 0);
	REQUIRE(cli.exists);

	CHECK(file.bytes == cli.bytes);
}

TEST_CASE("T21 — the terminal list is what the strip holds when it is exported",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-list-applied.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	const Rails rails = mountRails(bench);
	REQUIRE(rails.x1 != nullptr);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());

		//Sorted as the Sort button of the strip editor sorts: the command
		//goes on the undo stack of the project, so it is applied, and the
		//next export has to follow it - no list is kept from one export to
		//the next.
	bench->undoStack()->push(new SortTerminalStripCommand(rails.x1));

	const QString sorted_path = dir.filePath(QStringLiteral("sorted.csv"));
	REQUIRE(TerminalStripEditorWindow::exportTerminalList(
			bench.project(), rails.x1, sorted_path).isEmpty());
	const CsvFile sorted = readCsv(sorted_path);
	INFO(sorted.text.toStdString());
	checkShape(sorted);
	CHECK(cells(sorted, QStringLiteral("text"))
	      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
			      QStringLiteral("3"), QStringLiteral("4"),
			      QStringLiteral("10")}));

		//Undone, the rail is the one mounted, and so is the file.
	bench->undoStack()->undo();

	const QString undone_path = dir.filePath(QStringLiteral("undone.csv"));
	REQUIRE(TerminalStripEditorWindow::exportTerminalList(
			bench.project(), rails.x1, undone_path).isEmpty());
	const CsvFile undone = readCsv(undone_path);
	INFO(undone.text.toStdString());
	checkShape(undone);
	CHECK(cells(undone, QStringLiteral("text")) == x1Rail());
}

TEST_CASE("T21 — a terminal list that cannot be written leaves no file, and says why",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-list-refused.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	const Rails rails = mountRails(bench);
	REQUIRE(rails.x1 != nullptr);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString missing_folder = dir.filePath(QStringLiteral("missing"));
	const QString missing = missing_folder + QStringLiteral("/terminals.csv");

	SECTION("one strip, into a folder that does not exist")
	{
		int written = -1;
		const QString failure = TerminalStripEditorWindow::exportTerminalList(
					bench.project(), rails.x1, missing, &written);
		CHECK_FALSE(failure.isEmpty());
		CHECK(written == 0);
		CHECK_FALSE(QFile::exists(missing));
		CHECK_FALSE(QDir(missing_folder).exists());
	}

	SECTION("every strip, into a folder that does not exist")
	{
		int written = -1;
		const QString failure = TerminalStripEditorWindow::exportTerminalList(
					bench.project(), nullptr, missing, &written);
		CHECK_FALSE(failure.isEmpty());
		CHECK(written == 0);
		CHECK_FALSE(QFile::exists(missing));
		CHECK_FALSE(QDir(missing_folder).exists());
	}

	SECTION("no project and no strip: the reason, and nothing written")
	{
		const QString path = dir.filePath(QStringLiteral("none.csv"));
		int written = -1;
		const QString failure = TerminalStripEditorWindow::exportTerminalList(
					nullptr, nullptr, path, &written);
		CHECK_FALSE(failure.isEmpty());
		CHECK(written == 0);
		CHECK_FALSE(QFile::exists(path));
	}
}
