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
#include "drillingtable.h"

#include "../utils/csvwriter.h"
#include "bommeasure.h"
#include "mountingmeasure.h"

#include <QMap>

#include <cmath>

namespace
{
	/**
		@brief isUsableSize
		@param length a length in millimetre
		@return true when somebody actually measured it

		A zero is refused here and not in MountingMeasure::isLength,
		which answers that zero is a length and is right to: two points
		may sit on top of one another. A hole may not. Nothing is
		drilled at zero millimetre, so a zero in this file is what an
		unfilled field arrives as and never what a person typed.
	*/
	bool isUsableSize(qreal length)
	{
		return MountingMeasure::isLength(length)
		       && !MountingMeasure::isSameLength(length, 0.0);
	}

	/**
		@brief keyNumber
		@param value a length in millimetre
		@return the number as a grouping token holds it

		Twelve significant digits and no locale, the same spelling
		MountingProfile::key uses. This is compared and never shown.
	*/
	QString keyNumber(qreal value)
	{
		return QString::number(value, 'g', 12);
	}

	/**
		@brief shownNumber
		@param value a length in millimetre
		@param locale the locale whose decimal separator to use
		@return the number as this program writes a millimetre

		Borrowed from BomMeasure and not written again, for the reason
		MountingProfile borrows it: the drilling table and the material
		list are read side by side, and two hands writing millimetres
		would disagree about the decimal separator, about how many
		decimals survive and about whether a whole number keeps its
		zeros.

		A negative number is written out as it is and is not an error
		here, which a size would be: a coordinate above the origin of
		the surface has a negative y in the frame this works in, and
		hiding that would hide a hole placed off the plate.
	*/
	QString shownNumber(qreal value, const QLocale &locale)
	{
		if (!std::isfinite(value)) {
			return QStringLiteral("?");
		}
		return BomMeasure::formatQuantity(value,
						  QStringLiteral("mm"),
						  locale);
	}
}

/**
	@brief DrillingHole::drilled
	@param centre where to punch it
	@param hole_diameter through the metal, millimetre
	@param hole_purpose what it is for
	@return the hole
*/
DrillingHole DrillingHole::drilled(const QPointF &centre,
				   qreal hole_diameter,
				   const QString &hole_purpose)
{
	DrillingHole hole;
	hole.position = centre;
	hole.shape = DrillingShape::Round;
	hole.diameter = hole_diameter;
	hole.purpose = hole_purpose;
	return hole;
}

/**
	@brief DrillingHole::cutOut
	@param centre the middle of the opening
	@param cutout_size how wide and how tall, millimetre
	@param hole_purpose what it is for
	@return the cut-out
*/
DrillingHole DrillingHole::cutOut(const QPointF &centre,
				  const QSizeF &cutout_size,
				  const QString &hole_purpose)
{
	DrillingHole hole;
	hole.position = centre;
	hole.shape = DrillingShape::Rectangular;
	hole.cutout = cutout_size;
	hole.purpose = hole_purpose;
	return hole;
}

/**
	@brief DrillingHole::isNull
	@return true when nothing here describes a hole

	A position is not enough to make one: the origin of every mounting
	surface is (0, 0), so a default built record and a hole at the corner
	of the plate would be the same thing if position counted.
*/
bool DrillingHole::isNull() const
{
	return !hasSize()
	       && purpose.isEmpty()
	       && component.isEmpty()
	       && component_uuid.isEmpty();
}

/**
	@brief DrillingHole::hasSize
	@return true when the size is a real measurement

	A cut-out needs both of its numbers. Half of a cut-out is not half an
	answer, it is a record somebody started and did not finish - the same
	reading MountingPartView gives half an insertion axis.
*/
bool DrillingHole::hasSize() const
{
	if (shape == DrillingShape::Round) {
		return isUsableSize(diameter);
	}
	return isUsableSize(cutout.width()) && isUsableSize(cutout.height());
}

