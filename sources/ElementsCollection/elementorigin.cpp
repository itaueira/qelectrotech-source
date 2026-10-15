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

#include <QCryptographicHash>
#include <QDomElement>
#include <QStringList>

namespace {

const QString origin_attribute = QStringLiteral("origin_uuid");
const QString hash_attribute = QStringLiteral("origin_hash");

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
		else if (child.tagName() == QLatin1String("element"))
		{
			const QDomElement definition = child.firstChildElement(QStringLiteral("definition"));
			if (!child.hasAttribute(origin_attribute))
			{
				set(child, definitionUuid(definition));
				++stamped;
			}
				//The content is only known while the copy is still the version
				//it was taken as
			if (!child.hasAttribute(hash_attribute)
					&& definitionUuid(definition) == of(child)) {
				setHash(child, contentHash(definition));
			}
		}
	}
	return stamped;
}

/**
	@brief ElementOrigin::canonicalText
	@p element written out the same way whatever the order of its
	attributes and whatever its indentation: tag, attributes sorted by name,
	then its children in order, text made only of spaces left out.
	@param element : the element to write
	@param without_uuids : leave out every uuid attribute and every <uuid>
	child, which say which version a symbol is and not what it is
	@return the text
*/
QString ElementOrigin::canonicalText(const QDomElement &element, bool without_uuids)
{
	QStringList attributes;
	const QDomNamedNodeMap map = element.attributes();
	for (int i = 0 ; i < map.count() ; ++i)
	{
		const QDomAttr attribute = map.item(i).toAttr();
		if (without_uuids && attribute.name() == QLatin1String("uuid")) {
			continue;
		}
		attributes << attribute.name() + QStringLiteral("=\"") + attribute.value() + QLatin1Char('"');
	}
	attributes.sort();

	QString text = QLatin1Char('<') + element.tagName();
	if (!attributes.isEmpty()) {
		text += QLatin1Char(' ') + attributes.join(QLatin1Char(' '));
	}
	text += QLatin1Char('>');
	for (QDomNode child = element.firstChild() ; !child.isNull() ; child = child.nextSibling())
	{
		if (child.isElement())
		{
			const QDomElement child_element = child.toElement();
			if (without_uuids && child_element.tagName() == QLatin1String("uuid")) {
				continue;
			}
			text += canonicalText(child_element, without_uuids);
		}
			//The indentation between two tags is kept by a project's
			//collection and dropped by a file read from the disk: it says
			//nothing of the symbol
		else if ((child.isText() || child.isCDATASection())
				 && !child.nodeValue().trimmed().isEmpty()) {
			text += child.nodeValue();
		}
	}
	return text + QStringLiteral("</") + element.tagName() + QLatin1Char('>');
}

/**
	@brief ElementOrigin::contentHash
	@return a digest of what @p definition draws and says, every uuid left
	out: two saves of the same symbol give the same digest, and any change to
	it gives another
*/
QString ElementOrigin::contentHash(const QDomElement &definition)
{
	return QString::fromLatin1(
				QCryptographicHash::hash(canonicalText(definition, true).toUtf8(),
										 QCryptographicHash::Sha1).toHex());
}

/**
	@brief ElementOrigin::hashOf
	@param collection_element : an <element> of a project's collection
	@return the digest of the content its definition had when it was taken
	from a collection, empty when that is not known
*/
QString ElementOrigin::hashOf(const QDomElement &collection_element)
{
	return collection_element.attribute(hash_attribute);
}

/**
	@brief ElementOrigin::setHash
	Record @p hash as the content @p collection_element was taken with
*/
void ElementOrigin::setHash(QDomElement collection_element, const QString &hash)
{
	collection_element.setAttribute(hash_attribute, hash);
}
