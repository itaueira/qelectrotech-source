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
#include "mountedpartitem.h"

#include "../mountingmeasure.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOptionGraphicsItem>

#include <cmath>

namespace
{
	/// millimetre: the side of the square that stands in for a part nobody
	/// has measured. A marker, and never a measurement - see the class
	/// documentation for what happens when the two are confused.
	const qreal MARKER_SIDE = 10.0;

	/// millimetre: the slack the bounding rectangle leaves around what is
	/// painted, so a cosmetic outline is never clipped by its own item.
	const qreal OUTLINE_SLACK = 0.5;

	/// millimetre: how tall the mark of a part is written, at most.
	const qreal LABEL_HEIGHT = 4.0;

	/**
		@brief finitePosition
		@param position a position in millimetre
		@return the same position, with whatever is not a number read as
		zero

		The read of the layout is tolerant on purpose, so a file may hand
		over a position that is not one. Repairing it here keeps the part
		on the surface, where it can be seen and dragged somewhere sane;
		refusing it would make a damaged file into a lost part.
	*/
	QPointF finitePosition(const QPointF &position)
	{
		return QPointF(std::isfinite(position.x()) ? position.x() : 0.0,
			       std::isfinite(position.y()) ? position.y() : 0.0);
	}

	/**
		@brief sameMillimetre
		@param first a position in millimetre
		@param second another one
		@return true when the two are the same place

		Borrowed from MountingMeasure and not written again, coordinate
		by coordinate. It is the same slack the fit and the dimension
		already use, and having one answer to "are these two numbers the
		same" is the whole reason that function was put somewhere both
		could reach: a drag that thought it had moved something a
		dimension thought had not moved would be a disagreement nobody
		would ever look for.
	*/
	bool sameMillimetre(const QPointF &first, const QPointF &second)
	{
		return MountingMeasure::isSameLength(first.x(), second.x())
		       && MountingMeasure::isSameLength(first.y(), second.y());
	}
}

/**
	@brief MountedPartItem::MountedPartItem
	@param mounted_item what is mounted, in millimetre
	@param parent parent graphics item
*/
MountedPartItem::MountedPartItem(const MountedItem &mounted_item,
				 QGraphicsItem *parent) :
	QGraphicsObject(parent),
	m_item(mounted_item)
{
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setFlag(QGraphicsItem::ItemIsMovable, true);
	setAcceptedMouseButtons(Qt::LeftButton);

	m_item.position = finitePosition(m_item.position);
	setPos(m_item.position);
	setToolTip(m_item.designation());
}

MountedPartItem::~MountedPartItem()
{}

/**
	@brief MountedPartItem::type
	@return the type of this item
*/
int MountedPartItem::type() const
{
	return Type;
}

/**
	@brief MountedPartItem::uuid
	@return the identity the layout stitches everything onto
*/
QString MountedPartItem::uuid() const
{
	return m_item.uuid;
}

/**
	@brief MountedPartItem::mountedItem
	@return the item as the model holds it, dragged position included
*/
MountedItem MountedPartItem::mountedItem() const
{
	MountedItem item = m_item;
	item.position = millimetrePosition();
	return item;
}

/**
	@brief MountedPartItem::setMountedItem
	@param mounted_item what is mounted, millimetre
*/
void MountedPartItem::setMountedItem(const MountedItem &mounted_item)
{
	prepareGeometryChange();
	m_item = mounted_item;
	m_item.position = finitePosition(m_item.position);
	setPos(m_item.position);
	setToolTip(m_item.designation());
	update();
}

/**
	@brief MountedPartItem::hasDeclaredSize
	@return true when both dimensions of the part are known
*/
bool MountedPartItem::hasDeclaredSize() const
{
	return m_item.hasDeclaredSize();
}

/**
	@brief MountedPartItem::declaredSize
	@return the room the part takes, millimetre, zero when unmeasured
*/
QSizeF MountedPartItem::declaredSize() const
{
	return m_item.declaredSize();
}

/**
	@brief MountedPartItem::drawnRect
	@return what is painted, in the coordinates of this item

	Two rectangles and one of them is a lie told out loud: the footprint of
	a measured part, and a marker of a fixed side for a part nobody has
	measured. The second never leaves this class - declaredSize keeps
	answering zero about that part, and every rule downstream keeps counting
	it as unmeasured.
*/
QRectF MountedPartItem::drawnRect() const
{
	if (hasDeclaredSize()) {
		return QRectF(QPointF(0.0, 0.0), declaredSize());
	}

	return QRectF(0.0, 0.0, unmeasuredMarkerSide(), unmeasuredMarkerSide());
}

