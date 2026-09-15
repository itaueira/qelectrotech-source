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
#include "movemountedpartcommand.h"

#include "../location/mountingmeasure.h"

#include <QObject>

#include <cmath>

/**
	@brief MoveMountedPartCommand::MoveMountedPartCommand
	@param scene the surface the part is drawn on
	@param item_uuid which part
	@param before_mm where it was, millimetre
	@param after_mm where it goes, millimetre
	@param parent parent undo command

	The caption is written here, while the part is still there to be named.
	Reading the name at undo time would name whatever is under that identity
	then, and the point of a caption is to say what the person is about to
	take back.
*/
MoveMountedPartCommand::MoveMountedPartCommand(MountingScene *scene,
					       const QString &item_uuid,
					       const QPointF &before_mm,
					       const QPointF &after_mm,
					       QUndoCommand *parent) :
	QUndoCommand(parent),
	m_scene(scene),
	m_uuid(item_uuid),
	m_before(before_mm),
	m_after(after_mm)
{
	const MountedItem item = scene ? scene->mountedItem(item_uuid)
				       : MountedItem();

	if (item.isNull())
	{
		setText(QObject::tr("Déplacer un composant sur la platine"));
	}
	else
	{
		setText(QObject::tr("Déplacer %1 sur la platine")
			.arg(item.designation()));
	}
}

MoveMountedPartCommand::~MoveMountedPartCommand()
{}

/**
	@brief MoveMountedPartCommand::undo
	Put the part back where it was.
*/
void MoveMountedPartCommand::undo()
{
	if (m_scene) {
		m_scene->applyItemPosition(m_uuid, m_before);
	}
}

/**
	@brief MoveMountedPartCommand::redo
	Put the part where it goes.

	Called once by the stack as soon as the command is pushed, at a moment
	when a dragged part is already there. Writing the same millimetre twice
	costs nothing and keeps one path: the position of a part is what this
	command says it is, whether it got there by a mouse or by a number typed
	into a box.
*/
void MoveMountedPartCommand::redo()
{
	if (m_scene) {
		m_scene->applyItemPosition(m_uuid, m_after);
	}
}

/**
	@brief MoveMountedPartCommand::itemUuid
	@return which part this moves
*/
QString MoveMountedPartCommand::itemUuid() const
{
	return m_uuid;
}

/**
	@brief MoveMountedPartCommand::before
	@return where the part was, millimetre
*/
QPointF MoveMountedPartCommand::before() const
{
	return m_before;
}

/**
	@brief MoveMountedPartCommand::after
	@return where the part goes, millimetre
*/
QPointF MoveMountedPartCommand::after() const
{
	return m_after;
}

/**
	@brief MoveMountedPartCommand::isNull
	@return true when this would leave a step that undoes nothing
*/
bool MoveMountedPartCommand::isNull() const
{
	if (!m_scene || m_uuid.isEmpty()) {
		return true;
	}

		//Asked before the comparison and not through it: a coordinate
		//that is not a number makes isSameLength answer false, which
		//would read here as "these are two different places" and let a
		//not-a-number onto the stack.
	if (!std::isfinite(m_before.x()) || !std::isfinite(m_before.y())
	    || !std::isfinite(m_after.x()) || !std::isfinite(m_after.y())) {
		return true;
	}

		//The slack of the mounting rules, borrowed rather than declared
		//again, so that a move and a dimension never disagree about
		//whether two numbers are the same.
	return MountingMeasure::isSameLength(m_before.x(), m_after.x())
	       && MountingMeasure::isSameLength(m_before.y(), m_after.y());
}
