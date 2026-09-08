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
#include "../../../../sources/catalog/catalogproperty.h"
#include "../../../../sources/catalog/ui/catalogprojectactions.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

/*
	Which products of an open project the catalogue cannot draw yet.

	The rule and not the window. Neither suite opens a QDialog, so what is
	proved here is the list showMissingPhysicalViewReport puts in its table
	and the three numbers it puts in its sentence; the table itself, the
	double click that walks the folio and the button that opens the part
	record are a roteiro for a person. Deleting the lines that build the
	dialogue leaves every case below green, and that is written down rather
	than hidden.

	It needs a project open - the whole question is which components of a
	project point at which product code - which is what puts it in this
	suite instead of beside physicalview_test.cpp.

	What is deliberately not repeated here: whether an empty cell is a zero,
	whether a comma is a decimal separator, whether half an axis is half an
	answer. That is CatalogPhysicalView, proved without a project in
	physicalview_test.cpp, and this file reads through it rather than
	beside it.
*/

namespace {

	const QString label_key = QStringLiteral("label");

	/// The four product codes the fixture draws, and the state of each.
	const QString measured_code   = QStringLiteral("CONT-9A-24VCC");
	const QString unmeasured_code = QStringLiteral("DJ-C16-2P");
	const QString half_code       = QStringLiteral("PORTE-FUS-1P");
	const QString unknown_code    = QStringLiteral("REF-QUI-NEXISTE-PAS");

	/**
		The project the cases are read from.

		Seven elements, and every one of them is there to answer a
		question the report has to get right:

		- K1 and K2 carry the measured product, twice, because the queue
		  is as long as the number of codes and not the number of
		  components;
		- Q1 carries a product the catalogue holds and nobody measured;
		- F1 carries one measured in width alone;
		- H1 carries a code the catalogue does not hold at all;
		- S1 carries no product code, and is the trap: it belongs to the
		  report next door and must not appear in this one;
		- X1 is a terminal, which is drawn and not bought.
	*/
	QString fixtureXml()
	{
		struct Instance
		{
			const char *symbol;
			int x;
			const char *label;
			QString part_code;
			int uuid;
		};

		const Instance instances[] = {
			{"contactor.elmt", 100, "K1", measured_code, 1},
			{"contactor.elmt", 200, "K2", measured_code, 2},
			{"breaker.elmt", 300, "Q1", unmeasured_code, 3},
			{"breaker.elmt", 400, "F1", half_code, 4},
			{"breaker.elmt", 500, "H1", unknown_code, 5},
			{"contactor.elmt", 600, "S1", QString(), 6},
			{"terminalblock.elmt", 700, "X1", QString(), 7}};

		QString drawn;
		for (const Instance &instance : instances)
		{
			QString information = QStringLiteral(
						      "<elementInformation show=\"1\" name=\"label\">%1"
						      "</elementInformation>")
					      .arg(QLatin1String(instance.label));
			if (!instance.part_code.isEmpty()) {
				information += QStringLiteral(
						       "<elementInformation show=\"1\" name=\"part_code\">%1"
						       "</elementInformation>")
					       .arg(instance.part_code);
			}

			drawn += QStringLiteral(
					 "<element x=\"%1\" y=\"100\" z=\"10\" prefix=\"\""
					 " freezeLabel=\"false\" orientation=\"0\""
					 " type=\"embed://bench/%2\""
					 " uuid=\"{cafe0000-0000-4000-8000-00000000000%3}\">"
					 "<terminals/><inputs/>"
					 "<elementInformations>%4</elementInformations>"
					 "<dynamic_texts/><texts_groups/>"
					 "</element>")
				 .arg(instance.x)
				 .arg(QLatin1String(instance.symbol))
				 .arg(instance.uuid)
				 .arg(information);
		}

		auto definition = [](const QString &file_name, const QString &english_name,
				     const QString &link_type)
		{
			return QStringLiteral(
				       "<element name=\"%1\">"
				       "<definition type=\"element\" version=\"0.80\""
				       " width=\"20\" height=\"40\""
				       " hotspot_x=\"10\" hotspot_y=\"20\""
				       " orientation=\"dnnn\" link_type=\"%3\">"
				       "<names><name lang=\"en\">%2</name></names>"
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
				       "</element>")
			       .arg(file_name, english_name, link_type);
		};

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2%3</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%4</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definition(QStringLiteral("contactor.elmt"),
				       QStringLiteral("Contactor"),
				       QStringLiteral("simple")),
			    definition(QStringLiteral("breaker.elmt"),
				       QStringLiteral("Breaker"),
				       QStringLiteral("simple")),
			    definition(QStringLiteral("terminalblock.elmt"),
				       QStringLiteral("Terminal block"),
				       QStringLiteral("terminal")),
			    drawn);
	}

	/// A project with no element at all, for the case that has none.
	QString emptyProjectXml()
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

	/// A catalogue in memory holding three of the four codes the project draws.
	struct CatalogFixture
	{
		CatalogFixture()
		{
			QString error;
			const bool opened = catalog.openInMemory(&error);
			INFO(error.toStdString());
			REQUIRE(opened);

			component_id = catalog.classByKey(QStringLiteral("component")).id;
			REQUIRE(component_id > 0);

			save(measured_code, QStringLiteral("Contacteur 9 A"),
			     QStringLiteral("45"), QStringLiteral("85"));
			save(unmeasured_code, QStringLiteral("Disjoncteur 16 A"),
			     QString(), QString());
			save(half_code, QStringLiteral("Porte-fusible 1 P"),
			     QStringLiteral("18"), QString());
		}

		void save(const QString &code, const QString &designation,
			  const QString &width, const QString &height)
		{
			CatalogPart part(code, component_id);
			part.setValue(QStringLiteral("designation"), designation);
			if (!width.isEmpty()) {
				part.setValue(QStringLiteral("width"), width);
			}
			if (!height.isEmpty()) {
				part.setValue(QStringLiteral("height"), height);
			}

			QString error;
			REQUIRE(catalog.savePart(part, &error));
			REQUIRE(error.isEmpty());
		}

		/**
			@brief Give one property of the component class an initial
			value.

			Through the catalogue, the way the office does it: this is
			how a size comes to belong to a class instead of to a
			product, which is the state the report counts apart.
		*/
		void setInitialValue(const QString &key, const QString &value)
		{
			CatalogProperty property =
					catalog.effectiveProperty(component_id, key);
			REQUIRE_FALSE(property.isNull());
			property.default_value = value;

			QString error;
			REQUIRE(catalog.updateProperty(property, &error));
			REQUIRE(error.isEmpty());
		}

		Catalog catalog;
		int component_id = 0;
	};

	using Report  = CatalogProjectActions::PhysicalViewReport;
	using Missing = CatalogProjectActions::MissingPhysicalView;

	/// The tags of the components the report lists, in the order it lists them.
	QStringList tags(const Report &report)
	{
		QStringList found;
		for (const Missing &entry : report.missing)
		{
			found << (entry.element
				  ? entry.element->elementInformations()
					    .value(label_key).toString()
				  : QStringLiteral("(no element)"));
		}
		return found;
	}

	/// The entry the report holds for the component tagged @a tag, if any.
	Missing entryFor(const Report &report, const QString &tag)
	{
		for (const Missing &entry : report.missing)
		{
			if (entry.element
					&& entry.element->elementInformations()
						   .value(label_key).toString() == tag) {
				return entry;
			}
		}
		return Missing();
	}
}

