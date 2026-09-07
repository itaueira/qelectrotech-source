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
#include "mountinglayout.h"

#include <QDomDocument>
#include <QDomElement>
#include <QUuid>

#include <cmath>

namespace
{
	/**
		@brief isUsableLength
		@param length a length in millimetre
		@return true when it is a number and there is something of it

		The same answer MountingArea gives, on purpose: a dimension
		nobody filled in arrives as zero, and zero millimetre is not a
		length. Asking the question the same way here is what makes an
		unmeasured face survive a file round trip as unmeasured instead
		of coming back as a face of zero by nought.
	*/
	bool isUsableLength(qreal length)
	{
		return std::isfinite(length)
		       && length > MountingArea::tolerance();
	}

	/**
		@brief num
		@param value a length or a coordinate in millimetre
		@return the number as it goes into the file

		Twelve significant digits. Not decoration: the comparisons of
		this family allow a nanometre of slack, so a number written with
		fewer digits could come back far enough out for an item flush
		against the edge of a plate to read as hanging off it. Twelve
		also writes 22.5 as 22.5 and 800 as 800, which is what makes the
		.qet readable by a person.
	*/
	QString num(qreal value)
	{
		return QString::number(value, 'g', 12);
	}

	/**
		@brief length
		@param text an attribute as the file holds it
		@return the number it says, zero when it says nothing usable

		Zero and not a refusal, because a missing dimension is the
		normal state of a part nobody has measured - see MountedItem.
	*/
	qreal length(const QString &text)
	{
		bool ok = false;
		const qreal value = text.toDouble(&ok);
		return ok ? value : 0.0;
	}

	/**
		@brief sameLength
		@param before a length in millimetre
		@param after another one
		@return true when the two are the same length

		Compared with the slack of the family and not bit for bit, so
		that a value that went through the file and came back is the
		value that went in. Two numbers that are not lengths count as
		the same: reporting a change between two absent measurements
		would make a project ask to be saved for ever.
	*/
	bool sameLength(qreal before, qreal after)
	{
		if (!std::isfinite(before) || !std::isfinite(after)) {
			return !std::isfinite(before) && !std::isfinite(after);
		}
		return qAbs(after - before) <= MountingArea::tolerance();
	}

	/**
		@brief sameMeasure
		@param before a dimension in millimetre
		@param after another one
		@return true when the two say the same thing about a dimension

		A dimension has one state more than a coordinate: nobody typed
		it. And it has more than one spelling for that state, which is
		the trap this exists for - a default built QSizeF holds -1 by -1
		and not zero by zero, while a dimension that came back from a
		file where it was absent holds zero. Both mean unmeasured
		everywhere in this family, so comparing them as numbers would
		make a project that was opened and saved ask to be saved again,
		for ever, over a part nobody had finished describing.

		Deliberately not used for a coordinate: a part at x = -1 is a
		part one millimetre off the left edge of the plate, which is a
		real place and a real complaint, and folding it into zero would
		hide the complaint.
	*/
	bool sameMeasure(qreal before, qreal after)
	{
		if (!isUsableLength(before) || !isUsableLength(after)) {
			return !isUsableLength(before) && !isUsableLength(after);
		}
		return sameLength(before, after);
	}

	/// @return true when two faces have the same room on them
	bool sameArea(const MountingArea &before, const MountingArea &after)
	{
		return sameMeasure(before.width, after.width)
		       && sameMeasure(before.height, after.height);
	}

	/**
		@brief sameItem
		@return true when the two describe the same thing in the same
		place

		Written here rather than as an operator on MountedItem, because
		that class is the input of a rule that has its own tests and no
		need of a comparison. What needs one is the project, to know
		whether it has anything new to save.
	*/
	bool sameItem(const MountedItem &before, const MountedItem &after)
	{
		return before.uuid == after.uuid
		       && before.label == after.label
		       && before.part_code == after.part_code
		       && sameLength(before.position.x(), after.position.x())
		       && sameLength(before.position.y(), after.position.y())
		       && sameMeasure(before.size.width(), after.size.width())
		       && sameMeasure(before.size.height(), after.size.height());
	}

