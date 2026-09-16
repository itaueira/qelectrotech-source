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
#include "alignmountedpartscommand.h"

#include "../location/mountingmeasure.h"

#include <QObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
	/// @return a position no part can be at, for a question about a part
	/// this step does not move
	QPointF nowhere()
	{
		const qreal not_a_number =
				std::numeric_limits<qreal>::quiet_NaN();

		return QPointF(not_a_number, not_a_number);
	}
}

/**
	@brief AlignMountedPartsCommand::AlignMountedPartsCommand
	@param scene the surface the parts are drawn on
	@param targets where each part goes, millimetre, keyed by identity
	@param caption what the person reads in the undo list
	@param parent parent undo command

	Where each part stands is read here, at the moment the step is made.
	Nothing has moved yet when a gesture is applied from a menu, so what is
	read is where the parts were before it.

	The identities are taken in sorted order, and that is not decoration: a
	hash hands its keys back in whatever order it feels like, and a step
	whose list of parts changes from one run to the next is a step nobody
	can compare against another. The positions are independent of one
	another, so the order changes nothing about the result - only about
	whether the result can be read twice and match.
*/
AlignMountedPartsCommand::AlignMountedPartsCommand(
		MountingScene *scene,
		const QHash<QString, QPointF> &targets,
		const QString &caption,
		QUndoCommand *parent) :
	QUndoCommand(parent),
	m_scene(scene)
{
	QStringList uuids = targets.keys();
	std::sort(uuids.begin(), uuids.end());

	for (const QString &item_uuid : std::as_const(uuids))
	{
		if (!scene || item_uuid.isEmpty()) {
			continue;
		}

		const QPointF landing = targets.value(item_uuid);
		if (!std::isfinite(landing.x()) || !std::isfinite(landing.y())) {
			continue;
		}

		const MountedItem part = scene->mountedItem(item_uuid);
		if (part.isNull()) {
			continue;
		}

			//A part already where it is being sent is not part of
			//this step. The rule that built the table drops those
			//as well; it is asked again here because a table typed
			//by a caller that did not use that rule must not be
			//able to fill the stack with steps that move nothing.
		if (MountingMeasure::isSameLength(landing.x(), part.position.x())
		    && MountingMeasure::isSameLength(landing.y(),
						     part.position.y())) {
			continue;
		}

		m_before << qMakePair(item_uuid, part.position);
		m_after.insert(item_uuid, landing);
	}

	if (caption.isEmpty())
	{
		setText(QObject::tr("Aligner des composants sur la platine"));
	}
	else {
		setText(caption);
	}
}

AlignMountedPartsCommand::~AlignMountedPartsCommand()
{}

/**
	@brief AlignMountedPartsCommand::undo
	Put every part back where it was.
*/
void AlignMountedPartsCommand::undo()
{
	if (!m_scene) {
		return;
	}

	for (const QPair<QString, QPointF> &entry : std::as_const(m_before)) {
		m_scene->applyItemPosition(entry.first, entry.second);
	}
}

/**
	@brief AlignMountedPartsCommand::redo
	Put every part where the gesture sends it.
*/
void AlignMountedPartsCommand::redo()
{
	if (!m_scene) {
		return;
	}

	for (const QPair<QString, QPointF> &entry : std::as_const(m_before)) {
		m_scene->applyItemPosition(entry.first,
					   m_after.value(entry.first));
	}
}

/**
	@brief AlignMountedPartsCommand::count
	@return how many parts this moves
*/
int AlignMountedPartsCommand::count() const
{
	return m_before.count();
}

/**
	@brief AlignMountedPartsCommand::itemUuids
	@return which parts this moves, in a settled order
*/
QStringList AlignMountedPartsCommand::itemUuids() const
{
	QStringList uuids;
	uuids.reserve(m_before.size());

	for (const QPair<QString, QPointF> &entry : m_before) {
		uuids << entry.first;
	}

	return uuids;
}

/**
	@brief AlignMountedPartsCommand::before
	@param item_uuid one of the parts moved
	@return where it was when the step was made, millimetre
*/
QPointF AlignMountedPartsCommand::before(const QString &item_uuid) const
{
	for (const QPair<QString, QPointF> &entry : m_before)
	{
		if (entry.first == item_uuid) {
			return entry.second;
		}
	}

	return nowhere();
}

/**
	@brief AlignMountedPartsCommand::after
	@param item_uuid one of the parts moved
	@return where it goes, millimetre
*/
QPointF AlignMountedPartsCommand::after(const QString &item_uuid) const
{
	return m_after.contains(item_uuid) ? m_after.value(item_uuid)
					   : nowhere();
}

/**
	@brief AlignMountedPartsCommand::isNull
	@return true when this would leave a step that undoes nothing
*/
bool AlignMountedPartsCommand::isNull() const
{
	return !m_scene || m_before.isEmpty();
}
