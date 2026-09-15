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
#include "../../../../sources/elementprovider.h"
#include "../../../../sources/properties/elementdata.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

/*
	Which of the things a sheet draws the project data base writes down, and
	which of them a list shows. They are not the same set, and until now the
	difference was not a decision - it was two filters that disagreed.

	The element table was filled with four kinds of element: plain component,
	terminal, master and thumbnail. A relay contact and a folio reference
	arrow were left out. The wires that end on them were not - the conductor
	table holds the uuid of the element at each end, whatever kind it is - so
	a list that joins a wire to the components at its ends loses that wire,
	because one of the two ends answers no row.

	The way it showed is the reason it took this long to see. addElement(),
	which runs when an element lands on a sheet, had no filter at all: the
	contact was in the table for as long as the project stayed open, and the
	join found it. The repopulation that runs when a project is opened had
	the filter. So the list was right while it was being drawn, and one line
	shorter the next morning, and nothing anywhere said so.

	Two things are measured here, and they pull in opposite directions on
	purpose:

	1. the tables now hold every element the sheets draw, and hold the same
	   thing whether the rows were written as the project was drawn or as it
	   was read back from the file (CU-17.15);

	2. the two element views publish exactly the four kinds they have always
	   published, so nothing a person reads grew by one line. Whether a relay
	   contact belongs in a parts list is a question for the factory, and it
	   is not answered here.

	What is not proved here: the wiring list itself. There is none yet - it
	is a later step of the T17. What the case below generates is the join
	that list is made of, which is the thing the case describes as being lost
	and found again.
*/

namespace
{
	/// The kind of an element, as the element table spells it.
	QString kindOf(ElementData::Type type)
	{
		return ElementData::typeToString(type);
	}

	/**
		One line per row of the element table, sorted.

		Every column of it, and not only the uuid: the two insert paths
		could agree on which elements to write and still disagree on what
		to write about them, and that is the half of the defect that does
		not change a count.
	*/
	QStringList elementRows(projectDataBase *data_base)
	{
		QStringList rows;
		QSqlQuery query = data_base->newQuery(
					QStringLiteral("SELECT uuid, diagram_uuid, pos, type,"
						       " sub_type FROM element"));
		if (!query.exec()) {
			return QStringList(QStringLiteral("query failed: ")
					   + query.lastError().text());
		}

		while (query.next())
		{
			QStringList values;
			for (int i = 0 ; i < 5 ; ++i) {
				values << query.value(i).toString();
			}
			rows << values.join(QStringLiteral(" | "));
		}

		rows.sort();
		return rows;
	}

	/**
		The wiring list, reduced to what the case is about: one line per
		wire, naming the wire and the label of the component at each end.

		An inner join, deliberately. An outer join would answer a line for
		every wire whatever the tables hold, which is the safe behaviour the
		decision I of the specification asks for while this filter is wrong
		- and which would also make this case unable to fail.
	*/
	QStringList wiringRows(projectDataBase *data_base)
	{
		QStringList rows;
		QSqlQuery query = data_base->newQuery(
					QStringLiteral(
						"SELECT c.text, a.label, b.label"
						" FROM conductor c"
						" JOIN element ea ON ea.uuid = c.terminal1_element_uuid"
						" JOIN element eb ON eb.uuid = c.terminal2_element_uuid"
						" JOIN element_info a ON a.element_uuid = ea.uuid"
						" JOIN element_info b ON b.element_uuid = eb.uuid"));
		if (!query.exec()) {
			return QStringList(QStringLiteral("query failed: ")
					   + query.lastError().text());
		}

		while (query.next())
		{
			rows << query.value(0).toString()
				+ QStringLiteral(" : ")
				+ query.value(1).toString()
				+ QStringLiteral(" -- ")
				+ query.value(2).toString();
		}

		rows.sort();
		return rows;
	}

	/// How many rows @a statement answers, -1 when it could not be run.
	int countOf(projectDataBase *data_base, const QString &statement)
	{
		QSqlQuery query = data_base->newQuery(statement);
		if (!query.exec() || !query.next()) {
			return -1;
		}
		return query.value(0).toInt();
	}

