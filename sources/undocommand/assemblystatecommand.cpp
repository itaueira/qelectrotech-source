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
#include "assemblystatecommand.h"

#include "../autoNum/projectrenumberer.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QObject>

/**
	@brief AssemblyStateCommand::AssemblyStateCommand
	@param project : the project whose state changes
	@param new_state : the state to put it in, photograph included
	@param parent
*/
AssemblyStateCommand::AssemblyStateCommand(QETProject *project,
					   const AssemblyState &new_state,
					   QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project),
	m_new_state(new_state)
{
	if (m_project) {
		m_old_state = m_project->assemblyState();
	}

	if (m_new_state.isFrozen())
	{
		setText(QObject::tr("Marquer le projet comme %1 : %n élément(s) figé(s)",
				    "", m_new_state.frozenCount())
			.arg(AssemblyState::translatedStage(m_new_state.stage)));
	}
	else
	{
			//Taking the marking back is not "freezing zero elements": it is
			//its own thing, and the undo menu has to read like it.
		setText(QObject::tr("Remettre le projet en étude"));
	}
}

/**
	@brief AssemblyStateCommand::frozenCount
	@return how many items this command freezes
*/
int AssemblyStateCommand::frozenCount() const
{
	return m_new_state.frozenCount();
}

/**
	@brief AssemblyStateCommand::changesAnything
	@return whether pushing this would change the project
*/
bool AssemblyStateCommand::changesAnything() const
{
	return m_project && m_old_state != m_new_state;
}

/**
	@brief AssemblyStateCommand::undo
*/
void AssemblyStateCommand::undo()
{
	apply(m_old_state);
}

/**
	@brief AssemblyStateCommand::redo
*/
void AssemblyStateCommand::redo()
{
	apply(m_new_state);
}

/**
	@brief AssemblyStateCommand::apply
	@param state
*/
void AssemblyStateCommand::apply(const AssemblyState &state)
{
	if (!m_project) {
		return;
	}
	m_project->setAssemblyState(state);
}

/**
	@brief AssemblyStateCommand::photograph
	@param project
	@param stage
	@return what the project looks like right now, under @a stage
*/
AssemblyState AssemblyStateCommand::photograph(QETProject *project,
					       AssemblyStage stage)
{
	AssemblyState state;
	state.stage = stage;

	if (!project || stage == AssemblyStage::InProject) {
			//Nothing to leave alone once the project is back on the drawing
			//board, so no photograph is taken and the file keeps no node.
		return state;
	}

		//The same list the renumbering walks, so that what the photograph
		//holds and what the renumbering would touch cannot drift apart. It
		//is components only: terminals and cross references are numbered by
		//another path, and freezing them belongs with that path.
	const QList<Element *> components = ProjectRenumberer::components(project);
	for (Element *element : components)
	{
		if (!element) {
			continue;
		}
		const QString uuid = element->uuid().toString();
		if (uuid.isEmpty()) {
			continue;
		}
		state.components.insert(uuid,
					element->elementInformations()
					.value(QStringLiteral("label")).toString());
	}

	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
		if (!diagram) {
			continue;
		}
		const QList<Conductor *> conductors = diagram->conductors();
		for (Conductor *conductor : conductors)
		{
			if (!conductor) {
				continue;
			}
			const QString uuid = conductor->uuid().toString();
			if (uuid.isEmpty()) {
				continue;
			}
			state.conductors.insert(uuid, conductor->properties().text);
		}
	}

	return state;
}
