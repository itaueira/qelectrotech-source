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
#ifndef BOMMEASURE_H
#define BOMMEASURE_H

#include <QLocale>
#include <QString>

/**
	@brief How a line of the bill of material arrives at its number, and
	how that number has to be written down.

	Two kinds of thing are bought, and they are not counted the same way. A
	breaker is handed over as a piece: fourteen of them are fourteen. A
	length of rail or of cable duct is cut to size, and a two metre bar cut
	into three pieces is neither one bar nor three - it is the metres that
	were used. The list has to be able to say both, which is why the
	quantity of a line is a real number with a unit beside it rather than a
	count.

	@par Why the unit travels with the number

	The number alone does not say which of the two it is. A line reading
	"3.2" is a length; a line reading "3.2" pieces is a mistake nobody can
	see. The unit is therefore part of the line and not part of the column
	heading, and it is what every reader of the quantity asks before
	printing it.

	The catalogue already models exactly this pair on a part - a decimal
	property "quantity" with a free text property "unity" beside it, both
	seeded by the catalogue schema - and this is the same pair, on the line
	rather than on the part. Inventing a second vocabulary of units here
	would give the same rail two answers.

	@par The unit is data, not a word on the screen

	countUnit and defaultLengthUnit return keys, and are deliberately not
	translated: they are written into lists and read back, and a key whose
	spelling follows the interface language cannot be read back. What a
	person sees is produced by the format functions, which put the locale
	only where a locale belongs - in the decimal separator.
*/
namespace BomMeasure
{
		/// How the quantity of a line of material is arrived at.
	enum class Kind
	{
		Count,  ///< one per place the part is used; the storeroom hands over pieces
		Length  ///< the length actually used, summed; the storeroom cuts to size
	};

	/**
		@return the unit a counted line carries

		The default for everything, and it stays the default: only the
		classes named by lengthClassKey leave it. Making the fractional
		quantity change how an ordinary component is counted would be the
		one way this file could do harm.
	*/
	QString countUnit();

		/// @return the unit a measured line carries when nobody said otherwise
	QString defaultLengthUnit();

	/**
		@return the catalogue class whose parts are measured and not counted

		One spelling of the key, in one place. A caller holding a Catalog
		does not compare keys itself: it asks
		Catalog::isDescendantOf(class_id, BomMeasure::lengthClassKey()),
		so that a class somebody adds under Rail / Duct is measured too,
		which comparing the key alone would miss.
	*/
	QString lengthClassKey();

	/**
		@brief How a part of this catalogue class is arrived at.
		@param class_key the key of the class, as the catalogue spells it
		@return Length for the rail and duct class, Count for everything else

		Compares the key exactly, because that is all a rule with no
		catalogue can do - see lengthClassKey for the question a caller
		with a catalogue asks instead.

		Wire and cable are counted here as everything else is, and that is
		on purpose rather than an omission: they are already listed by
		length by the cable list, which has its own length column and its
		own source for it. Answering "measured" here as well would give one
		cable two lines with two different numbers behind them.
	*/
	Kind kindForClass(const QString &class_key);

		/// @return the unit a line of that kind starts with
	QString defaultUnit(Kind kind);

	/**
		@brief How a line carrying this unit has to be read.
		@param unit the unit written on the line
		@return Length when the unit is a length, Count otherwise

		An empty unit reads as Count, which is what an old line - written
		before a line had a unit at all - has to mean. Nothing stored needs
		rewriting for that to hold: the empty string and "un" answer the
		same.
	*/
	Kind kindForUnit(const QString &unit);

		/// @return true when the unit is one of the lengths this program writes
	bool isLengthUnit(const QString &unit);

	/**
		@brief The quantity as a list has to print it, the unit not included.
		@param quantity the number on the line
		@param unit the unit on the line
		@param locale the locale whose decimal separator to use
		@return the text of the number

		The whole point of the function. A counted line prints a whole
		number and never a decimal: fourteen doors read "14", and a report
		of breakers that suddenly reads "14.0" everywhere is the accident
		this file exists to prevent. Group separators are left out of a
		count as well, because that is what the lists print today and a
		thousand terminals reading "1,000" would be read as one.

		A measured line prints its fraction, to at most three decimals -
		the layout works in millimetre, and a metre carries a millimetre in
		three - with trailing zeros dropped, so that a rail that happens to
		come out whole reads "2" and not "2.000".
	*/
	QString formatQuantity(double quantity,
			       const QString &unit,
			       const QLocale &locale);

		/// The same, in the locale of whoever is looking at the screen.
	QString formatQuantity(double quantity, const QString &unit);

	/**
		@brief The quantity with its unit after it.
		@param quantity the number on the line
		@param unit the unit on the line
		@param locale the locale whose decimal separator to use
		@return "3,2 m" for a length, "14" for a count

		A count prints no unit, because it never had one on the screen and
		putting "14 un" in the quantity column of every list would be a
		change nobody asked for. A length prints one, because the number
		alone does not say metres.
	*/
	QString formatQuantityWithUnit(double quantity,
				       const QString &unit,
				       const QLocale &locale);

		/// The same, in the locale of whoever is looking at the screen.
	QString formatQuantityWithUnit(double quantity, const QString &unit);
}

#endif // BOMMEASURE_H
