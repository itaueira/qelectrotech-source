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
#ifndef MOUNTINGCHECK_H
#define MOUNTINGCHECK_H

#include "mountinglayout.h"

#include <QHash>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
	@brief How much air one part asks for on each of its four sides,
	millimetre.

	Four numbers and not one, because that is the shape the answer has: a
	variable speed drive wants room above it and below it and nothing at
	all at its sides, and a single figure would either starve the top or
	waste the width of the plate. The four keys exist in the catalogue
	already - clearance_top, clearance_bottom, clearance_left and
	clearance_right, seeded in millimetre on the component class - and this
	class is the shape they are read into.

	Nothing is asked for by default. A clearance nobody measured has to
	behave as no requirement, which is what lets the arithmetic run today
	while the number is still being decided: no requirement produces no
	violation, and the report says how many parts were checked against
	nothing rather than announcing them clear.

	Zero and unmeasured are deliberately the same state here, and only
	here. The part dialogue already treats a measure left at zero as not
	filled in, so a third state would be a distinction the file the number
	comes from cannot make. A part which really needs no air gets the same
	answer as a part nobody has measured, and neither of the two ever turns
	into a violation.

	A negative number is folded away rather than used. Growing a footprint
	by a negative clearance shrinks it, and a shrunken footprint hides the
	overlap it was asked about - the one failure mode of this class that
	would make the panel quieter than the truth.
*/
class MountingClearance
{
	public:
		MountingClearance() {}
		MountingClearance(qreal clearance_top,
				  qreal clearance_bottom,
				  qreal clearance_left,
				  qreal clearance_right);
			/// @return the same air on all four sides
		static MountingClearance uniform(qreal all_sides);

			/// @return true when no side asks for anything
		bool isEmpty() const;
		/**
			@param side a clearance in millimetre
			@return the number asked for, zero when it is not a
			length
		*/
		static qreal asked(qreal side);

		/**
			@brief The room a part needs, its body and its air.
			@param footprint the room the body takes, millimetre
			@return the body grown by what each side asks for

			Returns the footprint untouched when nothing is asked
			for, so that a part with no clearance recorded is
			compared as its own body and not as a rectangle a
			rounding away from it.
		*/
		QRectF grown(const QRectF &footprint) const;

		bool operator==(const MountingClearance &other) const;
		bool operator!=(const MountingClearance &other) const;

			/// air wanted above it, millimetre
		qreal top = 0.0;
			/// air wanted below it, millimetre
		qreal bottom = 0.0;
			/// air wanted to its left, millimetre
		qreal left = 0.0;
			/// air wanted to its right, millimetre
		qreal right = 0.0;
};

/**
	@brief Two parts whose bodies want the same room on the plate.

	Symmetric, and reported once for the pair: metal on metal is not a
	complaint one of the two owns. The two identifiers come in the order
	the items were handed over, so a caption built from the list reads the
	same way twice.

	shared is the room the two of them claim at once, in the frame of
	MountingArea. It is carried rather than left to whoever shows it,
	because the drawing has to be able to hatch exactly what the rule
	objected to.
*/
class MountingOverlap
{
	public:
			/// the first of the two, in the order they were handed over
		QString first_uuid;
			/// the second one
		QString second_uuid;
			/// the room they both claim, millimetre
		QRectF shared;
		/**
			How far the shorter way out is, millimetre: the smallest
			distance either part can be pushed, along one axis, for
			the room to be free. Which of the two moves is a
			person's decision, and this rule does not take it - the
			number is the same either way, since pushing one part
			right is pushing the other one left.

			Carried rather than worked out by whoever shows it,
			because it needs both rectangles and not just the shared
			one. The smaller side of the shared rectangle is the
			tempting answer and it is wrong whenever one part sits
			entirely inside the other along that axis: the shared
			room is then the whole width of the inner part, which is
			not how far it has to travel to get out.
		*/
		qreal way_out = 0.0;
};

/**
	@brief One part sitting in the air another one asked for.

	Directional on purpose, and this is the whole subtlety of the clearance
	check. The air belongs to the part that asked for it, so the answer
	names a claimant and an intruder, and the same two parts can produce
	two entries when both of them asked and both are starved.

	Directional because the arithmetic has to be. Growing both footprints
	and intersecting them counts the same gap twice: two parts that each
	want 100 mm and stand 150 mm apart are both satisfied - each one has
	150 mm of body-free room where it wanted 100 - yet their grown
	rectangles overlap by 50 mm, and a symmetric rule would report a
	violation that does not exist. So one part is grown at a time and
	tested against the other one's body, which is the question the person
	laying the panel out is actually asking.

	Bodies that overlap outright are not reported here. They are a
	MountingOverlap, which is a different sentence for a different problem:
	one is two parts that cannot both be screwed down, the other is a part
	that will overheat.
*/
class MountingEncroachment
{
	public:
			/// the part whose air is taken, and which asked for it
		QString claimant_uuid;
			/// the part standing in that air
		QString intruder_uuid;
			/// the asked-for room the intruder occupies, millimetre
		QRectF shared;
		/**
			How many millimetre of air are missing: how far the
			intruder has to be pushed, along the shortest way out,
			for the requirement to be met. Reads as the plain number
			a person expects - a part 30 mm away from one that
			wanted 100 is 70 mm short - and that plain number is
			exactly what the smaller side of the shared rectangle
			fails to give, because the shared room here is usually
			the whole body of the intruder.
		*/
		qreal missing = 0.0;
};

