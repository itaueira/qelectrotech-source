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
#include "terminalmove.h"

/**
	@brief TerminalMove::Decision::isValid
	@return true when terminals are to be moved
*/
bool TerminalMove::Decision::isValid() const
{
	return refusal == Refusal::None;
}

/**
	@brief TerminalMove::Decision::describe
	@return why nothing is to be moved, in one sentence
*/
QString TerminalMove::Decision::describe() const
{
	return describe(refusal);
}

/**
	@brief TerminalMove::Decision::describe
	@param refusal
	@return why @a refusal moves nothing, in one sentence
*/
QString TerminalMove::Decision::describe(Refusal refusal)
{
	switch (refusal)
	{
		case Refusal::None:
			return QString();
		case Refusal::NothingSelected:
				//Names the button, because the click that
				//deserves this sentence is a click on it: being
				//told what is missing without being told where
				//to go next is half an answer.
			return tr("Aucune borne sélectionnée : sélectionnez la "
				  "ligne de la borne dans le tableau, puis "
				  "cliquez sur « Déplacer ».");
		case Refusal::SelectionNotTerminal:
			return tr("La sélection ne désigne aucune borne à "
				  "déplacer : sélectionnez la ligne d'une borne.");
		case Refusal::NoDestination:
			return tr("Aucun bornier dans ce projet : créez-en un "
				  "avant d'y déplacer une borne.");
		case Refusal::DestinationVanished:
				//The one refusal that is not the person's doing,
				//so it says what to do rather than what they did
				//wrong.
			return tr("Le bornier de destination n'existe plus : "
				  "rechargez la fenêtre et choisissez-en un autre.");
	}
	return QString();
}

/**
	@brief TerminalMove::Decision::describeMove
	@param terminal_count
	@param destination
	@return what the move did, in one sentence
*/
QString TerminalMove::Decision::describeMove(int terminal_count,
					     const QString &destination)
{
	return tr("%n borne(s) déplacée(s) vers %1.", "", terminal_count)
			.arg(destination);
}

/**
	@brief TerminalMove::Decision::describeApply
	@param change_count
	@return what one click on Apply did, in one sentence
*/
QString TerminalMove::Decision::describeApply(int change_count)
{
	if (change_count > 0) {
		return tr("%n modification(s) enregistrée(s).", "", change_count);
	}

		//Said in full, and only when nothing was written: a person who
		//has just saved something does not need to be lectured about a
		//button they did not press, and a hint repeated after every
		//successful click is a hint nobody reads.
	return tr("Aucune modification à enregistrer. Appliquer ne déplace pas "
		  "de borne : pour cela, utilisez le bouton « Déplacer ».");
}

/**
	@brief TerminalMove::decide
	@param selected_index_count
	@param terminal_count
	@param destination
	@return the decision, and the sentence that goes with it
	@sa the declaration, for the order the three readings are judged in
*/
TerminalMove::Decision TerminalMove::decide(int selected_index_count,
					    int terminal_count,
					    Destination destination)
{
	Decision decision;

	if (destination == Destination::Empty) {
		decision.refusal = Refusal::NoDestination;
	}
	else if (selected_index_count <= 0) {
		decision.refusal = Refusal::NothingSelected;
	}
	else if (terminal_count <= 0) {
			//Something is selected and it names no terminal: the
			//two are told apart because the cure is not the same.
			//"Select something" would be wrong advice to somebody
			//who has just selected a row that carries no terminal
			//of this strip.
		decision.refusal = Refusal::SelectionNotTerminal;
	}
	else if (destination == Destination::Vanished) {
		decision.refusal = Refusal::DestinationVanished;
	}
	else {
		decision.refusal = Refusal::None;
	}

	return decision;
}
