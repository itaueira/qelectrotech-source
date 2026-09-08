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

#include "../../../../sources/autoNum/numberingformat.h"
#include "../../../../sources/autoNum/projectrenumberer.h"
#include "../../../../sources/autoNum/renumberplan.h"
#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	Renumbering the ways of one connector on an open project (T34).

	The counting itself is proved without a project, in
	src/connectornumbering_test.cpp, and that is where the rules are argued
	with. What only a project can answer is the road: which components the
	scope collects, where the connector name is read from, that the new tags
	reach the parts list beside the drawing, and that one Ctrl+Z takes the
	whole thing back.

	Labelled T34 and not CU-34.3: the case is a draughtsman deleting three
	ways from a folio and running the command from the menu, and neither the
	deletion nor the menu entry for "renumber the ways of this connector"
	exists yet - the format is offered as the default of the renumbering
	dialog, and the scope is the selection. What is proved here is everything
	between the two.
*/

namespace
{
	struct Pin
	{
		int y;
		const char *label;
		const char *connector;
	};

	/**
		A sheet built from @a pins, one component each, all of them drawn from
		the same embedded symbol.

		Every instance gets a uuid of its own and every terminal an id of its
		own. Not decoration: a project XML that repeats the pair makes the
		program keep the first element and silently drop the rest, and the
		case would then measure a sheet with one component on it.
	*/
	QString fixtureXml(const QList<Pin> &pins)
	{
		QString instances;
		int index = 0;
		for (const Pin &pin : pins)
		{
			instances += QStringLiteral(
					     "<element x=\"200\" y=\"%1\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/pin.elmt\""
					     " uuid=\"{c0ffee02-0000-4000-8000-00000000000%2}\">"
					     "<terminals>"
					     "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%3\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%4"
					     "</elementInformation>"
					     "<elementInformation show=\"1\" name=\"connector\">%5"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(pin.y)
				     .arg(index)
				     .arg(index)
				     .arg(QLatin1String(pin.label))
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

	/// Three ways of CN1 with two holes in them, two ways of CN2 numbered from
	/// four, and a contactor that belongs to no connector at all.
	QString holedFixture()
	{
		const QList<Pin> pins = {{200, "1", "CN1"},
					 {240, "5", "CN1"},
					 {280, "9", "CN1"},
					 {340, "4", "CN2"},
					 {380, "6", "CN2"},
					 {420, "K1", ""}};
		return fixtureXml(pins);
	}

	/**
		What the parts list view answers for one column, sorted so that the
		answer does not depend on the order the rows came back in.

		A query that fails must not read as a list that came back empty: the
		two mean opposite things. So a failure comes back as a legible line
		instead of as nothing.
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

	/// The built-in format that numbers by connector, as the dialog offers it.
	NumberingFormat byConnector()
	{
		const QList<NumberingFormat> formats = NumberingFormat::builtinFormats();
		for (const NumberingFormat &format : formats)
		{
			if (format.scope == NumberingScope::Connector) {
				return format;
			}
		}
		return NumberingFormat();
	}

	/// The components of @a sheet whose connector field says @a connector.
	QList<Element *> pinsOf(Diagram *sheet, const QString &connector)
	{
		QList<Element *> found;
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->elementInformations()
			    .value(QETInformation::ELMT_CONNECTOR).toString() == connector) {
				found.append(element);
			}
		}
		return found;
	}

	/// One entry per component, the tag it carries, sorted.
	QStringList labelsOnSheet(Diagram *sheet)
	{
		QStringList labels = UiBench::information(sheet, QETInformation::ELMT_LABEL);
		labels.sort();
		return labels;
	}
}

TEST_CASE("T34 — renuméroter les voies d'un connecteur ferme les trous et laisse le reste",
	  "[t34][connector][renumber]")
{
	UiBench::ScratchProject bench(holedFixture(),
				      QStringLiteral("connectorrenumber.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().size() == 6);

		//The scope of the whole project: the six components are all drawn as
		//simple symbols, which is what a pin of a connector is today - the
		//library declares no type for one.
	const QList<Element *> scope = ProjectRenumberer::components(bench.project());
	REQUIRE(scope.size() == 6);

	Catalog catalog;
	const QList<RenumberInput> inputs =
		ProjectRenumberer::inputsFor(catalog, scope, byConnector());
	REQUIRE(inputs.size() == 6);

		//The connector each pin belongs to was read off the pin, and the
		//contactor carries none. This is the joint between the key of the
		//first step and the counting of this one: an input list that left the
		//field empty would number every way of the project 1 and pass every
		//check about holes below.
	QStringList carried;
	for (const RenumberInput &input : inputs) {
		carried << input.connector;
	}
	carried.sort();
	CHECK(carried == QStringList({QString(), QStringLiteral("CN1"),
				      QStringLiteral("CN1"), QStringLiteral("CN1"),
				      QStringLiteral("CN2"), QStringLiteral("CN2")}));

	const RenumberPlan plan = Renumberer::plan(inputs);

		//Four tags move: 5 and 9 of CN1 become 2 and 3, and 4 and 6 of CN2
		//become 1 and 2. The contactor is passed over because it belongs to
		//no connector, and it is counted apart from the ones a user froze.
	CHECK(plan.changeCount() == 4);
	CHECK(plan.skippedCount() == 1);
	CHECK(plan.frozenCount() == 0);

	const int steps_before = bench.project()->undoStack()->index();
	CHECK(ProjectRenumberer::applyPlan(scope, plan) == 4);

		//One step on the stack for the whole renumbering, not one per
		//component: a user who has to press Ctrl+Z four hundred times has not
		//been given undo.
	REQUIRE(bench.project()->undoStack()->index() == steps_before + 1);

	CHECK(labelsOnSheet(sheet) == QStringList({QStringLiteral("1"),
						   QStringLiteral("1"),
						   QStringLiteral("2"),
						   QStringLiteral("2"),
						   QStringLiteral("3"),
						   QStringLiteral("K1")}));

	SECTION("et la liste à côté du dessin dit la même chose")
	{
			//The wire this section exists for: the command tells the data
			//base, and nothing else does. Cut that notice and the sheet is
			//right while the parts list beside it, the export and the table
			//drawn on a folio all keep printing the old ways.
		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN1'"))
		      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
				      QStringLiteral("3")}));
		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN2'"))
		      == QStringList({QStringLiteral("1"), QStringLiteral("2")}));
	}

	SECTION("le composant sans connecteur garde son repère")
	{
			//"Sans toucher au reste du projet" of the specification, taken at
			//its word: the contactor is inside the scope, it went through the
			//plan, and it came out untouched.
		const QList<Element *> nameless = pinsOf(sheet, QString());
		REQUIRE(nameless.size() == 1);
		CHECK(nameless.first()->elementInformations()
		      .value(QETInformation::ELMT_LABEL).toString()
		      == QStringLiteral("K1"));
	}

	SECTION("et un seul Ctrl+Z remet tout comme avant")
	{
		bench.project()->undoStack()->undo();

		CHECK(labelsOnSheet(sheet) == QStringList({QStringLiteral("1"),
							   QStringLiteral("4"),
							   QStringLiteral("5"),
							   QStringLiteral("6"),
							   QStringLiteral("9"),
							   QStringLiteral("K1")}));

			//The element being right again is not the point: the list has to
			//come back with it.
		CHECK(viewValues(bench.project(), QStringLiteral("label"),
				 QStringLiteral("connector = 'CN1'"))
		      == QStringList({QStringLiteral("1"), QStringLiteral("5"),
				      QStringLiteral("9")}));

		SECTION("et un rétablir la refait")
		{
			bench.project()->undoStack()->redo();
			CHECK(viewValues(bench.project(), QStringLiteral("label"),
					 QStringLiteral("connector = 'CN1'"))
			      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
					      QStringLiteral("3")}));
		}
	}
}

TEST_CASE("T34 — renuméroter la sélection ne touche pas aux autres connecteurs",
	  "[t34][connector][renumber]")
{
	/*
		The half of CU-34.3 that says "without affecting other connectors",
		and the only way to tell it apart from the case above: CN2 is wrong
		too, on purpose. A run over the whole project puts it right; a run
		over the ways of CN1 alone has to leave it wrong.

		Without that difference the two runs would give the same sheet, and a
		scope that quietly ignored the selection would pass.
	*/
	UiBench::ScratchProject bench(holedFixture(),
				      QStringLiteral("connectorrenumber.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	const QList<Element *> selection = pinsOf(sheet, QStringLiteral("CN1"));
	REQUIRE(selection.size() == 3);

	const QList<Element *> scope =
		ProjectRenumberer::components(bench.project(), selection);
	REQUIRE(scope.size() == 3);

	Catalog catalog;
	const RenumberPlan plan = Renumberer::plan(
		ProjectRenumberer::inputsFor(catalog, scope, byConnector()));

	CHECK(plan.entries.size() == 3);
	CHECK(plan.changeCount() == 2);
	CHECK(ProjectRenumberer::applyPlan(scope, plan) == 2);

	CHECK(labelsOnSheet(sheet) == QStringList({QStringLiteral("1"),
						   QStringLiteral("2"),
						   QStringLiteral("3"),
						   QStringLiteral("4"),
						   QStringLiteral("6"),
						   QStringLiteral("K1")}));

		//CN2 still carries the four and the six it was drawn with. The parts
		//list is asked as well as the sheet, because that is where a
		//renumbering that reached one and not the other would show up.
	CHECK(viewValues(bench.project(), QStringLiteral("label"),
			 QStringLiteral("connector = 'CN2'"))
	      == QStringList({QStringLiteral("4"), QStringLiteral("6")}));
}

TEST_CASE("T34 — un connecteur écrit de deux façons est numéroté comme un seul, et signalé",
	  "[t34][connector][renumber]")
{
	/*
		Nothing normalises the field, and this is the shape the consequence
		takes on an open project. The two spellings are one connector for the
		counting - counting them apart restarts the numbering halfway and
		hands two ways the same number - and they are two different strings
		for the parts list, whose filter is an equality on the text.

		So the plan reports the spellings instead of merging them in silence:
		the ways come out right, and whoever reads the warning can put the
		field right afterwards.
	*/
	const QList<Pin> pins = {{200, "1", "CN1"},
				 {240, "5", "cn1"}};
	UiBench::ScratchProject bench(fixtureXml(pins),
				      QStringLiteral("connectorspelling.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	const QList<Element *> scope = ProjectRenumberer::components(bench.project());
	REQUIRE(scope.size() == 2);

	Catalog catalog;
	const RenumberPlan plan = Renumberer::plan(
		ProjectRenumberer::inputsFor(catalog, scope, byConnector()));

	CHECK(plan.inconsistentConnectors() == QStringList({QStringLiteral("CN1")}));

		//One connector, so the second way is the second one. Counted apart,
		//both would have come out as way 1.
	CHECK(ProjectRenumberer::applyPlan(scope, plan) == 1);
	CHECK(labelsOnSheet(sheet) == QStringList({QStringLiteral("1"),
						   QStringLiteral("2")}));

		//And the reason the warning is worth printing: the list beside the
		//drawing does not merge them. Asking for CN1 brings back one of the
		//two ways, and the other is filed under a name of its own.
	CHECK(viewValues(bench.project(), QStringLiteral("label"),
			 QStringLiteral("connector = 'CN1'"))
	      == QStringList({QStringLiteral("1")}));
	CHECK(viewValues(bench.project(), QStringLiteral("label"),
			 QStringLiteral("connector = 'cn1'"))
	      == QStringList({QStringLiteral("2")}));
}
