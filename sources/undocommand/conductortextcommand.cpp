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
#include "conductortextcommand.h"

#include <QCoreApplication>

#include "../qetgraphicsitem/conductor.h"

ConductorTextCommand::ConductorTextCommand(const QList<Conductor *> &conductors,
					   bool visible,
					   QUndoCommand *parent) :
	QUndoCommand(parent),
	m_visible(visible)
{
	for (Conductor *conductor : conductors)
	{
		if (!conductor) {
			continue;
		}
		if (conductor->properties().m_show_text == visible) {
				//Already the way it is being asked to be. Left out so that
				//the count the status bar reports is the number of stretches
				//that actually changed.
			continue;
		}
		Change change;
		change.conductor = conductor;
		change.old_visible = conductor->properties().m_show_text;
		m_changes << change;
	}

	setText(visible
		? QCoreApplication::translate("ConductorTextCommand",
			"afficher le numéro de %n conducteur(s)", "", m_changes.size())
		: QCoreApplication::translate("ConductorTextCommand",
			"masquer le numéro de %n conducteur(s)", "", m_changes.size()));
}

int ConductorTextCommand::conductorCount() const
{
	return m_changes.size();
}

bool ConductorTextCommand::isEmpty() const
{
	return m_changes.isEmpty();
}

void ConductorTextCommand::apply(bool forward)
{
	for (const Change &change : m_changes)
	{
		if (!change.conductor) {
			continue;
		}
		ConductorProperties properties = change.conductor->properties();
		properties.m_show_text = forward ? m_visible : change.old_visible;
		change.conductor->setProperties(properties);
	}
}

void ConductorTextCommand::redo()
{
	apply(true);
}

void ConductorTextCommand::undo()
{
	apply(false);
}
