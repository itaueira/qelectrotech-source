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
#include "../../../../sources/TerminalStrip/physicalterminal.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/autoNum/iecstructure.h"
#include "../../../../sources/cli_export.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/label/componentlabelcollector.h"
#include "../../../../sources/label/labelcsvwriter.h"
#include "../../../../sources/label/terminallabelcollector.h"
#include "../../../../sources/location/locationtree.h"
#include "../../../../sources/location/projectlocation.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/titleblockproperties.h"
#include "../../../../sources/undocommand/assignlocationcommand.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QDomDocument>
#include <QFile>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QVector>

/*
	The label file, as the command line writes it from a project file.

	The writer has a bench of its own in the pure suite, with lists built by
	hand. What that bench cannot see is the road to it: the strips read back
	from a file by a process that never opened a window, the components
	read from the data base of that project, and the file a shop opens.
	That is what is asserted here, and the file is read back by a reader
	written in this file, against a first row written here too.

	Each export runs on a project file of its own, written from the bench
	by QETProject::toXml() - the strips mounted in the bench go into it -,
	and never on the file the bench has open: opening one file twice in a
	process is a question these cases do not ask.

	Three cases carry the number of their use case, because they prove it
	whole:

	- CU-21.11: a ';' in the tag of a component and in the function of a
	  terminal symbol stays in its cell, quoted, and every row is as wide as
	  the first. The function of a terminal reaches the file on the row the
	  data base gives that symbol as a component; a ';' in a field of the
	  terminal row itself is checked too, in the cable of a terminal.
	- CU-21.12: exported, sorted the way the Sort button of the strip editor
	  sorts - TerminalStripEditor::on_m_auto_ordering_pb_clicked() pushes
	  that very command -, exported again: the first file is the order of
	  the rail, the second the new order, and the position column travels
	  with each label.
	- CU-21.14: a component and a terminal given a place of the location
	  tree, by the command the location manager pushes, come out with the
	  mark of the norm in the location column - +QCM1+PLACA -, and the file
	  holds neither the path they store nor the names the tree gives them.

	CU-21.16 is not claimed: it ends at the file the window produces for the
	same selection, and no window exports labels yet. What is proved instead
	is that the command line writes, byte for byte, what the collectors and
	the writer give inside the program for the same project. CU-21.8 ends at
	a printed label; the bytes of its accents are checked, nothing more.
*/

namespace
{
		//The folio number each sheet carries in its title block. Not 1 and
		//2, which are the positions of the sheets in the project.
	const char *const sheet_one_folio = "11";
	const char *const sheet_two_folio = "12";

	/**
		One terminal symbol of the bench.

		@param index : unique over the whole project. Its docking points are
		2 * index (east) and 2 * index + 1 (west), and its uuid ends in it.

		The docking coordinates are written as the arithmetic, as
		terminallabel_test.cpp does: the coordinates of an instance are
		offset by Terminal::terminalSize, and the bench should not need
		editing the day that size changes.
	*/
	QString terminalXml(int index, int x, const QString &label,
			    const QString &information = QString())
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		return QStringLiteral(
			       "<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/terminal.elmt\""
			       " uuid=\"{c0ffee21-0009-4000-8000-0000000000%2}\">"
			       "<terminals>"
			       "<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
			       "<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
			       "</terminals>"
			       "<inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%7"
			       "</elementInformation>"
			       "%8"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(x)
			.arg(index, 2, 10, QLatin1Char('0'))
			.arg(east_dock)
			.arg(index * 2)
			.arg(west_dock)
			.arg(index * 2 + 1)
			.arg(label, information);
	}

