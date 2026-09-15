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
#ifndef DRILLINGORIGIN_H
#define DRILLINGORIGIN_H

#include "enclosuretransfer.h"

#include <QCoreApplication>
#include <QList>
#include <QPointF>
#include <QString>
#include <QtGlobal>

/**
	@brief The corner of the mounting surface the workshop measures from.

	Four, because a rectangle has four corners and a workshop measures from
	one of them. Which one is not a question this program can answer: it is
	how the bench works, how the tape is hooked over the edge, and how the
	machine that punches the plate was set up. Technical drawing habitually
	measures up from the bottom left; a plate laid face up on a bench is
	habitually measured down from the top left. Both are right where they
	are practised, and the one that is wrong is whichever one the table says
	and the bench does not do.

	@par The axes always grow into the plate

	The corner picks the directions and nothing else picks them: from the
	top left, x runs right and y runs down; from the bottom right, x runs
	left and y runs up. There is no corner from which an axis runs off the
	plate, because a coordinate on the plate would then be negative, and a
	table of negative numbers is a table nobody reads twice. So the rule is
	one sentence - every axis grows towards the inside of the surface - and
	it makes every hole on the surface a pair of positive numbers whichever
	corner was picked.

	@par What this is not

	It is not a change of model frame. Everything that computes - where an
	item sits, whether a hole fits, what a duct collides with - stays in the
	one frame MountingArea declares, the top left one, and this is applied
	at the moment a number is shown to a person. That was already the shape
	of the problem when the decision was deferred: swapping the corner is a
	display transformation, and buying it as a display transformation is
	what keeps the arithmetic of the rest of this module untouched.
*/
enum class DrillingCorner
{
	TopLeft,     ///< x to the right, y downwards - the default
	TopRight,    ///< x to the left, y downwards
	BottomLeft,  ///< x to the right, y upwards
	BottomRight  ///< x to the left, y upwards
};

/**
	@brief Where the zero of the drilling coordinates is, as a choice a
	person makes.

	A corner and a shift away from it. The corner is the coarse answer and
	covers the shop that hooks its tape over an edge; the shift is the fine
	one and covers the shop that measures from a datum - a fixing hole, a
	scribed line, the first hole of a row - which is a real practice and the
	reason the answer to this could not be a choice between two corners.

	@par The shift is measured along the axes this origin declares

	"35 mm in from the left edge and 40 mm down from the top", for a top
	left origin; "35 mm in from the right edge and 40 mm up from the
	bottom", for a bottom right one. Stated that way it is positive for
	every datum that is on the plate, whichever corner was picked, and the
	sentence that describes it reads the same for all four. Stated in the
	model frame instead, it would change sign with the corner, and a person
	who changed the corner would find the datum had jumped to the other side
	of the plate.

	@par It is a choice and not a frame

	Nothing here knows how big the plate is, and that is deliberate: the
	choice survives on a surface nobody has measured yet, and gets bound to
	the dimensions at the moment something is converted - by DrillingFrame,
	which is the only thing that holds both. Storing the dimensions in here
	would be a second copy of the two numbers MountingSurface already has,
	and two copies of a measurement is one of them wrong the first time
	somebody edits the other.
*/
class DrillingOrigin
{
	Q_DECLARE_TR_FUNCTIONS(DrillingOrigin)

	public:
		DrillingOrigin() {}
		explicit DrillingOrigin(DrillingCorner origin_corner,
					const QPointF &origin_offset = QPointF());

		/**
			@return true when this is the origin a project that
			never chose one is read as.

			Worth asking, and not only for tidiness: it is what lets
			the file stay unwritten for the default, so a project
			saved by somebody who never opened the setting comes back
			byte for byte and does not read as modified.
		*/
		bool isDefault() const;

			/// @return true when the zero is shifted off the corner
		bool hasOffset() const;

		/**
			@return true when the surface dimensions are needed to
			measure from here.

			False for the top left corner alone, and that is the one
			asymmetry of this class: the model frame already starts
			there, so measuring from it is a subtraction of the
			offset and needs no width and no height. Every other
			corner is the far end of a dimension, and the far end of
			a dimension nobody has measured cannot be found. Saying
			so is the point - the alternative is falling back to the
			top left corner without telling anyone, which produces
			a plate full of numbers that are all wrong the same way.
		*/
		bool needsSurfaceSize() const;

			/// @return true when x grows to the right of the plate
		bool xGrowsRight() const;
			/// @return true when y grows towards the bottom
		bool yGrowsDown() const;

