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
#include "drillingorigin.h"

#include <cmath>
#include <limits>

namespace
{
	/**
		@brief isShift
		@param value a distance in millimetre
		@return true when somebody actually moved the zero

		A zero is not a shift, and neither is a number that is not one.
		Asked with the slack of the family rather than against nought,
		so that a value that went through the file and came back reads
		as the same choice it went in as - and so that the default keeps
		reading as the default after a round trip, which is what stops a
		project asking to be saved for ever.
	*/
	bool isShift(qreal value)
	{
		return std::isfinite(value)
		       && std::fabs(value) > MountingArea::tolerance();
	}

	/**
		@brief notMeasured
		@return the answer to a coordinate that cannot be worked out

		Not a number, spelled the way MountingMeasure spells it, because
		the two files answer the same kind of question and a reader who
		learnt the convention in one has to find it in the other. Every
		printer of this module already folds it to "?" - what must never
		come back from here is a plausible number.
	*/
	qreal notMeasured()
	{
		return std::numeric_limits<qreal>::quiet_NaN();
	}

	/**
		@brief sameShift
		@param before a distance in millimetre
		@param after another one
		@return true when the two are the same shift

		Two numbers that are not shifts count as the same, for the
		reason MountingLayout compares two absent measurements as equal:
		a difference between two nothings is not a change a person made.
	*/
	bool sameShift(qreal before, qreal after)
	{
		if (!isShift(before) || !isShift(after)) {
			return !isShift(before) && !isShift(after);
		}
		return std::fabs(before - after) <= MountingArea::tolerance();
	}
}

/**
	@brief DrillingOrigin::DrillingOrigin
	@param origin_corner which corner of the surface the zero sits at
	@param origin_offset how far from that corner, millimetre
*/
DrillingOrigin::DrillingOrigin(DrillingCorner origin_corner,
			       const QPointF &origin_offset) :
	corner(origin_corner),
	offset(origin_offset)
{
}

/**
	@brief DrillingOrigin::isDefault
	@return true when this is the origin a project that never chose one is
	read as
*/
bool DrillingOrigin::isDefault() const
{
	return corner == DrillingCorner::TopLeft && !hasOffset();
}

/**
	@brief DrillingOrigin::hasOffset
	@return true when the zero is shifted off the corner

	Either coordinate is enough. A datum on the left edge of the plate, half
	way down it, is shifted in y and not in x, and it is as much a datum as
	one shifted both ways.
*/
bool DrillingOrigin::hasOffset() const
{
	return isShift(offset.x()) || isShift(offset.y());
}

/**
	@brief DrillingOrigin::needsSurfaceSize
	@return true when the surface dimensions are needed to measure from here

	Read off the two axis directions rather than compared against the top
	left corner, so that a corner added to the enumeration later cannot get
	this wrong by omission: whichever way a new corner runs, it needs the
	width when x does not run right and the height when y does not run down.
*/
bool DrillingOrigin::needsSurfaceSize() const
{
	return !xGrowsRight() || !yGrowsDown();
}

/**
	@brief DrillingOrigin::xGrowsRight
	@return true when x grows to the right of the plate
*/
bool DrillingOrigin::xGrowsRight() const
{
	return corner == DrillingCorner::TopLeft
	       || corner == DrillingCorner::BottomLeft;
}

/**
	@brief DrillingOrigin::yGrowsDown
	@return true when y grows towards the bottom of the plate
*/
bool DrillingOrigin::yGrowsDown() const
{
	return corner == DrillingCorner::TopLeft
	       || corner == DrillingCorner::TopRight;
}

/**
	@brief DrillingOrigin::cornerName
	@param corner the corner
	@return what to call it out loud
*/
QString DrillingOrigin::cornerName(DrillingCorner corner)
{
	switch (corner)
	{
		case DrillingCorner::TopRight:
			return tr("coin supérieur droit");
		case DrillingCorner::BottomLeft:
			return tr("coin inférieur gauche");
		case DrillingCorner::BottomRight:
			return tr("coin inférieur droit");
		case DrillingCorner::TopLeft:
			break;
	}
	return tr("coin supérieur gauche");
}

/**
	@brief DrillingOrigin::corners
	@return every corner, in the order a selector lists them

	The default first, and then round the plate. A list and not four
	constants written again at every call site, so that the day a fifth
	answer exists - a datum named on the drawing, say - the selector that
	shows them does not have to be found and edited too.
*/
QList<DrillingCorner> DrillingOrigin::corners()
{
	QList<DrillingCorner> all;
	all << DrillingCorner::TopLeft
	    << DrillingCorner::TopRight
	    << DrillingCorner::BottomRight
	    << DrillingCorner::BottomLeft;
	return all;
}

