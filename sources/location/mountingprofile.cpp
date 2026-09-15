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
#include "mountingprofile.h"

#include "bommeasure.h"
#include "mountingmeasure.h"

#include <cmath>

namespace
{
	/// millimetre: the shortest piece a gesture may leave behind
	const qreal MINIMUM_LENGTH = 10.0;

	/// millimetre per metre, the one place this division is written
	const qreal MILLIMETRE_PER_METRE = 1000.0;

	/**
		@brief isUsableLength
		@param length a length in millimetre
		@return true when there is something of it

		Composed of the two answers this family already has rather than
		written as a third one: it is a length, and it is not the same
		length as nothing. Asking it this way is what keeps a profile of
		zero section and an unmeasured plate reading as the same state
		everywhere - the day the slack changes, it changes here too,
		because it is not repeated here.
	*/
	bool isUsableLength(qreal length)
	{
		return MountingMeasure::isLength(length)
		       && !MountingMeasure::isSameLength(length, 0.0);
	}

	/**
		@brief key
		@param value a length in millimetre
		@return the number as a key holds it

		Twelve significant digits and no locale: this goes into a token
		that is compared, never shown. A separator that followed the
		interface language would make the same bar into two lines of the
		material list the day somebody changed languages.
	*/
	QString keyNumber(qreal value)
	{
		return QString::number(value, 'g', 12);
	}

	/**
		@brief shownNumber
		@param value a length in millimetre
		@return the number as the material list writes one

		Borrowed from BomMeasure and not written again. A profile is a
		line measured by length - that is the whole reason
		BomMeasure::lengthClassKey() exists - so the number beside its
		name has to be written by the same hand that writes the number
		in the list, decimal separator included.
	*/
	QString shownNumber(qreal value)
	{
		return BomMeasure::formatQuantity(value,
						  BomMeasure::defaultLengthUnit());
	}

	/// @return true when the two lengths say the same thing
	bool sameLength(qreal before, qreal after)
	{
		if (!std::isfinite(before) || !std::isfinite(after)) {
			return !std::isfinite(before) && !std::isfinite(after);
		}
		return MountingMeasure::isSameLength(before, after);
	}
}

/**
	@brief MountingProfile::MountingProfile
	@param profile_kind the token saying what this is
	@param profile_section the width it takes on the plate, millimetre
	@param profile_depth how far it stands off the plate, millimetre
*/
MountingProfile::MountingProfile(const QString &profile_kind,
				 qreal profile_section,
				 qreal profile_depth) :
	kind(profile_kind.trimmed()),
	section(profile_section),
	depth(profile_depth)
{}

/**
	@brief MountingProfile::isNull
	@return true when this says nothing about any profile

	The kind alone answers it. A profile whose section nobody typed is still
	a rail, and it has to keep saying so: what is missing is a measurement,
	which is a state this family reports rather than a reason to forget what
	the thing is.
*/
bool MountingProfile::isNull() const
{
	return kind.trimmed().isEmpty();
}

/**
	@brief MountingProfile::isRail
	@return true when the file calls this a rail
*/
bool MountingProfile::isRail() const
{
	return kind.trimmed().compare(railKind(), Qt::CaseInsensitive) == 0;
}

/**
	@brief MountingProfile::isDuct
	@return true when the file calls this a cable duct
*/
bool MountingProfile::isDuct() const
{
	return kind.trimmed().compare(ductKind(), Qt::CaseInsensitive) == 0;
}

/**
	@brief MountingProfile::hasSection
	@return true when the width it takes on the plate is known
*/
bool MountingProfile::hasSection() const
{
	return isUsableLength(section);
}

/**
	@brief MountingProfile::key
	@return the token two pieces have to share to be one line

	The depth is in it, and that is not padding: a 35 x 7,5 rail and a
	35 x 15 rail take the same room on the plate and are two different bars
	on the shelf. Leaving the depth out would add them together into a
	length nobody can order. A depth nobody typed is left out of the key
	altogether rather than written as a zero, so that the same bar described
	twice - once with its depth, once without - does not silently become two
	lines.
*/
QString MountingProfile::key() const
{
	if (isNull()) {
		return QString();
	}

	QString token = kind.trimmed().toLower();
	if (hasSection()) {
		token += QLatin1Char('-') + keyNumber(section);
	}
	if (isUsableLength(depth)) {
		token += QLatin1Char('x') + keyNumber(depth);
	}

	return token;
}

