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
#include "changeelementinformationcommand.h"
#include "../qetproject.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"

#include <QObject>

/**
	@brief ChangeElementInformationCommand::ChangeElementInformationCommand
	Default constructor
	@param elmt : element to change information
	@param old_info : old info of element
	@param new_info : new info of element
	@param parent
*/
ChangeElementInformationCommand::ChangeElementInformationCommand(
		Element *elmt,
		const DiagramContext &old_info,
		const DiagramContext &new_info,
		QUndoCommand *parent) :
	QUndoCommand (parent)
{
	m_map.insert(QPointer<Element>(elmt), qMakePair(old_info, new_info));
	setText(QObject::tr("Edit information of the element : %1")
			.arg(elmt -> name()));
}

ChangeElementInformationCommand::ChangeElementInformationCommand(QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext> > map,
																 QUndoCommand *parent) :
	QUndoCommand(parent),
	m_map(map)
{
	setText(QObject::tr("Edit information for multiple items"));
}

bool ChangeElementInformationCommand::mergeWith(const QUndoCommand *other)
{
	if (id() != other->id())
		return false;

	ChangeElementInformationCommand const *other_undo = static_cast<const ChangeElementInformationCommand*>(other);

		//In case of other undo_undo have the same elements as keys
	if (m_map.size() == other_undo->m_map.size())
	{
		for (auto key : other_undo->m_map.keys()) {
			if (!m_map.contains(key)) {
				return false;
			}
		}

			//Other_undo will be merged with this undo :
			//Replace the new_info values of this m_map
			//by the new_info values of other_undo's m_map
		for (auto key : other_undo->m_map.keys())
		{
			m_map.insert(key,
						 qMakePair(
							 m_map.value(key).first,
							 other_undo->m_map.value(key).second));
		}
			//And the wires the other one moved (T70): a wire both moved
			//goes back to what it had before this one, and on to what the
			//other one gave it. Both were redone before the merge, as the
			//stack redoes a command before it tries to merge it.
		for (const WireSections::Change &change : other_undo->m_wire_changes)
		{
			bool known = false;
			for (WireSections::Change &mine : m_wire_changes)
			{
				if (mine.conductor == change.conductor)
				{
					mine.after = change.after;
					known = true;
					break;
				}
			}
			if (!known) {
				m_wire_changes << change;
			}
		}
		return true;
	}
	else {
		return false;
	}
}

/**
	@brief ChangeElementInformationCommand::undo
*/
void ChangeElementInformationCommand::undo()
{
	updateWireSections(false);
	for (auto element : m_map.keys()) {
		element->setElementInformations(m_map.value(element).first);
	}
	updateProjectDB();
}

/**
	@brief ChangeElementInformationCommand::redo
*/
void ChangeElementInformationCommand::redo()
{
	for (auto element : m_map.keys()) {
		element->setElementInformations(m_map.value(element).second);
	}
	updateProjectDB();
	updateWireSections(true);
}

/**
	@brief ChangeElementInformationCommand::updateWireSections
	Give the points of the components whose wire cross-section changed the
	section they now ask for (T70), or take it back.
	@param forward : true from redo(), false from undo()
*/
void ChangeElementInformationCommand::updateWireSections(bool forward)
{
	if (forward && !m_wire_changes_known)
	{
		m_wire_changes_known = true;
			//Only the components whose section changed: this command also
			//carries edits of hundreds of components at once - a search
			//and replace, the tags of the folio reports - and those move
			//no wire
		QList<Element *> changed;
		for (auto it = m_map.cbegin() ; it != m_map.cend() ; ++it)
		{
			Element *element = it.key().data();
			if (element
			    && it.value().first.value(QETInformation::ELMT_WIRE_SECTION).toString()
			       != it.value().second.value(QETInformation::ELMT_WIRE_SECTION).toString())
			{
				changed << element;
			}
		}
		if (!changed.isEmpty()) {
			m_wire_changes = WireSections::changesForElements(changed);
		}
	}
	WireSections::apply(m_wire_changes, forward);
}

void ChangeElementInformationCommand::updateProjectDB()
{
	auto elmt = m_map.keys().first().data();
	if(elmt && elmt->diagram())
	{
			//need to have a list of element instead of QPointer<Element>
			//for the function elementInfoChange of the database
		QList<Element *> list_;
		for (auto p_elmt : m_map.keys())
			list_ << p_elmt.data();

		elmt->diagram()->project()->dataBase()->elementInfoChanged(list_);
	}
}

ChangeStoredElementInformationCommand::ChangeStoredElementInformationCommand(
		QETProject *project, const QUuid &element, const DiagramContext &old_info,
		const DiagramContext &new_info, QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project),
	m_element(element),
	m_old_info(old_info),
	m_new_info(new_info)
{
	setText(QObject::tr("Change the information of the element"));
}

void ChangeStoredElementInformationCommand::undo()
{
	if (m_project)
		m_project->dataBase()->setPlacedElementInformation(m_element, m_old_info);
}

void ChangeStoredElementInformationCommand::redo()
{
	if (m_project)
		m_project->dataBase()->setPlacedElementInformation(m_element, m_new_info);
}
