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
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	Which connector a pin belongs to, and whether that answer survives the
	trip to the parts list view.

	Why this file is in the suite that opens a project, and not in the pure
	one: the key itself is a string in a list, and asserting that the list
	contains it proves nothing anybody cares about. The two joints that carry
	the value are both outside pure reach.

	1. The column. Every key of QETInformation::elementInfoKeys() grows a
	   column of element_info on its own, but element_nomenclature_view is
	   written by hand. A key added to the list and forgotten in the view
	   still shows up in the column selector of the parts list, and choosing
	   it makes the whole query fail: the table drawn on the folio comes back
	   with no row and no column at all, empty and silent. That is not a
	   hypothetical - it is the defect the comment above the plc columns of
	   projectdatabase.cpp was written for, after two keys had shipped that
	   way.
	2. The write. Typing the connector name into the component's information
	   panel goes through ChangeElementInformationCommand, and it is that
	   command - not the element - that tells the data base. Cut the notice
	   and the pin carries the right connector while the list beside it keeps
	   showing the old one.

	Both were cut on purpose and measured; neither is visible from
	C_unittests, which cannot open a project and therefore has no view to
	select from.

	Labelled T34 and not CU-34.2: the case asks for pins to be inserted into
	one connector, switched to another with a keyboard modifier and switched
	back, with the numbering staying independent. What is proved here is the
	place the answer is stored and the road it takes to the list - the
	modifier, the insertion and the independent counter are not written yet.
*/

namespace
{
	/**
		Five pins on one sheet: three of them belong to CN1 and two to CN2.

		The pin numbers repeat across the two connectors on purpose - CN1
		carries 1, 2, 3 and CN2 carries 1, 2 - because that is the arrangement
		the use case is about. A fixture that numbered the five pins 1 to 5
		would pass whether the connector column exists or not.

		Every instance gets a uuid of its own and every terminal an id of its
		own. Not decoration: a project XML that repeats the pair makes the
		program keep the first element and drop the rest, with no warning
		anywhere, and the case then measures a sheet with one pin on it.
	*/
	QString fixtureXml()
	{
		struct Pin
		{
			int x;
			int y;
			const char *number;
			const char *connector;
		};

		const Pin pins[] = {
			{200, 200, "1", "CN1"},
			{240, 200, "2", "CN1"},
			{280, 200, "3", "CN1"},
			{200, 300, "1", "CN2"},
			{240, 300, "2", "CN2"}};

		QString instances;
		int index = 0;
		for (const Pin &pin : pins)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/pin.elmt\""
					     " uuid=\"{c0ffee01-0000-4000-8000-00000000000%3}\">"
					     "<terminals>"
					     "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%5"
					     "</elementInformation>"
					     "<elementInformation show=\"1\" name=\"connector\">%6"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(pin.x)
				     .arg(pin.y)
				     .arg(index)
				     .arg(index)
				     .arg(QLatin1String(pin.number))
				     .arg(QLatin1String(pin.connector));
			++index;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"pin.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Pin</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(instances);
	}

	/**
		What the parts list view answers for one column, one entry per row,
		sorted so that the answer does not depend on the order the rows came
		back in.

		Asking the view and not the element is the point: this is the query
		the bill of materials, the export and the table drawn on a folio all
		run, so it answers whether the value would be printed - a different
		question from whether the component carries it.

		A query that fails must not read as a list that came back empty: the
		two mean opposite things, and telling them apart is the whole reason
		this file exists. So a failure comes back as a legible line instead
		of as nothing.
	*/
	QStringList viewValues(QETProject *project, const QString &column,
			       const QString &where = QString())
	{
		QStringList values;
		if (!project || !project->dataBase()) {
			values << QStringLiteral("no data base");
			return values;
		}

		QString statement = QStringLiteral("SELECT %1 FROM element_nomenclature_view")
				    .arg(column);
		if (!where.isEmpty()) {
			statement += QStringLiteral(" WHERE ") + where;
		}

		QSqlQuery query = project->dataBase()->newQuery(statement);
		if (!query.exec())
		{
			values << QStringLiteral("query failed: %1")
				  .arg(query.lastError().text());
			return values;
		}
		while (query.next()) {
			values << query.value(0).toString();
		}
		values.sort();
		return values;
	}

