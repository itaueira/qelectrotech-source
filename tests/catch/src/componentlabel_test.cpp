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
#include <catch2/catch.hpp>

#include "qt_catch_tostring.h"

#include "../../../sources/autoNum/iecstructure.h"
#include "../../../sources/diagramcontext.h"
#include "../../../sources/label/componentlabelquery.h"
#include "../../../sources/location/locationtree.h"
#include "../../../sources/qetinformation.h"

#include <QList>

/*
	What a component label asks for, and what it makes of the answer.

	Two rules, and both are about not losing something silently. The first
	is which view the question goes to: the parts list view drops the rows
	the user ticked out of the purchase order, and a label collector reading
	it would leave gaps in the marking of a cabinet that nobody sees until
	the cabinet is being assembled. The second is where a revision comes
	from: it is a property of a sheet, so a run that spans two sheets
	carries two revisions, and the moment they quietly become one the label
	starts claiming something the project never said.

	Labelled T21 and not CU-21.x on purpose. Nothing here proves a use case:
	CU-21.10 ends in exporting a real project and finding the excluded item
	on the tape, and CU-21.15 ends in reading two revisions off one run.
	Both are in the screen queue. What is proved here is the arithmetic
	underneath them.

	And what is *not* proved here, said plainly because a green suite reads
	as more than it is: none of these cases opens a project. The calls that
	carry these rules to a real data base live in ComponentLabelCollector,
	out of reach of this suite - remove the applyFolioRevisions() call from
	it, or point its query at the nomenclature view, and every assertion
	below still passes. Both were planted, and both were caught only by the
	cases with a project open, in tests/catch/src/ui/.
*/

namespace {
	/// A row as the collector builds it: column name to value, all strings.
	QHash<QString, QString> row(const QString &label,
				    int folio_position,
				    const QString &excluded = QString())
	{
		QHash<QString, QString> r;
		r.insert(QStringLiteral("label"), label);
		r.insert(QStringLiteral("diagram_position"),
			 QString::number(folio_position));
		if (!excluded.isEmpty()) {
			r.insert(QStringLiteral("exclude_from_bom"), excluded);
		}
		return r;
	}

	/**
		One entry, read by a project that never turned the identification
		structure on - which is the default, and the state of every project
		delivered so far.

		The cases that use it say more than they did before the composition
		was wired in: each of them now also states that with the structure
		off the tag on the tape is the stored field, character for
		character. That is the promise the switch is worth having for, and
		it is checked here on every row a case builds rather than once.
	*/
	LabelEntry entryWithStructureOff(const QHash<QString, QString> &r)
	{
		return ComponentLabelQuery::entryFromRow(
				r, IecStructureSettings(), DiagramContext());
	}

	/**
		The four ways a project can stand towards the location of its
		components, in this order: the structure off; off, with the free
		text field switched in - what a project that turned the structure
		off again keeps; on, with the field left out; on, with the field
		declared a place of the norm.

		Only the last one says the field is a place. The second is there
		because the switch is kept in the file whatever the structure says,
		and it must not be read on its own.
	*/
	QList<IecStructureSettings> everySetting()
	{
		IecStructureSettings off;

		IecStructureSettings off_with_field;
		off_with_field.location_from_element = true;

		IecStructureSettings on;
		on.enabled = true;
		on.display = IecTagDisplay::Full;

		IecStructureSettings on_with_field = on;
		on_with_field.location_from_element = true;

		return {off, off_with_field, on, on_with_field};
	}

	/// One of everySetting(), as a line a failure can be read by.
	std::string describe(const IecStructureSettings &settings)
	{
		return std::string("structure ")
				+ (settings.enabled ? "on" : "off")
				+ ", free text field "
				+ (settings.location_from_element ? "declared a place"
								  : "left out");
	}
}

