/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/
#include "csvwriter.h"

namespace QETCsv
{

/**
	@brief QETCsv::field
	@param value : the raw text of one cell
	@param separator : the delimiter the row will be joined with
	@return @p value, quoted only when it has to be

	RFC-4180 : a cell is wrapped in double quotes as soon as it holds the
	separator, a double quote or an end of line, and the double quotes it
	holds are doubled. A cell holding none of those comes back untouched,
	which is what keeps a file whose cells are all plain identical to the
	one the exporters wrote before this function existed - the only way
	the change can be proved to have added quoting and nothing else.

	The separator is a parameter and not the semicolon it used to be : the
	same list is copied to the clipboard with a tabulation between the
	cells, and a function that escapes the wrong character is a function
	that leaves the defect in place while looking fixed.

	An empty separator quotes nothing by itself. QString::contains()
	answers true for the empty string, so without the guard a caller that
	forgot its separator would quote every cell of the file - a new way of
	being wrong on top of the old one.
*/
QString field(const QString &value, const QString &separator)
{
	const bool needs_quotes = (!separator.isEmpty()
				   && value.contains(separator))
				  || value.contains(QLatin1Char('"'))
				  || value.contains(QLatin1Char('\n'))
				  || value.contains(QLatin1Char('\r'));
	if (!needs_quotes) {
		return value;
	}

	QString quoted = value;
	quoted.replace(QLatin1Char('"'), QStringLiteral("\"\""));
	return QLatin1Char('"') + quoted + QLatin1Char('"');
}

/**
	@brief QETCsv::row
	@param values : the cells, in column order
	@param separator : the delimiter between them
	@return the cells quoted where needed and joined, without end of line

	The end of line is left to the caller on purpose : the exporters of
	this program disagree about it - one writes "\n", the catalogue table
	writes "\r\n" because a spreadsheet on Windows reads it - and a helper
	that picked one would silently change the other's output.
*/
QString row(const QStringList &values, const QString &separator)
{
	QStringList quoted;
	quoted.reserve(values.size());
	for (const QString &value : values) {
		quoted.append(field(value, separator));
	}
	return quoted.join(separator);
}

}
