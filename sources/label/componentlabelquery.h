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

#include "../diagramcontext.h"

#include <QHash>
#include <QString>
#include <QStringList>

class IecStructureSettings;

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
		The function and the location each sheet carries, by sheet position:
		what a component inherits from the folio it is drawn on.

		A second question to the same view as the revision, and asked apart
		for a reason that shows on the clock: with the structure off - the
		default, and the state of every project delivered so far - it is not
		asked at all, because nothing is inherited. Folded into the revision
		statement it would be paid for by every export, to compose nothing.
	*/
	QString folioStructureStatement();

	/**
		The information of one sheet, as the composition of a tag reads it,
		built from a row of folioStructureStatement().

		@param plant the `=` the title block carries
		@param locmach the `+` the title block carries

		It is a DiagramContext and not the two strings because that is what
		the drawing hands over - BorderTitleBlock::titleblockInformation() -
		and the composition has to be given the same shape from both sides.
		The keys are asked of IecStructure, which is where the asymmetry
		between a folio and a component lives: the folio keeps its location
		under another name.
	*/
	DiagramContext folioInformation(const QString &plant,
					const QString &locmach);

	/**
		One entry built from one row of selectStatement(), the row given as
		column name to value.

		The revision is deliberately left empty here. It does not come from
		this row - it comes from the sheet - and filling it in needs the
		second query, so a caller that forgets applyFolioRevisions() gets
		labels with no revision rather than labels with a wrong one.

		@param settings the identification structure of the project
		@param folio_info what the sheet of this row hands down, from
		folioInformation()

		The text that goes on the tape is composed through
		IecStructureSettings::composedTag(), the same function the sheet
		draws with - never the `label` column on its own. That column is the
		stored field, and a tape printed from it names a component the sheet
		beside it does not name. The two are asked for rather than defaulted
		on purpose: a collector that has not got them yet does not compile,
		which is the one way to keep the raw field from reaching the tape by
		omission.

		With the structure off, which is what a default constructed
		@a settings says, the composition gives the stored tag back
		untouched.
	*/
	LabelEntry entryFromRow(const QHash<QString, QString> &row,
				const IecStructureSettings &settings,
				const DiagramContext &folio_info);

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
