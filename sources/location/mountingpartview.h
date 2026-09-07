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
#ifndef MOUNTINGPARTVIEW_H
#define MOUNTINGPARTVIEW_H

#include "mountingcheck.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

class Catalog;
class CatalogPart;

/**
	@brief What the catalogue says about the body of one bought product:
	how big it is, how much air it wants, where its axis is, and whether
	its outline is drawn.

	Millimetre throughout, in the frame MountingArea declares. Ten keys are
	read and all ten belong to the product code and not to the instance -
	the seventh contactor somebody drops onto a plate is not a different
	contactor. That is what makes changing the product code change the
	drawing, and it is what makes the drawing impossible to falsify by
	typing a number into a box.

	Every measure has one state more than a number: nobody filled it in.
	The two are told apart at the boundary, where the catalogue still holds
	the difference - an empty cell is not a zero, and the file the value
	comes from keeps them apart. What each field does with that state is
	not the same answer everywhere, and the difference is deliberate:

	| Field                | Missing reads as | Why |
	|----------------------|------------------|-----|
	| width, height, depth | not measured     | nothing is 0 mm wide, so zero and empty are one state - see MountedItem |
	| clearance            | asks for nothing | a part nobody measured must not become a violation - see MountingClearance |
	| insertion            | not declared     | an offset of zero is the top left corner, which is a real answer somebody may have chosen |
	| draw_outline         | true             | a box in millimetre is what a part with no picture has to show |

	A negative measure is folded away rather than used, for the reason
	MountingClearance folds it: growing a body by a negative number shrinks
	it, and a shrunken body hides the overlap it was asked about.

	known is the field worth reading before the others. A code the
	catalogue does not have and a part with nothing filled in are two
	different problems - the first is a wrong code or a share that is
	down, the second is a part nobody has measured yet - and a view that
	could not tell them apart would let the second sentence be said about
	the first.
*/
class MountingPartView
{
	public:
		MountingPartView() {}

			/// @return true when the catalogue holds this product code
		bool isKnownPart() const;
			/// @return true when nothing at all was read
		bool isNull() const;

			/// @return true when the width is a length
		bool hasWidth() const;
			/// @return true when the height is a length
		bool hasHeight() const;
			/// @return true when both are, so the body is a rectangle
		bool hasSize() const;
			/// @return true when the depth is a length
		bool hasDepth() const;

		/**
			@return true when at least one of the four clearances was
			filled in, whatever it says.

			Kept apart from the numbers because the numbers cannot
			hold it: MountingClearance treats a clearance of zero and
			a clearance nobody measured as one state, on purpose, so
			a part somebody deliberately recorded as needing no air
			reads there exactly like a part nobody looked at. The
			distinction survives here, for whoever writes the
			sentence the panel shows.
		*/
		bool hasClearance() const;

			/// @return true when both offsets of the axis were filled in
		bool hasInsertion() const;
		/**
			@return true when exactly one of the two offsets was
			filled in.

			The pair is indivisible - an axis needs both numbers - so
			half of it is not half an answer, it is a record somebody
			started and did not finish. Reported rather than
			completed, because the missing half would have to be
			invented.
		*/
		bool hasHalfInsertion() const;

		/**
			@return the room the body takes, millimetre, with a
			dimension nobody filled in folded to zero.

			Zero and not a guess, which is the same answer
			MountedItem gives and for the same reason: an item
			checked as a point is an item that stays in the list and
			gets reported, while an item given an invented 20 mm box
			passes the check and fails on the bench.
		*/
		QSizeF size() const;
			/// @return the room the body takes with its top left corner at @a corner
		QRectF footprintAt(const QPointF &corner) const;

		/**
			@return where the axis of the part sits inside its own
			body: an offset from the top left corner of the
			footprint, x running right and y running down.

			(0, 0) when the pair was not declared, which is the top
			left corner and is also what a caller that aligns by the
			edge of the footprint would use. hasInsertion is what
			tells the two apart, and a caller that clips a part onto
			a rail has to ask: aligning a fuse holder by its corner
			when its axis is 12 mm above centre is the mistake that
			puts a whole row out of line.
		*/
		QPointF insertionOffset() const;
			/// @return the axis of a part whose corner is at @a corner
		QPointF insertionAt(const QPointF &corner) const;

