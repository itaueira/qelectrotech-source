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

#include "../../../../sources/cli_export.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/dataBase/ui/elementquerywidget.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/bomexportdialog.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>

/*
	What comes out of the bill of materials when the material is named the
	way people name material.

	The failure this file exists for is silent, and that is the whole
	reason it is worth a bench. A designation holding the column
	separator - "Disjuntor 3P; 25 A; curva C", which is one designation and
	not three - does not make the export fail and does not make the file
	unreadable. It shifts every column of that row by two, and the row
	underneath it looks exactly as well formed as the rows that are right.
	Purchasing reads a folio number where the quantity should be, orders
	from it, and the only way anybody finds out is by recognising the
	value.

	So what is asserted here is not that the exporter ran. It is what a
	reader gets back: the text is parsed again, by a reader written in this
	file and not by the writer under test, and the questions are the ones a
	spreadsheet asks - how many columns does this row have, and what is in
	them. A round trip through a second implementation is the only assertion
	that can tell a quoted separator from a shifted row, because every
	assertion made with the writer's own rules would agree with itself.

	Two ways in, deliberately, because the case covers both: the window,
	through BOMExportDialog::getBom(), and the command line, through
	--export-bom. They share one quoting function now and they did not
	before, so the test that proves one of them is not the test that proves
	the other.

	@par Why this file builds a QDialog when no other bench here does
	The rule in tests/catch/CMakeLists.txt is that no suite opens a
	QDialog, and it is a good rule: a modal exec() under the offscreen
	platform does not fail, it waits forever for a click nobody is there to
	give, and the run has to be killed. It is respected here in the way
	that matters - exec() is never called, the window is never shown, and
	the only member used is getBom(), which the export path calls too.

	getBom() has exactly one modal of its own: a critical box when the
	query fails. It is disarmed before it can fire, by running the very
	same query first and requiring that it executed. If that requirement
	fails the case stops there, and the box is never reached - which is why
	the probe is not the redundant line it looks like.
*/

namespace
{
	/// One component of the fixture, as the sheet stores it.
	struct Instance
	{
		const char *label;
		const char *designation;
		const char *manufacturer;
		const char *manufacturer_reference;
		const char *supplier;
		int uuid;
	};

	/**
		The components the two cases are drawn on.

		The first is the defect as it was met: a designation holding the
		separator twice over. The second holds a double quote, which is
		how a thread size or an inch measurement reaches a list of
		material. The third is the aligned-row case: manufacturer filled,
		reference empty, supplier filled, so that the empty cell is in the
		middle of the row and not at its end - an end cell that goes
		missing shortens the row, and nobody would notice a middle one.
		The fourth is plain, and it is there to prove the export did not
		start quoting everything.
	*/
	const Instance instances[] = {
		{"Q1", "Disjuntor 3P; 25 A; curva C", "Fabricante A", "AAA-25C",
		 "Fornecedor A", 1},
		{"X1", "Borne 2,5 mm² - passagem 1/2\"", "Fabricante B",
		 "BBB-25", "Fornecedor B", 2},
		{"K1", "Contator 9 A", "Fabricante C", "", "Fornecedor C", 3},
		{"H1", "Sinaleiro 22 mm", "Fabricante D", "DDD-22",
		 "Fornecedor D", 4}};