/**
	@brief MountingProfile::designation
	@return how to call this profile out loud, never empty
*/
QString MountingProfile::designation() const
{
	if (isNull()) {
		return tr("un profilé sans type");
	}

	const bool with_depth = isUsableLength(depth);

	if (isRail())
	{
		if (!hasSection()) {
			return tr("Rail");
		}
		return with_depth
		       ? tr("Rail %1 × %2 mm").arg(shownNumber(section),
						   shownNumber(depth))
		       : tr("Rail %1 mm").arg(shownNumber(section));
	}

	if (isDuct())
	{
		if (!hasSection()) {
			return tr("Goulotte");
		}
		return with_depth
		       ? tr("Goulotte %1 × %2 mm").arg(shownNumber(section),
						       shownNumber(depth))
		       : tr("Goulotte %1 mm").arg(shownNumber(section));
	}

		//A kind this version does not know, read out loud as what the
		//file says it is. It is drawn, it is listed and it keeps its
		//name: a profile nobody here can name is still a profile
		//somebody cut.
	return hasSection()
	       ? tr("Profilé « %1 » %2 mm").arg(kind.trimmed(),
					       shownNumber(section))
	       : tr("Profilé « %1 »").arg(kind.trimmed());
}

/**
	@brief MountingProfile::sizeFor
	@param length how long the piece is, millimetre
	@param run which way it runs
	@return the room it takes on the surface, millimetre
*/
QSizeF MountingProfile::sizeFor(qreal length, MountingRun run) const
{
	return sizeFor(length, section, run);
}

bool MountingProfile::operator==(const MountingProfile &other) const
{
	return kind.trimmed().compare(other.kind.trimmed(),
				      Qt::CaseInsensitive) == 0
	       && sameLength(section, other.section)
	       && sameLength(depth, other.depth);
}

bool MountingProfile::operator!=(const MountingProfile &other) const
{
	return !(*this == other);
}

/**
	@brief MountingProfile::railKind
	@return the token a rail is written under

	Untranslated, and written here once. It goes into the .qet and is read
	back out of it, and a token whose spelling followed the interface
	language would be a token no other installation could read.
*/
QString MountingProfile::railKind()
{
	return QStringLiteral("rail");
}

/**
	@brief MountingProfile::ductKind
	@return the token a cable duct is written under
*/
QString MountingProfile::ductKind()
{
	return QStringLiteral("duct");
}

/**
	@brief MountingProfile::rail
	@param section the width it takes on the plate, millimetre
	@param depth how far it stands off it, millimetre
	@return that rail
*/
MountingProfile MountingProfile::rail(qreal section, qreal depth)
{
	return MountingProfile(railKind(), section, depth);
}

/**
	@brief MountingProfile::duct
	@param section the width it takes on the plate, millimetre
	@param depth how far it stands off it, millimetre
	@return that duct
*/
MountingProfile MountingProfile::duct(qreal section, qreal depth)
{
	return MountingProfile(ductKind(), section, depth);
}

/**
	@brief MountingProfile::standardRails
	@return the rails of IEC/EN 60715

	Four sections, no brand and no reference. The names are those of the
	standard - TH 35 is the top hat rail every panel is built on, G 32 the
	one a few older devices still ask for - and a shop that uses something
	else types its two numbers instead of choosing from here.
*/
QList<MountingProfile> MountingProfile::standardRails()
{
	QList<MountingProfile> rails;

	rails << rail(35.0, 7.5)   //TH 35-7.5
	      << rail(35.0, 15.0)  //TH 35-15
	      << rail(15.0, 5.5)   //TH 15-5.5
	      << rail(32.0, 15.0); //G 32

	return rails;
}

