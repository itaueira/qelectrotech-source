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
#include "mountingpartview.h"

#include "../catalog/catalog.h"

#include <QSet>
#include <QVariant>

#include <cmath>

namespace
{
	/**
		@brief isUsableLength
		@param length a length in millimetre
		@return true when it is a number and there is something of it

		The same question MountingArea, MountingLayout and MountingCheck
		ask, asked the same way on purpose: a dimension nobody filled in
		arrives as zero, and four files that disagreed about whether zero
		is a length would describe four different panels.
	*/
	bool isUsableLength(qreal length)
	{
		return std::isfinite(length)
		       && length > MountingArea::tolerance();
	}

	/**
		@brief sameMeasure
		@param before a dimension in millimetre
		@param after another one
		@return true when the two say the same thing about a dimension

		Written here for the reason MountingLayout writes it: a
		dimension has more than one spelling for "nobody typed it" - a
		default built QSizeF holds -1 by -1, a dimension read back from
		a file that did not carry it holds zero - and comparing them as
		numbers would report a change between two absent measurements.
		Refreshing a layout has to answer "did anything change" the same
		way the project answers "do I have anything to save", or the two
		would disagree and every refresh would dirty the file.
	*/
	bool sameMeasure(qreal before, qreal after)
	{
		if (!isUsableLength(before) || !isUsableLength(after)) {
			return !isUsableLength(before) && !isUsableLength(after);
		}
		return qAbs(after - before) <= MountingArea::tolerance();
	}

	/// What one catalogue cell says about a length, and whether it said it.
	class ReadMeasure
	{
		public:
				/// the number the cell holds, whatever it is
			qreal value = 0.0;
				/// true when the cell held a number at all
			bool declared = false;
	};

	/**
		@brief measureOf
		@param properties the properties the class of the part declares,
		by key
		@param values every value of the part, the initial values of its
		class included
		@param key which measure
		@return the number and whether anybody typed one

		The conversion is the catalogue's own, CatalogProperty::toVariant,
		and it is used rather than a bare toDouble for one reason: it
		answers with an invalid QVariant instead of a zero, which is the
		only place in the program where "not measured" and "zero
		millimetre" are still told apart. Reading it as a number here
		would collapse the two before this file ever saw them.

		The value is trimmed first, and that is the one repair the read
		makes: a spreadsheet import carries a stray space often enough,
		and reading is tolerant. Not repaired, and worth knowing: a
		comma as the decimal separator is not read as a number, because
		1,250 is a thousand two hundred and fifty to one office and one
		and a quarter to another - so it is reported as unmeasured
		rather than guessed at.

		A property the class no longer declares still has its value read,
		as text: Catalog::effectiveValues keeps the values of a removed
		property visible instead of dropping them, and a clearance that
		survived the removal of its own field is a number somebody typed.
	*/
	ReadMeasure measureOf(const QHash<QString, CatalogProperty> &properties,
			      const QHash<QString, QString> &values,
			      const QString &key)
	{
		ReadMeasure read;

		const QString raw = values.value(key).trimmed();
		if (raw.isEmpty()) {
			return read;
		}

		const QVariant typed = properties.contains(key)
				       ? properties.value(key).toVariant(raw)
				       : QVariant(raw);
		bool ok = false;
		const qreal number = typed.toDouble(&ok);
		if (!ok) {
			return read;
		}

		read.value    = number;
		read.declared = true;
		return read;
	}

