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
#ifndef CSVWRITER_H
#define CSVWRITER_H

#include <QString>
#include <QStringList>

/**
	@brief RFC-4180 quoting for the delimited text the program exports.

	Every list the program writes - the two bills of material, the wiring
	list, the wire numbers, the catalogue table and the command-line
	exports - is a grid of user-written text joined by a separator. A cell
	holding that separator, a double quote or an end of line shifts every
	column of its row, and nothing downstream can tell a shifted row from
	a well-formed one: the file is wrong and it still opens. A designation
	such as "Contactor 3P; 25A" is enough.

	The rule lives here, once, because it was written twice already and
	the two copies had different ideas of what the separator was. Quoting
	is applied only where it is needed, so a list whose cells are all
	plain comes out byte for byte the way it did before.
*/
namespace QETCsv
{
		/**
			@brief Quote one cell for a delimited file.
			@param value : the raw text of the cell
			@param separator : the delimiter the row will be joined with
			@return @p value, quoted only when it has to be
		*/
	QString field(const QString &value,
		      const QString &separator = QStringLiteral(";"));

		/**
			@brief Quote and join one whole row.
			@param values : the cells, in column order
			@param separator : the delimiter between them
			@return the row, without its end of line
		*/
	QString row(const QStringList &values,
		    const QString &separator = QStringLiteral(";"));
}

#endif // CSVWRITER_H
