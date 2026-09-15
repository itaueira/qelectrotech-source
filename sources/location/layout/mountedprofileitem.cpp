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
#include "mountedprofileitem.h"

#include "../../QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../bommeasure.h"
#include "../mountingmeasure.h"

#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>

#include <utility>

namespace
{
	/**
		The size of a handle, in the units of the folio the class was
		written for. It is written here rather than asked of
		QETUtils::graphicsHandlerSize, and the reason is worth a line:
		that function reads a property off the diagram editor that owns
		the view, and this drawing has no diagram editor over it. It
		would answer its own default, ten, every single time - so the
		default is taken directly, and nothing pretends to be
		configurable that is not. The handle draws itself at a constant
		size on the screen whatever the zoom, so ten is ten pixels and
		never ten millimetres.
	*/
	const int HANDLE_SIZE = 10;

	/// millimetre: how often a tooth of the comb of a duct is drawn
	const qreal TOOTH_PITCH = 10.0;

	/// how many teeth are worth drawing before the comb becomes a smear
	const int MAX_TEETH = 400;

	/// millimetre: how tall the caption of a piece is written, at most
	const qreal LABEL_HEIGHT = 4.0;

	/// @return the length written the way the material list writes one
	QString shownLength(qreal length)
	{
		return BomMeasure::formatQuantity(length,
						  BomMeasure::defaultLengthUnit());
	}

	/**
		@brief sameFootprint
		@param first a rectangle in millimetre
		@param second another one
		@return true when the two are the same room in the same place

		The slack of the mounting rules on every one of the four
		numbers, borrowed and not declared again. A gesture that ends
		where it began must leave nothing on the undo stack, and the
		answer to "did this change" has to be the same one the fit and
		the dimension give.
	*/
	bool sameFootprint(const QRectF &first, const QRectF &second)
	{
		return MountingMeasure::isSameLength(first.x(), second.x())
		       && MountingMeasure::isSameLength(first.y(), second.y())
		       && MountingMeasure::isSameLength(first.width(),
							second.width())
		       && MountingMeasure::isSameLength(first.height(),
							second.height());
	}
}

/**
	@brief MountedProfileItem::MountedProfileItem
	@param mounted_item the piece, in millimetre
	@param parent parent graphics item
*/
MountedProfileItem::MountedProfileItem(const MountedItem &mounted_item,
				       QGraphicsItem *parent) :
	MountedPartItem(mounted_item, parent)
{}

MountedProfileItem::~MountedProfileItem()
{
	if (!m_handlers.isEmpty()) {
		qDeleteAll(m_handlers);
	}
}

/**
	@brief MountedProfileItem::type
	@return the type of this item
*/
int MountedProfileItem::type() const
{
	return Type;
}

/**
	@brief MountedProfileItem::setMountedItem
	@param mounted_item the piece, in millimetre
*/
void MountedProfileItem::setMountedItem(const MountedItem &mounted_item)
{
	MountedPartItem::setMountedItem(mounted_item);
	adjustHandles();
}

/**
	@brief MountedProfileItem::profile
	@return the bar this piece was cut from
*/
MountingProfile MountedProfileItem::profile() const
{
	return mountedItem().profile;
}

/**
	@brief MountedProfileItem::run
	@return which way the piece runs
*/
MountingRun MountedProfileItem::run() const
{
	return mountedItem().run;
}

/**
	@brief MountedProfileItem::cutLength
	@return how long the piece is, millimetre, zero when nobody cut it
*/
qreal MountedProfileItem::cutLength() const
{
	return mountedItem().cutLength();
}

/**
	@brief MountedProfileItem::footprintRect
	@return the room the piece takes, millimetre
*/
QRectF MountedProfileItem::footprintRect() const
{
	return mapRectToScene(drawnRect());
}

