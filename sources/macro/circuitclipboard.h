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
#ifndef CIRCUITCLIPBOARD_H
#define CIRCUITCLIPBOARD_H

#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief What a spreadsheet puts on the clipboard, and how to read it back.

	A range copied from a spreadsheet arrives as text/plain: cells separated by
	tabs, rows by newlines. Splitting on those two characters is right until a
	cell contains one of them, and then it is wrong in a way that is invisible
	until a project is generated from the wrong columns. A cell holding a tab, a
	newline or a quotation mark is written between quotation marks, and a
	quotation mark inside is doubled - the same convention as CSV, on tabs. So
	"Cabo 3x2,5\nazul" is one cell of two lines, and a naive split turns it into
	two rows, shifting every column after it by one.

	The ACME component list has exactly that: descriptions with a line break
	in them. This is why the paste of CU-08.2 does not split on the two
	characters.

	Nothing here knows about macros, parameters or the table. It turns text into
	a grid of strings and back, which is all it should ever do, and is what lets
	the test binary link it on its own.
*/
namespace CircuitClipboard
{
		/**
			@brief Read the clipboard text as a grid.
			@param text : what the spreadsheet wrote
			@return one QStringList per row, one string per cell

			The end of line is taken as written: a file from Windows, one from
			a Mac and one from Linux all give the same grid. A last newline
			ends the last row instead of opening an empty one, so a range of
			three rows gives three rows, not four.
		*/
	QList<QStringList> parse(const QString &text);

		/**
			@brief Write a grid the way a spreadsheet expects to read it.
			@param rows
			@return the text to put on the clipboard

			The reverse of parse(): compose(parse(text)) gives back what was
			read, which is what makes copying out of the table and pasting it
			back a round trip and not a slow way of losing line breaks.
		*/
	QString compose(const QList<QStringList> &rows);

		/**
			@brief Whether @a cell has to be written between quotation marks.
			@param cell
			@return true when it holds a tab, an end of line or a quotation mark
		*/
	bool needsQuotes(const QString &cell);

		/**
			@brief @a cell as it goes on the clipboard.
			@param cell
			@return quoted, with inner quotation marks doubled, when it needs it
		*/
	QString quoted(const QString &cell);
}

#endif // CIRCUITCLIPBOARD_H
