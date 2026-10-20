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
#include "wiregauge.h"

#include <QChar>
#include <QLatin1Char>
#include <QLatin1String>
#include <QRegularExpression>

#include <cmath>

namespace
{
		/// The superscript two of "mm²", written by its code so that the
		/// source file says the same thing whatever it is opened as
	const QChar superscript_two(0x00B2);
}

/**
	@brief WireGauge::parse
	See the header.
*/
double WireGauge::parse(const QString &text)
{
		//Every run of blanks becomes one space, the non-breaking space a
		//word processor puts between a number and its unit included; and
		//the superscript two becomes a plain 2, so that "mm²" and "mm2" are
		//one unit for the expression below
	QString simple = text.simplified();
	simple.replace(superscript_two, QLatin1Char('2'));

		//\d without the Unicode option is the ten ASCII digits only
	static const QRegularExpression pattern(
				QStringLiteral("^(\\d+(?:[.,]\\d+)?) ?(?:mm2?)?$"),
				QRegularExpression::CaseInsensitiveOption);
	const QRegularExpressionMatch match = pattern.match(simple);
	if (!match.hasMatch()) {
		return 0.0;
	}

	QString number = match.captured(1);
	number.replace(QLatin1Char(','), QLatin1Char('.'));
	bool ok = false;
	const double value = number.toDouble(&ok);
	if (!ok || !std::isfinite(value)) {
		return 0.0;
	}

		//Rounded to the thousandth, the finest section format() writes,
		//so that a section this returns is always one format() can write:
		//"0,0001" is no section, rather than one written "0mm²"
	const double rounded = std::round(value * 1000.0) / 1000.0;
	return rounded > 0.0 ? rounded : 0.0;
}

/**
	@brief WireGauge::format
	See the header.
*/
QString WireGauge::format(double section)
{
		//Written so that a NaN says no too
	if (!(section > 0.0)) {
		return QString();
	}

		//'f' writes a point whatever the locale, and three decimals always
		//leave one: the zeros taken off below are decimals only
	QString text = QString::number(section, 'f', 3);
	while (text.endsWith(QLatin1Char('0'))) {
		text.chop(1);
	}
	if (text.endsWith(QLatin1Char('.'))) {
		text.chop(1);
	}
	text.replace(QLatin1Char('.'), QLatin1Char(','));
	return text + QLatin1String("mm") + superscript_two;
}

/**
	@brief WireGauge::normalized
	See the header.
*/
QString WireGauge::normalized(const QString &text)
{
	return format(parse(text));
}

/**
	@brief WireGauge::largest
	See the header.
*/
QString WireGauge::largest(const QStringList &texts)
{
	double largest_section = 0.0;
	for (const QString &text : texts)
	{
		const double section = parse(text);
		if (section > largest_section) {
			largest_section = section;
		}
	}
	return format(largest_section);
}
