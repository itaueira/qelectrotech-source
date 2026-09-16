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
#include "mountingalign.h"

#include "mountingmeasure.h"

#include <QRectF>
#include <QSizeF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	/**
		@param position a position in millimetre
		@return true when both numbers are numbers
	*/
	bool isAPosition(const QPointF &position)
	{
		return std::isfinite(position.x())
		       && std::isfinite(position.y());
	}

	/// @return where @a rect begins along @a run, millimetre
	qreal runStart(const QRectF &rect, MountingRun run)
	{
		return run == MountingRun::Down ? rect.top() : rect.left();
	}

	/// @return where @a rect ends along @a run, millimetre
	qreal runEnd(const QRectF &rect, MountingRun run)
	{
		return run == MountingRun::Down ? rect.bottom() : rect.right();
	}

	/// @return how much room @a rect takes along @a run, millimetre
	qreal runLength(const QRectF &rect, MountingRun run)
	{
		return run == MountingRun::Down ? rect.height() : rect.width();
	}

	/**
		@param items the parts handed over
		@return the ones either gesture is allowed to move, in the
		order they were handed over
	*/
	QList<MountedItem> movable(const QList<MountedItem> &items)
	{
		QList<MountedItem> chosen;

		for (const MountedItem &item : items)
		{
			if (MountingAlign::isMovable(item)) {
				chosen << item;
			}
		}

		return chosen;
	}

	/**
		@param items the parts, already filtered
		@return the box around all of them, millimetre

		Edge by edge, and NOT by QRectF::united, which is the trap this
		function exists to avoid: united() answers "the other one"
		whenever a rectangle is null, and the footprint of a part nobody
		has measured is null - it is a point. A box built with united()
		would quietly drop every unmeasured part out of the selection,
		so a row whose leftmost part has no width would be aligned onto
		the second one from the left and nobody would see why.
	*/
	QRectF boxAround(const QList<MountedItem> &items)
	{
		bool first = true;
		qreal left = 0.0;
		qreal top = 0.0;
		qreal right = 0.0;
		qreal bottom = 0.0;

		for (const MountedItem &item : items)
		{
			const QRectF footprint = item.footprint();

			if (first)
			{
				left = footprint.left();
				top = footprint.top();
				right = footprint.right();
				bottom = footprint.bottom();
				first = false;
				continue;
			}

			left = qMin(left, footprint.left());
			top = qMin(top, footprint.top());
			right = qMax(right, footprint.right());
			bottom = qMax(bottom, footprint.bottom());
		}

		if (first) {
			return QRectF();
		}

		return QRectF(QPointF(left, top), QPointF(right, bottom));
	}
}

/**
	@brief MountingAlign::isMovable
	@param item anything mounted on a surface
	@return true when this part can be lined up or spread

	A piece cut to length is refused here and not in each gesture, so that
	the two can never disagree about it: a rail left behind by an alignment
	but taken by a distribution would move the breakers of one row and not
	of the other, and both would look right on their own.
*/
bool MountingAlign::isMovable(const MountedItem &item)
{
	return !item.uuid.isEmpty()
	       && !item.isCutToLength()
	       && isAPosition(item.position);
}

/**
	@brief MountingAlign::movableUuids
	@param items the parts handed over
	@return the identities either gesture will move
*/
QStringList MountingAlign::movableUuids(const QList<MountedItem> &items)
{
	QStringList uuids;

	for (const MountedItem &item : items)
	{
		if (isMovable(item)) {
			uuids << item.uuid;
		}
	}

	return uuids;
}

/**
	@brief MountingAlign::referenceOf
	@param items the parts to line up
	@param alignment which line they end up sharing
	@return the coordinate they are lined up onto, millimetre
*/
qreal MountingAlign::referenceOf(const QList<MountedItem> &items,
				 MountingAlignment alignment)
{
	const QList<MountedItem> chosen = movable(items);

	if (chosen.isEmpty()) {
		return std::numeric_limits<qreal>::quiet_NaN();
	}

	const QRectF box = boxAround(chosen);

	switch (alignment)
	{
		case MountingAlignment::LeftEdges:
			return box.left();
		case MountingAlignment::RightEdges:
			return box.right();
		case MountingAlignment::TopEdges:
			return box.top();
		case MountingAlignment::BottomEdges:
			return box.bottom();
		case MountingAlignment::VerticalAxes:
			return box.center().x();
		case MountingAlignment::HorizontalAxes:
			return box.center().y();
	}

	return std::numeric_limits<qreal>::quiet_NaN();
}

