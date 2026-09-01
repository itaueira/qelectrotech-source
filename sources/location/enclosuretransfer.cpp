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
#include "enclosuretransfer.h"

#include <cmath>

namespace
{
	/**
		@brief isUsableLength
		@param length a length in millimetre
		@return true when it is a number and there is something of it

		A dimension nobody has filled in arrives as zero and not as a
		complaint, and an area of zero millimetre holds nothing - so both
		cases get the same answer here.
	*/
	bool isUsableLength(qreal length)
	{
		return std::isfinite(length)
		       && length > MountingArea::tolerance();
	}

	/**
		@brief usableLength
		@param length a length in millimetre
		@return the length, or zero when it is not a length

		Folded and not refused. An item whose size makes no sense is
		checked as a point and flagged, because refusing it would take it
		out of the plan, and taking something out of the plan is the one
		thing this file exists to prevent.
	*/
	qreal usableLength(qreal length)
	{
		return isUsableLength(length) ? length : 0.0;
	}

	/**
		@brief changedLength
		@param before a length in millimetre
		@param after the length it became
		@return true when the two are not the same length

		Numbers that are not lengths count as changed. Reporting them as
		unchanged would be a claim about an enclosure nobody has
		measured, and that is the one answer able to keep the warning
		quiet when it must not be.
	*/
	bool changedLength(qreal before, qreal after)
	{
		if (!std::isfinite(before) || !std::isfinite(after)) {
			return true;
		}
		return qAbs(after - before) > MountingArea::tolerance();
	}

	/**
		@brief millimetre
		@param length a length in millimetre
		@return the length as it goes into a sentence

		Six significant digits, which is more than sheet metal has and
		enough that 22.5 does not read as 23.
	*/
	QString millimetre(qreal length)
	{
		if (!std::isfinite(length)) {
			return QStringLiteral("?");
		}
		return QString::number(length);
	}
}

/**
	@brief MountingArea::MountingArea
	@param area_width how wide the surface is, millimetre
	@param area_height how tall it is, millimetre
*/
MountingArea::MountingArea(qreal area_width, qreal area_height) :
	width(area_width),
	height(area_height)
{}

/**
	@brief MountingArea::MountingArea
	@param area_size the surface as one size, millimetre
*/
MountingArea::MountingArea(const QSizeF &area_size) :
	width(area_size.width()),
	height(area_size.height())
{}

/**
	@brief MountingArea::isValid
	@return true when both dimensions are a real length
*/
bool MountingArea::isValid() const
{
	return isUsableLength(width) && isUsableLength(height);
}

/**
	@brief MountingArea::size
	@return the two dimensions as they were given, unfolded

	Unfolded on purpose: whoever compares two areas has to be able to see
	that one of them was never filled in.
*/
QSizeF MountingArea::size() const
{
	return QSizeF(width, height);
}

/**
	@brief MountingArea::rect
	@return the area in its own frame, origin at its top left corner
*/
QRectF MountingArea::rect() const
{
	return QRectF(QPointF(0.0, 0.0), size());
}

/**
	@brief MountingArea::holds
	@param footprint the room an item takes, in the frame of this area
	@return true when the footprint sits inside the area where it is
*/
bool MountingArea::holds(const QRectF &footprint) const
{
	if (!isValid()) {
		return false;
	}

	const QRectF box = footprint.normalized();
	if (!std::isfinite(box.left()) || !std::isfinite(box.top())
	    || !std::isfinite(box.right()) || !std::isfinite(box.bottom())) {
		return false;
	}

	const qreal slack = tolerance();
	return box.left() >= -slack
	       && box.top() >= -slack
	       && box.right() <= width + slack
	       && box.bottom() <= height + slack;
}

/**
	@brief MountingArea::canEverHold
	@param footprint the size of an item, its position ignored
	@return true when the area is at least that wide and that tall
*/
bool MountingArea::canEverHold(const QSizeF &footprint) const
{
	if (!isValid()) {
		return false;
	}
	if (!std::isfinite(footprint.width())
	    || !std::isfinite(footprint.height())) {
		return false;
	}

	const qreal slack = tolerance();
	return footprint.width() <= width + slack
	       && footprint.height() <= height + slack;
}

