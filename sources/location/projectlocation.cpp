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
#include "projectlocation.h"

#include <QDomDocument>
#include <QDomElement>
#include <QStringList>

ProjectLocation::ProjectLocation()
{}

/**
	@brief ProjectLocation::ProjectLocation
	@param location_code the designation that goes after the plus sign
	@param location_name what the storeroom reads, the code again when empty
*/
ProjectLocation::ProjectLocation(const QString &location_code,
				 const QString &location_name) :
	code(sanitizeCode(location_code)),
	name(location_name)
{}

/**
	@brief ProjectLocation::isNull
	@return true when there is no code

	A location without a code has no path, and a path is the only thing a
	component ever holds - so a location without one cannot be pointed at,
	whatever else it carries.
*/
bool ProjectLocation::isNull() const
{
	return code.isEmpty();
}

/**
	@brief ProjectLocation::isVirtual
	@return true when this location must not produce a line of its own
*/
bool ProjectLocation::isVirtual() const
{
	return virtual_part || part_code.isEmpty();
}

/**
	@brief ProjectLocation::toXml
	@param document
	@return the location as one element, empty fields left out
*/
QDomElement ProjectLocation::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());

	if (!uuid.isEmpty()) {
		element.setAttribute(QStringLiteral("uuid"), uuid);
	}
	if (!parent_uuid.isEmpty()) {
		element.setAttribute(QStringLiteral("parent"), parent_uuid);
	}
	element.setAttribute(QStringLiteral("code"), code);
	if (!name.isEmpty()) {
		element.setAttribute(QStringLiteral("name"), name);
	}
	if (!description.isEmpty()) {
		element.setAttribute(QStringLiteral("description"), description);
	}
	if (!part_code.isEmpty())
	{
		element.setAttribute(QStringLiteral("part"), part_code);
		if (part_revision > 0) {
			element.setAttribute(QStringLiteral("revision"),
					     QString::number(part_revision));
		}
	}
	if (virtual_part) {
		element.setAttribute(QStringLiteral("virtual"),
				     QStringLiteral("true"));
	}

	return element;
}

/**
	@brief ProjectLocation::fromXml
	@param element
	@return true when the element was one of ours

	Tolerant on purpose, as every read in this program is: a code that came
	back with the spaces somebody left around it is folded, and a revision
	that came back as nonsense becomes the current one rather than refusing
	the whole location. What the tree does with a code that is still empty
	afterwards is the tree's business, not this one's.
*/
bool ProjectLocation::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	uuid        = element.attribute(QStringLiteral("uuid"));
	parent_uuid = element.attribute(QStringLiteral("parent"));
	code        = sanitizeCode(element.attribute(QStringLiteral("code")));
	name        = element.attribute(QStringLiteral("name"));
	description = element.attribute(QStringLiteral("description"));
	part_code   = element.attribute(QStringLiteral("part"));

	bool ok = false;
	const int revision = element.attribute(QStringLiteral("revision"))
			     .toInt(&ok);
	part_revision = (ok && revision > 0) ? revision : 0;

	virtual_part = element.attribute(QStringLiteral("virtual"))
		       == QLatin1String("true");

	return true;
}

/**
	@brief ProjectLocation::tagName
	@return the name of the element that holds one location
*/
QString ProjectLocation::tagName()
{
	return QStringLiteral("location");
}

/**
	@brief ProjectLocation::separator
	@return the character that joins two codes into a path
*/
QString ProjectLocation::separator()
{
	return QStringLiteral("/");
}

/**
	@brief ProjectLocation::isValidCode
	@param location_code
	@param error
	@return true when the code is usable as one step of a path
*/
bool ProjectLocation::isValidCode(const QString &location_code, QString *error)
{
	const QString clean = sanitizeCode(location_code);

	if (clean.isEmpty())
	{
		if (error) {
			*error = tr("Le code d'une localisation ne peut pas "
				    "être vide.");
		}
		return false;
	}

		//The four characters below are the prefixes of IEC 81346, and the
		//fifth is what joins two codes into a path. A code holding any of
		//them would make the tag it produces impossible to read back.
	const QString forbidden = QStringLiteral("=+-:/");
	for (const QChar &character : forbidden)
	{
		if (clean.contains(character))
		{
			if (error) {
				*error = tr("Le code d'une localisation ne peut "
					    "pas contenir « %1 ».")
					 .arg(character);
			}
			return false;
		}
	}

	return true;
}

/**
	@brief ProjectLocation::sanitizeCode
	@param location_code
	@return the code without the spaces a person leaves behind
*/
QString ProjectLocation::sanitizeCode(const QString &location_code)
{
	return location_code.simplified();
}

bool ProjectLocation::operator==(const ProjectLocation &other) const
{
	return uuid == other.uuid
		&& parent_uuid == other.parent_uuid
		&& code == other.code
		&& name == other.name
		&& description == other.description
		&& part_code == other.part_code
		&& part_revision == other.part_revision
		&& virtual_part == other.virtual_part;
}

bool ProjectLocation::operator!=(const ProjectLocation &other) const
{
	return !(*this == other);
}