/**
	@brief MountingAlign::aligned
	@param items the parts to line up
	@param alignment which line they end up sharing
	@return the new position of each part that moves, millimetre
*/
QHash<QString, QPointF> MountingAlign::aligned(const QList<MountedItem> &items,
					       MountingAlignment alignment)
{
	QHash<QString, QPointF> moves;

	const QList<MountedItem> chosen = movable(items);
	if (chosen.count() < 2) {
		return moves;
	}

	const qreal reference = referenceOf(chosen, alignment);
	if (!std::isfinite(reference)) {
		return moves;
	}

	for (const MountedItem &item : chosen)
	{
		const QSizeF size = item.declaredSize();
		QPointF landing = item.position;

		switch (alignment)
		{
			case MountingAlignment::LeftEdges:
				landing.setX(reference);
				break;
			case MountingAlignment::RightEdges:
				landing.setX(reference - size.width());
				break;
			case MountingAlignment::TopEdges:
				landing.setY(reference);
				break;
			case MountingAlignment::BottomEdges:
				landing.setY(reference - size.height());
				break;
			case MountingAlignment::VerticalAxes:
				landing.setX(reference - size.width() / 2.0);
				break;
			case MountingAlignment::HorizontalAxes:
				landing.setY(reference - size.height() / 2.0);
				break;
		}

			//Only what moves goes in the answer. The slack is the
			//one the mounting rules already use, so that a part a
			//nanometre from the line is not moved onto it just to
			//leave a step on a stack.
		if (MountingMeasure::isSameLength(landing.x(), item.position.x())
		    && MountingMeasure::isSameLength(landing.y(),
						     item.position.y())) {
			continue;
		}

		moves.insert(item.uuid, landing);
	}

	return moves;
}

/**
	@brief MountingAlign::spreadGap
	@param items the parts to spread
	@param run along which axis they are spread
	@return the air left between two of them, millimetre
*/
qreal MountingAlign::spreadGap(const QList<MountedItem> &items,
			       MountingRun run)
{
	const QList<MountedItem> chosen = movable(items);

	if (chosen.count() < 3) {
		return std::numeric_limits<qreal>::quiet_NaN();
	}

	const QRectF box = boxAround(chosen);
	qreal bodies = 0.0;

	for (const MountedItem &item : chosen) {
		bodies += runLength(item.footprint(), run);
	}

		//The room they stand in, less the room they take, shared out
		//between the spaces - and there is one space fewer than there
		//are parts, which is the arithmetic everybody gets wrong once.
	return (runLength(box, run) - bodies) / qreal(chosen.count() - 1);
}

/**
	@brief MountingAlign::spread
	@param items the parts to spread
	@param run along which axis they are spread
	@return the new position of each part that moves, millimetre
*/
QHash<QString, QPointF> MountingAlign::spread(const QList<MountedItem> &items,
					      MountingRun run)
{
	QHash<QString, QPointF> moves;

	QList<MountedItem> chosen = movable(items);
	if (chosen.count() < 3) {
		return moves;
	}

	const qreal gap = spreadGap(chosen, run);
	if (!std::isfinite(gap)) {
		return moves;
	}

		//In the order they stand, and not in the order they were
		//handed over: a selection is a set and the row it makes on the
		//plate is the thing being spread. Stable, so that two parts at
		//the same millimetre keep the order the caller gave them
		//instead of swapping places for no reason a person could see.
	std::stable_sort(chosen.begin(), chosen.end(),
			 [run](const MountedItem &first,
			       const MountedItem &second)
			 {
				 return runStart(first.footprint(), run)
					< runStart(second.footprint(), run);
			 });

	qreal cursor = runStart(boxAround(chosen), run);

	for (const MountedItem &item : chosen)
	{
		const qreal length = runLength(item.footprint(), run);
		QPointF landing = item.position;

		if (run == MountingRun::Down) {
			landing.setY(cursor);
		}
		else {
			landing.setX(cursor);
		}

		cursor += length + gap;

		if (MountingMeasure::isSameLength(landing.x(), item.position.x())
		    && MountingMeasure::isSameLength(landing.y(),
						     item.position.y())) {
			continue;
		}

		moves.insert(item.uuid, landing);
	}

	return moves;
}
