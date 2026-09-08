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

#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/catalog/catalogclass.h"
#include "../../../../sources/catalog/catalogpart.h"
#include "../../../../sources/connector/connectorcheck.h"
#include "../../../../sources/connector/connectorways.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	What the connectors of an open project still need (T34, step 4).

	The rule and not the window. Neither suite of this fork opens a QDialog,
	so what is proved here is the two lists ConnectorCheck::report puts
	together and the numbers the window prints out of them; the tables, the
	double click that walks to the folio and the two buttons are a roteiro for
	a person. Deleting connectorreportdialog.cpp and the action of the menu
	leaves every case below green, and that is written down rather than left
	to be discovered - it was measured, by building the suite with the two of
	them out.

	It needs a project open - the whole question is which components of a
	project carry which connector name, on which folio - which is what puts it
	in this suite instead of beside connectorways_test.cpp. What is
	deliberately not repeated here is the arithmetic of the reserve itself:
	that a way is used or spare, that a label the part has no way for is
	neither, that a part with no pinout has no reserve rather than a reserve
	of nought. All of that is ConnectorWays, proved without a project, and
	this file reads through it rather than beside it.

	Labelled T34 and not CU-34.n: this step closes an acceptance criterion of
	the task - "no pin exists outside a connector" - and no numbered use case.
*/

namespace
{
	/// The product codes the fixture draws, and what the catalogue holds of each.
	const QString sixteen_ways = QStringLiteral("CONN-16V");
	const QString nine_ways    = QStringLiteral("CONN-9V");
	const QString no_pinout    = QStringLiteral("CONN-SANS-BROCHAGE");
	const QString unknown_code = QStringLiteral("REF-QUI-NEXISTE-PAS");
	const QString a_contactor  = QStringLiteral("CONT-9A-24VCC");

	/// One component of the fixture.
	struct Drawn
	{
		int folio;             ///< 1-based, as the reader counts them
		int x;
		int y;
		const char *label;     ///< the pin number, unnormalised on purpose
		const char *connector; ///< what is written on the pin, may be nothing
		QString part_code;     ///< may be empty
		const char *symbol;    ///< which embedded definition it is drawn from
		const char *built_as;  ///< catalog_class, as the symbol builder writes it
	};