	/// @return one mounted item as the file holds it
	QDomElement itemToXml(QDomDocument &document, const MountedItem &item)
	{
		QDomElement element =
				document.createElement(MountingSurface::itemTagName());

		if (!item.uuid.isEmpty()) {
			element.setAttribute(QStringLiteral("uuid"), item.uuid);
		}
		if (!item.label.isEmpty()) {
			element.setAttribute(QStringLiteral("label"), item.label);
		}
		if (!item.part_code.isEmpty()) {
			element.setAttribute(QStringLiteral("part"), item.part_code);
		}

			//The position is written whatever it is, because the top
			//left corner of the face is a place like any other and
			//(0, 0) is where a person pushes the first rail. The two
			//dimensions are written only when they are lengths: what
			//nobody measured has to read back as not measured, and a
			//zero written down would read as a measurement of nothing.
		element.setAttribute(QStringLiteral("x"), num(item.position.x()));
		element.setAttribute(QStringLiteral("y"), num(item.position.y()));
		if (isUsableLength(item.size.width())) {
			element.setAttribute(QStringLiteral("width"),
					     num(item.size.width()));
		}
		if (isUsableLength(item.size.height())) {
			element.setAttribute(QStringLiteral("height"),
					     num(item.size.height()));
		}

		return element;
	}

	/// @return one mounted item as the file said it, whatever it said
	MountedItem itemFromXml(const QDomElement &element)
	{
		MountedItem item;
		item.uuid      = element.attribute(QStringLiteral("uuid"));
		item.label     = element.attribute(QStringLiteral("label"));
		item.part_code = element.attribute(QStringLiteral("part"));
		item.position  = QPointF(length(element.attribute(QStringLiteral("x"))),
					 length(element.attribute(QStringLiteral("y"))));
		item.size      = QSizeF(length(element.attribute(QStringLiteral("width"))),
					length(element.attribute(QStringLiteral("height"))));
		return item;
	}
}

/**
	@brief MountingSurface::MountingSurface
	@param surface_location_path the location this face belongs to
	@param surface_kind which face of it: plate, door, side
	@param surface_area how much room there is on it, millimetre
*/
MountingSurface::MountingSurface(const QString &surface_location_path,
				 const QString &surface_kind,
				 const MountingArea &surface_area) :
	location_path(surface_location_path),
	kind(surface_kind),
	area(surface_area)
{}

/**
	@brief MountingSurface::isNull
	@return true when nothing here says which face this is
*/
bool MountingSurface::isNull() const
{
	return location_path.isEmpty() && kind.isEmpty() && name.isEmpty();
}

int MountingSurface::itemCount() const
{
	return int(items.count());
}

/**
	@brief MountingSurface::indexOfItem
	@param item_uuid the identity of a mounted item
	@return where it sits in items, -1 when it is not on this face
*/
int MountingSurface::indexOfItem(const QString &item_uuid) const
{
	if (item_uuid.isEmpty()) {
		return -1;
	}
	for (int i = 0; i < items.count(); ++i)
	{
		if (items.at(i).uuid == item_uuid) {
			return i;
		}
	}
	return -1;
}

/**
	@brief MountingSurface::item
	@param item_uuid the identity of a mounted item
	@return the item, a default built one when this face has no such item
*/
MountedItem MountingSurface::item(const QString &item_uuid) const
{
	const int index = indexOfItem(item_uuid);
	return index < 0 ? MountedItem() : items.at(index);
}

/// @return the identity of everything mounted here, in the order it was mounted
QStringList MountingSurface::itemUuids() const
{
	QStringList uuids;
	for (const MountedItem &item : items) {
		uuids << item.uuid;
	}
	return uuids;
}

/**
	@brief MountingSurface::planFor
	@param new_area the face of the enclosure taking this one's place
	@return one entry per item, none of them dropped
*/
EnclosureTransferPlan MountingSurface::planFor(const MountingArea &new_area) const
{
	return EnclosureTransfer::plan(items, area, new_area);
}

