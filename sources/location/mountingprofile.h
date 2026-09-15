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
#ifndef MOUNTINGPROFILE_H
#define MOUNTINGPROFILE_H

#include <QCoreApplication>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QtGlobal>

/**
	@brief Which way a piece cut to length runs on the surface.

	Two directions and not an angle, and that is a decision rather than a
	simplification. A rail is screwed to a plate along one of its two edges
	because that is how a plate is drilled and how a panel is wired; a rail
	at seventeen degrees is not a panel, it is a mistake nobody would cut.
	Keeping it to two also keeps the footprint a rectangle, which is what
	every rule underneath - the fit, the overlap, the clearance - is written
	over. The day something really does sit at an angle, it arrives with its
	own rotation and its own bounding box, and this enumeration is not the
	thing that has to change.

	The names are those of the frame MountingArea declares: x runs right and
	y runs DOWN, so Across is along x and Down is along y.
*/
enum class MountingRun
{
	Across, ///< along x, left to right
	Down    ///< along y, top to bottom
};

/**
	@brief Which end of a piece is being pulled.

	Start is the end at the smaller coordinate - the left end of a piece
	running across, the top end of one running down - and End is the other.
	Naming them rather than passing an index is what keeps the arithmetic
	readable at the two places that use it: the handle of the drawing, and a
	length typed into a box.
*/
enum class MountingEnd
{
	Start, ///< the end at the smaller coordinate
	End    ///< the end at the larger one
};

/**
	@brief What a rail or a cable duct is, for a panel shop that cuts its
	own.

	Two numbers and a token, and no product code. That is the answer the
	house gave when it was asked whether a rail is a bought part or a cut
	material: rail and duct are cut to size, and what the material list
	needs of them is the total length used, not a manufacturer and not a
	reference. So a profile here is not a row of the catalogue - it is the
	section of the bar the piece was cut from, which is the only thing two
	pieces have to have in common to be added up into one line.

	@par Why the kind is a token and not an enumeration

	Because reading is tolerant and writing is strict, and an enumeration
	makes those two disagree: a file naming a kind this version does not
	know would read as "no kind" and be written back without it, which is
	the quiet loss this family is written against. A token survives the
	round trip whatever it says. It is the same choice MountingSurface made
	for which face of a cabinet it is, for the same reason.

	@par The section, the depth, and which of the two the drawing uses

	section is the width the piece takes ON the plate, across its run: 35 mm
	for a standard DIN rail, whatever its length. depth is how far it stands
	off the plate, which a two dimensional layout never draws and which
	somebody fitting a door very much wants to know. Only the first takes
	part in any footprint, and the asymmetry is on purpose - the day the
	third dimension is drawn, it is drawn from a number that was already
	being kept.

	@par What is deliberately not here

	A colour, a name somebody typed, a slot pitch. All three are properties
	of a product, and a product is what this is not. And no length: a length
	belongs to the piece that was cut and not to the bar it came from, and
	keeping it here would turn two pieces of one profile into two profiles.
*/
class MountingProfile
{
	Q_DECLARE_TR_FUNCTIONS(MountingProfile)

	public:
		MountingProfile() {}
		MountingProfile(const QString &profile_kind,
				qreal profile_section,
				qreal profile_depth = 0.0);

			/// @return true when this says nothing about any profile
		bool isNull() const;
			/// @return true when the file calls this a rail
		bool isRail() const;
			/// @return true when the file calls this a cable duct
		bool isDuct() const;
			/// @return true when the width it takes on the plate is known
		bool hasSection() const;

		/**
			@return the token two pieces have to share to be one line
			of the material list.

			Canonical and untranslated, because it is compared and
			not read out loud: "rail-35x7.5". It is computed rather
			than stored so that a profile cannot carry a key
			disagreeing with its own numbers - which is the one way a
			list of lengths could add two different bars together.
		*/
		QString key() const;

		/**
			@return how to call this profile out loud, never empty.

			Translated, and the numbers in it written the way the
			material list writes a length, so that a rail called
			"Rail 35 x 7,5" on a drawing is not called
			"Rail 35.000 x 7.500" in the list beside it.
		*/
		QString designation() const;

		/**
			@brief The size a piece of this profile takes.
			@param length how long the piece is, millimetre
			@param run which way it runs
			@return the room it takes on the surface, millimetre

			The whole of what "elastic" means here: one of the two
			dimensions is the cut and the other is the bar, and which
			is which is the run. Nothing else in the layout is allowed
			to decide that, because a duct whose section followed its
			length would grow wider every time somebody pulled it
			longer.
		*/
		QSizeF sizeFor(qreal length, MountingRun run) const;

