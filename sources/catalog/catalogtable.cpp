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
#include "catalogtable.h"

/**
	@brief CatalogTable::isEmpty
	@return true when there is nothing to import
*/
bool CatalogTable::isEmpty() const
{
	return headers.isEmpty() && rows.isEmpty();
}

/**
	@brief CatalogTable::rowCount
	@return how many data rows the table has
*/
int CatalogTable::rowCount() const
{
	return rows.size();
}

/**
	@brief CatalogTable::columnCount
	@return how many columns the header row declares
*/
int CatalogTable::columnCount() const
{
	return headers.size();
}

/**
	@brief CatalogTable::columnIndex
	@param header
	@return the index of the column named @a header, -1 when there is none
*/
int CatalogTable::columnIndex(const QString &header) const
{
	const QString wanted = header.trimmed();
	if (wanted.isEmpty()) {
		return -1;
	}
	for (int index = 0 ; index < headers.size() ; ++index)
	{
		if (headers.at(index).trimmed().compare(wanted, Qt::CaseInsensitive) == 0) {
			return index;
		}
	}
	return -1;
}

/**
	@brief CatalogTable::value
	@param row
	@param column
	@return the cell, empty when out of range
*/
QString CatalogTable::value(int row, int column) const
{
	if (row < 0 || row >= rows.size() || column < 0) {
		return QString();
	}
	const QStringList &cells = rows.at(row);
	return column < cells.size() ? cells.at(column).trimmed() : QString();
}

/**
	@brief CatalogTable::value
	@param row
	@param header
	@return the cell, empty when the row is short or the column absent
*/
QString CatalogTable::value(int row, const QString &header) const
{
	return value(row, columnIndex(header));
}