	/// One component of the bench: a symbol with no terminal, on a sheet.
	QString componentXml(int index, int x, const QString &label,
			     const QString &information = QString())
	{
		return QStringLiteral(
			       "<element x=\"%1\" y=\"400\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/component.elmt\""
			       " uuid=\"{c0ffee21-000a-4000-8000-0000000000%2}\">"
			       "<terminals/><inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%3"
			       "</elementInformation>"
			       "%4"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(x)
			.arg(index, 2, 10, QLatin1Char('0'))
			.arg(label, information);
	}

	/// One more piece of information of a symbol, as XML.
	QString informationXml(const QString &key, const QString &value)
	{
		return QStringLiteral("<elementInformation show=\"0\" name=\"%1\">%2"
				      "</elementInformation>")
			.arg(key, value);
	}

		//The texts of the bench that have to survive the trip to the file
		//as they are: a separator, and the accents of CU-21.8.
	QString separatorTag() {return QStringLiteral("Q1;Q2");}
	QString cableWithSeparator() {return QStringLiteral("W1;W2");}
	QString accentedDesignation()
	{
		return QString::fromUtf8("V\xC3\xA1lvula de press\xC3\xA3o");
	}
	QString accentedFunction()
	{
		return QString::fromUtf8("Emerg\xC3\xAAncia");
	}

	/**
		A project of two folios: five terminal symbols and two components.

		Sheet one draws the terminals 1, 2 and 10, left to right, and the
		component tagged Q1;Q2. 2 and 10 share the wire W7; 1 has no wire,
		and instead a cable whose name holds a ';', with a wire of that
		cable. 2 carries a function holding a ';' - the terminal of CU-21.11.
		Sheet two draws the terminals 3 and 4, and the component K2, whose
		designation and function carry the accents of CU-21.8.

		The file carries no strip: every case mounts the rail it needs, so
		the order of the rail is the case's own choice and is stated where
		it is checked.
	*/
	QString fixtureXml()
	{
		const QString one_information =
				informationXml(RealTerminal::cableInfoKey(),
					       cableWithSeparator())
				+ informationXml(RealTerminal::cableWireInfoKey(),
						 QStringLiteral("BK"));
		const QString two_information =
				informationXml(QStringLiteral("function"),
					       separatorTag());
		const QString k2_information =
				informationXml(QStringLiteral("designation"),
					       accentedDesignation())
				+ informationXml(QStringLiteral("function"),
						 accentedFunction());

		const QString sheet_one =
				terminalXml(0, 240, QStringLiteral("1"), one_information)
				+ terminalXml(1, 300, QStringLiteral("2"), two_information)
				+ terminalXml(2, 360, QStringLiteral("10"))
				+ componentXml(0, 600, separatorTag());

			//From the east side of 2 (id 2) to the west side of 10 (id 5).
		const QString sheet_one_wires = QStringLiteral(
				"<conductor terminal1=\"2\" terminal2=\"5\""
				" num=\"W7\" displaytext=\"1\" type=\"multi\""
				" condsize=\"1\"/>");

		const QString sheet_two =
				terminalXml(3, 240, QStringLiteral("3"))
				+ terminalXml(4, 300, QStringLiteral("4"))
				+ componentXml(1, 600, QStringLiteral("K2"), k2_information);

		const QString diagram = QStringLiteral(
				"<diagram title=\"%1\" order=\"%2\" height=\"600\""
				" folio=\"%3\""
				" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
				" displaycols=\"true\" displayrows=\"true\">"
				"<elements>%4</elements>"
				"<inputs/>"
				"<conductors>%5</conductors>"
				"</diagram>");

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
			       "<element name=\"component.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Component</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>")
		       + diagram.arg(QStringLiteral("Sheet one"), QStringLiteral("1"),
				     QString::fromLatin1(sheet_one_folio), sheet_one,
				     sheet_one_wires)
		       + diagram.arg(QStringLiteral("Sheet two"), QStringLiteral("2"),
				     QString::fromLatin1(sheet_two_folio), sheet_two,
				     QString())
		       + QStringLiteral("</project>");
	}

		//How many symbols the bench draws, counted from fixtureXml() above:
		//the data base publishes the terminal symbols among the components,
		//so every one of the seven is a component row. Written as a literal
		//so that a bench that shrinks says so here.
	const int drawn_symbols = 7;

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

	/// The symbol of @a diagram tagged @a label, of any kind; nullptr when none.
	Element *elementLabelled(Diagram *diagram, const QString &label)
	{
		if (!diagram) {
			return nullptr;
		}

		const auto elements = diagram->elements();
		for (Element *element : elements)
		{
			if (element->actualLabel() == label) {
				return element;
			}
		}
		return nullptr;
	}

	/**
		A new strip named @a name, with the terminals tagged @a labels
		mounted one after the other, in that order. Null when one of them
		is not found or not taken.
	*/
	TerminalStrip *mountStrip(QETProject *project, const QString &name,
				  const QList<QPair<Diagram *, QString>> &terminals)
	{
		TerminalStrip *strip =
				project->newTerminalStrip(QString(), QString(), name);
		if (!strip) {
			return nullptr;
		}

		for (const auto &terminal : terminals)
		{
			Element *element = terminalLabelled(terminal.first,
							    terminal.second);
			if (!element || !strip->addTerminal(element)) {
				return nullptr;
			}
		}
		return strip;
	}

	/// The tags of the terminals of @a strip, in the order of its rail.
	QStringList labelsOnRail(TerminalStrip *strip)
	{
		QStringList labels;
		const auto real_terminals = strip->realTerminals();
		for (const auto &real_terminal : real_terminals) {
			labels << real_terminal->label();
		}
		return labels;
	}

	/// Give a sheet a revision index, as the title block dialog would.
	void setRevision(Diagram *sheet, const QString &revision)
	{
		TitleBlockProperties properties =
				sheet->border_and_titleblock.exportTitleBlock();
		properties.indexrev = revision;
		sheet->border_and_titleblock.importTitleBlock(properties);
	}

	bool writeFile(const QString &path, const QByteArray &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(content) == content.size();
	}

