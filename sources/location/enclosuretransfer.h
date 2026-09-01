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
#ifndef ENCLOSURETRANSFER_H
#define ENCLOSURETRANSFER_H

#include <QCoreApplication>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
	@brief What became of one mounted item when the enclosure changed.

	Four answers and not two, because "it does not fit" is two different
	problems for the person who has to fix it. An item that would fit the
	new area somewhere else only needs dragging; an item wider than the
	whole area needs another enclosure, or another part. Telling them apart
	is the difference between a warning that can be acted on and a warning
	that can only be acknowledged.
*/
enum class MountingFit
{
	Fits,           ///< it stays, at the very millimetre it was already at
	OutsideArea,    ///< the new area could hold it, but not where it sits
	LargerThanArea, ///< the new area cannot hold it at any position
	NoArea          ///< the new area has no usable dimension at all
};

/**
	@brief The room there is to mount things on: a mounting plate, a door,
	the back of a box.

	Two numbers in millimetre, and a frame. The frame is the part worth
	writing down: x runs right, y runs down, and the origin is the top left
	corner of the area itself, so a position of (0, 0) is the top left
	corner of what can be mounted on. That makes the origin true by
	definition rather than by agreement, and it leaves the offset between
	an area and the enclosure around it where it belongs - with whoever
	draws them.

	The numbers arrive as parameters and are read from nothing. That is not
	a shortcut taken to keep this file pure. The useful mounting area is not
	the outside size of the enclosure: a 600 x 800 x 250 box has a mounting
	plate smaller than the box on every side, sold as a part of its own, and
	its door is a third area again. The catalogue seeds width, height and
	depth on its component class, which is what a bought component carries;
	the location class is a sibling of that class and inherits none of the
	three, and none of the three would be the mounting area even if it did.
	So the caller is the only honest source of these two numbers today, and
	the day a location declares its own mounting area, this signature does
	not have to change.

	One area is one mounting surface, which is why the transfer is called
	once per surface and not once per enclosure. A door and a plate lose
	different items in the same swap, and averaging them would report a fit
	that exists on neither.
*/
class MountingArea
{
	public:
		MountingArea() {}
		MountingArea(qreal area_width, qreal area_height);
		explicit MountingArea(const QSizeF &area_size);

		/**
			@return true when both dimensions are a real length.

			Zero is refused along with the negative and the not-a-number:
			nothing is mounted on a line, and an area nobody has filled in
			arrives as zero rather than as an announcement.
		*/
		bool isValid() const;
		QSizeF size() const;
			/// @return the area in its own frame: (0, 0, width, height)
		QRectF rect() const;

		/**
			@brief Whether @a footprint sits inside this area where it is.
			@param footprint the room an item takes, in the frame above
			@return false for an unusable area and for a footprint whose
			numbers are not lengths

			Compared with tolerance() of slack on each edge, so that an
			item pushed flush against the edge stays inside. Flush is the
			normal case and not the exception: a duct is run to the edge
			of the plate, and a rail is cut to it.
		*/
		bool holds(const QRectF &footprint) const;

		/**
			@brief Whether @a footprint would fit this area anywhere.
			@param footprint the size of an item, position ignored
			@return true when the area is at least that wide and that tall

			This is the question that separates an item to be dragged from
			an item to be bought again.
		*/
		bool canEverHold(const QSizeF &footprint) const;

		/**
			@return the slack a length comparison allows, in millimetre.

			A nanometre: far below anything sheet metal promises, and
			above what a sum of millimetres in a double leaves behind. It
			exists so that an item ending exactly at the edge is inside,
			which is a statement about arithmetic and not about clearance -
			the clearance a real cabinet needs is a decision for the person
			laying it out, and taking it here would silently shrink every
			area the program was handed.
		*/
		static qreal tolerance();

			/// how wide the surface is, millimetre
		qreal width = 0.0;
			/// how tall it is, millimetre
		qreal height = 0.0;
};

/**
	@brief One thing screwed to a mounting surface, as the transfer sees it.

	Everything already resolved: where it sits and how much room it takes.
	Resolved on purpose, the same way the renumbering resolves its input -
	working out the size of an item means asking the catalogue for the part,
	and doing that in here would mean the rule could not be checked without
	a project open. The rule is the part that goes wrong.

	Three fields name the item and none of them is mandatory, because the
	warning has to be able to name an item that was never named. label is
	what the sheet shows, -Q1; part_code is the product; uuid is what the
	caller stitches the answer back onto.
*/
class MountedItem
{
	Q_DECLARE_TR_FUNCTIONS(MountedItem)