/**
	@brief How much of a rail is taken and how much of it is left.

	The rail is a length and not a bought part, which is the decision this
	whole class is shaped by: rails and ducts are cut to size on the bench,
	so what a rail knows about itself is how long it was cut and what is
	clipped onto it. Nothing here carries a product code, and the total
	length per profile that the bill of material wants is a sum over the
	whole layout, not a property of one rail.

	used is the sum of the widths of what is clipped on, and the widths are
	the declared ones. A part nobody has measured contributes nothing and
	is counted in unknown_width_count instead - so a rail can read as
	having room left while the answer rests on a measurement nobody took,
	and isConclusive is what says so. A rail reported as roomy because
	three unmeasured breakers counted as zero is the one way this
	arithmetic can lie.
*/
class MountingRailFill
{
	public:
			/// how long the rail was cut, millimetre
		qreal length = 0.0;
			/// the widths of what is clipped on, added up, millimetre
		qreal used = 0.0;
			/// how many parts are clipped on
		int item_count = 0;
			/// how many of them have no width anybody typed
		int unknown_width_count = 0;

			/// @return true when the rail has a length at all
		bool isMeasured() const;
			/// @return what is left of it, millimetre; negative when it is over
		qreal free() const;
			/// @return by how much it is over, zero when it is not
		qreal overrun() const;
			/// @return true when more is clipped on than the rail is long
		bool isOverfilled() const;
			/// @return true when every part on it has a width
		bool isConclusive() const;
			/**
				@return how much of the rail is taken: 1 is
				full, more than 1 is overfilled, and 0 is what
				an uncut rail answers because nobody has said
				what it would be a fraction of
			*/
		qreal ratio() const;
};

/**
	@brief What became of one part when the surface it is on was checked.

	Borrows MountingFit rather than answering true or false, for the reason
	MountingFit exists: a part which would fit somewhere else on the plate
	needs dragging, and a part wider than the plate needs another
	enclosure, and telling the person the first when it is the second
	wastes an afternoon.
*/
class MountingItemFit
{
	public:
			/// which part
		QString uuid;
			/// where it stands with the surface it is on
		MountingFit fit = MountingFit::NoArea;
			/// nobody typed this part's dimensions - see MountedItem
		bool size_unknown = false;

			/// @return true when it is where it can be
		bool isFitting() const;
};

/**
	@brief Everything wrong with one mounting surface, before a hole is
	drilled in it.

	Three answers in one report, because they are asked at the same moment
	and by the same person: do two parts want the same room, does every
	part have the air it asked for, and is everything on the plate at all.
	The three lists are kept apart rather than merged into one list of
	complaints, since each one is shown differently and counted
	differently.

	isClean is not the same as isConclusive, and the difference is the
	honest part. A plate whose parts nobody has measured comes back clean,
	because a part with no dimensions cannot be shown to collide with
	anything - and that clean answer is worth exactly what the measurements
	behind it are worth. Whoever shows this report has to show the second
	question too, otherwise the panel promises a fit it never verified.
*/
class MountingSurfaceReport
{
	public:
			/// which face was checked
		QString surface_uuid;
			/// the room that face has, millimetre
		MountingArea area;
			/// pairs of parts whose bodies want the same room
		QList<MountingOverlap> overlaps;
			/// parts standing in the air another part asked for
		QList<MountingEncroachment> encroachments;
			/// one entry per part, none of them dropped
		QList<MountingItemFit> fits;
			/// how many parts have no dimensions anybody typed
		int unknown_size_count = 0;
			/// how many parts were checked against no clearance at all
		int unknown_clearance_count = 0;

			/// @return how many parts were looked at
		int itemCount() const;
			/// @return how many parts are not where they can be
		int misfitCount() const;
			/// @return every complaint added up
		int issueCount() const;
			/// @return true when nothing was found against this face
		bool isClean() const;
		/**
			@return true when no part of the answer rests on a
			measurement nobody took.

			False when a part has no dimensions, when a part was
			checked against no clearance, or when the face itself has
			no usable area. Read together with isClean, never instead
			of it.
		*/
		bool isConclusive() const;
			/// @return the parts that are not where they can be, in order
		QList<MountingItemFit> misfits() const;
			/// @return the identifier of every part named in a complaint
		QStringList complainedAbout() const;
};

