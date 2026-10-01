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
#include "../../ElementsCollection/elementslocation.h"
#include "../../elementprovider.h"
#include "../../qetgraphicsitem/element.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QPen>
#include <QPolygonF>
#include <QUuid>
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
	updateDrawingToolTip();
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
		//No catalogue box, but a drawing in millimetre: the drawing at its
		//own size. Still not a measurement - hasDeclaredSize() keeps saying
		//so, and the model is told nothing.
	if (hasLayoutDrawing()) {
		return QRectF(0.0, 0.0,
			      qMax(m_drawing.bounds.width(), 1.0),
			      qMax(m_drawing.bounds.height(), 1.0));
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
		//A drawing at real size may be larger than its catalogue box, and
		//what is painted has to be inside what is declared
	QRectF area = drawnRect();
	if (hasLayoutDrawing()) {
		area |= layoutDrawingRect();
	}
	return area.adjusted(-OUTLINE_SLACK, -OUTLINE_SLACK,
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

	if (hasLayoutDrawing())
	{
			//The drawing replaces the plain box, and the name is written
			//over it. The box is still traced, light, because it is the
			//room the part takes and the one the fit check reads: dotted
			//when the catalogue gave it, dashed in the colour of "not
			//measured" when nobody did.
		painter->save();
		QPen outline(measured ? QColor(150, 150, 150) : QColor(176, 106, 0));
		outline.setCosmetic(true);
		outline.setWidth(isSelected() ? 2 : 1);
		outline.setStyle(measured ? Qt::DotLine : Qt::DashLine);
		painter->setPen(outline);
		painter->setBrush(QColor(255, 255, 255));
		painter->drawRect(body);

		const QRectF target = layoutDrawingRect();
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing, true);
			//Real size, never scaled: a unit of the drawing is a millimetre
			//of the plate. Not clipped to the box either - a drawing larger
			//than the catalogue box shows by how much.
		painter->translate(target.topLeft() - m_drawing.bounds.topLeft());
		painter->drawPicture(QPointF(0.0, 0.0), m_drawing.picture);
		painter->restore();

			//The name stays readable over the drawing: three breakers drawn
			//alike are three parts nobody can tell apart otherwise
		paintDesignationOver(painter, body);

		if (drawingSizeDiffers())
		{
				//A corner, not a colour over the whole part: the drawing
				//is right, it is its fit in the box that is in question
			const qreal side = qMax(1.0, qMin(qMin(body.width(), body.height()) / 4.0, 6.0));
			QPolygonF mark;
			mark << body.topRight()
			     << body.topRight() - QPointF(side, 0.0)
			     << body.topRight() + QPointF(0.0, side);
			painter->setPen(Qt::NoPen);
			painter->setBrush(QColor(220, 120, 0));
			painter->drawPolygon(mark);
		}
		painter->restore();
		return;
	}

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
	@brief MountedPartItem::setLayoutDrawing
	@param drawing the layout_drawing of the element this part is
*/
void MountedPartItem::setLayoutDrawing(const ElementPictureFactory::LayoutDrawing &drawing)
{
	prepareGeometryChange();
	m_drawing = drawing;
	updateDrawingToolTip();
	update();
}

bool MountedPartItem::hasLayoutDrawing() const
{
	return !m_drawing.isNull();
}

/**
	@brief MountedPartItem::layoutDrawingRect
	@return where the drawing is painted: at its real size, centred on the
	catalogue box when there is one, and filling the dashed outline of an
	unmeasured part when there is not
*/
QRectF MountedPartItem::layoutDrawingRect() const
{
	if (!hasLayoutDrawing()) {
		return QRectF();
	}
	QRectF target(QPointF(0.0, 0.0), m_drawing.bounds.size());
	target.moveCenter(drawnRect().center());
	return target;
}

bool MountedPartItem::drawingSizeDiffers() const
{
	if (!hasLayoutDrawing() || !hasDeclaredSize()) {
		return false;
	}
	const QSizeF box = declaredSize();
	auto differs = [](qreal drawn, qreal declared) {
		const qreal allowed = qMax(drawingToleranceMm(), drawingTolerance() * declared);
		return std::abs(drawn - declared) > allowed;
	};
	return differs(m_drawing.bounds.width(), box.width())
	       || differs(m_drawing.bounds.height(), box.height());
}

qreal MountedPartItem::drawingTolerance()
{
	return 0.02;
}

qreal MountedPartItem::drawingToleranceMm()
{
	return 0.5;
}

/**
	@brief MountedPartItem::paintDesignationOver
	The designation, on a pale plate so that the lines of the drawing do
	not cross it out, at the size the plain box uses.
*/
void MountedPartItem::paintDesignationOver(QPainter *painter, const QRectF &body) const
{
	const qreal text_height = qMin(LABEL_HEIGHT, body.height() / 3.0);
	if (text_height < 1.0) {
		return;
	}
	painter->save();
	QFont font = painter->font();
	font.setPixelSize(qMax(1, qRound(text_height)));
	painter->setFont(font);
	const QString text = m_item.designation();
	const QRectF text_rect = QFontMetricsF(font)
				 .boundingRect(body, Qt::AlignCenter, text)
				 .adjusted(-text_height * 0.2, 0.0, text_height * 0.2, 0.0)
				 .intersected(body);
	painter->setPen(Qt::NoPen);
	painter->setBrush(QColor(255, 255, 255, 210));
	painter->drawRect(text_rect);
	painter->setPen(QPen(QColor(40, 40, 40)));
	painter->drawText(body, Qt::AlignCenter, text);
	painter->restore();
}

/**
	@brief MountedPartItem::updateDrawingToolTip
	The designation, and when drawing and box disagree, the sentence that
	says so: the mark on the corner is what is seen, this is what is read.
*/
void MountedPartItem::updateDrawingToolTip()
{
	QString tip = m_item.designation();
	if (drawingSizeDiffers()) {
		tip += QLatin1Char('\n')
		       + tr("Le dessin 2D n'a pas la taille de la boîte du "
			    "catalogue : il est montré en vraie grandeur, et c'est "
			    "la boîte qui compte pour l'implantation.");
	}
	setToolTip(tip);
}

/**
	@brief MountedPartItem::layoutDrawingsOf
	@param project the project the elements are in
	@param items the mounted items to find the drawings of
	@return the drawing of each element, by the uuid of the element as
	QUuid::toString() writes it
*/
QHash<QString, ElementPictureFactory::LayoutDrawing>
MountedPartItem::layoutDrawingsOf(QETProject *project, const QList<MountedItem> &items)
{
	QHash<QString, ElementPictureFactory::LayoutDrawing> drawings;
	if (!project) {
		return drawings;
	}

	QList<QUuid> wanted;
	for (const MountedItem &item : items) {
		if (!item.element.isEmpty()) {
			wanted << QUuid(item.element);
		}
	}
	if (wanted.isEmpty()) {
		return drawings;
	}

		//Read from the definition the project holds, not from a collection
		//on this computer: that copy is what travels with the project
	for (Element *element : ElementProvider(project).fromUuids(wanted))
	{
		const auto drawing = ElementPictureFactory::layoutDrawing(element->location().xml());
		if (!drawing.isNull()) {
			drawings.insert(element->uuid().toString(), drawing);
		}
	}
	return drawings;
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
