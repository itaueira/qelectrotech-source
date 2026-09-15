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
#ifndef DRILLINGTABLE_H
#define DRILLINGTABLE_H

#include "enclosuretransfer.h"

#include <QCoreApplication>
#include <QList>
#include <QLocale>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
	@brief What has to be made in the sheet metal, and how it is named.

	Two and not more, because the workshop has two tools and one of them
	is a drill. A round hole is named by one number and made by picking a
	bit; anything with corners is named by two numbers and made by
	cutting. A shape that cannot say which of the two it is would have to
	be read off the drawing, and reading it off the drawing is exactly
	what a drilling table exists to stop.
*/
enum class DrillingShape
{
	Round,      ///< drilled through, named by its diameter
	Rectangular ///< cut out, named by its width and its height
};

/**
	@brief One hole or one cut-out in a mounting surface.

	The four things the workshop is owed about a hole, and nothing else:
	where it is, how big it is, what it is for, and which component it
	belongs to. Everything else about the panel - what the component is,
	what it costs, which folio draws it - is somebody else's list.

	@par The frame, which is not invented here

	position is in the frame MountingArea declares and this file does not
	redeclare: millimetre, x running right, y running DOWN, origin at the
	top left corner of the mounting surface. It is written down in one
	place, enclosuretransfer.h, and read from there by everything that
	measures - the arithmetic of MountingMeasure, the enclosure transfer,
	and this. A table that declared its own would be a second convention,
	and the failure of a drilling table with two conventions is that every
	number in it is plausible.

	@par The anchor inside the hole is the centre

	That part is new here, because a hole has no top left corner anybody
	can punch. A round hole has a centre and the workshop marks it; a
	rectangular cut-out has a centre too, and a cut-out dimensioned from a
	corner changes its coordinate the day somebody makes it 2 mm bigger,
	while one dimensioned from its centre does not. boundingRect() is how
	the extents are had when they are wanted, so that the two readings can
	never drift apart by each doing its own halving.

	@par purpose is the field this whole step was written around

	It is free text a person types - "passage de câble ; côté gauche" -
	and it is therefore the cell that holds the separator of whatever
	delimited file it goes into. Nothing in this class escapes anything:
	the escaping is QETCsv, once, at the moment of writing, which is the
	only place that knows what the separator is.
*/
class DrillingHole
{
	Q_DECLARE_TR_FUNCTIONS(DrillingHole)

	public:
		DrillingHole() {}

		/**
			@brief A round hole.
			@param centre where to punch it, in the frame above
			@param hole_diameter through the metal, millimetre
			@param hole_purpose what it is for, free text
		*/
		static DrillingHole drilled(const QPointF &centre,
					    qreal hole_diameter,
					    const QString &hole_purpose = QString());

		/**
			@brief A rectangular cut-out.
			@param centre the middle of the opening, in the frame above
			@param cutout_size how wide and how tall it is, millimetre
			@param hole_purpose what it is for, free text
		*/
		static DrillingHole cutOut(const QPointF &centre,
					   const QSizeF &cutout_size,
					   const QString &hole_purpose = QString());

			/// @return true when nothing here describes a hole
		bool isNull() const;

		/**
			@return true when the size of this hole is a real
			measurement.

			False covers the two states that must not be confused
			with a hole of zero: a diameter nobody has filled in, and
			a cut-out with one of its two numbers missing. Both stay
			in the table and are reported as unmeasured rather than
			being dropped or given an invented size - a hole silently
			left out of the list is a hole the panel will not have.
		*/
		bool hasSize() const;

		/**
			@return the room this hole takes, millimetre, with
			anything that is not a measurement folded to zero.

			A round hole answers its diameter both ways, which is
			what makes one bounding rectangle serve both shapes.
		*/
		QSizeF size() const;

			/// @return the extents on the surface, centred on position
		QRectF boundingRect() const;

		/**
			@return how to call the component this hole belongs to,
			never empty.

			The same ladder MountedItem::designation climbs, for the
			same reason: a row of the table that names nothing points
			the workshop at nothing. A hole that belongs to no
			component - a cable gland, a fixing hole for a duct that
			was cut on the bench - is honestly nameless, and says so.
		*/
		QString designation() const;

			/// @return the size as the table prints it
		QString sizeText(const QLocale &locale = QLocale()) const;

		/**
			@return the token that groups this hole with the holes
			made by the same tool.

			Not translated and not localised, for the reason
			MountingProfile::key is not: it is compared and never
			shown, and a key whose decimal separator followed the
			interface language would split one drill bit into two
			lines the day somebody changed languages.
		*/
		QString toolKey() const;

			/// the centre, millimetre, in the frame above
		QPointF position;
			/// drilled or cut out
		DrillingShape shape = DrillingShape::Round;
			/// through the metal, millimetre; meaningless for a cut-out
		qreal diameter = 0.0;
			/// the opening, millimetre; meaningless for a round hole
		QSizeF cutout;
			/// what it is for, as a person typed it
		QString purpose;
			/// what the folio calls the component: -Q1
		QString component;
			/// identity of that component, so a row can be pointed back
		QString component_uuid;
};

/**
	@brief How many holes one tool makes on one surface.

	The question the workshop asks before it starts and not while it is
	drilling: which bits to fit, and how many times each. It is a count
	and never a length, so it prints as a whole number.

	Every hole handed over lands in exactly one of these lines, including
	the ones nobody has measured - they group together, by shape, under a
	key of their own. That is what makes the counts add up to the number
	of holes, and the sum is worth checking rather than the number of
	lines: a hole lost between two groups leaves the number of groups
	exactly as it was.
*/
class DrillingToolTotal
{
	public:
			/// the token holes were grouped by, see DrillingHole::toolKey
		QString key;
			/// drilled or cut out
		DrillingShape shape = DrillingShape::Round;
			/// the diameter of the group, millimetre; zero when unmeasured
		qreal diameter = 0.0;
			/// the opening of the group, millimetre; null when unmeasured
		QSizeF cutout;
			/// false when this is the group of what nobody has measured
		bool measured = false;
			/// how many holes of this group there are
		int count = 0;

