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
#include "cablewire.h"

#include "../qetgraphicsitem/conductor.h"

#include <QDomDocument>
#include <QDomElement>

CableWire::CableWire()
{}

CableWire::CableWire(const QString &number) :
	m_number(number)
{}

/**
	@brief Write this wire.

	Strict on the way out: an attribute is written only when it carries
	something. A spare wire has no conductor attribute at all, which is what
	makes a file legible - a wire with no conductor attribute is a spare,
	and there is no second way of saying it.

	The state is not written, and that is deliberate. Whether a wire found
	its conductor is not a property of the wire, it is the answer the
	project gives when asked; deriving it again at every load is what lets a
	wire heal - the conductor comes back, an undone deletion for instance,
	and the wire is Occupied again with nothing to clean up. Writing the
	mark down would make a stale mark possible, and a stale mark is a lie
	that survives saves.
*/
QDomElement CableWire::toXml(QDomDocument &xml_document) const
{
	auto root_elmt = xml_document.createElement(xmlTagName());

	if (!m_number.isEmpty()) {
		root_elmt.setAttribute(QStringLiteral("number"), m_number);
	}
	if (!m_color.isEmpty()) {
		root_elmt.setAttribute(QStringLiteral("color"), m_color);
	}
	if (!m_section.isEmpty()) {
		root_elmt.setAttribute(QStringLiteral("section"), m_section);
	}
	if (!m_function.isEmpty()) {
		root_elmt.setAttribute(QStringLiteral("function"), m_function);
	}
	if (!m_conductor_uuid.isNull()) {
		root_elmt.setAttribute(QStringLiteral("conductor"), m_conductor_uuid.toString());
	}

	return root_elmt;
}

/**
	@brief Read this wire.

	Tolerant on the way in: every attribute is optional and an absent one
	leaves the default. A wire written by a version that did not know about
	sections still reads, and so does a project that has no cables at all -
	which is every project written before this.

	@return false only when @a xml_element is not a wire.
*/
bool CableWire::fromXml(const QDomElement &xml_element)
{
	if (xml_element.tagName() != xmlTagName()) {
		return false;
	}

	m_number   = xml_element.attribute(QStringLiteral("number"));
	m_color    = xml_element.attribute(QStringLiteral("color"));
	m_section  = xml_element.attribute(QStringLiteral("section"));
	m_function = xml_element.attribute(QStringLiteral("function"));

		//A malformed uuid reads as null, which makes the wire a spare
		//rather than failing the whole cable.
	setConductorUuid(QUuid::fromString(xml_element.attribute(QStringLiteral("conductor"))));

	return true;
}

void CableWire::setConductorUuid(const QUuid &uuid)
{
	m_conductor_uuid = uuid;
	m_conductor = nullptr;
	m_state = uuid.isNull() ? Reserved : Orphan;
}

void CableWire::setConductor(Conductor *conductor)
{
	if (!conductor)
	{
		setConductorUuid(QUuid());
		return;
	}

		//A conductor with a null uuid cannot be pointed at - there would be
		//nothing to write down and nothing to look up again - so it frees the
		//wire instead of half occupying it. Conductor::fromXml() mints one for
		//a project that has none, so the only way here is a conductor built in
		//memory that has not been through a load yet.
	if (conductor->uuid().isNull())
	{
		setConductorUuid(QUuid());
		return;
	}

	m_conductor_uuid = conductor->uuid();
	m_conductor = conductor;
	m_state = Occupied;
}

Conductor *CableWire::conductor() const
{
	return m_state == Occupied ? m_conductor.data() : nullptr;
}

void CableWire::setConductorLost()
{
	m_conductor = nullptr;
	m_state = m_conductor_uuid.isNull() ? Reserved : Orphan;
}