	/**
		Write the project @a project holds now - its strips included - to a
		file of its own, through the function a save writes it with.
	*/
	bool writeProject(QETProject *project, const QString &path)
	{
		return writeFile(path, project->toXml().toString(4).toUtf8());
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

	/// What one run of --export-labels left behind.
	struct LabelFile
	{
		int code = -1;
		bool written = false;
		QByteArray bytes;
			/// The text behind the byte order mark.
		QString text;
		bool byte_order_mark = false;
		QVector<QStringList> rows;
		int crlf_ends = 0;
		int other_ends = 0;
	};

	/**
		RFC 4180 with ';' between the cells, written here so that a defect
		of the writer cannot be matched by the same defect in the reader. A
		cell between double quotes may hold the separator, a doubled quote
		and an end of line; an end of line outside quotes ends the row.
	*/
	void parse(LabelFile &file)
	{
		const QString &text = file.text;
		QStringList row;
		QString cell;
		bool quoted = false;
		bool pending = false;

		for (int i = 0 ; i < text.size() ; ++i)
		{
			const QChar c = text.at(i);
			if (quoted)
			{
				if (c == QLatin1Char('"'))
				{
					if (i + 1 < text.size()
					    && text.at(i + 1) == QLatin1Char('"')) {
						cell += c;
						++i;
					} else {
						quoted = false;
					}
				} else {
					cell += c;
				}
				continue;
			}

			if (c == QLatin1Char('"')) {
				quoted = true;
				pending = true;
				continue;
			}
			if (c == QLatin1Char(';')) {
				row << cell;
				cell.clear();
				pending = true;
				continue;
			}
			if (c == QLatin1Char('\r') || c == QLatin1Char('\n'))
			{
				if (c == QLatin1Char('\r') && i + 1 < text.size()
				    && text.at(i + 1) == QLatin1Char('\n')) {
					++file.crlf_ends;
					++i;
				} else {
					++file.other_ends;
				}
				row << cell;
				file.rows << row;
				row.clear();
				cell.clear();
				pending = false;
				continue;
			}
			cell += c;
			pending = true;
		}
		if (pending || !cell.isEmpty()) {
			row << cell;
			file.rows << row;
		}
	}

	/// Run --export-labels on @a project_path, and read back what it wrote.
	LabelFile exportLabels(const QString &project_path,
			       const QString &csv_path,
			       const QStringList &options = QStringList())
	{
		LabelFile file;
		file.code = CLIExport::run(QStringList()
					   << QStringLiteral("--export-labels")
					   << project_path << csv_path << options);
		file.written = QFile::exists(csv_path);

		QFile csv(csv_path);
		if (csv.open(QIODevice::ReadOnly)) {
			file.bytes = csv.readAll();
		}
		const QByteArray mark("\xEF\xBB\xBF", 3);
		file.byte_order_mark = file.bytes.startsWith(mark);
		file.text = QString::fromUtf8(file.byte_order_mark
					      ? file.bytes.mid(mark.size())
					      : file.bytes);
		parse(file);
		return file;
	}

	/**
		The shape every label file has, whatever the project: the mark, the
		first row, every row as wide as it, and CRLF at the end of each row
		and nowhere else outside a cell.
	*/
	void checkShape(const LabelFile &file)
	{
		CHECK(file.byte_order_mark);
		REQUIRE_FALSE(file.rows.isEmpty());
		CHECK(file.rows.first() == expectedHeader());
		for (const QStringList &row : file.rows)
		{
			INFO("row " << row.join(QStringLiteral("|")).toStdString());
			CHECK(row.size() == expectedHeader().size());
		}
		CHECK(file.crlf_ends == file.rows.size());
		CHECK(file.other_ends == 0);
	}

	/// The rows under the first one.
	QVector<QStringList> labelRows(const LabelFile &file)
	{
		return file.rows.mid(1);
	}

	/// The rows of one kind, in the order of the file.
	QVector<QStringList> rowsOfKind(const LabelFile &file, const QString &kind)
	{
		QVector<QStringList> rows;
		const QVector<QStringList> labels = labelRows(file);
		for (const QStringList &row : labels)
		{
			if (row.value(column(QStringLiteral("kind"))) == kind) {
				rows << row;
			}
		}
		return rows;
	}

	/// One column of @a rows, in their order.
	QStringList cells(const QVector<QStringList> &rows, const QString &key)
	{
		QStringList list;
		for (const QStringList &row : rows) {
			list << row.value(column(key));
		}
		return list;
	}

	/// The row of @a rows tagged @a text; an empty list when there is none.
	QStringList rowTagged(const QVector<QStringList> &rows, const QString &text)
	{
		for (const QStringList &row : rows)
		{
			if (row.value(column(QStringLiteral("text"))) == text) {
				return row;
			}
		}
		return QStringList();
	}
}

TEST_CASE("CU-21.12 — the exported labels follow the rail, and after a sort the new order, each with its position",
	  "[t21][label][cli]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("labels-rail.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

		//Mounted 2, 10, 1: neither the order the folio draws them in (1, 2,
		//10) nor any order a sort gives, so an export that sorted, or that
		//walked the drawing instead of the rail, fails the first file.
	TerminalStrip *strip = mountStrip(
			bench.project(), QStringLiteral("X1"),
			{{sheet, QStringLiteral("2")},
			 {sheet, QStringLiteral("10")},
			 {sheet, QStringLiteral("1")}});
	REQUIRE(strip != nullptr);
	REQUIRE(labelsOnRail(strip)
		== QStringList({QStringLiteral("2"), QStringLiteral("10"),
				QStringLiteral("1")}));

	QTemporaryDir dir;
	REQUIRE(dir.isValid());

		//The first export: the strip as it was mounted, read back from a
		//file by a process that never saw the strip editor.
	const QString mounted = dir.filePath(QStringLiteral("mounted.qet"));
	REQUIRE(writeProject(bench.project(), mounted));
	const LabelFile first = exportLabels(
			mounted, dir.filePath(QStringLiteral("mounted.csv")),
			{QStringLiteral("kinds=terminal")});
	INFO("first file:\n" << first.text.toStdString());
	REQUIRE(first.code == 0);
	checkShape(first);

	const QVector<QStringList> first_rows = labelRows(first);
	CHECK(cells(first_rows, QStringLiteral("text"))
	      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
			      QStringLiteral("1")}));
	CHECK(cells(first_rows, QStringLiteral("position"))
	      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
			      QStringLiteral("3")}));
	CHECK(cells(first_rows, QStringLiteral("strip"))
	      == QStringList({QStringLiteral("X1"), QStringLiteral("X1"),
			      QStringLiteral("X1")}));

		//Held against the rail itself, row by row: the tag the strip holds
		//at each place, and the number the Position column of the strip
		//editor shows there - PhysicalTerminal::pos() plus one, the rail
		//counted from 1.
	REQUIRE(first_rows.size() == strip->physicalTerminalCount());
	const auto real_terminals = strip->realTerminals();
	REQUIRE(real_terminals.size() == first_rows.size());
	for (int i = 0 ; i < first_rows.size() ; ++i)
	{
		INFO("row " << i);
		CHECK(first_rows.at(i).value(column(QStringLiteral("text")))
		      == real_terminals.at(i)->label());
		CHECK(first_rows.at(i).value(column(QStringLiteral("position")))
		      == QString::number(strip->physicalTerminal(i)->pos() + 1));
	}

		//Sorted as the Sort button of the strip editor sorts.
	bench->undoStack()->push(new SortTerminalStripCommand(strip));

		//The precondition: the command did reorder the rail. Without it the
		//second file below would pass for an export that ignored the strip.
	REQUIRE(labelsOnRail(strip)
		== QStringList({QStringLiteral("1"), QStringLiteral("2"),
				QStringLiteral("10")}));

	const QString sorted = dir.filePath(QStringLiteral("sorted.qet"));
	REQUIRE(writeProject(bench.project(), sorted));
	const LabelFile second = exportLabels(
			sorted, dir.filePath(QStringLiteral("sorted.csv")),
			{QStringLiteral("kinds=terminal")});
	INFO("second file:\n" << second.text.toStdString());
	REQUIRE(second.code == 0);
	checkShape(second);

		//The new order, because the user sorted the rail - the first file
		//is what tells that apart from an export that sorts on its own.
	const QVector<QStringList> second_rows = labelRows(second);
	CHECK(cells(second_rows, QStringLiteral("text"))
	      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
			      QStringLiteral("10")}));
	CHECK(cells(second_rows, QStringLiteral("position"))
	      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
			      QStringLiteral("3")}));

		//And the position travels with the label: 10 was at 2 on the rail
		//and is at 3 now, which is what the bench sees before the rail does.
	CHECK(rowTagged(first_rows, QStringLiteral("10"))
	      .value(column(QStringLiteral("position"))) == QStringLiteral("2"));
	CHECK(rowTagged(second_rows, QStringLiteral("10"))
	      .value(column(QStringLiteral("position"))) == QStringLiteral("3"));
}

