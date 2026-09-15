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

#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/dataBase/ui/elementquerywidget.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/bomexportdialog.h"

#include <catch2/catch.hpp>

#include <QCheckBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QVector>

/*
	What one line of a purchase list is allowed to stand for.

	A bill of materials is read by somebody who orders from it. A line
	saying "Contactor 9 A - 5" is an instruction to buy five of one item,
	so the line has to correspond to one item. Grouped by the designation,
	it did not: two different products described with the same words - and
	people describe products with the same words all the time - were
	summed into one line carrying one of the two part codes. Nothing in
	the file says it happened. The quantity is right, the designation is
	right, and five of the wrong item arrive.

	It fails the other way too, and that half is cheaper to notice: one
	product that somebody described twice came out as two lines, which is
	two orders for the same thing.

	@par The two assertions, and why one of them is not enough
	The number of lines is the cheap question and it catches the gross
	case: a merge that should not have happened removes a line. It is not
	enough on its own, because a grouping that splits too far also changes
	the line count, and so does a grouping that drops a component
	entirely. The invariant that tells those apart is the sum: whatever
	the grouping does, every component of the project is counted exactly
	once, so the quantity column has to add up to the number of components
	drawn. Every case here asserts both, and a case asserting only the
	first would call a list that lost a component correct.

	@par The modal box inside getBom(), and how it is disarmed
	getBom() raises a critical box when its query fails, and a modal
	exec() under the offscreen platform waits forever for a click nobody
	is there to give: the run does not fail, it has to be killed. The
	helper below runs the very same query first and requires that it
	executed, so a query that cannot run stops the case on that line
	instead. "The very same" is what the explicit setGroupBy() there is
	for - it puts the widget in the state getBom() will recompute anyway,
	so the string probed and the string run are one.
*/

namespace
{
	/// One component of the fixture, as the sheet stores it.
	struct Instance
	{
		const char *label;
		const char *designation;
		const char *part_code;
		const char *part_revision;
	};

	/**
		The nine components the cases are drawn on.

		Three of them and then two of them are two different products
		wearing the same designation - the defect as it was met, and the
		reason the sum of a grouped line cannot be trusted to mean one
		item. Two more are one product described twice, which is the
		same error with its sign flipped. The last two carry no part code
		at all, because a project always holds components nobody has
		assigned a product to yet, and a list that dropped them or that
		refused to group them would be wrong in a way nobody would
		connect to this change.
	*/
	const Instance instances[] = {
		{"K1", "Contator 9 A",             "XA-100", "1"},
		{"K2", "Contator 9 A",             "XA-100", "1"},
		{"K3", "Contator 9 A",             "XA-100", "1"},
		{"K4", "Contator 9 A",             "XB-200", "1"},
		{"K5", "Contator 9 A",             "XB-200", "1"},
		{"Q1", "Disjuntor 3P 25 A",        "XC-300", "1"},
		{"Q2", "Disjuntor tripolar 25 A",  "XC-300", "1"},
		{"H1", "Sinaleiro 22 mm",          "",       ""},
		{"H2", "Sinaleiro 22 mm",          "",       ""}};

	/// How many components the fixture draws
	int componentCount()
	{
		return int(sizeof(instances) / sizeof(instances[0]));
	}

	/// @return the XML of a project drawing the components above
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
		int uuid = 0;
		for (const Instance &instance : instances)
		{
			QString info;
			info += information(QStringLiteral("label"),
					    QString::fromUtf8(instance.label));
			info += information(QStringLiteral("designation"),
					    QString::fromUtf8(instance.designation));
			info += information(QStringLiteral("part_code"),
					    QString::fromUtf8(instance.part_code));
			info += information(QStringLiteral("part_revision"),
					    QString::fromUtf8(instance.part_revision));

			++uuid;
			drawn += QStringLiteral(
					 "<element x=\"%1\" y=\"100\" z=\"10\" prefix=\"\""
					 " freezeLabel=\"false\" orientation=\"0\""
					 " type=\"embed://bench/part.elmt\""
					 " uuid=\"{cafe0000-0000-4000-8000-%2}\">"
					 "<terminals/><inputs/>"
					 "<elementInformations>%3</elementInformations>"
					 "<dynamic_texts/><texts_groups/>"
					 "</element>")
				 .arg(x)
				 .arg(uuid, 12, 10, QLatin1Char('0'))
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
			       " cols=\"25\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definition, drawn);
	}