/**
	@brief MountingSurface::designation
	@return how to call this face out loud, never empty
*/
QString MountingSurface::designation() const
{
	if (!name.isEmpty()) {
		return name;
	}
	if (!location_path.isEmpty() && !kind.isEmpty()) {
		return location_path + QStringLiteral(" ") + kind;
	}
	if (!location_path.isEmpty()) {
		return location_path;
	}
	if (!kind.isEmpty()) {
		return kind;
	}
	if (!uuid.isEmpty()) {
		return uuid;
	}
	return tr("une surface de montage sans repère");
}

/**
	@brief MountingSurface::toXml
	@param document the document the element is created in
	@return this face and everything mounted on it
*/
QDomElement MountingSurface::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());

	if (!uuid.isEmpty()) {
		element.setAttribute(QStringLiteral("uuid"), uuid);
	}
	element.setAttribute(QStringLiteral("location"), location_path);
	element.setAttribute(QStringLiteral("kind"), kind);
	if (!name.isEmpty()) {
		element.setAttribute(QStringLiteral("name"), name);
	}

		//One dimension at a time, and never a zero: a person who typed
		//the width of a plate and not its height has said one true
		//thing, and the file has to be able to hold exactly that.
	if (isUsableLength(area.width)) {
		element.setAttribute(QStringLiteral("width"), num(area.width));
	}
	if (isUsableLength(area.height)) {
		element.setAttribute(QStringLiteral("height"), num(area.height));
	}

	for (const MountedItem &item : items) {
		element.appendChild(itemToXml(document, item));
	}

	return element;
}

/**
	@brief MountingSurface::fromXml
	@param element a face as a file holds it
	@return false when this is not one of our elements

	Tolerant of everything else. A face that says nothing about which face
	it is becomes the plate, an item that says nothing about its size comes
	back with no size, and nothing is skipped for being incomplete - the
	identifiers are made unique by MountingLayout::fromXml, which is the
	only place that can see the whole file.
*/
bool MountingSurface::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	uuid          = element.attribute(QStringLiteral("uuid"));
	location_path = element.attribute(QStringLiteral("location")).trimmed();
	kind          = element.attribute(QStringLiteral("kind")).trimmed();
	name          = element.attribute(QStringLiteral("name"));
	if (kind.isEmpty()) {
		kind = defaultKind();
	}

	area = MountingArea(length(element.attribute(QStringLiteral("width"))),
			    length(element.attribute(QStringLiteral("height"))));

	items.clear();
	for (QDomElement child = element.firstChildElement(itemTagName());
	     !child.isNull();
	     child = child.nextSiblingElement(itemTagName()))
	{
		items.append(itemFromXml(child));
	}

	return true;
}

QString MountingSurface::tagName()
{
	return QStringLiteral("mounting_surface");
}

QString MountingSurface::itemTagName()
{
	return QStringLiteral("mounted_item");
}

/**
	@brief MountingSurface::defaultKind
	@return the face a file that names none is read as

	The plate, because it is the face a panel is built on and the one a
	layout that came from anywhere else would be about. A guess all the
	same, which is why it is made here, on the way in from a file, and
	never by a mutator: what a person creates says which face it is or is
	refused.
*/
QString MountingSurface::defaultKind()
{
	return QStringLiteral("plate");
}

bool MountingSurface::operator==(const MountingSurface &other) const
{
	if (uuid != other.uuid
	    || location_path != other.location_path
	    || kind != other.kind
	    || name != other.name
	    || !sameArea(area, other.area)
	    || items.count() != other.items.count()) {
		return false;
	}
	for (int i = 0; i < items.count(); ++i)
	{
		if (!sameItem(items.at(i), other.items.at(i))) {
			return false;
		}
	}
	return true;
}

bool MountingSurface::operator!=(const MountingSurface &other) const
{
	return !(*this == other);
}

MountingLayout::MountingLayout()
{}

int MountingLayout::count() const
{
	return int(m_surfaces.count());
}

bool MountingLayout::isEmpty() const
{
	return m_surfaces.isEmpty();
}

void MountingLayout::clear()
{
	m_surfaces.clear();
}

/// @return how many items the project has mounted, every face added up
int MountingLayout::itemCount() const
{
	int total = 0;
	for (const MountingSurface &surface : m_surfaces) {
		total += surface.itemCount();
	}
	return total;
}

