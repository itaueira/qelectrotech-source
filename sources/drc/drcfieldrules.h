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
#ifndef DRCFIELDRULES_H
#define DRCFIELDRULES_H

#include <QString>

class DrcRuleSet;
class DrcSqlEngine;
class Element;

/**
	@brief The rules a component answers with one of its information
	fields left empty.

	They are of the Sql route: the field is a column of element_info, one
	row per component, and the question is a SELECT over the base tables.
	Never over element_nomenclature_view - that view publishes no uuid, so
	a finding read from it would not know where to go, and its WHERE drops
	the components kept out of the bill of materials, which would then be
	checked by no rule at all.

	@par Every query names the component it found
	Each statement selects the uuid of the offending element under
	elementUuidColumn(). That column is the whole contract with the step
	that turns a row into a finding: the row is looked up among the
	components of the project and the finding points at it, with the folio
	and the position written the way the walk route writes them. A row
	whose uuid names nothing - a data base behind the project - is still
	listed, without a target, rather than dropped.

	@par Empty means three things in this data base, not one
	A key the component never had is bound as a null string and lands as
	NULL; a key holding an empty text lands as ''; and a key typed as
	spaces lands as spaces. TRIM(col) = '' catches the second and the
	third and is NULL - so not true - on the first, which is the component
	that never had the field: the one the rule exists for. blankCondition()
	is the predicate that covers the three, and it trims the very
	characters QString::trimmed() trims, so it agrees with the C++ reading
	of the same field.

	The first shape is not a corner case. In the 24 projects of examples/,
	no component carries part_code at all and every one of those cells is
	NULL: TRIM(part_code) = '' finds no component without a part in any of
	them, where every component of every one of them has none.
*/
namespace DrcFieldRules
{
	/// Machine key of "a component bought without a catalogue part".
	QString elementWithoutPartIdentifier();

	/// The column every statement here selects the element uuid under.
	QString elementUuidColumn();

	/**
		The SQL that is true when @a column holds nothing.

		True for NULL, for the empty text, and for a text made only of
		the characters QString::trimmed() removes - so that it says
		exactly what QString::trimmed().isEmpty() says about the same
		value. Plain TRIM() would not: it removes spaces and nothing else,
		and a tab or a non-breaking space pasted from a supplier sheet
		would read as a code.
	*/
	QString blankCondition(const QString &column);

	/**
		The question behind elementWithoutPartIdentifier().

		The SQL reading of CatalogAssignment::isWithoutPart(), limited
		to the kinds CatalogProjectActions::buyableComponentTypes() names,
		and leaving out the components kept out of the bill of materials.
	*/
	QString elementWithoutPartStatement();

	/**
		The sentence of one finding of the rule @a identifier on
		@a element, already translated.

		@return an empty string for a rule that is not one of these, or
		for no element: the caller then lists the row without a target
		instead of inventing a sentence for it.
	*/
	QString findingText(const QString &identifier, Element *element);

	/**
		Put the rules of this file into a register and the engine that
		asks them.

		@return how many were taken; fewer than written when one of the
		identifiers was already there, which the register refuses rather
		than replaces.
	*/
	int registerRules(DrcRuleSet &set, DrcSqlEngine &engine);
}

#endif // DRCFIELDRULES_H