	/**
		@brief flagOf
		@param values every value of the part
		@param key which flag
		@param fallback what it means when the cell says nothing usable
		@return what the flag says

		The three spellings the catalogue writes for a boolean, and the
		fallback decided here rather than in CatalogProperty: that class
		answers "invalid" for anything else, which is the right answer
		for a conversion and no answer at all for a drawing. What a
		missing outline flag has to mean belongs where the outline is
		drawn.
	*/
	bool flagOf(const QHash<QString, QString> &values,
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

	/// @return the distinct codes of @a codes, in the order they were met
	QStringList distinctCodes(const QStringList &codes)
	{
		QStringList distinct;
		QSet<QString> seen;

		for (const QString &code : codes)
		{
			const QString trimmed = code.trimmed();
			if (trimmed.isEmpty() || seen.contains(trimmed)) {
				continue;
			}
			seen.insert(trimmed);
			distinct << trimmed;
		}
		return distinct;
	}
}

/**
	@brief MountingPartView::isKnownPart
	@return true when the catalogue holds this product code
*/
bool MountingPartView::isKnownPart() const
{
	return known;
}

/**
	@brief MountingPartView::isNull
	@return true when nothing at all was read: no code asked for and no
	part found
*/
bool MountingPartView::isNull() const
{
	return !known && part_code.isEmpty();
}

/**
	@brief MountingPartView::hasWidth
	@return true when the width is a length
*/
bool MountingPartView::hasWidth() const
{
	return isUsableLength(width);
}

/**
	@brief MountingPartView::hasHeight
	@return true when the height is a length
*/
bool MountingPartView::hasHeight() const
{
	return isUsableLength(height);
}

/**
	@brief MountingPartView::hasSize
	@return true when both dimensions are lengths, so the body is a
	rectangle and not a point
*/
bool MountingPartView::hasSize() const
{
	return hasWidth() && hasHeight();
}

/**
	@brief MountingPartView::hasDepth
	@return true when the depth is a length
*/
bool MountingPartView::hasDepth() const
{
	return isUsableLength(depth);
}

/**
	@brief MountingPartView::hasClearance
	@return true when at least one of the four clearances was filled in
*/
bool MountingPartView::hasClearance() const
{
	return clearance_declared;
}

/**
	@brief MountingPartView::hasInsertion
	@return true when both offsets of the axis were filled in
*/
bool MountingPartView::hasInsertion() const
{
	return insertion_declared;
}

/**
	@brief MountingPartView::hasHalfInsertion
	@return true when exactly one of the two offsets was filled in
*/
bool MountingPartView::hasHalfInsertion() const
{
	return insertion_half_declared;
}

/**
	@brief MountingPartView::size
	@return the room the body takes, millimetre, a dimension nobody filled
	in folded to zero
*/
QSizeF MountingPartView::size() const
{
	return QSizeF(hasWidth() ? width : 0.0,
		      hasHeight() ? height : 0.0);
}

/**
	@brief MountingPartView::footprintAt
	@param corner where the top left corner of the body goes, millimetre
	@return the room it takes there
*/
QRectF MountingPartView::footprintAt(const QPointF &corner) const
{
	return QRectF(corner, size());
}

/**
	@brief MountingPartView::insertionOffset
	@return the axis of the part inside its own body, millimetre, (0, 0)
	when the pair was not declared
*/
QPointF MountingPartView::insertionOffset() const
{
	return insertion_declared ? insertion : QPointF(0.0, 0.0);
}

/**
	@brief MountingPartView::insertionAt
	@param corner where the top left corner of the body sits, millimetre
	@return where the axis of the part sits, millimetre
*/
QPointF MountingPartView::insertionAt(const QPointF &corner) const
{
	return corner + insertionOffset();
}

/**
	@brief MountingPartReader::physicalViewKeys
	@return the catalogue keys this class reads
*/
QStringList MountingPartReader::physicalViewKeys()
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
	@brief MountingPartReader::viewOf
	@param catalog the catalogue to ask
	@param part_code the product code
	@return what the catalogue says about that code
*/
MountingPartView MountingPartReader::viewOf(const Catalog &catalog,
					    const QString &part_code)
{
	const QString code = part_code.trimmed();
	if (code.isEmpty()) {
		return MountingPartView();
	}

	MountingPartView view = viewOfPart(catalog, catalog.partByCode(code));

		//The code that was asked for, and not the code that was found:
		//a code nobody holds has to be able to name itself in the
		//sentence that says so.
	view.part_code = code;
	return view;
}

/**
	@brief MountingPartReader::viewOfPart
	@param catalog the catalogue the part came from
	@param part the part
	@return the body it describes
*/
MountingPartView MountingPartReader::viewOfPart(const Catalog &catalog,
						const CatalogPart &part)
{
	MountingPartView view;
	view.part_code = part.code;
	view.known     = !part.isNull();
	if (!view.known) {
			//Everything else stays at its own answer for "nothing
			//was said", the outline flag included: a code the
			//catalogue does not hold still has to draw as a box,
			//because a part that draws nothing is a part nobody
			//sees is missing.
		return view;
	}

		//The declarations are fetched once, not once per key:
		//Catalog::effectiveProperty walks the ancestry of the class on
		//every call, and ten calls would walk it ten times for one
		//part.
	QHash<QString, CatalogProperty> properties;
	const QList<CatalogProperty> declared =
			catalog.effectiveProperties(part.class_id);
	for (const CatalogProperty &property : declared) {
		properties.insert(property.key, property);
	}

	const QHash<QString, QString> values = catalog.effectiveValues(part);

	const ReadMeasure part_width =
			measureOf(properties, values, QStringLiteral("width"));
	const ReadMeasure part_height =
			measureOf(properties, values, QStringLiteral("height"));
	const ReadMeasure part_depth =
			measureOf(properties, values, QStringLiteral("depth"));

		//Folded here and not at the point of use: a width of minus ten
		//is not a body ten millimetre wide facing the other way, it is
		//a record somebody mistyped, and a negative dimension that
		//reached a rectangle would make that rectangle claim room to
		//the left of where the part stands.
	view.width  = isUsableLength(part_width.value) ? part_width.value : 0.0;
	view.height = isUsableLength(part_height.value) ? part_height.value : 0.0;
	view.depth  = isUsableLength(part_depth.value) ? part_depth.value : 0.0;

	const ReadMeasure top =
			measureOf(properties, values, QStringLiteral("clearance_top"));
	const ReadMeasure bottom =
			measureOf(properties, values, QStringLiteral("clearance_bottom"));
	const ReadMeasure left =
			measureOf(properties, values, QStringLiteral("clearance_left"));
	const ReadMeasure right =
			measureOf(properties, values, QStringLiteral("clearance_right"));

		//Through asked(), which folds what is not a length to nothing
		//asked for - the same fold MountingClearance applies, applied
		//here so that the table handed to the check holds only numbers
		//it can use.
	view.clearance = MountingClearance(MountingClearance::asked(top.value),
					   MountingClearance::asked(bottom.value),
					   MountingClearance::asked(left.value),
					   MountingClearance::asked(right.value));

		//Declared is about the cell and not about the number. A part
		//recorded as needing no air at all is a statement somebody made
		//- a breaker is meant to be clipped against its neighbour - and
		//it reads in the numbers exactly like a part nobody looked at.
		//The flag is the only place the difference survives.
	view.clearance_declared = top.declared || bottom.declared
				  || left.declared || right.declared;

	const ReadMeasure insertion_x =
			measureOf(properties, values, QStringLiteral("insertion_x"));
	const ReadMeasure insertion_y =
			measureOf(properties, values, QStringLiteral("insertion_y"));

		//Both or neither, because an axis needs two numbers. And not
		//folded: an offset of zero is the top left corner of the body,
		//which is a place somebody may well have chosen, and an offset
		//may be negative for a part whose axis sits above its own
		//outline. Only the declaration decides here.
	view.insertion_declared = insertion_x.declared && insertion_y.declared;
	view.insertion_half_declared =
			insertion_x.declared != insertion_y.declared;
	if (view.insertion_declared) {
		view.insertion = QPointF(insertion_x.value, insertion_y.value);
	}

	view.draw_outline = flagOf(values, QStringLiteral("draw_outline"), true);

	return view;
}

/**
	@brief MountingPartReader::viewsOf
	@param catalog the catalogue to ask
	@param part_codes the product codes
	@return one view per distinct code
*/
QHash<QString, MountingPartView>
MountingPartReader::viewsOf(const Catalog &catalog,
			    const QStringList &part_codes)
{
	QHash<QString, MountingPartView> views;

	const QStringList codes = distinctCodes(part_codes);
	for (const QString &code : codes) {
		views.insert(code, viewOf(catalog, code));
	}
	return views;
}

/**
	@brief MountingPartReader::clearancesOf
	@param catalog the catalogue to ask
	@param part_codes the product codes
	@return what each code asks for, the codes that ask for nothing left
	out
*/
QHash<QString, MountingClearance>
MountingPartReader::clearancesOf(const Catalog &catalog,
				 const QStringList &part_codes)
{
	QHash<QString, MountingClearance> table;

	const QHash<QString, MountingPartView> views = viewsOf(catalog, part_codes);
	for (auto view = views.constBegin(); view != views.constEnd(); ++view)
	{
		if (view.value().clearance.isEmpty()) {
			continue;
		}
		table.insert(view.key(), view.value().clearance);
	}
	return table;
}

/**
	@brief MountingPartReader::clearancesFor
	@param catalog the catalogue to ask
	@param surface the face
	@return the clearances of everything mounted on it
*/
QHash<QString, MountingClearance>
MountingPartReader::clearancesFor(const Catalog &catalog,
				  const MountingSurface &surface)
{
	return clearancesOf(catalog, partCodesOf(surface));
}

/**
	@brief MountingPartReader::clearancesFor
	@param catalog the catalogue to ask
	@param layout every face of the project
	@return the clearances of everything mounted anywhere in it
*/
QHash<QString, MountingClearance>
MountingPartReader::clearancesFor(const Catalog &catalog,
				  const MountingLayout &layout)
{
	return clearancesOf(catalog, partCodesOf(layout));
}

/**
	@brief MountingPartReader::checkSurface
	@param catalog the catalogue to ask
	@param surface the face and everything screwed to it
	@return the report, with the clearances the catalogue holds
*/
MountingSurfaceReport
MountingPartReader::checkSurface(const Catalog &catalog,
				 const MountingSurface &surface)
{
	return MountingCheck::surfaceReport(surface,
					    clearancesFor(catalog, surface));
}

/**
	@brief MountingPartReader::mountedItemFor
	@param catalog the catalogue to ask
	@param part_code the product code
	@param position where the top left corner goes, millimetre
	@param label what the sheet will show
	@return the item, sized by the catalogue

	No identifier is given here: MountingLayout::mountItem gives one to an
	item that arrives without, and identity belongs to whoever keeps the
	list rather than to whoever reads the product record.
*/
MountedItem MountingPartReader::mountedItemFor(const Catalog &catalog,
					       const QString &part_code,
					       const QPointF &position,
					       const QString &label)
{
	const MountingPartView view = viewOf(catalog, part_code);

	MountedItem item;
	item.label     = label;
	item.part_code = view.part_code;
	item.position  = position;
	item.size      = view.size();
	return item;
}

/**
	@brief MountingPartReader::applyTo
	@param catalog the catalogue to ask
	@param item the item, its size rewritten when the catalogue answers
	@return true when the size changed
*/
bool MountingPartReader::applyTo(const Catalog &catalog, MountedItem &item)
{
		//A rail is cut on the bench and carries no code, and its length
		//is the person's. This is a fast path and not the lock: it
		//spares the data base one query per rail, and the same answer is
		//held by the test below, since viewOf refuses an empty code and
		//an unknown part changes nothing. Measured rather than assumed -
		//removing this line leaves every case of the suite green. So
		//whoever simplifies it away has not broken the rail, and whoever
		//believes it is what keeps the rail safe is reading the wrong
		//line.
	if (item.part_code.trimmed().isEmpty()) {
		return false;
	}

	const MountingPartView view = viewOf(catalog, item.part_code);
	if (!view.isKnownPart()) {
			//The share may be down and the code may be wrong. Either
			//way the catalogue said nothing, and nothing is not a
			//measurement: what the project stored stays.
		return false;
	}

	const QSizeF read = view.size();
	if (sameMeasure(item.size.width(), read.width())
	    && sameMeasure(item.size.height(), read.height())) {
		return false;
	}

	item.size = read;
	return true;
}

/**
	@brief MountingPartReader::applyTo
	@param catalog the catalogue to ask
	@param surface the face, the size of its items rewritten
	@return the identifier of every item whose size changed
*/
QStringList MountingPartReader::applyTo(const Catalog &catalog,
					MountingSurface &surface)
{
	QStringList changed;

	for (int index = 0; index < surface.items.count(); ++index)
	{
		if (applyTo(catalog, surface.items[index])) {
			changed << surface.items.at(index).uuid;
		}
	}
	return changed;
}

/**
	@brief MountingPartReader::applyTo
	@param catalog the catalogue to ask
	@param layout every face of the project, the size of every item
	rewritten
	@return the identifier of every item whose size changed

	Written back through updateSurface rather than into the faces
	directly, so that the layout keeps its own invariants: it is the one
	entry point, for the reason an undo command wants one operation and
	not four. A face it refuses to write is a face nothing is reported
	about, because nothing was changed.
*/
QStringList MountingPartReader::applyTo(const Catalog &catalog,
					MountingLayout &layout)
{
	QStringList changed;

	for (int index = 0; index < layout.count(); ++index)
	{
		MountingSurface surface = layout.at(index);
		const QStringList touched = applyTo(catalog, surface);
		if (touched.isEmpty()) {
			continue;
		}
		if (layout.updateSurface(surface)) {
			changed << touched;
		}
	}
	return changed;
}

/**
	@brief MountingPartReader::partCodesOf
	@param surface the face
	@return the distinct product codes mounted on it
*/
QStringList MountingPartReader::partCodesOf(const MountingSurface &surface)
{
	QStringList codes;

	for (const MountedItem &item : surface.items) {
		codes << item.part_code;
	}
	return distinctCodes(codes);
}

/**
	@brief MountingPartReader::partCodesOf
	@param layout every face of the project
	@return the distinct product codes mounted anywhere in it
*/
QStringList MountingPartReader::partCodesOf(const MountingLayout &layout)
{
	QStringList codes;

	for (int index = 0; index < layout.count(); ++index)
	{
		const MountingSurface &surface = layout.at(index);
		for (const MountedItem &item : surface.items) {
			codes << item.part_code;
		}
	}
	return distinctCodes(codes);
}

/**
	@brief MountingPartReader::unknownCodes
	@param catalog the catalogue to ask
	@param surface the face
	@return the codes it mounts that the catalogue does not hold
*/
QStringList MountingPartReader::unknownCodes(const Catalog &catalog,
					     const MountingSurface &surface)
{
	QStringList unknown;

	const QStringList codes = partCodesOf(surface);
	for (const QString &code : codes)
	{
		if (!viewOf(catalog, code).isKnownPart()) {
			unknown << code;
		}
	}
	return unknown;
}
