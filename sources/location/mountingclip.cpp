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
#include "mountingclip.h"

#include <QRectF>
#include <QSizeF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	/**
		millimetre: how far off the centre line of a rail the body of a
		part may stop and still be caught by it. See
		MountingClip::grabMargin for why there is one at all and why it
		is small.
	*/
	const qreal GRAB_MARGIN = 5.0;

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

	/// @return where @a rect begins across @a run, millimetre
	qreal crossStart(const QRectF &rect, MountingRun run)
	{
		return run == MountingRun::Down ? rect.left() : rect.top();
	}

	/// @return where @a rect ends across @a run, millimetre
	qreal crossEnd(const QRectF &rect, MountingRun run)
	{
		return run == MountingRun::Down ? rect.right() : rect.bottom();
	}

	/// @return the coordinate of @a point across @a run, millimetre
	qreal crossOf(const QPointF &point, MountingRun run)
	{
		return run == MountingRun::Down ? point.x() : point.y();
	}

	/**
		@param item_uuid which one
		@param items everything mounted on the surface
		@return the first item of that identity, a default one when
		there is none

		The first and not the last, which is the same repair the drawing
		makes: a file written by hand can name the same identity twice,
		and every reader of it has to keep the same one.
	*/
	MountedItem itemOf(const QString &item_uuid,
			   const QList<MountedItem> &items)
	{
		if (item_uuid.isEmpty()) {
			return MountedItem();
		}

		for (const MountedItem &item : items)
		{
			if (item.uuid == item_uuid) {
				return item;
			}
		}

		return MountedItem();
	}
}

/**
	@brief MountingClip::isCarrier
	@param item anything mounted on a surface
	@return true when things can be clipped onto it
*/
bool MountingClip::isCarrier(const MountedItem &item)
{
	return item.isCutToLength() && item.profile.isRail();
}

/**
	@brief MountingClip::isClippable
	@param item anything mounted on a surface
	@return true when it can be clipped onto a rail
*/
bool MountingClip::isClippable(const MountedItem &item)
{
	return !item.isCutToLength();
}

/**
	@brief MountingClip::axisOf
	@param rail a piece cut to length
	@return the coordinate of its centre line, across its run, millimetre

	The middle of the rectangle the rail takes, and not its declared
	section: the two are the same number for every rail this program cuts,
	and where they are not - a file written by a hand that gave a size
	disagreeing with its bar - what a part is clipped onto is what is drawn,
	not what a token says it should have been.
*/
qreal MountingClip::axisOf(const MountedItem &rail)
{
	if (!isAPosition(rail.position)) {
		return std::numeric_limits<qreal>::quiet_NaN();
	}

	const QRectF rect = rail.footprint();

	return (crossStart(rect, rail.run) + crossEnd(rect, rail.run)) / 2.0;
}

/**
	@brief MountingClip::grabMargin
	@return how far off the line a body still catches, millimetre
*/
qreal MountingClip::grabMargin()
{
	return GRAB_MARGIN;
}

/**
	@brief MountingClip::axisOffsetOf
	@param part the part
	@param axes the axis offset of each product code, millimetre
	@return the offset, (0, 0) when the catalogue does not say

	An offset that is not a pair of numbers is read as no offset at all
	rather than carried through the arithmetic: a part clipped at
	not-a-number is a part nobody can find again, and the corner of the
	footprint is the answer this whole family already gives when a
	measurement is missing.
*/
QPointF MountingClip::axisOffsetOf(const MountedItem &part,
				   const QHash<QString, QPointF> &axes)
{
	if (part.part_code.isEmpty() || !axes.contains(part.part_code)) {
		return QPointF(0.0, 0.0);
	}

	const QPointF offset = axes.value(part.part_code);

	return isAPosition(offset) ? offset : QPointF(0.0, 0.0);
}

