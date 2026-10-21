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
#include "assemblychangemarks.h"

#include "../autoNum/assemblychanges.h"
#include "../qetproject.h"
#include "assemblywalk.h"

#include <QGraphicsObject>
#include <QPainter>
#include <QPainterPath>
#include <QUndoStack>

namespace
{
		/// Apart from the blue of the highlight, and from the red of a wire
		/// selected: what changed since the panel was wired
	const QColor mark_colour(255, 128, 0);
}

/**
	@brief AssemblyChangeMarks::of
	@return the marks of @a project, made the first time they are asked for,
	or nullptr without a project
*/
AssemblyChangeMarks *AssemblyChangeMarks::of(QETProject *project)
{
	if (!project) {
		return nullptr;
	}
	if (auto *marks = project->findChild<AssemblyChangeMarks *>(QString(), Qt::FindDirectChildrenOnly)) {
		return marks;
	}
	return new AssemblyChangeMarks(project);
}

/**
	@brief AssemblyChangeMarks::AssemblyChangeMarks
	Watches @a project: every change of it - a step done, undone or done
	again, which marking the project as wired is too - asks for the marks
	to be worked out again a moment later, once and not at every step of a
	run of them.
*/
AssemblyChangeMarks::AssemblyChangeMarks(QETProject *project) :
	QObject(project),
	m_project(project)
{
	m_timer.setSingleShot(true);
	m_timer.setInterval(250);
	connect(&m_timer, &QTimer::timeout, this, &AssemblyChangeMarks::refresh);
	if (QUndoStack *stack = project->undoStack()) {
		connect(stack, &QUndoStack::indexChanged, &m_timer, [this]() {m_timer.start();});
	}
		//A project opened already marked has its marks from the start
	m_timer.start();
}

/**
	@brief AssemblyChangeMarks::isMarked
	@return whether @a item wears the mark
*/
bool AssemblyChangeMarks::isMarked(const QGraphicsItem *item)
{
	return item && item->data(MarkKey).toBool();
}

/**
	@brief AssemblyChangeMarks::drawAround
	The mark of a component: a rounded frame around @a box, filled lightly
	enough to leave the symbol readable
*/
void AssemblyChangeMarks::drawAround(QPainter *painter, const QRectF &box)
{
	painter->save();
	QPen pen(mark_colour, 2);
	pen.setCosmetic(true);
	painter->setPen(pen);
	QColor fill(mark_colour);
	fill.setAlpha(40);
	painter->setBrush(fill);
	painter->drawRoundedRect(box.adjusted(-2, -2, 2, 2), 3, 3);
	painter->restore();
}

/**
	@brief AssemblyChangeMarks::drawAlong
	The mark of a wire: a wide stroke under it, along @a path
*/
void AssemblyChangeMarks::drawAlong(QPainter *painter, const QPainterPath &path)
{
	painter->save();
	QColor colour(mark_colour);
	colour.setAlpha(110);
	QPen pen(colour, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawPath(path);
	painter->restore();
}

/**
	@brief AssemblyChangeMarks::refresh
	Puts the mark on what the order of service lists to mount, to label
	again or to number again, and takes it off the rest. Nothing is marked
	on a project that is not marked as wired.
*/
void AssemblyChangeMarks::refresh()
{
	m_timer.stop();
	QList<QPointer<QGraphicsObject>> now;
	if (m_project && m_project->assemblyState().isFrozen())
	{
		const AssemblyWalk walked = AssemblyWalk::of(m_project);
		const AssemblyChanges changes = AssemblyChanges::compare(
					m_project->assemblyState(), walked.components, walked.wires);
		for (const AssemblyChange &change : changes.components.to_add + changes.components.to_relabel)
		{
			if (change.source >= 0 && change.source < walked.component_objects.size()) {
				if (Element *element = walked.component_objects.at(change.source)) {
					now << QPointer<QGraphicsObject>(element);
				}
			}
		}
		for (const AssemblyChange &change : changes.wires.to_add + changes.wires.to_relabel)
		{
			if (change.source >= 0 && change.source < walked.wire_objects.size()) {
				if (Conductor *wire = walked.wire_objects.at(change.source)) {
					now << QPointer<QGraphicsObject>(wire);
				}
			}
		}
	}

	for (const QPointer<QGraphicsObject> &item : std::as_const(m_marked))
	{
		if (item && !now.contains(item)) {
			item->setData(MarkKey, QVariant());
			item->update();
		}
	}
	for (const QPointer<QGraphicsObject> &item : std::as_const(now))
	{
		if (item && !isMarked(item)) {
			item->setData(MarkKey, true);
			item->update();
		}
	}
	m_marked = now;
}
