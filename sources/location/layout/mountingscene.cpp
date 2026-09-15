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
#include "mountingscene.h"

#include "../../undocommand/movemountedpartcommand.h"
#include "mountedpartitem.h"

#include <QColor>
#include <QPainter>
#include <QPen>

#include <cmath>
#include <utility>

namespace
{
	/// millimetre: the spacing of the grid drawn over the face
	const qreal GRID_STEP = 10.0;

	/// millimetre: how much room is left around the face, so a part can be
	/// dragged off the plate and still be seen sitting off it
	const qreal SURROUNDING_MARGIN = 20.0;

	/// below this zoom the grid is a grey wash rather than a grid, so it is
	/// not drawn at all
	const qreal GRID_VISIBLE_ZOOM = 0.2;

	/**
		@brief isAPosition
		@param position a position in millimetre
		@return true when both numbers are numbers

		A position that is not one is refused rather than repaired here:
		this is the mutator, and the mutators refuse and say why. The
		tolerant repair belongs to the read.
	*/
	bool isAPosition(const QPointF &position)
	{
		return std::isfinite(position.x()) && std::isfinite(position.y());
	}
}

/**
	@brief MountingScene::MountingScene
	@param parent parent QObject
*/
MountingScene::MountingScene(QObject *parent) :
	QGraphicsScene(parent)
{
	updateSceneRect();
}

MountingScene::~MountingScene()
{}

/**
	@brief MountingScene::setSurface
	@param surface the face and everything mounted on it
*/
void MountingScene::setSurface(const MountingSurface &surface)
{
	m_surface = surface;
	rebuild();
	m_undo_stack.clear();
	updateSceneRect();
	emit surfaceChanged();
}

/**
	@brief MountingScene::surface
	@return the face as it now stands, positions read off the drawing

	Read off the items and not off the copy kept here, so that what this
	answers is what a person is looking at - the middle of a drag included.
	Two truths about where a part is would be one truth too many.
*/
MountingSurface MountingScene::surface() const
{
	MountingSurface surface = m_surface;

	for (int index = 0 ; index < surface.items.size() ; ++ index)
	{
		MountedPartItem *part = partItem(surface.items.at(index).uuid);
		if (part) {
			surface.items[index].position = part->millimetrePosition();
		}
	}

	return surface;
}

/**
	@brief MountingScene::area
	@return how much room there is to mount on, millimetre
*/
MountingArea MountingScene::area() const
{
	return m_surface.area;
}

/**
	@brief MountingScene::isAreaMeasured
	@return true when the area of the face is a usable pair of lengths

	An unmeasured face is a state and not an error: the parts are drawn, and
	nothing is drawn under them. That is honest - the program was refused
	permission to guess how much room a cabinet has - and it is also what
	the person sees when a location has been created and not yet measured.
*/
bool MountingScene::isAreaMeasured() const
{
	return m_surface.area.isValid();
}

/**
	@brief MountingScene::partCount
	@return how many parts are drawn
*/
int MountingScene::partCount() const
{
	return m_items.size();
}

/**
	@brief MountingScene::itemUuids
	@return the identity of every part drawn
*/
QStringList MountingScene::itemUuids() const
{
	QStringList uuids;

	for (const MountedItem &item : std::as_const(m_surface.items))
	{
		if (m_items.contains(item.uuid)) {
			uuids << item.uuid;
		}
	}

	return uuids;
}

/**
	@brief MountingScene::partItem
	@param item_uuid which part
	@return the item drawn for it, nullptr when there is none
*/
MountedPartItem *MountingScene::partItem(const QString &item_uuid) const
{
	return m_items.value(item_uuid, nullptr);
}

/**
	@brief MountingScene::mountedItem
	@param item_uuid which part
	@return what is mounted under it, a default one when nothing is
*/
MountedItem MountingScene::mountedItem(const QString &item_uuid) const
{
	MountedPartItem *part = partItem(item_uuid);

	return part ? part->mountedItem() : MountedItem();
}