	/// Every element of every sheet of @a project whose kind is in @a types.
	int drawnCount(QETProject *project, ElementData::Types types)
	{
		int total = 0;
		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets) {
			const ElementProvider provider(sheet);
			total += static_cast<int>(provider.find(types).count());
		}
		return total;
	}

	/**
		A control circuit with one of every kind of element that can sit at
		the end of a wire.

		The shape is the one the case describes: a push button, a relay
		contact and a terminal block in a row, so that the wire between the
		button and the contact is a wire whose far end is a relay contact,
		and the wire after it leaves the contact for the terminal block.
		The two folio arrows and the coil are there because they are the
		other kinds the old filter refused or accepted.

		Every element carries two terminals, arrows included. An arrow with
		two terminals is not what the shipped collection draws, and it does
		not matter to anything measured here: what is measured is which
		element a wire end belongs to, and one terminal or two makes no
		difference to that. Uniform is one less way to get the XML wrong.
	*/
	QString fixtureXml()
	{
		struct Item
		{
			const char *definition;
			const char *link_type;
			const char *kind_informations;
			int x;
			int y;
			const char *label;
		};

			//Order matters: the terminal identifiers a conductor refers to
			//are handed out two per item, in this order.
		const Item items[] = {
			{"button.elmt",  "simple",           "",
			 200, 200, "S1"},
			{"lamp.elmt",    "simple",           "",
			 200, 300, "H1"},
			{"coil.elmt",    "master",
			 "<kindInformations>"
			 "<kindInformation name=\"type\">coil</kindInformation>"
			 "</kindInformations>",
			 200, 400, "KM1"},
			{"contact.elmt", "slave",
			 "<kindInformations>"
			 "<kindInformation name=\"type\">power</kindInformation>"
			 "<kindInformation name=\"state\">NO</kindInformation>"
			 "<kindInformation name=\"number\">1</kindInformation>"
			 "</kindInformations>",
			 400, 200, "KM1"},
			{"going.elmt",   "next_report",      "",
			 400, 300, ""},
			{"coming.elmt",  "previous_report",  "",
			 400, 400, ""},
			{"strip.elmt",   "terminal",
			 "<kindInformations>"
			 "<kindInformation name=\"type\">generic</kindInformation>"
			 "<kindInformation name=\"function\">generic</kindInformation>"
			 "</kindInformations>",
			 600, 200, "X1"}};

			//The docking point of a terminal, which is what the instance
			//stores.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString definitions;
		QString instances;
		int index = 0;
		for (const Item &item : items)
		{
			definitions += QStringLiteral(
					       "<element name=\"%1\">"
					       "<definition type=\"element\" version=\"0.80\""
					       " width=\"30\" height=\"20\""
					       " hotspot_x=\"15\" hotspot_y=\"10\""
					       " orientation=\"dnnn\" link_type=\"%2\">"
					       "<names><name lang=\"en\">%1</name></names>"
					       "%3"
					       "<description>"
					       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
					       " antialias=\"false\""
					       " style=\"line-style:normal;line-weight:normal;"
					       "filling:none;color:black\"/>"
					       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
					       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
					       "</description>"
					       "</definition>"
					       "</element>")
					//One at a time, and not the three-argument form: %1
					//appears twice on purpose, and each call replaces
					//every occurrence of the lowest number left.
				       .arg(QLatin1String(item.definition))
				       .arg(QLatin1String(item.link_type))
				       .arg(QLatin1String(item.kind_informations));

			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/%3\""
					     " uuid=\"{decaf000-0000-4000-8000-00000000000%4}\">"
					     "<terminals>"
					     "<terminal x=\"%5\" y=\"0\" orientation=\"1\" id=\"%6\"/>"
					     "<terminal x=\"%7\" y=\"0\" orientation=\"3\" id=\"%8\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%9"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(item.x)
				     .arg(item.y)
				     .arg(QLatin1String(item.definition))
				     .arg(index)
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(QLatin1String(item.label));
			++index;
		}

			//W1 ends on the relay contact, which is the wire of the case.
			//W2 and W3 end on a folio arrow. W4 leaves the contact for the
			//terminal block, so the contact is an end at both ends.
		const QString conductors = QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"7\" num=\"W1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"2\" terminal2=\"9\" num=\"W2\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"4\" terminal2=\"11\" num=\"W3\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"6\" terminal2=\"13\" num=\"W4\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">%1</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors>%3</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(definitions, instances, conductors);
	}
}

