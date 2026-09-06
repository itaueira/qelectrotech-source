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
#include "../../../../sources/label/componentlabelcollector.h"
#include "../../../../sources/label/componentlabelquery.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/titleblockproperties.h"

#include <catch2/catch.hpp>

#include <QSet>
#include <QSqlQuery>
#include <QString>

/*
	The collector with a real project behind it.

	The rules it applies are checked without one, in
	tests/catch/src/componentlabel_test.cpp, and they pass there whatever
	the collector does with them. These cases exist because that is not
	enough: a rule that is never called is a rule that is green and absent
	at the same time, and this is the file that can tell the difference.

	Two wires are what they hold. That the query goes to the view without
	the parts list filter - point it back at element_nomenclature_view and
	the case below turns red, while the pure suite stays entirely green.
	And that the revisions are carried onto the entries - delete the
	applyFolioRevisions() call and the same thing happens. Both were
	planted, both were measured, and neither was visible from the other
	suite.

	Labelled T21 and not CU-21.x: CU-21.10 and CU-21.15 both end at a
	printed label held against a real cabinet, and stay in the screen
	queue. What is here is the reading, which is what they are built on.
*/

namespace {
	/**
		An example with several sheets that draw. Which sheets carry the
		components is not assumed - the cases below find them.
	*/
	const QString reference_example = QStringLiteral("affuteuse_250h.qet");

	const QString exclude_key = QStringLiteral("exclude_from_bom");

	/// The first sheets of the project that draw at least one component.
	QList<Diagram *> drawingSheets(const QList<Diagram *> &sheets, int wanted)
	{
		QList<Diagram *> found;
		for (Diagram *sheet : sheets)
		{
			if (!sheet->elements().isEmpty()) {
				found << sheet;
			}
			if (found.count() == wanted) {
				break;
			}
		}
		return found;
	}

	/// Give a sheet a revision index, as the title block dialog would.
	void setRevision(Diagram *sheet, const QString &revision)
	{
		TitleBlockProperties properties =
				sheet->border_and_titleblock.exportTitleBlock();
		properties.indexrev = revision;
		sheet->border_and_titleblock.importTitleBlock(properties);
	}

	/// How many rows the parts list view answers with.
	int nomenclatureRowCount(QETProject *project)
	{
		QSqlQuery query = project->dataBase()->newQuery(
				QStringLiteral("SELECT COUNT(*)"
					       " FROM element_nomenclature_view"));
		if (!query.exec() || !query.next()) {
			return -1;
		}
		return query.value(0).toInt();
	}
}

TEST_CASE("T21 — an item kept out of the parts list is still collected",
	  "[t21][label]")
{
	const QString content = UiBench::fileContent(
			UiBench::examplePath(reference_example));
	REQUIRE_FALSE(content.isEmpty());

	UiBench::ScratchProject scratch(content, QStringLiteral("labels.qet"));
	REQUIRE(scratch.isOpen());

	const QList<Diagram *> sheets = drawingSheets(scratch.diagrams(), 1);
	REQUIRE(sheets.count() == 1);
	REQUIRE_FALSE(sheets.first()->elements().isEmpty());

	ComponentLabelCollector collector(scratch.project());

	const LabelEntryList before = collector.collect();
	INFO("collector said: " << collector.error().toStdString());
	REQUIRE(collector.error().isEmpty());
	REQUIRE_FALSE(before.isEmpty());

	const int nomenclature_before = nomenclatureRowCount(scratch.project());
	REQUIRE(nomenclature_before > 0);

		//The box the user ticks to keep a component off the purchase list.
	Element *excluded = sheets.first()->elements().first();
	DiagramContext information = excluded->elementInformations();
	information.addValue(exclude_key, QStringLiteral("true"));
	excluded->setElementInformations(information);

	const LabelEntryList after = collector.collect();
	REQUIRE(collector.error().isEmpty());

		//The parts list loses it, which is what the box is for...
	CHECK(nomenclatureRowCount(scratch.project()) == nomenclature_before - 1);

		//...and the labels do not. This is the whole case: a component the
		//buyer does not order is still a component the fitter has to find
		//on the rail. Were the collector reading the filtered view, this
		//count would drop with the other one.
	CHECK(after.count() == before.count());

	int excluded_labels = 0;
	for (const LabelEntry &entry : after) {
		if (entry.excluded_from_bom) {
			++excluded_labels;
		}
	}
	CHECK(excluded_labels == 1);
}