/**
	@brief MountedPartItem::millimetrePosition
	@return where the top left corner sits, millimetre

	pos() is the truth about where this part is, and m_item.position is a
	copy of it that goes stale while a drag is in progress. Everything that
	reads a position reads it through here.
*/
QPointF MountedPartItem::millimetrePosition() const
{
	return pos();
}

/**
	@brief MountedPartItem::setMillimetrePosition
	@param position_mm where the top left corner goes, millimetre
*/
void MountedPartItem::setMillimetrePosition(const QPointF &position_mm)
{
	const QPointF wanted = finitePosition(position_mm);
	if (sameMillimetre(wanted, pos())) {
		return;
	}

	setPos(wanted);
	m_item.position = wanted;
}

/**
	@brief MountedPartItem::boundingRect
	@return what is painted, plus the slack the outline needs
*/
QRectF MountedPartItem::boundingRect() const
{
	return drawnRect().adjusted(-OUTLINE_SLACK, -OUTLINE_SLACK,
				    OUTLINE_SLACK, OUTLINE_SLACK);
}

/**
	@brief MountedPartItem::shape
	@return the shape a click has to land in

	The drawn rectangle and not the bounding one: the slack above exists for
	the pen, and picking a part half a millimetre away from it would make
	two parts flush against each other impossible to tell apart by clicking.
*/
QPainterPath MountedPartItem::shape() const
{
	QPainterPath path;
	path.addRect(drawnRect());
	return path;
}

/**
	@brief MountedPartItem::paint
	@param painter the painter to use
	@param option the style options
	@param widget the widget being painted on

	A measured part is a plain rectangle of its own size. An unmeasured one
	is hatched and dashed, because the one thing it must not look like is a
	part of ten millimetres.
*/
void MountedPartItem::paint(QPainter *painter,
			    const QStyleOptionGraphicsItem *option,
			    QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	const QRectF body = drawnRect();
	const bool measured = hasDeclaredSize();

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, false);

	QPen outline(measured ? QColor(60, 60, 60) : QColor(176, 106, 0));
	outline.setCosmetic(true);
	outline.setWidth(isSelected() ? 2 : 1);
	if (!measured) {
		outline.setStyle(Qt::DashLine);
	}

	painter->setPen(outline);
	painter->setBrush(measured
			  ? QBrush(QColor(236, 236, 236))
			  : QBrush(QColor(176, 106, 0), Qt::BDiagPattern));
	painter->drawRect(body);

		//The mark of the part, inside its own body. A part with no room to
		//write in is left unwritten rather than written over its
		//neighbour: the list of the editor says what the drawing cannot.
	const qreal text_height = qMin(LABEL_HEIGHT, body.height() / 3.0);
	if (text_height >= 1.0) {
		QFont font = painter->font();
		font.setPixelSize(qMax(1, qRound(text_height)));
		painter->setFont(font);
		painter->setPen(QPen(QColor(40, 40, 40)));
		painter->setClipRect(body);
		painter->drawText(body, Qt::AlignCenter, m_item.designation());
	}

	painter->restore();
}

/**
	@brief MountedPartItem::unmeasuredMarkerSide
	@return the side of the square drawn for an unmeasured part, millimetre
*/
qreal MountedPartItem::unmeasuredMarkerSide()
{
	return MARKER_SIDE;
}

/**
	@brief MountedPartItem::mousePressEvent
	@param event the mouse event

	Where the part was when the gesture began, remembered here because by
	the time it ends the framework has already moved it.
*/
void MountedPartItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	m_press_position = millimetrePosition();
	m_pressed = true;
	QGraphicsObject::mousePressEvent(event);
}

/**
	@brief MountedPartItem::mouseReleaseEvent
	@param event the mouse event

	A gesture that ended where it began is not a move, and it must not leave
	a step on a stack: an undo that undoes nothing is worse than no undo at
	all, because the next one then undoes something the person had forgotten
	about.
*/
void MountedPartItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	QGraphicsObject::mouseReleaseEvent(event);

	if (!m_pressed) {
		return;
	}
	m_pressed = false;

	if (sameMillimetre(m_press_position, millimetrePosition())) {
		return;
	}

	emit dragged(m_press_position);
}