	/**
		Twenty five components over two folios, and every one of them is
		there to answer a question the report has to get right.

		Folio 1 draws five connectors, one per state the ways can be in:

		- CN1 has nine pins against a sixteen way part, and is the only one
		  of the five whose reserve is a number. Its name is spelled three
		  ways - CN1, cn1 and "CN1 " - because two spellings of one
		  connector have nowhere to come out: counted apart, they would
		  hand two ways the number 1 in silence;
		- XS2 has no product code on any pin;
		- XS3 has one the catalogue holds and nobody gave a pinout to;
		- XS4 has one the catalogue does not hold;
		- XS5 has two different ones, one on each of its pins.

		Folio 2 draws the second one whose ways can be counted and the other
		list:

		- XS6 draws three labels against a nine way part, and two of them -
		  "9 " and "a1" - are spellings the part does not have. The trailing
		  blank and the case are the two ways a label goes wrong while
		  looking right, and neither may be folded away into a way that
		  reads spare while it is in fact wired;
		- two pins belong to no connector, one known to be a pin by the
		  class of its part and the other by the class its symbol was built
		  with, because those are two different roads to the same answer;
		- a contactor carrying a product of the component class, which is
		  the trap: it belongs to no connector either, and it must not be
		  reported as a pin outside one;
		- a terminal, which is drawn and not bought, and which the census
		  leaves out before any of this starts.
	*/
	QString fixtureXml()
	{
		const Drawn drawn[] = {
				//CN1, nine ways of sixteen, three spellings of one name
			{1, 100, 100, "1", "CN1",  sixteen_ways, "pin.elmt", ""},
			{1, 140, 100, "2", "CN1",  sixteen_ways, "pin.elmt", ""},
			{1, 180, 100, "3", "cn1",  sixteen_ways, "pin.elmt", ""},
			{1, 220, 100, "4", "CN1",  sixteen_ways, "pin.elmt", ""},
			{1, 260, 100, "5", "CN1 ", sixteen_ways, "pin.elmt", ""},
			{1, 300, 100, "6", "CN1",  sixteen_ways, "pin.elmt", ""},
			{1, 340, 100, "7", "CN1",  sixteen_ways, "pin.elmt", ""},
			{1, 380, 100, "8", "CN1",  sixteen_ways, "pin.elmt", ""},
			{1, 420, 100, "9", "CN1",  sixteen_ways, "pin.elmt", ""},
				//XS2, no product code anywhere on it
			{1, 100, 200, "1", "XS2", QString(), "pin.elmt", ""},
			{1, 140, 200, "2", "XS2", QString(), "pin.elmt", ""},
			{1, 180, 200, "3", "XS2", QString(), "pin.elmt", ""},
				//XS3, a product nobody wrote a pinout for
			{1, 100, 300, "1", "XS3", no_pinout, "pin.elmt", ""},
			{1, 140, 300, "2", "XS3", no_pinout, "pin.elmt", ""},
				//XS4, a code the catalogue does not hold
			{1, 100, 400, "1", "XS4", unknown_code, "pin.elmt", ""},
			{1, 140, 400, "2", "XS4", unknown_code, "pin.elmt", ""},
				//XS5, two products under one name
			{1, 100, 500, "1", "XS5", sixteen_ways, "pin.elmt", ""},
			{1, 140, 500, "2", "XS5", nine_ways,    "pin.elmt", ""},
				//XS6, on the second folio, drawing two labels its part
				//has no way for
			{2, 100, 100, "1",  "XS6", nine_ways, "pin.elmt", ""},
			{2, 140, 100, "9 ", "XS6", nine_ways, "pin.elmt", ""},
			{2, 180, 100, "a1", "XS6", nine_ways, "pin.elmt", ""},
				//The other list: two pins outside every connector, known
				//to be pins by two different roads
			{2, 100, 200, "B1", "", sixteen_ways, "pin.elmt", ""},
			{2, 140, 200, "B2", "", QString(),    "pin.elmt", "connector"},
				//And the trap: a component of the project that is not a
				//pin and belongs to no connector
			{2, 100, 300, "K1", "", a_contactor, "contactor.elmt", ""},
				//Drawn and not bought: the census leaves it out
			{2, 100, 400, "X1", "", QString(), "terminal.elmt", ""}};

		QString folio_one;
		QString folio_two;
		int index = 0;
		for (const Drawn &component : drawn)
		{
			QString information = QStringLiteral(
						      "<elementInformation show=\"1\" name=\"label\">%1"
						      "</elementInformation>")
					      .arg(QLatin1String(component.label));
			if (component.connector[0] != '\0') {
				information += QStringLiteral(
						       "<elementInformation show=\"1\" name=\"connector\">%1"
						       "</elementInformation>")
					       .arg(QLatin1String(component.connector));
			}
			if (!component.part_code.isEmpty()) {
				information += QStringLiteral(
						       "<elementInformation show=\"1\" name=\"part_code\">%1"
						       "</elementInformation>")
					       .arg(component.part_code);
			}
			if (component.built_as[0] != '\0') {
				information += QStringLiteral(
						       "<elementInformation show=\"0\" name=\"catalog_class\">%1"
						       "</elementInformation>")
					       .arg(QLatin1String(component.built_as));
			}

				//Every instance gets a uuid of its own and every
				//terminal an id of its own. Not decoration: a project
				//XML that repeats the pair makes the program keep the
				//first element and silently drop the rest, and the
				//cases would then measure a folio with one pin on it.
			const QString instance = QStringLiteral(
							 "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
							 " freezeLabel=\"false\" orientation=\"0\""
							 " type=\"embed://bench/%3\""
							 " uuid=\"{c0ffee04-0000-4000-8000-%4}\">"
							 "<terminals>"
							 "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
							 "</terminals>"
							 "<inputs/>"
							 "<elementInformations>%6</elementInformations>"
							 "<dynamic_texts/><texts_groups/>"
							 "</element>")
						 .arg(component.x)
						 .arg(component.y)
						 .arg(QLatin1String(component.symbol))
						 .arg(index, 12, 16, QLatin1Char('0'))
						 .arg(index)
						 .arg(information);

			if (component.folio == 1) {
				folio_one += instance;
			} else {
				folio_two += instance;
			}
			++index;
		}

		auto definition = [](const QString &file_name, const QString &english_name,
				     const QString &link_type)
		{
			return QStringLiteral(
				       "<element name=\"%1\">"
				       "<definition type=\"element\" version=\"0.80\""
				       " width=\"20\" height=\"20\""
				       " hotspot_x=\"10\" hotspot_y=\"10\""
				       " orientation=\"dnnn\" link_type=\"%3\">"
				       "<names><name lang=\"en\">%2</name></names>"
				       "<description>"
				       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
				       " antialias=\"false\""
				       " style=\"line-style:normal;line-weight:normal;"
				       "filling:none;color:black\"/>"
				       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
				       "</description>"
				       "</definition>"
				       "</element>")
			       .arg(file_name, english_name, link_type);
		};

		auto folio = [](int order, const QString &title, const QString &elements)
		{
			return QStringLiteral(
				       "<diagram title=\"%1\" order=\"%2\" height=\"900\""
				       " cols=\"17\" colsize=\"50\" rows=\"10\" rowsize=\"80\""
				       " displaycols=\"true\" displayrows=\"true\">"
				       "<elements>%3</elements>"
				       "<inputs/><conductors/>"
				       "</diagram>")
			       .arg(title).arg(order).arg(elements);
		};

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2%3</category></collection>"
			       "%4%5"
			       "</project>")
		       .arg(definition(QStringLiteral("pin.elmt"),
				       QStringLiteral("Pin"),
				       QStringLiteral("simple")),
			    definition(QStringLiteral("contactor.elmt"),
				       QStringLiteral("Contactor"),
				       QStringLiteral("simple")),
			    definition(QStringLiteral("terminal.elmt"),
				       QStringLiteral("Terminal"),
				       QStringLiteral("terminal")),
			    folio(1, QStringLiteral("Bench 1"), folio_one),
			    folio(2, QStringLiteral("Bench 2"), folio_two));
	}

	/// A project that draws no connector at all.
	QString withoutConnectorXml()
	{
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection/>"
			"<diagram title=\"Bench\" order=\"1\" height=\"500\""
			" cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements/><inputs/><conductors/>"
			"</diagram>"
			"</project>");
	}

	/// A catalogue in memory holding four of the five codes the project draws.
	struct CatalogFixture
	{
		CatalogFixture()
		{
			QString error;
			const bool opened = catalog.openInMemory(&error);
			INFO(error.toStdString());
			REQUIRE(opened);

			connector_id = catalog.classByKey(QStringLiteral("connector")).id;
			component_id = catalog.classByKey(QStringLiteral("component")).id;
			REQUIRE(connector_id > 0);
			REQUIRE(component_id > 0);

			save(sixteen_ways, connector_id, ways(1, 16));
			save(nine_ways,    connector_id, ways(1, 9));
				//Registered, and nobody typed its pinout: the state
				//that must not read as a reserve of nought.
			save(no_pinout,    connector_id, QStringList());
				//Filed under the component class, which is what makes
				//the contactor of the fixture not a pin.
			save(a_contactor,  component_id,
			     QStringList({QStringLiteral("A1"), QStringLiteral("A2")}));
		}

		/// The labels @a first to @a last, as a maker prints them.
		static QStringList ways(int first, int last)
		{
			QStringList labels;
			for (int way = first ; way <= last ; ++way) {
				labels << QString::number(way);
			}
			return labels;
		}

		void save(const QString &code, int class_id, const QStringList &pin_labels)
		{
			CatalogPart part(code, class_id);
			for (const QString &label : pin_labels) {
				part.pins.append(CatalogPin(label, CatalogPinRole::Unknown));
			}

			QString error;
			REQUIRE(catalog.savePart(part, &error));
			REQUIRE(error.isEmpty());
		}

		Catalog catalog;
		int connector_id = 0;
		int component_id = 0;
	};

	using DrawnConnector = ConnectorCheck::DrawnConnector;
	using Report = ConnectorCheck::Report;

	/// The names of @a connectors, in the order they come.
	QStringList names(const QList<DrawnConnector> &connectors)
	{
		QStringList found;
		for (const DrawnConnector &connector : connectors) {
			found << connector.name;
		}
		return found;
	}

	/// The connector @a report holds under @a key, an empty one when it holds none.
	DrawnConnector connectorFor(const Report &report, const QString &key)
	{
		for (const DrawnConnector &connector : report.connectors)
		{
			if (connector.key == key) {
				return connector;
			}
		}
		return DrawnConnector();
	}

	/// The tag every element of @a elements carries, in the order they come.
	QStringList tags(const QList<Element *> &elements)
	{
		QStringList found;
		for (Element *element : elements)
		{
			found << (element
				  ? element->elementInformations()
					    .value(QETInformation::ELMT_LABEL).toString()
				  : QStringLiteral("(no element)"));
		}
		return found;
	}
}

