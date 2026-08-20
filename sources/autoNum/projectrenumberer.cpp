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
#include "projectrenumberer.h"

#include "../catalog/catalog.h"
#include "../catalog/catalogassignment.h"
#include "../diagram.h"
#include "../diagramposition.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "../undocommand/renumbercommand.h"
#include "iecstructure.h"

#include <QUndoStack>

namespace
{
	/// The element types a renumbering touches: the things that get a tag.
	bool isTaggedComponent(const Element *element)
	{
		if (!element) {
			return false;
		}
		const ElementData::Type type = element->elementData().m_type;
		return type == ElementData::Simple
		       || type == ElementData::Master
		       || type == ElementData::Slave;
	}
}

/**
	@brief ProjectRenumberer::components
	@param project
	@param selection
	@return the components to renumber
*/
QList<Element *> ProjectRenumberer::components(QETProject *project,
					       const QList<Element *> &selection)
{
	QList<Element *> found;

	if (!selection.isEmpty())
	{
		for (Element *element : selection)
		{
			if (isTaggedComponent(element)) {
				found.append(element);
			}
		}
		return found;
	}

	if (!project) {
		return found;
	}

	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
		const QList<QGraphicsItem *> items = diagram->items();
		for (QGraphicsItem *item : items)
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
			if (isTaggedComponent(element)) {
				found.append(element);
			}
		}
	}
	return found;
}

/**
	@brief ProjectRenumberer::rootOfLabel
	@param label
	@return the letters at the start of @a label
*/
QString ProjectRenumberer::rootOfLabel(const QString &label)
{
	QString root;
	for (const QChar character : label)
	{
		if (!character.isLetter()) {
			break;
		}
		root.append(character);
	}
	return root;
}

/**
	@brief ProjectRenumberer::rootFor
	@param catalog
	@param element
	@param iec
	@return the tag root to use
*/
QString ProjectRenumberer::rootFor(const Catalog &catalog, const Element *element, bool iec)
{
	if (!element) {
		return QString();
	}

	const DiagramContext information = element->elementInformations();

	// 1. The class of the assigned catalog part. This is the one the plan
	//    calls for: switching the project between the house standard and the
	//    norm becomes reading the other column of the class.
	const QString part_code =
		information.value(CatalogAssignment::partCodeKey()).toString().trimmed();
	if (!part_code.isEmpty() && catalog.isOpen())
	{
		const CatalogPart part = catalog.partByCode(part_code);
		if (!part.isNull())
		{
			const QString root = catalog.tagRoot(part.class_id, iec);
			if (!root.isEmpty()) {
				return root;
			}
		}
	}

	// 2. The letters the tag already starts with. A project drawn before the
	//    catalog existed keeps numbering the way it always did, which is what
	//    makes turning this on safe.
	const QString label = information.value(QStringLiteral("label")).toString();
	const QString from_label = rootOfLabel(label);
	if (!from_label.isEmpty()) {
		return from_label;
	}

	// 3. Nothing. The format then produces a bare number, which is what
	//    QElectroTech already does for a symbol it knows nothing about.
	return QString();
}

/**
	@brief ProjectRenumberer::isFrozen
	@param element
	@return true when this component was numbered by hand
*/
bool ProjectRenumberer::isFrozen(const Element *element)
{
	if (!element) {
		return false;
	}
	return element->elementInformations()
		.value(QStringLiteral("auto_num_locked")).toBool();
}

