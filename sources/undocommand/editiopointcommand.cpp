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
#include "editiopointcommand.h"

#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "changeelementdatacommand.h"

#include <QObject>

/**
	@brief EditIoPointCommand::EditIoPointCommand
	@param project the project whose I/O list is replaced
	@param element the PLC master whose channel row is written, null when
	the point has not taken a channel yet
	@param list the list as the edit left it
	@param data the element data as the edit left it, ignored when element
	is null
	@param label how the point is called in the caption
	@param parent parent undo command
*/
EditIoPointCommand::EditIoPointCommand(QETProject *project,
				       Element *element,
				       const IoList &list,
				       const ElementData &data,
				       const QString &label,
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

	setText(label.isEmpty()
		? QObject::tr("modifier un point d'E/S")
		: QObject::tr("modifier le point d'E/S %1").arg(label));
}

/**
	@brief EditIoPointCommand::redo
*/
void EditIoPointCommand::redo()
{
	QUndoCommand::redo();
	apply(m_new_list);
	refresh();
}

void EditIoPointCommand::undo()
{
	apply(m_old_list);
	QUndoCommand::undo();
	refresh();
}

/**
	@brief EditIoPointCommand::apply
	@param list
*/
void EditIoPointCommand::apply(const IoList &list)
{
	if (!m_project) {
		return;
	}
	m_project->setIoList(list);
}

/**
	@brief EditIoPointCommand::refresh

	Element::setElementData hands the new row to the linked slave, which is
	what makes the description change on the folio, but it never asks for a
	repaint of the card itself - and the card draws its channel table at
	every paint. Without this the person edits a cell and the table under
	it keeps the old text until something else dirties the folio.
*/
void EditIoPointCommand::refresh()
{
	if (m_element && m_element.data()->scene()) {
		m_element.data()->update();
	}
}
