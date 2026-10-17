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
#ifndef DRCWIRENUMBERS_H
#define DRCWIRENUMBERS_H

#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief What counts as a wire number on the conductors of one potential,
	and the sentence a potential carrying more than one of them is reported
	with.

	The half of the potential rules that needs no project: it is handed the
	texts the conductors of one potential carry, and the folio each of them
	is drawn on, and it answers from that alone. Which conductors make up a
	potential is the other half, and it lives in DrcPotentialRules, because
	only the scene knows it - the project data base holds no potential.

	@par Absent is not a second number
	A conductor of a potential whose text is empty, or made only of spacing,
	carries no number: it is not a second number disagreeing with the
	first. Leaving a number off some lengths of a potential is a drawing
	habit, and counting it as a conflict would fill the panel on its first
	run with findings nobody would act on.

	The same holds for the text a conductor is given when nobody numbers
	it, placeholder(): it is the default of a new conductor
	(ConductorProperties), and the program already reads it as "no number"
	elsewhere - the trace of a wire that could not be reconnected and the
	DXF export both leave it out. Read as a number, it would turn every
	potential numbered on one conductor and left at the default on the
	next into a finding.

	@par A number is compared trimmed
	" 24" and "24" print the same on the folio, so they are one number
	here; a finding that said "24 and 24" would be a finding nobody could
	clear by looking at the drawing.
*/
namespace DrcWireNumbers
{
	/**
		@brief One conductor of a potential, as far as these rules see it.
	*/
	struct Mark
	{
		/// The text the conductor carries, as it carries it.
		QString text;
		/// The number printed on its sheet, 1-based; 0 when unknown.
		int folio = 0;
	};

	/// The text a conductor carries when nobody numbered it.
	QString placeholder();

	/**
		Whether @a text carries no wire number: empty, only spacing, or
		the placeholder a new conductor is given.
	*/
	bool isAbsent(const QString &text);

	/**
		The order the numbers of a finding are listed in.

		Numbers first, in numeric order, then the rest as text - the order
		the wiring list sorts wire numbers in, so that a finding names them
		the way the workshop reads them. 24 comes before 105, which plain
		text order would not give.
	*/
	bool isBefore(const QString &a, const QString &b);

	/**
		The distinct wire numbers among @a texts, trimmed, in the order of
		isBefore(). Absent texts are left out.
	*/
	QStringList numbersOf(const QStringList &texts);

	/**
		The sentence of a finding about the potential @a marks describes.

		@return an empty string when the potential carries fewer than two
		distinct numbers - a single number, or none at all, is not what
		this sentence reports. Otherwise the numbers, in the order of
		isBefore(); and, when the numbered conductors are spread over more
		than one folio, the folio each number is first found on, because
		the reader sent to one of them would otherwise have to hunt the
		other one down across the report arrows.
	*/
	QString severalNumbersText(const QList<Mark> &marks);
}

#endif // DRCWIRENUMBERS_H