/**
	@brief MountedProfileItem::setFootprintRect
	@param footprint_mm the room the piece takes afterwards, millimetre
*/
void MountedProfileItem::setFootprintRect(const QRectF &footprint_mm)
{
	const QRectF piece = footprint_mm.normalized();

	MountedItem item = mountedItem();
	item.position = piece.topLeft();
	item.size     = piece.size();

	setMountedItem(item);
}

/**
	@brief MountedProfileItem::paint
	@param painter the painter to use
	@param option the style options
	@param widget the widget being painted on

	A bar, and it has to read as a bar at a glance: a rail wears the two
	lines of its hat and a duct wears its comb, so that somebody looking at
	a plate can tell which of the two carries wire without reading a word.
	Both are drawn inside the body and never outside it - what is painted is
	the room the piece takes, and a decoration that stuck out would be a
	drawing claiming room the model does not hold.
*/
void MountedProfileItem::paint(QPainter *painter,
			       const QStyleOptionGraphicsItem *option,
			       QWidget *widget)
{
		//A piece nobody has cut is the hatched marker of the part, and
		//nothing below applies to it: there is no bar to draw the inside
		//of, and drawing one would be this class inventing a length -
		//which is the failure the marker exists against.
	if (!hasDeclaredSize())
	{
		MountedPartItem::paint(painter, option, widget);
		return;
	}

	Q_UNUSED(option)
	Q_UNUSED(widget)

	const QRectF body = drawnRect();
	const bool duct   = profile().isDuct();
	const bool down   = run() == MountingRun::Down;
	const qreal along = down ? body.height() : body.width();
	const qreal across = down ? body.width() : body.height();

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, false);

	QPen outline(QColor(90, 90, 90));
	outline.setCosmetic(true);
	outline.setWidth(isSelected() ? 2 : 1);
	painter->setPen(outline);
	painter->setBrush(QBrush(duct ? QColor(222, 232, 244)
				      : QColor(216, 216, 216)));
	painter->drawRect(body);

	QPen inside(QColor(140, 140, 140));
	inside.setCosmetic(true);
	painter->setPen(inside);

	if (duct)
	{
			//The comb, drawn from both edges inwards. Left out
			//altogether when there would be more teeth than a screen
			//can tell apart: at that zoom it is a grey smear, and a
			//smear costs the same to paint as a drawing.
		const qreal tooth = across / 3.0;
		if (TOOTH_PITCH > 0.0 && along / TOOTH_PITCH < qreal(MAX_TEETH))
		{
			for (qreal at = TOOTH_PITCH ; at < along ; at += TOOTH_PITCH)
			{
				if (down)
				{
					painter->drawLine(
						QPointF(body.left(), body.top() + at),
						QPointF(body.left() + tooth,
							body.top() + at));
					painter->drawLine(
						QPointF(body.right() - tooth,
							body.top() + at),
						QPointF(body.right(), body.top() + at));
				}
				else
				{
					painter->drawLine(
						QPointF(body.left() + at, body.top()),
						QPointF(body.left() + at,
							body.top() + tooth));
					painter->drawLine(
						QPointF(body.left() + at,
							body.bottom() - tooth),
						QPointF(body.left() + at,
							body.bottom()));
				}
			}
		}
	}
	else
	{
			//The hat of a DIN rail, seen from above: the two folds,
			//at a quarter and at three quarters of the section.
		const qreal first  = across / 4.0;
		const qreal second = across * 3.0 / 4.0;

		if (down)
		{
			painter->drawLine(QPointF(body.left() + first, body.top()),
					  QPointF(body.left() + first, body.bottom()));
			painter->drawLine(QPointF(body.left() + second, body.top()),
					  QPointF(body.left() + second, body.bottom()));
		}
		else
		{
			painter->drawLine(QPointF(body.left(), body.top() + first),
					  QPointF(body.right(), body.top() + first));
			painter->drawLine(QPointF(body.left(), body.top() + second),
					  QPointF(body.right(), body.top() + second));
		}
	}

		//The caption is written along the piece, so a piece standing up
		//goes without one: turning the painter to write down the side is
		//a drawing decision of its own, and this step does not take it.
		//Nothing is lost by it - the same words are in the tool tip, and
		//in the list of the window.
	const qreal text_height = qMin(LABEL_HEIGHT, across / 3.0);
	if (!down && text_height >= 1.0)
	{
		QFont font = painter->font();
		font.setPixelSize(qMax(1, qRound(text_height)));
		painter->setFont(font);
		painter->setPen(QPen(QColor(40, 40, 40)));
		painter->setClipRect(body);
		painter->drawText(body, Qt::AlignCenter,
				  tr("%1 — %2 mm")
				  .arg(mountedItem().designation(),
				       shownLength(cutLength())));
	}

	painter->restore();
}