TEST_CASE("CU-21.11 — a semicolon in a tag or in a field of a terminal stays in its own cell",
	  "[t21][label][cli][csv]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("labels-separator.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(mountStrip(bench.project(), QStringLiteral("X1"),
			   {{sheet, QStringLiteral("2")},
			    {sheet, QStringLiteral("10")},
			    {sheet, QStringLiteral("1")}})
		!= nullptr);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("separator.qet"));
	REQUIRE(writeProject(bench.project(), project_path));

	const LabelFile file = exportLabels(
			project_path, dir.filePath(QStringLiteral("separator.csv")));
	INFO(file.text.toStdString());
	REQUIRE(file.code == 0);

		//Every row as wide as the first: the assertion a spreadsheet makes
		//when it opens the file, and the one a shifted row fails.
	checkShape(file);
	REQUIRE(labelRows(file).size() == drawn_symbols + 3);

		//The tag of the component, whole, in one cell.
	const QVector<QStringList> components =
			rowsOfKind(file, QStringLiteral("component"));
	const QStringList q = rowTagged(components, separatorTag());
	REQUIRE_FALSE(q.isEmpty());
	CHECK(q.value(column(QStringLiteral("text"))) == separatorTag());

		//The function of the terminal 2, on the row the data base gives that
		//symbol among the components: one cell too.
	CHECK(rowTagged(components, QStringLiteral("2"))
	      .value(column(QStringLiteral("lines"))) == separatorTag());

		//And a field of a terminal row: the cable of 1, with the wire of
		//the cable on the line under it, in the same cell.
	const QVector<QStringList> terminals =
			rowsOfKind(file, QStringLiteral("terminal"));
	CHECK(rowTagged(terminals, QStringLiteral("1"))
	      .value(column(QStringLiteral("lines")))
	      == cableWithSeparator() + QStringLiteral("\nBK"));

		//Quoted in the file itself, by the RFC 4180 rule, so that a reader
		//more forgiving than a spreadsheet cannot hide a missing quote.
	CHECK(file.text.contains(QStringLiteral("component;\"Q1;Q2\";")));
	CHECK(file.text.contains(QStringLiteral(";\"Q1;Q2\";")));
	CHECK(file.text.contains(QStringLiteral(";\"W1;W2\nBK\";")));

		//The plain cells kept their plain form.
	CHECK(file.text.contains(QStringLiteral("terminal;10;W7;X1;2;0;")));
}