TEST_CASE("CU-17.15 — o fio que termina em contato de relé sobrevive a salvar e reabrir",
	  "[uibench][database][t17]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("elementtable.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->conductors().size() == 4);

	projectDataBase *data_base = bench.project()->dataBase();
	REQUIRE(data_base != nullptr);

		//The probe: without a relay contact and without a folio arrow on
		//the sheet, everything below would pass on a fixture that measures
		//nothing.
	REQUIRE(drawnCount(bench.project(),
			   ElementData::Slave
			   | ElementData::NextReport
			   | ElementData::PreviousReport) == 3);

	SECTION("a tabela guarda tudo o que a folha desenha")
	{
		CHECK(countOf(data_base, QStringLiteral("SELECT COUNT(*) FROM element"))
		      == drawnCount(bench.project(),
				    projectDataBase::populatedElementTypes()));

			//element and element_info are filled from the same set, and
			//they have to be: the views join the two, so a row in one
			//without a row in the other is a component no list can reach.
		CHECK(countOf(data_base, QStringLiteral("SELECT COUNT(*) FROM element"))
		      == countOf(data_base,
				 QStringLiteral("SELECT COUNT(*) FROM element_info")));

		for (const ElementData::Type kind : {ElementData::Slave,
						     ElementData::NextReport,
						     ElementData::PreviousReport})
		{
			INFO("kind " << kindOf(kind).toStdString());
			CHECK(countOf(data_base,
				      QStringLiteral("SELECT COUNT(*) FROM element"
						     " WHERE type = '%1'")
				      .arg(kindOf(kind))) == 1);
		}
	}

	SECTION("a lista de fiação tem as mesmas linhas depois de salvar e reabrir")
	{
		const QStringList before = wiringRows(data_base);

			//Four wires, four lines: the join finds a component at both
			//ends of every one of them. Stated as the number of wires on
			//the sheet rather than as a literal, so that a fixture that
			//grows a wire does not silently stop proving this.
		CHECK(before.count() == sheet->conductors().size());

			//The wire of the case, by name: the far end is the contact,
			//and the contact answers with its own label.
		CHECK(before.contains(QStringLiteral("W1 : S1 -- KM1")));

			//The result first and the message after: an INFO builds its
			//text when it is reached, so one written below a REQUIRE that
			//failed is never seen.
		const bool reopened_ok = bench.saveAndReopen();
		INFO(bench.error().toStdString());
		REQUIRE(reopened_ok);
		REQUIRE(bench.isOpen());

		projectDataBase *reopened = bench.project()->dataBase();
		REQUIRE(reopened != nullptr);

			//The sentence of the case: the same number of lines and the
			//same label on the contact.
		CHECK(wiringRows(reopened) == before);
	}

	SECTION("o que o desenho escreve e o que a releitura escreve dizem o mesmo")
	{
		/*
			The two insert paths, compared against each other rather than
			against a literal. The rows now in the table were written by
			the repopulation that runs when a project is opened; the table
			is emptied and filled again through addElement(), which is the
			path an element takes when it lands on a sheet, and the two are
			asked to produce the same thing.

			It would have failed twice before this step: addElement() took
			kinds the repopulation refuses, and it wrote the sub_type of a
			terminal - the kind of terminal - where the repopulation writes
			the master kind, which a terminal has not got.
		*/
		const QStringList from_repopulate = elementRows(data_base);
		REQUIRE_FALSE(from_repopulate.isEmpty());

		QSqlQuery clear_elements = data_base->newQuery(
					QStringLiteral("DELETE FROM element"));
		REQUIRE(clear_elements.exec());
		QSqlQuery clear_info = data_base->newQuery(
					QStringLiteral("DELETE FROM element_info"));
		REQUIRE(clear_info.exec());
		REQUIRE(countOf(data_base,
				QStringLiteral("SELECT COUNT(*) FROM element")) == 0);

		const QList<Element *> drawn = sheet->elements();
		for (Element *element : drawn) {
			data_base->addElement(element);
		}

		CHECK(elementRows(data_base) == from_repopulate);
	}
}