/**
	@brief MountedProfileItem::itemChange
	@param change what is changing
	@param value what it is changing to
	@return the value the framework goes on with

	The three moments the handles have to be kept in step with, and they are
	the three QetShapeItem already listens for: they appear when the piece is
	picked, they follow it when it is dragged, and they go away with it when
	it leaves the scene. The last is not tidiness - a handle left behind on a
	scene whose item has gone is a blue dot nothing can select and nothing
	can delete.
*/
QVariant MountedProfileItem::itemChange(GraphicsItemChange change,
					const QVariant &value)
{
	if (change == ItemSelectedHasChanged)
	{
		if (value.toBool()) {
			addHandles();
		}
		else {
			removeHandles();
		}
	}
	else if (change == ItemPositionHasChanged) {
		adjustHandles();
	}
	else if (change == ItemSceneHasChanged)
	{
		if (!scene()) {
			removeHandles();
		}
	}

	return MountedPartItem::itemChange(change, value);
}

/**
	@brief MountedProfileItem::sceneEventFilter
	@param watched the item the event was for
	@param event the event
	@return true when this item dealt with it

	The wiring QetShapeItem uses, and it is copied rather than invented: the
	handles are items of the scene, they filter their events through here,
	and what comes out is one of the three gestures below. Anything that is
	not one of our own handles is handed straight back.
*/
bool MountedProfileItem::sceneEventFilter(QGraphicsItem *watched,
					  QEvent *event)
{
	if (watched && watched->type() == QetGraphicsHandlerItem::Type)
	{
		QetGraphicsHandlerItem *handler =
				qgraphicsitem_cast<QetGraphicsHandlerItem *>(watched);

		if (handler && m_handlers.contains(handler))
		{
			m_handler_index = int(m_handlers.indexOf(handler));
			if (m_handler_index != -1)
			{
				if (event->type() == QEvent::GraphicsSceneMousePress)
				{
					handlerMousePressEvent();
					return true;
				}
				if (event->type() == QEvent::GraphicsSceneMouseMove)
				{
					handlerMouseMoveEvent(
						static_cast<QGraphicsSceneMouseEvent *>(event));
					return true;
				}
				if (event->type() == QEvent::GraphicsSceneMouseRelease)
				{
					handlerMouseReleaseEvent();
					return true;
				}
			}
		}
	}

	return MountedPartItem::sceneEventFilter(watched, event);
}

/**
	@brief MountedProfileItem::addHandles
	Put a handle on each end of the piece.
*/
void MountedProfileItem::addHandles()
{
	if (!m_handlers.isEmpty() || !scene()) {
		return;
	}

		//A piece nobody has cut gets no handles, and that is the same
		//rule the marker states: what is drawn for it is a marker of a
		//size this program chose, not a bar of a length anybody cut.
		//Handles on the ends of that square would let somebody stretch
		//the marker, and the number they stretched would be the
		//program's own invention turned into a measurement.
	if (!hasDeclaredSize()) {
		return;
	}

	const QVector<QPointF> ends = endPoints();
	if (ends.isEmpty()) {
		return;
	}

	m_handlers = QetGraphicsHandlerItem::handlerForPoint(mapToScene(ends),
							     HANDLE_SIZE);

	for (QetGraphicsHandlerItem *handler : std::as_const(m_handlers))
	{
		handler->setZValue(zValue() + 1);
		handler->setColor(Qt::blue);
		scene()->addItem(handler);
		handler->installSceneEventFilter(this);
	}
}

