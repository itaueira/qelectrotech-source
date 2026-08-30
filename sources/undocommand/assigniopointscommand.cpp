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
#include "assigniopointscommand.h"

#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "changeelementdatacommand.h"

#include <QObject>

/**
	@brief AssignIoPointsCommand::AssignIoPointsCommand
	@param project the project whose I/O list is replaced
	@param element the PLC master whose channel table is written
	@param list the list as the assignment left it
	@param data the element data as the assignment left it
	@param count how many points moved, for the caption
	@param releasing true when the points are being taken back out
	@param parent parent undo command
*/
AssignIoPointsCommand::AssignIoPointsCommand(QETProject *project,
					     Element *element,
					     const IoList &list,
					     const ElementData &data,
					     int count,
					     bool releasing,
					     QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project),
	m_element(element),
	m_new_list(list)
{
	if (m_project) {
		m_old_list = m_project->ioList();
	}
	if (element) {
		new ChangeElementDataCommand(element, data, this);
	}

	setText(releasing
		? QObject::tr("libérer %n voie(s) d'E/S", "", count)
		: QObject::tr("affecter %n point(s) d'E/S", "", count));
}

/**
	@brief AssignIoPointsCommand::redo

	The card first, then the list: QUndoCommand::redo() is what runs the
	child, and nothing here reads the list while the card is being written,
	so the order is only a matter of undo being its mirror.
*/
void AssignIoPointsCommand::redo()
{
	QUndoCommand::redo();
	apply(m_new_list);
	refresh();
}

void AssignIoPointsCommand::undo()
{
	apply(m_old_list);
	QUndoCommand::undo();
	refresh();
}

/**
	@brief AssignIoPointsCommand::apply
	@param list
*/
void AssignIoPointsCommand::apply(const IoList &list)
{
	if (!m_project) {
		return;
	}
	m_project->setIoList(list);
}

/**
	@brief AssignIoPointsCommand::refresh

	Element::drawPlcTable paints the channel table from the element data at
	every paint, and setElementData changes that data without asking for a
	repaint. Without this, the function texts only appear the next time
	something else happens to dirty the folio.
*/
void AssignIoPointsCommand::refresh()
{
	if (m_element && m_element.data()->scene()) {
		m_element.data()->update();
	}
}
