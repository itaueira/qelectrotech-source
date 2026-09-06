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

#include "../../../sources/label/componentlabelquery.h"

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

	const LabelEntry entry = ComponentLabelQuery::entryFromRow(r);

	CHECK(entry.primary_text == QStringLiteral("KM1"));
	CHECK(entry.source == LabelSource::Component);

		//Two of the four secondary fields were filled in, so two lines and
		//not four: tape is a fixed width and a blank line is width spent on
		//nothing.
	REQUIRE(entry.secondary_texts.count() == 2);
	CHECK(entry.secondary_texts.at(0) == QStringLiteral("LC1D09"));
	CHECK(entry.secondary_texts.at(1) == QStringLiteral("LC1D09BD"));

	CHECK(entry.location == QStringLiteral("CT1/QCM"));
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

	CHECK(ComponentLabelQuery::entryFromRow(r).location
	      == QStringLiteral("QCM"));
}

TEST_CASE("T21 — an item kept out of the parts list still becomes a label",
	  "[t21][label]")
{
	const LabelEntry excluded = ComponentLabelQuery::entryFromRow(
			row(QStringLiteral("X1"), 2, QStringLiteral("true")));
	CHECK(excluded.primary_text == QStringLiteral("X1"));
	CHECK(excluded.excluded_from_bom);

	const LabelEntry ordinary = ComponentLabelQuery::entryFromRow(
			row(QStringLiteral("X2"), 2, QStringLiteral("false")));
	CHECK_FALSE(ordinary.excluded_from_bom);

		//A row that never carried the field at all - an older project, or
		//a component the box was never shown for - is not excluded either.
	CHECK_FALSE(ComponentLabelQuery::entryFromRow(
			row(QStringLiteral("X3"), 2)).excluded_from_bom);
}

TEST_CASE("T21 — each label carries the revision of its own sheet",
	  "[t21][label]")
{
	LabelEntryList entries;
	entries << ComponentLabelQuery::entryFromRow(row(QStringLiteral("KM1"), 1))
		<< ComponentLabelQuery::entryFromRow(row(QStringLiteral("KM2"), 2))
		<< ComponentLabelQuery::entryFromRow(row(QStringLiteral("KM3"), 1));

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
	entries << ComponentLabelQuery::entryFromRow(row(QStringLiteral("KM1"), 1))
		<< ComponentLabelQuery::entryFromRow(row(QStringLiteral("KM2"), 7));

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
	unplaced << ComponentLabelQuery::entryFromRow(broken);
	CHECK(unplaced.at(0).folio_position == -1);

	ComponentLabelQuery::applyFolioRevisions(unplaced, revisions);
	CHECK(unplaced.at(0).revision.isEmpty());
}
