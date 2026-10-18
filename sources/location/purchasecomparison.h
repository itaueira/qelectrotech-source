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
#ifndef PURCHASECOMPARISON_H
#define PURCHASECOMPARISON_H

#include "locationtree.h"

#include <QList>
#include <QLocale>
#include <QString>
#include <QStringList>

/**
	@brief What changed between two purchase lists: the one exported
	before a revision and the one exported after it.

	A revision of a project is stamped on every folio alike, so it cannot
	say which folio changed, and still less which part. The one memory of
	what was ordered is the file that was exported then, and this compares
	two of those files, line by line, on the key a line is: the catalogue
	part and its revision - never the words describing it. Two different
	breakers described alike swap without a trace in a comparison by
	description; here they are a line that went and a line that came.

	@par Three answers, and only three
	A line is removed (its key is in the file before and not after),
	added (the other way round), or changed (in both, with a quantity
	that does not print the same). A line whose designation or tags
	changed and whose quantity did not is no difference: what is ordered
	is the same. A part that moves to a new revision is a line removed
	and a line added, because a new revision is a new part to buy.

	@par Rule only
	Text in, lines and differences out: no project, no data base, no
	file. Reading the files and writing the report is the command line's
	(--compare-purchase), which is why every case of this rule can be
	proved without a project open.

	@par The format lives here
	The first rows of the blocks, and the key of the count of lines, are
	spelled here once and written by --export-purchase from here: the
	export and its reader agreeing on a header is a promise only one
	spelling can keep.
*/
namespace PurchaseComparison
{
	/**
		@brief The first row of the lines block of --export-purchase.

		part_code and part_revision are the key of a line - the part and
		its revision, never the words describing it (BomCollector) - and
		they come first because they are what two exports of the same
		project are matched by. The names are keys and are not translated:
		the file is read back, and a header spelled in the interface
		language cannot be.
	*/
	const QStringList &lineHeader();

	/// The first row of the pending block of --export-purchase. A key,
	/// like lineHeader(), for the same reason.
	const QStringList &pendingHeader();

	/// @return the key of the row of the counts block that says how many
	/// lines the file holds
	QString linesCountKey();

	/**
		@brief The lines of one purchase list, read back from its text.
		@param text the whole file, as written by --export-purchase
		@param lines filled with one line per row of the lines block, in
		the order of the file
		@param error filled with the reason when the text is refused
		@return false when the text is not a purchase list this program
		wrote whole - and then @p lines is left empty

		The blocks are found by their first rows and never by counting
		rows: the lines are what lies between lineHeader() and
		pendingHeader(). Each row comes back as the LocationTree::BomLine
		the export wrote it from: the code trimmed, the revision a number
		(an empty cell is 0, "whatever is current will do"), the quantity
		read by quantityOf() and the unit as it was written. The tags are
		not read back: they are not part of what is compared.

		@par Refused, with a reason, rather than half read
		- no lines block, which is every file that is not a purchase list;
		- no pending block after it: the export always writes one, so a
		  file without it was cut short;
		- no count of lines, or a count that is not the number of rows
		  found: a row lost or added by hand, which a comparison would
		  otherwise report as a part removed or added;
		- a row wider than the header, which is a cell split at a
		  separator it should have been quoted around;
		- a row with no code, a revision that is not a number, or a
		  quantity quantityOf() cannot read;
		- two rows with one key, which the export never writes.

		A comparison that read such a file would answer, and the answer
		would look like one about the project. The refusal is the one
		answer that cannot be mistaken for it.
	*/
	bool readLines(const QString &text,
		       QList<LocationTree::BomLine> *lines,
		       QString *error = nullptr);

