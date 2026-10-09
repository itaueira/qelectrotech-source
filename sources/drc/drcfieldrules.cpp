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
#include "drcfieldrules.h"

#include "drclinkrules.h"
#include "drcrule.h"
#include "drcruleset.h"

#include "../catalog/catalogassignment.h"
#include "../catalog/ui/catalogprojectactions.h"
#include "../dataBase/drcsqlengine.h"
#include "../dataBase/projectdatabase.h"

#include <QChar>
#include <QCoreApplication>
#include <QStringList>

namespace
{
	/**
		The characters QString::trimmed() removes, as a call to SQLite's
		char().

		Generated from QChar::isSpace(), which is the test trimmed()
		applies to each end, rather than written out: a list typed by hand
		is a second definition of "blank", and the day it misses one
		character the panel and the missing-part report disagree on a
		project with nobody able to say why. Every such character is in
		the basic plane, so the walk stops there.
	*/
	QString trimmedCharacters()
	{
		QStringList codes;
		for (char32_t code = 0 ; code <= 0xFFFF ; ++code)
		{
			if (QChar::isSpace(code)) {
				codes << QString::number(quint32(code));
			}
		}
		return QStringLiteral("char(")
			+ codes.join(QLatin1Char(','))
			+ QStringLiteral(")");
	}
}

/**
	@brief DrcFieldRules::elementWithoutPartIdentifier
	@return the machine key of the rule. Never translated: the project
	file writes it.
*/
QString DrcFieldRules::elementWithoutPartIdentifier()
{
	return QStringLiteral("element_without_part");
}

/**
	@brief DrcFieldRules::elementUuidColumn
	@return the name the uuid of the offending element is selected under
*/
QString DrcFieldRules::elementUuidColumn()
{
	return QStringLiteral("element_uuid");
}

/**
	@brief DrcFieldRules::blankCondition
	@param column : the column to test, with its table alias
	@return the SQL that is true when the column holds nothing. See the
	header for the three shapes of "nothing" it covers.
*/
QString DrcFieldRules::blankCondition(const QString &column)
{
	static const QString characters = trimmedCharacters();
	return QStringLiteral("COALESCE(TRIM(%1, %2), '') = ''")
		.arg(column, characters);
}

/**
	@brief DrcFieldRules::elementWithoutPartStatement
	@return the SELECT behind the missing-part rule

	Three conditions, each one the SQL form of something already written
	elsewhere, and none of them a new definition:

	- the kinds are CatalogProjectActions::buyableComponentTypes(), the set
	  the missing-part report walks the folios with. The slave contact is
	  one of them, and it has a row here because the element tables take
	  every kind of projectDataBase::populatedElementTypes() - without that
	  row the two readings would part company on every contact;
	- the part is missing by CatalogAssignment::isWithoutPart(), through
	  blankCondition();
	- a component kept out of the bill of materials is not asked for a
	  part: whoever ticked it said it is not bought. The test is the very
	  clause the parts list drops it with, the literal 'true', so that a
	  component is excused here exactly when it is absent from the list.
	  The report of the catalogue does not excuse it, and that is the one
	  difference between the two readings.

	The folio and the cell are selected for the reader of a row that
	cannot be found among the components, not for navigation: the two
	columns named pos are aliased apart, because diagram.pos is the order
	of the sheet and element.pos is the cell of the grid.
*/
QString DrcFieldRules::elementWithoutPartStatement()
{
	return QStringLiteral("SELECT e.uuid AS %1,"
			      " ei.label AS label,"
			      " d.pos AS folio,"
			      " e.pos AS position"
			      " FROM element e"
			      " JOIN element_info ei ON ei.element_uuid = e.uuid"
			      " LEFT JOIN diagram d ON d.uuid = e.diagram_uuid"
			      " WHERE (ei.exclude_from_bom IS NOT 'true')")
			.arg(elementUuidColumn())
		+ projectDataBase::elementTypeClause(
			CatalogProjectActions::buyableComponentTypes(),
			QStringLiteral("e.type"))
		+ QStringLiteral(" AND ")
		+ blankCondition(QStringLiteral("ei.")
				 + CatalogAssignment::partCodeKey())
		+ QStringLiteral(" ORDER BY d.pos, e.pos, e.uuid");
}

/**
	@brief DrcFieldRules::findingText
	@param identifier : the rule that found the row
	@param element : the component the row names
	@return the sentence, or an empty string for a rule of another file
*/
QString DrcFieldRules::findingText(const QString &identifier, Element *element)
{
	if (!element) {
		return QString();
	}

	if (identifier == elementWithoutPartIdentifier())
	{
		return QCoreApplication::translate(
			       "DrcFieldRules",
			       "%1 n'a aucune pièce du catalogue.")
		       .arg(DrcLinkRules::labelOf(element));
	}

	return QString();
}

/**
	@brief DrcFieldRules::registerRules
	@param set : the register the rules join
	@param engine : the engine that asks them
	@return how many were taken

	A warning, switched on: a missing part stops nobody from drawing, and
	a drawing still being made has parts nobody has chosen yet.
*/
int DrcFieldRules::registerRules(DrcRuleSet &set, DrcSqlEngine &engine)
{
	int taken = 0;

	if (engine.addRule(set,
			   DrcRule(elementWithoutPartIdentifier(),
				   QCoreApplication::translate(
					   "DrcFieldRules",
					   "Composant à acheter sans pièce "
					   "du catalogue"),
				   DrcSeverity::Warning,
				   DrcRoute::Sql),
			   elementWithoutPartStatement())) {
		++taken;
	}

	return taken;
}