TEST_CASE("T21 — the command line writes the very file the collectors and the writer give in the program",
	  "[t21][label][cli]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("labels-parity.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet_one = bench.diagram(0);
	Diagram *sheet_two = bench.diagram(1);
	REQUIRE(sheet_one != nullptr);
	REQUIRE(sheet_two != nullptr);

		//Two revisions and two strips, one on each sheet: what the file
		//has to carry from the project, row by row.
	setRevision(sheet_one, QStringLiteral("A"));
	setRevision(sheet_two, QStringLiteral("B"));
	REQUIRE(mountStrip(bench.project(), QStringLiteral("X1"),
			   {{sheet_one, QStringLiteral("2")},
			    {sheet_one, QStringLiteral("10")},
			    {sheet_one, QStringLiteral("1")}})
		!= nullptr);
	REQUIRE(mountStrip(bench.project(), QStringLiteral("X2"),
			   {{sheet_two, QStringLiteral("4")},
			    {sheet_two, QStringLiteral("3")}})
		!= nullptr);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("parity.qet"));
	REQUIRE(writeProject(bench.project(), project_path));

		//Inside the program: the two collectors over the project the bench
		//holds open, in the order the command line runs them by default,
		//and the writer.
	ComponentLabelCollector component_collector(bench.project());
	LabelEntryList entries = component_collector.collect();
	INFO("component collector said: "
	     << component_collector.error().toStdString());
	REQUIRE(component_collector.error().isEmpty());
	const int component_count = int(entries.size());

	TerminalLabelCollector terminal_collector(bench.project());
	entries += terminal_collector.collect();
	REQUIRE(terminal_collector.error().isEmpty());
	const int terminal_count = int(entries.size()) - component_count;

		//The literal probes beside the counts, so that a collector that
		//came back empty cannot make the comparison below trivially true.
	REQUIRE(component_count == drawn_symbols);
	REQUIRE(terminal_count == 5);

	const QByteArray expected = LabelCsvWriter::bytes(entries);

		//And from the command line, out of the file alone.
	const LabelFile file = exportLabels(
			project_path, dir.filePath(QStringLiteral("parity.csv")));
	INFO("expected:\n" << QString::fromUtf8(expected).toStdString()
	     << "\nwritten:\n" << file.text.toStdString());
	REQUIRE(file.code == 0);
	checkShape(file);

	CHECK(file.bytes == expected);

		//The same number of rows, the terminals among them, and each
		//terminal with the revision of its own sheet.
	CHECK(labelRows(file).size() == entries.size());
	const QVector<QStringList> terminals =
			rowsOfKind(file, QStringLiteral("terminal"));
	CHECK(cells(terminals, QStringLiteral("text"))
	      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
			      QStringLiteral("1"), QStringLiteral("4"),
			      QStringLiteral("3")}));
	CHECK(cells(terminals, QStringLiteral("revision"))
	      == QStringList({QStringLiteral("A"), QStringLiteral("A"),
			      QStringLiteral("A"), QStringLiteral("B"),
			      QStringLiteral("B")}));
}