	public:
		MountedItem() {}
		MountedItem(const QString &item_label,
			    const QPointF &item_position,
			    const QSizeF &item_size);

			/// @return true when nothing here names the item
		bool isNull() const;

		/**
			@return size with the numbers that are not lengths folded to
			zero.

			A part whose dimensions nobody has typed yet has to be checked
			as a point rather than refused, because refusing it would drop
			it out of the transfer - and dropping something out is the one
			outcome this whole file exists to prevent. hasDeclaredSize
			reports that the check was made on a point, so the warning can
			say so instead of promising a fit it never verified.
		*/
		QSizeF declaredSize() const;
			/// @return true when both dimensions of the item are known
		bool hasDeclaredSize() const;
			/// @return the room it takes where it sits now
		QRectF footprint() const;

		/**
			@return how to call this item out loud: its label, failing
			that its part, failing that its identifier, failing all three
			a sentence saying it has no mark. Never empty, because a
			warning that lists an empty name lists nothing.
		*/
		QString designation() const;

			/// identity, so the plan can be applied back onto it
		QString uuid;
			/// what the sheet shows: -Q1
		QString label;
			/// the catalogue part it was bought as, to name the product
		QString part_code;
			/// top left corner of its footprint, millimetre
		QPointF position;
			/// the room it takes on the surface, millimetre
		QSizeF size;
};

/**
	@brief What the transfer decided about one item.
*/
class TransferredItem
{
	Q_DECLARE_TR_FUNCTIONS(TransferredItem)

	public:
			/// the item as it was handed over
		MountedItem item;
			/**
				What happens to it. NoArea by default on purpose: a
				default built entry has been told nothing, and nothing
				told must not read as permission to keep it.
			*/
		MountingFit fit = MountingFit::NoArea;
			/**
				Where to mount it now. Equal to item.position whenever
				fit is Fits - see EnclosureTransfer::transferredPosition
				for why that equality is the whole rule. For anything
				else it is where the item still is, and applying it would
				be applying a transfer that was refused.
			*/
		QPointF position;
			/**
				It did not sit inside the old area either. Only ever true
				when the old area was known, so read it together with the
				old area of the plan: false means nothing was found
				against the item, not that it was inside.

				Kept because a swap gets blamed for what it did not do.
				An item somebody dragged off the plate months ago is
				reported here as it should be, but the sentence says the
				old enclosure did not hold it either.
			*/
		bool outside_before = false;
			/// nobody has typed this item's dimensions - see MountedItem
		bool size_unknown = false;

			/// @return true when the item survives the swap where it is
		bool isTransferred() const;
			/// @return what to say about it, empty when it just fits
		QString reason() const;
};

/**
	@brief What swapping the enclosure would do to the items mounted in it,
	before it does it.

	The plan is what makes the swap answerable. Losing a layout because a
	code was changed in a combo box is the failure this is written against,
	so nothing here removes anything: the plan says what would survive,
	where, and names what would not, and the caller does not get to skip
	reading it.

	Both areas are kept. The old one is not needed to work out a single new
	position - that follows from how the anchor was chosen, see
	EnclosureTransfer::transferredPosition - and it is asked for anyway,
	because it is what lets the plan say that the dimensions changed at all,
	which is half of what the person is owed, and what lets an item that was
	already hanging off the plate be reported without accusing the new
	enclosure of it.
*/
class EnclosureTransferPlan
{
	Q_DECLARE_TR_FUNCTIONS(EnclosureTransferPlan)

	public:
			/// one per item handed over, in the order they were handed
		QList<TransferredItem> entries;
			/// the surface the items are mounted on today
		MountingArea old_area;
			/// the surface of the enclosure being put in its place
		MountingArea new_area;

		/**
			@return true when the new area can hold anything at all.

			False is not a plan with nothing in it: it is a plan that must
			not be applied. Every entry then reads NoArea, and the list of
			what would be lost is the whole list - which is the point, as
			the caller still has to be told what is at stake.
		*/
		bool isUsable() const;

			/// @return true when either dimension is not what it was
		bool dimensionsChanged() const;
			/// @return new minus old, millimetre; negative where it shrank
		QSizeF sizeChange() const;
			/// @return true when some dimension got smaller
		bool shrinks() const;
			/// @return true when some dimension got bigger
		bool grows() const;

