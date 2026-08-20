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
#include "assigncatalogpartcommand.h"

#include "../ElementsCollection/elementslocation.h"
#include "../catalog/catalog.h"
#include "../catalog/catalogassignment.h"
#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetproject.h"

#include <QObject>

/**
	@brief AssignCatalogPartCommand::AssignCatalogPartCommand
	@param elements : the components receiving the part
	@param catalog : the catalog the part comes from
	@param part : the part to assign
	@param parent
*/
AssignCatalogPartCommand::AssignCatalogPartCommand(const QList<Element *> &elements,
						   const Catalog &catalog,
						   const CatalogPart &part,
						   QUndoCommand *parent) :
	QUndoCommand(parent)
{
	const QHash<QString, QString> values =
		CatalogAssignment::valuesForElement(catalog, part);

	for (Element *element : elements)
	{
		if (!element) {
			continue;
		}

		Assignment assignment;
		assignment.element = QPointer<Element>(element);
		assignment.old_information = element->elementInformations();

		DiagramContext information = assignment.old_information;
		const QStringList keys = values.keys();
		for (const QString &key : keys) {
			information.addValue(key, values.value(key));
		}
		assignment.new_information = information;

		// The pins of the part come per sub symbol; the symbol the component
		// was drawn with is what says which set is its own.
		const QList<Terminal *> terminals = element->terminals();
		const QStringList names =
			CatalogAssignment::terminalNames(part,
							 element->location().path(),
							 terminals.size());

		for (int index = 0 ; index < terminals.size() ; ++index)
		{
			assignment.old_terminal_names.append(terminals.at(index)->instanceName());
			assignment.new_terminal_names.append(index < names.size() ? names.at(index)
										  : QString());
		}

		m_assignments.append(assignment);
	}

	if (m_assignments.size() == 1)
	{
		setText(QObject::tr("Attribuer la pièce %1 à %2")
			.arg(part.code, m_assignments.first().element->name()));
	}
	else
	{
		setText(QObject::tr("Attribuer la pièce %1 à %n composant(s)", "", m_assignments.size())
			.arg(part.code));
	}
}

/**
	@brief AssignCatalogPartCommand::componentCount
	@return how many components the command touches
*/
int AssignCatalogPartCommand::componentCount() const
{
	return m_assignments.size();
}

/**
	@brief AssignCatalogPartCommand::undo
*/
void AssignCatalogPartCommand::undo()
{
	apply(false);
}

/**
	@brief AssignCatalogPartCommand::redo
*/
void AssignCatalogPartCommand::redo()
{
	apply(true);
}

/**
	@brief AssignCatalogPartCommand::apply
	@param forward : true to assign, false to put back what was there
*/
void AssignCatalogPartCommand::apply(bool forward)
{
	for (const Assignment &assignment : m_assignments)
	{
		Element *element = assignment.element.data();
		if (!element) {
			continue;
		}

		element->setElementInformations(forward ? assignment.new_information
							: assignment.old_information);

		const QStringList &names = forward ? assignment.new_terminal_names
						   : assignment.old_terminal_names;
		const QList<Terminal *> terminals = element->terminals();
		for (int index = 0 ; index < terminals.size() && index < names.size() ; ++index)
		{
			// An empty name means the symbol keeps the label it came with,
			// which is also how undo puts a component back to a state that
			// never had a part assigned.
			terminals.at(index)->setInstanceName(names.at(index));
		}
		element->update();
	}
	updateProjectDataBase();
}

/**
	@brief AssignCatalogPartCommand::updateProjectDataBase
	Tell the project database that the information changed, so that the lists
	and the summary see the new part without the project being reopened.
*/
void AssignCatalogPartCommand::updateProjectDataBase()
{
	QList<Element *> elements;
	for (const Assignment &assignment : m_assignments)
	{
		if (assignment.element) {
			elements.append(assignment.element.data());
		}
	}
	if (elements.isEmpty()) {
		return;
	}

	Element *first = elements.first();
	if (first->diagram() && first->diagram()->project()
	    && first->diagram()->project()->dataBase())
	{
		first->diagram()->project()->dataBase()->elementInfoChanged(elements);
	}
}
