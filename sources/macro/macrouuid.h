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
#ifndef MACROUUID_H
#define MACROUUID_H

#include <QHash>
#include <QString>
#include <QStringList>

class QDomElement;

/**
	@brief Reissues the uuids of a macro subtree, so that inserting the same
	macro twice draws two circuits instead of two copies of one.

	QElectroTech keeps the uuid written in the file when it reads a diagram
	back. Element::fromXml() says it in one line:

		m_uuid = QUuid(e.attribute("uuid", QUuid::createUuid().toString()));

	createUuid() is only the default value of the attribute: it applies when
	the attribute is absent, and a .qetmak has the attribute everywhere. So
	pasting one macro twenty times gives twenty elements sharing one uuid,
	twenty conductors sharing one uuid, and twenty dynamic texts sharing one
	uuid. That is not cosmetic. The uuid is how the program finds things
	again: a conductor stores the uuid of the element at each of its ends,
	and a master finds its slave through links_uuids. Twenty identical uuids
	wire the cross reference of circuit 1 to the contactor of circuit 17.

	renew() therefore walks a cloned subtree twice.

	The first pass collects "old to new" for every tag that *defines* a uuid:
	element, dynamic_elmt_text and conductor. The list is closed on purpose,
	see definingTagNames().

	The second pass rewrites every attribute *whose value* is a key of that
	map, whatever the attribute happens to be called. The distinction matters
	more than it sounds: a conductor carries the uuid of the element at each
	end under the names element1 and element2, not under the name uuid, and
	Diagram::fromXml() resolves a conductor end by looking for those uuids
	among the elements it has just added. Rewriting only what is called
	"uuid" would leave every conductor of circuit 2 attached to the elements
	of circuit 1.

	The same rule by value is what protects what must not move. terminal1 and
	terminal2 are uuids too, but of terminals, which are born in the .elmt
	definition and are therefore identical in every instance of a symbol:
	they are never in the map, so they are never touched. And a reference
	pointing *outside* the subtree is not in the map either, so a link to
	something already drawn survives the copy untouched, while a link inside
	the macro follows it.

	Like MacroSubstitution, this knows nothing of Diagram, Element or QETApp:
	it walks a QDomElement and holds QString, which is what lets the test
	binary link it with a handful of sources instead of half the program.
*/
namespace MacroUuid
{
	/**
		@brief What one renewal did.
	*/
	struct Result
	{
			/// how many uuids were reissued
		int definitions = 0;
			/// how many references followed them
		int references = 0;
			/**
				Old uuid, normalised, to new uuid, braced. The caller that
				has to remember what one insertion produced - to delete it
				again, or to regenerate one row of a table without touching
				the other nineteen - reads it here.
			*/
		QHash<QString, QString> map;
			/// the uuids issued, in document order
		QStringList issued;
	};

	/**
		@brief Give @a subtree a new identity, in place.
		@param subtree : the node to walk, normally the <diagram> a macro
		carries, already cloned. Only what is handed over is touched: the
		element definitions a .qetmak keeps in its <collection> are out of
		reach, as they are for MacroSubstitution, and for the same reason.
		@return what was reissued, and the map from old to new
	*/
	Result renew(QDomElement &subtree);

	/**
		@brief The tags that define a uuid of their own.
		@return element, dynamic_elmt_text and conductor

		The list is closed, and that is the safe way round. A tag that
		defines a uuid and is missing here simply keeps the duplicate this
		file exists to remove - the bug stays as it was. A tag that only
		*refers* to a uuid and were listed here by mistake would be given a
		brand new one, and the thing it pointed at would be lost without a
		word. terminal_strip_item is the live example: it stores the uuid of
		a terminal strip that lives at project level, outside any subtree
		this function ever sees.
	*/
	QStringList definingTagNames();

	/**
		@brief The comparable form of a uuid.
		@param uuid : as written in the file
		@return without braces, trimmed, lower case

		The files are not consistent: Element writes "{...}" and
		WiringListExport strips the braces before comparing. Keys are
		normalised so that the two spellings meet.
	*/
	QString normalised(const QString &uuid);

	/**
		@brief Write @a uuid the way @a written was written.
		@param written : the value being replaced
		@param uuid : the new value, braced
		@return @a uuid with braces if @a written had them, without if not

		A file that stored bare uuids goes on storing bare uuids.
	*/
	QString shapedLike(const QString &written, const QString &uuid);
}

#endif // MACROUUID_H