TEST_CASE("T34 — o relatório separa o conector sem peça da broche sem conector, "
	  "e as duas listas não se misturam",
	  "[uibench][t34][connector]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("connectorcheck.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagramCount() == 2);

	const Report report = ConnectorCheck::report(scratch.project(),
						     fixture.catalog);
	REQUIRE(report.catalog_read);

	SECTION("os conectores desenhados são seis, e os três modos de escrever CN1 são um só")
	{
			//Six and not eight: cn1 and "CN1 " are the same connector as
			//CN1, folded by the key of step 3. Counting them apart would
			//hand three ways the number 1, in silence.
		CHECK(names(report.connectors)
		      == QStringList({QStringLiteral("CN1"), QStringLiteral("XS2"),
				      QStringLiteral("XS3"), QStringLiteral("XS4"),
				      QStringLiteral("XS5"), QStringLiteral("XS6")}));

		const DrawnConnector cn1 = connectorFor(report, QStringLiteral("cn1"));
		CHECK(cn1.pins.size() == 9);
			//The first pin in reading order names it, which is the rule
			//the renumbering already follows.
		CHECK(cn1.name == QStringLiteral("CN1"));

			//Two spellings and not three, and the missing one is the
			//measurement: the report trims the ends of the name as it
			//reads it, so "CN1 " never becomes a row of its own, while
			//cn1 does. Which is the right way round - a difference of
			//case is one a reader can see and act on, and a row reading
			//"CN1" beside a row reading "CN1" would be the list saying
			//there are two of something and showing one.
		CHECK(cn1.spellings == QStringList({QStringLiteral("CN1"),
						    QStringLiteral("cn1")}));
		for (const QString &spelling : cn1.spellings)
		{
			INFO(spelling.toStdString());
			CHECK(spelling == spelling.trimmed());
		}
		CHECK(report.pins == 21);
	}

	SECTION("o conector cuja peça não se pode contar entra numa lista")
	{
			//Four rows, four different reasons. The counted ones are not
			//in it: CN1 has a pinout and so has XS6.
		CHECK(names(report.uncountable())
		      == QStringList({QStringLiteral("XS2"), QStringLiteral("XS3"),
				      QStringLiteral("XS4"), QStringLiteral("XS5")}));
		CHECK(report.countedConnectors() == 2);
	}

	SECTION("a broche fora de conector entra na outra, e as duas não se cruzam")
	{
			//B1 is known to be a pin by the class of its part, B2 by the
			//class its symbol was built with. K1 is the trap - a
			//component with a product of the component class - and X1 is
			//a terminal the census leaves out before this starts.
		CHECK(tags(report.pins_without_connector)
		      == QStringList({QStringLiteral("B1"), QStringLiteral("B2")}));

			//And the fence: no pin of the second list is a pin of any
			//connector of the first. One list holding both would be one
			//heading over two problems, and the count under it would mean
			//neither.
		QList<Element *> in_a_connector;
		for (const DrawnConnector &connector : report.connectors) {
			in_a_connector << connector.pins;
		}
		CHECK(in_a_connector.size() == 21);
		for (Element *loose : report.pins_without_connector) {
			CHECK_FALSE(in_a_connector.contains(loose));
		}
	}

	SECTION("o componente que não é broche não é relatado como broche solta")
	{
			//The one measurement that says the second list is read from
			//the catalogue class and not from "has no connector written
			//on it". Without the class, every component of the project
			//would be a suspect - a false alarm that looks exactly like a
			//finding.
		CHECK_FALSE(tags(report.pins_without_connector)
			    .contains(QStringLiteral("K1")));
		CHECK_FALSE(tags(report.pins_without_connector)
			    .contains(QStringLiteral("X1")));
	}
}

