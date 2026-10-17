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
#ifndef LABELCSVWRITER_H
#define LABELCSVWRITER_H

#include "labelentry.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

/**
	@brief A list of labels, written as a delimited text file.

	The first writer of the list the collectors make: one row per label,
	whatever collector filled it, and nothing read from a project. It takes
	a LabelEntryList and gives text back, so the same list can be written
	twice - here and to a sheet of adhesive labels - without being collected
	twice, and a second format is a second writer beside this one rather
	than a change to a collector.

	@par Every cell goes through QETCsv
	The rule of the program for a delimited file: a cell holding the
	separator, a double quote or an end of line is quoted, and only then.
	Never a join of raw text: two of the lists of this program did that, and
	a ';' typed in a tag shifted every column of its row, which is a file
	that still opens and is wrong.

	@par The layout of the file
	A first row of keys - header() -, then one row per label, in the order
	of the list. The writer never reorders: the order of a run of terminals
	is the order of the rail, and a writer that sorted would print a tape
	that matches neither the rail nor the strip editor.

	- ';' between the cells, CRLF between the rows;
	- the lines printed under the tag are one cell, one line of it each,
	  joined by a line feed: the number of lines changes from one label to
	  the next, and the number of columns must not. A spreadsheet shows such
	  a cell on several lines, which is what the label will show;
	- a number that does not apply - the position on the rail of a
	  component, say - is an empty cell, never -1;
	- UTF-8 with a byte order mark. Without the mark a spreadsheet on
	  Windows reads the file in the local 8-bit code page, and an accented
	  tag comes out mangled - on the tape, after it is spent. The command
	  line wiring list writes the mark for the same reason.

	The keys of the first row and the words of the kind column are keys,
	not sentences, and are not translated: the file is read back - by a
	label program, by a comparison of two exports - and a header in the
	language of the machine that wrote it cannot be.

	@par What the cells hold is what the entry holds
	The writer adds nothing and turns nothing into another form. In
	particular the location is written as the entry carries it: writing a
	stored location path the way the standard writes it is the job of the
	step that passes the collected values through
	QETInformation::displayedInfoValue(), and the writer cannot do it on
	its own - an entry's location is either a path or the plain field, and
	the entry does not say which.
*/
namespace LabelCsvWriter
{
	/// The delimiter between the cells of a row: ';'.
	QString separator();

	/**
		The first row of the file, one key per column.

		kind, text, lines, strip, position, level, location, folio,
		revision, excluded_from_bom - the label first, then what it is
		checked against on the bench.
	*/
	QStringList header();

	/**
		The word the kind column writes for @a source: component,
		terminal, wire, cable or door. Also the word the command line
		reads to choose which labels to export.
	*/
	QString kindKey(LabelSource source);

	/**
		The cells of one row, unquoted, one per column of header() and in
		that order.

		The quoting is not done here so that a test can compare a cell
		with the value of the entry, and so that the rule of quoting stays
		in one place - QETCsv - and is applied once, when the row is
		joined.
	*/
	QStringList cells(const LabelEntry &entry);

	/**
		The whole file as text: header() and one row per entry, each
		quoted by QETCsv and ended by CRLF. Without the byte order mark,
		which belongs to the bytes and not to the text.

		An empty list gives the first row alone: an export of a project
		with nothing to label is a file that says so, not a missing file.
	*/
	QString text(const LabelEntryList &entries);

	/// text() as the file holds it: UTF-8, behind a byte order mark.
	QByteArray bytes(const LabelEntryList &entries);

	/**
		Write @a entries to the file at @a path.

		@return an empty string when the file was written, else the
		reason, ready to be shown.

		Through QSaveFile: a write that fails part-way leaves the previous
		file where it was rather than half of a new one, and a folder that
		cannot be written to leaves no file at all.
	*/
	QString write(const LabelEntryList &entries, const QString &path);
}

#endif // LABELCSVWRITER_H
