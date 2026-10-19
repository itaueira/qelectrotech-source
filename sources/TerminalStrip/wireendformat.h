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
#ifndef WIREENDFORMAT_H
#define WIREENDFORMAT_H

#include <QString>

/**
 * @brief The WireEnd struct
 * The other end of a wire of a terminal, as the plan of a terminal strip
 * writes it above (entry) and under (exit) the terminal.
 */
struct WireEnd
{
		///Tag of the element; the name of the strip for a terminal of a strip
	QString tag;
		///Name of the point; the tag of the terminal for a terminal of a strip
	QString point;
		///Number of the folio of the element, from 1
	QString folio;
		///Cell of the grid of the folio where the element is ("B4")
	QString position;
		///Number of the wire
	QString wire;
		///Description of the element
	QString description;
};

namespace WireEndFormat
{
	/**
		@brief Write @a end with @a pattern.
		The variables %tag, %point, %folio, %position, %wire and
		%description are replaced by the fields of @a end. An empty one
		leaves nothing behind: the ':' '-' or '/' just before it goes with
		it, and a pair of brackets left empty goes too.
		Pure: no project, no folio, so it is checked on its own.
	*/
	QString format(const QString &pattern, const WireEnd &end);
}

#endif // WIREENDFORMAT_H
