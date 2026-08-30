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
#ifndef IOSHEET_H
#define IOSHEET_H

#include "iopoint.h"

#include <QCoreApplication>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

/**
	@brief A grid of cells read as a list of I/O points.

	The sheet decides the format, not the program: the automation department
	sends what it sends, and the import maps its columns rather than asking it
	to be rewritten. Two shapes have to work with no configuration at all - the
	two column sheet, type and description, and the full one - and everything
	between them is a matter of which column feeds which field.

	Nothing here reads a file or opens a dialogue. It takes the grid
	CircuitClipboard::parse already knows how to build - from the clipboard or
	from a CSV - and turns it into points, which is what lets the test binary
	link it on its own.
*/
class IoSheet
{
	Q_DECLARE_TR_FUNCTIONS(IoSheet)

	public:
		/**
			@brief Which column of the sheet feeds which field.

			A field the sheet does not have is simply not mapped, and
			fields() is then the set the import is allowed to write. That
			set is what keeps a two column sheet from blanking the address
			and the card of ninety-six points that were already assigned.
		*/
	struct Mapping
	{
			/// true when the first row of the grid holds the column names
		bool has_header = false;

		int columnOf(IoField field) const;
		void setColumn(IoField field, int column);
		void unsetField(IoField field);
		bool isEmpty() const;
			/// @return the fields this sheet carries, as flags
		IoFields fields() const;
			/// @return the mapped fields, in the order of the sheet
		QList<IoField> mappedFields() const;

		private:
			QMap<int, int> m_columns;
	};

		/**
			@brief What one reading of a grid produced.

			The counts and the row numbers are what the dialogue shows
			before anything enters the project: a person who is about to
			import sixty points has to be able to see that the sheet had
			sixty one rows and that row 25 was blank.
		*/
	struct Report
	{
			/// the points, in the order of the sheet
		QList<IoPoint> points;
			/// rows that held nothing at all, by their number in the sheet
		QList<int> blank_rows;
			/// rows whose type cell said something nobody recognised
		QList<int> unknown_type_rows;
			/// rows that had a type but nothing to name the point by
		QList<int> nameless_rows;

		bool isEmpty() const;
			/// @return true when every row of the sheet was understood
		bool isClean() const;
			/// @return the whole thing in one paragraph, ready to be shown
		QString text() const;
	};

		/**
			@brief The mapping of a sheet with no header row.
			@return type in the first column, description in the second

			The minimum the task asks for, and the shape a sheet takes when
			nobody thought about the import while writing it.
		*/
	static Mapping basic();

		/**
			@brief Read a row as column names.
			@param header the cells of the first row
			@return what each name was understood to be

			Names are compared folded - no case, no accent, no double space -
			against what the three languages of this trade actually write in
			that cell. has_header comes back true only when at least two
			columns were recognised: one alone is as likely to be a data row
			that happens to start with the word "tipo".
		*/
	static Mapping guess(const QStringList &header);

		/**
			@brief The mapping to use for a grid, header or no header.
			@param grid
			@return the guessed mapping, or the basic one when there is none
		*/
	static Mapping mappingFor(const QList<QStringList> &grid);

		/**
			@brief Turn a grid into points.
			@param grid the cells, one QStringList per row
			@param mapping which column feeds which field
			@return the points and everything that was not plain sailing

			A blank row in the middle never truncates the reading. It is
			skipped, its number is kept, and the caller shows it: importing
			twenty four of sixty and reporting success is the one outcome
			the task forbids.
		*/
	static Report read(const QList<QStringList> &grid, const Mapping &mapping);

		/// @return the name of @a field, as a person reading a dialogue knows it
	static QString fieldName(IoField field);
		/// @return every field a column can feed, in a sensible order
	static QList<IoField> mappableFields();

		/**
			@brief Read a cell that answers yes or no.
			@param text
			@return true when the cell says yes in any of the usual ways
		*/
	static bool isYes(const QString &text);
};

#endif // IOSHEET_H