TEST_CASE("T21 — the label question is asked of the unfiltered view",
	  "[t21][label]")
{
	CHECK(ComponentLabelQuery::viewName()
	      == QStringLiteral("element_label_view"));

	const QString statement = ComponentLabelQuery::selectStatement();

		//The name of the other view must not appear at all. Checking only
		//that the clause is absent would pass for a statement that asks the
		//filtered view, which already has the filter baked in and would
		//never need to repeat it - the exact way this could be got wrong.
	CHECK_FALSE(statement.contains(
			QStringLiteral("element_nomenclature_view")));
	CHECK(statement.contains(QStringLiteral("element_label_view")));
	CHECK_FALSE(statement.contains(QStringLiteral("exclude_from_bom IS NOT")));

		//The column is selected even though it is not filtered on: an
		//excluded item is labelled like any other, and saying so on the row
		//is what lets a writer show the difference.
	CHECK(ComponentLabelQuery::columns()
	      .contains(QStringLiteral("exclude_from_bom")));

		//An order that does not depend on the order rows happen to sit in.
		//A roll printed twice from an unchanged project has to match
		//itself, and without this it would not.
	CHECK(statement.contains(QStringLiteral("ORDER BY")));
}

TEST_CASE("T21 — the revision query asks the sheets, not the project",
	  "[t21][label]")
{
	const QString statement = ComponentLabelQuery::folioRevisionStatement();

	CHECK(statement.contains(QStringLiteral("indexrev")));
	CHECK(statement.contains(QStringLiteral("project_summary_view")));

		//By sheet position, which is the sheet's rank in the project and is
		//unique. Not by folio number: that is a free text field a
		//draughtsman fills in, two sheets may carry the same one, and a
		//project may leave every one of them empty - which would collapse
		//every revision onto one key.
	CHECK(statement.contains(QStringLiteral("pos")));
}

TEST_CASE("T21 — a component row becomes a label, and empty fields take no line",
	  "[t21][label]")
{
	QHash<QString, QString> r = row(QStringLiteral("KM1"), 3);
	r.insert(QStringLiteral("designation"), QStringLiteral("LC1D09"));
	r.insert(QStringLiteral("manufacturer"), QString());
	r.insert(QStringLiteral("manufacturer_reference"),
		 QStringLiteral("LC1D09BD"));
	r.insert(QStringLiteral("function"), QString());
	r.insert(QStringLiteral("location_path"), QStringLiteral("CT1/QCM"));
	r.insert(QStringLiteral("folio"), QStringLiteral("4"));

	const LabelEntry entry = entryWithStructureOff(r);

	CHECK(entry.primary_text == QStringLiteral("KM1"));
	CHECK(entry.source == LabelSource::Component);

		//Two of the four secondary fields were filled in, so two lines and
		//not four: tape is a fixed width and a blank line is width spent on
		//nothing.
	REQUIRE(entry.secondary_texts.count() == 2);
	CHECK(entry.secondary_texts.at(0) == QStringLiteral("LC1D09"));
	CHECK(entry.secondary_texts.at(1) == QStringLiteral("LC1D09BD"));

		//The mark of the norm, and not the path the row stores: the tape
		//names a place the way the folio and the location dialogues do.
	CHECK(entry.location == QStringLiteral("+CT1+QCM"));
	CHECK(entry.folio == QStringLiteral("4"));
	CHECK(entry.folio_position == 3);

		//Empty, and empty on purpose: the revision is not in this row. A
		//caller that forgets the second query gets a label with no revision
		//rather than one carrying a number it invented.
	CHECK(entry.revision.isEmpty());
}

TEST_CASE("T21 — the location falls back to the plain field when there is no path",
	  "[t21][label]")
{
	QHash<QString, QString> r = row(QStringLiteral("Q1"), 1);
	r.insert(QStringLiteral("location_path"), QString());
	r.insert(QStringLiteral("location"), QStringLiteral("QCM"));

		//As it was typed, and with no `+` put in front: this project never
		//said the field is a place of the norm, so the tape does not say it
		//either.
	CHECK(entryWithStructureOff(r).location
	      == QStringLiteral("QCM"));
}

TEST_CASE("T21 — an item kept out of the parts list still becomes a label",
	  "[t21][label]")
{
	const LabelEntry excluded = entryWithStructureOff(
			row(QStringLiteral("X1"), 2, QStringLiteral("true")));
	CHECK(excluded.primary_text == QStringLiteral("X1"));
	CHECK(excluded.excluded_from_bom);

	const LabelEntry ordinary = entryWithStructureOff(
			row(QStringLiteral("X2"), 2, QStringLiteral("false")));
	CHECK_FALSE(ordinary.excluded_from_bom);

		//A row that never carried the field at all - an older project, or
		//a component the box was never shown for - is not excluded either.
	CHECK_FALSE(entryWithStructureOff(
			row(QStringLiteral("X3"), 2)).excluded_from_bom);
}

