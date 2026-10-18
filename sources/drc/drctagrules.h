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
#ifndef DRCTAGRULES_H
#define DRCTAGRULES_H

#include "drcfinding.h"
#include "drcrule.h"

#include <QList>
#include <QString>

class DrcRuleSet;
class DrcScanEngine;
class Element;
class QETProject;

/**
	@brief The rules a tag answers: the designation a component carries on
	the folio.

	One rule today - two components carrying the same tag. When the two
	tags are the same, what the two components are, and the sentence, are
	DrcDuplicateTags; this file is the walk that hands it the components.

	@par Why the walk route, measured and not preferred
	The element_info table of the project data base holds the tag the
	component carries, and not the tag the folio draws: composing it with
	the IEC 81346 structure is done when the text is drawn, and on purpose
	never written back. A "duplicated tag" written as a SELECT grouping
	that column would answer with the structure off and keep answering the
	same when it is turned on - reporting -K3 in two cabinets that the
	drawing itself tells apart. The composition needs the title block of
	the sheet each component sits on, and the walk has it at hand.

	@par Which components are compared
	Simple symbols and masters, and nothing else:

	- a slave contact is left out by kind, and not by luck. A contact
	  linked to its coil normally carries no tag of its own - the one
	  under it is the coil's - but one drawn with the tag of its coil
	  typed in, and the slaves of a programmable controller, which are
	  given the tag of their master, carry it all the same. Each of those
	  is the same device as its master, not a second one; and a contact
	  with no coil is already the finding of another rule;
	- a terminal is left out: its number is given by the strip it
	  belongs to, and a number repeated inside a strip is a question for
	  the strip and not for this rule. The master that draws a terminal on
	  a strip sheet is left out for the same reason - it is the terminal,
	  seen from its strip;
	- folio report arrows and thumbnails carry no tag, and are not
	  components.

	A symbol that is not a component - a frame, a cover page - is a simple
	symbol too, and is compared like one; it carries no tag, and a missing
	tag takes part in no comparison. Whether a component with no tag is a
	finding of its own is a separate rule, and it is not written here.

	@par An error, and not a warning
	Two components with one tag contradict each other inside the drawing:
	the parts list, the cross references and the labels printed for the
	workshop cannot tell them apart, and no reading of the folio makes
	that right. It is the other of the two rules born as errors.
*/
namespace DrcTagRules
{
	/// Machine key of "two components carrying the same tag".
	QString duplicateTagIdentifier();

	/**
		Whether @a element takes part in the comparison of tags: a simple
		symbol or a master, and not the master that draws a terminal on a
		strip sheet. See the description of this namespace for why.
	*/
	bool isCompared(const Element *element);

	/**
		Every tag of @a project carried by more than one component.

		The folios are walked in order, and the components of each folio
		from the top of the sheet down and, on one row, from the left, so
		that the first member of a group is the same on every run whatever
		order the scene hands its items out in.

		@param project the project to walk. Every folio is built.
		@param rule the rule as it stands, whose current severity every
		finding copies
		@return one finding per such tag, pointing at its first member -
		on the lowest folio, the topmost and then leftmost there - and
		naming the folio and the cell of every member
	*/
	QList<DrcFinding> duplicateTags(QETProject *project, const DrcRule &rule);

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

#endif // DRCTAGRULES_H
