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
		/// Two coordinates closer than this are the same: on one line, or
		/// one point. Not for the length a segment of a terminal keeps,
		/// which is measured exactly - with it the drag stopped at 0.99.
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

		/// @a points with the segment @a index moved by @a d across
		/// itself, before the path is made clean: a neighbour across the
		/// segment takes the move, getting longer or shorter; a neighbour on
		/// the same line keeps its end, and a segment joining the two is
		/// added
	QList<QPointF> moved(const QList<QPointF> &points, int index, double d, bool horizontal)
	{
		const QPointF shift = horizontal ? QPointF(0, d) : QPointF(d, 0);
		const QPointF a = points.at(index);
		const QPointF b = points.at(index + 1);
		const auto onLine = [horizontal](const QPointF &p, const QPointF &q) {
			return horizontal ? std::abs(p.y() - q.y()) < near : std::abs(p.x() - q.x()) < near;
		};

		QList<QPointF> out;
		for (int i = 0 ; i < index ; ++i) out << points.at(i);
		if (onLine(points.at(index - 1), a)) out << a;
		out << a + shift << b + shift;
		if (onLine(points.at(index + 2), b)) out << b;
		for (int i = index + 2 ; i < points.size() ; ++i) out << points.at(i);
		return out;
	}

		/// Whether the segment of a terminal, from @a dock, still points
		/// the way it did - to @a was - and is at least @a minimum long,
		/// or as long as it was if it was shorter. A segment of no length,
		/// with @a out the way its terminal points: it may grow that way,
		/// and never into the terminal
	bool endHolds(const QPointF &dock, const QPointF &was, const QPointF &now, double minimum,
		      const QPointF &out)
	{
		const QPointF o = was - dock;
		const QPointF n = now - dock;
		const double length = std::abs(o.x()) + std::abs(o.y());
		if (length < near)
		{
			if (out.isNull()) return true;
			const double along = n.x() * out.x() + n.y() * out.y();
			const double across = std::abs(out.x() * n.y() - out.y() * n.x());
			return across < near && along > -near;
		}
		const double along = (o.x() * n.x() + o.y() * n.y()) / length;
		const double across = std::abs(o.x() * n.y() - o.y() * n.x()) / length;
		return across < near && along >= std::min(minimum, length) - 1e-6;
	}

	bool endsHold(const QList<QPointF> &before, const QList<QPointF> &after, double minimum,
		      const QPointF &first_out, const QPointF &last_out)
	{
		return endHolds(before.first(), before.at(1), after.at(1), minimum, first_out)
				&& endHolds(before.last(), before.at(before.size() - 2), after.at(after.size() - 2),
					    minimum, last_out);
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

/**
	@brief WirePath::dragged
	@a points with its segment @a index dragged by @a delta - of which only
	the part across the segment counts -, made clean. A neighbour across the
	segment gets longer or shorter, the segment of a terminal included; a
	neighbour on the same line keeps its end, and a segment joining the two
	is added. The segments of the terminals, the first and the last, do not
	drag, keep the way they point, and do not get shorter than
	@a minimum_end: past that, the drag stops where it reaches it - that is
	where the bend is right over the terminal, and further the conductor
	would run into the symbol.
	Computed from where the drag started and the whole of it, and not move
	by move: what one move of the mouse did never limits the next.
	@param middle : where the middle of the dragged segment is now
	@param first_out, last_out : the way the terminal of the first and of
	the last point points, for a segment of that terminal of no length -
	put before a run that leaves the terminal to the side, so that the run
	drags (Conductor::handlerSegments()): it grows that way and never into
	the terminal. Null: such a segment may grow any way, as before
*/
QList<QPointF> WirePath::dragged(const QList<QPointF> &points, int index, const QPointF &delta,
				 QPointF *middle, double minimum_end,
				 const QPointF &first_out, const QPointF &last_out)
{
	const auto holds = [&](const QList<QPointF> &before, const QList<QPointF> &after) {
		return endsHold(before, after, minimum_end, first_out, last_out);
	};
	const auto unchanged = [&]() {
		if (middle && index >= 0 && index + 1 < points.size()) {
			*middle = (points.at(index) + points.at(index + 1)) / 2;
		}
		return points;
	};
	if (points.size() < 4 || index < 1 || index > points.size() - 3) return unchanged();
	const QPointF a = points.at(index);
	const QPointF b = points.at(index + 1);
	const bool horizontal = std::abs(a.y() - b.y()) < near && std::abs(a.x() - b.x()) >= near;
	const bool vertical = std::abs(a.x() - b.x()) < near && std::abs(a.y() - b.y()) >= near;
	if (!horizontal && !vertical) return unchanged();

	const auto attempt = [&](double d) {
		return simplified(moved(points, index, d, horizontal));
	};
	double d = horizontal ? delta.y() : delta.x();
	QList<QPointF> result = attempt(d);
	if (!holds(points, result))
	{
			// The largest part of the drag that keeps the two ends
		double good = 0;
		double bad = d;
		for (int i = 0 ; i < 48 ; ++i)
		{
			const double half = (good + bad) / 2;
			if (holds(points, attempt(half))) good = half;
			else bad = half;
		}
			// On a whole hundredth when that holds too: the drag of the
			// mouse is on the grid, the limit is not
		const double nearest = std::round(good * 100) / 100;
		const double inside = std::trunc(good * 100) / 100;
		d = holds(points, attempt(nearest)) ? nearest
		  : holds(points, attempt(inside)) ? inside
		  : good;
		result = attempt(d);
	}
	if (middle) {
		*middle = (a + b) / 2 + (horizontal ? QPointF(0, d) : QPointF(d, 0));
	}
	return result;
}
