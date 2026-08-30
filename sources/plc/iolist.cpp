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
#include "iolist.h"

#include <QDomDocument>
#include <QDomElement>
#include <QSet>
#include <QUuid>

namespace
{
		/// The three fields the cascade of IoList::indexOfKey tries, in order.
	enum KeyKind
	{
		TagKey,
		AddressKey,
		DescriptionKey
	};

		/**
			@return what this point offers as a key of that kind, empty when
			it offers none.
			The description is folded before it is compared, the other two are
			not: a tag and an address are codes, and two codes that differ by
			an accent are two codes.
		*/
	QString keyValue(const IoPoint &point, KeyKind kind)
	{
		switch (kind)
		{
			case TagKey:
				return point.tag.trimmed();
			case AddressKey:
				return point.address.trimmed();
			case DescriptionKey:
				return IoPoint::normalize(point.description);
		}
		return QString();
	}

		/**
			@brief Write value into target, if the import is allowed to and if
			there is anything to write.
			@return true when target actually changed
		*/
	bool applyText(QString &target, const QString &value, bool allowed)
	{
		if (!allowed) {
			return false;
		}
		const QString trimmed = value.trimmed();
		if (trimmed.isEmpty()) {
			return false;
		}
		if (target == trimmed) {
			return false;
		}
		target = trimmed;
		return true;
	}
}

/**
	@brief IoList::MergeReport::isEmpty
	@return true when the import did nothing at all
*/
bool IoList::MergeReport::isEmpty() const
{
	return added.isEmpty()
		&& updated.isEmpty()
		&& unchanged.isEmpty()
		&& missing.isEmpty()
		&& ambiguous.isEmpty();
}

/**
	@brief IoList::MergeReport::text
	@return the whole report in one paragraph
*/
QString IoList::MergeReport::text() const
{
	QStringList lines;

	if (added.isEmpty() && updated.isEmpty()) {
		lines << tr("Aucun point d'E/S n'a été ajouté ni modifié.");
	} else {
		lines << tr("%1 point(s) ajouté(s), %2 mis à jour, %3 inchangé(s).")
				 .arg(added.count())
				 .arg(updated.count())
				 .arg(unchanged.count());
	}

	if (!ambiguous.isEmpty()) {
		lines << tr("%1 ligne(s) correspondaient à plusieurs points existants "
			    "et ont été ajoutées comme nouvelles.")
				 .arg(ambiguous.count());
	}

	if (!missing.isEmpty()) {
		lines << tr("%1 point(s) déjà présent(s) ne figurent pas dans cette "
			    "feuille et ont été conservés.")
				 .arg(missing.count());
	}

	return lines.join(QLatin1Char('\n'));
}

/**
	@brief IoList::IoList
*/
IoList::IoList()
{}

int IoList::count() const
{
	return m_points.count();
}

bool IoList::isEmpty() const
{
	return m_points.isEmpty();
}

/**
	@brief IoList::at
	@param index
	@return the point at index, a null point when index is not one
*/
const IoPoint &IoList::at(int index) const
{
	static const IoPoint null_point;
	if (index < 0 || index >= m_points.count()) {
		return null_point;
	}
	return m_points.at(index);
}

/**
	@brief IoList::point
	@param index
	@return a copy of the point at index
*/
IoPoint IoList::point(int index) const
{
	return at(index);
}

/**
	@brief IoList::setPoint
	@param index
	@param point
	@return true when index was one of ours
	The id of the stored point is kept whatever the caller handed in: it is
	the identity, and a widget that hands a point back must not be able to
	change what point it is.
*/
bool IoList::setPoint(int index, const IoPoint &point)
{
	if (index < 0 || index >= m_points.count()) {
		return false;
	}

	const QString id = m_points.at(index).id;
	m_points[index] = point;
	m_points[index].id = id;
	return true;
}

/**
	@brief IoList::indexOfId
	@param id
	@return where that point is, -1 when it is not here
*/
int IoList::indexOfId(const QString &id) const
{
	if (id.isEmpty()) {
		return -1;
	}
	for (int i = 0 ; i < m_points.count() ; ++i)
	{
		if (m_points.at(i).id == id) {
			return i;
		}
	}
	return -1;
}

/**
	@brief IoList::indexOfKey
	@param other
	@param ambiguous
	@return the index of the point other is talking about
*/
int IoList::indexOfKey(const IoPoint &other, bool *ambiguous) const
{
	if (ambiguous) {
		*ambiguous = false;
	}

	const KeyKind kinds[] = {TagKey, AddressKey, DescriptionKey};
	for (const KeyKind kind : kinds)
	{
		const QString key = keyValue(other, kind);
		if (key.isEmpty()) {
			continue;
		}

		int found = -1;
		for (int i = 0 ; i < m_points.count() ; ++i)
		{
			if (keyValue(m_points.at(i), kind) != key) {
				continue;
			}
			if (found >= 0)
			{
					//Two points answer to the same key. Neither of them is
					//it, and picking one would be picking at random.
				if (ambiguous) {
					*ambiguous = true;
				}
				return -1;
			}
			found = i;
		}

		if (found >= 0) {
			return found;
		}
	}

	return -1;
}