const MountingSurface &MountingLayout::at(int index) const
{
	static const MountingSurface null_surface;
	if (index < 0 || index >= count()) {
		return null_surface;
	}
	return m_surfaces.at(index);
}

/**
	@brief MountingLayout::surface
	@param surface_uuid the identity of a face
	@return the face, a default built one when there is no such face
*/
MountingSurface MountingLayout::surface(const QString &surface_uuid) const
{
	const int index = indexOfSurface(surface_uuid);
	return index < 0 ? MountingSurface() : m_surfaces.at(index);
}

int MountingLayout::indexOfSurface(const QString &surface_uuid) const
{
	if (surface_uuid.isEmpty()) {
		return -1;
	}
	for (int i = 0; i < m_surfaces.count(); ++i)
	{
		if (m_surfaces.at(i).uuid == surface_uuid) {
			return i;
		}
	}
	return -1;
}

QStringList MountingLayout::surfaceUuids() const
{
	QStringList uuids;
	for (const MountingSurface &surface : m_surfaces) {
		uuids << surface.uuid;
	}
	return uuids;
}

/**
	@brief MountingLayout::surfacesOfLocation
	@param location_path the path of codes of a location: QCM1/PORTE
	@return the faces of that location, in the order they were added

	Compared whole and never by prefix, for the reason the tree rewrites a
	path by lookup: the faces of QCM1 are not the faces of QCM10.
*/
QStringList MountingLayout::surfacesOfLocation(const QString &location_path) const
{
	QStringList uuids;
	if (location_path.isEmpty()) {
		return uuids;
	}
	for (const MountingSurface &surface : m_surfaces)
	{
		if (surface.location_path == location_path) {
			uuids << surface.uuid;
		}
	}
	return uuids;
}

/**
	@brief MountingLayout::appendSurface
	@param surface the face to add
	@param error filled with why nothing was added
	@return the identifier of the face added, empty when refused
*/
QString MountingLayout::appendSurface(MountingSurface surface, QString *error)
{
	if (error) {
		error->clear();
	}

	surface.location_path = surface.location_path.trimmed();
	surface.kind          = surface.kind.trimmed();

	if (surface.location_path.isEmpty())
	{
		if (error) {
			*error = tr("Une surface de montage appartient à une "
				    "localisation du projet.");
		}
		return QString();
	}
	if (surface.kind.isEmpty())
	{
		if (error) {
			*error = tr("La surface de montage ne dit pas de quelle "
				    "face il s'agit.");
		}
		return QString();
	}

	if (surface.uuid.isEmpty()) {
		surface.uuid = newId();
	}
	else if (takenUuid(surface.uuid))
	{
		if (error) {
			*error = tr("Le calepinage utilise déjà l'identifiant "
				    "« %1 ».").arg(surface.uuid);
		}
		return QString();
	}

		//The items come with the face, because a whole terminal strip
		//arrives in one operation. Their identifiers are checked here
		//and not one by one afterwards, so that a face is either added
		//whole or not added at all.
	QStringList given;
	for (int i = 0; i < surface.items.count(); ++i)
	{
		const QString item_uuid = surface.items.at(i).uuid;
		if (item_uuid.isEmpty())
		{
			surface.items[i].uuid = newId();
			continue;
		}
		if (takenUuid(item_uuid) || given.contains(item_uuid))
		{
			if (error) {
				*error = tr("Le composant « %1 » est déjà monté "
					    "sur une surface du projet.")
					 .arg(surface.items.at(i).designation());
			}
			return QString();
		}
		given << item_uuid;
	}

	m_surfaces.append(surface);
	return surface.uuid;
}