/**
	@brief DrillingOrigin::cornerToken
	@param corner the corner
	@return the corner as the .qet holds it
*/
QString DrillingOrigin::cornerToken(DrillingCorner corner)
{
	switch (corner)
	{
		case DrillingCorner::TopRight:
			return QStringLiteral("top-right");
		case DrillingCorner::BottomLeft:
			return QStringLiteral("bottom-left");
		case DrillingCorner::BottomRight:
			return QStringLiteral("bottom-right");
		case DrillingCorner::TopLeft:
			break;
	}
	return QStringLiteral("top-left");
}

/**
	@brief DrillingOrigin::cornerFromToken
	@param token the attribute as the file holds it
	@param ok set to false when the token names no corner
	@return the corner, the default one when the token names none

	Trimmed and case folded on the way in, because this attribute is one a
	person edits by hand in a .qet more readily than most: it is four words
	of English in a file of numbers.
*/
DrillingCorner DrillingOrigin::cornerFromToken(const QString &token, bool *ok)
{
	const QString wanted = token.trimmed().toLower();

	const QList<DrillingCorner> all = corners();
	for (DrillingCorner corner : all)
	{
		if (cornerToken(corner) == wanted)
		{
			if (ok) {
				*ok = true;
			}
			return corner;
		}
	}

	if (ok) {
		*ok = false;
	}
	return DrillingCorner::TopLeft;
}

bool DrillingOrigin::operator==(const DrillingOrigin &other) const
{
	return corner == other.corner
	       && sameShift(offset.x(), other.offset.x())
	       && sameShift(offset.y(), other.offset.y());
}

bool DrillingOrigin::operator!=(const DrillingOrigin &other) const
{
	return !(*this == other);
}

/**
	@brief DrillingFrame::DrillingFrame
	@param frame_origin where the zero is, as the face declares it
	@param frame_area the face the coordinates are measured on
*/
DrillingFrame::DrillingFrame(const DrillingOrigin &frame_origin,
			     const MountingArea &frame_area) :
	origin(frame_origin),
	area(frame_area)
{
}

/**
	@brief DrillingFrame::canMeasure
	@return true when a coordinate can be worked out in this frame at all
*/
bool DrillingFrame::canMeasure() const
{
	return !origin.needsSurfaceSize() || area.isValid();
}

/**
	@brief DrillingFrame::coordinateOf
	@param position where the hole is, in the model frame
	@return the coordinate to print, in this frame

	The one subtraction, and the one flip, in the whole module. Two things
	happen here in this order and the order matters: the axis is turned over
	against the far edge of the plate first, and the shift of the zero is
	taken off afterwards. Doing it the other way round would measure the
	shift from the model origin instead of from the chosen corner, which is
	the sign error this arrangement exists to make impossible - it would put
	the datum on the wrong side of the plate for three of the four corners
	and be right for the fourth, which is the worst way for a defect to
	behave.
*/
QPointF DrillingFrame::coordinateOf(const QPointF &position) const
{
	if (!canMeasure()) {
		return QPointF(notMeasured(), notMeasured());
	}

	const qreal from_corner_x = origin.xGrowsRight()
				    ? position.x()
				    : (area.width - position.x());
	const qreal from_corner_y = origin.yGrowsDown()
				    ? position.y()
				    : (area.height - position.y());

	return QPointF(from_corner_x - origin.offset.x(),
		       from_corner_y - origin.offset.y());
}

/**
	@brief DrillingFrame::positionOf
	@param coordinate a coordinate in this frame
	@return the position in the model frame

	coordinateOf undone, step for step in the other order: the shift goes
	back on first, and then the axis is turned over again.
*/
QPointF DrillingFrame::positionOf(const QPointF &coordinate) const
{
	if (!canMeasure()) {
		return QPointF(notMeasured(), notMeasured());
	}

	const qreal from_corner_x = coordinate.x() + origin.offset.x();
	const qreal from_corner_y = coordinate.y() + origin.offset.y();

	return QPointF(origin.xGrowsRight() ? from_corner_x
					    : (area.width - from_corner_x),
		       origin.yGrowsDown() ? from_corner_y
					   : (area.height - from_corner_y));
}

bool DrillingFrame::operator==(const DrillingFrame &other) const
{
	if (origin != other.origin) {
		return false;
	}
	return sameShift(area.width, other.area.width)
	       && sameShift(area.height, other.area.height);
}

bool DrillingFrame::operator!=(const DrillingFrame &other) const
{
	return !(*this == other);
}
