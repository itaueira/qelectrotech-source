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
#include "locationtree.h"

#include <QDomDocument>
#include <QDomElement>
#include <QHash>
#include <QSet>
#include <QUuid>

namespace
{
	/**
		@brief Take out of a code everything a path cannot carry.
		Only used when reading a file: a code typed by a person is refused
		with a sentence, but a code that is already in a file has to become
		something usable, because refusing it would lose the location.
	*/
	QString usableCode(const QString &code)
	{
		QString clean = ProjectLocation::sanitizeCode(code);
		clean.remove(QLatin1Char('='));
		clean.remove(QLatin1Char('+'));
		clean.remove(QLatin1Char('-'));
		clean.remove(QLatin1Char(':'));
		clean.remove(QLatin1Char('/'));
		return clean.simplified();
	}

	/**
		@brief Whether a location already accepted answers to this code.
		@param read the locations accepted so far
		@param before index to stop at, so that only what is settled counts
		@param parent_uuid the level to look in
		@param code the code to look for, case folded away
	*/
	bool siblingTaken(const QVector<ProjectLocation> &read,
			  int before,
			  const QString &parent_uuid,
			  const QString &code)
	{
		for (int i = 0; i < before; ++i)
		{
			if (read.at(i).parent_uuid != parent_uuid) {
				continue;
			}
			if (read.at(i).code.compare(code, Qt::CaseInsensitive) == 0) {
				return true;
			}
		}
		return false;
	}
}

LocationTree::LocationTree()
{}

int LocationTree::count() const
{
	return int(m_locations.count());
}

bool LocationTree::isEmpty() const
{
	return m_locations.isEmpty();
}

void LocationTree::clear()
{
	m_locations.clear();
}

/**
	@brief LocationTree::at
	@param index
	@return the location at that index, an empty one when there is none
*/
const ProjectLocation &LocationTree::at(int index) const
{
	static const ProjectLocation null_location;
	if (index < 0 || index >= count()) {
		return null_location;
	}
	return m_locations.at(index);
}

/**
	@brief LocationTree::location
	@param uuid
	@return the location that goes by this uuid, an empty one when none does
*/
ProjectLocation LocationTree::location(const QString &uuid) const
{
	const int index = indexOfUuid(uuid);
	return index < 0 ? ProjectLocation() : m_locations.at(index);
}

int LocationTree::indexOfUuid(const QString &uuid) const
{
	if (uuid.isEmpty()) {
		return -1;
	}
	const int total = count();
	for (int i = 0; i < total; ++i)
	{
		if (m_locations.at(i).uuid == uuid) {
			return i;
		}
	}
	return -1;
}

/**
	@brief LocationTree::indexOfPath
	@param path a path of codes, QCM1/PORTE
	@return the index of the location that path names, -1 when it names none

	Walked step by step rather than compared against every path of the tree,
	so that the walk stops at the first code that does not exist - which is
	also what tells the caller the path is stale rather than merely different.
*/
int LocationTree::indexOfPath(const QString &path) const
{
	const QStringList codes = splitPath(path);
	if (codes.isEmpty()) {
		return -1;
	}

	QString parent_uuid;
	int index = -1;
	for (const QString &code : codes)
	{
		index = indexOfSiblingCode(parent_uuid, code);
		if (index < 0) {
			return -1;
		}
		parent_uuid = m_locations.at(index).uuid;
	}
	return index;
}

QString LocationTree::uuidOfPath(const QString &path) const
{
	const int index = indexOfPath(path);
	return index < 0 ? QString() : m_locations.at(index).uuid;
}