/**
	@brief MountingScene::moveItem
	@param item_uuid which part
	@param position_mm where its top left corner goes, millimetre
	@param error filled with why nothing was moved
	@return true when a step was pushed on the stack
*/
bool MountingScene::moveItem(const QString &item_uuid,
			     const QPointF &position_mm,
			     QString *error)
{
	MountedPartItem *part = partItem(item_uuid);
	if (!part)
	{
		if (error) {
			*error = tr("Aucun composant de cet identifiant sur "
				    "cette platine");
		}
		return false;
	}

	if (!isAPosition(position_mm))
	{
		if (error) {
			*error = tr("Une position est deux nombres en "
				    "millimètre");
		}
		return false;
	}

	return pushMove(item_uuid, part->millimetrePosition(), position_mm);
}

/**
	@brief MountingScene::pushMove
	@param item_uuid which part
	@param before_mm where it was, millimetre
	@param after_mm where it goes, millimetre
	@return true when a step was pushed on the stack

	The one place a move reaches the stack, whether it came from a mouse or
	from a number. What counts as no move at all is asked of the command
	itself: two places deciding that would be two places to keep in step,
	and the day they disagree the stack grows a step that undoes nothing.
*/
bool MountingScene::pushMove(const QString &item_uuid,
			     const QPointF &before_mm,
			     const QPointF &after_mm)
{
	MoveMountedPartCommand *command =
			new MoveMountedPartCommand(this, item_uuid,
						   before_mm, after_mm);

	if (command->isNull())
	{
		delete command;
		return false;
	}

	m_undo_stack.push(command);
	return true;
}

/**
	@brief MountingScene::applyItemPosition
	@param item_uuid which part
	@param position_mm where its top left corner goes, millimetre
	@return true when there was such a part
*/
bool MountingScene::applyItemPosition(const QString &item_uuid,
				      const QPointF &position_mm)
{
	MountedPartItem *part = partItem(item_uuid);
	if (!part || !isAPosition(position_mm)) {
		return false;
	}

	part->setMillimetrePosition(position_mm);

	const int index = m_surface.indexOfItem(item_uuid);
	if (index >= 0) {
		m_surface.items[index].position = part->millimetrePosition();
	}

	updateSceneRect();
	emit itemMoved(item_uuid);
	emit surfaceChanged();

	return true;
}

/**
	@brief MountingScene::undoStack
	@return the stack the moves of this surface are on
*/
QUndoStack &MountingScene::undoStack()
{
	return m_undo_stack;
}

/**
	@brief MountingScene::sceneFromMillimetre
	@param position_mm a position in millimetre
	@return the same position
*/
QPointF MountingScene::sceneFromMillimetre(const QPointF &position_mm)
{
	return position_mm;
}

/**
	@brief MountingScene::millimetreFromScene
	@param scene_position a position on this scene
	@return the millimetre it stands for
*/
QPointF MountingScene::millimetreFromScene(const QPointF &scene_position)
{
	return scene_position;
}

/**
	@brief MountingScene::gridStep
	@return the spacing of the grid drawn on the face, millimetre
*/
qreal MountingScene::gridStep()
{
	return GRID_STEP;
}

/**
	@brief MountingScene::surroundingMargin
	@return the room left around the face on the scene, millimetre
*/
qreal MountingScene::surroundingMargin()
{
	return SURROUNDING_MARGIN;
}