/**
	@brief MountingClip::holds
	@param rail the piece that might carry
	@param part the part that might be carried
	@return true when the two are in reach of one another
*/
bool MountingClip::holds(const MountedItem &rail, const MountedItem &part)
{
	if (!isCarrier(rail) || !isClippable(part)) {
		return false;
	}

		//Nothing carries what shares its name. It reads like a guard
		//against an impossible case - a piece cut to length cannot be
		//clippable, so nothing can be both - and it is not: the two
		//sides are two entries of one list, and a file written by hand
		//can name the same identity twice, once as a rail and once as
		//a part. Without this, a move would write two positions for
		//one identity and the second would win.
	if (!rail.uuid.isEmpty() && rail.uuid == part.uuid) {
		return false;
	}

	if (!isAPosition(rail.position) || !isAPosition(part.position)) {
		return false;
	}

	const MountingRun run = rail.run;
	const QRectF rail_rect = rail.footprint();
	const QRectF part_rect = part.footprint();
	const qreal tolerance = MountingArea::tolerance();

		//Along the rail: a part has to be somewhere over it. Touching
		//its very end counts, which is what the slack is for - a rail
		//cut at 600 and a breaker clipped at 600 are flush, and a
		//sum of millimetres in a double lands a nanometre away from
		//where the arithmetic says it does.
	if (runEnd(part_rect, run) < runStart(rail_rect, run) - tolerance) {
		return false;
	}
	if (runStart(part_rect, run) > runEnd(rail_rect, run) + tolerance) {
		return false;
	}

	const qreal axis = axisOf(rail);
	if (!std::isfinite(axis)) {
		return false;
	}

		//Across the rail: the body of the part, grown by the grab
		//distance, has to cover the centre line. The body and not the
		//axis of the part - see the header.
	const qreal reach = grabMargin() + tolerance;

	return axis >= crossStart(part_rect, run) - reach
	       && axis <= crossEnd(part_rect, run) + reach;
}

/**
	@brief MountingClip::carrierOf
	@param part the part
	@param items everything mounted on the same surface
	@param axes the axis offset of each product code
	@return the identity of the rail that carries it, empty when none does

	A rail with no identity carries nothing, and that is the same guard the
	drawing makes when a drag ends: a step that cannot name what it moves
	cannot undo it either, so a rail no command could address must not be
	able to take twelve breakers with it.
*/
QString MountingClip::carrierOf(const MountedItem &part,
				const QList<MountedItem> &items,
				const QHash<QString, QPointF> &axes)
{
	if (!isClippable(part)) {
		return QString();
	}

	const QPointF offset = axisOffsetOf(part, axes);
	const QPointF reference = part.position + offset;

	QString chosen;
	qreal best = 0.0;

	for (const MountedItem &candidate : items)
	{
		if (candidate.uuid.isEmpty() || !holds(candidate, part)) {
			continue;
		}

		const qreal axis = axisOf(candidate);
		const qreal distance =
				qAbs(crossOf(reference, candidate.run) - axis);

		if (!std::isfinite(distance)) {
			continue;
		}

		if (chosen.isEmpty() || distance < best)
		{
			chosen = candidate.uuid;
			best = distance;
		}
	}

	return chosen;
}

/**
	@brief MountingClip::carriedItems
	@param rail_uuid which rail
	@param items everything mounted on the same surface
	@param axes the axis offset of each product code
	@return what is clipped onto it, in the order it stands along the rail
*/
QList<MountedItem> MountingClip::carriedItems(const QString &rail_uuid,
					      const QList<MountedItem> &items,
					      const QHash<QString, QPointF> &axes)
{
	QList<MountedItem> carried;

	const MountedItem rail = itemOf(rail_uuid, items);
	if (rail_uuid.isEmpty() || !isCarrier(rail)) {
		return carried;
	}

	for (const MountedItem &item : items)
	{
		if (item.uuid.isEmpty()) {
			continue;
		}
		if (carrierOf(item, items, axes) == rail_uuid) {
			carried << item;
		}
	}

	const MountingRun run = rail.run;

	std::stable_sort(carried.begin(), carried.end(),
			 [run](const MountedItem &first,
			       const MountedItem &second)
			 {
				 return runStart(first.footprint(), run)
					< runStart(second.footprint(), run);
			 });

	return carried;
}