	/// @return the XML of a project drawing the four components above
	QString fixtureXml()
	{
		auto information = [](const QString &name, const QString &value)
		{
			if (value.isEmpty()) {
				return QString();
			}
			return QStringLiteral(
				       "<elementInformation show=\"1\" name=\"%1\">%2"
				       "</elementInformation>")
			       .arg(name, value.toHtmlEscaped());
		};

		QString drawn;
		int x = 100;
		for (const Instance &instance : instances)
		{
			QString info;
			info += information(QStringLiteral("label"),
					    QString::fromUtf8(instance.label));
			info += information(QStringLiteral("designation"),
					    QString::fromUtf8(instance.designation));
			info += information(QStringLiteral("manufacturer"),
					    QString::fromUtf8(instance.manufacturer));
			info += information(QStringLiteral("manufacturer_reference"),
					    QString::fromUtf8(instance.manufacturer_reference));
			info += information(QStringLiteral("supplier"),
					    QString::fromUtf8(instance.supplier));

			drawn += QStringLiteral(
					 "<element x=\"%1\" y=\"100\" z=\"10\" prefix=\"\""
					 " freezeLabel=\"false\" orientation=\"0\""
					 " type=\"embed://bench/part.elmt\""
					 " uuid=\"{cafe0000-0000-4000-8000-00000000000%2}\">"
					 "<terminals/><inputs/>"
					 "<elementInformations>%3</elementInformations>"
					 "<dynamic_texts/><texts_groups/>"
					 "</element>")
				 .arg(x)
				 .arg(instance.uuid)
				 .arg(info);
			x += 100;
		}

		const QString definition = QStringLiteral(
			"<element name=\"part.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"40\""
			" hotspot_x=\"10\" hotspot_y=\"20\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">Part</name></names>"
			"<description>"
			"<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			" end1=\"none\" end2=\"none\" length1=\"1.5\""
			" length2=\"1.5\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;"
			"filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
			"<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"
			"</description>"
			"</definition>"
			"</element>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definition, drawn);
	}

	/**
		The reader a spreadsheet is, written here on purpose.

		It is a second implementation of RFC-4180, and that is the point:
		asking the writer's own rules whether the writer got it right
		proves nothing. Cells split on the separator, except inside a
		quoted cell; a doubled quote inside one stands for a single
		quote; and an end of line inside one is text and not a new row.

		@param text the whole file
		@param separator the delimiter it was written with
		@return one QStringList per row
	*/
	QVector<QStringList> parseCsv(const QString &text,
				      QChar separator = QLatin1Char(';'))
	{
		QVector<QStringList> rows;
		QStringList row;
		QString cell;
		bool quoted = false;
		bool cell_started = false;

		for (int i = 0 ; i < text.size() ; ++i)
		{
			const QChar c = text.at(i);

			if (quoted)
			{
				if (c == QLatin1Char('"'))
				{
					if (i + 1 < text.size()
					    && text.at(i + 1) == QLatin1Char('"')) {
						cell += QLatin1Char('"');
						++i;
					} else {
						quoted = false;
					}
					continue;
				}
				cell += c;
				continue;
			}

			if (c == QLatin1Char('"') && !cell_started) {
				quoted = true;
				cell_started = true;
				continue;
			}
			if (c == separator) {
				row << cell;
				cell.clear();
				cell_started = false;
				continue;
			}
			if (c == QLatin1Char('\n')) {
				row << cell;
				cell.clear();
				cell_started = false;
				rows << row;
				row.clear();
				continue;
			}
			if (c == QLatin1Char('\r')) {
				continue;
			}
			cell += c;
			cell_started = true;
		}

		if (cell_started || !cell.isEmpty() || !row.isEmpty()) {
			row << cell;
			rows << row;
		}
		return rows;
	}

	/// @return the first row holding @a value in any of its cells
	QStringList rowHolding(const QVector<QStringList> &rows,
			       const QString &value)
	{
		for (const QStringList &row : rows) {
			if (row.contains(value)) {
				return row;
			}
		}
		return QStringList();
	}

	/**
		The bill of materials the window writes, for @a columns.

		@param project the open project
		@param columns the column keys, in order
		@return the csv text, or a null string when the case must stop
	*/
	QString bomOfWindow(QETProject *project, const QStringList &columns)
	{
		BOMExportDialog dialog(project);

		ElementQueryWidget *widget =
			dialog.findChild<ElementQueryWidget *>();
		REQUIRE(widget != nullptr);

			//The columns of the case, and neither the grouping nor
			//the count the dialogue opens with: one row per
			//component is what makes a missing cell visible.
		widget->setQuery(QStringLiteral("SELECT ")
				 + columns.join(QStringLiteral(", "))
				 + QStringLiteral(" FROM element_nomenclature_view"));
		widget->setGroupBy(QString(), false);
		widget->setCount(QString(), false);

		const QString built = widget->queryStr();
		INFO(built.toStdString());
		REQUIRE_FALSE(built.isEmpty());

			//The probe that disarms the modal box inside getBom():
			//if the query cannot run, the case stops on this line
			//instead of waiting for a click nobody will give.
		project->dataBase()->updateDB();
		QSqlQuery probe = project->dataBase()->newQuery(built);
		INFO(probe.lastError().text().toStdString());
		REQUIRE(probe.exec());

		return dialog.getBom();
	}

	/// @return false when @a content could not be written to @a path
	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return false;
		}
		QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		stream.setCodec("UTF-8");
#endif
		stream << content;
		file.close();
		return true;
	}

	/// @return the whole text of @a path, or a null string
	QString readFile(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			return QString();
		}
		QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		stream.setCodec("UTF-8");
#endif
		return stream.readAll();
	}
}

TEST_CASE("CU-16.10 — ponto e vírgula na descrição sai numa coluna só",
	  "[csv][bom]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("csvexport.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const QStringList columns {QStringLiteral("label"),
				   QStringLiteral("designation"),
				   QStringLiteral("manufacturer")};
	const QString bom = bomOfWindow(scratch.project(), columns);
	INFO(bom.toStdString());
	REQUIRE_FALSE(bom.isEmpty());

	const QVector<QStringList> rows = parseCsv(bom);
		//The header plus the four components.
	REQUIRE(rows.size() == 5);

		//Every row has as many cells as the header names columns. This
		//is the assertion the spreadsheet makes when it opens the file,
		//and the one a shifted row fails.
	const int width = rows.first().size();
	REQUIRE(width == columns.size());
	for (const QStringList &row : rows) {
		REQUIRE(row.size() == width);
	}

		//The designation arrives whole, in one cell, semicolons and all.
	const QStringList breaker = rowHolding(rows, QStringLiteral("Q1"));
	REQUIRE(breaker.size() == width);
	REQUIRE(breaker.at(1)
		== QStringLiteral("Disjuntor 3P; 25 A; curva C"));

		//And it is quoted in the file itself, which is what makes the
		//reading above possible - asserted on the text so that a reader
		//more forgiving than a spreadsheet cannot hide a missing quote.
	REQUIRE(bom.contains(QStringLiteral("\"Disjuntor 3P; 25 A; curva C\"")));

		//The plain component did not gain quotes it does not need.
	REQUIRE(bom.contains(QStringLiteral("H1;Sinaleiro 22 mm;Fabricante D")));

		//A double quote inside a designation is doubled, and comes back
		//single on the other side.
	const QStringList terminal = rowHolding(rows, QStringLiteral("X1"));
	REQUIRE(terminal.size() == width);
	REQUIRE(terminal.at(1)
		== QString::fromUtf8("Borne 2,5 mm² - passagem 1/2\""));
}

