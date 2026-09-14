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
#ifndef TERMINALMOVE_H
#define TERMINALMOVE_H

#include <QCoreApplication>
#include <QString>

/**
	@brief Whether a request to move terminals can go ahead, and the
	sentence said when it cannot.

	A rule over three readings of the dialogue - how many rows of the table
	are selected, how many terminals those rows name, and what the list of
	destinations has to say - so that no click on the move button ends in
	silence.

	@par Why this is not a method of the editor

	Both pages of the terminal strip window move terminals: one moves an
	independent terminal into a strip, the other moves a terminal from one
	strip to another or back out of every strip. They refused the same way
	and would have been repaired twice, with two wordings for the same
	refusal. The rule is here so that there is one answer, and so that it
	can be checked without a window being opened.

	@par A refusal always carries a sentence

	describe() returns an empty string for Refusal::None and for nothing
	else. That is the whole point of the module: what it repairs is a
	button that returned without a word, and a refusal added later with no
	sentence would be that same defect coming back through the door.

	@par The sentence about Apply lives here too

	describeApply() is not about moving, and it is here on purpose. The
	person who reported the defect had used Apply to move a terminal - the
	reasonable thing to try, since Apply is the button that commits - and
	the two pages have to answer that in the same words. Splitting the
	wording between the two editors is how the two pages start disagreeing.
*/
namespace TerminalMove
{
		/// What the list of destinations has to say
	enum class Destination
	{
		Resolved, ///< a destination was chosen and found
		Empty,    ///< the list offers nothing to move into
		Vanished  ///< a destination was chosen and it was not found
	};

		/// Why a move writes nothing
	enum class Refusal
	{
		None,                 ///< it does not: the move goes ahead
		NothingSelected,      ///< no row of the table is selected
		SelectionNotTerminal, ///< the selected rows name no terminal to move
		NoDestination,        ///< there is nowhere to move a terminal into
		DestinationVanished   ///< the chosen destination was not found
	};

	/**
		@brief What one click on the move button does, or why it does
		nothing.

		One field, and it is deliberate: the caller already holds the
		terminals and the destination it read: what it lacks is the
		answer, and the sentence that goes with it.
	*/
	class Decision
	{
			//The namespace and not the class, as the context these
			//sentences are filed under: "Decision" on its own says
			//nothing to whoever meets it in a file of four thousand
			//messages.
		Q_DECLARE_TR_FUNCTIONS(TerminalMove)

		public:
				/// @return true when terminals are to be moved
			bool isValid() const;
				/// @return why nothing is to be moved, in one sentence
			QString describe() const;
			/**
				@return why @a refusal moves nothing, in one
				sentence; empty for Refusal::None.

				Static because a caller that refuses early holds
				a Refusal and not a Decision.
			*/
			static QString describe(Refusal refusal);

			/**
				@param terminal_count how many terminals were moved
				@param destination the destination as the person
				picked it in the list, so that the sentence names
				what they read and not an identifier
				@return what the move did, in one sentence

				A move that goes through says so. The table does
				change under it, but the person who has just been
				refused twice needs to be told the difference
				between "it worked" and "it did nothing again",
				and the table alone does not say which.
			*/
			static QString describeMove(int terminal_count,
						    const QString &destination);

			/**
				@param change_count how many changes were written
				@return what one click on Apply did, in one
				sentence

				When it wrote nothing, the sentence names the
				button that does move a terminal. That is the
				whole cure for the confusion: Apply is pressed at
				the exact moment the person expects a terminal to
				travel, so that is the moment to say which button
				makes it travel.
			*/
			static QString describeApply(int change_count);

				/// why nothing is to be moved
			Refusal refusal = Refusal::NothingSelected;
	};

	/**
		@brief Decide what one click on the move button does.
		@param selected_index_count how many cells the table reports as
		selected; zero means nothing is selected
		@param terminal_count how many terminals those cells name, once
		the model has resolved them
		@param destination what the destination list has to say
		@return the decision, and the sentence that goes with it

		@par The order the three readings are judged in

		An empty destination list is judged first, because it is the one
		refusal that no amount of selecting will cure: with no strip in
		the project there is nowhere to move a terminal to, and telling
		somebody to select a row first would send them to a second dead
		end.

		The selection comes next, since it is what the person does
		first, and a destination that vanished is judged last - it is
		the rarest of the four and the only one that is not the person's
		doing.
	*/
	Decision decide(int selected_index_count,
			int terminal_count,
			Destination destination);
}

#endif // TERMINALMOVE_H