		bool operator==(const MountingProfile &other) const;
		bool operator!=(const MountingProfile &other) const;

			/// @return the token a rail is written under
		static QString railKind();
			/// @return the token a cable duct is written under
		static QString ductKind();

			/// @return a rail of that section and that depth, millimetre
		static MountingProfile rail(qreal section, qreal depth = 0.0);
			/// @return a duct of that section and that depth, millimetre
		static MountingProfile duct(qreal section, qreal depth = 0.0);

		/**
			@return the rails a panel shop has on the wall.

			The sections of IEC/EN 60715, which is a standard and not
			a catalogue: nobody is named here and nothing is bought
			here. It is a starting list for a box somebody chooses
			from, and a section typed by hand is as good as any of
			them - which is what rail() is for.
		*/
		static QList<MountingProfile> standardRails();

		/**
			@return the shortest piece the drawing will cut,
			millimetre.

			A length to be able to grab hold of again, and not a
			length anything is cut at. A piece pulled down to nothing
			is a piece nobody can get hold of to pull back, so the
			stretch stops here instead - the same reasoning that gives
			an unmeasured part a marker to be seen by. No file ever
			holds this number: it bounds a gesture, and a length typed
			into a box is the person's to type.
		*/
		static qreal minimumLength();

			/// @return the size of a piece @a length long, @a section wide
		static QSizeF sizeFor(qreal length,
				      qreal section,
				      MountingRun run);
			/// @return how long a piece of that size is, along its run
		static qreal lengthOf(const QSizeF &size, MountingRun run);
			/// @return how wide a piece of that size is, across its run
		static qreal sectionOf(const QSizeF &size, MountingRun run);

		/**
			@brief The footprint of a piece, from a corner and a
			length.
			@param position the top left corner of it, millimetre
			@param length how long it is, millimetre
			@param section how wide it is across its run, millimetre
			@param run which way it runs
			@return the room it takes, millimetre, in the frame
			MountingArea declares
		*/
		static QRectF footprintFor(const QPointF &position,
					   qreal length,
					   qreal section,
					   MountingRun run);

		/**
			@brief One end of a piece pulled to a coordinate.
			@param footprint the room the piece takes now, millimetre
			@param run which way it runs
			@param end which end is being pulled
			@param coordinate_mm where that end goes, along the run
			@return the room it takes afterwards

			The arithmetic of stretching, and it is here rather than
			in the item that draws it for the reason the dimension
			arithmetic is in MountingMeasure: what can be got wrong is
			the arithmetic and the frame it is done in, and neither
			needs anything drawn to be proved.

			Three things it will not do. It will not move the end
			nobody is pulling, so the other end of a rail stays where
			it was screwed. It will not turn the piece inside out: an
			end pulled past the other stops at minimumLength(),
			because a rail of minus fifty millimetres is not a report,
			it is a drawing nobody can fix. And it will not touch the
			section: a duct pulled longer does not get wider.

			A coordinate that is not a number leaves the piece exactly
			as it was. The tolerant repair belongs here, at the edge
			where a mouse or a file hands a number in.
		*/
		static QRectF stretch(const QRectF &footprint,
				      MountingRun run,
				      MountingEnd end,
				      qreal coordinate_mm);

			/// the token saying what this is: a rail, a duct
		QString kind;
			/// the width it takes on the plate, across its run, millimetre
		qreal section = 0.0;
			/// how far it stands off the plate, millimetre, 0 when unknown
		qreal depth = 0.0;
};

/**
	@brief One line of the material list, for what is cut and not counted.

	A profile, the length of it that was used, and how many pieces that
	length was cut into. The three travel together because the storeroom is
	asked for one of them and the workshop for another: twelve metres of
	rail is what is bought, eight pieces is what is cut - and a line
	carrying only the first is a line nobody can check against a drawing.

	uncut is the fourth number and the one that must not be folded into the
	others. It counts the pieces of this profile that nobody has given a
	length to, and it exists so that they do not read as pieces of zero
	metres: a list that quietly added nothing for them would say the panel
	needs less rail than it does, which is the error that is only found when
	the bar runs out. Zero is a length; not knowing is not.

	Lengths here are in millimetre, like everything else in this family.
	metres() is the one conversion, written once, because the day two places
	divide by a thousand is the day they disagree.
*/
class MountingProfileTotal
{
	public:
			/// what was cut
		MountingProfile profile;
			/// how much of it was used, millimetre
		qreal length = 0.0;
			/// how many pieces that is
		int pieces = 0;
			/// how many of those pieces nobody has given a length to
		int uncut = 0;

			/// @return the length in the unit a material list prints
		qreal metres() const;
};

#endif // MOUNTINGPROFILE_H