TEST_CASE("T21 — each label carries the revision of its own sheet",
	  "[t21][label]")
{
	LabelEntryList entries;
	entries << entryWithStructureOff(row(QStringLiteral("KM1"), 1))
		<< entryWithStructureOff(row(QStringLiteral("KM2"), 2))
		<< entryWithStructureOff(row(QStringLiteral("KM3"), 1));

	QHash<int, QString> revisions;
	revisions.insert(1, QStringLiteral("A"));
	revisions.insert(2, QStringLiteral("B"));

	ComponentLabelQuery::applyFolioRevisions(entries, revisions);

		//Two revisions in one run, which is what a strip fed by two sheets
		//prints. It is correct and it looks wrong, so it is written down
		//here as the expected answer rather than left to be discovered on
		//the bench.
	CHECK(entries.at(0).revision == QStringLiteral("A"));
	CHECK(entries.at(1).revision == QStringLiteral("B"));
	CHECK(entries.at(2).revision == QStringLiteral("A"));
}

TEST_CASE("T21 — a sheet with no revision does not borrow one",
	  "[t21][label]")
{
	LabelEntryList entries;
	entries << entryWithStructureOff(row(QStringLiteral("KM1"), 1))
		<< entryWithStructureOff(row(QStringLiteral("KM2"), 7));

	QHash<int, QString> revisions;
	revisions.insert(1, QStringLiteral("A"));

	ComponentLabelQuery::applyFolioRevisions(entries, revisions);

	CHECK(entries.at(0).revision == QStringLiteral("A"));

		//Sheet seven carries no revision index, so neither does its label.
		//Carrying over the previous one would be the one failure nobody
		//could see on the tape: a label that names a revision the sheet it
		//came from never had.
	CHECK(entries.at(1).revision.isEmpty());

		//And the same holds for a row whose sheet could not be read at all.
	LabelEntryList unplaced;
	QHash<QString, QString> broken;
	broken.insert(QStringLiteral("label"), QStringLiteral("KM9"));
	broken.insert(QStringLiteral("diagram_position"), QStringLiteral("--"));
	unplaced << entryWithStructureOff(broken);
	CHECK(unplaced.at(0).folio_position == -1);

	ComponentLabelQuery::applyFolioRevisions(unplaced, revisions);
	CHECK(unplaced.at(0).revision.isEmpty());
}

