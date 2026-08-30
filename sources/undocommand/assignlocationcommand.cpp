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
#include "assignlocationcommand.h"

#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"
#include "../qetproject.h"

#include <QObject>
#include <QVariant>

/**
	@brief AssignLocationCommand::AssignLocationCommand
	@param elements the components receiving the location
	@param path the path they all take, empty to unassign them
	@param parent parent undo command

	The batch of CU-32.1: what the person selected on the folio goes into
	one enclosure in one gesture, and comes back out in one undo.
*/
AssignLocationCommand::AssignLocationCommand(const QList<Element *> &elements,
					     const QString &path,
					     QUndoCommand *parent) :
	QUndoCommand(parent)
{
	for (Element *element : elements) {
		take(element, path);
	}

	if (path.isEmpty())
	{
		setText(QObject::tr("Retirer %n composant(s) de leur localisation",
				    "", m_assignments.size()));
	}
	else
	{
		setText(QObject::tr("Affecter %n composant(s) à la localisation %1",
				    "", m_assignments.size())
			.arg(path));
	}
}

/**
	@brief AssignLocationCommand::AssignLocationCommand
	@param assignments one path per component
	@param label how the caption calls what happened
	@param parent parent undo command

	The shape a rename or a move needs: the tree changed under the
	components, and each one follows its own branch to a different place.
*/
AssignLocationCommand::AssignLocationCommand(const QList<LocationAssignment> &assignments,
					     const QString &label,
					     QUndoCommand *parent) :
	QUndoCommand(parent)
{
	for (const LocationAssignment &assignment : assignments) {
		take(assignment.element, assignment.path);
	}

	setText(label.isEmpty()
		? QObject::tr("Modifier la localisation de %n composant(s)",
			      "", m_assignments.size())
		: label);
}

/**
	@brief AssignLocationCommand::take
	@param element the component to write, ignored when null
	@param path the path it should carry afterwards

	A component already carrying the wanted path is not taken: it has
	nothing to undo, and keeping it would make componentCount say more
	happened than did.
*/
void AssignLocationCommand::take(Element *element, const QString &path)
{
	if (!element) {
		return;
	}

	const QString old_path =
		element->elementInformations()
			.value(QETInformation::ELMT_LOCATION_PATH).toString();
	if (old_path == path) {
		return;
	}

	Assignment assignment;
	assignment.element = QPointer<Element>(element);
	assignment.old_path = old_path;
	assignment.new_path = path;
	m_assignments.append(assignment);
}

/**
	@brief AssignLocationCommand::componentCount
	@return how many components the command really changes
*/
int AssignLocationCommand::componentCount() const
{
	return m_assignments.size();
}

/**
	@brief AssignLocationCommand::undo
*/
void AssignLocationCommand::undo()
{
	apply(false);
}

/**
	@brief AssignLocationCommand::redo
*/
void AssignLocationCommand::redo()
{
	apply(true);
}

/**
	@brief AssignLocationCommand::apply
	@param forward true to write the new paths, false to put back the old
*/
void AssignLocationCommand::apply(bool forward)
{
	for (const Assignment &assignment : m_assignments)
	{
		if (!assignment.element) {
			continue;
		}

		Element *element = assignment.element.data();
		const QString path = forward ? assignment.new_path
					     : assignment.old_path;

		DiagramContext information = element->elementInformations();
		if (path.isEmpty()) {
			information.remove(QETInformation::ELMT_LOCATION_PATH);
		} else {
			information.addValue(QETInformation::ELMT_LOCATION_PATH,
					     path);
		}

		element->setElementInformations(information);
		element->update();
	}

	updateProjectDataBase();
}

/**
	@brief AssignLocationCommand::updateProjectDataBase
	Tell the database once, with everything that moved.
*/
void AssignLocationCommand::updateProjectDataBase()
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