			/// @return the size as a list prints it
		QString sizeText(const QLocale &locale = QLocale()) const;
};

/**
	@brief The drilling table: the rows the workshop drills from, and the
	delimited text they are handed over as.

	Static and pure, the same family as MountingMeasure and
	EnclosureTransfer: no scene, no project, no graphics item. What can be
	got wrong here is the arithmetic, the frame it is stated in and the
	writing of a cell, and none of the three needs anything drawn to be
	proved.

	@par Why the frame is written into the file

	The first line of the emitted text is the frame of
	MountingArea, spelled out. Without it the same pair of numbers admits
	four readings - the origin could be any of the four corners - and a
	workshop that guesses wrong drills a panel that is a mirror of the one
	that was drawn. The drawing prints the same sentence, from
	referenceFrameText(), so that the two cannot say different things.

	@par Why every line has the same number of cells

	Including the frame line, which is one sentence padded with empty
	cells. A file whose lines have different widths opens in a spreadsheet
	and reads as a table with a ragged first row, and a reader that checks
	its own input has nothing to check against. Padding costs a few
	separators and buys one invariant that can be asserted over the whole
	file: every line, header and frame included, has columnCount() cells.

	@par Nothing here escapes anything

	The cells go out through QETCsv::field and QETCsv::row, with the
	separator the caller passed, which is the one copy of that rule in the
	program. That matters twice over here, because the frame sentence
	itself holds a semicolon: the line that declares the frame is the
	first line of the file that would shift its own columns.
*/
class DrillingTable
{
	Q_DECLARE_TR_FUNCTIONS(DrillingTable)

	public:
		/**
			@return the frame the coordinates are stated in, as a
			sentence for a person to read.

			The words are here; the decision is not. It is
			MountingArea's, at enclosuretransfer.h, and this sentence
			says what that file says and adds nothing to it.
		*/
		static QString referenceFrameText();

			/// @return the column names, in order
		static QStringList header();
			/// @return how many cells every line of the text has
		static int columnCount();

		/**
			@brief One hole as a row of cells.
			@param hole the hole
			@param locale the locale whose decimal separator to use
			@return the cells, raw and unquoted, in column order

			Raw on purpose: quoting is done when the cells are
			joined, because that is the moment the separator is
			known. A caller that puts these into a table widget
			wants them unquoted, and a caller that writes a file
			calls toDelimitedText instead of quoting them itself.
		*/
		static QStringList row(const DrillingHole &hole,
				       const QLocale &locale = QLocale());

		/**
			@brief The whole table as delimited text.
			@param holes the holes, in the order they are to be drilled
			@param separator the delimiter between cells
			@param locale the locale whose decimal separator to use
			@return the frame line, the header line and one line per
			hole, joined by "\n", without a trailing one

			The end of line is "\n" and the last line carries none,
			which is what the lists of this program already do - see
			LocationBomDialog::asText. Whoever writes the file adds
			what it needs.

			A locale and a separator that disagree are not refused,
			because refusing them would mean this function deciding
			what a person may export: a French locale with a comma
			separator writes "120,5" into a comma separated file, and
			the quoting is what keeps that row whole. It is quoted by
			the same rule that quotes the purpose, and for the same
			reason.
		*/
		static QString toDelimitedText(const QList<DrillingHole> &holes,
					       const QString &separator = QStringLiteral(";"),
					       const QLocale &locale = QLocale());

		/**
			@brief How many holes each tool makes.
			@param holes the holes
			@return one line per tool, ordered by key so the answer
			does not depend on the order the holes came in

			Nothing is dropped and nothing is counted twice: the
			counts add up to holes.size(), always, and a hole nobody
			has measured is counted in the group of its own shape
			rather than left out of the summary.
		*/
		static QList<DrillingToolTotal> toolTotals(const QList<DrillingHole> &holes);

		/**
			@brief Whether a hole is in the metal at all.
			@param hole the hole
			@param area the surface it is to be made in
			@return Fits, OutsideArea, LargerThanArea or NoArea

			The question MountingMeasure::fitOfCoordinate could not
			answer and said so: a coordinate has no size, so it can
			never be LargerThanArea, while a hole has one and a
			200 mm cut-out in a 150 mm door is a different problem
			from a cut-out placed off the edge. The first needs
			another door, the second needs dragging.

			A hole nobody has measured is judged on its centre alone,
			which is the most this can honestly say about it.
		*/
		static MountingFit fitOf(const DrillingHole &hole,
					 const MountingArea &area);

		/**
			@brief The size of a hole, as text.
			@param shape drilled or cut out
			@param hole_diameter millimetre, for a round hole
			@param hole_cutout millimetre, for a cut-out
			@param locale the locale whose decimal separator to use
			@return "Ø 22", "92 x 92", or the words for unmeasured

			Public, and one address, because a hole and a group of
			holes both print it and two spellings of the same
			diameter in the same document read as two diameters.

			The number goes through BomMeasure, not through a format
			written here: the drilling table and the material list
			print millimetres in the same document, and a rail of
			"2,5 m" beside a hole of "22.5" is the same defect as two
			conventions for the origin.
		*/
		static QString sizeText(DrillingShape shape,
					qreal hole_diameter,
					const QSizeF &hole_cutout,
					const QLocale &locale = QLocale());
};

#endif // DRILLINGTABLE_H
