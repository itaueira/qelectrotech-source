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

#include "../../../../sources/autoNum/iecstructure.h"
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

	/// One element of the fixture: a tag, a place on the sheet, an identity.
	QString elementXml(const QString &uuid, int x, const QString &label)
	{
		return QStringLiteral(
				"<element x=\"%1\" y=\"100\" z=\"10\" prefix=\"\""
				" freezeLabel=\"false\" orientation=\"0\""
				" type=\"embed://bench/contactor.elmt\""
				" uuid=\"%2\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%3"
				"</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(QString::number(x), uuid, label);
	}

	/**
		A project of two components and nothing else.

		Not an example of the collection, and for the reason the screen
		roteiro gives when it draws its own folio: these two tags are
		chosen, so what the composition owes for each of them is known
		beforehand instead of being read off whatever an example happens to
		carry. K1 is a designation of the norm and takes the dash; 7 is a
		terminal number, which is a connection and takes none.
	*/
	QString iecFixtureXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"contactor.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Contactor</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>")
		       + elementXml(
				 QStringLiteral("{c0ffee21-0000-4000-8000-000000000001}"),
				 100, QStringLiteral("K1"))
		       + elementXml(
				 QStringLiteral("{c0ffee21-0000-4000-8000-000000000002}"),
				 300, QStringLiteral("7"))
		       + QStringLiteral(
			       "</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/// The tags of a run, sorted: what is compared is the set, not the order.
	QStringList primaryTexts(const LabelEntryList &entries)
	{
		QStringList texts;
		for (const LabelEntry &entry : entries) {
			texts << entry.primary_text;
		}
		texts.sort();
		return texts;
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

/*
	CU-21.7, and the measurement it closes.

	No export of the fork applied the identification structure. The data base
	keeps Element::actualLabel() in its `label` column, so every export - the
	parts list, the wiring list, the command line - wrote the stored field. A
	project that had turned the norm on drew =CT1+QCM-K1 on the sheet and
	would have printed K1 on the tape; the two are only ever held together at
	the bench, with the tape already spent.

	What is checked is not that the collector composes something, but that it
	composes **what the sheet draws**: the tags of a run are held against
	Element::displayedLabel() of the very components they were read from. No
	second implementation of the composition passes that, however carefully
	written, which is why the composition was extracted into
	IecStructureSettings::composedTag() instead of being repeated here.

	What this does not prove, said plainly because a green case reads as more
	than it is: nothing here writes a file. A tape coming out of a printer
	with the right tag on it is CU-21.2, it needs a writer that does not
	exist yet, and it stays in the screen queue.
*/
TEST_CASE("CU-21.7 - with the structure on, the tape carries the composed tag",
	  "[t21][label][iec]")
{
	UiBench::ScratchProject scratch(iecFixtureXml(),
					QStringLiteral("iec-labels.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 2);

		//The folio hands down the function and the location: this is where
		//the inheritance of the norm starts.
	TitleBlockProperties folio =
			sheet->border_and_titleblock.exportTitleBlock();
	folio.plant = QStringLiteral("CT1");
	folio.locmach = QStringLiteral("QCM");
	sheet->border_and_titleblock.importTitleBlock(folio);

	ComponentLabelCollector collector(scratch.project());

	SECTION("off, the run carries the stored tags, unchanged")
	{
		REQUIRE(scratch.project()->iecSettings().enabled == false);

		const LabelEntryList entries = collector.collect();
		INFO("collector said: " << collector.error().toStdString());
		REQUIRE(collector.error().isEmpty());

		CHECK(primaryTexts(entries)
		      == QStringList({QStringLiteral("7"), QStringLiteral("K1")}));
	}

	SECTION("on, the run carries the tag of the norm")
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Full;
		scratch.project()->setIecSettings(settings);

		const LabelEntryList entries = collector.collect();
		INFO("collector said: " << collector.error().toStdString());
		REQUIRE(collector.error().isEmpty());

			//The case itself: the tape says what the folio says, and not
			//the bare suffix the field holds.
			//
			//Both tags are composed here, and the second one is the one
			//worth reading twice. A tag that is nothing but a number is
			//not a product - the dash of the norm marks a product - so it
			//is read as the connection, and the full form says the plant,
			//the location and the connection with no product invented in
			//between: =CT1+QCM:7, with the colon of the norm. That is not
			//a decision of this collector; it is what T10 settled and what
			//the sheet draws, fixed in tests/catch/src/iecstructure_test.cpp
			//by CU-10.10, "o modo completo diz o que sabe, e nao inventa
			//produto".
			//
			//The first expectation written here was "7", copied from the
			//short form without measuring the full one. It is the short
			//form that leaves a number alone, and the section below is
			//where that is checked - the two say different things about
			//the same component on purpose, and a tape has to say the one
			//the folio beside it says.
		CHECK(primaryTexts(entries)
		      == QStringList({QStringLiteral("=CT1+QCM-K1"),
				      QStringLiteral("=CT1+QCM:7")}));

			//And no dash was glued to the number on the way. That is the
			//failure T10 measured on a real project - 173 labels reading
			//-7 - and a tape repeating it would be 173 labels to peel off.
		for (const LabelEntry &entry : entries) {
			INFO("tag: " << entry.primary_text.toStdString());
			CHECK_FALSE(entry.primary_text.contains(
					QStringLiteral("-7")));
		}
	}

	SECTION("short, the run carries the short form of the same tag")
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Short;
		scratch.project()->setIecSettings(settings);

		const LabelEntryList entries = collector.collect();
		REQUIRE(collector.error().isEmpty());

			//And a terminal number is still the number: the dash of the
			//norm marks a product, and forty terminals reading -7 would be
			//forty labels to peel off again.
		CHECK(primaryTexts(entries)
		      == QStringList({QStringLiteral("-K1"), QStringLiteral("7")}));
	}

	SECTION("the tape and the sheet say the same thing, in the three modes")
	{
		/*
			The assertion the extraction exists for. A second
			implementation of the composition passes every other case of
			this file and fails this one the day the two drift - which is
			what was going to happen, the composition having been
			reachable only through an Element.
		*/
		IecStructureSettings settings;
		settings.enabled = true;

		for (IecTagDisplay display : {IecTagDisplay::Short,
					      IecTagDisplay::Context,
					      IecTagDisplay::Full})
		{
			settings.display = display;
			scratch.project()->setIecSettings(settings);

			QStringList drawn = UiBench::displayedLabels(sheet);
			drawn.sort();

			const LabelEntryList entries = collector.collect();
			REQUIRE(collector.error().isEmpty());

			INFO("display mode "
			     << IecStructureSettings::displayToString(display)
					.toStdString());
			CHECK(primaryTexts(entries) == drawn);
		}
	}

	SECTION("composing the tape writes nothing back")
	{
			//The promise that makes the switch safe: the composition is
			//printed, never stored. Were the collector writing back what
			//it composed, turning the norm off again would no longer give
			//the drawing back.
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Full;
		scratch.project()->setIecSettings(settings);

		REQUIRE_FALSE(collector.collect().isEmpty());

		QStringList stored;
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements) {
			stored << element->elementInformations()
				  .value(QStringLiteral("label")).toString();
		}
		stored.sort();

		CHECK(stored
		      == QStringList({QStringLiteral("7"), QStringLiteral("K1")}));
	}
}