TEST_CASE("T20 — o relatório lista a peça sem vista física e deixa de fora "
	  "o componente que não tem peça nenhuma",
	  "[uibench][catalog][physicalview]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("physicalview.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagram(0) != nullptr);

	const Report report =
			CatalogProjectActions::physicalViewReport(scratch.project(),
								  fixture.catalog);

	SECTION("os componentes contados são os que se compram, e o borne não é um deles")
	{
			//Six of the seven elements: the terminal block is drawn
			//and not bought, and counting it would make every number
			//below one too big.
		CHECK(report.components == 6);
		CHECK(report.with_part == 5);
		CHECK(report.catalog_read);
	}

	SECTION("a peça medida não entra, e a que ninguém mediu entra")
	{
			//Q1, F1 and H1, and neither K1 nor K2. Sorted, because
			//the list is worked through product by product.
		CHECK(tags(report) == QStringList({QStringLiteral("Q1"),
						   QStringLiteral("F1"),
						   QStringLiteral("H1")}));
	}

	SECTION("o componente sem peça nenhuma fica de fora, e é o cuidado central")
	{
			//S1 has no product code. It is missing from a bill of
			//material and it is not missing a measurement, and the
			//report next door is the one that says so. A list holding
			//both would be one column meaning two things.
		CHECK_FALSE(tags(report).contains(QStringLiteral("S1")));
		CHECK(entryFor(report, QStringLiteral("S1")).element == nullptr);
	}

	SECTION("a fila do cadastro é medida em código, e não em componente")
	{
			//Three rows, three codes - and the measured contactor is
			//drawn twice without adding anything to either number.
		CHECK(report.missing.size() == 3);
		CHECK(report.codesToMeasure()
		      == QStringList({unmeasured_code, half_code, unknown_code}));
	}

	SECTION("cada linha diz por que a peça não pode ser desenhada")
	{
		const Missing nothing = entryFor(report, QStringLiteral("Q1"));
		CHECK(nothing.state == Missing::NoMeasure);
		CHECK(nothing.part_code == unmeasured_code);
		CHECK(nothing.description == QStringLiteral("Disjoncteur 16 A"));

			//Half a body is not a body: the width alone draws no
			//rectangle, and the row has to name which half is there.
		const Missing half = entryFor(report, QStringLiteral("F1"));
		CHECK(half.state == Missing::WidthOnly);
		CHECK(half.description == QStringLiteral("Porte-fusible 1 P"));

			//A code the catalogue does not hold is a different
			//problem from a product nobody measured - a wrong code,
			//or a share that is down - and the two must not read the
			//same.
		const Missing unknown = entryFor(report, QStringLiteral("H1"));
		CHECK(unknown.state == Missing::UnknownPart);
		CHECK(unknown.part_code == unknown_code);
		CHECK(unknown.description.isEmpty());

		CHECK(nothing.describe() != half.describe());
		CHECK(half.describe() != unknown.describe());
	}

	SECTION("nenhuma medida veio de classe, então nada é contado como herdado")
	{
		CHECK(report.inherited_size == 0);
	}
}

