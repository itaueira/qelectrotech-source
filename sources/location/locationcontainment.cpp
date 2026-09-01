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
#include "locationcontainment.h"

#include <QSet>

namespace
{
	/**
		@brief The area a rectangle covers, after being straightened out.
		@param rect any rectangle, normalized or not
		@return width times height, zero for a rectangle with no surface

		The rectangles arrive built from two handle points, so the one on
		screen may well have a negative width - the person dragged right to
		left. Straightening happens here, once, rather than being a rule the
		callers have to remember.
	*/
	qreal surfaceOf(const QRectF &rect)
	{
		const QRectF r{rect.normalized()};
		return r.width() * r.height();
	}

	/**
		@brief Whether a point falls in a rectangle that actually has surface.
		@param rect the candidate area
		@param point the point looked for
		@return true when the straightened rectangle holds the point

		A rectangle with no surface is refused rather than consulted. That is
		the state an area is in while it is being drawn, before the second
		handle has moved, and a rectangle of nothing should not swallow the
		component that happens to sit under the first click.
	*/
	bool holdsPoint(const QRectF &rect, const QPointF &point)
	{
		const QRectF r{rect.normalized()};
		if (!r.isValid()) {
			return false;
		}
		return r.contains(point);
	}
}

/**
	@brief The path a single component falls into, ignoring what it carries.
	@param areas the rectangles drawn on the folio
	@param rect the component, in scene coordinates
	@return the path of the innermost area holding the centre, empty when none

	An area with no path assigned yet takes part in nothing. It is a rectangle
	somebody drew and has not named, and until it has a name it cannot answer
	the question being asked here. The consequence is worth knowing and is the
	one that helps: a small unnamed rectangle drawn inside a named enclosure
	does not steal the components from it, and the day it gets a name they move
	across.
*/
QString locationPathAt(const QVector<LocationArea> &areas, const QRectF &rect)
{
	const QPointF centre{rect.normalized().center()};

	QString best_path;
	qreal best_surface{0};
	qreal best_z{0};

	for (const auto &area : areas)
	{
		if (area.path.isEmpty()) {
			continue;
		}
		if (!holdsPoint(area.rect, centre)) {
			continue;
		}

		const qreal surface{surfaceOf(area.rect)};

		if (best_path.isEmpty()
		    || surface < best_surface
		    || (qFuzzyCompare(surface, best_surface) && area.z > best_z))
		{
			best_path = area.path;
			best_surface = surface;
			best_z = area.z;
		}
	}

	return best_path;
}

/**
	@brief Which components belong to which drawn area, and what changed.
	@param areas the rectangles drawn on the folio, in scene coordinates
	@param items the components that can be mounted, in scene coordinates
	@return one entry per component whose path must change

	The full reasoning, and what each decision costs, is in the header. What is
	worth pointing at from here is the shape of the loop: every component is
	asked the same question twice - which area am I in, and does the path I
	carry still make sense - and only the difference between the two answers
	comes out. A component already carrying the right path produces nothing,
	which is what lets the caller push no command at all when nothing moved.
*/
QVector<LocationContainmentChange>
locationContainmentChanges(const QVector<LocationArea> &areas,
			   const QVector<LocatableItem> &items)
{
	//Collected once: the paths that some rectangle on this folio stands for.
	//It is what tells an assignment that came from a drawing, and can be
	//retracted by the drawing, from one somebody made by hand elsewhere.
	QSet<QString> drawn_paths;
	for (const auto &area : areas)
	{
		if (!area.path.isEmpty()) {
			drawn_paths.insert(area.path);
		}
	}

	QVector<LocationContainmentChange> changes;

	for (const auto &item : items)
	{
		const QString path{locationPathAt(areas, item.rect)};

		if (!path.isEmpty())
		{
			if (item.path != path) {
				changes.append({item.id, path});
			}
			continue;
		}

		//Inside nothing. The path is only taken away when a rectangle
		//claiming it exists here, which means the component left it.
		//Anything else is somebody's hand assignment and stays.
		if (!item.path.isEmpty() && drawn_paths.contains(item.path)) {
			changes.append({item.id, QString()});
		}
	}

	return changes;
}
