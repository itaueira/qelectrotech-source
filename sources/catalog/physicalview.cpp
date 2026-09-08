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
#include "physicalview.h"

#include <cmath>

/**
	@brief CatalogMeasure::isDeclared
	@return true when the cell held a number at all
*/
bool CatalogMeasure::isDeclared() const
{
	return declared;
}

/**
	@brief CatalogMeasure::isLength
	@return true when the cell held a number and there is something of it
*/
bool CatalogMeasure::isLength() const
{
	return declared && CatalogPhysicalView::isLength(value);
}

/**
	@brief CatalogPhysicalView::keys
	@return the catalogue keys the physical view is read from
*/
QStringList CatalogPhysicalView::keys()
{
	return { QStringLiteral("width"),
		 QStringLiteral("height"),
		 QStringLiteral("depth"),
		 QStringLiteral("clearance_top"),
		 QStringLiteral("clearance_bottom"),
		 QStringLiteral("clearance_left"),
		 QStringLiteral("clearance_right"),
		 QStringLiteral("insertion_x"),
		 QStringLiteral("insertion_y"),
		 QStringLiteral("draw_outline") };
}

/**
	@brief CatalogPhysicalView::tolerance
	@return below what a length is not a length, millimetre
*/
qreal CatalogPhysicalView::tolerance()
{
	return 1e-6;
}

/**
	@brief CatalogPhysicalView::isLength
	@param length a length in millimetre
	@return true when it is a number and there is something of it

	A negative length is not a body facing the other way, it is a record
	somebody mistyped, and it is turned away here rather than at the point
	of use: a negative dimension that reached a rectangle would make that
	rectangle claim room to the left of where the part stands.
*/
bool CatalogPhysicalView::isLength(qreal length)
{
	return std::isfinite(length) && length > tolerance();
}

/**
	@brief CatalogPhysicalView::measureIn
	@param values every value of the part, the initial values of its class
	included
	@param properties the properties the class declares, by key
	@param key which measure
	@return the number and whether anybody typed one

	The conversion is the catalogue's own, CatalogProperty::toVariant, and
	it is used rather than a bare toDouble for one reason: it answers with
	an invalid QVariant instead of a zero, which is the only place in the
	program where "not measured" and "zero millimetre" are still told apart.
	Reading it as a number here would collapse the two before anybody saw
	them.

	The value is trimmed first, and that is the one repair the read makes:
	a spreadsheet import carries a stray space often enough, and reading is
	tolerant. Not repaired, and worth knowing: a comma as the decimal
	separator is not read as a number, because 1,250 is a thousand two
	hundred and fifty to one office and one and a quarter to another - so it
	is reported as unmeasured rather than guessed at.

	A property the class no longer declares still has its value read, as
	text: Catalog::effectiveValues keeps the values of a removed property
	visible instead of dropping them, and a clearance that survived the
	removal of its own field is a number somebody typed.
*/
CatalogMeasure CatalogPhysicalView::measureIn(const QHash<QString, QString> &values,
					      const QHash<QString, CatalogProperty> &properties,
					      const QString &key)
{
	const QString raw = values.value(key).trimmed();
	if (raw.isEmpty()) {
		return CatalogMeasure();
	}

	const QVariant typed = properties.contains(key)
			       ? properties.value(key).toVariant(raw)
			       : QVariant(raw);
	bool ok = false;
	const qreal number = typed.toDouble(&ok);
	if (!ok) {
		return CatalogMeasure();
	}

	return CatalogMeasure(number);
}

/**
	@brief CatalogPhysicalView::measureIn
	@param values every value of the part, the initial values of its class
	included
	@param properties the properties the class declares, by key
	@param origins where each value was written
	@param key which measure
	@return the number, whether anybody typed one, and which record holds it

	The number is read by the overload above rather than read again here:
	two readings of one cell is how a panel ends up explaining a number
	other than the one it draws.
*/
CatalogMeasure CatalogPhysicalView::measureIn(const QHash<QString, QString> &values,
					      const QHash<QString, CatalogProperty> &properties,
					      const QHash<QString, CatalogValueOrigin> &origins,
					      const QString &key)
{
	CatalogMeasure measure = measureIn(values, properties, key);
	measure.origin = origins.value(key);
	return measure;
}

/**
	@brief CatalogPhysicalView::flagIn
	@param values every value of the part
	@param key which flag
	@param fallback what it means when the cell says nothing usable
	@return what the flag says

	The three spellings the catalogue writes for a boolean, and the fallback
	decided by the caller rather than in CatalogProperty: that class answers
	"invalid" for anything else, which is the right answer for a conversion
	and no answer at all for a drawing.
*/
bool CatalogPhysicalView::flagIn(const QHash<QString, QString> &values,
				 const QString &key,
				 bool fallback)
{
	const QString raw = values.value(key).trimmed().toLower();
	if (raw.isEmpty()) {
		return fallback;
	}
	if (raw == QStringLiteral("1") || raw == QStringLiteral("true")) {
		return true;
	}
	if (raw == QStringLiteral("0") || raw == QStringLiteral("false")) {
		return false;
	}
	return fallback;
}

