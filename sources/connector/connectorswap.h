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
#ifndef CONNECTORSWAP_H
#define CONNECTORSWAP_H

#include <QCoreApplication>
#include <QList>
#include <QString>

/**
	@brief Exchanging the way labels of two pins of one connector, as a
	rule over a list: no Element, no Diagram, no project.

	Two pins in, two labels out, crossed over. Handed the pins of one
	connector in the order a list shows them, and the two places in that
	list, it says which label each of the two is to carry afterwards, or
	why nothing is to be written at all.

	@par This is not the reordering next door, and the difference is the
	whole operation

	TerminalStrip::setOrderTo moves an item to another place: the items
	travel and the list is read again in a new order. This does the
	opposite, and the use case says so - "the place in the list does not
	change, the content of the rows swaps". Nothing here moves, and
	applied() is that sentence written as code: it gives back a list of
	the same length, holding the same pins under the same identifiers in
	the same order, with two labels exchanged and nothing else touched.

	Reordering and exchanging are easy to confuse, because one particular
	reading of the outcome looks alike. A table sorted by way number reads
	the same either way, so an implementation that moved the two pins over
	and renamed them back would print an identical table while the folio
	had the two symbols in each other's place. That is why the identifier
	is carried here at all: it is what tells the two apart, and what a
	test can hold on to. Places are not something this rule computes - it
	is handed an order and it hands the same order back.

	@par What is refused, and why each one

	Nothing here writes a warning and carries on. Every reason it will not
	swap comes back named, so that the caller can say it:

	- @b SamePin - one pin with itself. It would write the label the pin
	  already carries, and a step on the undo stack that undoes nothing is
	  a step the user has to press Ctrl+Z twice to get past. It is
	  answered by identifier and not only by place, so that a caller
	  holding one pin twice gets the reason it asked about rather than the
	  one below.
	- @b SameLabel - two pins already carrying the same text. Nothing
	  would change, for the same reason. Two ways of one connector
	  labelled alike is a fault of its own, and it is the report's to
	  raise, not this one's to hide by moving them about.
	- @b OtherConnector - two pins of different connectors. Not a
	  scruple: swapping across connectors takes a way out of one and puts
	  it into the other, and two connectors each numbered 1..n come out of
	  it with the number 1 twice on one of them and gone from the other.
	  Moving a pin to another connector is a different move, and it has
	  its own door - ConnectorCheck::assignConnector, which writes the
	  connector field rather than the way label.
	- @b NoConnector - a pin belonging to no connector. There is no list
	  for it to keep its place in.
	- @b NoSuchPin - a place naming no pin of the list. The caller on the
	  project side answers it too, for a pin that is null or drawn on no
	  folio.

	@par The connector name folds and the pin label does not

	Which two pins are of one connector is asked of
	Renumberer::connectorKey, the one already deciding that "CN1", "cn1"
	and "CN1 " are one connector, so that a swap is not refused over how
	the name happens to be typed on each of the two. A second normalising
	of that field, written here, would be two parts of one program
	disagreeing about how many connectors there are.

	The labels themselves travel exactly as they are - untrimmed, case
	kept - because the label is what the maker prints on the product and
	the catalogue is what decides. ConnectorWays states the same rule from
	the other side, and a case reproves whoever trims it: a "9 " swapped
	into a way the part calls "9" has to go on coming out by name in
	ConnectorWays::notOnPart(), instead of being quietly folded into a way
	that reads used while the folio prints a spelling the product does not
	have.
*/
namespace ConnectorSwap
{
	/**
		@brief One pin as a swap sees it: what tells it from the others,
		what connector it says it belongs to, and what it is called.

		A reading and not a record. On the project side the identifier
		is the uuid of the drawn component; in a test it is any string
		unique within the list. It is what makes "the place in the list
		did not change" something that can be checked rather than hoped
		for.
	*/
	class Pin
	{
		public:
			Pin() = default;
			Pin(const QString &pin_id, const QString &connector_name,
			    const QString &way_label);

			/**
				@return true when the two are the same reading

				All three fields, because the reading is the
				whole of it: a caller comparing a list before
				and after has to be told about a label that
				moved and about a pin that did not.
			*/
			bool operator==(const Pin &other) const;
			/// @return the opposite of operator==
			bool operator!=(const Pin &other) const;

				/// what tells this pin from the others of the list
			QString id;
				/// the connector name, as written on the pin
			QString connector;
				/// the way label, exactly as drawn
			QString label;
	};

	/// Why two pins will not be swapped
	enum class Refusal
	{
		None,           ///< they will be
		NoSuchPin,      ///< a place naming no pin of the list
		SamePin,        ///< one pin with itself
		NoConnector,    ///< one of them belongs to no connector
		OtherConnector, ///< they belong to two different connectors
		SameLabel       ///< they already carry the same label
	};

	/**
		@brief What one swap would write, or why it would write nothing.

		Two places and two labels, and never a new order: the places are
		the ones handed in, and they come back untouched, so that the
		caller writing the result knows it is writing to the same two
		rows it read.
	*/
	class Plan
	{
			//The namespace and not the class, as the context these
			//sentences are filed under: "Plan" on its own says nothing
			//to whoever meets it in a file of four thousand messages.
		Q_DECLARE_TR_FUNCTIONS(ConnectorSwap)

		public:
			/// @return true when two labels are to be written
			bool isValid() const;
			/// @return why nothing is to be written, in one sentence
			QString describe() const;
			/**
				@return why @a refusal writes nothing, in one
				sentence; empty for Refusal::None.

				Static because the project side answers with a
				Refusal and not with a Plan - the plan it built
				held two pins at the places 0 and 1, which would
				tell the reader nothing.
			*/
			static QString describe(Refusal refusal);

				/// where the first pin sits, as it was handed in
			int first_index = -1;
				/// where the second sits
			int second_index = -1;
				/// the label the pin at first_index takes
			QString first_label;
				/// the label the pin at second_index takes
			QString second_label;
				/// why nothing is to be written
			Refusal refusal = Refusal::NoSuchPin;
	};

	/**
		@brief Plan the swap of the pins at @a first_index and
		@a second_index of @a pins.
		@param pins the pins of one connector, in the order a list shows them
		@param first_index
		@param second_index
		@return what would be written, or why nothing would be
	*/
	Plan plan(const QList<Pin> &pins, int first_index, int second_index);

	/**
		@brief Apply @a swap to @a pins.
		@param pins
		@param swap
		@return @a pins with the two labels exchanged; @a pins unchanged
		when @a swap writes nothing.

		The same length, the same identifiers in the same order, the
		same connector on every pin: only two labels differ. It is the
		use case as an expression, and it is the shape of the answer a
		table shows before anything is written to the project.
	*/
	QList<Pin> applied(const QList<Pin> &pins, const Plan &swap);
}

#endif // CONNECTORSWAP_H
