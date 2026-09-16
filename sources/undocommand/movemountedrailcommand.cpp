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
#include "movemountedrailcommand.h"

#include "../location/mountingmeasure.h"

#include <QObject>

#include <cmath>
#include <limits>
#include <utility>

/**
	@brief MoveMountedRailCommand::MoveMountedRailCommand
	@param scene the surface the rail is drawn on
	@param rail_uuid which rail
	@param before_mm where it was, millimetre
	@param after_mm where it goes, millimetre
	@param carried_uuids what is clipped onto it, worked out before it moved
	@param parent parent undo command

	Where each carried part stands is read here, at the moment the step is
	made, and never again. At this moment the rail may already have been
	dragged - the framework moves it as the mouse moves - but the parts have
	not been touched by anything, so what is read is where they were when
	the gesture began. That is the position undo has to give back.

	The caption is written here too, while the rail is still there to be
	named, for the reason MoveMountedPartCommand states: reading the name at
	undo time names whatever carries that identity then.
*/
MoveMountedRailCommand::MoveMountedRailCommand(MountingScene *scene,
					       const QString &rail_uuid,
					       const QPointF &before_mm,
					       const QPointF &after_mm,
					       const QStringList &carried_uuids,
					       QUndoCommand *parent) :
	QUndoCommand(parent),
	m_scene(scene),
	m_uuid(rail_uuid),
	m_before(before_mm),
	m_after(after_mm)
{
	QStringList taken;

	for (const QString &item_uuid : carried_uuids)
	{
			//A rail does not carry itself, and a part named twice
			//is carried once: writing a position twice would be
			//harmless, but reading two before-positions for one
			//part and keeping the second is how an undo lands
			//somewhere nobody was.
		if (!scene || item_uuid.isEmpty() || item_uuid == rail_uuid
		    || taken.contains(item_uuid)) {
			continue;
		}

		const MountedItem part = scene->mountedItem(item_uuid);
		if (part.isNull()) {
			continue;
		}

		taken << item_uuid;
		m_carried << qMakePair(item_uuid, part.position);
	}

	const MountedItem rail = scene ? scene->mountedItem(rail_uuid)
				       : MountedItem();
	const QString designation = rail.isNull() ? QString()
						  : rail.designation();
	const int count = m_carried.count();

	if (designation.isEmpty())
	{
		setText(QObject::tr("Déplacer un rail et ce qu'il porte"));
	}
	else if (count == 1)
	{
		setText(QObject::tr("Déplacer %1 et le composant qu'il porte")
			.arg(designation));
	}
	else
	{
		setText(QObject::tr("Déplacer %1 et les %2 composants qu'il "
				    "porte")
			.arg(designation).arg(count));
	}
}

MoveMountedRailCommand::~MoveMountedRailCommand()
{}

/**
	@brief MoveMountedRailCommand::undo
	Put the rail back where it was, and everything it carried with it.

	The stored positions are written back as they were, and not the
	movement subtracted from where the parts now are - see the header.
*/
void MoveMountedRailCommand::undo()
{
	if (!m_scene) {
		return;
	}

	m_scene->applyItemPosition(m_uuid, m_before);

	for (const QPair<QString, QPointF> &entry : std::as_const(m_carried)) {
		m_scene->applyItemPosition(entry.first, entry.second);
	}
}

/**
	@brief MoveMountedRailCommand::redo
	Move the rail, and everything it carries with it.

	Called once by the stack as soon as the command is pushed, at a moment
	when a dragged rail is already where it was dropped and the parts are
	still where they were. Writing the same millimetre twice for the rail
	costs nothing and keeps one path: the position of a rail is what this
	command says it is, whether it got there by a mouse or by a number typed
	into a box.
*/
void MoveMountedRailCommand::redo()
{
	if (!m_scene) {
		return;
	}

	const QPointF movement = delta();

	m_scene->applyItemPosition(m_uuid, m_after);

	for (const QPair<QString, QPointF> &entry : std::as_const(m_carried)) {
		m_scene->applyItemPosition(entry.first, entry.second + movement);
	}
}

/**
	@brief MoveMountedRailCommand::itemUuid
	@return which rail this moves
*/
QString MoveMountedRailCommand::itemUuid() const
{
	return m_uuid;
}

/**
	@brief MoveMountedRailCommand::before
	@return where the rail was, millimetre
*/
QPointF MoveMountedRailCommand::before() const
{
	return m_before;
}

/**
	@brief MoveMountedRailCommand::after
	@return where the rail goes, millimetre
*/
QPointF MoveMountedRailCommand::after() const
{
	return m_after;
}

/**
	@brief MoveMountedRailCommand::delta
	@return how far the rail travels, millimetre
*/
QPointF MoveMountedRailCommand::delta() const
{
	return m_after - m_before;
}

/**
	@brief MoveMountedRailCommand::carried
	@return what the rail takes with it, in the order handed over
*/
QStringList MoveMountedRailCommand::carried() const
{
	QStringList uuids;
	uuids.reserve(m_carried.size());

	for (const QPair<QString, QPointF> &entry : m_carried) {
		uuids << entry.first;
	}

	return uuids;
}

/**
	@brief MoveMountedRailCommand::carriedCount
	@return how many parts the rail takes with it
*/
int MoveMountedRailCommand::carriedCount() const
{
	return m_carried.count();
}

/**
	@brief MoveMountedRailCommand::carriedBefore
	@param item_uuid one of the parts carried
	@return where that part was when the gesture began, millimetre
*/
QPointF MoveMountedRailCommand::carriedBefore(const QString &item_uuid) const
{
	for (const QPair<QString, QPointF> &entry : m_carried)
	{
		if (entry.first == item_uuid) {
			return entry.second;
		}
	}

	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();

	return QPointF(not_a_number, not_a_number);
}

/**
	@brief MoveMountedRailCommand::isNull
	@return true when this would leave a step that undoes nothing
*/
bool MoveMountedRailCommand::isNull() const
{
	if (!m_scene || m_uuid.isEmpty()) {
		return true;
	}

		//Asked before the comparison and not through it, exactly as the
		//move of a single part asks it: a coordinate that is not a
		//number makes isSameLength answer false, which would read here
		//as "these are two different places" and let a not-a-number
		//onto the stack - and this one would take twelve breakers with
		//it.
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
