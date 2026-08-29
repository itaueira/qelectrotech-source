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
#ifndef MACROSEQUENCE_H
#define MACROSEQUENCE_H

#include <QHash>
#include <QSet>
#include <QString>

class MacroParameterSet;

/**
	@brief The MacroSequence namespace
	Answers one question, and only that one: the macro proposes this value,
	these values are already taken - what should the dialogue show?

	Inserting the same macro twice is the case this exists for. The second
	insertion arrives with the same defaults as the first, and the first
	has already spent them. Renumbering afterwards would work too, and it
	is what the multiple paste does, but it means the user confirms a tag
	and then watches the program change it. Proposing the free value up
	front means what the dialogue shows is what the sheet gets, and the
	collision is never offered in the first place.

	Everything here is QString arithmetic: no diagram, no project, no
	catalog. That is what lets the test binary link it.
*/
namespace MacroSequence
{
		/**
			Split @a value into the part before its trailing digits and the
			number those digits spell.
			@param value
			@param number : receives the number, 1 when there are no digits
			@param width : receives how many digits were written, 0 when
			there are none, so that -Q03 answers 2 and keeps its padding
			@return the part before the digits
		*/
	QString stemOf(const QString &value, int *number = nullptr, int *width = nullptr);

		/**
			@param proposal
			@param taken
			@return @a proposal when nothing has taken it, the next value of
			the same shape that nobody has otherwise
		*/
	QString nextFree(const QString &proposal, const QSet<QString> &taken);

		/**
			Walk @a parameters in declaration order and move every text value
			of @a values that @a taken already spends onto the next free one.
			@param parameters
			@param values
			@param taken
			@return the values as the dialogue should first show them
		*/
	QHash<QString, QString> proposeFree(const MacroParameterSet &parameters,
					    const QHash<QString, QString> &values,
					    const QSet<QString> &taken);
}

#endif // MACROSEQUENCE_H