/**
	@brief DrillingHole::size
	@return the room it takes, anything unmeasured folded to zero
*/
QSizeF DrillingHole::size() const
{
	if (!hasSize()) {
		return QSizeF(0.0, 0.0);
	}
	if (shape == DrillingShape::Round) {
		return QSizeF(diameter, diameter);
	}
	return cutout;
}

/**
	@brief DrillingHole::boundingRect
	@return the extents on the surface, centred on position

	The one halving of the size in this file, so that the drawing, the fit
	and the table cannot each do their own and disagree in the last
	millimetre.
*/
QRectF DrillingHole::boundingRect() const
{
	const QSizeF extents = size();
	const QPointF corner(position.x() - (extents.width() / 2.0),
			     position.y() - (extents.height() / 2.0));
	return QRectF(corner, extents);
}

/**
	@brief DrillingHole::designation
	@return how to call the component this hole belongs to, never empty
*/
QString DrillingHole::designation() const
{
	if (!component.isEmpty()) {
		return component;
	}
	if (!component_uuid.isEmpty()) {
		return component_uuid;
	}
	return tr("aucun composant");
}

/**
	@brief DrillingHole::sizeText
	@param locale the locale whose decimal separator to use
	@return the size as the table prints it
*/
QString DrillingHole::sizeText(const QLocale &locale) const
{
	return DrillingTable::sizeText(shape, diameter, cutout, locale);
}

/**
	@brief DrillingHole::toolKey
	@return the token that groups this hole with the ones made by the same
	tool

	Shape first, so that an unmeasured round hole and an unmeasured
	cut-out never fall into one line: they are two different things
	nobody has measured, and the workshop has to be told about both.
*/
QString DrillingHole::toolKey() const
{
	const QString prefix = (shape == DrillingShape::Round)
			       ? QStringLiteral("d")
			       : QStringLiteral("r");
	if (!hasSize()) {
		return prefix + QStringLiteral("?");
	}
	if (shape == DrillingShape::Round) {
		return prefix + keyNumber(diameter);
	}
	return prefix + keyNumber(cutout.width())
	       + QStringLiteral("x") + keyNumber(cutout.height());
}

/**
	@brief DrillingToolTotal::sizeText
	@param locale the locale whose decimal separator to use
	@return the size as a list prints it
*/
QString DrillingToolTotal::sizeText(const QLocale &locale) const
{
	if (!measured) {
		return DrillingTable::sizeText(shape, 0.0, QSizeF(), locale);
	}
	return DrillingTable::sizeText(shape, diameter, cutout, locale);
}

/**
	@brief DrillingTable::referenceFrameText
	@return the frame the coordinates are stated in

	It holds a semicolon of its own, and that is not an accident of
	punctuation worth removing: it is the first line of every file this
	writes, so the rule that keeps a cell whole is exercised by the file
	header before any hole is in it.
*/
QString DrillingTable::referenceFrameText()
{
	return tr("Origine : coin supérieur gauche de la surface de montage ; "
		  "x vers la droite, y vers le bas ; cotes en millimètres.");
}

/**
	@brief DrillingTable::header
	@return the column names, in order
*/
QStringList DrillingTable::header()
{
	QStringList names;
	names << tr("Repère")
	      << tr("X (mm)")
	      << tr("Y (mm)")
	      << tr("Type")
	      << tr("Dimension")
	      << tr("Destination");
	return names;
}

/**
	@brief DrillingTable::columnCount
	@return how many cells every line of the text has

	Read from header() rather than written down, so that a column added
	to the table cannot leave the frame line one cell short.
*/
int DrillingTable::columnCount()
{
	return static_cast<int>(header().size());
}

/**
	@brief DrillingTable::row
	@param hole the hole
	@param locale the locale whose decimal separator to use
	@return the cells, raw and unquoted, in column order
*/
QStringList DrillingTable::row(const DrillingHole &hole, const QLocale &locale)
{
	QStringList cells;
	cells << hole.designation()
	      << shownNumber(hole.position.x(), locale)
	      << shownNumber(hole.position.y(), locale)
	      << ((hole.shape == DrillingShape::Round) ? tr("Perçage")
							: tr("Découpe"))
	      << hole.sizeText(locale)
	      << hole.purpose;
	return cells;
}