/**
	@brief MountingLayout::updateSurface
	@param surface the face as it should now be, matched by uuid
	@param error filled with why nothing was written
	@return true when the layout was changed
*/
bool MountingLayout::updateSurface(const MountingSurface &surface, QString *error)
{
	if (error) {
		error->clear();
	}

	const int index = indexOfSurface(surface.uuid);
	if (index < 0)
	{
		if (error) {
			*error = tr("Cette surface de montage n'est plus dans le "
				    "projet.");
		}
		return false;
	}

	MountingSurface written = surface;
	written.location_path = written.location_path.trimmed();
	written.kind          = written.kind.trimmed();

	if (written.location_path.isEmpty())
	{
		if (error) {
			*error = tr("Une surface de montage appartient à une "
				    "localisation du projet.");
		}
		return false;
	}
	if (written.kind.isEmpty())
	{
		if (error) {
			*error = tr("La surface de montage ne dit pas de quelle "
				    "face il s'agit.");
		}
		return false;
	}

	QStringList given;
	for (int i = 0; i < written.items.count(); ++i)
	{
		const QString item_uuid = written.items.at(i).uuid;
		if (item_uuid.isEmpty())
		{
			written.items[i].uuid = newId();
			continue;
		}
		if (takenUuid(item_uuid, written.uuid) || given.contains(item_uuid))
		{
			if (error) {
				*error = tr("Le composant « %1 » est déjà monté "
					    "sur une surface du projet.")
					 .arg(written.items.at(i).designation());
			}
			return false;
		}
		given << item_uuid;
	}

	if (m_surfaces.at(index) == written) {
		return false;
	}

	m_surfaces[index] = written;
	return true;
}

/**
	@brief MountingLayout::removeSurface
	@param surface_uuid which face
	@param unmounted filled with the identifier of everything that was on it
	@return true when something was removed
*/
bool MountingLayout::removeSurface(const QString &surface_uuid,
				   QStringList *unmounted)
{
	if (unmounted) {
		unmounted->clear();
	}

	const int index = indexOfSurface(surface_uuid);
	if (index < 0) {
		return false;
	}

	if (unmounted) {
		*unmounted = m_surfaces.at(index).itemUuids();
	}
	m_surfaces.remove(index);
	return true;
}

/**
	@brief MountingLayout::mountItem
	@param surface_uuid which face
	@param item what is screwed to it
	@param error filled with why nothing was mounted
	@return the identifier of the item mounted, empty when refused

	An item whose dimensions nobody typed is mounted, and so is an item
	that does not fit where it was put: both are answered by the plan and
	by the checks, never by a refusal here. What is refused is an item
	already mounted somewhere in this project, because a component is
	screwed in one place.
*/
QString MountingLayout::mountItem(const QString &surface_uuid,
				  MountedItem item,
				  QString *error)
{
	if (error) {
		error->clear();
	}

	const int index = indexOfSurface(surface_uuid);
	if (index < 0)
	{
		if (error) {
			*error = tr("Cette surface de montage n'est plus dans le "
				    "projet.");
		}
		return QString();
	}

	if (item.uuid.isEmpty()) {
		item.uuid = newId();
	}
	else if (takenUuid(item.uuid))
	{
		if (error) {
			*error = tr("Le composant « %1 » est déjà monté sur une "
				    "surface du projet.").arg(item.designation());
		}
		return QString();
	}

	m_surfaces[index].items.append(item);
	return item.uuid;
}

/**
	@brief MountingLayout::updateItem
	@param item the item as it should now be, matched by uuid
	@param error filled with why nothing was written
	@return true when the layout was changed
*/
bool MountingLayout::updateItem(const MountedItem &item, QString *error)
{
	if (error) {
		error->clear();
	}

	const int owner = indexOfItemOwner(item.uuid);
	if (owner < 0)
	{
		if (error) {
			*error = tr("Ce composant n'est monté sur aucune surface "
				    "du projet.");
		}
		return false;
	}

	const int index = m_surfaces.at(owner).indexOfItem(item.uuid);
	if (sameItem(m_surfaces.at(owner).items.at(index), item)) {
		return false;
	}

	m_surfaces[owner].items[index] = item;
	return true;
}

