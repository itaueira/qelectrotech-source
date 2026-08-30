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
#include "editlocationtreecommand.h"

#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "assignlocationcommand.h"

#include <QGraphicsItem>
#include <QObject>
#include <QVariant>

/**
	@brief EditLocationTreeCommand::EditLocationTreeCommand
	@param project the project owning the tree
	@param tree the tree as the edit left it
	@param changed old path to new path, as update, move or lostPaths
	report it, empty when nothing that was written on a component moved
	@param label how the caption calls what happened
	@param parent parent undo command
*/
EditLocationTreeCommand::EditLocationTreeCommand(QETProject *project,
						 const LocationTree &tree,
						 const QMap<QString, QString> &changed,
						 const QString &label,
						 QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project)
{
	if (m_project) {
		m_old_tree = m_project.data()->locationTree();
	}
	m_new_tree = tree;

	const QList<LocationAssignment> to_follow = followers(project, changed);
	if (!to_follow.isEmpty())
	{
		AssignLocationCommand *follow =
			new AssignLocationCommand(to_follow, QString(), this);
		m_component_count = follow->componentCount();
	}

	setText(label.isEmpty()
		? QObject::tr("Modifier les localisations du projet")
		: label);
}

/**
	@brief EditLocationTreeCommand::followers
	@param project the project to walk
	@param changed the paths that moved
	@return every component standing on one of those paths, with the path
	it should carry afterwards

	Walking the whole project is the honest way to do this: nothing indexes
	components by location, and a stale index would be worse than a walk
	that happens once per edit of the tree. An empty map short-circuits it,
	which is the common case - creating an enclosure moves nobody.
*/
QList<LocationAssignment> EditLocationTreeCommand::followers(
		QETProject *project,
		const QMap<QString, QString> &changed)
{
	QList<LocationAssignment> found;
	if (!project || changed.isEmpty()) {
		return found;
	}

	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
		const QList<QGraphicsItem *> items = diagram->items();
		for (QGraphicsItem *item : items)
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
			if (!element) {
				continue;
			}

			const QString path =
				element->elementInformations()
					.value(QETInformation::ELMT_LOCATION_PATH)
					.toString();
			if (path.isEmpty() || !changed.contains(path)) {
				continue;
			}

			LocationAssignment assignment;
			assignment.element = element;
			assignment.path =
				LocationTree::rewrittenPath(path, changed);
			found.append(assignment);
		}
	}

	return found;
}

/**
	@brief EditLocationTreeCommand::componentCount
	@return how many components followed the tree
*/
int EditLocationTreeCommand::componentCount() const
{
	return m_component_count;
}

/**
	@brief EditLocationTreeCommand::undo
	The components go back first, while the tree still describes the world
	they are being put back into.
*/
void EditLocationTreeCommand::undo()
{
	apply(m_old_tree);
	QUndoCommand::undo();
}

/**
	@brief EditLocationTreeCommand::redo
*/
void EditLocationTreeCommand::redo()
{
	QUndoCommand::redo();
	apply(m_new_tree);
}

/**
	@brief EditLocationTreeCommand::apply
	@param tree the state to put on the project
*/
void EditLocationTreeCommand::apply(const LocationTree &tree)
{
	if (m_project) {
		m_project.data()->setLocationTree(tree);
	}
}