TEST_CASE("T21 — one run carries the revision of each sheet it read",
	  "[t21][label]")
{
	const QString content = UiBench::fileContent(
			UiBench::examplePath(reference_example));
	REQUIRE_FALSE(content.isEmpty());

	UiBench::ScratchProject scratch(content, QStringLiteral("revisions.qet"));
	REQUIRE(scratch.isOpen());

	const QList<Diagram *> sheets = drawingSheets(scratch.diagrams(), 2);
	REQUIRE(sheets.count() == 2);

	const int first_position = scratch->folioIndex(sheets.at(0)) + 1;
	const int second_position = scratch->folioIndex(sheets.at(1)) + 1;
	REQUIRE(first_position != second_position);

	setRevision(sheets.at(0), QStringLiteral("A"));
	setRevision(sheets.at(1), QStringLiteral("B"));

	ComponentLabelCollector collector(scratch.project());
	const LabelEntryList entries = collector.collect();
	INFO("collector said: " << collector.error().toStdString());
	REQUIRE(collector.error().isEmpty());
	REQUIRE_FALSE(entries.isEmpty());

	QSet<QString> revisions_of_first, revisions_of_second;
	for (const LabelEntry &entry : entries)
	{
		if (entry.folio_position == first_position) {
			revisions_of_first.insert(entry.revision);
		} else if (entry.folio_position == second_position) {
			revisions_of_second.insert(entry.revision);
		}
	}

		//Every label of a sheet carries that sheet's index, and only that
		//one. A single revision for the whole run would mean the collector
		//invented a project-wide value the .qet file does not hold.
	REQUIRE(revisions_of_first.count() == 1);
	REQUIRE(revisions_of_second.count() == 1);
	CHECK(*revisions_of_first.constBegin() == QStringLiteral("A"));
	CHECK(*revisions_of_second.constBegin() == QStringLiteral("B"));

		//And the two are both in the same export, which is the part that
		//looks like a defect on the bench and is not one.
	CHECK(*revisions_of_first.constBegin() != *revisions_of_second.constBegin());
}

TEST_CASE("T21 — the collected list is in sheet order and is the same twice",
	  "[t21][label]")
{
	const QString content = UiBench::fileContent(
			UiBench::examplePath(reference_example));
	REQUIRE_FALSE(content.isEmpty());

	UiBench::ScratchProject scratch(content, QStringLiteral("order.qet"));
	REQUIRE(scratch.isOpen());

	ComponentLabelCollector collector(scratch.project());
	const LabelEntryList first_run = collector.collect();
	REQUIRE(collector.error().isEmpty());
	REQUIRE(first_run.count() > 1);

	int previous = first_run.first().folio_position;
	for (const LabelEntry &entry : first_run)
	{
		INFO("label " << entry.primary_text.toStdString());
		CHECK(entry.folio_position >= previous);
		previous = entry.folio_position;
	}

		//Twice from an unchanged project, and the same both times: a roll
		//reprinted after a look at the drawing has to match the one already
		//stuck on the rail.
	const LabelEntryList second_run = collector.collect();
	REQUIRE(second_run.count() == first_run.count());
	for (int i = 0 ; i < first_run.count() ; ++i)
	{
		INFO("entry " << i);
		CHECK(second_run.at(i).primary_text == first_run.at(i).primary_text);
		CHECK(second_run.at(i).folio_position == first_run.at(i).folio_position);
	}
}

TEST_CASE("T21 — the collector says so when there is no project",
	  "[t21][label]")
{
	ComponentLabelCollector collector(nullptr);
	CHECK(collector.collect().isEmpty());
	CHECK_FALSE(collector.error().isEmpty());
}
