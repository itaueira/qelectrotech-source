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
#ifndef SYMBOLTERMINALS_H
#define SYMBOLTERMINALS_H

#include <QDomElement>
#include <QList>
#include <QPointF>
#include <QtGlobal>

/**
	@brief The terminals of a symbol on whole numbers (T72).

	A wire ends on the docking point of a terminal, and a terminal lands on
	the folio at the position of its element plus its place in the symbol.
	An element sits on the grid of the folio, on whole numbers for any grid
	of 1 or finer, so a terminal at y = -15.83 in its symbol puts the end of
	every wire on it 0.17 off any line the terminal of another element can
	be on: the wire from a terminal in line with it bends by that much, and
	no grid takes the step away.

	Only the place of each <terminal> of a <definition> is rounded, x and y,
	to the nearest whole number, half up (nearestWhole()). The drawing is left as
	it is: a terminal moves by less than half a unit, which the line reaching
	it does not show, and the uuid of the terminal stays - the wires of a
	project are attached to it by that uuid. A symbol two of whose terminals
	would meet on one point once rounded is left as it is, whole: the
	two wires would land on one point, which nobody drew.

	A rule on the XML of a symbol and nothing else - QtCore and QtXml - so
	that it is proved without a project; bringing it to the symbols of a
	project and to the elements placed from them is
	EmbeddedElementRefresh::reopenedOnWholeNumbers().
*/
namespace SymbolTerminals
{
		/// What putting the terminals of one symbol on whole numbers does
	struct Rounding
	{
		int terminals = 0;     ///< terminals off whole numbers, in x or y or both
		bool merged = false;   ///< two terminals would meet: nothing is moved
	};

		/// One terminal put on whole numbers
	struct Move
	{
		QDomElement terminal;  ///< the <terminal>, as changed
		QPointF from;          ///< its place before
		QPointF to;            ///< its place now, on whole numbers
	};

	bool isWhole(qreal value);
	qreal nearestWhole(qreal value);
	Rounding planned(const QDomElement &definition);
	Rounding putOnWholeNumbers(const QDomElement &definition,
				   QList<Move> *moves = nullptr);
}

#endif // SYMBOLTERMINALS_H