/**
	@brief MountingClip::carried
	@param rail_uuid which rail
	@param items everything mounted on the same surface
	@param axes the axis offset of each product code
	@return the identity of what it carries, in the order along the rail
*/
QStringList MountingClip::carried(const QString &rail_uuid,
				  const QList<MountedItem> &items,
				  const QHash<QString, QPointF> &axes)
{
	QStringList uuids;

	const QList<MountedItem> parts = carriedItems(rail_uuid, items, axes);
	uuids.reserve(parts.size());

	for (const MountedItem &part : parts) {
		uuids << part.uuid;
	}

	return uuids;
}

/**
	@brief MountingClip::clippedPosition
	@param part the part
	@param rail the rail
	@param axis_offset where the axis of the part sits inside its own body
	@return the top left corner it takes, millimetre
*/
QPointF MountingClip::clippedPosition(const MountedItem &part,
				      const MountedItem &rail,
				      const QPointF &axis_offset)
{
	if (!isCarrier(rail) || !isClippable(part)) {
		return part.position;
	}
	if (!isAPosition(part.position) || !isAPosition(axis_offset)) {
		return part.position;
	}

	const qreal axis = axisOf(rail);
	if (!std::isfinite(axis)) {
		return part.position;
	}

		//One coordinate moves and the other is left exactly as it was
		//dropped: where along the rail a part sits is the person's
		//decision, and this rule does not take it.
	if (rail.run == MountingRun::Down) {
		return QPointF(axis - axis_offset.x(), part.position.y());
	}

	return QPointF(part.position.x(), axis - axis_offset.y());
}

/**
	@brief MountingClip::clippedPosition
	@param part the part, where it was let go
	@param items everything mounted on the same surface
	@param axes the axis offset of each product code
	@return the top left corner it takes, its own when no rail catches it
*/
QPointF MountingClip::clippedPosition(const MountedItem &part,
				      const QList<MountedItem> &items,
				      const QHash<QString, QPointF> &axes)
{
	const QString carrier = carrierOf(part, items, axes);
	if (carrier.isEmpty()) {
		return part.position;
	}

	return clippedPosition(part, itemOf(carrier, items),
			       axisOffsetOf(part, axes));
}

/**
	@brief MountingClip::fillOf
	@param rail_uuid which rail
	@param items everything mounted on the same surface
	@param axes the axis offset of each product code
	@return how long it was cut, how much of it is taken, and what that
	answer rests on
*/
MountingRailFill MountingClip::fillOf(const QString &rail_uuid,
				      const QList<MountedItem> &items,
				      const QHash<QString, QPointF> &axes)
{
	const MountedItem rail = itemOf(rail_uuid, items);

	if (!isCarrier(rail)) {
		return MountingRailFill();
	}

	QList<MountedItem> parts = carriedItems(rail_uuid, items, axes);

	if (rail.run == MountingRun::Down)
	{
			//railFill adds up widths, which is the room a part
			//takes along a rail that runs across. Along a rail that
			//runs down it is the height, so the parts are handed
			//over in the frame of the rail - their two dimensions
			//swapped - and nothing outside this function ever sees
			//them that way. Swapped rather than added up a second
			//time here, so that the one place a rail is summed
			//stays the one place; a sum written again would also
			//have to remember to count the parts nobody measured,
			//and that is the half such a copy forgets.
		for (MountedItem &part : parts) {
			part.size = QSizeF(part.size.height(),
					   part.size.width());
		}
	}

		//The length of the piece and never the width of its rectangle:
		//cutLength() reads along the run, so a rail standing up is six
		//hundred millimetres of rail and not sixty.
	return MountingCheck::railFill(parts, rail.cutLength());
}