/**
	@brief MountingArea::tolerance
	@return the slack a length comparison allows, millimetre
*/
qreal MountingArea::tolerance()
{
	return 1e-6;
}

/**
	@brief MountedItem::MountedItem
	@param item_label what the sheet shows
	@param item_position top left corner of its footprint, millimetre
	@param item_size the room it takes on the surface, millimetre
*/
MountedItem::MountedItem(const QString &item_label,
			 const QPointF &item_position,
			 const QSizeF &item_size) :
	label(item_label),
	position(item_position),
	size(item_size)
{}

/**
	@brief MountedItem::isNull
	@return true when nothing here names the item
*/
bool MountedItem::isNull() const
{
	return uuid.isEmpty() && label.isEmpty() && part_code.isEmpty();
}

/**
	@brief MountedItem::declaredSize
	@return the size with whatever is not a length folded to zero
*/
QSizeF MountedItem::declaredSize() const
{
	return QSizeF(usableLength(size.width()),
		      usableLength(size.height()));
}

/**
	@brief MountedItem::hasDeclaredSize
	@return true when both dimensions of the item are known
*/
bool MountedItem::hasDeclaredSize() const
{
	return isUsableLength(size.width()) && isUsableLength(size.height());
}

/**
	@brief MountedItem::footprint
	@return the room it takes where it sits now
*/
QRectF MountedItem::footprint() const
{
	return QRectF(position, declaredSize());
}

/**
	@brief MountedItem::designation
	@return how to call this item out loud, never empty
*/
QString MountedItem::designation() const
{
	if (!label.isEmpty()) {
		return label;
	}
	if (!part_code.isEmpty()) {
		return part_code;
	}
	if (!uuid.isEmpty()) {
		return uuid;
	}
	return tr("un élément sans repère");
}

/**
	@brief TransferredItem::isTransferred
	@return true when the item survives the swap where it is
*/
bool TransferredItem::isTransferred() const
{
	return fit == MountingFit::Fits;
}

/**
	@brief TransferredItem::reason
	@return what to say about this item, empty when it simply fits

	One sentence, the name of the item first, because the list it goes into
	is read to find out which parts are at stake.
*/
QString TransferredItem::reason() const
{
	const QString name = item.designation();

	switch (fit)
	{
		case MountingFit::Fits:
			if (size_unknown) {
				return tr("%1 : dimensions inconnues, position "
					  "conservée sans vérification.")
					.arg(name);
			}
			return QString();

		case MountingFit::OutsideArea:
			if (outside_before) {
				return tr("%1 : déjà hors de la surface de "
					  "montage avant le remplacement.")
					.arg(name);
			}
			return tr("%1 : ne tient plus à cette position, mais "
				  "tient ailleurs sur la surface de montage.")
				.arg(name);

		case MountingFit::LargerThanArea:
			return tr("%1 : trop grand pour la surface de montage, "
				  "il mesure %2 x %3 mm.")
				.arg(name,
				     millimetre(item.declaredSize().width()),
				     millimetre(item.declaredSize().height()));

		case MountingFit::NoArea:
			return tr("%1 : le coffret de remplacement n'a aucune "
				  "surface de montage utilisable.").arg(name);
	}

	return QString();
}

/**
	@brief EnclosureTransferPlan::isUsable
	@return true when the new area can hold anything at all
*/
bool EnclosureTransferPlan::isUsable() const
{
	return new_area.isValid();
}

/**
	@brief EnclosureTransferPlan::dimensionsChanged
	@return true when either dimension is not what it was
*/
bool EnclosureTransferPlan::dimensionsChanged() const
{
	return changedLength(old_area.width, new_area.width)
	       || changedLength(old_area.height, new_area.height);
}

/**
	@brief EnclosureTransferPlan::sizeChange
	@return new minus old, millimetre, negative where it shrank
*/
QSizeF EnclosureTransferPlan::sizeChange() const
{
	return QSizeF(new_area.width - old_area.width,
		      new_area.height - old_area.height);
}