TEST_CASE("T34 — a peça sem brochagem não tem reserva, e não uma reserva de zero",
	  "[uibench][t34][connector]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("connectorcheck.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report = ConnectorCheck::report(scratch.project(),
						     fixture.catalog);

	SECTION("o conector contado diz quantas vias tem, quantas usa e quantas sobram")
	{
		const DrawnConnector cn1 = connectorFor(report, QStringLiteral("cn1"));
		REQUIRE(cn1.isCounted());
		CHECK(cn1.way_count == 16);
		CHECK(cn1.used_count == 9);
		CHECK(cn1.reserve_count == 7);
		CHECK(cn1.drawn_count == 9);
		CHECK(cn1.not_on_part.isEmpty());
		CHECK(cn1.describe().isEmpty());
	}

	SECTION("a peça registrada sem broche nenhuma deixa as três contas desconhecidas")
	{
		const DrawnConnector xs3 = connectorFor(report, QStringLiteral("xs3"));
		REQUIRE(xs3.state == DrawnConnector::NoPinout);
		REQUIRE_FALSE(xs3.isCounted());

			//The contract the window relies on, and the reason this case
			//exists: whoever prints one of these without asking
			//isCounted() prints "-1 reserve" on a crimping guide. Written
			//as "not nought" as well as "unknown", because nought is the
			//plausible wrong answer and it is the one that would ship.
		CHECK(xs3.way_count == ConnectorWays::unknownCount());
		CHECK(xs3.used_count == ConnectorWays::unknownCount());
		CHECK(xs3.reserve_count == ConnectorWays::unknownCount());
		CHECK(xs3.reserve_count != 0);
		CHECK(ConnectorWays::unknownCount() < 0);

			//What the folios draw is still read, and is the one number
			//this row can show: "two pins drawn and no pinout written" is
			//exactly the sentence the report has to be able to say.
		CHECK(xs3.drawn_count == 2);
	}

	SECTION("nenhuma linha da lista tem conta que se possa imprimir")
	{
			//The general form of the same contract, over every row the
			//window fills: a row of that table with a printable count
			//would be a row printing a count of a pinout nobody wrote.
		for (const DrawnConnector &entry : report.uncountable())
		{
			INFO(entry.name.toStdString());
			CHECK_FALSE(entry.isCounted());
			CHECK(entry.way_count == ConnectorWays::unknownCount());
			CHECK(entry.reserve_count == ConnectorWays::unknownCount());
			CHECK_FALSE(entry.describe().isEmpty());
		}
	}

	SECTION("o total de reserva soma só o que é contável")
	{
			//Seven of CN1 plus eight of XS6, and nothing from the four
			//that have no pinout. A total that added their reserve_count
			//would read eleven - one less per connector nobody
			//catalogued, which is a wrong number wearing the face of a
			//right one.
		CHECK(report.reserveWays() == 15);
	}

	SECTION("cada estado diz o que fazer, e nenhum diz o mesmo que outro")
	{
		const DrawnConnector xs2 = connectorFor(report, QStringLiteral("xs2"));
		const DrawnConnector xs3 = connectorFor(report, QStringLiteral("xs3"));
		const DrawnConnector xs4 = connectorFor(report, QStringLiteral("xs4"));
		const DrawnConnector xs5 = connectorFor(report, QStringLiteral("xs5"));

		CHECK(xs2.state == DrawnConnector::NoPart);
		CHECK(xs3.state == DrawnConnector::NoPinout);
		CHECK(xs4.state == DrawnConnector::UnknownPart);
		CHECK(xs5.state == DrawnConnector::MixedParts);

			//Four sentences and not one, because the move that settles
			//each of them is a different move: buy and register a
			//product, correct a code, type the pinout of a product
			//already registered, or agree on which product the pins are.
		QStringList said;
		said << xs2.describe() << xs3.describe()
		     << xs4.describe() << xs5.describe();
		said.removeDuplicates();
		CHECK(said.size() == 4);

			//And the two that name a product name it, so nobody has to go
			//and find out which one.
		CHECK(xs4.describe().contains(unknown_code));
		CHECK(xs3.describe().contains(no_pinout));
		CHECK(xs5.part_codes.size() == 2);
	}
}

