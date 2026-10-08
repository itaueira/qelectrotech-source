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
#include "elementorigin.h"

#include <QDomElement>

namespace {

const QString origin_attribute = QStringLiteral("origin_uuid");

}

/**
	@brief ElementOrigin::definitionUuid
	@param definition : a <definition>
	@return the uuid it carries, null when it carries none
*/
QUuid ElementOrigin::definitionUuid(const QDomElement &definition)
{
	return QUuid(definition.firstChildElement(QStringLiteral("uuid"))
				 .attribute(QStringLiteral("uuid")));
}

/**
	@brief ElementOrigin::of
	@param collection_element : an <element> of a project's collection
	@return the uuid its definition had when it was taken from a
	collection, null when it had none. A copy that was never stamped
	answers with its own uuid: taken as it is, the way stampMissing() would
	have stamped it.
*/
QUuid ElementOrigin::of(const QDomElement &collection_element)
{
		//A record of a definition without uuid is a null uuid, and it is
		//still a record: falling back to the copy's uuid would take the
		//uuid a correction from a folio gives it for the origin
	if (collection_element.hasAttribute(origin_attribute)) {
		return QUuid(collection_element.attribute(origin_attribute));
	}
	return definitionUuid(collection_element.firstChildElement(QStringLiteral("definition")));
}

/**
	@brief ElementOrigin::set
	Record @p origin as the version @p collection_element was taken from.
	A null @p origin is recorded too: the version taken carried no uuid.
*/
void ElementOrigin::set(QDomElement collection_element, const QUuid &origin)
{
	collection_element.setAttribute(origin_attribute, origin.toString());
}

/**
	@brief ElementOrigin::stampMissing
	Give every <element> under @p collection_directory that has no record
	the uuid of its own definition: a copy that was just taken, or one of a
	project saved before the record existed, is taken to be the version it
	came from. A record already there is never changed.

	A definition without uuid - old projects have many - is stamped with
	a null uuid. Left unstamped, the first correction from a folio would
	give the copy a uuid, of() would answer with it, and the copy would
	look untouched.
	@return the number of elements stamped
*/
int ElementOrigin::stampMissing(const QDomElement &collection_directory)
{
	int stamped = 0;
	for (QDomElement child = collection_directory.firstChildElement();
		 !child.isNull();
		 child = child.nextSiblingElement())
	{
		if (child.tagName() == QLatin1String("category")) {
			stamped += stampMissing(child);
		}
		else if (child.tagName() == QLatin1String("element")
				 && !child.hasAttribute(origin_attribute))
		{
			set(child, definitionUuid(child.firstChildElement(QStringLiteral("definition"))));
			++stamped;
		}
	}
	return stamped;
}