		int transferCount() const;
		int lossCount() const;
		bool hasLoss() const;
		QList<TransferredItem> transferred() const;
			/// what does not survive, in the order the items came in
		QList<TransferredItem> lost() const;
		/**
			@return the name of everything that would be lost.

			Complete, always, however long. This is the list that answers
			"which ones", and it is deliberately not the sentence
			warning() builds: a message box that names two hundred items
			names none of them, while a plan that hides the two hundredth
			has lost it. The two jobs are separated so that neither has to
			be compromised for the other.
		*/
		QStringList lostDesignations() const;
			/// what was transferred without its dimensions being known
		QList<TransferredItem> withUnknownSize() const;

			/// @return the entry of @a uuid, a default one when there is none
		TransferredItem entryOf(const QString &uuid) const;

		/**
			@return the whole warning as one text, empty when there is
			nothing to warn about.

			Says the dimensions change even when nothing is lost, because
			a person who is told nothing reads the swap as free and finds
			out at the drilling. Names the items that would not follow, up
			to warningItemLimit of them, and counts the rest - the names of
			all of them are in lostDesignations.
		*/
		QString warning() const;

		/**
			@return how many items the warning names one by one.

			The limit is on the sentence and never on the plan. Six,
			because a swap that loses a handful is the common one and its
			whole loss then reads at a glance, while a swap that loses
			forty is not made readable by listing forty.
		*/
		static int warningItemLimit();
};

/**
	@brief The rule that moves what is mounted from one enclosure into
	another one.

	Static and pure. It holds no project, opens no dialogue and knows no
	graphics item, which is what lets a swap be proved on a bench instead of
	by swapping a real cabinet and looking.

	What it deliberately does not do is find a new place for what no longer
	fits. Packing items onto a plate is a decision with rules the program
	does not hold - which side the incoming supply enters, what has to stay
	reachable, what may not sit above what - and taking that decision
	quietly would be the same fault as discarding the item quietly, one step
	further down. The rule names the item and stops.
*/
class EnclosureTransfer
{
	public:
		/**
			@brief Where an item goes in the new enclosure.
			@param item the item as it is mounted today
			@param old_area the surface it is mounted on
			@param new_area the surface of the enclosure replacing it
			@return the position to mount it at, which is the position it
			already has

			The whole decision of this file, in one function, so that
			there is one place to read it and one place to change it.

			Relative position is kept as an absolute distance from a
			corner - the origin corner of the area, which by the frame of
			MountingArea is the top left - and not as a proportion of the
			two dimensions. Proportional is the tempting reading of
			"relative" and it is the wrong one here, because a panel part
			does not scale: a 22.5 mm breaker is 22.5 mm in every cabinet
			ever built. Scale the coordinates of two breakers standing
			side by side into an area half as wide and their centres come
			half as far apart while their bodies stay the same width, so
			they overlap - the layout is not preserved, it is destroyed
			with arithmetic. The same scaling walks a rail off its mounting
			holes and stretches a duct that is sold in fixed lengths.

			Anchoring costs something and it is worth naming: the items
			all stay bunched against the origin corner, so a taller
			enclosure gains its extra room at the bottom and a wider one at
			the right, and nothing recentres itself. That is a layout a
			person finishes by dragging, which is the honest state of
			affairs, and it buys the property that matters - centre to
			centre spacing, rail lengths and the drilling plan all still
			hold, and an enclosure that only grows moves nothing at all.

			old_area and new_area are therefore unused, and are in the
			signature anyway: the caller reads the rule from what it is
			given, and a later rule that does need them does not change
			every call.
		*/
		static QPointF transferredPosition(const MountedItem &item,
						   const MountingArea &old_area,
						   const MountingArea &new_area);

		/**
			@brief What becomes of one item in one area.
			@param item the item as it is mounted today
			@param area the surface of the enclosure replacing the old one
			@return which of the four answers applies

			An item both too large and out of bounds is reported as too
			large, because that is the one of the two a person cannot fix
			by dragging.
		*/
		static MountingFit fitOf(const MountedItem &item,
					 const MountingArea &area);

		/**
			@brief What the swap would do.
			@param items everything mounted on the old surface
			@param old_area the surface they are mounted on
			@param new_area the surface of the enclosure replacing it
			@return the plan, one entry per item, none of them dropped

			Every item comes back in the plan, including the one that
			names itself in no way and the one whose dimensions nobody
			typed. Skipping either would be quietly deciding that an item
			nobody finished describing does not count, and the item nobody
			finished describing is exactly the one that goes missing.
		*/
		static EnclosureTransferPlan plan(const QList<MountedItem> &items,
						  const MountingArea &old_area,
						  const MountingArea &new_area);
};

#endif // ENCLOSURETRANSFER_H
