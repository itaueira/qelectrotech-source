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
#include "optiontree.h"

#include <QDomDocument>
#include <QDomElement>
#include <QHash>
#include <QSet>
#include <QUuid>

namespace
{
	/**
		@brief Whether an option already accepted answers to this name.
		@param read the options accepted so far
		@param before index to stop at, so that only what is settled counts
		@param parent_uuid the level to look in
		@param name the name to look for, case folded away
	*/
	bool siblingTaken(const QVector<ProjectOption> &read,
			  int before,
			  const QString &parent_uuid,
			  const QString &name)
	{
		for (int i = 0; i < before; ++i)
		{
			if (read.at(i).parent_uuid != parent_uuid) {
				continue;
			}
			if (read.at(i).name.compare(name, Qt::CaseInsensitive) == 0) {
				return true;
			}
		}
		return false;
	}
}

OptionTree::OptionTree()
{}

int OptionTree::count() const
{
	return int(m_options.count());
}

bool OptionTree::isEmpty() const
{
	return m_options.isEmpty();
}

void OptionTree::clear()
{
	m_options.clear();
}

/**
	@brief OptionTree::at
	@param index
	@return the option at this index, an empty one when there is none
*/
const ProjectOption &OptionTree::at(int index) const
{
	static const ProjectOption null_option;
	if (index < 0 || index >= count()) {
		return null_option;
	}
	return m_options.at(index);
}

/**
	@brief OptionTree::option
	@param uuid
	@return the option that goes by this uuid, an empty one when none does
*/
ProjectOption OptionTree::option(const QString &uuid) const
{
	const int index = indexOfUuid(uuid);
	return index < 0 ? ProjectOption() : m_options.at(index);
}

int OptionTree::indexOfUuid(const QString &uuid) const
{
	if (uuid.isEmpty()) {
		return -1;
	}
	const int total = count();
	for (int i = 0; i < total; ++i)
	{
		if (m_options.at(i).uuid == uuid) {
			return i;
		}
	}
	return -1;
}

/**
	@brief OptionTree::append
	@param option
	@param error
	@return the uuid of the option added, empty when it was refused
*/
QString OptionTree::append(ProjectOption option, QString *error)
{
	option.name = ProjectOption::sanitizeName(option.name);
	if (!ProjectOption::isValidName(option.name, error)) {
		return QString();
	}

	if (!option.parent_uuid.isEmpty()
	    && indexOfUuid(option.parent_uuid) < 0)
	{
		if (error) {
			*error = tr("L'option parente n'existe plus.");
		}
		return QString();
	}

	if (indexOfSiblingName(option.parent_uuid, option.name) >= 0)
	{
		if (error) {
			*error = tr("Une option de même niveau porte déjà le "
				    "nom « %1 ».").arg(option.name);
		}
		return QString();
	}

	if (option.uuid.isEmpty() || indexOfUuid(option.uuid) >= 0) {
		option.uuid = newId();
	}
	m_options.append(option);
	return option.uuid;
}

/**
	@brief OptionTree::update
	@param option
	@param error
	@return true when the tree was changed

	False means one of two things, and error tells them apart: filled, the
	change was refused and nothing was written; empty, the option handed in
	was the one already there. The second is not a failure - it is the guard
	that keeps a dialogue opened and closed from marking a project modified.
*/
bool OptionTree::update(const ProjectOption &option, QString *error)
{
	if (error) {
		error->clear();
	}

	const int index = indexOfUuid(option.uuid);
	if (index < 0)
	{
		if (error) {
			*error = tr("Cette option n'est plus dans le projet.");
		}
		return false;
	}

	ProjectOption updated = option;
	updated.name = ProjectOption::sanitizeName(updated.name);
	if (!ProjectOption::isValidName(updated.name, error)) {
		return false;
	}

	if (!updated.parent_uuid.isEmpty())
	{
		if (updated.parent_uuid == updated.uuid)
		{
			if (error) {
				*error = tr("Une option ne peut pas être placée "
					    "dans elle-même.");
			}
			return false;
		}
		if (indexOfUuid(updated.parent_uuid) < 0)
		{
			if (error) {
				*error = tr("L'option parente n'existe plus.");
			}
			return false;
		}
		if (isDescendantOf(updated.parent_uuid, updated.uuid))
		{
			if (error) {
				*error = tr("Une option ne peut pas être placée "
					    "dans une option qu'elle contient.");
			}
			return false;
		}
	}

	if (indexOfSiblingName(updated.parent_uuid, updated.name,
			       updated.uuid) >= 0)
	{
		if (error) {
			*error = tr("Une option de même niveau porte déjà le "
				    "nom « %1 ».").arg(updated.name);
		}
		return false;
	}

	if (m_options.at(index) == updated) {
		return false;
	}

	m_options[index] = updated;
	return true;
}

