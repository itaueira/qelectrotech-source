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
#include "catalogassignment.h"

#include "catalog.h"

/**
	@brief CatalogAssignment::partCodeKey
	@return the element information key holding the part code
*/
QString CatalogAssignment::partCodeKey()
{
	return QStringLiteral("part_code");
}

/**
	@brief CatalogAssignment::accessoryOwnerKey
	@return the element information key holding the uuid of the component an
	accessory belongs to
*/
QString CatalogAssignment::accessoryOwnerKey()
{
	return QStringLiteral("accessory_of");
}

/**
	@brief CatalogAssignment::partRevisionKey
	@return the element information key holding the part revision
*/
QString CatalogAssignment::partRevisionKey()
{
	return QStringLiteral("part_revision");
}

/**
	@brief CatalogAssignment::protectedElementKeys
	@return the keys a part assignment never writes
*/
QStringList CatalogAssignment::protectedElementKeys()
{
	return { QStringLiteral("label"),
		 QStringLiteral("formula"),
		 QStringLiteral("auto_num_locked"),
		 QStringLiteral("potential_isolating"),
		 QStringLiteral("exclude_from_bom"),
		 QStringLiteral("plant"),
		 QStringLiteral("location") };
}

/**
	@brief CatalogAssignment::valuesForElement
	@param catalog
	@param part
	@return the values the assignment writes into the component
*/
QHash<QString, QString> CatalogAssignment::valuesForElement(const Catalog &catalog,
							    const CatalogPart &part)
{
	QHash<QString, QString> values;
	if (part.isNull()) {
		return values;
	}

	const QStringList protected_keys = protectedElementKeys();
	const QHash<QString, QString> effective = catalog.effectiveValues(part);
	const QStringList keys = effective.keys();

	for (const QString &key : keys)
	{
		if (protected_keys.contains(key)) {
			continue;
		}
		// An empty value is written too: assigning another part has to clear
		// what the previous one had put there, otherwise the component keeps
		// the manufacturer of a product it no longer is.
		values.insert(key, effective.value(key));
	}

	values.insert(partCodeKey(), part.code);
	values.insert(partRevisionKey(), QString::number(part.revision));

	return values;
}

/**
	@brief CatalogAssignment::terminalNames
	@param part
	@param group
	@param terminal_count
	@return one name per terminal, empty where the symbol keeps its own label
*/
QStringList CatalogAssignment::terminalNames(const CatalogPart &part,
					     const QString &group,
					     int terminal_count)
{
	QStringList names;
	if (terminal_count <= 0) {
		return names;
	}

	// Pins of the asked group, in pin order. A part registered from a project
	// records which sub symbol each pin came from, so a contactor with a coil
	// and four auxiliary contacts gives each symbol its own numbers.
	QList<CatalogPin> pins;
	for (const CatalogPin &pin : part.pins)
	{
		if (pin.group == group) {
			pins.append(pin);
		}
	}

	// Nothing recorded for this group: fall back to the pins that belong to no
	// group at all, which is what a part drawn as a single symbol has.
	if (pins.isEmpty() && !group.isEmpty())
	{
		for (const CatalogPin &pin : part.pins)
		{
			if (pin.group.isEmpty()) {
				pins.append(pin);
			}
		}
	}

	for (int index = 0 ; index < terminal_count ; ++index)
	{
		names.append(index < pins.size() ? pins.at(index).label : QString());
	}
	return names;
}

/**
	@brief CatalogAssignment::isWithoutPart
	@param values
	@return true when no catalog part is assigned
*/
bool CatalogAssignment::isWithoutPart(const QHash<QString, QString> &values)
{
	return values.value(partCodeKey()).trimmed().isEmpty();
}
