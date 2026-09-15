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
#include "stretchmountedprofilecommand.h"

#include "../location/bommeasure.h"
#include "../location/mountingmeasure.h"

#include <QObject>

#include <cmath>

namespace
{
	/// @return true when every number of the rectangle is one
	bool isARectangle(const QRectF &rectangle)
	{
		return std::isfinite(rectangle.x())
		       && std::isfinite(rectangle.y())
		       && std::isfinite(rectangle.width())
		       && std::isfinite(rectangle.height());
	}
}

/**
	@brief StretchMountedProfileCommand::StretchMountedProfileCommand
	@param scene the surface the piece is drawn on
	@param item_uuid which piece
	@param before_mm the room it took, millimetre
	@param after_mm the room it takes, millimetre
	@param parent parent undo command

	The caption says how long the piece ends up, because that is the number
	the person was aiming at - "Couper Rail 35 × 7,5 mm à 480 mm" is a step
	somebody can find again in a list of twenty.
*/
StretchMountedProfileCommand::StretchMountedProfileCommand(
		MountingScene *scene,
		const QString &item_uuid,
		const QRectF &before_mm,
		const QRectF &after_mm,
		QUndoCommand *parent) :
	QUndoCommand(parent),
	m_scene(scene),
	m_uuid(item_uuid),
	m_before(before_mm.normalized()),
	m_after(after_mm.normalized())
{
	const MountedItem item = scene ? scene->mountedItem(item_uuid)
				       : MountedItem();

	if (item.isNull())
	{
		setText(QObject::tr("Couper un profilé de la platine"));
	}
	else
	{
		const qreal cut = MountingProfile::lengthOf(m_after.size(),
							    item.run);
			//Written by the hand that writes the lengths of the
			//material list, decimal separator included: a caption
			//reading 480,5 beside a list reading 480.5 is two
			//programs in one window.
		setText(QObject::tr("Couper %1 à %2 mm")
			.arg(item.designation(),
			     BomMeasure::formatQuantity(
				     cut, BomMeasure::defaultLengthUnit())));
	}
}

StretchMountedProfileCommand::~StretchMountedProfileCommand()
{}

/**
	@brief StretchMountedProfileCommand::undo
	Put the piece back as it was cut before.
*/
void StretchMountedProfileCommand::undo()
{
	if (m_scene) {
		m_scene->applyItemGeometry(m_uuid, m_before);
	}
}

/**
	@brief StretchMountedProfileCommand::redo
	Cut the piece.

	Called once by the stack as soon as this is pushed, at a moment when the
	piece the handles cut is already that length. Writing the same rectangle
	twice costs nothing and keeps one path: the room a piece takes is what
	this command says it is, whether it got there by a handle or by a number
	typed into a box.
*/
void StretchMountedProfileCommand::redo()
{
	if (m_scene) {
		m_scene->applyItemGeometry(m_uuid, m_after);
	}
}

/**
	@brief StretchMountedProfileCommand::itemUuid
	@return which piece this cuts
*/
QString StretchMountedProfileCommand::itemUuid() const
{
	return m_uuid;
}

/**
	@brief StretchMountedProfileCommand::before
	@return the room the piece took, millimetre
*/
QRectF StretchMountedProfileCommand::before() const
{
	return m_before;
}

/**
	@brief StretchMountedProfileCommand::after
	@return the room the piece takes, millimetre
*/
QRectF StretchMountedProfileCommand::after() const
{
	return m_after;
}

/**
	@brief StretchMountedProfileCommand::isNull
	@return true when this would leave a step that undoes nothing
*/
bool StretchMountedProfileCommand::isNull() const
{
	if (!m_scene || m_uuid.isEmpty()) {
		return true;
	}

		//Asked before the comparison and not through it, the way the
		//move command asks it: a number that is not one makes
		//isSameLength answer false, which would read here as "these are
		//two different rectangles" and let it onto the stack.
	if (!isARectangle(m_before) || !isARectangle(m_after)) {
		return true;
	}

	return MountingMeasure::isSameLength(m_before.x(), m_after.x())
	       && MountingMeasure::isSameLength(m_before.y(), m_after.y())
	       && MountingMeasure::isSameLength(m_before.width(),
						m_after.width())
	       && MountingMeasure::isSameLength(m_before.height(),
						m_after.height());
}