/**
	@brief MountingLayout::moveItem
	@param item_uuid which item
	@param surface_uuid which face it goes to
	@param error filled with why nothing was moved
	@return true when the layout was changed
*/
bool MountingLayout::moveItem(const QString &item_uuid,
			      const QString &surface_uuid,
			      QString *error)
{
	if (error) {
		error->clear();
	}

	const int owner = indexOfItemOwner(item_uuid);
	if (owner < 0)
	{
		if (error) {
			*error = tr("Ce composant n'est monté sur aucune surface "
				    "du projet.");
		}
		return false;
	}

	const int target = indexOfSurface(surface_uuid);
	if (target < 0)
	{
		if (error) {
			*error = tr("Cette surface de montage n'est plus dans le "
				    "projet.");
		}
		return false;
	}
	if (target == owner) {
		return false;
	}

	const int index = m_surfaces.at(owner).indexOfItem(item_uuid);
	const MountedItem item = m_surfaces.at(owner).items.at(index);
	m_surfaces[owner].items.removeAt(index);
	m_surfaces[target].items.append(item);
	return true;
}

/**
	@brief MountingLayout::unmountItem
	@param item_uuid which item
	@return true when the item was mounted and no longer is
*/
bool MountingLayout::unmountItem(const QString &item_uuid)
{
	const int owner = indexOfItemOwner(item_uuid);
	if (owner < 0) {
		return false;
	}

	m_surfaces[owner].items.removeAt(m_surfaces.at(owner).indexOfItem(item_uuid));
	return true;
}

/**
	@brief MountingLayout::item
	@param item_uuid which item
	@return the item wherever it is mounted, a default one when nowhere
*/
MountedItem MountingLayout::item(const QString &item_uuid) const
{
	const int owner = indexOfItemOwner(item_uuid);
	return owner < 0 ? MountedItem() : m_surfaces.at(owner).item(item_uuid);
}

/**
	@brief MountingLayout::surfaceOfItem
	@param item_uuid which item
	@return the face it is mounted on, empty when it is mounted on none
*/
QString MountingLayout::surfaceOfItem(const QString &item_uuid) const
{
	const int owner = indexOfItemOwner(item_uuid);
	return owner < 0 ? QString() : m_surfaces.at(owner).uuid;
}

bool MountingLayout::holdsItem(const QString &item_uuid) const
{
	return indexOfItemOwner(item_uuid) >= 0;
}

QStringList MountingLayout::mountedItemUuids() const
{
	QStringList uuids;
	for (const MountingSurface &surface : m_surfaces) {
		uuids << surface.itemUuids();
	}
	return uuids;
}

/**
	@brief MountingLayout::planForSurface
	@param surface_uuid which face
	@param new_area the face of the enclosure taking its place
	@return the plan, an empty one when there is no such face
*/
EnclosureTransferPlan MountingLayout::planForSurface(const QString &surface_uuid,
						     const MountingArea &new_area) const
{
	const int index = indexOfSurface(surface_uuid);
	if (index < 0) {
		return EnclosureTransfer::plan(QList<MountedItem>(),
					       MountingArea(),
					       new_area);
	}
	return m_surfaces.at(index).planFor(new_area);
}

/**
	@brief MountingLayout::applyArea
	@param surface_uuid which face
	@param new_area how much room it has now, millimetre
	@param plan filled with what the change did, the refusal included
	@param error filled with why nothing was written
	@return true when the layout was changed
*/
bool MountingLayout::applyArea(const QString &surface_uuid,
			       const MountingArea &new_area,
			       EnclosureTransferPlan *plan,
			       QString *error)
{
	if (error) {
		error->clear();
	}

	const int index = indexOfSurface(surface_uuid);
	if (index < 0)
	{
		if (plan) {
			*plan = EnclosureTransferPlan();
		}
		if (error) {
			*error = tr("Cette surface de montage n'est plus dans le "
				    "projet.");
		}
		return false;
	}

		//Read before anything is written, which is the whole point of
		//there being a plan at all.
	const EnclosureTransferPlan decided =
			m_surfaces.at(index).planFor(new_area);
	if (plan) {
		*plan = decided;
	}

	if (!new_area.isValid())
	{
		if (error) {
			*error = tr("Les deux dimensions de la surface de montage "
				    "sont attendues en millimètres.");
		}
		return false;
	}

	if (sameArea(m_surfaces.at(index).area, new_area)) {
		return false;
	}

		//Every position comes back from the plan and not from here. The
		//rule keeps each item at the millimetre it was already at, so
		//this assignment changes nothing today - and it is what makes
		//the model follow the day the rule has something else to say,
		//instead of two places having to agree about it.
	m_surfaces[index].area = new_area;
	const int mounted = m_surfaces.at(index).itemCount();
	for (int i = 0; i < mounted && i < decided.entries.count(); ++i) {
		m_surfaces[index].items[i].position = decided.entries.at(i).position;
	}

	return true;
}