/**
	@brief IoList::append
	@param point
	@return the id the point ended up with
*/
QString IoList::append(IoPoint point)
{
	if (point.id.isEmpty()) {
		point.id = newId();
	}
	m_points.append(point);
	return point.id;
}

/**
	@brief IoList::removeAt
	@param index
	@return true when index was one of ours
*/
bool IoList::removeAt(int index)
{
	if (index < 0 || index >= m_points.count()) {
		return false;
	}
	m_points.remove(index);
	return true;
}

void IoList::clear()
{
	m_points.clear();
}

/**
	@brief IoList::unassigned
	@return the indexes of the points that are not in a card yet
	This is the working list of the person doing the assignment, and the
	reason the whole class exists.
*/
QList<int> IoList::unassigned() const
{
	QList<int> indexes;
	for (int i = 0 ; i < m_points.count() ; ++i)
	{
		if (!m_points.at(i).isAssigned()) {
			indexes.append(i);
		}
	}
	return indexes;
}

/**
	@brief IoList::merge
	@param incoming
	@param fields
	@return what the import did
*/
IoList::MergeReport IoList::merge(const QList<IoPoint> &incoming, IoFields fields)
{
	MergeReport report;
	QSet<int> claimed;

	for (const IoPoint &line : incoming)
	{
		if (line.isNull()) {
			continue;
		}

		bool is_ambiguous = false;
		int index = indexOfKey(line, &is_ambiguous);

			//Two lines of the same sheet that answer to one point are the
			//same case as one line answering to two points: the import
			//cannot tell which is which, and says so.
		if (index >= 0 && claimed.contains(index))
		{
			is_ambiguous = true;
			index = -1;
		}

		if (index < 0)
		{
			IoPoint fresh = line;
			fresh.id.clear();
			fresh.master_uuid.clear();
			fresh.io_index = -1;
			fresh.channel.clear();

			const QString id = append(fresh);
			report.added << id;
			if (is_ambiguous) {
				report.ambiguous << id;
			}
			claimed.insert(m_points.count() - 1);
			continue;
		}

		IoPoint &stored = m_points[index];
		bool changed = false;

		if (fields.testFlag(IoTypeField) && stored.type != line.type)
		{
			stored.type = line.type;
			changed = true;
		}
		if (fields.testFlag(IoTerminalField)
		    && stored.needs_terminal != line.needs_terminal)
		{
			stored.needs_terminal = line.needs_terminal;
			changed = true;
		}

		changed |= applyText(stored.tag, line.tag,
				     fields.testFlag(IoTagField));
		changed |= applyText(stored.description, line.description,
				     fields.testFlag(IoDescriptionField));
		changed |= applyText(stored.address, line.address,
				     fields.testFlag(IoAddressField));
		changed |= applyText(stored.card, line.card,
				     fields.testFlag(IoCardField));
		changed |= applyText(stored.connect_to, line.connect_to,
				     fields.testFlag(IoConnectField));
		changed |= applyText(stored.comment, line.comment,
				     fields.testFlag(IoCommentField));

		claimed.insert(index);
		if (changed) {
			report.updated << stored.id;
		} else {
			report.unchanged << stored.id;
		}
	}

		//What the sheet did not mention. Kept, and reported.
	for (int i = 0 ; i < m_points.count() ; ++i)
	{
		if (!claimed.contains(i)) {
			report.missing << m_points.at(i).id;
		}
	}

	return report;
}

/**
	@brief IoList::toXml
	@param document
	@return the list as one element
*/
QDomElement IoList::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	for (const IoPoint &point : m_points) {
		element.appendChild(point.toXml(document));
	}
	return element;
}

/**
	@brief IoList::fromXml
	@param element
	@return true when the element was one of ours

	Tolerant on purpose, the same way CircuitTable is: a project that never
	opened the I/O list has no such element, and no element means an empty
	list rather than an error.
*/
bool IoList::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	m_points.clear();
	for (QDomElement child = element.firstChildElement(IoPoint::tagName()) ;
	     !child.isNull() ;
	     child = child.nextSiblingElement(IoPoint::tagName()))
	{
		IoPoint point;
		if (!point.fromXml(child)) {
			continue;
		}
		if (point.id.isEmpty()) {
			point.id = newId();
		}
		m_points.append(point);
	}
	return true;
}

/**
	@brief IoList::tagName
	@return the name of the element that holds the whole list
*/
QString IoList::tagName()
{
	return QStringLiteral("io_list");
}

/**
	@brief IoList::newId
	@return an identity no other point has
*/
QString IoList::newId()
{
	return QUuid::createUuid().toString();
}

bool IoList::operator==(const IoList &other) const
{
	return m_points == other.m_points;
}

bool IoList::operator!=(const IoList &other) const
{
	return !(*this == other);
}