TEST_CASE("T21 — the tag of a row is composed, never the stored field",
	  "[t21][label][iec]")
{
	/*
		The gap this closes, measured on the fork: no export applied the
		identification structure. The data base keeps
		Element::actualLabel() in the `label` column, so a tape printed
		from that column names a component the folio beside it does not
		name - K3 on the tape, =CT1+QCM-K3 on the sheet - and nobody sees
		it until the two are held together at the bench.
	*/
	const DiagramContext folio = ComponentLabelQuery::folioInformation(
			QStringLiteral("CT1"), QStringLiteral("QCM"));

	IecStructureSettings settings;
	settings.enabled = true;

	SECTION("what the folio says is inherited by the tag on the tape")
	{
		settings.display = IecTagDisplay::Full;
		CHECK(ComponentLabelQuery::entryFromRow(
			      row(QStringLiteral("K3"), 1), settings, folio)
		      .primary_text == QStringLiteral("=CT1+QCM-K3"));

		settings.display = IecTagDisplay::Short;
		CHECK(ComponentLabelQuery::entryFromRow(
			      row(QStringLiteral("K3"), 1), settings, folio)
		      .primary_text == QStringLiteral("-K3"));
	}

	SECTION("what the row carries itself wins, field by field")
	{
		settings.display = IecTagDisplay::Full;

		QHash<QString, QString> r = row(QStringLiteral("K3"), 1);
		r.insert(IecStructure::plantKey(), QStringLiteral("CT2"));
		CHECK(ComponentLabelQuery::entryFromRow(r, settings, folio)
		      .primary_text == QStringLiteral("=CT2+QCM-K3"));

			//The place assigned from the tree of the project, which is
			//the preferred source of the `+` and is read whatever the
			//project said about the free text field.
		QHash<QString, QString> placed = row(QStringLiteral("K3"), 1);
		placed.insert(IecStructure::locationPathKey(),
			      QStringLiteral("QCP1"));
		CHECK(ComponentLabelQuery::entryFromRow(placed, settings, folio)
		      .primary_text == QStringLiteral("=CT1+QCP1-K3"));
	}

	SECTION("the free text switch of the project reaches the row")
	{
			//It is read off the settings that were handed in, and not
			//decided here: were the composition rebuilding its own
			//settings, the second check would come back with +QCM.
		settings.display = IecTagDisplay::Full;

		QHash<QString, QString> r = row(QStringLiteral("K3"), 1);
		r.insert(IecStructure::locationKey(), QStringLiteral("QCP"));

		CHECK(ComponentLabelQuery::entryFromRow(r, settings, folio)
		      .primary_text == QStringLiteral("=CT1+QCM-K3"));

		settings.location_from_element = true;
		CHECK(ComponentLabelQuery::entryFromRow(r, settings, folio)
		      .primary_text == QStringLiteral("=CT1+QCP-K3"));
	}

	SECTION("a terminal number is a connection, in whichever form")
	{
			//The common case of a label, and the one a composition can
			//most easily spoil: a strip of forty terminals whose tape
			//reads -7 instead of 7 is forty wrong labels.
		settings.display = IecTagDisplay::Short;
		CHECK(ComponentLabelQuery::entryFromRow(
			      row(QStringLiteral("7"), 1), settings, folio)
		      .primary_text == QStringLiteral("7"));

			//The full form is not the same string, and the tape has to
			//follow the folio into it: the number is the connection of
			//the norm, written after the colon, with no product invented
			//to carry a dash. CU-10.10 is where the drawing settled it.
		settings.display = IecTagDisplay::Full;
		CHECK(ComponentLabelQuery::entryFromRow(
			      row(QStringLiteral("7"), 1), settings, folio)
		      .primary_text == QStringLiteral("=CT1+QCM:7"));
	}

	SECTION("composing the tag does not touch the other fields of the entry")
	{
			//The location is the row's own place, written as the norm
			//writes it - +CT1+QCM for the path CT1/QCM - and nothing of the
			//composed tag: not the `=` the folio hands down, not the
			//product. It used to be asserted here that it stayed as it was
			//stored; that was the state before the location had a rule of
			//its own, and the rule is checked case by case further down.
		settings.display = IecTagDisplay::Full;

		QHash<QString, QString> r = row(QStringLiteral("K3"), 4);
		r.insert(IecStructure::locationPathKey(),
			 QStringLiteral("CT1/QCM"));
		r.insert(QStringLiteral("designation"), QStringLiteral("LC1D09"));

		const LabelEntry entry =
				ComponentLabelQuery::entryFromRow(r, settings, folio);

		CHECK(entry.location == QStringLiteral("+CT1+QCM"));
		REQUIRE(entry.secondary_texts.count() == 1);
		CHECK(entry.secondary_texts.at(0) == QStringLiteral("LC1D09"));
		CHECK(entry.folio_position == 4);
	}
}

