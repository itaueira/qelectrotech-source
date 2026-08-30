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
#include "circuitclipboard.h"

#include <QChar>
#include <QLatin1Char>

/**
	@brief CircuitClipboard::parse
	@param text
	@return the grid the spreadsheet meant
*/
QList<QStringList> CircuitClipboard::parse(const QString &text)
{
	QList<QStringList> rows;
	if (text.isEmpty()) {
		return rows;
	}

	QStringList row;
	QString cell;
		//Whether the current row has been started. A text ending in a newline
		//has closed its last row already, and flushing again would hand back
		//one empty row nobody copied - which, pasted, would add one circuit
		//nobody asked for.
	bool pending = false;
		//Inside a quoted cell every character is content, tabs and newlines
		//included. That is the whole point of the quotes.
	bool in_quotes = false;
		//Whether this cell opened with a quote. A quote met later in the cell
		//is a quote, not an opening: 3" is three inches.
	bool quoted_cell = false;

	const int length = text.length();
	int i = 0;
	while (i < length)
	{
		const QChar character = text.at(i);

		if (in_quotes)
		{
			if (character == QLatin1Char('"'))
			{
					//Two in a row are one, written down.
				if (i + 1 < length && text.at(i + 1) == QLatin1Char('"'))
				{
					cell += QLatin1Char('"');
					i += 2;
					continue;
				}
				in_quotes = false;
				++ i;
				continue;
			}
			if (character == QLatin1Char('\r'))
			{
					//An end of line inside a cell is kept, in one spelling.
				cell += QLatin1Char('\n');
				if (i + 1 < length && text.at(i + 1) == QLatin1Char('\n')) {
					++ i;
				}
				++ i;
				continue;
			}
			cell += character;
			++ i;
			continue;
		}

		if (character == QLatin1Char('"') && cell.isEmpty() && !quoted_cell)
		{
			in_quotes = true;
			quoted_cell = true;
			pending = true;
			++ i;
			continue;
		}

		if (character == QLatin1Char('\t'))
		{
			row << cell;
			cell.clear();
			quoted_cell = false;
			pending = true;
			++ i;
			continue;
		}

		if (character == QLatin1Char('\n') || character == QLatin1Char('\r'))
		{
			row << cell;
			cell.clear();
			quoted_cell = false;
			rows << row;
			row.clear();
			pending = false;
			if (character == QLatin1Char('\r')
			    && i + 1 < length
			    && text.at(i + 1) == QLatin1Char('\n'))
			{
				++ i;
			}
			++ i;
			continue;
		}

		cell += character;
		pending = true;
		++ i;
	}

	if (pending)
	{
		row << cell;
		rows << row;
	}

	return rows;
}

/**
	@brief CircuitClipboard::compose
	@param rows
	@return the text a spreadsheet reads back as @a rows
*/
QString CircuitClipboard::compose(const QList<QStringList> &rows)
{
	QStringList lines;
	lines.reserve(rows.count());
	for (const QStringList &row : rows)
	{
		QStringList cells;
		cells.reserve(row.count());
		for (const QString &cell : row) {
			cells << CircuitClipboard::quoted(cell);
		}
		lines << cells.join(QStringLiteral("\t"));
	}
	return lines.join(QStringLiteral("\n"));
}

/**
	@brief CircuitClipboard::needsQuotes
	@param cell
	@return whether writing it plainly would break the grid
*/
bool CircuitClipboard::needsQuotes(const QString &cell)
{
	return cell.contains(QLatin1Char('\t'))
	       || cell.contains(QLatin1Char('\n'))
	       || cell.contains(QLatin1Char('\r'))
	       || cell.contains(QLatin1Char('"'));
}

/**
	@brief CircuitClipboard::quoted
	@param cell
	@return the cell as it goes on the clipboard
*/
QString CircuitClipboard::quoted(const QString &cell)
{
	if (!CircuitClipboard::needsQuotes(cell)) {
		return cell;
	}

	QString escaped = cell;
	escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
	return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}