TEST_CASE("T21 — kinds= chooses the labels and their order in the file",
	  "[t21][label][cli]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("labels-kinds.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(mountStrip(bench.project(), QStringLiteral("X1"),
			   {{sheet, QStringLiteral("2")},
			    {sheet, QStringLiteral("10")},
			    {sheet, QStringLiteral("1")}})
		!= nullptr);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("kinds.qet"));
	REQUIRE(writeProject(bench.project(), project_path));
	const QString csv_path = dir.filePath(QStringLiteral("kinds.csv"));

	const QString component = QStringLiteral("component");
	const QString terminal = QStringLiteral("terminal");

	SECTION("by default, the components and then the terminals")
	{
		const LabelFile file = exportLabels(project_path, csv_path);
		INFO(file.text.toStdString());
		REQUIRE(file.code == 0);
		checkShape(file);

		QStringList expected_kinds;
		for (int i = 0 ; i < drawn_symbols ; ++i) {
			expected_kinds << component;
		}
		expected_kinds << terminal << terminal << terminal;
		CHECK(cells(labelRows(file), QStringLiteral("kind"))
		      == expected_kinds);

			//The documented price of asking for both: a terminal symbol on
			//a strip is a component of the data base and a terminal of the
			//rail, and comes out once as each.
		CHECK_FALSE(rowTagged(rowsOfKind(file, component),
				      QStringLiteral("2")).isEmpty());
		CHECK_FALSE(rowTagged(rowsOfKind(file, terminal),
				      QStringLiteral("2")).isEmpty());
	}

	SECTION("kinds=terminal: the rail and nothing else")
	{
		const LabelFile file = exportLabels(
				project_path, csv_path,
				{QStringLiteral("kinds=terminal")});
		REQUIRE(file.code == 0);
		checkShape(file);
		CHECK(cells(labelRows(file), QStringLiteral("kind"))
		      == QStringList({terminal, terminal, terminal}));
	}

	SECTION("kinds=component: the components and nothing else")
	{
		const LabelFile file = exportLabels(
				project_path, csv_path,
				{QStringLiteral("kinds=component")});
		REQUIRE(file.code == 0);
		checkShape(file);
		CHECK(labelRows(file).size() == drawn_symbols);
		CHECK(rowsOfKind(file, terminal).isEmpty());
	}

	SECTION("kinds=terminal,component: in that order")
	{
		const LabelFile file = exportLabels(
				project_path, csv_path,
				{QStringLiteral("kinds=terminal,component")});
		REQUIRE(file.code == 0);
		checkShape(file);

		QStringList expected_kinds {terminal, terminal, terminal};
		for (int i = 0 ; i < drawn_symbols ; ++i) {
			expected_kinds << component;
		}
		CHECK(cells(labelRows(file), QStringLiteral("kind"))
		      == expected_kinds);
	}
}

TEST_CASE("T21 — the label export refuses what it cannot answer, and writes nothing",
	  "[t21][label][cli]")
{
		//The bench file as it is, with no strip: what is asked here is
		//never about the rail.
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("refusals.qet"));
	REQUIRE(writeFile(project_path, fixtureXml().toUtf8()));
	const QString csv_path = dir.filePath(QStringLiteral("refusals.csv"));

	SECTION("an option it does not know, or a kind it cannot collect: 2")
	{
		const QList<QStringList> refused {
			{QStringLiteral("kind=terminal")},
			{QStringLiteral("kinds=")},
			{QStringLiteral("kinds=wire")},
			{QStringLiteral("kinds=terminal,terminal")},
			{QStringLiteral("kinds=terminal"),
			 QStringLiteral("kinds=component")}
		};
		for (const QStringList &options : refused)
		{
			INFO("options: " << options.join(QStringLiteral(" ")).toStdString());
			const LabelFile file = exportLabels(project_path, csv_path,
							    options);
			CHECK(file.code == 2);
			CHECK_FALSE(file.written);
		}
	}

	SECTION("a folder that does not exist: 1, and no file")
	{
		const QString missing =
				dir.filePath(QStringLiteral("missing/labels.csv"));
		const LabelFile file = exportLabels(project_path, missing);
		CHECK(file.code == 1);
		CHECK_FALSE(file.written);
	}

	SECTION("a project that does not exist: 2, as the other verbs")
	{
		const LabelFile file = exportLabels(
				dir.filePath(QStringLiteral("none.qet")), csv_path);
		CHECK(file.code == 2);
		CHECK_FALSE(file.written);
	}

	SECTION("a project with no strip: the terminals are an empty answer, not a fault")
	{
		const LabelFile file = exportLabels(
				project_path, csv_path,
				{QStringLiteral("kinds=terminal")});
		INFO(file.text.toStdString());
		CHECK(file.code == 0);
		REQUIRE(file.written);
		checkShape(file);
		CHECK(file.rows.size() == 1);
	}
}

