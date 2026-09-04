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
#include "cable.h"

#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetproject.h"
#include "../qetxml.h"
#include "cablereport.h"

#include <QDomDocument>
#include <QDomElement>

namespace
{
		/// How a cable is named in a report line: its label, its uuid failing that
	QString reportCableName(const Cable *cable)
	{
		return cable->label().isEmpty() ? cable->uuid().toString()
						: cable->label();
	}

		/// How a wire is named in a report line: its number, its rank failing that
	QString reportWireName(const CableWire &wire, int index)
	{
		return wire.number().isEmpty() ? QString::number(index + 1)
					       : wire.number();
	}
}

Cable::Cable(QETProject *project) :
	QObject(project),
	m_project(project)
{}

Cable::Cable(const QString &label, QETProject *project) :
	QObject(project),
	m_label(label),
	m_project(project)
{}

QETProject *Cable::project() const
{
	return m_project.data();
}

void Cable::setWireCount(int count)
{
	if (count < 0) {
		count = 0;
	}

	while (m_wires.count() > count) {
		m_wires.removeLast();
	}
	while (m_wires.count() < count) {
		m_wires.append(CableWire(QString::number(m_wires.count() + 1)));
	}
}

CableWire Cable::wire(int index) const
{
	if (index < 0 || index >= m_wires.count()) {
		return CableWire();
	}

	return m_wires.at(index);
}

CableWire Cable::wireByNumber(const QString &number) const
{
	for (const CableWire &wire : m_wires)
	{
		if (wire.number() == number) {
			return wire;
		}
	}

	return CableWire();
}

bool Cable::setWire(int index, const CableWire &wire)
{
	if (index < 0 || index >= m_wires.count()) {
		return false;
	}

	m_wires[index] = wire;
	return true;
}

void Cable::appendWire(const CableWire &wire)
{
	m_wires.append(wire);
}

QVector<CableWire> Cable::lostWires() const
{
	QVector<CableWire> lost;

	for (const CableWire &wire : m_wires)
	{
		if (wire.hasLostConductor()) {
			lost.append(wire);
		}
	}

	return lost;
}

QList<Diagram *> Cable::folios() const
{
	QList<Diagram *> folio_list;

	if (!m_project) {
		return folio_list;
	}

		//Walked in the order of the project and not in the order of the
		//wires, so that a cable entering on folio 3 and leaving on folio 7
		//answers 3 then 7 whatever order its wires happen to be in.
	const QList<Diagram *> project_folios = m_project->diagrams();
	for (Diagram *folio : project_folios)
	{
		for (const CableWire &wire : m_wires)
		{
			const Conductor *conductor = wire.conductor();
			if (conductor && conductor->diagram() == folio)
			{
				folio_list.append(folio);
				break;
			}
		}
	}

	return folio_list;
}

/**
	@brief Write this cable, with its wires under it.

	Strict: an attribute that carries nothing is not written, so a cable
	with no part code has no part attribute and a cable that is not shielded
	has no shielded attribute. The uuid is always written, because it is
	what the file uses to point at this cable.

	The wire_count attribute is a trace for whoever reads the file, and is
	never read back - the wires under the element are the only count there
	is. Same treatment CatalogClassPackage gives the uuid of an exported
	class: written down so the file says where it came from, and never used
	to decide anything on the way in.
*/
QDomElement Cable::toXml(QDomDocument &xml_document) const
{
	auto root_elmt = xml_document.createElement(xmlTagName());

	root_elmt.setAttribute(QStringLiteral("uuid"), m_uuid.toString());

	if (!m_label.isEmpty()) {
		root_elmt.setAttribute(QStringLiteral("label"), m_label);
	}
	if (!m_part_code.isEmpty()) {
		root_elmt.setAttribute(QStringLiteral("part"), m_part_code);
	}
	if (m_shielded) {
		root_elmt.setAttribute(QStringLiteral("shielded"), QStringLiteral("true"));
	}
	root_elmt.setAttribute(QStringLiteral("wire_count"),
			       QString::number(m_wires.count()));

	for (const CableWire &wire : m_wires) {
		root_elmt.appendChild(wire.toXml(xml_document));
	}

	return root_elmt;
}

