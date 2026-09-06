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
#ifndef MOUNTINGMEASURE_H
#define MOUNTINGMEASURE_H

#include "enclosuretransfer.h"

#include <QPointF>
#include <QtGlobal>

/**
	@brief Which of the three readings of one pair of points is meant.

	The same two corners answer three different questions, and a drawing
	asks all three: how far apart they are, how far apart they are across,
	and how far apart they are down. Making it one enumeration rather than
	three functions is what keeps the three answers about the same pair of
	points instead of about three pairs somebody typed separately - the
	whole point of forcing a projection is that the points do not move.
*/
enum class MeasureAxis
{
	Direct,     ///< straight from one point to the other
	Horizontal, ///< the across part of it alone, x
	Vertical    ///< the down part of it alone, y
};

/**
	@brief What to do to the drawing so a dimension reads the wanted value.

	A dimension here drives: typing a number into it moves a part until the
	distance is that number. So the answer this carries is a movement and
	not a position - a movement is what the undo command that performs it
	takes, and a movement is the one form of the answer that stays right
	when the caller has already moved something else.

	is_possible exists because a movement of nothing has two meanings that
	must not be confused. Asking a dimension that already reads 120 mm for
	120 mm moves nothing, and so does asking two points sitting on top of
	one another to be 120 mm apart - the second one is a request nothing can
	satisfy, because two coincident points name no direction to push along.
	Returning (0, 0) for both would let the second read as done.
*/
class MeasureAdjustment
{
	public:
			/**
				How far to move the part, millimetre, in the frame
				of MountingArea. Meaningless unless is_possible.
			*/
		QPointF movement;
			/// what the dimension reads before the edit, millimetre
		qreal measured = 0.0;
			/// what was asked of it, millimetre
		qreal wanted = 0.0;
			/// false when no movement makes the dimension read wanted
		bool is_possible = false;

		/**
			@return true when the drawing already reads the wanted
			value.

			Told apart from the refusal on purpose: this one is a
			request that has been satisfied, and the caller pushes
			no undo command for it rather than pushing an empty one.
		*/
		bool isAlreadyThere() const;
};

/**
	@brief The arithmetic behind a dimension and behind a drilling
	coordinate.

	Static and pure, the same family as EnclosureTransfer: no scene, no
	project, no graphics item. What can be got wrong here is the arithmetic
	and the frame it is done in, and neither of the two needs anything
	drawn to be proved.

	Everything is in the frame MountingArea declares - millimetre, x running
	right, y running DOWN, origin at the top left corner of the mounting
	surface. Written in capitals because it is the one thing in this file
	that a reader will assume wrongly: technical drawing habitually has y
	running up, and this tree already contains a place where the two frames
	meet and one is flipped into the other - Createdxf::drawCircle writes
	sheetHeight - y, because DXF is a y-up world. Nothing in here flips
	anything. A part below the origin has a positive y, a dimension asked
	to grow pushes that part further down, and any flipping belongs to the
	writer of whatever file wants the other convention, at its own edge.
*/
class MountingMeasure
{
	public:
		/**
			@brief What a dimension between two points reads.
			@param from the first point, in the frame above
			@param to the second point
			@param axis which of the three readings is wanted
			@return the distance, millimetre, never negative; not a
			number when a coordinate is not one

			Symmetric in its two points, all three ways: a dimension
			reads the same whichever end was clicked first. Which
			end MOVES is the other question, and it is answered by
			adjustment().
		*/
		static qreal distance(const QPointF &from,
				      const QPointF &to,
				      MeasureAxis axis = MeasureAxis::Direct);

		/**
			@param length a length in millimetre
			@return true when it is a number and not negative

			A distance of zero is a length: two points may sit on
			top of one another and the drawing has to be able to say
			so. A distance below zero is not, and neither is the
			answer to a distance between coordinates nobody typed.
		*/
		static bool isLength(qreal length);

		/**
			@param first a length in millimetre
			@param second another one
			@return true when the two are the same length

			Compared with MountingArea::tolerance() of slack, which
			is the slack the mounting rules already use, so that a
			dimension and a fit never disagree about whether two
			numbers are equal.
		*/
		static bool isSameLength(qreal first, qreal second);

		/**
			@brief Where a point lies on the mounting surface.
			@param point the point, in the frame the caller works in
			@param area_origin the top left corner of the mounting
			surface, in that same frame
			@return the coordinate to print in the drilling table

			A subtraction and nothing else, and it is a function so
			that the subtraction has one address. The y of the
			answer grows downwards, so a hole 30 mm below the top
			edge is at y = 30 and never at y = -30 nor at
			height - 30. That is the sign the whole drilling table
			hangs on: get it wrong and every number is wrong the
			same way, which is the one kind of error nobody spots by
			looking at one hole.
		*/
		static QPointF coordinateOf(const QPointF &point,
					    const QPointF &area_origin);

		/**
			@brief The way back from a printed coordinate.
			@param coordinate a coordinate on the mounting surface
			@param area_origin the top left corner of that surface
			@return the point in the frame the caller works in

			The exact inverse of coordinateOf, and here so that the
			table and the drawing cannot drift apart by each doing
			its own addition.
		*/
		static QPointF pointOf(const QPointF &coordinate,
				       const QPointF &area_origin);

		/**
			@brief Whether a coordinate is on the surface at all.
			@param coordinate a coordinate on the mounting surface
			@param area the surface
			@return Fits, OutsideArea, or NoArea

			Borrows the vocabulary of MountingFit rather than
			answering true or false, because "there is no surface"
			and "the surface does not reach there" send the reader
			to two different places. LargerThanArea is the one of
			the four that cannot come out of here: a coordinate has
			no size to be too large. A hole does, and the day a hole
			is asked about, it is asked with its diameter.
		*/
		static MountingFit fitOfCoordinate(const QPointF &coordinate,
						   const MountingArea &area);

		/**
			@brief What to move so the dimension reads @a wanted.
			@param from the point the dimension is measured from
			@param to the point it is measured to, and the point
			that moves
			@param wanted the value typed into the dimension,
			millimetre
			@param axis which reading was typed into
			@return the movement to hand to the undo command

			@a to moves and @a from stays. That is the second point
			clicked when the dimension was drawn, which is the
			convention the drawing tools people already have in
			their hands; the other end is had by calling with the
			two points the other way round, since the distance is
			symmetric and only the mover is not.

			Refused, rather than guessed, in three cases: a value
			that is not a length, a coordinate that is not a number,
			and a pair of points with nothing between them along the
			asked axis. The last one is the interesting one - two
			parts at the same height have no vertical distance, so
			there is no telling which way "make it 120" means, and
			answering anyway would move a part to a side the
			program chose on its own.
		*/
		static MeasureAdjustment adjustment(const QPointF &from,
						    const QPointF &to,
						    qreal wanted,
						    MeasureAxis axis = MeasureAxis::Direct);
};

#endif // MOUNTINGMEASURE_H