/**
	@brief DrillingTable::toDelimitedText
	@param holes the holes, in the order they are to be drilled
	@param separator the delimiter between cells
	@param locale the locale whose decimal separator to use
	@return the frame line, the header line and one line per hole
*/
QString DrillingTable::toDelimitedText(const QList<DrillingHole> &holes,
				       const QString &separator,
				       const QLocale &locale)
{
	QStringList lines;

		//The frame, padded to the width of the table. A ragged first
		//row is what a spreadsheet shows when a title is written as one
		//cell, and it costs the reader the one invariant worth having
		//over this file: every line has the same number of cells.
	QStringList frame;
	frame << referenceFrameText();
	while (frame.size() < columnCount()) {
		frame << QString();
	}
	lines << QETCsv::row(frame, separator);

	lines << QETCsv::row(header(), separator);

	for (const DrillingHole &hole : holes) {
		lines << QETCsv::row(DrillingTable::row(hole, locale),
				     separator);
	}

	return lines.join(QStringLiteral("\n"));
}

/**
	@brief DrillingTable::toolTotals
	@param holes the holes
	@return one line per tool, ordered by key

	A QMap and not a QHash, so the order of the answer is the order of the
	keys and not the order of a hash function: a summary that comes out
	shuffled between two runs cannot be compared with the one that was
	printed yesterday.
*/
QList<DrillingToolTotal> DrillingTable::toolTotals(const QList<DrillingHole> &holes)
{
	QMap<QString, DrillingToolTotal> totals;

	for (const DrillingHole &hole : holes)
	{
		const QString key = hole.toolKey();
		if (!totals.contains(key))
		{
			DrillingToolTotal total;
			total.key = key;
			total.shape = hole.shape;
			total.measured = hole.hasSize();
			if (total.measured)
			{
				total.diameter = hole.diameter;
				total.cutout = hole.cutout;
			}
			totals.insert(key, total);
		}
		++ totals[key].count;
	}

	return totals.values();
}

/**
	@brief DrillingTable::fitOf
	@param hole the hole
	@param area the surface it is to be made in
	@return Fits, OutsideArea, LargerThanArea or NoArea
*/
MountingFit DrillingTable::fitOf(const DrillingHole &hole,
				 const MountingArea &area)
{
	if (!area.isValid()) {
		return MountingFit::NoArea;
	}

		//Nothing to be too large about: judged on its centre, which is
		//the most that can honestly be said about a hole nobody has
		//measured.
	if (!hole.hasSize()) {
		return MountingMeasure::fitOfCoordinate(hole.position, area);
	}

	if (area.holds(hole.boundingRect())) {
		return MountingFit::Fits;
	}
	return area.canEverHold(hole.size()) ? MountingFit::OutsideArea
					     : MountingFit::LargerThanArea;
}

/**
	@brief DrillingTable::sizeText
	@param shape drilled or cut out
	@param hole_diameter millimetre, for a round hole
	@param hole_cutout millimetre, for a cut-out
	@param locale the locale whose decimal separator to use
	@return the size as this program writes it
*/
QString DrillingTable::sizeText(DrillingShape shape,
				qreal hole_diameter,
				const QSizeF &hole_cutout,
				const QLocale &locale)
{
	if (shape == DrillingShape::Round)
	{
		if (!isUsableSize(hole_diameter)) {
			return tr("non mesuré");
		}
		return tr("Ø %1").arg(shownNumber(hole_diameter, locale));
	}

	if (!isUsableSize(hole_cutout.width())
	    || !isUsableSize(hole_cutout.height())) {
		return tr("non mesuré");
	}
	return tr("%1 x %2").arg(shownNumber(hole_cutout.width(), locale),
				 shownNumber(hole_cutout.height(), locale));
}