/**
	@brief LocationTree::append
	@param location
	@param error
	@return the uuid of the location added, empty when it was refused
*/
QString LocationTree::append(ProjectLocation location, QString *error)
{
	location.code = ProjectLocation::sanitizeCode(location.code);
	if (!ProjectLocation::isValidCode(location.code, error)) {
		return QString();
	}

	if (!location.parent_uuid.isEmpty()
	    && indexOfUuid(location.parent_uuid) < 0)
	{
		if (error) {
			*error = tr("La localisation parente n'existe plus.");
		}
		return QString();
	}

	if (indexOfSiblingCode(location.parent_uuid, location.code) >= 0)
	{
		if (error) {
			*error = tr("Une localisation de même niveau porte déjà "
				    "le code « %1 ».").arg(location.code);
		}
		return QString();
	}

	if (location.uuid.isEmpty() || indexOfUuid(location.uuid) >= 0) {
		location.uuid = newId();
	}
	m_locations.append(location);
	return location.uuid;
}

/**
	@brief LocationTree::update
	@param location
	@param moved
	@param error
	@return true when the tree was changed

	False means one of two things, and error tells them apart: filled, the
	change was refused and nothing was written; empty, the location handed in
	was the one already there. The second is not a failure - it is the guard
	that keeps a dialogue opened and closed from marking a project modified.
*/
bool LocationTree::update(const ProjectLocation &location,
			  QMap<QString, QString> *moved,
			  QString *error)
{
	if (error) {
		error->clear();
	}
	if (moved) {
		moved->clear();
	}

	const int index = indexOfUuid(location.uuid);
	if (index < 0)
	{
		if (error) {
			*error = tr("Cette localisation n'est plus dans le projet.");
		}
		return false;
	}

	ProjectLocation updated = location;
	updated.code = ProjectLocation::sanitizeCode(updated.code);
	if (!ProjectLocation::isValidCode(updated.code, error)) {
		return false;
	}

	if (!updated.parent_uuid.isEmpty())
	{
		if (updated.parent_uuid == updated.uuid)
		{
			if (error) {
				*error = tr("Une localisation ne peut pas être "
					    "placée dans elle-même.");
			}
			return false;
		}
		if (indexOfUuid(updated.parent_uuid) < 0)
		{
			if (error) {
				*error = tr("La localisation parente n'existe plus.");
			}
			return false;
		}
		if (isDescendantOf(updated.parent_uuid, updated.uuid))
		{
			if (error) {
				*error = tr("Une localisation ne peut pas être "
					    "placée dans une localisation qu'elle "
					    "contient.");
			}
			return false;
		}
	}

	if (indexOfSiblingCode(updated.parent_uuid, updated.code,
			       updated.uuid) >= 0)
	{
		if (error) {
			*error = tr("Une localisation de même niveau porte déjà "
				    "le code « %1 ».").arg(updated.code);
		}
		return false;
	}

	if (m_locations.at(index) == updated) {
		return false;
	}

		//The branch is read before the change and read again after it: the
		//components below a renamed enclosure follow it, and only the
		//operation that made the change knows which ones those are.
	QStringList branch;
	branch << updated.uuid;
	branch << descendantUuids(updated.uuid);
	const QMap<QString, QString> before = pathsOf(branch);

	m_locations[index] = updated;

	if (moved)
	{
		const QMap<QString, QString> after = pathsOf(branch);
		for (auto it = before.constBegin(); it != before.constEnd(); ++it)
		{
			const QString new_path = after.value(it.key());
			if (!new_path.isEmpty() && new_path != it.value()) {
				moved->insert(it.value(), new_path);
			}
		}
	}

	return true;
}

/**
	@brief LocationTree::move
	@param uuid
	@param new_parent_uuid
	@param moved
	@param error
	@return true when the tree was changed
*/
bool LocationTree::move(const QString &uuid,
			const QString &new_parent_uuid,
			QMap<QString, QString> *moved,
			QString *error)
{
	const int index = indexOfUuid(uuid);
	if (index < 0)
	{
		if (error) {
			*error = tr("Cette localisation n'est plus dans le projet.");
		}
		if (moved) {
			moved->clear();
		}
		return false;
	}

	ProjectLocation updated = m_locations.at(index);
	updated.parent_uuid = new_parent_uuid;
	return update(updated, moved, error);
}