/**
	@brief Read this cable, with its wires under it.

	Tolerant: every attribute is optional. A cable written without a part
	code, without a label or without wires reads, and so does a project that
	has no cable block at all - which is every project written before this
	existed.

	Reading does not link the wires to their conductors: that is resolve(),
	and it has to happen after the folios are in memory. Doing it here would
	find no conductor at all and turn every wire of every cable into a
	spare, which is the silent loss this class is written to avoid.

	@return false only when @a xml_element is not a cable.
*/
bool Cable::fromXml(const QDomElement &xml_element)
{
	if (xml_element.tagName() != xmlTagName()) {
		return false;
	}

	m_uuid = QUuid::fromString(xml_element.attribute(QStringLiteral("uuid")));
	if (m_uuid.isNull())
	{
			//Absent, empty or malformed: mint one, the same treatment
			//conductor uuids get in Conductor::fromXml(). A null uuid is not
			//a usable identity, and a cable nothing can point at is worse
			//than a cable carrying a fresh identity.
		m_uuid = QUuid::createUuid();
	}

	m_label     = xml_element.attribute(QStringLiteral("label"));
	m_part_code = xml_element.attribute(QStringLiteral("part"));

	const QString shielded = xml_element.attribute(QStringLiteral("shielded"));
	m_shielded = shielded.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
		     || shielded == QLatin1String("1");

	m_wires.clear();
	const QVector<QDomElement> xml_wires =
			QETXML::directChild(xml_element, CableWire::xmlTagName());
	for (const QDomElement &xml_wire : xml_wires)
	{
		CableWire wire;
		if (wire.fromXml(xml_wire)) {
			m_wires.append(wire);
		}
	}

	return true;
}

QHash<QUuid, Conductor *> Cable::conductorsOf(const QETProject *project,
					      CableReport *report)
{
	QHash<QUuid, Conductor *> index;

	if (!project) {
		return index;
	}

	const QList<Diagram *> folios = project->diagrams();
	for (Diagram *folio : folios)
	{
		const QList<Conductor *> conductors = folio->conductors();
		for (Conductor *conductor : conductors)
		{
			const QUuid uuid = conductor->uuid();
			if (uuid.isNull()) {
					//Nothing can point at it, so it cannot answer for a wire.
				continue;
			}

			if (index.contains(uuid))
			{
					//Two conductors answering to one identity. The first one
					//is kept so that the answer at least does not move
					//between runs, and the fact is counted - a wire linked to
					//the wrong one of the two would otherwise look perfectly
					//linked.
				if (report) {
					report->duplicated_conductors += 1;
				}
				continue;
			}

			index.insert(uuid, conductor);
		}
	}

	return index;
}

void Cable::resolve(const QHash<QUuid, Conductor *> &index, CableReport *report)
{
	if (report) {
		report->cables += 1;
	}

	for (int i = 0 ; i < m_wires.count() ; ++i)
	{
		CableWire &wire = m_wires[i];

		if (wire.conductorUuid().isNull())
		{
				//A wire the designer left empty: a spare, and nothing to
				//look for.
			if (report) {
				report->wires_reserved += 1;
			}
			continue;
		}

		Conductor *conductor = index.value(wire.conductorUuid(), nullptr);
		if (conductor)
		{
			wire.setConductor(conductor);
			if (report) {
				report->wires_occupied += 1;
			}
			continue;
		}

			//The wire names a conductor and no conductor answers. This is
			//where TerminalStrip::fromXml() has no else branch and drops the
			//member; here the wire is kept, marked as having lost its
			//conductor - which is not the same thing as being a spare - and
			//named in the report. The uuid is kept too, so the wire is whole
			//again the day the conductor comes back.
		wire.setConductorLost();

		if (report)
		{
			report->wires_orphan += 1;
			report->orphans.append(
					tr("cable %1, wire %2: the conductor %3 is not in the project any more")
					.arg(reportCableName(this),
					     reportWireName(wire, i),
					     wire.conductorUuid().toString()));
		}
	}
}

void Cable::resolve(CableReport *report)
{
	resolve(conductorsOf(m_project.data(), report), report);
}