/**
	@brief OptionTree::remove
	@param uuid
	@param removed
	@return true when something was removed
*/
bool OptionTree::remove(const QString &uuid, QStringList *removed)
{
	if (removed) {
		removed->clear();
	}

	const int index = indexOfUuid(uuid);
	if (index < 0) {
		return false;
	}

	QStringList going;
	going << uuid;
	going << descendantUuids(uuid);

	QVector<ProjectOption> kept;
	kept.reserve(m_options.count());
	for (const ProjectOption &option : m_options)
	{
		if (!going.contains(option.uuid)) {
			kept.append(option);
		}
	}
	m_options = kept;

	if (removed) {
		*removed = going;
	}
	return true;
}

/**
	@brief OptionTree::setSwitchedOn
	@param uuid
	@param on
	@return true when it changed
*/
bool OptionTree::setSwitchedOn(const QString &uuid, bool on)
{
	const int index = indexOfUuid(uuid);
	if (index < 0 || m_options.at(index).switched_on == on) {
		return false;
	}
	m_options[index].switched_on = on;
	return true;
}

/**
	@brief OptionTree::rootUuids
	@return the options that refine nothing, in the order they were created
*/
QStringList OptionTree::rootUuids() const
{
	return childUuids(QString());
}

/**
	@brief OptionTree::childUuids
	@param parent_uuid the level to read, empty for the top one
	@return the options of that level, in the order they were created
*/
QStringList OptionTree::childUuids(const QString &parent_uuid) const
{
	QStringList list;
	for (const ProjectOption &option : m_options)
	{
		if (option.parent_uuid == parent_uuid) {
			list << option.uuid;
		}
	}
	return list;
}

/**
	@brief OptionTree::descendantUuids
	@param uuid
	@return everything below this option, each one before what it refines

	Iterative and with a set of what it has already seen, because this is a
	public method and a file can hold a tree that loops. It is cheaper to
	survive that here than to trust every reader of the file.
*/
QStringList OptionTree::descendantUuids(const QString &uuid) const
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
	@brief OptionTree::ancestorUuids
	@param uuid
	@return the options above this one, the nearest first
*/
QStringList OptionTree::ancestorUuids(const QString &uuid) const
{
	QStringList list;
	const int start = indexOfUuid(uuid);
	if (start < 0) {
		return list;
	}

	QSet<QString> seen;
	seen.insert(uuid);
	QString current = m_options.at(start).parent_uuid;
	while (!current.isEmpty() && !seen.contains(current))
	{
		const int index = indexOfUuid(current);
		if (index < 0) {
			break;
		}
		seen.insert(current);
		list << current;
		current = m_options.at(index).parent_uuid;
	}
	return list;
}

/**
	@brief OptionTree::depth
	@param uuid
	@return 0 for an option of the top level, -1 when it is not in the tree
*/
int OptionTree::depth(const QString &uuid) const
{
	if (indexOfUuid(uuid) < 0) {
		return -1;
	}
	return int(ancestorUuids(uuid).count());
}

/**
	@brief OptionTree::displayPath
	@param uuid
	@return the names down to this option, joined for a person to read
*/
QString OptionTree::displayPath(const QString &uuid) const
{
	const int index = indexOfUuid(uuid);
	if (index < 0) {
		return QString();
	}

	QStringList names;
	names << m_options.at(index).name;
	const QStringList above = ancestorUuids(uuid);
	for (const QString &ancestor : above) {
		names.prepend(m_options.at(indexOfUuid(ancestor)).name);
	}
	return names.join(QStringLiteral(" / "));
}

/**
	@brief OptionTree::isSwitchedOn
	@param uuid
	@return what the person clicked, stored as it was clicked
*/
bool OptionTree::isSwitchedOn(const QString &uuid) const
{
	const int index = indexOfUuid(uuid);
	return index < 0 ? false : m_options.at(index).switched_on;
}

/**
	@brief OptionTree::isActive
	@param uuid
	@return true when this option applies to the drawing

	An option applies only inside the one it refines: a brake exists on a
	bidirectional conveyor, and on nothing else. So the answer walks up,
	and one ancestor switched off is enough to say no - without the flag of
	the option itself being touched, which is what gives it back untouched
	when the ancestor comes back.
*/
bool OptionTree::isActive(const QString &uuid) const
{
	const int index = indexOfUuid(uuid);
	if (index < 0 || !m_options.at(index).switched_on) {
		return false;
	}

	const QStringList above = ancestorUuids(uuid);
	for (const QString &ancestor : above)
	{
		if (!isSwitchedOn(ancestor)) {
			return false;
		}
	}
	return true;
}