	/// The pin drawn with @a number that says it belongs to @a connector.
	Element *pin(Diagram *sheet, const QString &number, const QString &connector)
	{
		if (!sheet) {
			return nullptr;
		}

		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			const DiagramContext info = element->elementInformations();
			if (info.value(QETInformation::ELMT_LABEL).toString() == number
			    && info.value(QETInformation::ELMT_CONNECTOR).toString() == connector) {
				return element;
			}
		}
		return nullptr;
	}

	/// The connector name of @a element, moved to @a connector the way the
	/// information panel does it: one undo command on the project's stack.
	void moveToConnector(QETProject *project, Element *element,
			     const QString &connector)
	{
		const DiagramContext old_info = element->elementInformations();
		DiagramContext new_info = old_info;
		new_info.addValue(QETInformation::ELMT_CONNECTOR, connector);

		project->undoStack()->push(
			new ChangeElementInformationCommand(element, old_info, new_info));
	}
}

TEST_CASE("T34 — a pin says which connector it belongs to, and the parts list can select it",
	  "[t34][connector][database]")
{
		//The key has to be in the list before anything else can work: the
		//column of element_info is created from that list, and so are the
		//insert and the update statements.
	REQUIRE(QETInformation::elementInfoKeys()
		.contains(QETInformation::ELMT_CONNECTOR));

		//And it has to have a name a person can read. A key with no entry in
		//translatedInfoKey() still gets a row in the component's information
		//panel - an anonymous one, with no label at all beside the field.
	REQUIRE_FALSE(QETInformation::translatedInfoKey(QETInformation::ELMT_CONNECTOR)
		      .isEmpty());

	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("connectorlink.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

		//Five pins, and not one: the fixture repeats no uuid and no terminal
		//id, so nothing was dropped on the way in.
	REQUIRE(sheet->elements().size() == 5);

	SECTION("the sheet carries the connector name on each pin")
	{
		QStringList carried = UiBench::information(sheet, QETInformation::ELMT_CONNECTOR);
		carried.sort();
		CHECK(carried == QStringList({QStringLiteral("CN1"), QStringLiteral("CN1"),
					      QStringLiteral("CN1"), QStringLiteral("CN2"),
					      QStringLiteral("CN2")}));
	}

	SECTION("the parts list view answers for the connector column")
	{
			//This is the joint the plc columns were missing: the value is on
			//the element and the column is in element_info, and the view is
			//still written by hand.
		CHECK(viewValues(bench.project(), QStringLiteral("connector"))
		      == QStringList({QStringLiteral("CN1"), QStringLiteral("CN1"),
				      QStringLiteral("CN1"), QStringLiteral("CN2"),
				      QStringLiteral("CN2")}));
	}

	SECTION("the pins of one connector can be asked for on their own")
	{
			//"Every pin of CN1", which is what the list on the folio and the
			//crimping guide both need, and what the numbers being independent
			//per connector means: CN1 has 1, 2, 3 while CN2 also has a 1 and
			//a 2, and neither list is polluted by the other.
		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN1'"))
		      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
				      QStringLiteral("3")}));

		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN2'"))
		      == QStringList({QStringLiteral("1"), QStringLiteral("2")}));
	}
}

TEST_CASE("T34 — moving a pin to another connector is undoable and reaches the parts list",
	  "[t34][connector][database][undo]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("connectorlink.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(bench.project()->dataBase() != nullptr);

		//Pin 2 of CN2 - the one a draughtsman decides belongs to CN1 after
		//all. Taken by number and connector rather than by index, so that a
		//change in the fixture cannot silently move the pin under the test.
	Element *moved = pin(sheet, QStringLiteral("2"), QStringLiteral("CN2"));
	REQUIRE(moved != nullptr);

	const int steps_before = bench.project()->undoStack()->index();

	moveToConnector(bench.project(), moved, QStringLiteral("CN1"));

		//One step on the stack, so the gesture can be taken back - the whole
		//reason the panel pushes a command instead of writing the value.
	REQUIRE(bench.project()->undoStack()->index() == steps_before + 1);

	CHECK(viewValues(bench.project(), QStringLiteral("label"),
			 QStringLiteral("connector = 'CN1'"))
	      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
			      QStringLiteral("2"), QStringLiteral("3")}));
	CHECK(viewValues(bench.project(), QStringLiteral("label"),
			 QStringLiteral("connector = 'CN2'"))
	      == QStringList({QStringLiteral("1")}));

	SECTION("and an undo puts it back, in the list too")
	{
		bench.project()->undoStack()->undo();

		CHECK(moved->elementInformations()
		      .value(QETInformation::ELMT_CONNECTOR).toString()
		      == QStringLiteral("CN2"));

			//The element being right is not the point: the list beside it has
			//to be right as well, and it is the command that says so.
		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN1'"))
		      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
				      QStringLiteral("3")}));
		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN2'"))
		      == QStringList({QStringLiteral("1"), QStringLiteral("2")}));
	}

	SECTION("and a redo moves it again")
	{
		bench.project()->undoStack()->undo();
		bench.project()->undoStack()->redo();

		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN2'"))
		      == QStringList({QStringLiteral("1")}));
	}
}