/**
	@brief EnclosureTransferPlan::shrinks
	@return true when some dimension got smaller
*/
bool EnclosureTransferPlan::shrinks() const
{
	const qreal slack = MountingArea::tolerance();
	return new_area.width < old_area.width - slack
	       || new_area.height < old_area.height - slack;
}

/**
	@brief EnclosureTransferPlan::grows
	@return true when some dimension got bigger

	True together with shrinks when the enclosure changed proportion. Taller
	and narrower is another enclosure and not a bigger one, and it is the
	swap that surprises people.
*/
bool EnclosureTransferPlan::grows() const
{
	const qreal slack = MountingArea::tolerance();
	return new_area.width > old_area.width + slack
	       || new_area.height > old_area.height + slack;
}

/**
	@brief EnclosureTransferPlan::transferCount
	@return how many items keep their place
*/
int EnclosureTransferPlan::transferCount() const
{
	int count = 0;
	for (const TransferredItem &entry : entries)
	{
		if (entry.isTransferred()) {
			++count;
		}
	}
	return count;
}

/**
	@brief EnclosureTransferPlan::lossCount
	@return how many items do not follow the swap
*/
int EnclosureTransferPlan::lossCount() const
{
	return entries.count() - transferCount();
}

/**
	@brief EnclosureTransferPlan::hasLoss
	@return true when the swap costs something
*/
bool EnclosureTransferPlan::hasLoss() const
{
	return lossCount() > 0;
}

/**
	@brief EnclosureTransferPlan::transferred
	@return the entries that keep their place
*/
QList<TransferredItem> EnclosureTransferPlan::transferred() const
{
	QList<TransferredItem> result;
	for (const TransferredItem &entry : entries)
	{
		if (entry.isTransferred()) {
			result << entry;
		}
	}
	return result;
}

/**
	@brief EnclosureTransferPlan::lost
	@return the entries that do not follow, in the order they came in
*/
QList<TransferredItem> EnclosureTransferPlan::lost() const
{
	QList<TransferredItem> result;
	for (const TransferredItem &entry : entries)
	{
		if (!entry.isTransferred()) {
			result << entry;
		}
	}
	return result;
}

/**
	@brief EnclosureTransferPlan::lostDesignations
	@return the name of everything that would be lost, all of it
*/
QStringList EnclosureTransferPlan::lostDesignations() const
{
	QStringList names;
	for (const TransferredItem &entry : entries)
	{
		if (!entry.isTransferred()) {
			names << entry.item.designation();
		}
	}
	return names;
}

/**
	@brief EnclosureTransferPlan::withUnknownSize
	@return the entries kept although their dimensions are not known
*/
QList<TransferredItem> EnclosureTransferPlan::withUnknownSize() const
{
	QList<TransferredItem> result;
	for (const TransferredItem &entry : entries)
	{
		if (entry.isTransferred() && entry.size_unknown) {
			result << entry;
		}
	}
	return result;
}

/**
	@brief EnclosureTransferPlan::entryOf
	@param uuid the identity of an item that was handed over
	@return its entry, a default built one when there is no such item
*/
TransferredItem EnclosureTransferPlan::entryOf(const QString &uuid) const
{
	if (uuid.isEmpty()) {
		return TransferredItem();
	}

	for (const TransferredItem &entry : entries)
	{
		if (entry.item.uuid == uuid) {
			return entry;
		}
	}
	return TransferredItem();
}