			/// the product code this was read for
		QString part_code;
			/// true when the catalogue holds that code
		bool known = false;
			/// how wide the body is, millimetre, zero when not measured
		qreal width = 0.0;
			/// how tall it is, millimetre, zero when not measured
		qreal height = 0.0;
			/// how deep it is, millimetre, zero when not measured
		qreal depth = 0.0;
			/// the air it asks for on each side, millimetre
		MountingClearance clearance;
			/// true when at least one clearance was filled in
		bool clearance_declared = false;
			/// the axis offset inside its own body, millimetre
		QPointF insertion;
			/// true when both offsets were filled in
		bool insertion_declared = false;
			/// true when exactly one of the two was filled in
		bool insertion_half_declared = false;
			/// whether the outline of the part is drawn
		bool draw_outline = true;
};

/**
	@brief The one road from a product code to the body of a part, and the
	only place in the layout that opens the catalogue.

	It exists because the two ends of it were built not to know each other.
	The catalogue is a database of products and knows nothing about plates;
	the mounting rules are arithmetic over rectangles and know nothing about
	databases, which is what lets a change of enclosure be proved on a
	bench. Something has to carry the ten numbers across, and it is better
	that it be one named file than a line of it in every dialogue that ever
	shows a plate.

	Reading only. Nothing here writes to the catalogue, nothing here opens
	a window, and nothing here decides where a part goes - laying a panel
	out is a person's work.

	@par A catalogue that does not answer never erases what the project knows

	The share the catalogue sits on can be down, and a product code can be
	wrong. In both cases partByCode answers nothing, and in both cases
	applyTo leaves the item exactly as the project stored it. That is the
	whole invalidation policy, and it is one sentence because the failure it
	avoids is one sentence: a project opened on a laptop away from the
	office must not come back with every box blanked.

	What does erase is a part the catalogue has and says nothing about. That
	is not the share being down, it is the office having removed a width it
	no longer trusts, and the plate has to say "not measured" rather than go
	on showing a number nobody stands behind.

	@par An item with no product code is not a catalogue part

	Rails and ducts are cut to length on the bench and carry no code. Their
	size is the person's, typed once and never derived, so applyTo passes
	over them untouched. Refreshing a layout that would resize every rail to
	nothing is the one way this class could destroy a drawing.

	The answer is guarded twice, and knowing which guard holds it matters:
	the empty code is turned away at the door as a fast path, and even if it
	were let through it would be turned away again by the paragraph above,
	because a code the catalogue cannot find changes nothing. Measured by
	removing the first guard and finding no case of the suite change.
*/
class MountingPartReader
{
	public:
		/**
			@return the keys of the catalogue this class reads, in
			the order of the table above.

			Named here as well as where they are seeded, and it is
			worth the repetition: this is the reading end, and a key
			renamed on one end only would make every part read as
			unmeasured - which is a state the whole family is
			designed to tolerate, so nothing would break loudly.
		*/
		static QStringList physicalViewKeys();

		/**
			@brief What the catalogue says about one product code.
			@param catalog the catalogue to ask
			@param part_code the product code
			@return the view, its known flag false when the
			catalogue has no such code
		*/
		static MountingPartView viewOf(const Catalog &catalog,
					       const QString &part_code);

		/**
			@brief What one part in hand says about its own body.
			@param catalog the catalogue the part came from, for the
			types and the initial values its class declares
			@param part the part
			@return the view

			Separate from the overload above so that a dialogue
			holding a part it has just edited can see the body it
			would produce without saving first.
		*/
		static MountingPartView viewOfPart(const Catalog &catalog,
						   const CatalogPart &part);