TEST_CASE("T21 — the location on a label is the mark of the norm, never the stored form",
	  "[t21][label][iec]")
{
	/*
		The rule CU-21.14 rests on, checked without a project: a place the
		location tree of the project gave a component is written the way
		IEC 81346 writes it - the `+` repeated once per level - and not the
		path the component stores. The case itself ends at an exported file
		and is proved in tests/catch/src/ui/labelexport_test.cpp; what is
		here is every answer of the rule, one setting of the project at a
		time, which is why it is labelled T21.
	*/
	const QString path = QStringLiteral("QCM1/PLACA");
	const QString mark = QStringLiteral("+QCM1+PLACA");

	SECTION("a path of the tree, whatever the project says about the structure")
	{
		QHash<QString, QString> r = row(QStringLiteral("K3"), 1);
		r.insert(IecStructure::locationPathKey(), path);

		for (const IecStructureSettings &settings : everySetting())
		{
			INFO(describe(settings));
			CHECK(ComponentLabelQuery::entryFromRow(r, settings,
								DiagramContext())
			      .location == mark);
		}

			//And it is the very string the rest of the program writes for
			//that path: the tree's own conversion, and the function the
			//parts list and the table drawn on a folio write a row with. A
			//second spelling of the rule in the label code would pass the
			//loop above and fail one of these the day the two drift.
		CHECK(LocationTree::iecTag(path) == mark);
		CHECK(QETInformation::displayedInfoValue(
			      IecStructure::locationPathKey(), path)
		      == mark);
	}

	SECTION("one level is one prefix, and five levels are five")
	{
		QHash<QString, QString> r = row(QStringLiteral("K3"), 1);

		r.insert(IecStructure::locationPathKey(), QStringLiteral("QCM1"));
		CHECK(entryWithStructureOff(r).location == QStringLiteral("+QCM1"));

		r.insert(IecStructure::locationPathKey(),
			 QStringLiteral("CT1/QCM1/PORTA/PAINEL/TOPO"));
		CHECK(entryWithStructureOff(r).location
		      == QStringLiteral("+CT1+QCM1+PORTA+PAINEL+TOPO"));
	}

	SECTION("the path wins over the free text field, even where the field is a place")
	{
		QHash<QString, QString> r = row(QStringLiteral("K3"), 1);
		r.insert(IecStructure::locationPathKey(), path);
		r.insert(IecStructure::locationKey(), QStringLiteral("X1"));

		for (const IecStructureSettings &settings : everySetting())
		{
			INFO(describe(settings));
			CHECK(ComponentLabelQuery::entryFromRow(r, settings,
								DiagramContext())
			      .location == mark);
		}
	}

	SECTION("the free text field as it was typed, unless the project says it is a place")
	{
		QHash<QString, QString> r = row(QStringLiteral("K3"), 1);
		r.insert(IecStructure::locationKey(), QStringLiteral("X1"));

		const QList<IecStructureSettings> settings = everySetting();
		REQUIRE(settings.count() == 4);

			//Three of the four never said the field is a place - and the
			//second one switched it in under a structure that is off, which
			//the composition of the tag does not read either. In none of
			//them does a `+` appear: the field is older than the norm, and
			//in the project measured for T10 it held the strip the wiring
			//lands on, X1 among them.
		for (int i = 0 ; i < 3 ; ++i)
		{
			INFO(describe(settings.at(i)));
			CHECK(ComponentLabelQuery::entryFromRow(r, settings.at(i),
								DiagramContext())
			      .location == QStringLiteral("X1"));
		}

			//The fourth said it: the field behind the `+` of the norm.
		REQUIRE(settings.at(3).enabled);
		REQUIRE(settings.at(3).location_from_element);
		const LabelEntry entry = ComponentLabelQuery::entryFromRow(
				r, settings.at(3),
				ComponentLabelQuery::folioInformation(
					QStringLiteral("CT1"), QStringLiteral("QCM")));
		CHECK(entry.location == QStringLiteral("+X1"));

			//And it is the `+` the full tag on the sheet writes for the
			//same row: the tape's location and the tag above it name the
			//same place - the component's own, not the QCM of its folio.
		CHECK(entry.primary_text == QStringLiteral("=CT1+X1-K3"));
	}

	SECTION("nothing to say is an empty cell, never a lone +")
	{
		QHash<QString, QString> nothing = row(QStringLiteral("K3"), 1);

		QHash<QString, QString> empty = row(QStringLiteral("K3"), 1);
		empty.insert(IecStructure::locationPathKey(), QString());
		empty.insert(IecStructure::locationKey(), QString());

			//A path that is nothing but a separator names no place: the
			//tree splits it into no code at all. Stored as it is, it would
			//have reached the tape as a lone slash.
		QHash<QString, QString> separator = row(QStringLiteral("K3"), 1);
		separator.insert(IecStructure::locationPathKey(),
				 QStringLiteral("/"));

		for (const IecStructureSettings &settings : everySetting())
		{
			for (const QHash<QString, QString> &r : {nothing, empty, separator})
			{
				INFO(describe(settings) << ", path '"
				     << r.value(IecStructure::locationPathKey())
					.toStdString() << "'");
				CHECK(ComponentLabelQuery::entryFromRow(
					      r, settings, DiagramContext())
				      .location.isEmpty());
			}
		}
	}
}

