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
#ifndef DRCSTRIPRULES_H
#define DRCSTRIPRULES_H

#include "drcfinding.h"
#include "drcrule.h"

#include <QList>
#include <QString>

class DrcRuleSet;
class DrcScanEngine;
class QETProject;

/**
	@brief The rules a terminal strip answers: the strips of the project, as
	the strip editor holds them and the plan of each strip draws them.

	Three rules: a number carried by two terminals of one strip, a strip
	with no terminal, and two strips with the same name. When two numbers
	are the same, what the name of a strip is in a sentence, and the
	sentences, are DrcStripNumbers; this file is the walk that hands it the
	terminals.

	@par Why the walk route, measured and not preferred
	The project data base has no table for terminal strips: which strip a
	terminal belongs to, its place on the rail and the levels of a terminal
	exist only in the strips the project holds in memory. Any of the three
	written as a SELECT would come back with nothing and report a clean
	project.

	@par Only what is drawn on a folio
	A terminal whose symbol was deleted from its folio stays in its strip:
	deleting the symbol does not take it off the strip, and the undo stack
	keeps the symbol alive. It is not on any folio, the next opening of the
	file drops it from the strip (TerminalStrip::fromXml() finds no symbol
	with its uuid), and a finding pointing at it would send the reader
	nowhere. So it takes part in no comparison, and a strip
	whose every terminal was deleted is a strip with no terminal - which is
	what it will be once saved, and what the draughtsman who deleted them
	is looking at.

	@par A repeated number is an error; the other two are warnings
	Two terminals of one strip with the same number contradict each other
	inside the drawing: the plan of the strip and the labels printed for
	the workshop show the number twice, and whoever wires the panel cannot
	tell which terminal a wire goes to. No reading of the folio makes that
	right, and it is the same reason the duplicated tag is an error. A
	number repeated in two strips is not repeated at all - 3 of X1 and 3 of
	X4 are two terminals - and is not looked at.

	A strip with no terminal is a state of the strip and not a contradiction:
	it is left behind by a project copied from another one, or by deleting
	the symbols of a strip, and it changes nothing on any folio. It is a
	warning, which a build server reads without failing. So is a name
	carried by two strips: the tree tells them apart, the plans do not.
*/
namespace DrcStripRules
{
	/// Machine key of "a number carried by two terminals of one strip".
	QString duplicateTerminalNumberIdentifier();
	/// Machine key of "a terminal strip with no terminal".
	QString emptyStripIdentifier();
	/// Machine key of "two terminal strips with the same name".
	QString duplicateStripNameIdentifier();

	/**
		Every number carried by more than one terminal of a strip, strip by
		strip.

		The number compared is the one the plan of the strip draws under
		the terminal (TerminalStripDrawer::Helpers::terminalNumber()), and
		the levels of one terminal are not compared with each other.

		@param project the project to walk
		@param rule the rule as it stands, whose current severity every
		finding copies
		@return one finding per such number and strip, pointing at the
		terminal nearest the start of the rail, and naming the place on the
		rail, the folio and the cell of every terminal carrying it
	*/
	QList<DrcFinding> duplicateTerminalNumbers(QETProject *project,
						   const DrcRule &rule);

	/**
		Every strip of @a project with no terminal drawn on a folio.

		@param project the project to walk
		@param rule the rule as it stands
		@return one finding per such strip, about the project: a strip is on
		no folio, and the finding has nowhere to send the reader
	*/
	QList<DrcFinding> emptyStrips(QETProject *project, const DrcRule &rule);

	/**
		Every name carried by more than one strip of @a project.

		@param project the project to walk
		@param rule the rule as it stands
		@return one finding per such name, about the project
	*/
	QList<DrcFinding> duplicateStripNames(QETProject *project,
					      const DrcRule &rule);

	/**
		Put the rules of this file into a register and an engine.

		@param set the register they join
		@param engine the engine that will walk them
		@return how many were taken: three on a fresh register; fewer when
		one of the identifiers was already there, which the register
		refuses rather than replaces.
	*/
	int registerRules(DrcRuleSet &set, DrcScanEngine &engine);
}

#endif // DRCSTRIPRULES_H
