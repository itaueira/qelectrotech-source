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
#ifndef WIRENAMES_H
#define WIRENAMES_H

#include "autoNum/numerotationcontext.h"

#include <QString>

class Diagram;
class QETProject;

/**
	@brief The name of a wire, F<folio>-<number on the folio> (<square>
	<cross-section>), and the conversion of a project drawn before it (T70).

	The name is a conductor numbering scheme: the text "F", the number of
	the folio (%id), "-", a count that starts again on each folio (%sequf_1)
	and " (%wq %ws)" - the marker of the square of the colour of the wire
	and the cross-section of its point (WireSections). A folio that follows
	the scheme names every wire drawn on it.

	A project drawn before has its names typed by hand, "F5-2 (1mm²)", and
	convertProject() takes it to the scheme without renumbering it: each
	point keeps the number its text carries, read from "F<any number>-<n>"
	at the start of the text, and a point with none takes the next number
	free on its folio - the highest number the folio carries plus one, in
	the order the folio lists its conductors. The cross-section is not read
	from the old text: it is the one of the point, and nothing when no
	component of the point says one.

	A text that is not a number is read for what it says, in this order:
	- "PE" as a whole word at its start is the earth, named
	  earthFormula(), F<folio> PE (<square> <cross-section>): no number,
	  and none taken from the folio;
	- "N" or "F" as a whole word at its start, a text writing a
	  cross-section (mm², mm2) and no text at all take the next free
	  number, with the formula of the name;
	- any other text is a cable: the next free number, and the formula
	  cableFormula() of its type, F<folio>-<number> (<square> Cabo <type>)
	  - a cable has no cross-section of a component to show.
	A point already converted is recognised by its formula, earth or
	cable, and keeps it: converting twice changes nothing.

	A point is the potential of a conductor (WireSections::potentialOf()),
	taken folio by folio: the conductors of a point on a folio take the
	number read on that folio, so a point that crosses a folio report takes,
	on each folio, the number its text carries there. A point holding a wire
	of a panel already built is left alone, whole, as the numbering leaves
	it (ConductorAutoNumerotation::isFrozen()) - its number still counts
	for the next free one.
*/
namespace WireNames
{
		/// "F%id-%sequf_1 (%wq %ws)"
	QString formula();
		/// "F%id PE (%wq %ws)", the name of an earth conductor
	QString earthFormula();
		/**
			@return "F%id-%sequf_1 (%wq Cabo <type>)", @p type written
			as it is - a percent sign in it as a fullwidth one, which
			starts no variable
		*/
	QString cableFormula(const QString &type);
		/**
			@return the type of the cable a typed text names: what follows
			"Cabo " when the text starts with it; otherwise what the first
			pair of brackets holds; otherwise the whole text, trimmed
		*/
	QString cableType(const QString &text);

		/**
			The scheme writing formula(): each folio counts from 1, by 1
			- the initial value the count of a folio starts from when the
			folio is shown for the first time, as the numbering dialog
			writes it
		*/
	NumerotationContext scheme();

		/**
			@return the number @p text carries as the name of a wire, the
			n of "F<any number>-<n>" at its start; -1 when it carries
			none
		*/
	int typedNumber(const QString &text);

		/**
			@return the title of the first conductor numbering of
			@p project, in the order of the titles, whose formula is
			formula(); empty when it has none
		*/
	QString schemeOf(const QETProject *project);

		/**
			Name every wire of @p project by formula(), keeping its number
			(see the namespace): one undo step for the whole project, and
			none at all when nothing changes.

			Every folio follows the scheme afterwards - created when the
			project has none writing formula(), and made the one new
			conductors take - and the count of every folio goes on from
			the highest number it carries, so that the next wire drawn on
			it takes the number after. Folios not built yet are built, as a
			point can cross them.

			@param shown : the folio on screen, or none. The count new
			conductors take is the one of the folio last shown, read when a
			folio is shown (Diagram::loadCndFolioSeq()); the count of
			@p shown is put there in the same undo step, so that the next
			wire drawn without changing folio takes its number after the
			highest
			@return how many conductors changed
		*/
	int convertProject(QETProject *project, Diagram *shown = nullptr);
}

#endif // WIRENAMES_H