TEST_CASE("T34 — o rótulo de broche não é normalizado, e a discordância aparece "
	  "com nome em vez de virar reserva",
	  "[uibench][t34][connector]")
{
	/*
		The asymmetry of the two normalisations, measured on an open project
		rather than argued about: the connector name is folded, so CN1, cn1
		and "CN1 " are one connector; the pin label is not, so "9 " against a
		part that says "9" comes out by name.

		Folding the label would make the mismatch vanish into a number that
		looks right: XS6 would read two ways used and seven spare while the
		way that reads spare is in fact wired, and an electrician would be
		sent to crimp a hole that is taken.
	*/
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("connectorcheck.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report = ConnectorCheck::report(scratch.project(),
						     fixture.catalog);
	const DrawnConnector xs6 = connectorFor(report, QStringLiteral("xs6"));
	REQUIRE(xs6.isCounted());

	SECTION("os dois rótulos que a peça não tem saem pelo nome")
	{
			//Neither the trailing blank nor the case is folded away, and
			//the list is what somebody reads to put it right.
		CHECK(xs6.not_on_part == QStringList({QStringLiteral("9 "),
						      QStringLiteral("a1")}));
	}

	SECTION("e por isso a conta de usadas não os conta")
	{
			//One way used of nine, eight spare, three pins drawn. If the
			//label were folded, this would read two used and seven spare.
		CHECK(xs6.way_count == 9);
		CHECK(xs6.used_count == 1);
		CHECK(xs6.reserve_count == 8);
		CHECK(xs6.drawn_count == 3);
	}

	SECTION("a discordância é contada, e o conector continua fora da lista de faltas")
	{
			//Counted and not listed: XS6 does have a part with a pinout,
			//so a row of it among the ones that have none would make the
			//column say two things. But a way drawn that the product does
			//not have is a mistake on one side or the other, and a report
			//that never mentioned it would let it through.
		CHECK(report.mismatchedConnectors() == 1);
		CHECK_FALSE(names(report.uncountable()).contains(QStringLiteral("XS6")));
	}

	SECTION("o folio de cada conector é o que o leitor conta, e começa em um")
	{
		const DrawnConnector cn1 = connectorFor(report, QStringLiteral("cn1"));
		CHECK(cn1.folio == 1);
		CHECK(xs6.folio == 2);
	}
}

