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
#include "groupedupdatecommand.h"

#include "../dataBase/projectdatabase.h"
#include "../qetproject.h"

/**
	@brief GroupedUpdateCommand::GroupedUpdateCommand
	@param project : the project whose data base groups the notices of the
	children. A null project is accepted and means no grouping, so that a
	caller does not have to guard the construction.
	@param text : the caption of the command in the undo list
	@param parent
*/
GroupedUpdateCommand::GroupedUpdateCommand(QETProject *project,
					   const QString &text,
					   QUndoCommand *parent) :
	QUndoCommand(text, parent),
	m_project(project)
{}

/**
	@brief GroupedUpdateCommand::redo
	Run the children with one operation open around them.
*/
void GroupedUpdateCommand::redo()
{
	projectDataBase::Operation operation(m_project.data());
	QUndoCommand::redo();
}

/**
	@brief GroupedUpdateCommand::undo
	Take the children back with one operation open around them.
*/
void GroupedUpdateCommand::undo()
{
	projectDataBase::Operation operation(m_project.data());
	QUndoCommand::undo();
}
