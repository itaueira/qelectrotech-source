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
#include "macrosequence.h"

#include "macroparameter.h"
#include "macroparameterset.h"

#include <QChar>
#include <QLatin1Char>
#include <QList>
#include <QStringList>

/**
	@brief MacroSequence::stemOf
	@param value
	@param number
	@param width
	@param tail
	@return the part of @a value before the digits that count it
*/
QString MacroSequence::stemOf(const QString &value, int *number, int *width, QString *tail)
{
	if (tail) { *tail = QString(); }

		//Where the counting digits end. Normally at the end of the value,
		//but a tag written as "PS1 - NO BREAK" carries its number inside
		//the first word and its meaning after it, and appending there
		//would answer PS1 - NO BREAK2, which is not a tag anybody writes.
		//The search stops at the first space on purpose: past it the
		//digits are quantities, and proposing the next one would invent
		//engineering.
	int last_digit = value.length();
	if (last_digit == 0 || !value.at(last_digit - 1).isDigit())
	{
		const int space = value.indexOf(QLatin1Char(' '));
		last_digit = (space < 0) ? 0 : space;
		if (last_digit > 0 && !value.at(last_digit - 1).isDigit()) {
			last_digit = 0;
		}
	}

	int first_digit = last_digit;
	while (first_digit > 0 && value.at(first_digit - 1).isDigit()) {
		--first_digit;
	}

	const QString digits = value.mid(first_digit, last_digit - first_digit);
	bool ok = false;
	const int spelled = digits.toInt(&ok);

		//A run of digits too long to be an int is not a counter, it is a
		//serial number someone pasted in. Treated as digits it would come
		//back as zero and the next insertion would propose a value smaller
		//than the one it came from, which reads as data loss. Treated as
		//text it is simply left alone, and the sequence starts beside it.
	if (digits.isEmpty() || !ok) {
		if (number) { *number = 1; }
		if (width) { *width = 0; }
		return value;
	}

	if (number) { *number = spelled; }
	if (width) { *width = digits.length(); }
	if (tail) { *tail = value.mid(last_digit); }
	return value.left(first_digit);
}

/**
	@brief MacroSequence::nextFree
	@param proposal
	@param taken
	@return the first value of the shape of @a proposal that @a taken does
	not already hold
*/
QString MacroSequence::nextFree(const QString &proposal, const QSet<QString> &taken)
{
	if (proposal.isEmpty() || !taken.contains(proposal)) {
		return proposal;
	}

	int number = 1;
	int width = 0;
	QString tail;
	const QString stem = MacroSequence::stemOf(proposal, &number, &width, &tail);

		//A value carrying no number of its own starts at two, not at one:
		//the value without a number is the first of its kind, and calling
		//the next one X1 would read as if it came before X.
		//The bound is what makes this terminate. Every step takes a value
		//out of the way, so after as many steps as there are taken values
		//there is nothing left to collide with; the two spare turns cover
		//the value we started from and an off by one nobody should have to
		//reason about.
	for (int i = 0; i < taken.size() + 2; ++i)
	{
		++number;
		const QString candidate = stem + QString::number(number)
					  .rightJustified(width, QLatin1Char('0')) + tail;
		if (!taken.contains(candidate)) {
			return candidate;
		}
	}

	return proposal;
}

/**
	@brief MacroSequence::proposeFree
	@param parameters
	@param values
	@param taken
	@return @a values with every colliding text value moved on
*/
QHash<QString, QString> MacroSequence::proposeFree(const MacroParameterSet &parameters,
						  const QHash<QString, QString> &values,
						  const QSet<QString> &taken)
{
	QHash<QString, QString> proposed = values;
	if (taken.isEmpty() || parameters.isEmpty()) {
		return proposed;
	}

		//Declaration order, and not the order the hash happens to store the
		//values in: two parameters may open on the same default, and which
		//of them keeps it has to be the same on every run or the dialogue
		//shows one thing today and another tomorrow for the same macro.
		//Only a value that actually moved is written down as spoken for. A
		//value nobody had stays exactly as the macro declared it, even when
		//a second parameter declares the same one - a macro naming the
		//manufacturer twice means the manufacturer twice, and moving the
		//second one on would draw a company that does not exist.
	QSet<QString> spoken = taken;
	const QList<MacroParameter> declared = parameters.parameters();
	for (const MacroParameter &parameter : declared)
	{
			//Only free text is moved on. A number is a value and not a
			//name - proposing 8 because someone else drew 7 would be
			//inventing engineering. A list may only hold what it declares,
			//and a part code names a part in the catalogue: neither has a
			//next one.
		if (parameter.type != MacroParameterType::Text) {
			continue;
		}

		const QString value = proposed.value(parameter.name);
		if (value.isEmpty()) {
			continue;
		}

		const QString free = MacroSequence::nextFree(value, spoken);
		if (free == value) {
			continue;
		}

		proposed.insert(parameter.name, free);
		spoken.insert(free);
	}

	return proposed;
}
