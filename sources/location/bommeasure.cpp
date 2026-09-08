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
#include "bommeasure.h"

#include <QStringList>
#include <QtGlobal>

namespace
{
		/// At most a millimetre inside a metre, and never more.
	const int LengthDecimals = 3;

	/**
		@brief The units this program understands as a length.

		Metric only, and short on purpose. The unit of a line is a free
		field - the catalogue seeds the list of units empty and lets the
		office fill it - so this list does not police what may be typed:
		it only says which spellings make the quantity print as a
		fraction. A unit nobody here recognises is counted, which is the
		conservative answer, because it leaves an unknown line reading
		exactly as it reads today.
	*/
	QStringList lengthUnits()
	{
		return QStringList{QStringLiteral("mm"),
				   QStringLiteral("cm"),
				   QStringLiteral("m"),
				   QStringLiteral("km")};
	}

	/**
		@brief How many decimals this length actually needs.
		@param quantity the length
		@return LengthDecimals at most, fewer when the tail is zeros

		Counted before the number is written and not trimmed off the text
		afterwards, and that is not a stylistic preference: asking the
		locale for its decimal separator in order to trim would have to be
		written twice, because QLocale::decimalPoint returns a QChar in Qt5
		and a QString in Qt6. Counting the zeros in the integer that the
		text will be made of avoids the question entirely, and is exact
		where trimming text is a guess about what the locale printed.
	*/
	int lengthDecimalsFor(double quantity)
	{
		int decimals = LengthDecimals;

			//Beyond this the scaling below would leave the range of
			//qint64. A length that large is not a rail, and printing it
			//with every decimal is the harmless answer.
		if (!qIsFinite(quantity) || qAbs(quantity) > 1.0e15) {
			return decimals;
		}

		double scale = 1.0;
		for (int i = 0 ; i < LengthDecimals ; ++ i) {
			scale *= 10.0;
		}

		qint64 scaled = qRound64(quantity * scale);
		while (decimals > 0 && scaled % 10 == 0)
		{
			scaled /= 10;
			-- decimals;
		}
		return decimals;
	}
}

/**
	@brief BomMeasure::countUnit
	See the header.
*/
QString BomMeasure::countUnit()
{
	return QStringLiteral("un");
}

/**
	@brief BomMeasure::defaultLengthUnit
	The metre, because that is what a rail and a duct are bought and cut
	in, and because the layout that will produce the number works in
	millimetre - a unit that divides into it without a remainder.
*/
QString BomMeasure::defaultLengthUnit()
{
	return QStringLiteral("m");
}

/**
	@brief BomMeasure::lengthClassKey
	See the header. The key is the one the catalogue schema seeds for the
	"Rail / Goulotte" class, and it is written here once so that nothing
	else in the program has to spell it.
*/
QString BomMeasure::lengthClassKey()
{
	return QStringLiteral("rail_duct");
}

/**
	@brief BomMeasure::kindForClass
	See the header for why wire and cable are not here.
*/
BomMeasure::Kind BomMeasure::kindForClass(const QString &class_key)
{
	return class_key.trimmed().compare(lengthClassKey(),
					   Qt::CaseInsensitive) == 0
	       ? Kind::Length
	       : Kind::Count;
}

/**
	@brief BomMeasure::defaultUnit
	@param kind
	@return the unit a line of that kind starts with
*/
QString BomMeasure::defaultUnit(Kind kind)
{
	return kind == Kind::Length ? defaultLengthUnit() : countUnit();
}

/**
	@brief BomMeasure::kindForUnit
	See the header: an empty unit is a counted line, so that nothing
	written before a line had a unit needs rewriting.
*/
BomMeasure::Kind BomMeasure::kindForUnit(const QString &unit)
{
	return isLengthUnit(unit) ? Kind::Length : Kind::Count;
}

/**
	@brief BomMeasure::isLengthUnit
	@param unit the unit as it was written down
	@return true when this program prints it as a fraction

	Tolerant on the way in - trimmed and case insensitive - because the
	unit is typed by a person into a free field, and "M" from a spreadsheet
	is the same metre as "m".
*/
bool BomMeasure::isLengthUnit(const QString &unit)
{
	const QString wanted = unit.trimmed();
	if (wanted.isEmpty()) {
		return false;
	}
	const QStringList units = lengthUnits();
	for (const QString &known : units)
	{
		if (wanted.compare(known, Qt::CaseInsensitive) == 0) {
			return true;
		}
	}
	return false;
}

/**
	@brief BomMeasure::formatQuantity
	See the header. The two branches are the whole of this file's purpose:
	a count must never grow a decimal place, and a length must never lose
	one.
*/
QString BomMeasure::formatQuantity(double quantity,
				   const QString &unit,
				   const QLocale &locale)
{
	if (kindForUnit(unit) == Kind::Count)
	{
			//No locale, on purpose: this is the plain integer the lists
			//print today, group separators included - which is to say,
			//not included.
		return QString::number(qRound64(quantity));
	}

	return locale.toString(quantity, 'f', lengthDecimalsFor(quantity));
}

/**
	@brief BomMeasure::formatQuantity
	The same, in the locale of whoever is looking at the screen.
*/
QString BomMeasure::formatQuantity(double quantity, const QString &unit)
{
	return formatQuantity(quantity, unit, QLocale());
}

/**
	@brief BomMeasure::formatQuantityWithUnit
	See the header: a count says nothing after the number, a length says
	what it is measured in.
*/
QString BomMeasure::formatQuantityWithUnit(double quantity,
					   const QString &unit,
					   const QLocale &locale)
{
	const QString number = formatQuantity(quantity, unit, locale);
	if (kindForUnit(unit) == Kind::Count) {
		return number;
	}
	return number + QLatin1Char(' ') + unit.trimmed();
}

/**
	@brief BomMeasure::formatQuantityWithUnit
	The same, in the locale of whoever is looking at the screen.
*/
QString BomMeasure::formatQuantityWithUnit(double quantity,
					   const QString &unit)
{
	return formatQuantityWithUnit(quantity, unit, QLocale());
}