/**
	@brief LocationTree::remove
	@param uuid
	@param removed
	@return true when something was removed
*/
bool LocationTree::remove(const QString &uuid, QStringList *removed)
{
	if (removed) {
		removed->clear();
	}
	if (indexOfUuid(uuid) < 0) {
		return false;
	}

	QStringList branch;
	branch << uuid;
	branch << descendantUuids(uuid);

	if (removed)
	{
		const QMap<QString, QString> map = pathsOf(branch);
		for (const QString &id : branch)
		{
			const QString value = map.value(id);
			if (!value.isEmpty()) {
				removed->append(value);
			}
		}
	}

	for (int i = count() - 1; i >= 0; --i)
	{
		if (branch.contains(m_locations.at(i).uuid)) {
			m_locations.removeAt(i);
		}
	}
	return true;
}

QStringList LocationTree::rootUuids() const
{
	return childUuids(QString());
}

/**
	@brief LocationTree::childUuids
	@param parent_uuid the level to read, empty for the top one
	@return the locations of that level, in the order they were created
*/
QStringList LocationTree::childUuids(const QString &parent_uuid) const
{
	QStringList list;
	for (const ProjectLocation &location : m_locations)
	{
		if (location.parent_uuid == parent_uuid) {
			list << location.uuid;
		}
	}
	return list;
}

/**
	@brief LocationTree::descendantUuids
	@param uuid
	@return everything below this location, deepest branch first

	Iterative and with a set of what it has already seen, because this is a
	public method and a file can hold a tree that loops. It is cheaper to
	survive that here than to trust every reader of the file.
*/
QStringList LocationTree::descendantUuids(const QString &uuid) const
{
	QStringList list;
	QSet<QString> seen;
	seen.insert(uuid);

	QStringList pending = childUuids(uuid);
	while (!pending.isEmpty())
	{
		const QString current = pending.takeFirst();
		if (seen.contains(current)) {
			continue;
		}
		seen.insert(current);
		list << current;

		const QStringList children = childUuids(current);
		for (int i = int(children.count()) - 1; i >= 0; --i) {
			pending.prepend(children.at(i));
		}
	}
	return list;
}

/**
	@brief LocationTree::depth
	@param uuid
	@return 0 for a location of the top level, -1 when it is not in the tree
*/
int LocationTree::depth(const QString &uuid) const
{
	if (indexOfUuid(uuid) < 0) {
		return -1;
	}

	int level = 0;
	QSet<QString> seen;
	QString current = uuid;
	while (!current.isEmpty() && !seen.contains(current))
	{
		seen.insert(current);
		const int index = indexOfUuid(current);
		if (index < 0) {
			break;
		}
		current = m_locations.at(index).parent_uuid;
		if (!current.isEmpty()) {
			++level;
		}
	}
	return level;
}

/**
	@brief LocationTree::path
	@param uuid
	@return the codes down to this location joined, empty when it is not there
*/
QString LocationTree::path(const QString &uuid) const
{
	if (uuid.isEmpty()) {
		return QString();
	}

	QStringList codes;
	QSet<QString> seen;
	QString current = uuid;
	while (!current.isEmpty() && !seen.contains(current))
	{
		seen.insert(current);
		const int index = indexOfUuid(current);
		if (index < 0) {
			return QString();
		}
		codes.prepend(m_locations.at(index).code);
		current = m_locations.at(index).parent_uuid;
	}
	return joinPath(codes);
}

/**
	@brief LocationTree::displayPath
	@param uuid
	@return the names down to this location, the code where a name is missing
*/
QString LocationTree::displayPath(const QString &uuid) const
{
	if (uuid.isEmpty()) {
		return QString();
	}

	QStringList names;
	QSet<QString> seen;
	QString current = uuid;
	while (!current.isEmpty() && !seen.contains(current))
	{
		seen.insert(current);
		const int index = indexOfUuid(current);
		if (index < 0) {
			return QString();
		}
		const ProjectLocation &location = m_locations.at(index);
		names.prepend(location.name.isEmpty() ? location.code
						     : location.name);
		current = location.parent_uuid;
	}
	return names.join(QStringLiteral(" / "));
}

