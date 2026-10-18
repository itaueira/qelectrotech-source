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
#include "purchasecomparison.h"

#include "bomcollector.h"
#include "bommeasure.h"

#include "../catalog/catalogtable.h"
#include "../catalog/catalogtablereader.h"
#include "../utils/csvwriter.h"

namespace PurchaseComparison
{

namespace {

/**
	@param row a row as the reader split it
	@return the row with its cells trimmed and its empty cells at the end
	left out

	A spreadsheet that saves the file again may pad a short row to the
	width of the widest one, or drop the empty cells at its end. Neither
	says anything about the row, and both are forgiven here so that the
	header rows are found and a row is measured by what it holds.
*/
QStringList significantCells(const QStringList &row)
{
	QStringList cells;
	cells.reserve(row.size());
	for (const QString &cell : row) {
		cells << cell.trimmed();
	}
	while (!cells.isEmpty() && cells.last().isEmpty()) {
		cells.removeLast();
	}
	return cells;
}

/// @return true when @p row is the row @p header, cell for cell
bool isRow(const QStringList &row, const QStringList &header)
{
	return significantCells(row) == header;
}

/**
	@param a one unit
	@param b another one
	@return true when a quantity in one is a quantity in the other

	The rule BomCollector adds rows up by, written again because that one
	is private to the collector: a count is a count whatever word sits
	beside it, and two lengths have to be spelled the same.
*/
bool readsAsSameUnit(const QString &a, const QString &b)
{
	if (!BomMeasure::isLengthUnit(a) && !BomMeasure::isLengthUnit(b)) {
		return true;
	}
	return a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive) == 0;
}

/**
	@param integer the digits in front of the decimal separator
	@param group the character that may group them
	@return true when every group after the first is three digits long,
	and the first one is one to three

	"1.234.567" groups and "1.2.3" does not: read by dropping the dots,
	the second would come out 123 and nobody wrote that. Whether the
	groups are digits is left to the reading of the number that follows.
*/
bool readsAsGrouped(const QString &integer, QChar group)
{
	const QStringList parts = integer.split(group);
	if (parts.size() < 2) {
		return true;
	}
	if (parts.first().isEmpty() || parts.first().size() > 3) {
		return false;
	}
	for (int i = 1 ; i < parts.size() ; ++i)
	{
		if (parts.at(i).size() != 3) {
			return false;
		}
	}
	return true;
}

/**
	@param ok where to say that the cell was refused
	@return the number to hand back with the refusal: 1, as
	BomCollector::quantityOf() hands it back, for the caller that ignores
	ok - an understated line is wrong and visible, a line that vanished is
	wrong and is not
*/
double refused(bool *ok)
{
	if (ok) {
		*ok = false;
	}
	return 1.0;
}

/// @return the text of the revision cell of a line: the number, or
/// nothing - the way the export writes it
QString revisionCell(int revision)
{
	return revision > 0 ? QString::number(revision) : QString();
}

} // namespace

const QStringList &lineHeader()
{
	static const QStringList header {
		QStringLiteral("part_code"),
		QStringLiteral("part_revision"),
		QStringLiteral("designation"),
		QStringLiteral("quantity"),
		QStringLiteral("unit"),
		QStringLiteral("tags")
	};
	return header;
}

const QStringList &pendingHeader()
{
	static const QStringList header {
		QStringLiteral("reason"),
		QStringLiteral("tag"),
		QStringLiteral("designation"),
		QStringLiteral("where")
	};
	return header;
}

QString linesCountKey()
{
	return QStringLiteral("lines");
}

/**
	@brief PurchaseComparison::quantityOf
	See the header for the rule, and for the one reading it gets wrong.
*/
double quantityOf(const QString &cell, bool *ok)
{
	QString text = cell.trimmed();
	text.remove(QLatin1Char(' '));
	text.remove(QChar(0x00A0));
	text.remove(QChar(0x202F));
	text.remove(QLatin1Char('\''));
	text.remove(QChar(0x2019));
	if (text.isEmpty()) {
		return refused(ok);
	}

	const QChar dot(QLatin1Char('.'));
	const QChar comma(QLatin1Char(','));
	const int last_dot = int(text.lastIndexOf(dot));
	const int last_comma = int(text.lastIndexOf(comma));

	if (last_dot >= 0 && last_comma >= 0)
	{
		const bool dot_last = last_dot > last_comma;
		const QChar decimal = dot_last ? dot : comma;
		const QChar group = dot_last ? comma : dot;
		const int point = dot_last ? last_dot : last_comma;
		if (!readsAsGrouped(text.left(point), group)) {
			return refused(ok);
		}
		text.remove(group);
		if (text.count(decimal) != 1) {
			return refused(ok);
		}
		text.replace(decimal, dot);
	}
	else if (last_dot >= 0 || last_comma >= 0)
	{
		const QChar mark = last_dot >= 0 ? dot : comma;
		if (text.count(mark) > 1)
		{
			if (!readsAsGrouped(text, mark)) {
				return refused(ok);
			}
			text.remove(mark);
		}
		else
		{
			text.replace(mark, dot);
		}
	}

		//Zero, negative and text refused by the rule the list itself
		//refuses them by.
	return BomCollector::quantityOf(text, ok);
}

