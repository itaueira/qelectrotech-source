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
#include "importiopointscommand.h"

#include "../qetproject.h"

#include <QObject>

/**
	@brief ImportIoPointsCommand::ImportIoPointsCommand
	@param project the project whose list is replaced
	@param list the list as the merge left it
	@param touched how many points the import added or changed, for the caption
	@param parent parent undo command
*/
ImportIoPointsCommand::ImportIoPointsCommand(QETProject *project,
					     const IoList &list,
					     int touched,
					     QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project),
	m_new_list(list)
{
	if (m_project) {
		m_old_list = m_project->ioList();
	}

	setText(QObject::tr("importer %n point(s) d'E/S", "", touched));
}

void ImportIoPointsCommand::redo()
{
	apply(m_new_list);
}

void ImportIoPointsCommand::undo()
{
	apply(m_old_list);
}

/**
	@brief ImportIoPointsCommand::apply
	@param list

	QPointer and not a raw pointer: the undo stack outlives nothing here, but
	a command that survives its project would corrupt memory rather than fail,
	and the check costs one branch.
*/
void ImportIoPointsCommand::apply(const IoList &list)
{
	if (!m_project) {
		return;
	}
	m_project->setIoList(list);
}
