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
#ifndef DRCTITLEBLOCKRULES_H
#define DRCTITLEBLOCKRULES_H

#include "drcfinding.h"
#include "drcrule.h"
#include "drctitleblock.h"

#include <QList>
#include <QString>
#include <QStringList>

class DrcRuleSet;
class DrcScanEngine;
class QETProject;

/**
	@brief The rules the title block of a folio answers.

	One rule today - a folio whose title block leaves a required field
	blank. Which fields are blank, and the sentence, are DrcTitleBlock;
	this file is the walk that hands it the folios.

	@par Why the walk route, and not a query over diagram_info
	The diagram_info table holds exactly what this rule reads - it is
	filled from the same context, one column per key of
	QETInformation::diagramInfoKeys() - and a SELECT over it would find
	the same folios. What it cannot do is point at them: a row of the
	query route turns into a finding on a component when it names one, and
	into a finding with nowhere to go otherwise. The folio itself is the
	thing to open here, and only a finding built on the folio sends the
	reader there. The table also has no column for a field of the user's
	own, which the list of required fields is free to name.

	@par The finding is on the folio, and selects nothing
	There is no component to select: the title block is part of the sheet.
	The panel opens the folio and leaves the selection alone, which is what
	DrcPanel::activateFinding() already does for a finding on a diagram.

	@par One finding per folio, naming every blank field
	Two findings for one folio - "title empty", then "author empty" - would
	send the reader to the same sheet twice to fill in one dialog. One
	finding naming both is one trip.

	@par A warning, and not an error
	A blank title block is a sheet that should not leave the office, and
	nothing in the drawing contradicts itself because of it; the two rules
	born as errors are the ones the drawing cannot be right with. Raising
	this one is one line of the profile.

	@par The required fields are handed in
	The factory profile asks for the title and the author
	(DrcTitleBlock::factoryRequiredKeys()). Whoever delivers projects
	with another requirement hands registerRules() another list; nothing
	here is a constant of the rule, so changing it migrates nothing.
*/
namespace DrcTitleBlockRules
{
	/// Machine key of "a folio whose title block leaves a required field blank".
	QString emptyFieldIdentifier();

	/**
		Every folio of @a project whose title block leaves one of the
		fields of @a required blank.

		@param project the project to walk. Every folio is built: the
		context a title block is drawn from only exists on a built folio.
		@param rule the rule as it stands, whose current severity every
		finding copies
		@param required the fields that must be filled in
		@return one finding per such folio, in folio order, on the folio
		and naming every blank field
	*/
	QList<DrcFinding> emptyFields(QETProject *project,
				      const DrcRule &rule,
				      const QStringList &required);

	/**
		Put the rules of this file into a register and an engine.

		@param set the register they join
		@param engine the engine that will walk them
		@param required the fields every folio must fill in
		@return how many were taken; fewer than written when one of the
		identifiers was already there, which the register refuses rather
		than replaces - and none when @a required names no field, see
		the definition for why.
	*/
	int registerRules(DrcRuleSet &set,
			  DrcScanEngine &engine,
			  const QStringList &required
			  = DrcTitleBlock::factoryRequiredKeys());
}

#endif // DRCTITLEBLOCKRULES_H