TEST_CASE("T34 — um catálogo que não abriu não relata o projeto inteiro",
	  "[uibench][t34][connector]")
{
		//Never opened: the share is down, or the file is not there. Every
		//partByCode answers nothing and every class question answers
		//nothing, so a report built on that would say that six of six
		//connectors have no part and that every component of the project is
		//a pin outside one. That is the most plausible way this report could
		//waste an afternoon.
	Catalog closed;
	REQUIRE_FALSE(closed.isOpen());

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("noshare.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report = ConnectorCheck::report(scratch.project(), closed);

	CHECK_FALSE(report.catalog_read);
	CHECK(report.uncountable().isEmpty());
	CHECK(report.pins_without_connector.isEmpty());
	CHECK(report.reserveWays() == 0);
	CHECK(report.mismatchedConnectors() == 0);

		//What does not need the catalogue is still answered: which pins say
		//they belong to which connector is written on the folio.
	CHECK(report.connectors.size() == 6);
	CHECK(report.pins == 21);
	for (const DrawnConnector &connector : report.connectors)
	{
		INFO(connector.name.toStdString());
		CHECK(connector.state == DrawnConnector::Unread);
	}
}

TEST_CASE("T34 — um projeto que não desenha conector nenhum não tem o que conferir",
	  "[uibench][t34][connector]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(withoutConnectorXml(),
					QStringLiteral("empty.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report = ConnectorCheck::report(scratch.project(),
						     fixture.catalog);

		//Read, and with nothing to say - not the same answer as a catalogue
		//that never answered, which is the case above.
	CHECK(report.catalog_read);
	CHECK(report.connectors.isEmpty());
	CHECK(report.pins_without_connector.isEmpty());
	CHECK(report.pins == 0);
	CHECK(report.countedConnectors() == 0);
	CHECK(report.reserveWays() == 0);
}

TEST_CASE("T34 — atribuir um conector às broches soltas é um passo só, e volta atrás",
	  "[uibench][t34][connector][undo]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("assign.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.project()->undoStack() != nullptr);

	const Report before = ConnectorCheck::report(scratch.project(),
						     fixture.catalog);
	REQUIRE(before.pins_without_connector.size() == 2);

	const int steps_before = scratch.project()->undoStack()->index();
	const int touched = ConnectorCheck::assignConnector(
				    before.pins_without_connector,
				    QStringLiteral("XS9"));

	SECTION("as duas broches entram no conector, numa ação só")
	{
		CHECK(touched == 2);
			//One step and not two: putting six pins into XS1 is one
			//gesture on the designer's side, and it has to be one step on
			//the stack.
		CHECK(scratch.project()->undoStack()->index() == steps_before + 1);

		const Report after = ConnectorCheck::report(scratch.project(),
							    fixture.catalog);
		CHECK(after.pins_without_connector.isEmpty());
		CHECK(names(after.connectors).contains(QStringLiteral("XS9")));

		const DrawnConnector xs9 = connectorFor(after, QStringLiteral("xs9"));
		CHECK(xs9.pins.size() == 2);

			//Counted, and the pinout came from one pin of the two: B1
			//carries the sixteen way product and B2 carries none, and one
			//code among the pins is what a connector is counted against.
			//It is worth measuring rather than assuming, because the
			//alternative - refusing to count until every pin says the
			//same - would leave a connector uncountable for as long as one
			//of its ways is being drawn.
		CHECK(xs9.isCounted());
		CHECK(xs9.way_count == 16);

			//And the report is not fooled by that into calling the ways
			//used: the two pins are labelled B1 and B2, which are no ways
			//of that product, so nothing is used, everything is spare, and
			//the two labels come out by name to be put right.
		CHECK(xs9.used_count == 0);
		CHECK(xs9.reserve_count == 16);
		CHECK(xs9.not_on_part == QStringList({QStringLiteral("B1"),
						      QStringLiteral("B2")}));
		CHECK(after.mismatchedConnectors() == 2);
	}

	SECTION("e um desfazer põe as duas de volta na fila")
	{
		scratch.project()->undoStack()->undo();

		const Report undone = ConnectorCheck::report(scratch.project(),
							     fixture.catalog);
		CHECK(tags(undone.pins_without_connector)
		      == QStringList({QStringLiteral("B1"), QStringLiteral("B2")}));
		CHECK_FALSE(names(undone.connectors).contains(QStringLiteral("XS9")));
	}
}