/**
	@brief MountedProfileItem::removeHandles
	Take the handles off the drawing.
*/
void MountedProfileItem::removeHandles()
{
	if (m_handlers.isEmpty()) {
		return;
	}

	qDeleteAll(m_handlers);
	m_handlers.clear();
	m_handler_index = -1;
	m_stretching = false;
}

/**
	@brief MountedProfileItem::adjustHandles
	Put the handles back on the ends after the piece moved or was cut.
*/
void MountedProfileItem::adjustHandles()
{
	if (m_handlers.isEmpty()) {
		return;
	}

	const QVector<QPointF> ends = endPoints();
	if (ends.size() != m_handlers.size())
	{
		removeHandles();
		addHandles();
		return;
	}

	const QPolygonF scene_ends = mapToScene(ends);
	for (int index = 0 ; index < m_handlers.size() ; ++ index) {
		m_handlers.at(index)->setPos(scene_ends.at(index));
	}
}

/**
	@brief MountedProfileItem::handlerMousePressEvent
	A handle has just been taken hold of.

	What the piece was, kept here because by the time the gesture ends it is
	already something else - the cut happens as the mouse moves, so that what
	is on the screen is what is being decided.
*/
void MountedProfileItem::handlerMousePressEvent()
{
	m_before = footprintRect();
	m_stretching = true;
}

/**
	@brief MountedProfileItem::handlerMouseMoveEvent
	@param event the mouse event

	The end being pulled follows the pointer along the run of the piece and
	is deaf to the other axis: pulling a rail sideways does not move it
	sideways, it does nothing at all. Dragging the whole piece is what moves
	it, and the two gestures are kept apart on purpose - a handle that moved
	the bar as well as cut it would make a rail drift off its holes every
	time somebody trimmed it.
*/
void MountedProfileItem::handlerMouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (!event || !m_stretching || m_handler_index < 0) {
		return;
	}

	const MountingRun way = run();
	const MountingEnd end = m_handler_index == 0 ? MountingEnd::Start
						    : MountingEnd::End;
	const QPointF pointer = event->scenePos();
	const qreal coordinate = way == MountingRun::Down ? pointer.y()
							 : pointer.x();

	setFootprintRect(MountingProfile::stretch(footprintRect(), way, end,
						  coordinate));
}

/**
	@brief MountedProfileItem::handlerMouseReleaseEvent
	The handle has been let go of.
*/
void MountedProfileItem::handlerMouseReleaseEvent()
{
	if (!m_stretching) {
		return;
	}
	m_stretching = false;

	const QRectF before = m_before;
	m_before = QRectF();

	if (sameFootprint(before, footprintRect())) {
		return;
	}

	emit stretched(before);
}

/**
	@brief MountedProfileItem::endPoints
	@return the two ends of the piece, in the coordinates of this item

	The first is the end at the smaller coordinate and the second the other
	one, which is the order MountingEnd names them in: the handle at index
	zero is Start and the handle at index one is End, and that correspondence
	is the whole of what tells the arithmetic which end somebody grabbed.
	Both sit in the middle of the section, where a person aims when they mean
	"the end of this rail" - a handle in a corner would be a handle two
	pieces lying flush against one another would share.
*/
QVector<QPointF> MountedProfileItem::endPoints() const
{
	const QRectF body = drawnRect();
	QVector<QPointF> ends;

	if (run() == MountingRun::Down)
	{
		ends << QPointF(body.center().x(), body.top())
		     << QPointF(body.center().x(), body.bottom());
	}
	else
	{
		ends << QPointF(body.left(), body.center().y())
		     << QPointF(body.right(), body.center().y());
	}

	return ends;
}