TEST_CASE("CU-16.10 — a mesma descrição pela linha de comando", "[csv][bom]")
{
		//Written to a file of its own and never opened in this process:
		//the command line opens the project itself, and two openings of
		//one file is a question this case is not asking.
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("cli.qet"));
	const QString csv_path = dir.filePath(QStringLiteral("bom.csv"));
	REQUIRE(writeFile(project_path, fixtureXml()));

	const int code = CLIExport::run(QStringList()
					<< QStringLiteral("--export-bom")
					<< project_path << csv_path);
	REQUIRE(code == 0);

	const QString csv = readFile(csv_path);
	INFO(csv.toStdString());
	REQUIRE_FALSE(csv.isEmpty());

	const QVector<QStringList> rows = parseCsv(csv);
		//The header plus the four components, the same as the window.
	REQUIRE(rows.size() == 5);

	const int width = rows.first().size();
	for (const QStringList &row : rows) {
		REQUIRE(row.size() == width);
	}

	const QStringList breaker = rowHolding(rows, QStringLiteral("Q1"));
	REQUIRE(breaker.size() == width);
	REQUIRE(breaker.contains(
			QStringLiteral("Disjuntor 3P; 25 A; curva C")));
	REQUIRE(csv.contains(QStringLiteral("\"Disjuntor 3P; 25 A; curva C\"")));
}

TEST_CASE("CU-16.11 — célula vazia no meio da linha continua alinhada",
	  "[csv][bom]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("csvempty.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

		//Manufacturer filled, reference empty, supplier filled: the
		//empty cell is in the middle, where a row that came out short
		//would take the supplier for a reference and nobody would see
		//it.
	const QStringList columns {QStringLiteral("manufacturer"),
				   QStringLiteral("manufacturer_reference"),
				   QStringLiteral("supplier")};
	const QString bom = bomOfWindow(scratch.project(), columns);
	INFO(bom.toStdString());
	REQUIRE_FALSE(bom.isEmpty());

	const QVector<QStringList> rows = parseCsv(bom);
	REQUIRE(rows.size() == 5);

	const int width = rows.first().size();
	REQUIRE(width == columns.size());
	for (const QStringList &row : rows) {
		REQUIRE(row.size() == width);
	}

		//The row of the component with no reference: three cells, the
		//middle one empty, and the supplier still under the supplier.
	const QStringList contactor =
		rowHolding(rows, QStringLiteral("Fabricante C"));
	REQUIRE(contactor.size() == 3);
	REQUIRE(contactor.at(0) == QStringLiteral("Fabricante C"));
	REQUIRE(contactor.at(1).isEmpty());
	REQUIRE(contactor.at(2) == QStringLiteral("Fornecedor C"));

		//And a row with nothing missing is still the same shape.
	const QStringList lamp = rowHolding(rows, QStringLiteral("Fabricante D"));
	REQUIRE(lamp.size() == 3);
	REQUIRE(lamp.at(1) == QStringLiteral("DDD-22"));
}

TEST_CASE("T16 — quebra de linha dentro de uma descrição não parte a linha",
	  "[csv][bom]")
{
		//Not the use case, the mechanism underneath it: a value with an
		//end of line inside is rarer than a semicolon and breaks the
		//file harder, because it changes how many rows the reader
		//counts. Mechanism, so it carries the task and not the case.
	const QString designation =
		QStringLiteral("Contator 9 A\ncom bloco auxiliar");

	QString xml = fixtureXml();
	xml.replace(QStringLiteral("Contator 9 A"),
		    designation.toHtmlEscaped());

	UiBench::ScratchProject scratch(xml, QStringLiteral("csvbreak.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const QStringList columns {QStringLiteral("label"),
				   QStringLiteral("designation")};
	const QString bom = bomOfWindow(scratch.project(), columns);
	INFO(bom.toStdString());
	REQUIRE_FALSE(bom.isEmpty());

	const QVector<QStringList> rows = parseCsv(bom);
		//Five rows and not six: the break is inside a cell.
	REQUIRE(rows.size() == 5);
	for (const QStringList &row : rows) {
		REQUIRE(row.size() == 2);
	}

	const QStringList contactor = rowHolding(rows, QStringLiteral("K1"));
	REQUIRE(contactor.size() == 2);
	REQUIRE(contactor.at(1) == designation);
}