TEST_CASE("T20 — uma medida herdada da classe desenha, e por isso é contada "
	  "em vez de listada",
	  "[uibench][catalog][physicalview]")
{
	CatalogFixture fixture;

		//The office puts a size on the class itself, which is what a
		//generic box is: every product that never had a width filled in
		//is suddenly 100 by 100.
	fixture.setInitialValue(QStringLiteral("width"), QStringLiteral("100"));
	fixture.setInitialValue(QStringLiteral("height"), QStringLiteral("100"));

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("inherited.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report =
			CatalogProjectActions::physicalViewReport(scratch.project(),
								  fixture.catalog);

	SECTION("o que era falta de medida passa a desenhar, e sai da lista")
	{
			//Q1 and F1 now have a rectangle. Only H1 is left, because
			//an inherited value cannot reach a code the catalogue
			//does not hold.
		CHECK(tags(report) == QStringList({QStringLiteral("H1")}));
	}

	SECTION("mas a herança é contada, senão ela passa por cadastro")
	{
			//Q1 took both numbers from the class and F1 took its
			//height from it. Two, and not four: K1 and K2 were
			//measured on the product, so a report that counted every
			//drawable component would report them as generic too.
		CHECK(report.inherited_size == 2);
	}

	SECTION("a peça herda a altura, e a linha que sobra o diria")
	{
			//The sentence of a half measured product names the class
			//when the half it does have is not its own. Read here on
			//a product still in the list: the fuse holder keeps its
			//own width, and would take the height of the class - so
			//the report has to be able to say the width is the one
			//that was measured.
		fixture.setInitialValue(QStringLiteral("height"), QString());

		const Report again =
				CatalogProjectActions::physicalViewReport(scratch.project(),
									  fixture.catalog);
		const Missing half = entryFor(again, QStringLiteral("F1"));
		REQUIRE(half.state == Missing::WidthOnly);
		CHECK(half.measured_origin.isFromPart());
		CHECK_FALSE(half.describe().contains(QStringLiteral("classe")));

			//And the breaker, which owns neither number, takes its
			//width from the class: the row names the class, so
			//nobody types a height beside a width that belongs to
			//every other product of the catalogue.
		const Missing generic = entryFor(again, QStringLiteral("Q1"));
		REQUIRE(generic.state == Missing::WidthOnly);
		CHECK(generic.measured_origin.isFromClass());
		CHECK(generic.describe().contains(generic.measured_origin.className()));
	}
}

TEST_CASE("T20 — um projeto sem componente nenhum não tem nada a medir",
	  "[uibench][catalog][physicalview]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(emptyProjectXml(),
					QStringLiteral("empty.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report =
			CatalogProjectActions::physicalViewReport(scratch.project(),
								  fixture.catalog);

	CHECK(report.components == 0);
	CHECK(report.with_part == 0);
	CHECK(report.inherited_size == 0);
	CHECK(report.missing.isEmpty());
	CHECK(report.codesToMeasure().isEmpty());
		//Read, and with nothing to say. Not the same answer as a
		//catalogue that never answered, which is the case below.
	CHECK(report.catalog_read);
}

TEST_CASE("T20 — um catálogo que não abriu não relata o projeto inteiro "
	  "como por medir",
	  "[uibench][catalog][physicalview]")
{
		//Never opened: the share is down, or the file is not there. Every
		//partByCode answers nothing, and a report built on that would say
		//that five of five products need measuring - a false alarm that
		//looks exactly like a finding, and the most plausible way this
		//report could waste an afternoon.
	Catalog closed;
	REQUIRE_FALSE(closed.isOpen());

	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("noshare.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const Report report =
			CatalogProjectActions::physicalViewReport(scratch.project(),
								  closed);

	CHECK_FALSE(report.catalog_read);
	CHECK(report.missing.isEmpty());
		//The two counts that do not need the catalogue are still
		//answered: what could not be read is what a product is worth.
	CHECK(report.components == 6);
	CHECK(report.with_part == 5);
}
