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
#ifndef DRCPOTENTIALRULES_H
#define DRCPOTENTIALRULES_H

#include "drcfinding.h"
#include "drcrule.h"

#include <QList>
#include <QString>

class Conductor;
class DrcRuleSet;
class DrcScanEngine;
class QETProject;

/**
	@brief The rules a potential answers: the conductors joined into one
	electrical node, followed across folio reports and terminal elements.

	One rule today - a potential carrying two different wire numbers.

	@par Why the walk route, measured and not preferred
	The project data base keeps the text of each conductor, one row each,
	and no potential at all: which conductors make one node is worked out
	by Conductor::relatedPotentialConductors(), over the terminals, the
	report arrows and the terminal elements of the scene. A "duplicated wire
	number" written as a SELECT grouping the conductor table by text would
	answer another question - the same number on two potentials - and that
	one is not a defect.

	@par The same number on two potentials is not reported
	A number repeated on potentials that are not joined is common practice
	for field wiring, and a finding about it would be one the office learns
	to ignore. The inverse rule is therefore not written: the walk below
	groups by potential, and turning it into "number to potentials" is the
	whole of what writing it would take, should a delivered project ever
	show crossed numbering between panels.

	@par Absent is not a second number
	The texts are judged by DrcWireNumbers, which does not count an empty
	text, spacing or the placeholder of a new conductor as a number. A
	potential numbered on one conductor and left blank on the next carries
	one number, and is not reported.

	@par An error, and not a warning
	Two numbers on one node contradict each other inside the drawing: the
	workshop is told to label the same node twice, with two labels, and no
	reading of the folio makes that right. It is one of the two rules born
	as errors, and it is what gives the command line of the checker a
	finding that sets a non-zero exit code without a profile being made up
	for it.
*/
namespace DrcPotentialRules
{
	/// Machine key of "a potential carrying two different wire numbers".
	QString twoWireNumbersIdentifier();

	/**
		Every potential of @a project, each one listed once.

		The folios are walked in order and the conductors of each folio in
		the order the folio holds them; a potential is listed when its
		first conductor is met, and its conductors are listed in that same
		order. So the first conductor of a potential is the one on its
		lowest folio, and two runs over an unchanged project list the same
		potentials the same way.

		Function separation is respected - it is the default of
		relatedPotentialConductors() - so the two sides of a terminal whose
		separation is ticked are two potentials, and may carry two numbers.

		@param project the project to walk. Every folio is built.
		@return one list of conductors per potential
	*/
	QList<QList<Conductor *>> potentialsOf(QETProject *project);

	/**
		Every potential of @a project that carries more than one distinct
		wire number.

		@param project the project to walk
		@param rule the rule as it stands, whose current severity every
		finding copies
		@return one finding per such potential, pointing at its first
		numbered conductor - on the lowest folio the potential is
		numbered on
	*/
	QList<DrcFinding> potentialWithTwoNumbers(QETProject *project,
						  const DrcRule &rule);

	/**
		Put the rules of this file into a register and an engine.

		@param set the register they join
		@param engine the engine that will walk them
		@return how many were taken; fewer than written when one of the
		identifiers was already there, which the register refuses rather
		than replaces.
	*/
	int registerRules(DrcRuleSet &set, DrcScanEngine &engine);
}

#endif // DRCPOTENTIALRULES_H