/**
	@brief PurchaseComparison::readLines
	See the header for what is refused, and why refusing is the answer.
*/
bool readLines(const QString &text,
	       QList<LocationTree::BomLine> *lines,
	       QString *error)
{
	if (lines) {
		lines->clear();
	}
	auto refuse = [error](const QString &reason)
	{
		if (error) {
			*error = reason;
		}
		return false;
	};

		//A spreadsheet that saves the file again as "CSV UTF-8" puts a
		//byte order mark in front of it; the export writes none.
	QString body = text;
	if (body.startsWith(QChar(0xFEFF))) {
		body.remove(0, 1);
	}

		//The delimiter guessed from the first row, which in a purchase
		//list is "pending;N" - so ';' - and in a list saved again by a
		//spreadsheet set to another language is whatever it wrote. The
		//reader drops the empty rows between the blocks, which is why the
		//blocks are found by their first rows below and never by them.
	const CatalogTable table = CatalogTableReader::parseCsv(body);
	QList<QStringList> rows;
	if (!table.headers.isEmpty()) {
		rows << table.headers;
	}
	rows << table.rows;

	int header = -1;
	for (int i = 0 ; i < rows.size() ; ++i)
	{
		if (isRow(rows.at(i), lineHeader()))
		{
			header = i;
			break;
		}
	}
	if (header < 0)
	{
		return refuse(QStringLiteral(
			"no lines block: the row \"%1\" that opens it is not in the"
			" file, and every purchase list has it")
			.arg(lineHeader().join(QLatin1Char(';'))));
	}

	int pending = -1;
	for (int i = header + 1 ; i < rows.size() ; ++i)
	{
		if (isRow(rows.at(i), pendingHeader()))
		{
			pending = i;
			break;
		}
	}
	if (pending < 0)
	{
		return refuse(QStringLiteral(
			"no pending block after the lines: a purchase list always"
			" ends with the row \"%1\", even with nothing under it, so"
			" this one was cut short")
			.arg(pendingHeader().join(QLatin1Char(';'))));
	}

	int declared = -1;
	for (int i = 0 ; i < header ; ++i)
	{
		const QStringList cells = significantCells(rows.at(i));
		if (cells.value(0) != linesCountKey()) {
			continue;
		}
		bool number = false;
		const int value = cells.value(1).toInt(&number);
		if (number && value >= 0) {
			declared = value;
		}
		break;
	}
	if (declared < 0)
	{
		return refuse(QStringLiteral(
			"no count of lines in front of the lines block: the row"
			" \"%1;N\" is missing or does not say a number")
			.arg(linesCountKey()));
	}
	const int held = pending - header - 1;
	if (declared != held)
	{
		return refuse(QStringLiteral(
			"the file says it has %1 line(s) and holds %2: a row was"
			" lost or added after it was exported")
			.arg(declared)
			.arg(held));
	}

	const QStringList &columns = lineHeader();
	const int code_column = int(columns.indexOf(QStringLiteral("part_code")));
	const int revision_column =
		int(columns.indexOf(QStringLiteral("part_revision")));
	const int designation_column =
		int(columns.indexOf(QStringLiteral("designation")));
	const int quantity_column =
		int(columns.indexOf(QStringLiteral("quantity")));
	const int unit_column = int(columns.indexOf(QStringLiteral("unit")));

	QList<LocationTree::BomLine> read;
	for (int i = header + 1 ; i < pending ; ++i)
	{
		const int number = i - header;
		const QStringList &raw = rows.at(i);
		const QStringList cells = significantCells(raw);
		if (cells.size() > columns.size())
		{
			return refuse(QStringLiteral(
				"line %1 has %2 cells and the header %3: a cell"
				" was split at a separator it should have been"
				" quoted around")
				.arg(number)
				.arg(cells.size())
				.arg(columns.size()));
		}

		LocationTree::BomLine line;
		line.part_code = cells.value(code_column);
		if (line.part_code.isEmpty())
		{
			return refuse(QStringLiteral(
				"line %1 has no part code, and a purchase list"
				" puts an item with none in its pending block")
				.arg(number));
		}

		const QString revision = cells.value(revision_column);
		if (!revision.isEmpty())
		{
			bool readable = false;
			const int value = revision.toInt(&readable);
			if (!readable || value < 0)
			{
				return refuse(QStringLiteral(
					"line %1 (%2): the revision \"%3\" is not"
					" a number")
					.arg(number)
					.arg(line.part_code, revision));
			}
		}
		line.part_revision = BomCollector::revisionOf(revision);

			//As it was written, spaces included: it is shown and never
			//compared.
		line.name = raw.value(designation_column);

		const QString quantity = cells.value(quantity_column);
		bool readable = false;
		line.quantity = quantityOf(quantity, &readable);
		if (!readable)
		{
			return refuse(QStringLiteral(
				"line %1 (%2): \"%3\" is not a quantity")
				.arg(number)
				.arg(line.part_code, quantity));
		}
		line.unit = cells.value(unit_column);

		const int first = LocationTree::indexOfBomLine(
			read, line.part_code, line.part_revision);
		if (first >= 0)
		{
			return refuse(QStringLiteral(
				"lines %1 and %2 are both part %3, revision \"%4\":"
				" a purchase list has one line per part and"
				" revision")
				.arg(first + 1)
				.arg(number)
				.arg(line.part_code, revisionCell(line.part_revision)));
		}
		read << line;
	}

	if (lines) {
		*lines = read;
	}
	return true;
}

