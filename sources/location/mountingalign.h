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
#ifndef MOUNTINGALIGN_H
#define MOUNTINGALIGN_H

#include "enclosuretransfer.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
	@brief Which line a row of parts is brought onto.

	Named after the line they end up sharing, and not after a direction,
	because a direction is the thing everybody reads backwards: "align
	horizontally" is either a row put on one horizontal line or a row lined
	up by its horizontal coordinate, depending on who is speaking. LeftEdges
	says which edge of which part ends up where, and nobody has to guess.
*/
enum class MountingAlignment
{
	LeftEdges,      ///< every left edge on the leftmost one
	RightEdges,     ///< every right edge on the rightmost one
	TopEdges,       ///< every top edge on the topmost one
	BottomEdges,    ///< every bottom edge on the lowest one
	VerticalAxes,   ///< every part centred on one vertical line
	HorizontalAxes  ///< every part centred on one horizontal line
};

/**
	@brief Lining a selection up and spreading it evenly, in millimetre.

	The two gestures that turn a plate laid out by hand into a plate that
	looks laid out: bring a column of contactors onto one edge, and leave
	the same air between six breakers that were dropped by eye. Both are
	done today by pushing parts one at a time, and the second one cannot be
	done that way at all - equal gaps between bodies of different widths is
	a division, not an eye.

	Static and pure, the same family as MountingCheck, MountingClip and
	MountingMeasure: rectangles and arithmetic, with no scene, no project
	and no catalogue. Everything is in the frame MountingArea declares -
	millimetre, x right, y down, origin at the top left corner of the
	mounting surface - and nothing is multiplied by anything on the way in
	or out.

	@par What comes back is what MOVES, and nothing else

	Both answers are a table of new positions keyed by identity, holding an
	entry only for a part that ends up somewhere else. A part already on the
	line is not in the answer, which is what lets one undo step be built
	from it and be honestly empty when there was nothing to do: a step that
	moves six parts by nothing is a step the next undo spends on doing
	nothing visible.

	@par Three kinds of part are left out, and each for its own reason

	A piece cut to length - a rail, a duct - is never moved by either
	gesture. It is the thing other parts are clipped onto, so moving it is
	the other gesture entirely, the one that carries its parts along with it
	(MountingClip and the move of a rail). An alignment that shifted a rail
	sideways while leaving the breakers on it would undo the whole point of
	clipping them.

	A part with no identity is left out because nothing could name it
	afterwards: an undo step that cannot say what it moved cannot put it
	back.

	A part whose position is not a pair of numbers is left out because there
	is nothing to align it from. It stays exactly as unreadable as it was,
	and the report of the plate is where that gets said.

	@par A part nobody measured is aligned as the point it is

	Its footprint is a point, so its left edge and its right edge are the
	same place. Aligning it left and aligning it right put it at two
	different coordinates, both of them correct for a part of no width. It
	is never given an invented size to make the arithmetic prettier - that
	is the one thing this whole family refuses to do.
*/
class MountingAlign
{
	public:
		/**
			@brief Where each part goes to share one line.
			@param items the parts to line up
			@param alignment which line they end up sharing
			@return the new position of each part that moves,
			millimetre, keyed by identity

			The line is taken from the selection and never from the
			plate: aligning left means the leftmost part of what was
			chosen, so the gesture moves the others to it and leaves
			that one alone. A line taken from the plate would move
			everything at once and surprise everybody.

			The two centre alignments take the middle of the box
			around the whole selection, not the average of the
			centres. With parts of different widths the two are
			different numbers, and the box is the one a person can
			see: it is halfway between the two ends they are looking
			at.

			Empty when fewer than two parts can be moved, since a
			part is already aligned with itself.
		*/
		static QHash<QString, QPointF>
		aligned(const QList<MountedItem> &items,
			MountingAlignment alignment);

		/**
			@brief Where each part goes to leave equal air between
			them.
			@param items the parts to spread
			@param run along which axis they are spread
			@return the new position of each part that moves,
			millimetre, keyed by identity

			Equal GAPS between bodies, and not equal spacing of
			centres. The two are the same number for a row of
			identical breakers and are not for anything else: a
			contactor of 45 mm between two breakers of 22,5 mm
			centred evenly leaves more air on one side than on the
			other, and it is the air a person sees and a wire needs.

			The two outermost parts do not move. They are what the
			person already decided - where the row starts and where
			it ends - and a gesture that moved them would be
			answering a question nobody asked.

			Empty when fewer than three parts can be moved: with two
			of them, both are outermost and there is nothing in
			between to spread.

			The gap it uses can be negative, when the parts do not
			fit in the room they already occupy. This function
			spreads them anyway, evenly overlapping; whoever offers
			the gesture to a person asks spreadGap() first and says
			so instead, because a tidy uniform overlap is the one
			answer that looks deliberate and is wrong.
		*/
		static QHash<QString, QPointF>
		spread(const QList<MountedItem> &items, MountingRun run);

		/**
			@brief How much air spread() would leave between two
			parts.
			@param items the parts to spread
			@param run along which axis they are spread
			@return the gap, millimetre; not a number when fewer than
			three parts can be spread

			Negative when the bodies add up to more than the room
			they stand in. That is a state to report and not to
			repair here: the arithmetic has an answer, and whether a
			person should be allowed to apply it is a sentence in a
			window, not a rule about rectangles.
		*/
		static qreal spreadGap(const QList<MountedItem> &items,
				       MountingRun run);

		/**
			@brief The coordinate the selection is lined up onto.
			@param items the parts to line up
			@param alignment which line they end up sharing
			@return the coordinate, millimetre; not a number when
			nothing can be lined up

			Published so that whoever draws the gesture can show the
			line before it is applied, and so that the number a test
			asserts is the number the drawing uses.
		*/
		static qreal referenceOf(const QList<MountedItem> &items,
					 MountingAlignment alignment);

		/**
			@brief Which of these parts either gesture will move.
			@param items the parts handed over
			@return their identities, in the order handed over

			The three exclusions of this class, said once and read by
			both gestures - and readable from outside, so that a
			window can say "two of the five are rails and stay where
			they are" instead of quietly moving three.
		*/
		static QStringList movableUuids(const QList<MountedItem> &items);

			/// @return true when this part can be lined up or spread
		static bool isMovable(const MountedItem &item);
};

#endif // MOUNTINGALIGN_H
