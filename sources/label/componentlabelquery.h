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
#ifndef COMPONENTLABELQUERY_H
#define COMPONENTLABELQUERY_H

#include "labelentry.h"

#include <QHash>
#include <QString>
#include <QStringList>

/**
	@brief What a component label asks the project data base, and what it
	makes of the answer.

	Everything here is a rule and nothing here talks to a data base: the two
	statements are strings, and the two transforms take values in and give
	values back. That is on purpose - it is what lets the questions be read,
	and the answers checked, without a project open.

	The wiring lives next door in ComponentLabelCollector, which is the only
	thing here that needs one. The split is worth stating because it is also
	the split's weakness: nothing in this file can tell whether the collector
	still calls it. Only a check with a project open can, and that is where
	the two cases that cover it were put.
*/
namespace ComponentLabelQuery
{
	/**
		The view the component labels are read from: element_label_view,
		never element_nomenclature_view.

		The two carry the same columns and differ by one clause - the
		nomenclature view drops the rows whose exclude_from_bom is 'true'.
		That box means "keep it off the purchase list"; it never meant "it
		is not in the cabinet", so reading the filtered view here would
		leave gaps in the marking that show up at assembly and nowhere
		earlier.
	*/
	QString viewName();

	/**
		The columns a component label reads, in the order the statement
		selects them.
	*/
	QStringList columns();

	/**
		The whole SELECT, ordered by sheet and then by position on the
		sheet, so that a run comes out in reading order and comes out the
		same way twice.
	*/
	QString selectStatement();

	/**
		The revision of every sheet, by sheet position.

		A second query rather than a column added to the element view: the
		nomenclature view is shared with the parts list and the wiring list,
		and widening it would change what they export. The price is this
		extra round trip, which is the cheaper of the two.
	*/
	QString folioRevisionStatement();

	/**
		One entry built from one row of selectStatement(), the row given as
		column name to value.

		The revision is deliberately left empty here. It does not come from
		this row - it comes from the sheet - and filling it in needs the
		second query, so a caller that forgets applyFolioRevisions() gets
		labels with no revision rather than labels with a wrong one.
	*/
	LabelEntry entryFromRow(const QHash<QString, QString> &row);

	/**
		Carry the revision of each sheet onto the entries read from it.

		@param entries the list, modified in place
		@param revision_by_folio_position what folioRevisionStatement()
		answered: sheet position to revision index

		An entry whose sheet is not in the map keeps an empty revision. It
		does not inherit the one before it, and that matters: a run over a
		strip fed by two sheets prints two different revisions, and the
		moment they silently become one is the moment the label starts
		claiming something the project never said.
	*/
	void applyFolioRevisions(
			LabelEntryList &entries,
			const QHash<int, QString> &revision_by_folio_position);
}

#endif // COMPONENTLABELQUERY_H