/**
	@brief MountingScene::drawBackground
	@param painter the painter to use
	@param rect the part of the scene to be drawn

	The face is painted here, as background, and this is the whole of the
	rule that says it cannot be moved: there is no item to move. The grid is
	a grid of millimetres and nothing snaps to it - it is a ruler laid on
	the plate, and a part put at 22,5 mm stays at 22,5 mm.
*/
void MountingScene::drawBackground(QPainter *painter, const QRectF &rect)
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, false);

	painter->setPen(Qt::NoPen);
	painter->setBrush(QColor(246, 246, 246));
	painter->drawRect(rect);

	if (isAreaMeasured())
	{
		const QRectF plate = m_surface.area.rect();

		QPen edge(QColor(80, 80, 80));
		edge.setCosmetic(true);
		painter->setPen(edge);
		painter->setBrush(QColor(255, 255, 255));
		painter->drawRect(plate);

		const QRectF grid_rect = plate.intersected(rect);
		if (!grid_rect.isEmpty()
		    && painter->transform().m11() > GRID_VISIBLE_ZOOM)
		{
			QPen grid(QColor(214, 214, 214));
			grid.setCosmetic(true);
			painter->setPen(grid);

			const qreal step = gridStep();
			for (qreal x = std::ceil(grid_rect.left() / step) * step ;
			     x <= grid_rect.right() ;
			     x += step)
			{
				painter->drawLine(QPointF(x, grid_rect.top()),
						  QPointF(x, grid_rect.bottom()));
			}
			for (qreal y = std::ceil(grid_rect.top() / step) * step ;
			     y <= grid_rect.bottom() ;
			     y += step)
			{
				painter->drawLine(QPointF(grid_rect.left(), y),
						  QPointF(grid_rect.right(), y));
			}
		}
	}

	painter->restore();
}

/**
	@brief MountingScene::rebuild
	Draw the face from scratch.

	A part with no identity is drawn and cannot be addressed afterwards: the
	layout gives one to everything it mounts, so this is what becomes of a
	surface somebody built by hand. A repeated identity keeps the first, for
	the same reason - the model refuses to mount the same part twice, and
	silently drawing the second over the first would hide the day it stops
	refusing.
*/
void MountingScene::rebuild()
{
	clear();
	m_items.clear();

	for (const MountedItem &item : std::as_const(m_surface.items))
	{
		MountedPartItem *part = new MountedPartItem(item);
		addItem(part);

		if (!item.uuid.isEmpty() && !m_items.contains(item.uuid)) {
			m_items.insert(item.uuid, part);
		}

		connect(part, &MountedPartItem::dragged, this,
			[this, part](const QPointF &previous_position_mm)
			{
				endDrag(part, previous_position_mm);
			});
	}
}

/**
	@brief MountingScene::updateSceneRect
	Keep the face and everything on it reachable.

	The parts are taken in as well as the plate, on purpose: a part that
	does not fit where it was put sits off the plate, and a scene rectangle
	that stopped at the edge of the plate would put it where nobody can
	scroll to it. Being off the plate is the report, and the report has to
	be visible.
*/
void MountingScene::updateSceneRect()
{
	QRectF rect;

	if (isAreaMeasured()) {
		rect = m_surface.area.rect();
	}
	rect = rect.united(itemsBoundingRect());

	if (rect.isNull()) {
		rect = QRectF(0.0, 0.0, gridStep(), gridStep());
	}

	setSceneRect(rect.adjusted(-surroundingMargin(), -surroundingMargin(),
				   surroundingMargin(), surroundingMargin()));
}

/**
	@brief MountingScene::endDrag
	@param part the part the person has just let go of
	@param previous_position_mm where it was when the gesture began

	The part has already moved by now, so the step pushed here is the one
	that takes it back. A part with no identity is left where it was
	dropped and no step is pushed: there is nothing to address it by, and a
	step that cannot name what it moves cannot undo it either.
*/
void MountingScene::endDrag(MountedPartItem *part,
			    const QPointF &previous_position_mm)
{
	if (!part) {
		return;
	}

	const QString item_uuid = part->uuid();
	if (item_uuid.isEmpty() || !m_items.contains(item_uuid)) {
		return;
	}

	pushMove(item_uuid, previous_position_mm, part->millimetrePosition());
}
