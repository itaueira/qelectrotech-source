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
#ifndef MOUNTINGCLIP_H
#define MOUNTINGCLIP_H

#include "enclosuretransfer.h"
#include "mountingcheck.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
	@brief What is clipped onto which rail, and where a part lands when it
	is.

	The rule behind the one mistake of a panel drawing that is only ever
	found in the workshop: a rail dragged to another place with the breakers
	left behind at the old one. The plate is then drilled from a drawing
	where twelve parts sit where nothing is going to hold them, and nothing
	on the screen ever said so.

	Static and pure, the same family as MountingCheck, MountingMeasure and
	EnclosureTransfer: QRectF, QPointF and arithmetic, with no scene, no
	project and no catalogue. That is what lets "this breaker is on that
	rail" and "that rail carries these twelve" be settled on a bench -
	a geometry rule that can only be exercised by dragging something on a
	screen is a rule nobody checks twice.

	Everything is in the frame MountingArea declares: millimetre, x running
	right, y running down, origin at the top left corner of the mounting
	surface. No factor is applied to anything on the way in or on the way
	out.

	@par The link is read off the geometry, and is not a field in the file

	Nothing here is stored. Which rail carries a part is worked out from
	where the two of them are, every time it is asked, and that is a
	decision rather than a shortcut:

	- the file already says where everything is, and a rail is one of the
	  things it says that about, so a stored link would be a second truth
	  about a fact the file already holds;
	- a stored link can disagree with the drawing. A part dragged off its
	  rail and left in the middle of the plate would keep the link, and
	  would then follow a rail it is not on any more - which is the same
	  class of mistake as the one this file exists against, arrived at from
	  the other side;
	- a project written before this rule existed carries no link, and would
	  have to be told what is on what. Read off the geometry, it needs
	  nothing: an old plate answers the same as a new one.

	The price is real and is paid on purpose. A part that merely covers a
	rail without being clipped onto it - a large controller screwed to the
	plate over a rail passing behind it - is carried when that rail moves,
	and no drawing can tell the two apart from geometry alone. The day the
	workshop reports it, the way out is a field saying "this one is screwed
	to the plate", not a second rule guessing better.

	The relation step 7 of this task needs - an auxiliary block that belongs
	to its contactor - is deliberately NOT this one. That one cannot be read
	off geometry at all, since an accessory bolted to the side of a
	contactor looks exactly like a part standing beside it, and it is a
	field. Two different relations, and they do not share a mechanism.

	@par What carries, and what is carried

	Only a piece the file calls a rail carries anything. A cable duct does
	not: what goes into a duct is wire, and a duct dragged along the plate
	carrying the breakers that happened to sit over it would be a surprise
	nobody asked for. A piece cut to length is never carried either - rails
	and ducts are screwed to the plate, not clipped onto one another - and
	that guard is what keeps a duct crossing a rail from being dragged away
	by it.
*/
class MountingClip
{
	public:
		/**
			@param item anything mounted on a surface
			@return true when things can be clipped onto it

			A rail, and nothing else. A piece whose kind the file
			spells in a way this version does not know carries
			nothing, which is the tolerant reading working as
			designed: an unknown token is written back untouched and
			behaves as no answer, never as a wrong one.
		*/
		static bool isCarrier(const MountedItem &item);

		/**
			@param item anything mounted on a surface
			@return true when it can be clipped onto a rail

			Everything bought as a piece can be, an unmeasured part
			included: a breaker nobody has typed the width of is
			still a breaker somebody clipped on, and dropping it out
			of what a rail carries would be this rule deciding that
			an unmeasured part may be left behind. It is carried as
			the point its footprint is.
		*/
		static bool isClippable(const MountedItem &item);

		/**
			@param rail a piece cut to length
			@return the coordinate of its centre line, across its
			run, millimetre

			One number and not a line, because the other axis is the
			rail itself: a rail running across is held at a y, and
			where along it a part sits is the part's business. Not a
			number when the rail has no position.
		*/
		static qreal axisOf(const MountedItem &rail);

		/**
			@return how far off the line a body still catches,
			millimetre.

			A drop is a gesture of a hand, and a hand misses by a few
			millimetres; without this, a breaker let go two
			millimetres short of covering the rail would stay where
			it was dropped and be left behind by the next drag of
			that rail. It is a grab distance and nothing is ever
			stored at it - no file holds this number, and changing it
			changes what catches, never what was written.

			Small on purpose. Rails on a plate are never closer to
			one another than the parts clipped on them are tall, so a
			few millimetres of forgiveness cannot reach the next
			rail; a generous one could, and a breaker that clips onto
			the rail below the one it was dropped on is worse than a
			breaker that clips onto none.
		*/
		static qreal grabMargin();

		/**
			@brief Where the axis of a part sits inside its own body.
			@param part the part
			@param axes the axis offset of each product code, in
			millimetre from the top left corner of the body
			@return the offset, (0, 0) when the catalogue does not
			say

			(0, 0) is the top left corner, which is what a caller
			aligning by the edge of the footprint uses - and it is
			the documented answer of MountingPartView::insertionOffset
			for a part whose axis nobody has filled in. The
			consequence is visible and is meant to be: a row of parts
			whose axis is unknown lines up by its corners, hanging
			off the rail, which says on the drawing that the
			catalogue has not been told where those parts are held.
		*/
		static QPointF axisOffsetOf(const MountedItem &part,
					    const QHash<QString, QPointF> &axes);