/**
	@brief LocationTree::paths
	@return every path of the tree, a parent always before its children
*/
QStringList LocationTree::paths() const
{
	QStringList list;
	const QStringList roots = rootUuids();
	for (const QString &root : roots)
	{
		list << path(root);
		const QStringList children = descendantUuids(root);
		for (const QString &child : children) {
			list << path(child);
		}
	}
	list.removeAll(QString());
	return list;
}

/**
	@brief LocationTree::iecTag
	@param path
	@return the designation IEC 81346 writes for that path
*/
QString LocationTree::iecTag(const QString &path)
{
	const QStringList codes = splitPath(path);
	if (codes.isEmpty()) {
		return QString();
	}
	return QStringLiteral("+") + codes.join(QStringLiteral("+"));
}

QStringList LocationTree::splitPath(const QString &path)
{
	QStringList codes;
	const QStringList parts = path.split(ProjectLocation::separator());
	for (const QString &part : parts)
	{
		const QString clean = ProjectLocation::sanitizeCode(part);
		if (!clean.isEmpty()) {
			codes << clean;
		}
	}
	return codes;
}

QString LocationTree::joinPath(const QStringList &codes)
{
	return codes.join(ProjectLocation::separator());
}

/**
	@brief LocationTree::bomLines
	@return one line per part the locations themselves were bought as
*/
QList<LocationTree::BomLine> LocationTree::bomLines() const
{
	QList<BomLine> lines;

	for (const ProjectLocation &location : m_locations)
	{
		if (location.isVirtual()) {
			continue;
		}
		const QString location_path = path(location.uuid);
		if (location_path.isEmpty()) {
			continue;
		}

		int found = -1;
		const int total = int(lines.count());
		for (int i = 0; i < total; ++i)
		{
			if (lines.at(i).part_code == location.part_code
			    && lines.at(i).part_revision == location.part_revision)
			{
				found = i;
				break;
			}
		}

		if (found < 0)
		{
			BomLine line;
			line.part_code = location.part_code;
			line.part_revision = location.part_revision;
			line.name = location.name.isEmpty() ? location.code
							    : location.name;
			lines.append(line);
			found = int(lines.count()) - 1;
		}

		lines[found].quantity += 1;
		lines[found].paths << location_path;
	}

	return lines;
}

QDomElement LocationTree::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	for (const ProjectLocation &location : m_locations) {
		element.appendChild(location.toXml(document));
	}
	return element;
}

/**
	@brief LocationTree::fromXml
	@param element
	@return true when the element was ours

	Three things a file can hold that a tree cannot, and all three are
	repaired rather than refused: a parent that is not in the file, a branch
	that loops back into itself, and two locations of the same level sharing
	a code. Refusing any of them would throw away the locations that are
	sound, and the person who opens the project has done nothing wrong.
*/
bool LocationTree::fromXml(const QDomElement &element)
{
	clear();
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	QVector<ProjectLocation> read;
	QStringList uuids;
	for (QDomElement child =
		     element.firstChildElement(ProjectLocation::tagName());
	     !child.isNull();
	     child = child.nextSiblingElement(ProjectLocation::tagName()))
	{
		ProjectLocation location;
		if (!location.fromXml(child)) {
			continue;
		}
		if (location.uuid.isEmpty() || uuids.contains(location.uuid)) {
			location.uuid = newId();
		}
		uuids << location.uuid;
		read.append(location);
	}

	const int total = int(read.count());
	QHash<QString, int> index_of;
	for (int i = 0; i < total; ++i) {
		index_of.insert(read.at(i).uuid, i);
	}

		//A parent nobody wrote down, and a location that is its own parent,
		//both become locations of the top level: visible and correctable,
		//rather than lost somewhere nothing reaches.
	for (int i = 0; i < total; ++i)
	{
		if (read.at(i).parent_uuid.isEmpty()) {
			continue;
		}
		if (read.at(i).parent_uuid == read.at(i).uuid
		    || !index_of.contains(read.at(i).parent_uuid)) {
			read[i].parent_uuid.clear();
		}
	}

		//Walking up from every location finds any loop, and cutting the link
		//of the location the walk came back to breaks it once and for all.
	for (int i = 0; i < total; ++i)
	{
		QSet<QString> seen;
		QString current = read.at(i).uuid;
		while (!current.isEmpty())
		{
			if (seen.contains(current))
			{
				read[index_of.value(current)].parent_uuid.clear();
				break;
			}
			seen.insert(current);
			const int at_index = index_of.value(current, -1);
			if (at_index < 0) {
				break;
			}
			current = read.at(at_index).parent_uuid;
		}
	}

		//And a code has to name one place: what the file lost is given a
		//code, and what two siblings shared is numbered apart.
	for (int i = 0; i < total; ++i)
	{
		QString code = usableCode(read.at(i).code);
		if (code.isEmpty()) {
			code = QStringLiteral("L") + QString::number(i + 1);
		}

		QString candidate = code;
		int suffix = 1;
		while (siblingTaken(read, i, read.at(i).parent_uuid, candidate))
		{
			++suffix;
			candidate = code + QStringLiteral("_")
				    + QString::number(suffix);
		}
		read[i].code = candidate;
	}

	m_locations = read;
	return true;
}

