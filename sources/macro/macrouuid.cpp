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
#include "macrouuid.h"

#include <QDomElement>
#include <QDomNamedNodeMap>
#include <QDomNode>
#include <QUuid>

/**
	@brief Collect the uuids @a node and its children define.
	@param node : where to start
	@param result : receives the map and the issued list

	Nothing is written here. A uuid met twice keeps the first new value it
	was given, which is what makes a malformed file - the same uuid on two
	elements, which is precisely the state this file exists to fix - come out
	sane instead of half remapped.
*/
static void collect(const QDomNode &node, MacroUuid::Result &result)
{
	if (node.isNull()) {
		return;
	}

	if (node.isElement())
	{
		const QDomElement element = node.toElement();
		if (MacroUuid::definingTagNames().contains(element.tagName()))
		{
			const QString written = element.attribute(QStringLiteral("uuid"));
			const QString key = MacroUuid::normalised(written);
			if (!key.isEmpty()
			    && !QUuid(written).isNull()
			    && !result.map.contains(key))
			{
				const QString issued = QUuid::createUuid().toString();
				result.map.insert(key, issued);
				result.issued << issued;
			}
		}
	}

	for (QDomNode child = node.firstChild() ;
	     !child.isNull() ;
	     child = child.nextSibling())
	{
		collect(child, result);
	}
}

/**
	@brief Rewrite every attribute of @a node whose value is a known uuid.
	@param node : where to start
	@param result : holds the map, and receives the counts
*/
static void rewrite(QDomNode node, MacroUuid::Result &result)
{
	if (node.isNull()) {
		return;
	}

	if (node.isElement())
	{
		QDomElement element = node.toElement();
		const bool defines =
				MacroUuid::definingTagNames().contains(element.tagName());
		const QDomNamedNodeMap attributes = element.attributes();
			//The names are read first: writing an attribute back while
			//walking the map is asking the map to change under the walk.
		QStringList names;
		names.reserve(attributes.count());
		for (int i = 0 ; i < attributes.count() ; ++ i) {
			names << attributes.item(i).nodeName();
		}
		for (const QString &name : names)
		{
			const QString written = element.attribute(name);
			const QString key = MacroUuid::normalised(written);
			if (key.isEmpty() || !result.map.contains(key)) {
				continue;
			}
			element.setAttribute(name,
					     MacroUuid::shapedLike(written,
								   result.map.value(key)));
			if (defines && name == QLatin1String("uuid")) {
				++ result.definitions;
			} else {
				++ result.references;
			}
		}
	}

	for (QDomNode child = node.firstChild() ;
	     !child.isNull() ;
	     child = child.nextSibling())
	{
		rewrite(child, result);
	}
}

/**
	@brief MacroUuid::renew
	@param subtree
	@return what was reissued, and the map from old to new
*/
MacroUuid::Result MacroUuid::renew(QDomElement &subtree)
{
	MacroUuid::Result result;
	if (subtree.isNull()) {
		return result;
	}

	collect(subtree, result);
	if (result.map.isEmpty()) {
		return result;
	}
	rewrite(subtree, result);

	return result;
}

/**
	@brief MacroUuid::definingTagNames
	@return the tags that own a uuid, as opposed to those that quote one
*/
QStringList MacroUuid::definingTagNames()
{
	static const QStringList names{QStringLiteral("element"),
				       QStringLiteral("dynamic_elmt_text"),
				       QStringLiteral("conductor")};
	return names;
}

/**
	@brief MacroUuid::normalised
	@param uuid
	@return the comparable form: no braces, trimmed, lower case
*/
QString MacroUuid::normalised(const QString &uuid)
{
	QString normal = uuid;
	normal.remove(QLatin1Char('{'));
	normal.remove(QLatin1Char('}'));
	return normal.trimmed().toLower();
}

/**
	@brief MacroUuid::shapedLike
	@param written
	@param uuid
	@return @a uuid spelled the way @a written was spelled
*/
QString MacroUuid::shapedLike(const QString &written, const QString &uuid)
{
	if (written.trimmed().startsWith(QLatin1Char('{'))) {
		return uuid;
	}

	QString bare = uuid;
	bare.remove(QLatin1Char('{'));
	bare.remove(QLatin1Char('}'));
	return bare;
}