	/**
		@brief The quantity a cell of the lines block says.
		@param cell the quantity cell, as written
		@param ok filled with false when it says no usable quantity
		@return the number

		The export writes a count as a whole number and a length with the
		decimal separator of the machine that exported it - BomMeasure::
		formatQuantity(), the one every list of the program prints with -,
		group separators included. The two files being compared may come
		from two machines, so the cell is read without asking whose
		separator it is:

		- a space, a no-break space and an apostrophe are group
		  separators, and are dropped;
		- with both '.' and ',' in the cell, the last one is the decimal
		  separator and the other one groups ("1.234,5" and "1,234.5" are
		  both 1234.5);
		- with only one of them, written more than once, it groups
		  ("1.234.567");
		- with only one of them, written once, it is the decimal
		  separator ("2,5" and "2.5" are both 2.5).

		The last rule has one reading it gets wrong, and it is said here
		rather than left to be found: a whole length of a thousand units
		or more, printed with its group separator and nothing after the
		decimals ("1.234" for 1234 m on a machine in Portuguese), reads as
		1.234. Rail and duct are bought by the metre and a panel uses a
		few of them; a kilometre of one profile is not a panel.

		What is left is read by BomCollector::quantityOf(), so that zero,
		a negative number and text are refused here by the rule that
		refuses them in the list itself. An empty cell is refused too:
		the export never writes one, and quantityOf() would read it as 1.
	*/
	double quantityOf(const QString &cell, bool *ok = nullptr);

	/**
		@brief Do these two lines say the same quantity?
		@param before a line of the file before
		@param after the line of the same key in the file after
		@return true when the units read as one and the quantities print
		the same

		Two counts are one unit whatever word sits beside them ("un", "pc",
		an empty cell), and two lengths only when they are spelled the
		same - the rule BomCollector adds rows up by. The quantities are
		compared as the list prints them, in the C locale: a difference
		that would not print is no difference, and a report saying "2.5
		changed to 2.5" would be read as a fault in the report.
	*/
	bool sameQuantity(const LocationTree::BomLine &before,
			  const LocationTree::BomLine &after);

	/// What happened to one key between the two files.
	enum class Change
	{
		Removed, ///< in the file before, not in the file after
		Added,   ///< in the file after, not in the file before
		Changed  ///< in both, with a different quantity or unit
	};

	/// @return the word the report writes for @p change: removed, added
	/// or changed. A key, never translated, for a spreadsheet to filter on.
	QString changeKey(Change change);

	/**
		@brief One key that is not the same in the two files.

		The side a key is missing from is a default line: @ref before is
		empty for an added key, @ref after for a removed one.
	*/
	struct Difference
	{
		Change change = Change::Changed;
		LocationTree::BomLine before;
		LocationTree::BomLine after;

		/// @return the side that names the key: after, unless removed
		const LocationTree::BomLine &current() const;
	};

	/**
		@brief Compare two purchase lists on their key.
		@param before the lines of the file exported first
		@param after the lines of the file exported last
		@return the removed keys in the order of @p before, then the added
		keys in the order of @p after, then the changed ones in the order
		of @p after; empty when the two lists order the same thing

		Matched by LocationTree::indexOfBomLine(), the rule the lines were
		consolidated by when they were written, so that the comparison
		cannot draw the line between two parts anywhere the list did not.
		In the order of the files because the export orders its lines the
		same way twice, and a report in that order reads beside the files.
	*/
	QList<Difference> compare(const QList<LocationTree::BomLine> &before,
				  const QList<LocationTree::BomLine> &after);

	/**
		@brief The first row of the report.

		change, the key, the designation, and then each side as a pair -
		the quantity and its unit - before and after. A pair per side
		because a part can change unit as well as quantity, and one unit
		column would have to say one of the two wrong.
	*/
	const QStringList &reportHeader();

	/**
		@brief One row of the report.
		@param difference what changed
		@param locale whose decimal separator the quantities are written
		with
		@return the cells, in the order of reportHeader()

		The designation is the current one: the file after, unless the
		line was removed. The quantities are written by BomMeasure::
		formatQuantity(), as the export writes them; the side a key is
		missing from has two empty cells, never a zero - a zero would read
		as a quantity somebody wrote.
	*/
	QStringList reportRow(const Difference &difference, const QLocale &locale);

	/**
		@brief The whole report.
		@param differences what compare() answered
		@param locale see reportRow()
		@param end_of_row what ends each row
		@return reportHeader() then one row per difference, each cell
		quoted by QETCsv and each row followed by @p end_of_row

		The header is there when nothing differs, so that an empty report
		still says what it is a report of.
	*/
	QString reportText(const QList<Difference> &differences,
			   const QLocale &locale,
			   const QString &end_of_row);
}

#endif // PURCHASECOMPARISON_H