		/**
			@brief The bodies of several products, each read once.
			@param catalog the catalogue to ask
			@param part_codes the product codes, repeats and blanks
			allowed
			@return one entry per distinct code, blanks left out
		*/
		static QHash<QString, MountingPartView>
		viewsOf(const Catalog &catalog, const QStringList &part_codes);

		/**
			@brief The table of clearances the mounting check asks
			for.
			@param catalog the catalogue to ask
			@param part_codes the product codes to look up
			@return what each code asks for, keyed by code

			Only codes that ask for something are in it. An entry
			asking for nothing and a missing entry are the same
			answer to MountingCheck, so leaving them out keeps the
			table the size of what was actually measured - and keeps
			the count of parts checked against nothing right either
			way.
		*/
		static QHash<QString, MountingClearance>
		clearancesOf(const Catalog &catalog, const QStringList &part_codes);

			/// @return the clearances of everything mounted on @a surface
		static QHash<QString, MountingClearance>
		clearancesFor(const Catalog &catalog, const MountingSurface &surface);
			/// @return the clearances of everything mounted anywhere in @a layout
		static QHash<QString, MountingClearance>
		clearancesFor(const Catalog &catalog, const MountingLayout &layout);

		/**
			@brief The three questions asked of one face, with the
			numbers the catalogue holds.
			@param catalog the catalogue to ask
			@param surface the face and everything screwed to it
			@return the whole report

			The one call a panel makes. MountingCheck::surfaceReport
			with an empty table is a rule running with no
			requirement to compare against: it answers "clean" and
			says, through isConclusive, that it verified nothing.
			This is the call that gives it something to compare
			against, and the two have to be read the same way -
			isClean beside isConclusive, never one without the
			other.
		*/
		static MountingSurfaceReport
		checkSurface(const Catalog &catalog, const MountingSurface &surface);

		/**
			@brief One part of the catalogue, ready to be screwed
			down.
			@param catalog the catalogue to ask
			@param part_code the product code
			@param position where its top left corner goes,
			millimetre
			@param label what the sheet will show, empty when the
			panel is being laid out before the schematic
			@return the item, with the size the catalogue holds and
			no size at all when the catalogue holds none

			This is the box being born from the product code. An
			unknown code still produces an item, carrying the code
			that was asked for: the person typed a code and the
			answer to a wrong code is a part reported as unmeasured,
			not a part that never appeared.
		*/
		static MountedItem mountedItemFor(const Catalog &catalog,
						  const QString &part_code,
						  const QPointF &position,
						  const QString &label = QString());

		/**
			@brief Read the size of one mounted item back from the
			catalogue.
			@param catalog the catalogue to ask
			@param item the item, its size rewritten when the
			catalogue answers
			@return true when the size changed

			The invalidation the whole arrangement needs: the size
			stored in the project is a copy of what the catalogue
			said, so correcting a product record has to reach every
			plate that carries it. Called with a changed product
			code, it is what makes the box follow the code.

			Left untouched, and false returned, for an item with no
			product code and for a code the catalogue does not hold.
		*/
		static bool applyTo(const Catalog &catalog, MountedItem &item);

			/// @return the identifier of every item of @a surface whose size changed
		static QStringList applyTo(const Catalog &catalog,
					   MountingSurface &surface);
			/// @return the identifier of every item of @a layout whose size changed
		static QStringList applyTo(const Catalog &catalog,
					   MountingLayout &layout);

			/// @return the distinct product codes mounted on @a surface, blanks left out
		static QStringList partCodesOf(const MountingSurface &surface);
			/// @return the distinct product codes mounted anywhere in @a layout
		static QStringList partCodesOf(const MountingLayout &layout);

		/**
			@brief Which codes of a face the catalogue does not
			hold.
			@param catalog the catalogue to ask
			@param surface the face
			@return the codes, in the order they were met

			Asked separately from everything else because the answer
			is a different kind of trouble: a code nobody can find
			is a typing mistake or a catalogue that is not there,
			and neither is fixed by dragging anything on a plate.
		*/
		static QStringList unknownCodes(const Catalog &catalog,
						const MountingSurface &surface);
};

#endif // MOUNTINGPARTVIEW_H
