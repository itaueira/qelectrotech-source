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
#include "wirepath.h"

#include <cmath>

namespace
{
	constexpr double near = 0.01;

	bool same(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < near && std::abs(a.y() - b.y()) < near;
	}

		/// From @a a to @a b and on to @a c on one line, turning back at
		/// @a b: the segment to @a c runs back over the one before it
	bool turnsBack(const QPointF &a, const QPointF &b, const QPointF &c)
	{
		if (std::abs(a.x() - b.x()) < near && std::abs(b.x() - c.x()) < near) {
			return (b.y() - a.y()) * (c.y() - b.y()) < 0;
		}
		if (std::abs(a.y() - b.y()) < near && std::abs(b.y() - c.y()) < near) {
			return (b.x() - a.x()) * (c.x() - b.x()) < 0;
		}
		return false;
	}
}

/**
	@brief WirePath::simplified
	@a points without a point repeated, and without the point where the path
	turns back on one line: the segment that runs back over the one before
	it goes, and the tail beyond the bend with it. Two segments one after
	the other in the same direction stay two - the segment joined to a
	terminal and the next one are so in every automatic path, and joining
	them would take the handle away from the second. The first and the last
	point, on the terminals, stay. A path of two points left gets the point
	half way between them back, as the path of a conductor has three.
*/
QList<QPointF> WirePath::simplified(const QList<QPointF> &points)
{
	if (points.size() < 3) return points;

	QList<QPointF> path = points;
	bool changed = true;
	while (changed)
	{
		changed = false;
		for (int i = 1 ; i + 1 < path.size() ; ++i)
		{
			if (same(path.at(i), path.at(i - 1)) || same(path.at(i), path.at(i + 1))
				|| turnsBack(path.at(i - 1), path.at(i), path.at(i + 1))) {
				path.removeAt(i);
				changed = true;
				break;
			}
		}
	}
	if (path.size() == 2) {
		path.insert(1, (path.first() + path.last()) / 2);
	}
	return path;
}