		/**
			@brief What to call a corner out loud.
			@param corner the corner
			@return "coin supérieur gauche" and its three siblings

			Here and not in whoever shows it, so that the selector a
			person picks the corner in and the sentence printed at
			the head of the drilling table cannot call one corner by
			two names. A table that says one thing and a dialogue
			that says another is the same defect as two conventions
			for the origin, only harder to notice.
		*/
		static QString cornerName(DrillingCorner corner);

			/// @return every corner, in the order a selector lists them
		static QList<DrillingCorner> corners();

		/**
			@brief The corner as the .qet holds it.
			@param corner the corner
			@return "top-left" and its three siblings

			Not translated and not localised, for the reason
			MountingSurface::defaultKind is not: it is written to a
			file and compared on the way back, and a token that
			followed the interface language would make a project
			saved in French unreadable in Portuguese.
		*/
		static QString cornerToken(DrillingCorner corner);

		/**
			@brief The corner a token names.
			@param token the attribute as the file holds it
			@param ok set to false when the token names no corner
			@return the corner, the default one when the token names
			none

			Tolerant, like every read in this module: a file written
			by a later version, or by a hand, names a corner this one
			does not know, and the project has to open. @a ok is for
			the caller that wants to say so rather than for one that
			wants to refuse.
		*/
		static DrillingCorner cornerFromToken(const QString &token,
						      bool *ok = nullptr);

		bool operator==(const DrillingOrigin &other) const;
		bool operator!=(const DrillingOrigin &other) const;

			/// which corner of the surface the zero sits at
		DrillingCorner corner = DrillingCorner::TopLeft;
			/// how far the zero is from that corner, millimetre,
			/// measured along the axes this origin declares
		QPointF offset;
};

/**
	@brief A drilling origin bound to the surface it is measured on: the one
	place a coordinate is turned into the number a person reads.

	Two halves that come from two places - the choice, which is a property
	of the face and is stored with it, and the dimensions, which are the
	face's own - held together for as long as one conversion takes.
	MountingSurface::drillingFrame() is what builds it, and it is built and
	not stored so that the dimensions cannot go stale: a plate resized to
	600 x 800 changes every coordinate measured from its bottom edge, and a
	frame kept in a member would go on answering about the old plate.

	@par All of the origin arithmetic is in coordinateOf and positionOf

	Two functions, exact inverses, and nothing anywhere else subtracts an
	origin or flips an axis. That is not neatness: the failure this guards
	against is a coordinate that is right in the table and wrong in the
	drawing, or right on export and wrong on import, and every one of those
	comes from a second hand doing the same subtraction slightly
	differently. The way it is nailed is that the drilling table asks for
	its numbers here instead of printing the position it was handed.

	@par What it answers when it cannot answer

	Not a number, on both coordinates, when the corner needs the dimensions
	of the surface and the surface has none. It is the one honest answer:
	the number cannot be computed, and a table that printed the model
	position instead would be printing a measurement from a corner nobody
	chose while its own header said otherwise. The table already writes "?"
	for anything that is not a number, so the row survives, stays countable,
	and says on its face that it is unmeasured.
*/
class DrillingFrame
{
	public:
		DrillingFrame() {}
		DrillingFrame(const DrillingOrigin &frame_origin,
			      const MountingArea &frame_area);

		/**
			@return true when a coordinate can be worked out in this
			frame at all.

			True for the default origin on a surface nobody has
			measured, which is the normal state of a panel being laid
			out: the top left corner is where the model frame already
			starts, so nothing has to be known about the plate to
			measure from it.
		*/
		bool canMeasure() const;

		/**
			@brief What the workshop reads for a hole.
			@param position where the hole is, in the model frame -
			millimetre, x right, y down, origin at the top left
			corner of the surface
			@return the coordinate to print, in this frame

			The only place the chosen origin is ever applied.
		*/
		QPointF coordinateOf(const QPointF &position) const;

		/**
			@brief The way back from a coordinate somebody read.
			@param coordinate a coordinate in this frame
			@return the position in the model frame

			The exact inverse of coordinateOf, and here so that a
			number typed into a table and a number shown on a drawing
			cannot drift apart by each doing its own addition. Round
			tripping a position through the two gives the position
			back, every corner, offset or not.
		*/
		QPointF positionOf(const QPointF &coordinate) const;

		bool operator==(const DrillingFrame &other) const;
		bool operator!=(const DrillingFrame &other) const;

			/// where the zero is, as the face declares it
		DrillingOrigin origin;
			/// the face the coordinates are measured on
		MountingArea area;
};

#endif // DRILLINGORIGIN_H