/**
	@brief MountingLayout::toXml
	@param document the document the element is created in
	@return every face of the project and everything mounted on it
*/
QDomElement MountingLayout::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	for (const MountingSurface &surface : m_surfaces) {
		element.appendChild(surface.toXml(document));
	}
	return element;
}

/**
	@brief MountingLayout::fromXml
	@param element the layout as a file holds it
	@return false when this is not one of our elements

	Repairs and keeps. A face with no identifier, or with one another face
	already used, is given a new one; so is an item. Nothing is dropped for
	being repeated or incomplete, because the identifier is only there to
	stitch a mounted item back onto a symbol, and losing a plate full of
	parts to a duplicated string would cost incomparably more than a face
	whose link to a folio has to be made again by hand.
*/
bool MountingLayout::fromXml(const QDomElement &element)
{
	clear();
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	QStringList taken;
	for (QDomElement child =
		     element.firstChildElement(MountingSurface::tagName());
	     !child.isNull();
	     child = child.nextSiblingElement(MountingSurface::tagName()))
	{
		MountingSurface surface;
		if (!surface.fromXml(child)) {
			continue;
		}

		if (surface.uuid.isEmpty() || taken.contains(surface.uuid)) {
			surface.uuid = newId();
		}
		taken << surface.uuid;

		for (int i = 0; i < surface.items.count(); ++i)
		{
			const QString item_uuid = surface.items.at(i).uuid;
			if (item_uuid.isEmpty() || taken.contains(item_uuid)) {
				surface.items[i].uuid = newId();
			}
			taken << surface.items.at(i).uuid;
		}

		m_surfaces.append(surface);
	}

	return true;
}

QString MountingLayout::tagName()
{
	return QStringLiteral("mounting_layout");
}

QString MountingLayout::newId()
{
	return QUuid::createUuid().toString();
}

bool MountingLayout::operator==(const MountingLayout &other) const
{
	if (count() != other.count()) {
		return false;
	}
	for (int i = 0; i < m_surfaces.count(); ++i)
	{
		if (m_surfaces.at(i) != other.m_surfaces.at(i)) {
			return false;
		}
	}
	return true;
}

bool MountingLayout::operator!=(const MountingLayout &other) const
{
	return !(*this == other);
}

/**
	@brief MountingLayout::indexOfItemOwner
	@param item_uuid the identity of a mounted item
	@return the face it is mounted on, -1 when it is mounted on none
*/
int MountingLayout::indexOfItemOwner(const QString &item_uuid) const
{
	if (item_uuid.isEmpty()) {
		return -1;
	}
	for (int i = 0; i < m_surfaces.count(); ++i)
	{
		if (m_surfaces.at(i).indexOfItem(item_uuid) >= 0) {
			return i;
		}
	}
	return -1;
}

/**
	@brief MountingLayout::takenUuid
	@param uuid an identifier
	@param except_surface_uuid a face whose identifiers do not count
	@return true when the layout already uses that identifier

	Faces and items share one pool of identifiers, which is stricter than
	it has to be and is the answer that keeps surfaceOfItem answerable: a
	string that names both a plate and a breaker would make "where is this
	mounted" have two readings.
*/
bool MountingLayout::takenUuid(const QString &uuid,
			       const QString &except_surface_uuid) const
{
	if (uuid.isEmpty()) {
		return false;
	}
	for (const MountingSurface &surface : m_surfaces)
	{
		if (!except_surface_uuid.isEmpty()
		    && surface.uuid == except_surface_uuid) {
			continue;
		}
		if (surface.uuid == uuid || surface.indexOfItem(uuid) >= 0) {
			return true;
		}
	}
	return false;
}