/**
	@brief ProjectRenumberer::formatFor
	@param catalog
	@param element
	@param fallback
	@return the numbering format of the class of @a element
*/
NumberingFormat ProjectRenumberer::formatFor(const Catalog &catalog,
					     const Element *element,
					     const NumberingFormat &fallback)
{
	if (!element || !catalog.isOpen()) {
		return fallback;
	}

	const QString part_code = element->elementInformations()
				  .value(CatalogAssignment::partCodeKey()).toString().trimmed();
	if (part_code.isEmpty()) {
		return fallback;
	}

	const CatalogPart part = catalog.partByCode(part_code);
	if (part.isNull()) {
		return fallback;
	}

	// Walk up from the class of the part: a subclass that says nothing about
	// numbering follows its mother, the same way it follows her tag root.
	const QList<int> ancestry = catalog.classAncestry(part.class_id);
	for (int index = ancestry.size() - 1 ; index >= 0 ; --index)
	{
		const CatalogClass catalog_class = catalog.classById(ancestry.at(index));
		if (catalog_class.numbering_format.isEmpty()) {
			continue;
		}
		const NumberingFormat format =
			NumberingFormat::fromXml(catalog_class.numbering_format);
		if (format.isValid()) {
			return format;
		}
	}

	return fallback;
}

/**
	@brief ProjectRenumberer::inputsFor
	@param catalog
	@param elements
	@param fallback
	@return one input per element
*/
QList<RenumberInput> ProjectRenumberer::inputsFor(const Catalog &catalog,
						  const QList<Element *> &elements,
						  const NumberingFormat &fallback)
{
	QList<RenumberInput> inputs;

	for (Element *element : elements)
	{
		if (!element || !element->diagram()) {
			continue;
		}

		const DiagramContext information = element->elementInformations();
		Diagram *diagram = element->diagram();
		QETProject *project = diagram->project();

		RenumberInput input;
		input.uuid = element->uuid().toString();
		input.current = information.value(QStringLiteral("label")).toString();
		input.root = rootFor(catalog, element, false);
		input.position = element->scenePos();
		input.frozen = isFrozen(element);
		input.folio_index = project ? project->folioIndex(diagram) : 0;
		input.folio = QString::number(input.folio_index + 1);

		// The location, for the formats and for the uniqueness check: two
		// panels may each have their own -Q1.
		input.location = information.value(IecStructure::locationKey()).toString();
		if (input.location.isEmpty())
		{
			input.location = diagram->border_and_titleblock
					.titleblockInformation()
					.value(IecStructure::folioLocationKey())
					.toString();
		}

		// The rung, i.e. the line of the schematic the symbol sits on, comes
		// from the coordinate system of the folio - which is exactly what the
		// border already computes for the cross references.
		input.rung = QString::number(
			diagram->convertPosition(element->scenePos()).number());
		input.format = formatFor(catalog, element, fallback);

		inputs.append(input);
	}

	return inputs;
}

/**
	@brief ProjectRenumberer::applyPlan
	@param elements
	@param plan
	@return how many components changed
*/
int ProjectRenumberer::applyPlan(const QList<Element *> &elements, const RenumberPlan &plan)
{
	Diagram *diagram = nullptr;
	for (Element *element : elements)
	{
		if (element && element->diagram())
		{
			diagram = element->diagram();
			break;
		}
	}
	if (!diagram) {
		return 0;
	}

	auto *command = new RenumberCommand(elements, plan);
	const int count = command->changeCount();
	if (count == 0)
	{
		delete command;
		return 0;
	}
	diagram->undoStack().push(command);
	return count;
}

/**
	@brief ProjectRenumberer::elementWithLabel
	@param project
	@param label
	@param location
	@param except
	@return the component already carrying @a label in @a location
*/
Element *ProjectRenumberer::elementWithLabel(QETProject *project,
					     const QString &label,
					     const QString &location,
					     Element *except)
{
	const QString wanted = label.trimmed();
	if (!project || wanted.isEmpty()) {
		return nullptr;
	}

	const QList<Element *> element_list = components(project);
	for (Element *element : element_list)
	{
		if (!element || element == except) {
			continue;
		}
		const DiagramContext info = element->elementInformations();
		if (info.value(QStringLiteral("label")).toString().trimmed() != wanted) {
			continue;
		}
			//Two cabinets may each have their own -Q1. Only a collision in
			//the same location is a collision.
		if (info.value(IecStructure::locationKey()).toString().trimmed()
				!= location.trimmed()) {
			continue;
		}
		return element;
	}
	return nullptr;
}