	/**
		@param text the whole csv
		@return one QStringList per row, the header included

		Split on the separator and nothing else. The cells of this
		fixture hold no separator, no quote and no end of line on
		purpose: how a cell that does is written is proved in
		csvexport_test.cpp, and repeating it here would mean two
		measurements of one rule and one place to forget.
	*/
	QVector<QStringList> rowsOf(const QString &text)
	{
		QVector<QStringList> rows;
		const QStringList lines = text.split(QLatin1Char('\n'));
		for (const QString &line : lines)
		{
			if (!line.isEmpty()) {
				rows << line.split(QLatin1Char(';'));
			}
		}
		return rows;
	}

	/**
		@param rows the parsed csv, header included
		@return the quantity of every line added up

		The count column is the last one: ElementQueryWidget appends it
		to the columns the person chose, so it comes after them in the
		SELECT and therefore in the file.
	*/
	int quantitySum(const QVector<QStringList> &rows)
	{
		int sum = 0;
			//From the second row: the first one names the columns.
		for (int i = 1 ; i < rows.size() ; ++i)
		{
			const QStringList &row = rows.at(i);
			if (row.isEmpty()) {
				continue;
			}
			sum += row.last().toInt();
		}
		return sum;
	}

	/// @return the quantities of the lines whose first cell is @a value
	QVector<int> quantitiesOf(const QVector<QStringList> &rows,
				  const QString &value)
	{
		QVector<int> quantities;
		for (int i = 1 ; i < rows.size() ; ++i)
		{
			const QStringList &row = rows.at(i);
			if (!row.isEmpty() && row.first() == value) {
				quantities << row.last().toInt();
			}
		}
		return quantities;
	}

	/**
		The bill of materials the window writes, formatted as one.

		@param project the open project
		@param columns the column keys, in order
		@param group_by what the dialogue is expected to group by
		@return the csv text
	*/
	QString bomOfWindow(QETProject *project,
			    const QStringList &columns,
			    const QString &group_by)
	{
		BOMExportDialog dialog(project);

		ElementQueryWidget *widget =
			dialog.findChild<ElementQueryWidget *>();
		REQUIRE(widget != nullptr);

			//The state the case is about: the box is ticked, which is
			//how the dialogue opens. Asserted rather than assumed,
			//because everything below it means nothing if it is not.
		QCheckBox *format =
			dialog.findChild<QCheckBox *>(QStringLiteral("m_format_as_bom"));
		REQUIRE(format != nullptr);
		REQUIRE(format->isChecked());

			//And the header row, because every count of lines below
			//is written knowing there is one. A dialogue that stopped
			//writing it would shift all of them by one and the cases
			//would report a grouping defect that is not there.
		QCheckBox *headers =
			dialog.findChild<QCheckBox *>(QStringLiteral("m_include_headers"));
		REQUIRE(headers != nullptr);
		REQUIRE(headers->isChecked());

		widget->setQuery(QStringLiteral("SELECT ")
				 + columns.join(QStringLiteral(", "))
				 + QStringLiteral(" FROM element_nomenclature_view"));
		widget->setGroupBy(group_by);

		const QString built = widget->queryStr();
		INFO(built.toStdString());
		REQUIRE_FALSE(built.isEmpty());
		REQUIRE(built.contains(QStringLiteral(" GROUP BY ") + group_by));

			//The probe that disarms the modal box inside getBom().
			//The result is taken before the message is registered, and
			//not the other way round: INFO evaluates what it is given
			//on the spot, so an INFO written above the exec() reports
			//the error of a statement that has not run yet, which is
			//always the empty one.
		project->dataBase()->updateDB();
		QSqlQuery probe = project->dataBase()->newQuery(built);
		const bool probe_ran = probe.exec();
		INFO(probe.lastError().text().toStdString());
		REQUIRE(probe_ran);

		return dialog.getBom();
	}
}

