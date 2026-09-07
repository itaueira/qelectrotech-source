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
#include "mountingcheck.h"

#include <QPointF>

#include <cmath>

namespace
{
	/**
		@brief isUsableLength
		@param length a length in millimetre
		@return true when it is a number and there is something of it

		The same question MountingArea and MountingLayout ask, asked the
		same way on purpose: a dimension nobody filled in arrives as
		zero, and three files that disagree about whether zero is a
		length would report three different panels.
	*/
	bool isUsableLength(qreal length)
	{
		return std::isfinite(length)
		       && length > MountingArea::tolerance();
	}

	/**
		@brief isRealRectangle
		@param box a rectangle in millimetre
		@return true when it is somewhere and has some room to it

		A part nobody has measured is a point, and a part whose position
		is not a number is nowhere. Neither can share room with
		anything, and neither is refused: they come back in the fit list
		and in the counts, which is where the report says its answer
		rests on a measurement nobody took.
	*/
	bool isRealRectangle(const QRectF &box)
	{
		return std::isfinite(box.left())
		       && std::isfinite(box.top())
		       && std::isfinite(box.right())
		       && std::isfinite(box.bottom())
		       && isUsableLength(box.width())
		       && isUsableLength(box.height());
	}

	/**
		@brief sharedRoom
		@param first a rectangle in millimetre
		@param second another one
		@return the room they both claim, a null rectangle when they
		claim none

		Worked out here rather than by QRectF::intersects, so that the
		slack is the slack of this family and not the exact arithmetic
		of a double. Both dimensions of the overlap have to be more than
		MountingArea::tolerance() for it to count, which is what makes
		two breakers clipped at 0 and at 22.5 mm neighbours instead of a
		complaint - and flush is the normal case on a rail, not the
		exception.
	*/
	QRectF sharedRoom(const QRectF &first, const QRectF &second)
	{
		const qreal left = qMax(first.left(), second.left());
		const qreal right = qMin(first.right(), second.right());
		const qreal top = qMax(first.top(), second.top());
		const qreal bottom = qMin(first.bottom(), second.bottom());

		const qreal slack = MountingArea::tolerance();
		if (right - left <= slack || bottom - top <= slack) {
			return QRectF();
		}
		return QRectF(QPointF(left, top), QPointF(right, bottom));
	}

	/**
		@brief shorterWayOut
		@param claimed the room that is claimed
		@param body the rectangle standing in it
		@return how far the body has to be pushed to be out of it,
		along the shortest of the four ways, millimetre

		Four candidates and the smallest of them, because that is the
		question a person asks of a drawing: how far do I drag this.
		Pushing it right until its left edge clears the right edge of
		the claim, or left, or down, or up - and the smallest of those
		four is the way out.

		Written this way after the smaller side of the shared rectangle
		was tried and found wrong. The two agree while the two
		rectangles merely cross, and they part company in the case that
		matters most: a duct standing 30 mm from a drive that asked for
		100 mm of air sits entirely inside the claimed room along x, so
		the shared rectangle is 60 mm wide - the whole width of the duct
		- while the answer the person needs is 70, the distance from the
		duct to the far edge of the claim. Reporting 60 there would
		understate the move and the duct would still be short of air
		after being dragged exactly as far as the panel said.
	*/
	qreal shorterWayOut(const QRectF &claimed, const QRectF &body)
	{
		if (!isRealRectangle(claimed) || !isRealRectangle(body)) {
			return 0.0;
		}

		const QRectF room = claimed.normalized();
		const QRectF box = body.normalized();

		qreal out = room.right() - box.left();
		out = qMin(out, box.right() - room.left());
		out = qMin(out, room.bottom() - box.top());
		out = qMin(out, box.bottom() - room.top());
		return qMax(out, 0.0);
	}

	/**
		@brief clearanceOf
		@param item one mounted part
		@param clearances what each product code asks for
		@return what this part asks for, nothing when nobody said

		Keyed by product code, so a part with no code asks for nothing
		even when the caller handed over a table full of numbers. That
		is the same rule the size follows - the air a contactor needs
		belongs to the contactor, not to the seventh one dropped onto
		the plate - and it means a part somebody drew before choosing
		the product is checked against no clearance and counted as
		such, instead of borrowing a neighbour's requirement.
	*/
	MountingClearance clearanceOf(const MountedItem &item,
				      const QHash<QString, MountingClearance> &clearances)
	{
		if (item.part_code.isEmpty()) {
			return MountingClearance();
		}
		return clearances.value(item.part_code);
	}
}