/**
	@brief CatalogPhysicalView::read
	@param values every value of the part
	@param properties the properties the class declares, by key
	@return the body the values describe, every measure carrying an Unread
	origin because this caller did not ask for one
*/
CatalogPhysicalView CatalogPhysicalView::read(const QHash<QString, QString> &values,
					      const QHash<QString, CatalogProperty> &properties)
{
	return read(values, properties, QHash<QString, CatalogValueOrigin>());
}

/**
	@brief CatalogPhysicalView::read
	@param values every value of the part
	@param properties the properties the class declares, by key
	@param origins where each value was written
	@return the body the values describe
*/
CatalogPhysicalView CatalogPhysicalView::read(const QHash<QString, QString> &values,
					      const QHash<QString, CatalogProperty> &properties,
					      const QHash<QString, CatalogValueOrigin> &origins)
{
	CatalogPhysicalView view;

	view.width  = measureIn(values, properties, origins, QStringLiteral("width"));
	view.height = measureIn(values, properties, origins, QStringLiteral("height"));
	view.depth  = measureIn(values, properties, origins, QStringLiteral("depth"));

	view.clearance_top =
			measureIn(values, properties, origins, QStringLiteral("clearance_top"));
	view.clearance_bottom =
			measureIn(values, properties, origins, QStringLiteral("clearance_bottom"));
	view.clearance_left =
			measureIn(values, properties, origins, QStringLiteral("clearance_left"));
	view.clearance_right =
			measureIn(values, properties, origins, QStringLiteral("clearance_right"));

	view.insertion_x =
			measureIn(values, properties, origins, QStringLiteral("insertion_x"));
	view.insertion_y =
			measureIn(values, properties, origins, QStringLiteral("insertion_y"));

		//No origin on the flag, and it is not an omission: the outline
		//has a fallback of its own - a part with no picture draws as a
		//box - so "nobody wrote it anywhere" is already an answer with
		//nothing to name, while the four clearances have none.
	view.draw_outline = flagIn(values, QStringLiteral("draw_outline"), true);

	return view;
}

/**
	@brief CatalogPhysicalView::read
	@param values every value of the part
	@return the body the values describe
*/
CatalogPhysicalView CatalogPhysicalView::read(const QHash<QString, QString> &values)
{
	return read(values, QHash<QString, CatalogProperty>());
}

/**
	@brief CatalogPhysicalView::hasWidth
	@return true when the width is a length
*/
bool CatalogPhysicalView::hasWidth() const
{
	return width.isLength();
}

/**
	@brief CatalogPhysicalView::hasHeight
	@return true when the height is a length
*/
bool CatalogPhysicalView::hasHeight() const
{
	return height.isLength();
}

/**
	@brief CatalogPhysicalView::hasPhysicalView
	@return true when both dimensions are lengths, so the part can be drawn
	as a rectangle in millimetre
*/
bool CatalogPhysicalView::hasPhysicalView() const
{
	return hasWidth() && hasHeight();
}

/**
	@brief CatalogPhysicalView::hasDepth
	@return true when the depth is a length

	Asked apart from the two above because it is answered apart: the depth
	is the measure a manufacturer omits most often, and a part that can be
	drawn on a plate is not thereby a part that can be drawn on a side view.
*/
bool CatalogPhysicalView::hasDepth() const
{
	return depth.isLength();
}

/**
	@brief CatalogPhysicalView::hasClearance
	@return true when at least one of the four clearances was filled in
*/
bool CatalogPhysicalView::hasClearance() const
{
	return clearance_top.isDeclared() || clearance_bottom.isDeclared()
	       || clearance_left.isDeclared() || clearance_right.isDeclared();
}

/**
	@brief CatalogPhysicalView::hasInsertionPoint
	@return true when both offsets of the axis were filled in

	Both or neither, because an axis needs two numbers. And declaration and
	not length: an offset of zero is the top left corner of the body, which
	is a place somebody may well have chosen, and an offset may be negative
	for a part whose axis sits above its own outline.
*/
bool CatalogPhysicalView::hasInsertionPoint() const
{
	return insertion_x.isDeclared() && insertion_y.isDeclared();
}

/**
	@brief CatalogPhysicalView::hasHalfInsertionPoint
	@return true when exactly one of the two offsets was filled in
*/
bool CatalogPhysicalView::hasHalfInsertionPoint() const
{
	return insertion_x.isDeclared() != insertion_y.isDeclared();
}

/**
	@brief CatalogPhysicalView::insertionOffset
	@return the axis inside the body of the part, millimetre, (0, 0) when
	the pair was not declared
*/
QPointF CatalogPhysicalView::insertionOffset() const
{
	return hasInsertionPoint() ? QPointF(insertion_x.value, insertion_y.value)
				   : QPointF(0.0, 0.0);
}
