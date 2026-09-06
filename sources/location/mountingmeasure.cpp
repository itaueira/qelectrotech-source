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
#include "mountingmeasure.h"

#include <QRectF>
#include <QSizeF>

#include <cmath>
#include <limits>

namespace
{
	/**
		@return the answer given when there is no distance to give

		Not zero. Zero is a distance, and it is the distance between
		two points that sit on top of one another - handing it back for
		a coordinate nobody typed would report those two cases as the
		same one, and one of them is a drawing that is finished.
	*/
	qreal notALength()
	{
		return std::numeric_limits<qreal>::quiet_NaN();
	}

	/**
		@param point a point in millimetre
		@return true when both of its coordinates are numbers
	*/
	bool isFinitePoint(const QPointF &point)
	{
		return std::isfinite(point.x()) && std::isfinite(point.y());
	}
}

/**
	@brief MeasureAdjustment::isAlreadyThere
	@return true when the drawing already reads the wanted value
*/
bool MeasureAdjustment::isAlreadyThere() const
{
	return is_possible && MountingMeasure::isSameLength(measured, wanted);
}

/**
	@brief MountingMeasure::distance
	@param from the first point of the dimension
	@param to the second point
	@param axis which of the three readings is wanted
	@return the distance in millimetre, or not a number

	The three readings come out of one subtraction on purpose. Forcing a
	dimension horizontal is a change of what is read and never a change of
	what is read between, so the projections and the straight line are all
	built from the same two differences, and CU-22.2 - three values over one
	pair of points, coherent with each other - holds by construction rather
	than by being checked afterwards.
*/
qreal MountingMeasure::distance(const QPointF &from,
				const QPointF &to,
				MeasureAxis axis)
{
	if (!isFinitePoint(from) || !isFinitePoint(to)) {
		return notALength();
	}

	const qreal across = to.x() - from.x();
	const qreal down = to.y() - from.y();

	switch (axis)
	{
		case MeasureAxis::Horizontal:
			return qAbs(across);

		case MeasureAxis::Vertical:
			return qAbs(down);

		case MeasureAxis::Direct:
			break;
	}

	return std::hypot(across, down);
}

/**
	@brief MountingMeasure::isLength
	@param length a length in millimetre
	@return true when it is a number and not negative
*/
bool MountingMeasure::isLength(qreal length)
{
	return std::isfinite(length) && length >= 0.0;
}

/**
	@brief MountingMeasure::isSameLength
	@param first a length in millimetre
	@param second another one
	@return true when the two are the same length

	The slack is MountingArea::tolerance() and is taken from there rather
	than declared again, so that a dimension reading exactly the width of a
	surface and that surface holding a part flush against its edge can never
	be decided by two different notions of equal.
*/
bool MountingMeasure::isSameLength(qreal first, qreal second)
{
	if (!std::isfinite(first) || !std::isfinite(second)) {
		return false;
	}
	return qAbs(first - second) <= MountingArea::tolerance();
}

/**
	@brief MountingMeasure::coordinateOf
	@param point the point, in the frame the caller works in
	@param area_origin the top left corner of the mounting surface
	@return the coordinate on that surface, millimetre

	y is subtracted exactly like x, and that sentence is the whole function.
	The surface frame runs y downwards, the frame a caller draws in runs y
	downwards, so nothing has to be turned over and nothing is.
*/
QPointF MountingMeasure::coordinateOf(const QPointF &point,
				      const QPointF &area_origin)
{
	return QPointF(point.x() - area_origin.x(),
		       point.y() - area_origin.y());
}

/**
	@brief MountingMeasure::pointOf
	@param coordinate a coordinate on the mounting surface
	@param area_origin the top left corner of that surface
	@return the point in the frame the caller works in
*/
QPointF MountingMeasure::pointOf(const QPointF &coordinate,
				 const QPointF &area_origin)
{
	return QPointF(coordinate.x() + area_origin.x(),
		       coordinate.y() + area_origin.y());
}

/**
	@brief MountingMeasure::fitOfCoordinate
	@param coordinate a coordinate on the mounting surface
	@param area the surface
	@return Fits, OutsideArea, or NoArea

	The order is the one EnclosureTransfer::fitOf uses, and for the same
	reason: nothing can be said about being inside a surface that has no
	dimensions, so that answer comes first.
*/
MountingFit MountingMeasure::fitOfCoordinate(const QPointF &coordinate,
					     const MountingArea &area)
{
	if (!area.isValid()) {
		return MountingFit::NoArea;
	}

	const QRectF spot(coordinate, QSizeF(0.0, 0.0));
	return area.holds(spot) ? MountingFit::Fits : MountingFit::OutsideArea;
}

/**
	@brief MountingMeasure::adjustment
	@param from the point the dimension is measured from
	@param to the point it is measured to, and the one that moves
	@param wanted the value typed into the dimension, millimetre
	@param axis which reading was typed into
	@return the movement to hand to the undo command

	Built as a scaling of the difference between the two points and never as
	a new position, so that the answer keeps the direction the drawing
	already has. A dimension that grows pushes the part further along the
	way it already lies - further right when it lies right, further DOWN
	when it lies below - and the day somebody flips a frame somewhere else
	in the program, this function goes on being right, because it never
	names a direction of its own.
*/
MeasureAdjustment MountingMeasure::adjustment(const QPointF &from,
					      const QPointF &to,
					      qreal wanted,
					      MeasureAxis axis)
{
	MeasureAdjustment answer;
	answer.wanted = wanted;
	answer.measured = distance(from, to, axis);

	if (!isLength(answer.measured) || !isLength(wanted)) {
		return answer;
	}

	const qreal missing = wanted - answer.measured;

	if (answer.measured <= MountingArea::tolerance())
	{
			// Nothing between the two points along this axis, so
			// there is no way round to push. Asking for nothing is
			// the one request that survives it: it is already so.
		answer.is_possible = isSameLength(wanted, 0.0);
		return answer;
	}

	switch (axis)
	{
		case MeasureAxis::Direct:
		{
			const qreal factor = missing / answer.measured;
			answer.movement = QPointF((to.x() - from.x()) * factor,
						  (to.y() - from.y()) * factor);
			break;
		}

		case MeasureAxis::Horizontal:
		{
			const qreal way = (to.x() > from.x()) ? 1.0 : -1.0;
			answer.movement = QPointF(way * missing, 0.0);
			break;
		}

		case MeasureAxis::Vertical:
		{
			const qreal way = (to.y() > from.y()) ? 1.0 : -1.0;
			answer.movement = QPointF(0.0, way * missing);
			break;
		}
	}

	answer.is_possible = true;
	return answer;
}