TEST_CASE("T21 — the accents of a label reach the exported file as UTF-8",
	  "[t21][label][cli][csv]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("accents.qet"));
	REQUIRE(writeFile(project_path, fixtureXml().toUtf8()));

	const LabelFile file = exportLabels(
			project_path, dir.filePath(QStringLiteral("accents.csv")),
			{QStringLiteral("kinds=component")});
	INFO(file.text.toStdString());
	REQUIRE(file.code == 0);
	checkShape(file);

		//The bytes, written out byte by byte so that the check does not
		//depend on how this file was saved: a file written in the local
		//8-bit code page would hold one byte for each accented letter.
	CHECK(file.bytes.contains(QByteArray("V\xC3\xA1lvula de press\xC3\xA3o")));
	CHECK(file.bytes.contains(QByteArray("Emerg\xC3\xAAncia")));

		//And read back, in the cell they belong to.
	CHECK(rowTagged(labelRows(file), QStringLiteral("K2"))
	      .value(column(QStringLiteral("lines")))
	      == accentedDesignation() + QStringLiteral("\n")
		 + accentedFunction());
}

/*
	CU-21.14, whole: a place given by the location tree reaches the file as
	the norm writes it.

	The project holds a place in two forms, and the norm in a third. The
	component stores the path of codes the tree keeps it by, QCM1/PLACA; the
	tree has names for a person to read, which displayPath() joins with
	slashes; and IEC 81346 writes the place as +QCM1+PLACA, which is what
	the sheet and every location dialogue show. Only the third goes on a
	tape. The bench gives the tree names on purpose, so that the readable
	form is a real string the file could carry by mistake and is checked
	for, rather than a form that happens not to exist.

	The place is given by AssignLocationCommand, the command the location
	manager of T32 pushes, and the export runs from the file, so what is
	read is what a project saves and not what the bench holds in memory.
*/
TEST_CASE("CU-21.14 — the location column carries the mark of the norm, never the stored path",
	  "[t21][label][cli][iec]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("labels-location.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet_one = bench.diagram(0);
	Diagram *sheet_two = bench.diagram(1);
	REQUIRE(sheet_one != nullptr);
	REQUIRE(sheet_two != nullptr);

		//A cabinet and the plate inside it, both named.
	LocationTree tree;
	QString tree_error;
	const QString cabinet = tree.append(
			ProjectLocation(QStringLiteral("QCM1"),
					QStringLiteral("Command cabinet")),
			&tree_error);
	ProjectLocation plate_location(QStringLiteral("PLACA"),
				       QStringLiteral("Mounting plate"));
	plate_location.parent_uuid = cabinet;
	const QString plate = tree.append(plate_location, &tree_error);
	INFO("tree said: " << tree_error.toStdString());
	REQUIRE_FALSE(cabinet.isEmpty());
	REQUIRE_FALSE(plate.isEmpty());
	bench->setLocationTree(tree);

		//The three forms, measured and not assumed: the case below is
		//only worth anything if they are three different strings.
	const QString path = tree.path(plate);
	const QString mark = LocationTree::iecTag(path);
	const QString readable = tree.displayPath(plate);
	REQUIRE(path == QStringLiteral("QCM1/PLACA"));
	REQUIRE(mark == QStringLiteral("+QCM1+PLACA"));
	REQUIRE(readable == QStringLiteral("Command cabinet / Mounting plate"));

		//K2, a component of sheet two, and the terminal 2 of sheet one, put
		//on the plate in one go, as the manager puts a selection.
	Element *k2 = elementLabelled(sheet_two, QStringLiteral("K2"));
	Element *two = terminalLabelled(sheet_one, QStringLiteral("2"));
	Element *ten = terminalLabelled(sheet_one, QStringLiteral("10"));
	REQUIRE(k2 != nullptr);
	REQUIRE(two != nullptr);
	REQUIRE(ten != nullptr);
	bench->undoStack()->push(new AssignLocationCommand(
			QList<Element *>({k2, two}), path));

		//What they store is the path, and the project file carries it:
		//the form the export must not copy is there to be copied.
	REQUIRE(k2->elementInformations()
		.value(IecStructure::locationPathKey()).toString() == path);
	REQUIRE(two->elementInformations()
		.value(IecStructure::locationPathKey()).toString() == path);
	REQUIRE(bench->toXml().toString().contains(path));

		//10 gets the free text field, which no tree stands behind; 1, the
		//component Q1;Q2 and the rest get nothing.
	DiagramContext information = ten->elementInformations();
	information.addValue(IecStructure::locationKey(), QStringLiteral("X9"));
	bench->undoStack()->push(new ChangeElementInformationCommand(
			ten, ten->elementInformations(), information));

	REQUIRE(mountStrip(bench.project(), QStringLiteral("X1"),
			   {{sheet_one, QStringLiteral("2")},
			    {sheet_one, QStringLiteral("10")},
			    {sheet_one, QStringLiteral("1")}})
		!= nullptr);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString location = QStringLiteral("location");

	SECTION("the structure off: the place as the norm writes it, the free field as typed")
	{
		REQUIRE(bench->iecSettings().enabled == false);

		const QString project_path =
				dir.filePath(QStringLiteral("location-off.qet"));
		REQUIRE(writeProject(bench.project(), project_path));
		const LabelFile file = exportLabels(
				project_path,
				dir.filePath(QStringLiteral("location-off.csv")));
		INFO(file.text.toStdString());
		REQUIRE(file.code == 0);
		checkShape(file);

		const QVector<QStringList> components =
				rowsOfKind(file, QStringLiteral("component"));
		const QVector<QStringList> terminals =
				rowsOfKind(file, QStringLiteral("terminal"));

			//The case: the component the tree placed, with the mark of the
			//norm on its row.
		const QStringList k2_row = rowTagged(components, QStringLiteral("K2"));
		REQUIRE_FALSE(k2_row.isEmpty());
		CHECK(k2_row.value(column(location)) == mark);

			//The terminal the tree placed, on its row of the rail and on the
			//row the data base gives its symbol among the components: one
			//rule for both kinds of label, so the same mark on both.
		const QStringList two_terminal = rowTagged(terminals, QStringLiteral("2"));
		const QStringList two_component = rowTagged(components, QStringLiteral("2"));
		REQUIRE_FALSE(two_terminal.isEmpty());
		REQUIRE_FALSE(two_component.isEmpty());
		CHECK(two_terminal.value(column(location)) == mark);
		CHECK(two_component.value(column(location)) == mark);

			//The free text field as it was typed, with no `+`: this project
			//never said the field is a place of the norm.
		CHECK(rowTagged(terminals, QStringLiteral("10"))
		      .value(column(location)) == QStringLiteral("X9"));
		CHECK(rowTagged(components, QStringLiteral("10"))
		      .value(column(location)) == QStringLiteral("X9"));

			//Nothing is an empty cell, on both kinds of row.
		const QStringList one_terminal = rowTagged(terminals, QStringLiteral("1"));
		const QStringList q_component = rowTagged(components, separatorTag());
		REQUIRE_FALSE(one_terminal.isEmpty());
		REQUIRE_FALSE(q_component.isEmpty());
		CHECK(one_terminal.value(column(location)).isEmpty());
		CHECK(q_component.value(column(location)).isEmpty());

			//And the two other forms are nowhere in the file - not in the
			//location column, not anywhere else a careless writer could
			//have put them.
		CHECK_FALSE(file.text.contains(path));
		CHECK_FALSE(file.text.contains(readable));
		CHECK_FALSE(file.text.contains(QStringLiteral("Command cabinet")));
		CHECK_FALSE(file.text.contains(QStringLiteral("Mounting plate")));

			//No row reads a lone `+`, which is what a prefix glued to an
			//empty place would print.
		const QVector<QStringList> rows = labelRows(file);
		for (const QStringList &row : rows)
		{
			INFO("row " << row.join(QStringLiteral("|")).toStdString());
			CHECK(row.value(column(location)) != QStringLiteral("+"));
		}
	}

	SECTION("the structure on and the free field declared a place: that field behind the +")
	{
			//The short form keeps the terminal numbers as they are, so the
			//rows can be found by them; the component reads -K2.
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Short;
		settings.location_from_element = true;
		bench->setIecSettings(settings);

		const QString project_path =
				dir.filePath(QStringLiteral("location-on.qet"));
		REQUIRE(writeProject(bench.project(), project_path));
		const LabelFile file = exportLabels(
				project_path,
				dir.filePath(QStringLiteral("location-on.csv")));
		INFO(file.text.toStdString());
		REQUIRE(file.code == 0);
		checkShape(file);

		const QVector<QStringList> components =
				rowsOfKind(file, QStringLiteral("component"));
		const QVector<QStringList> terminals =
				rowsOfKind(file, QStringLiteral("terminal"));

			//The place of the tree does not change with the switch: it is
			//a place by construction, and it wins over the field.
		const QStringList k2_row = rowTagged(components, QStringLiteral("-K2"));
		REQUIRE_FALSE(k2_row.isEmpty());
		CHECK(k2_row.value(column(location)) == mark);
		CHECK(rowTagged(terminals, QStringLiteral("2"))
		      .value(column(location)) == mark);

			//The field, now that the project says it is a place, behind
			//the `+` of the norm - on both kinds of row.
		CHECK(rowTagged(terminals, QStringLiteral("10"))
		      .value(column(location)) == QStringLiteral("+X9"));
		CHECK(rowTagged(components, QStringLiteral("10"))
		      .value(column(location)) == QStringLiteral("+X9"));

			//And nothing is still nothing.
		const QStringList one_terminal = rowTagged(terminals, QStringLiteral("1"));
		REQUIRE_FALSE(one_terminal.isEmpty());
		CHECK(one_terminal.value(column(location)).isEmpty());

		CHECK_FALSE(file.text.contains(path));
		CHECK_FALSE(file.text.contains(readable));
	}
}
