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
#ifndef LOCATIONCONTAINMENT_H
#define LOCATIONCONTAINMENT_H

#include <QRectF>
#include <QString>
#include <QVector>

/**
	@brief One rectangle drawn on a folio, and the location it stands for.

	The rectangle is in scene coordinates, which is the only frame the
	comparison can happen in: an area and the components it swallows are
	siblings on the scene, not parent and child.

	z is the stacking order the person looking at the folio sees. It is
	carried here because it is what settles a tie between two areas of the
	same size, and a rule that settles ties by iteration order would give a
	different answer every time the item list is rebuilt.
*/
struct LocationArea
{
	QRectF rect;
	QString path;
	qreal z = 0;
};

/**
	@brief One component that can be mounted somewhere, as the rule sees it.

	rect is in scene coordinates. path is what the component carries right
	now, so that the rule can tell an assignment that must be written from
	one that is already right - the caller pushes nothing for the latter.

	The identifier is deliberately an opaque index rather than a pointer:
	the rule knows nothing about Element, which is what lets it be tested
	without a scene, a project or a database.
*/
struct LocatableItem
{
	int id = -1;
	QRectF rect;
	QString path;
};

/**
	@brief One component and the path it must carry after the rule ran.

	An empty path means clear the key. It is the answer for a component that
	just left an area, and it is not the same as "no result": a component
	the rule has nothing to say about does not appear in the list at all.
*/
struct LocationContainmentChange
{
	int id = -1;
	QString path;
};

/**
	@brief Which components belong to which drawn area, and what changed.

	This is the whole of the live rectangle, minus the drawing. It is a free
	function over plain structures on purpose: containment has to be
	re-evaluated on every move, and a rule that can only be exercised by
	dragging something across a screen is a rule nobody checks.

	Three decisions are folded in here, and each one costs something worth
	stating.

	@par Inside means the centre is inside

	Not full containment. A component snapped against the edge of an
	enclosure - which is exactly where a terminal block goes - would flip
	between in and out on a single pixel of the drawn border, and the person
	who drew the border would have no way to tell which side they were on.
	The centre is stable, it is what somebody looking at the folio judges by,
	and it survives a component wider than the area it sits in. The price is
	that a component half out of the enclosure counts as in, which is the
	benign direction: it appears in the list of what is mounted there and
	whoever reads the list can see it hanging over the edge.

	@par The smallest area wins

	Areas nest: a sub-plate is drawn inside an enclosure, which may be drawn
	inside a room. All three contain the component, and only one of them is
	the answer. The smallest is taken because it is the most specific, and
	because area is available here while the tree is not - the rule stays
	pure and does not have to be handed a LocationTree to know that QCM1 is
	inside PORTE. When two areas of equal size both hold the centre, the one
	drawn on top wins, since that is the one the person sees.

	@par Leaving an area clears the path, and only then

	This is the part that makes 6 -> 5 -> 6 work, and the part that must not
	destroy work somebody did by hand. The rule never clears a path merely
	because no area claims it: a component assigned through the location
	manager, on a folio where nobody ever drew a rectangle, is left alone
	for ever.

	What is cleared is narrower. A component whose current path is the path
	of some area that exists here, while its centre is not inside that area,
	has left it - and is cleared. Nothing else is.

	That is enough for the drag out of the rectangle, and it costs one case
	worth knowing: draw an area for QCM1, and assign a component to QCM1 by
	hand somewhere outside it, and the area wins. The hand assignment is
	dropped on the next move. That is defensible - the drawing now says
	where QCM1 is, and a component elsewhere is not in it - but it is a
	behaviour and not an accident, so it is written down.

	@param areas the rectangles drawn on the folio, in scene coordinates
	@param items the components that can be mounted, in scene coordinates
	@return one entry per component whose path must change, and no entry for
	a component already carrying the right path

	The order of the result follows the order of items, so that a caption
	built from it reads the same way twice.
*/
QVector<LocationContainmentChange>
locationContainmentChanges(const QVector<LocationArea> &areas,
			   const QVector<LocatableItem> &items);

/**
	@brief The path a single component falls into, ignoring what it carries.
	@param areas the rectangles drawn on the folio
	@param rect the component, in scene coordinates
	@return the path of the innermost area holding the centre, empty when
	none does

	Exposed apart from the batch because the creation of an area needs it
	before there is anything to compare against, and because it is the half
	of the rule that has no memory in it.
*/
QString locationPathAt(const QVector<LocationArea> &areas, const QRectF &rect);

#endif // LOCATIONCONTAINMENT_H