TEST_CASE("T34 — um nome em branco não vira conector, e escrever o que já está "
	  "escrito não gasta um passo",
	  "[uibench][t34][connector][undo]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("assign.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report = ConnectorCheck::report(scratch.project(),
						     fixture.catalog);
	const QList<Element *> loose = report.pins_without_connector;
	REQUIRE(loose.size() == 2);

	const int steps_before = scratch.project()->undoStack()->index();

	SECTION("um nome só de brancos é recusado")
	{
			//It would make a connector no report can name: the key of it
			//is empty, so its pins would read as belonging to no
			//connector while carrying a name.
		CHECK(ConnectorCheck::assignConnector(loose, QStringLiteral("   ")) == 0);
		CHECK(scratch.project()->undoStack()->index() == steps_before);
	}

	SECTION("nenhuma broche é nenhuma ação")
	{
		CHECK(ConnectorCheck::assignConnector(QList<Element *>(),
						      QStringLiteral("XS9")) == 0);
		CHECK(scratch.project()->undoStack()->index() == steps_before);
	}

	SECTION("as pontas do nome são aparadas, e escrevê-lo de novo não muda nada")
	{
			//Trimmed and nothing else: this is a place that writes the
			//field without a person typing it, so it writes no spelling
			//of its own.
		REQUIRE(ConnectorCheck::assignConnector(loose,
							QStringLiteral("  XS9  ")) == 2);
		CHECK(loose.first()->elementInformations()
		      .value(QETInformation::ELMT_CONNECTOR).toString()
		      == QStringLiteral("XS9"));

			//And the second time round nothing is a change, so no command
			//is pushed: an undo step that undoes nothing is a step the
			//user has to press Ctrl+Z twice to get past.
		const int steps_after = scratch.project()->undoStack()->index();
		CHECK(ConnectorCheck::assignConnector(loose, QStringLiteral("XS9")) == 0);
		CHECK(scratch.project()->undoStack()->index() == steps_after);
	}
}