/**
	@brief LocationTree::tagName
	@return the name of the element that holds the whole tree
*/
QString LocationTree::tagName()
{
	return QStringLiteral("location_tree");
}

QString LocationTree::newId()
{
	return QUuid::createUuid().toString();
}

bool LocationTree::operator==(const LocationTree &other) const
{
	return m_locations == other.m_locations;
}

bool LocationTree::operator!=(const LocationTree &other) const
{
	return !(*this == other);
}

/**
	@brief LocationTree::indexOfSiblingCode
	@param parent_uuid the level to look in
	@param code the code to look for
	@param except_uuid a location to skip, itself when it is being renamed
	@return the index of the sibling that answers to this code, -1 when none

	Case is folded away. Two enclosures called QCM1 and qcm1 on the same
	drawing are one enclosure typed twice, and letting them both exist would
	make the path ambiguous for a reader even though it is not for the file.
*/
int LocationTree::indexOfSiblingCode(const QString &parent_uuid,
				     const QString &code,
				     const QString &except_uuid) const
{
	const int total = count();
	for (int i = 0; i < total; ++i)
	{
		const ProjectLocation &location = m_locations.at(i);
		if (location.parent_uuid != parent_uuid) {
			continue;
		}
		if (!except_uuid.isEmpty() && location.uuid == except_uuid) {
			continue;
		}
		if (location.code.compare(code, Qt::CaseInsensitive) == 0) {
			return i;
		}
	}
	return -1;
}

/**
	@brief LocationTree::isDescendantOf
	@param uuid
	@param ancestor_uuid
	@return true when ancestor_uuid is uuid itself or contains it
*/
bool LocationTree::isDescendantOf(const QString &uuid,
				  const QString &ancestor_uuid) const
{
	if (uuid.isEmpty() || ancestor_uuid.isEmpty()) {
		return false;
	}

	QSet<QString> seen;
	QString current = uuid;
	while (!current.isEmpty() && !seen.contains(current))
	{
		if (current == ancestor_uuid) {
			return true;
		}
		seen.insert(current);
		const int index = indexOfUuid(current);
		if (index < 0) {
			return false;
		}
		current = m_locations.at(index).parent_uuid;
	}
	return false;
}

/**
	@brief LocationTree::pathsOf
	@param uuids
	@return the path of each of them, by uuid, the ones that have none left out
*/
QMap<QString, QString> LocationTree::pathsOf(const QStringList &uuids) const
{
	QMap<QString, QString> map;
	for (const QString &uuid : uuids)
	{
		const QString value = path(uuid);
		if (!value.isEmpty()) {
			map.insert(uuid, value);
		}
	}
	return map;
}
