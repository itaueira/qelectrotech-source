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
#include "mountpartcommand.h"

#include <QObject>

/**
	@brief MountPartCommand::MountPartCommand
	@param scene the surface the part is drawn on
	@param mounted_item the part, in millimetre
	@param way which way round the step is read
	@param index where in the list of the face it sits, -1 for the end
	@param parent parent undo command

	The caption is written here, while the part is in hand. Reading its name
	at undo time would name whatever holds that identity then - and after a
	removal, nothing does.
*/
MountPartCommand::MountPartCommand(MountingScene *scene,
				   const MountedItem &mounted_item,
				   Way way,
				   int index,
				   QUndoCommand *parent) :
	QUndoCommand(parent),
	m_scene(scene),
	m_item(mounted_item),
	m_way(way),
	m_index(index)
{
	if (m_item.isNull())
	{
		setText(way == Mount
			? QObject::tr("Poser un composant sur la platine")
			: QObject::tr("Retirer un composant de la platine"));
	}
	else
	{
		setText(way == Mount
			? QObject::tr("Poser %1 sur la platine")
			  .arg(m_item.designation())
			: QObject::tr("Retirer %1 de la platine")
			  .arg(m_item.designation()));
	}
}

MountPartCommand::~MountPartCommand()
{}

/**
	@brief MountPartCommand::undo
	The step read backwards.
*/
void MountPartCommand::undo()
{
	if (m_way == Mount) {
		unmount();
	}
	else {
		mount();
	}
}

/**
	@brief MountPartCommand::redo
	The step read forwards.

	Called once by the stack the moment the command is pushed, at which
	point the part may already be where it goes - the window puts it on the
	drawing by pushing this. Mounting what is already mounted is refused by
	the scene, by identity, so the second attempt costs nothing and there is
	still exactly one path: what is on the plate is what this command says.
*/
void MountPartCommand::redo()
{
	if (m_way == Mount) {
		mount();
	}
	else {
		unmount();
	}
}

/**
	@brief MountPartCommand::itemUuid
	@return which part this mounts or unmounts
*/
QString MountPartCommand::itemUuid() const
{
	return m_item.uuid;
}

/**
	@brief MountPartCommand::mountedItem
	@return the part, as it was when the step was made
*/
MountedItem MountPartCommand::mountedItem() const
{
	return m_item;
}

/**
	@brief MountPartCommand::way
	@return which way round the step is read
*/
MountPartCommand::Way MountPartCommand::way() const
{
	return m_way;
}

/**
	@brief MountPartCommand::isNull
	@return true when this would leave a step that does nothing
*/
bool MountPartCommand::isNull() const
{
	return !m_scene || m_item.uuid.isEmpty();
}

/**
	@brief MountPartCommand::mount
	Put the part back on the drawing, where it was in the list.
*/
void MountPartCommand::mount()
{
	if (m_scene) {
		m_scene->applyItemMounting(m_item, m_index);
	}
}

/**
	@brief MountPartCommand::unmount
	Take the part off the drawing.

	Where it sat is read again here rather than trusted from construction
	time: between the push and an undo, other steps may have taken parts off
	the face and moved this one up the list. Asking now is what makes the
	undo of a removal put the part back where it really was.
*/
void MountPartCommand::unmount()
{
	if (!m_scene) {
		return;
	}

	const int at = m_scene->indexOfItem(m_item.uuid);
	if (at >= 0) {
		m_index = at;
	}

	m_scene->applyItemUnmounting(m_item.uuid);
}
