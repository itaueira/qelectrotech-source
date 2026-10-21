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
#ifndef DRCTERMINALRULES_H
#define DRCTERMINALRULES_H

#include "drcfinding.h"
#include "drcrule.h"

#include <QList>
#include <QString>

class DrcRuleSet;
class DrcScanEngine;
class Element;
class QETProject;

/**
	@brief The rule of a component terminal nothing is wired to.

	It walks the folios and not the project data base, and that is
	measured rather than chosen: a terminal reaches the terminal table only
	on the way of a conductor, so a query for "a terminal with no
	conductor" would come back empty on every project - a clean report
	from a rule that never looked.

	@par Which terminals
	The terminals of a component: a plain element, a master and a slave -
	and only of a component that has a wire on another of its terminals.
	A component with no wire at all is left out: on a folio that draws a
	strip as blocks, with the destination of each terminal written beside
	it, nothing is wired by construction, and every terminal of the folio
	would be reported; and a component not wired yet is not a wire
	forgotten. Measured on a real project: 358 findings without this, 338
	of them on components with no wire, 20 with it.

	The terminal of a terminal strip is left out, because the rule that
	judges it is "a terminal with no field destination", and that one waits
	for the data of the field side. A folio report is left out as well: it
	is not a component, it carries no part, and a report with nothing
	wired to it is a question about the report, not about the panel.

	@par A warning, and never an error
	A pole left spare on purpose and a contact nobody needed are drawn
	unwired, and nothing in the drawing tells them from a wire forgotten.
	So the rule is born switched on and as a warning: switched off it would
	find nothing and nobody would ever count how many of the findings are
	spares; and a warning does not change the exit code of the command
	line, so a rule that fires a lot on the first day stops nobody's work.

	@par Which of two identical terminals
	The finding points at the component itself, the one drawn on that
	folio, so two identical symbols are never mixed up; the sentence names
	the terminal, and the position is the cell of the grid the terminal
	sits in, not the one of the component - a long symbol crosses cells.
*/
namespace DrcTerminalRules
{
		/// Machine key of "a component terminal with no conductor".
	QString unconnectedTerminalIdentifier();

		/// Whether the terminals of @a element are judged by the rule.
	bool isJudged(const Element *element);

	/**
		Every terminal with no conductor of a component of @a project
		that has a conductor on another terminal.
		@param project the project to walk
		@param rule the rule as it stands, whose current severity every
		finding copies
		@return one finding per terminal with no conductor, of a component
		that has one on another terminal, with the position of the
		terminal already filled in
	*/
	QList<DrcFinding> unconnectedTerminals(QETProject *project,
					       const DrcRule &rule);

	/**
		Put the rule into a register and an engine.
		@return 1 on a fresh register; 0 when the identifier was already
		there, which the register refuses rather than replaces.
	*/
	int registerRules(DrcRuleSet &set, DrcScanEngine &engine);
}

#endif // DRCTERMINALRULES_H