TEST_CASE("T34 — the connector of each pin is still there after a save and a reopening",
	  "[t34][connector][xml]")
{
	/*
		The failure this program produces most often: a field that is
		modelled, editable, listed and gone the next time the project is
		opened. The connector name rides in the element's information, whose
		toXml writes the keys it holds and whose fromXml takes back the keys
		it finds - so nothing here needed writing, and that is exactly why it
		needs proving rather than assuming. A key kept out of the written
		block, or a reader that only knows a fixed vocabulary, would pass
		every case above and lose the work on the first close.
	*/
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("connectorlink.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	REQUIRE(viewValues(bench.project(), QStringLiteral("connector"))
		== QStringList({QStringLiteral("CN1"), QStringLiteral("CN1"),
				QStringLiteral("CN1"), QStringLiteral("CN2"),
				QStringLiteral("CN2")}));

		//Every pointer taken above dangles from here on: the project was
		//closed and read again from the file.
	INFO(bench.error().toStdString());
	REQUIRE(bench.saveAndReopen());

	Diagram *reopened = bench.diagram(0);
	REQUIRE(reopened != nullptr);
	REQUIRE(reopened->elements().size() == 5);

	QStringList carried = UiBench::information(reopened, QETInformation::ELMT_CONNECTOR);
	carried.sort();
	CHECK(carried == QStringList({QStringLiteral("CN1"), QStringLiteral("CN1"),
				      QStringLiteral("CN1"), QStringLiteral("CN2"),
				      QStringLiteral("CN2")}));

		//And the road to the list is rebuilt with the project, not just the
		//value on the element: the data base of a reopened project is a new
		//one, filled from the file that was written.
	CHECK(viewValues(bench.project(), QStringLiteral("label"),
			 QStringLiteral("connector = 'CN1'"))
	      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
			      QStringLiteral("3")}));
}

TEST_CASE("T34 — every element information key the parts list offers is selectable from its view",
	  "[t34][database]")
{
	/*
		The general form of the defect the connector column had to avoid, put
		here rather than in the T34 case above because it is not about the
		connector: element_info grows a column for every key of
		elementInfoKeys() on its own, element_nomenclature_view does not, and
		nothing in the program compares the two. Two keys have already shipped
		in the list and missing from the view; each of them turned the table
		on the folio into nothing at all, with no message.

		formula is the one key deliberately absent from the view, and it is
		also one of the two the column selector of the parts list refuses to
		offer (elementquerywidget.cpp skips formula and exclude_from_bom). So
		the invariant is: everything the selector can offer, the view can
		select. exclude_from_bom is hidden for a different reason - it has its
		own check box - and it is in the view, so it needs no exception here.
	*/
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("connectorlink.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.project()->dataBase() != nullptr);

	const QStringList not_offered = {QStringLiteral("formula")};

	QStringList unselectable;
	const QStringList keys = QETInformation::elementInfoKeys();
	for (const QString &key : keys)
	{
		if (not_offered.contains(key)) {
			continue;
		}

		QSqlQuery query = bench.project()->dataBase()->newQuery(
					  QStringLiteral("SELECT %1 FROM element_nomenclature_view LIMIT 1")
					  .arg(key));
		if (!query.exec()) {
			unselectable << key;
		}
	}

		//Named and not counted: a failure that says "one column is missing"
		//sends whoever reads it back to a list of sixty keys.
	INFO("keys the view cannot select: " << unselectable.join(QStringLiteral(", ")).toStdString());
	CHECK(unselectable.isEmpty());

		//And the exception really is one: formula is absent from the view, so
		//the list above is an invariant and not an empty check that would
		//stay green if every key were excused.
	QSqlQuery formula_query = bench.project()->dataBase()->newQuery(
				    QStringLiteral("SELECT formula FROM element_nomenclature_view LIMIT 1"));
	CHECK_FALSE(formula_query.exec());
}
