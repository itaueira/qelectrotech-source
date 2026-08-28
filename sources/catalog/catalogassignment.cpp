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

#include <QObject>

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
	@brief CatalogAssignment::valuesForElement
	@param catalog
	@param part
	@param current : the information of the component as it stands
	@return the values the assignment writes, keeping what a person typed
*/
QHash<QString, QString> CatalogAssignment::valuesForElement(const Catalog &catalog,
							    const CatalogPart &part,
							    const QHash<QString, QString> &current)
{
	QHash<QString, QString> values = valuesForElement(catalog, part);
	if (values.isEmpty()) {
		return values;
	}

		//What the part the component carries today had written there. Only
		//that may be cleared; the rest of the information was typed by
		//somebody, and a part with an empty field is not an instruction to
		//delete it.
	QHash<QString, QString> previous;
	const QString previous_code = current.value(partCodeKey());
	if (!previous_code.isEmpty())
	{
		const int revision = current.value(partRevisionKey()).toInt();
		const CatalogPart previous_part =
				revision > 0 ? catalog.partByCode(previous_code, revision)
					     : catalog.partByCode(previous_code);
		if (!previous_part.isNull()) {
			previous = valuesForElement(catalog, previous_part);
		}
	}

	const QStringList keys = values.keys();
	for (const QString &key : keys)
	{
		if (!values.value(key).isEmpty()) {
				//The part has something to say about this field.
			continue;
		}
		const QString existing = current.value(key);
		if (existing.isEmpty()) {
				//Nothing to lose.
			continue;
		}
		if (previous.value(key) == existing) {
				//The previous part put it there: clearing it is the point.
			continue;
		}
			//Somebody typed it. Not ours to delete.
		values.remove(key);
	}

	return values;
}

/**
	@brief CatalogAssignment::commandLabel
	@param part
	@param tag : the component tag, as the drawing shows it
	@param symbol_name : the name of the symbol it was drawn with
	@return the sentence the undo list shows for a single assignment
*/
QString CatalogAssignment::commandLabel(const CatalogPart &part,
					const QString &tag,
					const QString &symbol_name)
{
	const QString who = tag.isEmpty() ? symbol_name : tag;
	return QObject::tr("Attribuer la pièce %1 à %2").arg(part.code, who);
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
	@brief CatalogAssignment::partFromValues
	@param catalog
	@param values
	@return the part @a values point to, brought up to date with what they say
*/
CatalogPart CatalogAssignment::partFromValues(const Catalog &catalog,
					      const QHash<QString, QString> &values)
{
	const QString code = values.value(partCodeKey()).trimmed();

	// The class is whatever the component already says, and what it says is
	// the part it points to. Component is for a component that points at
	// nothing yet.
	CatalogPart part = code.isEmpty() ? CatalogPart() : catalog.partByCode(code);
	if (part.isNull())
	{
		part = CatalogPart();
		const CatalogClass component_class =
				catalog.classByKey(QStringLiteral("component"));
		part.class_id = component_class.isNull() ? 0 : component_class.id;
		part.code = code;

		// Where it came from, said once: a part that already exists came from
		// somewhere else - a package, a price list - and keeps saying so.
		part.origin = QStringLiteral("project");
	}

	// What the draughtsman typed while drawing goes back into the part, but
	// only into fields the class of the part actually has. An empty field says
	// nothing, so it clears nothing: the same rule as assignment, the other
	// way round.
	const QList<CatalogProperty> properties = catalog.effectiveProperties(part.class_id);
	for (const CatalogProperty &property : properties)
	{
		const QString value = values.value(property.key).trimmed();
		if (!value.isEmpty()) {
			part.setValue(property.key, value);
		}
	}
	return part;
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
