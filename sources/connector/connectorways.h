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
#ifndef CONNECTORWAYS_H
#define CONNECTORWAYS_H

#include <QString>
#include <QStringList>

/**
	@brief What a connector has, what the folios draw of it, and what is
	left over: the reserve.

	Handed the pinout of the catalogue part - CatalogPart::pinLabels() is
	what it is fed with - and the pin labels the folios carry on one
	connector, it answers with three lists that do not overlap: the ways in
	use, the ways in reserve, and the labels the part has no way for. Two
	lists of strings in, lists and counts out: no Element, no Diagram, no
	catalogue object and no project, which is what lets "sixteen ways, nine
	used, seven reserve" be settled without a folio open.

	It answers about one connector. Which pins belong to which connector is
	the caller's question, and a caller that groups them has to group them
	with Renumberer::connectorKey - the one already used to decide that
	"CN1", "cn1" and "CN1 " are one connector. A second normalising of the
	same field, written here, would be two parts of one program disagreeing
	about how many connectors there are.

	@par A way in reserve and a label the part has no way for are not one thing

	A way of the part nothing draws is spare: it is there, it is crimpable,
	and it is what "seven ways reserve" counts. A label the folios carry
	that the part does not declare is a mistake - the part was catalogued
	wrong, or the label was typed wrong - and it is not spare capacity of
	anything. Handing the second to the first is how the reserve starts
	lying: a sixteen way connector drawn with nine pins, one of them typed
	"9 " where the part says "9", has eight ways used and eight spare, and
	the way that reads spare is in fact wired. The total would still add
	up, and it would send an electrician to crimp a hole that is taken.

	So they are kept apart, and the arithmetic says so out loud: while the
	pinout is known, usedCount() + reserveCount() == wayCount(), and
	notOnPart() sits outside that sum by construction.

	@par A part with no pinout has no reserve - not a reserve of zero

	Nobody having written down how many ways the connector has, and the
	connector having no way left, are two different answers, and only one
	of them can be printed on a crimping guide. isKnown() tells them apart,
	and the counts that would have to be invented read unknownCount()
	rather than nought:

	| Reading           | Pinout known          | Pinout not known |
	|-------------------|-----------------------|------------------|
	| ways, reserve     | the part              | empty            |
	| notOnPart         | what the part lacks   | empty            |
	| wayCount          | how many              | unknownCount()   |
	| usedCount         | how many              | unknownCount()   |
	| reserveCount      | how many              | unknownCount()   |
	| drawn, drawnCount | what the folios carry | the same         |

	notOnPart() is empty rather than everything: a label cannot be said to
	be absent from a pinout nobody wrote. What the folios draw is still
	readable, and is still read, because "this connector has nine pins
	drawn and no part catalogued" is exactly the sentence a report of what
	is still to be catalogued has to be able to say.

	@par The same label twice is one way used, and it is still reported

	A way is used or it is not; being claimed twice does not make two ways,
	so the second claim changes no count. It is a fact of its own all the
	same - two components reaching for the same way, or one pin drawn twice
	- so it comes out through drawnTwice() rather than being swallowed.
	declaredTwice() is the same reading on the other side: a part that
	declares one label twice has sixteen pins and fifteen ways, and counting
	the repeat as a way would put a phantom in the reserve.

	@par Nothing normalises a pin label, and that is a decision

	Labels are compared exactly. "9" and "9 " are two labels; "a1" and "A1"
	are two labels. The label is what the maker prints on the product, the
	catalogue is what decides, and a label that does not match comes out by
	name in notOnPart(), where somebody reads it and puts it right. Folding
	the case or trimming the ends would make the mismatch vanish into a
	number that looks right, while the folio went on printing a spelling
	the product does not have.

	It is the opposite of the choice made for the connector name, which is
	folded, and the asymmetry is the point: two spellings of a connector
	name have nowhere to come out - counting them apart hands two ways the
	number 1, in silence - while two spellings of a pin label have this
	list. Where a difference can be shown, it is shown; where it cannot, it
	is merged.

	A label with nothing but blanks on it is not a label. It is counted -
	blankWayCount(), blankDrawnCount() - and left out of every list,
	because a way nobody numbered is neither used, nor reserve, nor missing
	from the part.
*/
class ConnectorWays
{
	public:
		static ConnectorWays fromPinout(const QStringList &part_ways,
						const QStringList &drawn_labels);

		/**
			@return what a count reads when there is no pinout to
			count against.

			A value of its own and not a zero, so that a caller
			printing it without asking isKnown() prints something
			visibly wrong instead of a plausible "0 reserve".
		*/
		static int unknownCount();

			/// @return true when the part declares a pinout at all
		bool isKnown() const;

			/// @return every distinct way of the part, in part order
		QStringList ways() const;
			/// @return the ways the folios draw, in part order
		QStringList used() const;
			/// @return the ways nothing draws, in part order
		QStringList reserve() const;
		/**
			@return the labels the folios draw that the part has no
			way for, in the order they were first met.

			Empty when the pinout is not known - see the table above.
		*/
		QStringList notOnPart() const;
		/**
			@return every distinct label the folios draw, in the
			order it was first met.

			The reading about the drawing rather than about the part,
			and the only list that survives a part with no pinout.
		*/
		QStringList drawn() const;
			/// @return the labels drawn more than once
		QStringList drawnTwice() const;
			/// @return the labels the part declares more than once
		QStringList declaredTwice() const;

		/// @return how many ways the part has, unknownCount() when it declares none
		int wayCount() const;
		/// @return how many of them are drawn, unknownCount() when there are none to match
		int usedCount() const;
		/// @return how many are spare, unknownCount() when nobody wrote how many there are
		int reserveCount() const;
			/// @return how many distinct labels the folios draw
		int drawnCount() const;
			/// @return how many pins of the part carry no label at all
		int blankWayCount() const;
			/// @return how many drawn pins carry no label at all
		int blankDrawnCount() const;

	private:
		bool m_known = false;
		QStringList m_ways;
		QStringList m_used;
		QStringList m_reserve;
		QStringList m_not_on_part;
		QStringList m_drawn;
		QStringList m_drawn_twice;
		QStringList m_declared_twice;
		int m_blank_ways = 0;
		int m_blank_drawn = 0;
};

#endif // CONNECTORWAYS_H