		/**
			@brief Whether this rail is under this part at all.
			@param rail the piece that might carry
			@param part the part that might be carried
			@return true when the two are in reach of one another

			Two questions, one per axis, and they are not the same
			question. Along the rail, the part has to be somewhere
			over it: a breaker at the far end of the plate is not on
			a rail that stops halfway. Across the rail, the body of
			the part - grown by grabMargin() - has to cover the
			centre line of the rail, which is the plain reading of
			"it is sitting on it".

			The body and not the axis of the part, for the drop: what
			a person sees when they let go is a rectangle over a
			rail, and a rule asking instead where the axis of the
			part is would refuse a part that visibly covers the rail
			whenever the catalogue has not been told where its axis
			is. Which of several rails in reach is the one is the
			other question, and carrierOf answers it.

			Not a symmetric relation: a rail is never carried by
			anything, so holds(a, b) and holds(b, a) are two
			different questions with two different answers.
		*/
		static bool holds(const MountedItem &rail,
				  const MountedItem &part);

		/**
			@brief Which rail carries this part.
			@param part the part
			@param items everything mounted on the same surface
			@param axes the axis offset of each product code
			@return the identity of the rail, empty when none
			carries it

			One rail and never two, and that is the whole point of
			this function existing beside holds(). A part can cover
			two rails that stand close together, and a rule that let
			it be carried by both would move it twice when one of
			them is dragged - or, worse, would drag it away with the
			rail it was not clipped onto.

			The one chosen is the rail whose centre line is nearest
			the axis of the part, ties going to the one handed over
			first. Nearest the axis and not nearest the body, so that
			the rail a part was just clipped onto always wins it
			back: after a clip that axis is ON the line, at a
			distance of nothing, and no other rail can be nearer than
			that.
		*/
		static QString carrierOf(const MountedItem &part,
					 const QList<MountedItem> &items,
					 const QHash<QString, QPointF> &axes
						 = QHash<QString, QPointF>());

		/**
			@brief What one rail carries.
			@param rail_uuid which rail
			@param items everything mounted on the same surface
			@param axes the axis offset of each product code
			@return what is clipped onto it, in the order it stands
			along the rail

			In the order along the rail, and not in the order of the
			list: left to right for a rail running across, top to
			bottom for one running down. That is the order a person
			reads a rail in and the order a terminal strip is
			numbered in, and it costs a sort to have it here rather
			than in each caller. Two parts at the same coordinate
			keep the order they were handed over in, so the answer
			never depends on the sort being stable.
		*/
		static QList<MountedItem>
		carriedItems(const QString &rail_uuid,
			     const QList<MountedItem> &items,
			     const QHash<QString, QPointF> &axes
				     = QHash<QString, QPointF>());

			/// @return the identity of what the rail carries, in the same order
		static QStringList carried(const QString &rail_uuid,
					   const QList<MountedItem> &items,
					   const QHash<QString, QPointF> &axes
						   = QHash<QString, QPointF>());

		/**
			@brief Where a part lands when it is clipped onto a rail.
			@param part the part
			@param rail the rail
			@param axis_offset where the axis of the part sits inside
			its own body, millimetre
			@return the top left corner it takes, millimetre

			One coordinate changes and the other does not. Where
			along the rail a part sits is what the person dropping it
			decided, and a rule that moved it along as well would be
			answering a question nobody asked; what is decided here
			is only that it is held by the rail, which is the other
			coordinate.

			The part unchanged when no clip is possible - when the
			rail is not one, when the part is a cut piece, when
			either of them has no position. Tolerant rather than
			refusing, because this is read by a gesture: a drop that
			catches nothing has to leave the part exactly where the
			hand let it go.
		*/
		static QPointF clippedPosition(const MountedItem &part,
					       const MountedItem &rail,
					       const QPointF &axis_offset
						       = QPointF(0.0, 0.0));

		/**
			@brief Where a part lands when it is dropped among these.
			@param part the part, where it was let go
			@param items everything mounted on the same surface
			@param axes the axis offset of each product code
			@return the top left corner it takes, millimetre, its own
			when no rail catches it
		*/
		static QPointF clippedPosition(const MountedItem &part,
					       const QList<MountedItem> &items,
					       const QHash<QString, QPointF> &axes
						       = QHash<QString, QPointF>());

		/**
			@brief How full one rail is.
			@param rail_uuid which rail
			@param items everything mounted on the same surface
			@param axes the axis offset of each product code
			@return how long it was cut, how much of it is taken, and
			what that answer rests on

			The arithmetic is not written here: it is
			MountingCheck::railFill, which already adds widths and
			already counts the parts it could not add because nobody
			measured them. What this function contributes is the
			list - which parts are on that rail - and that is exactly
			the input railFill never had a caller for. Two places
			adding up a rail would be one place too many.

			A rail running down is handed over in its own frame, its
			parts with their two dimensions swapped, because railFill
			adds widths and the room a part takes along such a rail
			is its height. The swap lives in that one function and
			nothing outside it sees an item that way.
		*/
		static MountingRailFill fillOf(const QString &rail_uuid,
					       const QList<MountedItem> &items,
					       const QHash<QString, QPointF> &axes
						       = QHash<QString, QPointF>());
};

#endif // MOUNTINGCLIP_H
