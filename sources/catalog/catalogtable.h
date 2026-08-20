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
#ifndef CATALOGTABLE_H
#define CATALOGTABLE_H

#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief The CatalogTable class
	A spreadsheet as the importer sees it: one header row and the data rows.

	Reading a file and deciding what its columns mean are two different jobs,
	and this is what sits between them. It has no notion of a catalog, which
	is why the column mapping can be tried, shown and corrected before
	anything is written.
*/
class CatalogTable
{
	public:
		QStringList headers;
		QList<QStringList> rows;

		bool isEmpty() const;
		int rowCount() const;
		int columnCount() const;

		/**
			@param header
			@return the index of the column named @a header, -1 when there is
			none. The comparison ignores case and surrounding spaces, because
			a header typed by hand in a spreadsheet does neither.
		*/
		int columnIndex(const QString &header) const;

		/**
			@param row
			@param header
			@return the cell, empty when the row is short or the column absent.
			A short row is normal in an exported spreadsheet and must not throw
			the import away.
		*/
		QString value(int row, const QString &header) const;
		QString value(int row, int column) const;
};

#endif // CATALOGTABLE_H
