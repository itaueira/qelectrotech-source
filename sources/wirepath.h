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
#ifndef WIREPATH_H
#define WIREPATH_H

#include <QList>
#include <QPointF>

/**
	@brief The path of a conductor, as a list of points, made clean again
	after a segment was dragged.
	Dragging the segment next to the one joined to a terminal towards the
	terminal does not shorten that one: a segment is inserted between the
	two, and when the drag goes past the end of the terminal's segment the
	inserted one runs back over it - a tail beyond the bend, drawn on top of
	the conductor itself (T68).
	Pure on purpose: points in, points out.
*/
namespace WirePath
{
	QList<QPointF> simplified(const QList<QPointF> &points);
}

#endif // WIREPATH_H