TEST_CASE("T21 — the lines under the tag are written as every reader of the row writes them",
	  "[t21][label]")
{
	/*
		The other half of the step that writes the location: every value a
		component label reads from the data base goes through
		QETInformation::displayedInfoValue(), the function the parts list
		and the table drawn on a folio already write a row with, so that one
		component does not read two ways on two documents. Besides the
		location, what it changes today is a date, written in the locale of
		the machine - a designation somebody filled with one is the probe.
	*/
	const QString typed = QStringLiteral("2026-10-17");
	const QString written = QETInformation::displayedInfoValue(
			QStringLiteral("designation"), typed);

	QHash<QString, QString> r = row(QStringLiteral("K3"), 1);
	r.insert(QStringLiteral("designation"), typed);
	r.insert(QStringLiteral("function"), QStringLiteral("Pump 2"));

	const LabelEntry entry = entryWithStructureOff(r);
	REQUIRE(entry.secondary_texts.count() == 2);
	CHECK(entry.secondary_texts.at(0) == written);

		//Text that is not a date is written as it stands.
	CHECK(entry.secondary_texts.at(1) == QStringLiteral("Pump 2"));

		//The first check tells a converted value from a raw one only where
		//the machine writes a short date differently from ISO - pt_BR
		//writes 17/10/2026 - and not where the short date is ISO itself.
		//Said, rather than failed: a case reddened for a reason of the
		//platform is a false alarm.
	if (written == typed) {
		WARN("this machine writes a short date as ISO: the first check "
		     "cannot tell a converted designation from a raw one");
	}
}

TEST_CASE("T21 — the columns the composition needs are the ones selected",
	  "[t21][label][iec]")
{
	/*
		A tag composed from a row is only as good as the row. The three
		keys the composition reads have to be in the SELECT, and they are
		asked for by the name IecStructure gives them rather than by a
		literal written twice - a column renamed on one side and not the
		other would compose against empty fields and print a short tag
		that looks perfectly reasonable.
	*/
	const QStringList columns = ComponentLabelQuery::columns();

	CHECK(columns.contains(IecStructure::plantKey()));
	CHECK(columns.contains(IecStructure::locationKey()));
	CHECK(columns.contains(IecStructure::locationPathKey()));

		//And they reach the statement, which is what the collector reads
		//by index.
	const QString statement = ComponentLabelQuery::selectStatement();
	for (const QString &key : {IecStructure::plantKey(),
				   IecStructure::locationKey(),
				   IecStructure::locationPathKey()})
	{
		INFO("missing column: " << key.toStdString());
		CHECK(statement.contains(key));
	}
}

TEST_CASE("T21 — the folio question asks for what the norm inherits",
	  "[t21][label][iec]")
{
	const QString statement = ComponentLabelQuery::folioStructureStatement();

	CHECK(statement.contains(QStringLiteral("project_summary_view")));
	CHECK(statement.contains(IecStructure::plantKey()));

		//The folio keeps its location under a name of its own, which is the
		//one asymmetry between a folio and a component. Asking for the
		//component's name here would answer nothing, and answer it quietly.
	CHECK(statement.contains(IecStructure::folioLocationKey()));
	CHECK_FALSE(statement.contains(IecStructure::locationPathKey()));

		//By sheet position, the same key the revisions come back by.
	CHECK(statement.contains(QStringLiteral("pos")));

	SECTION("and the two values become the information a sheet hands down")
	{
		const DiagramContext info = ComponentLabelQuery::folioInformation(
				QStringLiteral("CT1"), QStringLiteral("QCM"));

		const IecStructure folio =
				IecStructure::fromFolioInformation(info);
		CHECK(folio.plant    == QStringLiteral("CT1"));
		CHECK(folio.location == QStringLiteral("QCM"));
	}
}