/**
	@brief EnclosureTransferPlan::warning
	@return the whole warning as one text, empty when there is nothing to
	warn about

	Built in the order the person needs it: what the enclosure did, then what
	that costs and to which parts, then what could not be checked. The
	enumeration stops at warningItemLimit and counts the rest, and that count
	is what points at lostDesignations, which never stops.
*/
QString EnclosureTransferPlan::warning() const
{
	QStringList lines;

	if (!isUsable())
	{
		lines << tr("Le coffret de remplacement n'a pas de surface de "
			    "montage utilisable : rien ne peut y être reporté.");
	}
	else if (!old_area.isValid())
	{
		lines << tr("La surface de montage du coffret de remplacement "
			    "est de %1 x %2 mm ; celle du coffret actuel n'est "
			    "pas connue.")
			 .arg(millimetre(new_area.width),
			      millimetre(new_area.height));
	}
	else if (dimensionsChanged())
	{
		lines << tr("La surface de montage passe de %1 x %2 mm à "
			    "%3 x %4 mm.")
			 .arg(millimetre(old_area.width),
			      millimetre(old_area.height),
			      millimetre(new_area.width),
			      millimetre(new_area.height));
	}

	const QList<TransferredItem> losses = lost();
	if (!losses.isEmpty())
	{
		lines << tr("%n élément(s) ne suivent pas le remplacement :",
			    "", losses.count());

		int shown = 0;
		for (const TransferredItem &entry : losses)
		{
			if (shown >= warningItemLimit()) {
				break;
			}
			const QString sentence = entry.reason();
			if (!sentence.isEmpty())
			{
				lines << sentence;
				++shown;
			}
		}

		const int hidden = losses.count() - shown;
		if (hidden > 0) {
			lines << tr("... et %n autre(s) élément(s).",
				    "", hidden);
		}
	}

	const QList<TransferredItem> unchecked = withUnknownSize();
	if (!unchecked.isEmpty())
	{
		lines << tr("%n élément(s) ont été reportés sans que leurs "
			    "dimensions soient connues.",
			    "", unchecked.count());
	}

	return lines.join(QStringLiteral("\n"));
}

/**
	@brief EnclosureTransferPlan::warningItemLimit
	@return how many items the warning names one by one
*/
int EnclosureTransferPlan::warningItemLimit()
{
	return 6;
}

/**
	@brief EnclosureTransfer::transferredPosition
	@param item the item as it is mounted today
	@param old_area the surface it is mounted on
	@param new_area the surface of the enclosure replacing it
	@return the position to mount it at, which is the one it already has

	The identity, and the reason it is the identity is in the header. It is a
	function and not an omission so that the choice has one address: a rule
	that anchors elsewhere, or that offsets the whole layout because the
	plate sits differently inside the box, is a change here and nowhere else.
*/
QPointF EnclosureTransfer::transferredPosition(const MountedItem &item,
					       const MountingArea &old_area,
					       const MountingArea &new_area)
{
	Q_UNUSED(old_area);
	Q_UNUSED(new_area);

	return item.position;
}

/**
	@brief EnclosureTransfer::fitOf
	@param item the item as it is mounted today
	@param area the surface of the enclosure replacing the old one
	@return which of the four answers applies

	Order matters. An unusable area answers first, because nothing can be
	said about fitting inside nothing. Too large answers before out of
	bounds, because an item that is both cannot be fixed by dragging it, and
	the warning is read to find out what has to be done.
*/
MountingFit EnclosureTransfer::fitOf(const MountedItem &item,
				     const MountingArea &area)
{
	if (!area.isValid()) {
		return MountingFit::NoArea;
	}
	if (area.holds(item.footprint())) {
		return MountingFit::Fits;
	}
	if (!area.canEverHold(item.declaredSize())) {
		return MountingFit::LargerThanArea;
	}
	return MountingFit::OutsideArea;
}

/**
	@brief EnclosureTransfer::plan
	@param items everything mounted on the old surface
	@param old_area the surface they are mounted on
	@param new_area the surface of the enclosure replacing it
	@return the plan, one entry per item, none of them dropped
*/
EnclosureTransferPlan EnclosureTransfer::plan(const QList<MountedItem> &items,
					      const MountingArea &old_area,
					      const MountingArea &new_area)
{
	EnclosureTransferPlan result;
	result.old_area = old_area;
	result.new_area = new_area;

	for (const MountedItem &item : items)
	{
		TransferredItem entry;
		entry.item = item;
		entry.size_unknown = !item.hasDeclaredSize();
		entry.fit = fitOf(item, new_area);
		entry.position = entry.isTransferred()
				 ? transferredPosition(item, old_area, new_area)
				 : item.position;
		entry.outside_before = old_area.isValid()
				       && !old_area.holds(item.footprint());

		result.entries << entry;
	}

	return result;
}