/**
	@brief The three questions a mounting surface is asked before the sheet
	metal is cut.

	Static and pure, the same family as EnclosureTransfer and
	MountingMeasure: QRectF, QString and arithmetic, with no scene, no
	project and no graphics item. That is what lets "these two parts
	collide" and "this drive is 70 mm short of air" be proved on a bench
	instead of by looking at a screen, which is the only way a geometry
	rule ever gets checked twice.

	Everything is in the frame MountingArea declares - millimetre, x
	running right, y running down, origin at the top left corner of the
	mounting surface. No factor is applied to anything on the way in or on
	the way out.

	Clearance arrives keyed by product code and not by part instance,
	because that is where the number lives: the air a contactor needs is a
	property of the contactor and not of the seventh one somebody dropped
	onto the plate. A part with no product code, or with a code the caller
	had nothing for, is checked against no clearance and counted as such.

	@par What this rule deliberately does not check

	Air that falls off the edge of the plate is not a violation. A part
	flush with the top of the mounting plate, asking for 100 mm above it,
	very often has that room: the plate is not the enclosure, and what is
	above the plate belongs to the enclosure, which this rule was refused
	knowledge of on purpose - see MountingArea. Reporting it would be this
	file inventing a wall.

	Rail neighbours are not told apart from anything else, and this one is a
	limit and not a decision. Breakers are clipped shoulder to shoulder by
	design, so a clearance recorded against a breaker is reported against
	the breaker beside it - correctly, by the arithmetic, and uselessly, to
	the person reading it. What would fix it is knowing which rail a part is
	clipped to, and nothing in the stored layout says that yet: the rail
	arrives with the editor. Until then the honest statement is that the air
	recorded on a part is the air the rule will ask for wherever that part
	stands.
*/
class MountingCheck
{
	public:
		/**
			@brief Which parts want the same room as which.
			@param items everything mounted on one surface
			@return one entry per colliding pair, in the order the
			items were handed over

			Touching is not colliding, and that is not a detail: a
			rail is cut to the edge of the plate and two 22.5 mm
			breakers are clipped at 0 and at 22.5 with nothing
			between them. Compared with MountingArea::tolerance() of
			slack for that reason, and for the other one - a sum of
			millimetres in a double lands a nanometre away from where
			the arithmetic says, and a panel that reports two flush
			breakers as overlapping is a panel nobody reads twice.

			A part whose dimensions nobody typed collides with
			nothing, since its body is a point. A part whose position
			is not a number collides with nothing either: two parts
			cannot be said to share room when nobody knows where one
			of them is. Both cases are counted in the report instead.
		*/
		static QList<MountingOverlap> overlaps(const QList<MountedItem> &items);

		/**
			@brief Which parts stand in air another part asked for.
			@param items everything mounted on one surface
			@param clearances what each product code asks for, keyed
			by code
			@return one entry per starved claim, claimant first

			Returns nothing when no clearance is asked for, which is
			the state of every project today: the arithmetic runs,
			finds no requirement and reports no violation, rather
			than refusing to run until the numbers are decided.

			The slack of MountingArea::tolerance() is a nanometre of
			arithmetic and is used here as such. It is never used as
			clearance: shrinking every requirement by a nanometre
			would be harmless, and calling a nanometre a clearance
			would not, so the two words stay apart - see
			MountingArea::tolerance.
		*/
		static QList<MountingEncroachment>
		encroachments(const QList<MountedItem> &items,
			      const QHash<QString, MountingClearance> &clearances);

		/**
			@brief How much of a rail is taken by what is clipped
			onto it.
			@param items the parts clipped onto that rail
			@param rail_length how long the rail was cut, millimetre
			@return the sum, the room left, and what the answer rests
			on

			Widths added up, not rectangles unioned: what is clipped
			onto a rail stands side by side along it, and two parts
			at the same coordinate are an overlap, which is the other
			question. Deliberately separate, so that a rail read as
			full and a rail with two parts on top of one another stay
			two different sentences.
		*/
		static MountingRailFill railFill(const QList<MountedItem> &items,
						 qreal rail_length);

		/**
			@brief Whether each part is on the surface at all.
			@param items everything mounted on one surface
			@param area the room that surface has
			@return one entry per part, in the order they were handed
			over, none of them dropped

			The arithmetic is not written here: it is
			MountingArea::holds and canEverHold, reached through
			EnclosureTransfer::fitOf, so that a part which fits when
			the enclosure is swapped is a part which fits when the
			panel is checked. Two answers to that question would be
			one answer too many.
		*/
		static QList<MountingItemFit> fits(const QList<MountedItem> &items,
						   const MountingArea &area);

		/**
			@brief The three questions asked of one face of one
			enclosure.
			@param surface the face and everything screwed to it
			@param clearances what each product code asks for, keyed
			by code
			@return the whole report, empty complaints included

			The one call the panel makes. It reads the face's own
			area and the face's own items, which is what makes this
			rule a consumer of the stored layout rather than a second
			place where a plate is described.
		*/
		static MountingSurfaceReport
		surfaceReport(const MountingSurface &surface,
			      const QHash<QString, MountingClearance> &clearances
				      = QHash<QString, MountingClearance>());
};

#endif // MOUNTINGCHECK_H
