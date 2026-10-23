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
#include "arrangeconductorscommand.h"

#include "../diagram.h"
#include "../qetgraphicsitem/conductortextitem.h"

#include <algorithm>

/**
	@brief ArrangeConductorsCommand::ArrangeConductorsCommand
	@param diagram : the folio of the conductors
	@param changes : the conductors and their new paths. What each one has
	now is kept here, for undo: a conductor about to be added by another
	command of the same parent has the automatic path, and gets it back.
	@param parent
*/
ArrangeConductorsCommand::ArrangeConductorsCommand(Diagram *diagram,
						   const QList<WireArrangement::Change> &changes,
						   QUndoCommand *parent) :
	QUndoCommand(QObject::tr("Arrange conductors", "undo caption"), parent),
	m_diagram(diagram)
{
	for (const WireArrangement::Change &change : changes)
	{
		if (!change.conductor) continue;
		Entry entry;
		entry.conductor = change.conductor;
		entry.profiles = change.conductor->profiles();
		entry.arranged = change.conductor->isAutoArranged();
		entry.text_pos = change.conductor->textItem()->pos();
		entry.points = change.points;
		entry.branch = change.conductor->arm() != nullptr;
		entry.arm_point = change.conductor->armPoint();
		entry.own_shape = change.conductor->ownShape();
		m_entries << entry;
	}
		//The branches after the arms (T86): a branch takes its place on its
		//arm, which has to be where the arrangement puts it first; and undo,
		//going backwards, gives a branch back its shape before its arm goes
		//back, which it then follows
	std::stable_partition(m_entries.begin(), m_entries.end(),
			      [](const Entry &entry) { return !entry.branch; });
}

/**
	@brief ArrangeConductorsCommand::undo
*/
void ArrangeConductorsCommand::undo()
{
	if (m_diagram) m_diagram->showMe();
	for (int i = m_entries.size() - 1 ; i >= 0 ; --i)
	{
		const Entry &entry = m_entries.at(i);
		if (!entry.conductor) continue;
		if (entry.branch)
		{
			entry.conductor->setBranchShape(entry.arm_point, entry.own_shape);
			entry.conductor->setAutoArranged(entry.arranged);
			entry.conductor->textItem()->setPos(entry.text_pos);
			continue;
		}
		entry.conductor->setProfiles(entry.profiles);
		entry.conductor->setAutoArranged(entry.arranged);
		entry.conductor->textItem()->setPos(entry.text_pos);
	}
	placeNames();
}

/**
	@brief ArrangeConductorsCommand::redo
*/
void ArrangeConductorsCommand::redo()
{
	if (m_diagram) m_diagram->showMe();
	for (const Entry &entry : std::as_const(m_entries))
	{
		if (!entry.conductor) continue;
		if (entry.branch)
		{
			if (!entry.points.isEmpty()) {
				entry.conductor->setBranchShape(entry.points.first(), {});
			}
			entry.conductor->setAutoArranged(true);
			continue;
		}
		entry.conductor->setArrangedPath(entry.points);
	}
	placeNames();
}

/**
	@brief ArrangeConductorsCommand::placeNames
	Place every name of the folio again, once every path is in place: the
	place of a name depends on the conductors around it (T71), and each
	one was placed as its own path changed, before the paths after it
*/
void ArrangeConductorsCommand::placeNames()
{
	if (!m_diagram) return;
	const QList<Conductor *> conductors = m_diagram->conductors();
	for (Conductor *conductor : conductors) {
		conductor->calculateTextItemPosition();
	}
}