/**
	@brief MountingClearance::MountingClearance
	@param clearance_top air wanted above it, millimetre
	@param clearance_bottom air wanted below it
	@param clearance_left air wanted to its left
	@param clearance_right air wanted to its right
*/
MountingClearance::MountingClearance(qreal clearance_top,
				     qreal clearance_bottom,
				     qreal clearance_left,
				     qreal clearance_right) :
	top(clearance_top),
	bottom(clearance_bottom),
	left(clearance_left),
	right(clearance_right)
{
}

/**
	@brief MountingClearance::uniform
	@param all_sides the air wanted, millimetre
	@return a clearance asking for it on each of the four sides
*/
MountingClearance MountingClearance::uniform(qreal all_sides)
{
	return MountingClearance(all_sides, all_sides, all_sides, all_sides);
}

/**
	@brief MountingClearance::isEmpty
	@return true when no side asks for anything
*/
bool MountingClearance::isEmpty() const
{
	return asked(top) <= 0.0
	       && asked(bottom) <= 0.0
	       && asked(left) <= 0.0
	       && asked(right) <= 0.0;
}

/**
	@brief MountingClearance::asked
	@param side a clearance in millimetre
	@return the number asked for, zero when it is not a length

	Folded and not refused, and the negative is folded along with the
	not-a-number: a clearance of minus ten would shrink the part it belongs
	to and hide an overlap the person has to see.
*/
qreal MountingClearance::asked(qreal side)
{
	return isUsableLength(side) ? side : 0.0;
}

/**
	@brief MountingClearance::grown
	@param footprint the room the body takes, millimetre
	@return the body grown by what each side asks for
*/
QRectF MountingClearance::grown(const QRectF &footprint) const
{
	if (isEmpty() || !isRealRectangle(footprint)) {
		return footprint;
	}

	return footprint.normalized().adjusted(-asked(left),
					       -asked(top),
					       asked(right),
					       asked(bottom));
}

/**
	@brief MountingClearance::operator==
	@param other another clearance
	@return true when the two ask for the same air

	Compared through asked(), so that a side left at minus one and a side
	left at zero are the same requirement - which they are, since both ask
	for nothing.
*/
bool MountingClearance::operator==(const MountingClearance &other) const
{
	const qreal slack = MountingArea::tolerance();
	return qAbs(asked(top) - asked(other.top)) <= slack
	       && qAbs(asked(bottom) - asked(other.bottom)) <= slack
	       && qAbs(asked(left) - asked(other.left)) <= slack
	       && qAbs(asked(right) - asked(other.right)) <= slack;
}

/**
	@brief MountingClearance::operator!=
	@param other another clearance
	@return true when the two do not ask for the same air
*/
bool MountingClearance::operator!=(const MountingClearance &other) const
{
	return !(*this == other);
}

/**
	@brief MountingRailFill::isMeasured
	@return true when the rail has a length at all
*/
bool MountingRailFill::isMeasured() const
{
	return isUsableLength(length);
}

/**
	@brief MountingRailFill::free
	@return what is left of the rail, millimetre

	Zero on a rail nobody has cut yet, and not a negative number: a rail
	with no length has no room to be short of, and answering minus the sum
	of what is on it would read as an overfilled rail.
*/
qreal MountingRailFill::free() const
{
	if (!isMeasured()) {
		return 0.0;
	}
	return length - used;
}

/**
	@brief MountingRailFill::overrun
	@return by how much the rail is over, zero when it is not
*/
qreal MountingRailFill::overrun() const
{
	if (!isOverfilled()) {
		return 0.0;
	}
	return used - length;
}

/**
	@brief MountingRailFill::isOverfilled
	@return true when more is clipped on than the rail is long

	False on an uncut rail, for the reason free() answers zero: nobody has
	said how long it is, so nothing about it can be too long. The rail
	being unmeasured is its own question, and isMeasured() is where it is
	asked.
*/
bool MountingRailFill::isOverfilled() const
{
	return isMeasured() && used > length + MountingArea::tolerance();
}