TEST_CASE("T17 — as duas visões de elemento continuam publicando as quatro espécies de sempre",
	  "[uibench][database][t17]")
{
	/*
		The other half of the step, and the one nobody sees: widening the
		tables must not widen a single list.

		industrial.qet and not a fixture written here, because what has to be
		measured is a project with enough of the two new kinds in it to make
		the difference legible - it draws over two hundred folio arrows and
		relay contacts against about three hundred and fifty components. A
		fixture with one of each would pass the same assertions and would not
		show that the difference is large.

		Nothing below is a literal count read off that file. The numbers are
		taken from the project itself, so a later synchronisation with the
		upstream that changes the example cannot turn this red for a reason
		that has nothing to do with what is being proved.
	*/
	const QString reference_example = QStringLiteral("industrial.qet");

	UiBench::Project project(reference_example);
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	projectDataBase *data_base = project.project()->dataBase();
	REQUIRE(data_base != nullptr);

	const int published = drawnCount(project.project(),
					 projectDataBase::publishedElementTypes());
	const int withheld = drawnCount(project.project(),
					projectDataBase::populatedElementTypes()
					& ~projectDataBase::publishedElementTypes());

		//The probe again, and it is what makes this file worth its
		//compilation time: on an example with none of the new kinds every
		//assertion below would hold with no code at all behind it.
	INFO("kinds the views withhold: " << withheld
	     << " of " << (published + withheld) << " drawn");
	REQUIRE(withheld > 0);

	SECTION("a tabela cresceu")
	{
		CHECK(countOf(data_base, QStringLiteral("SELECT COUNT(*) FROM element"))
		      == published + withheld);
	}

	SECTION("as visões não cresceram")
	{
			//element_label_view is the body without the bill of materials
			//filter, so it is the one that can be compared to a count of
			//drawn elements. The nomenclature view is that same set minus
			//whatever the user ticked out of the purchase list, which is
			//why it is checked as "no more than" and not as an equality.
		CHECK(countOf(data_base,
			      QStringLiteral("SELECT COUNT(*) FROM element_label_view"))
		      == published);
		CHECK(countOf(data_base,
			      QStringLiteral("SELECT COUNT(*)"
					     " FROM element_nomenclature_view"))
		      <= published);
	}

	SECTION("nenhuma espécie nova aparece em visão nenhuma")
	{
			//Said over the views and not over each consumer on purpose:
			//the parts list, the material list by location, the label
			//roll, the command line export and the tables drawn on a
			//folio all read one of these two. Closing the question here
			//closes it for every one of them at once.
		for (const QString &view : {QStringLiteral("element_nomenclature_view"),
					    QStringLiteral("element_label_view")})
		{
			for (const ElementData::Type kind : {ElementData::Slave,
							     ElementData::NextReport,
							     ElementData::PreviousReport})
			{
				INFO("view " << view.toStdString()
				     << ", kind " << kindOf(kind).toStdString());
				CHECK(countOf(data_base,
					      QStringLiteral("SELECT COUNT(*) FROM %1"
							     " WHERE element_type = '%2'")
					      .arg(view, kindOf(kind))) == 0);
			}

				//And the probe for the loop above: a kind the views do
				//publish has to answer with rows, or the three zeros
				//would only be saying that the count never works.
			INFO("view " << view.toStdString());
			CHECK(countOf(data_base,
				      QStringLiteral("SELECT COUNT(*) FROM %1"
						     " WHERE element_type = '%2'")
				      .arg(view, kindOf(ElementData::Simple))) > 0);
		}
	}
}