TEST_CASE("T16 — duas peças diferentes com a mesma designação não viram "
	  "uma linha só", "[bom][grouping]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("bomgrouping.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const QString bom = bomOfWindow(
		scratch.project(),
		QStringList {QStringLiteral("designation")},
		QStringLiteral("part_code, part_revision, designation"));
	INFO(bom.toStdString());
	REQUIRE_FALSE(bom.isEmpty());

	const QVector<QStringList> rows = rowsOf(bom);

		//The header plus five lines: the contactor twice, because two
		//products wear that designation; the breaker twice, because it
		//was described twice; and the pilot light once. Grouped by the
		//designation this was four.
	CHECK(rows.size() == 6);

		//And the invariant that the line count cannot give: every
		//component drawn is counted exactly once, whatever the grouping
		//does. A line that went missing, or a component dropped by the
		//join, moves this and nothing else.
	CHECK(quantitySum(rows) == componentCount());

		//The line that carried the defect. Two lines reading the same
		//designation, three and two - and no line reading five, which
		//is what the purchase order was written from.
	const QVector<int> contactor =
		quantitiesOf(rows, QString::fromUtf8("Contator 9 A"));
	REQUIRE(contactor.size() == 2);
	CHECK(contactor.at(0) + contactor.at(1) == 5);
	CHECK(contactor.at(0) != 5);
	CHECK(contactor.at(1) != 5);
	CHECK(((contactor.at(0) == 3 && contactor.at(1) == 2)
	       || (contactor.at(0) == 2 && contactor.at(1) == 3)));

		//The components nobody assigned a product to still group, by
		//the only identity they have. This is the decision about them,
		//pinned so that a later change to the grouping cannot quietly
		//split them one per component.
	const QVector<int> lamp =
		quantitiesOf(rows, QString::fromUtf8("Sinaleiro 22 mm"));
	REQUIRE(lamp.size() == 1);
	CHECK(lamp.at(0) == 2);
}

TEST_CASE("T16 — a mesma peça descrita de dois jeitos é uma linha quando a "
	  "lista não mostra a descrição", "[bom][grouping]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("bomgroupingcode.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

		//A list of codes and quantities, which is the shape a purchase
		//order is placed from.
	const QString bom = bomOfWindow(
		scratch.project(),
		QStringList {QStringLiteral("part_code")},
		QStringLiteral("part_code, part_revision"));
	INFO(bom.toStdString());
	REQUIRE_FALSE(bom.isEmpty());

	const QVector<QStringList> rows = rowsOf(bom);

		//The header plus four: three products and the components with
		//no product assigned.
	CHECK(rows.size() == 5);
	CHECK(quantitySum(rows) == componentCount());

		//One line for the product that was described twice - which is
		//the half of the defect that ordered the same thing twice.
	const QVector<int> breaker =
		quantitiesOf(rows, QStringLiteral("XC-300"));
	REQUIRE(breaker.size() == 1);
	CHECK(breaker.at(0) == 2);

		//And the two products that share a designation stay apart, with
		//their own codes beside their own quantities.
	const QVector<int> first = quantitiesOf(rows, QStringLiteral("XA-100"));
	const QVector<int> second = quantitiesOf(rows, QStringLiteral("XB-200"));
	REQUIRE(first.size() == 1);
	REQUIRE(second.size() == 1);
	CHECK(first.at(0) == 3);
	CHECK(second.at(0) == 2);
}