/**
	@brief MountingRailFill::isConclusive
	@return true when every part on the rail has a width
*/
bool MountingRailFill::isConclusive() const
{
	return unknown_width_count == 0;
}

/**
	@brief MountingRailFill::ratio
	@return how much of the rail is taken, 0 to 1
*/
qreal MountingRailFill::ratio() const
{
	if (!isMeasured()) {
		return 0.0;
	}
	return used / length;
}

/**
	@brief MountingItemFit::isFitting
	@return true when the part is where it can be
*/
bool MountingItemFit::isFitting() const
{
	return fit == MountingFit::Fits;
}

/**
	@brief MountingSurfaceReport::itemCount
	@return how many parts were looked at
*/
int MountingSurfaceReport::itemCount() const
{
	return fits.count();
}

/**
	@brief MountingSurfaceReport::misfitCount
	@return how many parts are not where they can be
*/
int MountingSurfaceReport::misfitCount() const
{
	int total = 0;
	for (const MountingItemFit &entry : fits) {
		if (!entry.isFitting()) {
			++total;
		}
	}
	return total;
}

/**
	@brief MountingSurfaceReport::issueCount
	@return every complaint added up
*/
int MountingSurfaceReport::issueCount() const
{
	return overlaps.count() + encroachments.count() + misfitCount();
}

/**
	@brief MountingSurfaceReport::isClean
	@return true when nothing was found against this face
*/
bool MountingSurfaceReport::isClean() const
{
	return issueCount() == 0;
}

/**
	@brief MountingSurfaceReport::isConclusive
	@return true when no part of the answer rests on a measurement nobody
	took
*/
bool MountingSurfaceReport::isConclusive() const
{
	return unknown_size_count == 0
	       && unknown_clearance_count == 0
	       && area.isValid();
}

/**
	@brief MountingSurfaceReport::misfits
	@return the parts that are not where they can be, in order
*/
QList<MountingItemFit> MountingSurfaceReport::misfits() const
{
	QList<MountingItemFit> result;
	for (const MountingItemFit &entry : fits) {
		if (!entry.isFitting()) {
			result.append(entry);
		}
	}
	return result;
}

/**
	@brief MountingSurfaceReport::complainedAbout
	@return the identifier of every part named in a complaint

	Each part once, in the order the complaints were built, so that a
	drawing can hatch what the report objected to without hatching the same
	part three times over.
*/
QStringList MountingSurfaceReport::complainedAbout() const
{
	QStringList result;
	const auto remember = [&result](const QString &uuid)
	{
		if (!uuid.isEmpty() && !result.contains(uuid)) {
			result.append(uuid);
		}
	};

	for (const MountingOverlap &overlap : overlaps) {
		remember(overlap.first_uuid);
		remember(overlap.second_uuid);
	}
	for (const MountingEncroachment &hit : encroachments) {
		remember(hit.claimant_uuid);
		remember(hit.intruder_uuid);
	}
	for (const MountingItemFit &entry : fits) {
		if (!entry.isFitting()) {
			remember(entry.uuid);
		}
	}
	return result;
}

/**
	@brief MountingCheck::overlaps
	@param items everything mounted on one surface
	@return one entry per colliding pair, in the order the items were
	handed over
*/
QList<MountingOverlap> MountingCheck::overlaps(const QList<MountedItem> &items)
{
	QList<MountingOverlap> result;
	const int total = items.count();

	for (int first = 0; first < total; ++first) {
		const QRectF first_box = items.at(first).footprint();
		if (!isRealRectangle(first_box)) {
			continue;
		}

		for (int second = first + 1; second < total; ++second) {
			const QRectF second_box = items.at(second).footprint();
			if (!isRealRectangle(second_box)) {
				continue;
			}

			const QRectF shared = sharedRoom(first_box, second_box);
			if (shared.isNull()) {
				continue;
			}

			MountingOverlap overlap;
			overlap.first_uuid = items.at(first).uuid;
			overlap.second_uuid = items.at(second).uuid;
			overlap.shared = shared;
			overlap.way_out = shorterWayOut(first_box, second_box);
			result.append(overlap);
		}
	}
	return result;
}