/**
	@brief MountingProfile::minimumLength
	@return the shortest piece the drawing will cut, millimetre
*/
qreal MountingProfile::minimumLength()
{
	return MINIMUM_LENGTH;
}

/**
	@brief MountingProfile::sizeFor
	@param length how long the piece is, millimetre
	@param section how wide it is across its run, millimetre
	@param run which way it runs
	@return the room it takes, millimetre

	Nothing is refused and nothing is repaired: a piece nobody has cut yet
	arrives with a length of zero and comes back as a size of zero, which
	every rule underneath already reads as "not measured". Folding it to a
	minimum here would be this function inventing a cut.
*/
QSizeF MountingProfile::sizeFor(qreal length, qreal section, MountingRun run)
{
	const qreal cut = MountingMeasure::isLength(length) ? length : 0.0;
	const qreal bar = isUsableLength(section) ? section : 0.0;

	return run == MountingRun::Down ? QSizeF(bar, cut) : QSizeF(cut, bar);
}

/**
	@brief MountingProfile::lengthOf
	@param size the room a piece takes, millimetre
	@param run which way it runs
	@return how long it is
*/
qreal MountingProfile::lengthOf(const QSizeF &size, MountingRun run)
{
	return run == MountingRun::Down ? size.height() : size.width();
}

/**
	@brief MountingProfile::sectionOf
	@param size the room a piece takes, millimetre
	@param run which way it runs
	@return how wide it is across its run

	The way back to a section when the profile itself does not carry one -
	a file written by a hand that gave the size and not the bar. The drawing
	asks this rather than guessing a standard rail, because a piece drawn
	35 mm wide when the file says 60 is a drawing that lies about the panel.
*/
qreal MountingProfile::sectionOf(const QSizeF &size, MountingRun run)
{
	return run == MountingRun::Down ? size.width() : size.height();
}

/**
	@brief MountingProfile::footprintFor
	@param position the top left corner of the piece, millimetre
	@param length how long it is, millimetre
	@param section how wide it is across its run, millimetre
	@param run which way it runs
	@return the room it takes, millimetre
*/
QRectF MountingProfile::footprintFor(const QPointF &position,
				     qreal length,
				     qreal section,
				     MountingRun run)
{
	return QRectF(position, sizeFor(length, section, run));
}

/**
	@brief MountingProfile::stretch
	@param footprint the room the piece takes now, millimetre
	@param run which way it runs
	@param end which end is being pulled
	@param coordinate_mm where that end goes, along the run
	@return the room it takes afterwards
*/
QRectF MountingProfile::stretch(const QRectF &footprint,
				MountingRun run,
				MountingEnd end,
				qreal coordinate_mm)
{
	if (!std::isfinite(coordinate_mm)
	    || !std::isfinite(footprint.x())
	    || !std::isfinite(footprint.y())
	    || !std::isfinite(footprint.width())
	    || !std::isfinite(footprint.height())) {
		return footprint;
	}

	const QRectF piece = footprint.normalized();
	const qreal shortest = minimumLength();

	if (run == MountingRun::Down)
	{
		qreal top    = piece.top();
		qreal bottom = piece.bottom();

		if (end == MountingEnd::Start) {
			top = qMin(coordinate_mm, bottom - shortest);
		}
		else {
			bottom = qMax(coordinate_mm, top + shortest);
		}

		return QRectF(QPointF(piece.left(), top),
			      QSizeF(piece.width(), bottom - top));
	}

	qreal left  = piece.left();
	qreal right = piece.right();

	if (end == MountingEnd::Start) {
		left = qMin(coordinate_mm, right - shortest);
	}
	else {
		right = qMax(coordinate_mm, left + shortest);
	}

	return QRectF(QPointF(left, piece.top()),
		      QSizeF(right - left, piece.height()));
}

/**
	@brief MountingProfileTotal::metres
	@return the length in the unit a material list prints
*/
qreal MountingProfileTotal::metres() const
{
	return length / MILLIMETRE_PER_METRE;
}