/**
	@brief PurchaseComparison::sameQuantity
	See the header: compared as printed, in the C locale.
*/
bool sameQuantity(const LocationTree::BomLine &before,
		  const LocationTree::BomLine &after)
{
	if (!readsAsSameUnit(before.unit, after.unit)) {
		return false;
	}
	const QLocale c = QLocale::c();
	return BomMeasure::formatQuantity(before.quantity, before.unit, c)
			== BomMeasure::formatQuantity(after.quantity, after.unit, c);
}

QString changeKey(Change change)
{
	switch (change)
	{
		case Change::Removed:
			return QStringLiteral("removed");
		case Change::Added:
			return QStringLiteral("added");
		case Change::Changed:
			return QStringLiteral("changed");
	}
	return QStringLiteral("unknown");
}

const LocationTree::BomLine &Difference::current() const
{
	return change == Change::Removed ? before : after;
}

/**
	@brief PurchaseComparison::compare
	See the header for the order, and for the rule the keys are matched by.
*/
QList<Difference> compare(const QList<LocationTree::BomLine> &before,
			  const QList<LocationTree::BomLine> &after)
{
	QList<Difference> removed;
	for (const LocationTree::BomLine &line : before)
	{
		if (LocationTree::indexOfBomLine(after, line.part_code,
						 line.part_revision) < 0)
		{
			Difference difference;
			difference.change = Change::Removed;
			difference.before = line;
			removed << difference;
		}
	}

	QList<Difference> added;
	QList<Difference> changed;
	for (const LocationTree::BomLine &line : after)
	{
		const int found = LocationTree::indexOfBomLine(
			before, line.part_code, line.part_revision);
		if (found < 0)
		{
			Difference difference;
			difference.change = Change::Added;
			difference.after = line;
			added << difference;
			continue;
		}
		if (!sameQuantity(before.at(found), line))
		{
			Difference difference;
			difference.change = Change::Changed;
			difference.before = before.at(found);
			difference.after = line;
			changed << difference;
		}
	}

	return removed + added + changed;
}

const QStringList &reportHeader()
{
	static const QStringList header {
		QStringLiteral("change"),
		QStringLiteral("part_code"),
		QStringLiteral("part_revision"),
		QStringLiteral("designation"),
		QStringLiteral("quantity_before"),
		QStringLiteral("unit_before"),
		QStringLiteral("quantity_after"),
		QStringLiteral("unit_after")
	};
	return header;
}

QStringList reportRow(const Difference &difference, const QLocale &locale)
{
	const LocationTree::BomLine &key = difference.current();
	const bool has_before = difference.change != Change::Added;
	const bool has_after = difference.change != Change::Removed;

	QStringList row;
	row << changeKey(difference.change)
	    << key.part_code
	    << revisionCell(key.part_revision)
	    << key.name;
	if (has_before)
	{
		row << BomMeasure::formatQuantity(difference.before.quantity,
						  difference.before.unit, locale)
		    << difference.before.unit;
	}
	else
	{
		row << QString() << QString();
	}
	if (has_after)
	{
		row << BomMeasure::formatQuantity(difference.after.quantity,
						  difference.after.unit, locale)
		    << difference.after.unit;
	}
	else
	{
		row << QString() << QString();
	}
	return row;
}

QString reportText(const QList<Difference> &differences,
		   const QLocale &locale,
		   const QString &end_of_row)
{
	QString text = QETCsv::row(reportHeader()) + end_of_row;
	for (const Difference &difference : differences) {
		text += QETCsv::row(reportRow(difference, locale)) + end_of_row;
	}
	return text;
}

} // namespace PurchaseComparison