/**
	@brief MountingCheck::encroachments
	@param items everything mounted on one surface
	@param clearances what each product code asks for, keyed by code
	@return one entry per starved claim, claimant first

	Every part is asked what it wants and then every other part is asked
	whether it is standing in it, which is n squared and stays so on
	purpose: a mounting plate holds tens of parts, not thousands, and a
	rule wrapped in an index is a rule whose answer nobody can check by
	hand.

	A claimant nobody has measured claims nothing. The air is asked for
	around a body, and a part with no dimensions has no body yet - growing
	its position by 100 mm would draw a requirement around a point somebody
	has not finished describing. It is counted in the report instead.
*/
QList<MountingEncroachment>
MountingCheck::encroachments(const QList<MountedItem> &items,
			     const QHash<QString, MountingClearance> &clearances)
{
	QList<MountingEncroachment> result;
	const int total = items.count();

	for (int claimant = 0; claimant < total; ++claimant) {
		const MountingClearance clearance =
			clearanceOf(items.at(claimant), clearances);
		if (clearance.isEmpty()) {
			continue;
		}

		const QRectF body = items.at(claimant).footprint();
		if (!isRealRectangle(body)) {
			continue;
		}
		const QRectF claimed = clearance.grown(body);

		for (int intruder = 0; intruder < total; ++intruder) {
			if (intruder == claimant) {
				continue;
			}

			const QRectF intruder_body = items.at(intruder).footprint();
			if (!isRealRectangle(intruder_body)) {
				continue;
			}

			const QRectF shared = sharedRoom(claimed, intruder_body);
			if (shared.isNull()) {
				continue;
			}
				// metal on metal is the other question, and it
				// is answered by overlaps(); saying both about
				// one pair would count one mistake twice
			if (!sharedRoom(body, intruder_body).isNull()) {
				continue;
			}

			MountingEncroachment hit;
			hit.claimant_uuid = items.at(claimant).uuid;
			hit.intruder_uuid = items.at(intruder).uuid;
			hit.shared = shared;
			hit.missing = shorterWayOut(claimed, intruder_body);
			result.append(hit);
		}
	}
	return result;
}

/**
	@brief MountingCheck::railFill
	@param items the parts clipped onto that rail
	@param rail_length how long the rail was cut, millimetre
	@return the sum, the room left, and what the answer rests on
*/
MountingRailFill MountingCheck::railFill(const QList<MountedItem> &items,
					 qreal rail_length)
{
	MountingRailFill fill;
	fill.length = rail_length;
	fill.item_count = items.count();

	for (const MountedItem &item : items) {
		const qreal width = item.declaredSize().width();
		if (!isUsableLength(width)) {
			++fill.unknown_width_count;
			continue;
		}
		fill.used += width;
	}
	return fill;
}

/**
	@brief MountingCheck::fits
	@param items everything mounted on one surface
	@param area the room that surface has
	@return one entry per part, in the order they were handed over
*/
QList<MountingItemFit> MountingCheck::fits(const QList<MountedItem> &items,
					   const MountingArea &area)
{
	QList<MountingItemFit> result;

	for (const MountedItem &item : items) {
		MountingItemFit entry;
		entry.uuid = item.uuid;
		entry.fit = EnclosureTransfer::fitOf(item, area);
		entry.size_unknown = !item.hasDeclaredSize();
		result.append(entry);
	}
	return result;
}

/**
	@brief MountingCheck::surfaceReport
	@param surface the face and everything screwed to it
	@param clearances what each product code asks for, keyed by code
	@return the whole report, empty complaints included
*/
MountingSurfaceReport
MountingCheck::surfaceReport(const MountingSurface &surface,
			     const QHash<QString, MountingClearance> &clearances)
{
	MountingSurfaceReport report;
	report.surface_uuid = surface.uuid;
	report.area = surface.area;
	report.overlaps = overlaps(surface.items);
	report.encroachments = encroachments(surface.items, clearances);
	report.fits = fits(surface.items, surface.area);

	for (const MountedItem &item : surface.items) {
		if (!item.hasDeclaredSize()) {
			++report.unknown_size_count;
		}
		if (clearanceOf(item, clearances).isEmpty()) {
			++report.unknown_clearance_count;
		}
	}
	return report;
}