/**
	@brief OptionTree::activeUuids
	@return the uuid of every active option, parents before children

	Walked down from the roots rather than filtered over the flat list, for
	two reasons that are the same reason: the order has to be the tree's,
	and a branch whose head is off cannot hold an active option - so it is
	not walked at all.
*/
QStringList OptionTree::activeUuids() const
{
	QStringList list;
	QSet<QString> seen;

	QStringList pending = rootUuids();
	while (!pending.isEmpty())
	{
		const QString current = pending.takeFirst();
		if (seen.contains(current)) {
			continue;
		}
		seen.insert(current);

		if (!isSwitchedOn(current)) {
			continue;
		}
		list << current;

		const QStringList children = childUuids(current);
		for (int i = int(children.count()) - 1; i >= 0; --i) {
			pending.prepend(children.at(i));
		}
	}
	return list;
}

/**
	@brief OptionTree::activeNames
	@return the name of every active option, in the same order
*/
QStringList OptionTree::activeNames() const
{
	QStringList names;
	const QStringList uuids = activeUuids();
	for (const QString &uuid : uuids) {
		names << m_options.at(indexOfUuid(uuid)).name;
	}
	return names;
}

QDomElement OptionTree::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	for (const ProjectOption &option : m_options) {
		element.appendChild(option.toXml(document));
	}
	return element;
}

/**
	@brief OptionTree::fromXml
	@param element
	@return true when the element was ours

	Three things a file can hold that a tree cannot, and all three are
	repaired rather than refused: a parent that is not in the file, a branch
	that loops back into itself, and two options of the same level sharing a
	name. Refusing any of them would throw away the options that are sound,
	and the person who opens the project has done nothing wrong.
*/
bool OptionTree::fromXml(const QDomElement &element)
{
	clear();
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	QVector<ProjectOption> read;
	QStringList uuids;
	for (QDomElement child =
		     element.firstChildElement(ProjectOption::tagName());
	     !child.isNull();
	     child = child.nextSiblingElement(ProjectOption::tagName()))
	{
		ProjectOption option;
		if (!option.fromXml(child)) {
			continue;
		}
		if (option.uuid.isEmpty() || uuids.contains(option.uuid)) {
			option.uuid = newId();
		}
		uuids << option.uuid;
		read.append(option);
	}

	const int total = int(read.count());
	QHash<QString, int> index_of;
	for (int i = 0; i < total; ++i) {
		index_of.insert(read.at(i).uuid, i);
	}

		//A parent nobody wrote down, and an option that is its own parent,
		//both become options of the top level: visible and correctable,
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

		//Walking up from every option finds any loop, and cutting the link
		//of the option the walk came back to breaks it once and for all.
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

		//And a name has to name one option: what the file lost is given a
		//name, and what two siblings shared is numbered apart. A tree with
		//two switches called the same thing is a tree the person cannot
		//use, which is worse than a name they did not choose.
	for (int i = 0; i < total; ++i)
	{
		QString name = ProjectOption::sanitizeName(read.at(i).name);
		if (name.isEmpty()) {
			name = QStringLiteral("Option ") + QString::number(i + 1);
		}

		QString candidate = name;
		int suffix = 1;
		while (siblingTaken(read, i, read.at(i).parent_uuid, candidate))
		{
			++suffix;
			candidate = name + QStringLiteral(" (")
				    + QString::number(suffix)
				    + QStringLiteral(")");
		}
		read[i].name = candidate;
	}

	m_options = read;
	return true;
}

/**
	@brief OptionTree::tagName
	@return the name of the element that holds the whole tree
*/
QString OptionTree::tagName()
{
	return QStringLiteral("option_tree");
}

QString OptionTree::newId()
{
	return QUuid::createUuid().toString();
}

bool OptionTree::operator==(const OptionTree &other) const
{
	return m_options == other.m_options;
}

bool OptionTree::operator!=(const OptionTree &other) const
{
	return !(*this == other);
}

/**
	@brief OptionTree::indexOfSiblingName
	@param parent_uuid the level to look in
	@param name the name to look for
	@param except_uuid an option to skip, itself when it is being renamed
	@return the index of the sibling that answers to this name, -1 when none

	Case is folded away, for the reason the tree of locations folds it: two
	options called "Bidirectionnel" and "bidirectionnel" side by side are
	one option typed twice, and the person reading the tree cannot tell
	which switch they are about to click.
*/
int OptionTree::indexOfSiblingName(const QString &parent_uuid,
				   const QString &name,
				   const QString &except_uuid) const
{
	const int total = count();
	for (int i = 0; i < total; ++i)
	{
		const ProjectOption &option = m_options.at(i);
		if (option.parent_uuid != parent_uuid) {
			continue;
		}
		if (!except_uuid.isEmpty() && option.uuid == except_uuid) {
			continue;
		}
		if (option.name.compare(name, Qt::CaseInsensitive) == 0) {
			return i;
		}
	}
	return -1;
}

/**
	@brief OptionTree::isDescendantOf
	@param uuid
	@param ancestor_uuid
	@return true when ancestor_uuid is uuid itself or contains it
*/
bool OptionTree::isDescendantOf(const QString &uuid,
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
		current = m_options.at(index).parent_uuid;
	}
	return false;
}
