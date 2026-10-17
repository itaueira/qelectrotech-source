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
#include "drcwirenumbers.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>

/*
	No conductor.h and no diagram.h here: this file is handed texts and
	folio numbers, never the objects they came from. That is what lets it
	be proved in the pure suite, which does not link the program.
*/

/**
	@brief DrcWireNumbers::placeholder
	@return the text ConductorProperties gives a conductor nobody numbered
*/
QString DrcWireNumbers::placeholder()
{
	return QStringLiteral("_");
}

/**
	@brief DrcWireNumbers::isAbsent
	@param text : the text a conductor carries
	@return whether it carries no wire number
*/
bool DrcWireNumbers::isAbsent(const QString &text)
{
	const QString trimmed = text.trimmed();
	return trimmed.isEmpty() || trimmed == placeholder();
}

/**
	@brief DrcWireNumbers::isBefore
	@param a : a wire number, trimmed
	@param b : another one
	@return whether @a a is listed before @a b

	Two texts that read as the same integer - "024" and "24" - are still two
	numbers, since the label printed for each differs; they are then ordered
	as text, so that the order stays total and the same on every run.
*/
bool DrcWireNumbers::isBefore(const QString &a, const QString &b)
{
	bool a_is_number = false;
	bool b_is_number = false;
	const qlonglong a_value = a.toLongLong(&a_is_number);
	const qlonglong b_value = b.toLongLong(&b_is_number);

	if (a_is_number != b_is_number) {
		return a_is_number;
	}
	if (a_is_number && a_value != b_value) {
		return a_value < b_value;
	}
	return a < b;
}

/**
	@brief DrcWireNumbers::numbersOf
	@param texts : the texts the conductors of one potential carry
	@return the distinct wire numbers among them, trimmed and ordered
*/
QStringList DrcWireNumbers::numbersOf(const QStringList &texts)
{
	QStringList numbers;
	QSet<QString> seen;
	for (const QString &text : texts)
	{
		if (isAbsent(text)) {
			continue;
		}

		const QString number = text.trimmed();
		if (seen.contains(number)) {
			continue;
		}
		seen.insert(number);
		numbers << number;
	}

	std::sort(numbers.begin(), numbers.end(), &DrcWireNumbers::isBefore);
	return numbers;
}

/**
	@brief DrcWireNumbers::severalNumbersText
	@param marks : every conductor of one potential
	@return the sentence of the finding, or an empty string when there is
	nothing to report
*/
QString DrcWireNumbers::severalNumbersText(const QList<Mark> &marks)
{
	QStringList texts;
		//The first folio each number is found on - the lowest one, so that
		//the answer does not depend on the order the marks come in.
	QHash<QString, int> first_folio;
		//The folios the numbered conductors are on. The unnumbered ones do
		//not count: a potential whose two numbers sit on the same folio
		//needs no folio in its sentence, whatever else it crosses.
	QSet<int> folios;

	for (const Mark &mark : marks)
	{
		texts << mark.text;
		if (isAbsent(mark.text)) {
			continue;
		}

		const QString number = mark.text.trimmed();
		if (mark.folio > 0)
		{
			folios.insert(mark.folio);
			const int known = first_folio.value(number, 0);
			if (known == 0 || mark.folio < known) {
				first_folio.insert(number, mark.folio);
			}
		}
	}

	const QStringList numbers = numbersOf(texts);
	if (numbers.count() < 2) {
		return QString();
	}

	const bool spread = folios.count() > 1;
	QStringList items;
	for (const QString &number : numbers)
	{
		const int folio = first_folio.value(number, 0);
		if (spread && folio > 0)
		{
				//Both values in one call: a number is text somebody typed,
				//and one holding a "%2" would otherwise be rewritten by
				//the second of two chained arg().
			items << QCoreApplication::translate("DrcWireNumbers",
							     "%1 (folio %2)")
				 .arg(number, QString::number(folio));
		}
		else
		{
			items << number;
		}
	}

		//Not a translated joiner: ", " carries no word, and the words of
		//the sentence are around it.
	return QCoreApplication::translate("DrcWireNumbers",
					   "Potential with %n wire numbers: %1",
					   nullptr,
					   int(numbers.count()))
	       .arg(items.join(QStringLiteral(", ")));
}
