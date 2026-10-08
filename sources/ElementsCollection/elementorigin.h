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
#ifndef ELEMENTORIGIN_H
#define ELEMENTORIGIN_H

#include <QUuid>

class QDomElement;

/**
	@brief The ElementOrigin namespace
	Which version of a collection symbol a project's copy was taken from.

	Every save of a symbol writes a new uuid into its definition, so the
	uuid of a project's copy and the uuid of the file it came from differ
	as soon as either one is saved again - and the two uuids alone cannot
	say which side changed. A copy corrected from a folio and a symbol
	corrected in the collection look exactly the same.

	So each <element> of a project's collection carries, in the attribute
	origin_uuid, the uuid its definition had when it was taken from a
	collection. Comparing the three then says it:
	 - the collection's uuid is still the origin: the collection did not
	   change, whatever happened to the copy;
	 - the copy's uuid is still the origin: the copy was not touched, and a
	   different collection uuid is a newer version to bring;
	 - neither: both were changed, and nothing may be decided for the user.

	The attribute is written where a copy is taken (XmlElementCollection::
	addElement() and copy()) and, for the copies of a project saved before
	it existed, when the project is opened. A copy edited from a folio
	keeps it, which is the whole point. QElectroTech versions that do not
	know it leave it alone: it is an attribute of the collection, which a
	project writes back as it read it.
*/
namespace ElementOrigin
{
	QUuid definitionUuid(const QDomElement &definition);
	QUuid of(const QDomElement &collection_element);
	void set(QDomElement collection_element, const QUuid &origin);
	int stampMissing(const QDomElement &collection_directory);
}

#endif // ELEMENTORIGIN_H
