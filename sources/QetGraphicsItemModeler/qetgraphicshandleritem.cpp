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
#include "qetgraphicshandleritem.h"
#include <QGraphicsView>
#include <QPainter>
#include <QDebug>
#include <utility>
#include <QPropertyAnimation>

/**
	@brief QetGraphicsHandlerItem::QetGraphicsHandlerItem
	@param size : the size of the handler
*/
QetGraphicsHandlerItem::QetGraphicsHandlerItem(qreal size)
{
	setAcceptHoverEvents(true);
	setFlag(QGraphicsItem::ItemIgnoresTransformations);
	setSize(size);
}

void QetGraphicsHandlerItem::setSize(qreal size)
{
	if (m_size_animation) {
		m_size_animation->stop();
		m_size_animation->deleteLater();
		m_size_animation.clear();
	}
	prepareGeometryChange();
	m_current_size = m_original_size = size;
	m_handler_rect.setRect(0-m_current_size/2, 0-m_current_size/2, m_current_size, m_current_size);
	m_br.setRect(-1-m_current_size/2, -1-m_current_size/2, m_current_size+2, m_current_size+2);
	update();
}

/**
	@brief QetGraphicsHandlerItem::boundingRect
	@return 
*/
QRectF QetGraphicsHandlerItem::boundingRect() const
{
	return m_br;
}

/**
	@brief QetGraphicsHandlerItem::setColor
	@param color : set the color of the handler
*/
void QetGraphicsHandlerItem::setColor(QColor color)
{
	m_color = std::move(color);
	update();
}

/**
	@brief QetGraphicsHandlerItem::paint
	@param painter
	@param option
	@param widget
*/
void QetGraphicsHandlerItem::paint(QPainter *painter,
				   const QStyleOptionGraphicsItem *option,
				   QWidget *widget)
{
	Q_UNUSED(option)

	painter->save();
		//Smaller with the drawing when it is zoomed out: at a fixed size on
		//screen, a handle covered a symbol drawn smaller than itself
		//(decision of 20/10/2026). Its full size at 250% and up, where a
		//text of the usual size 5 is tall enough beside it; in proportion
		//below, down to 40%, reached at 100%, so that it is still seen.
		//Shrinking only below 100% left it whole at the zoom one works at,
		//and nothing was seen to change. Only what is drawn: the place to
		//take it (boundingRect()) keeps its size, as easy to take as before.
	if (const auto *view = widget ? qobject_cast<const QGraphicsView *>(widget->parentWidget())
				       : nullptr) {
		const qreal scale = qBound(0.4, view->transform().m11() / 2.5, 1.0);
		painter->scale(scale, scale);
	}
	painter->setBrush(QBrush(m_color));
	QPen pen(QBrush(m_color),
		 2,
		 Qt::SolidLine,
		 Qt::SquareCap,
		 Qt::MiterJoin);
	pen.setCosmetic(true);
	painter->setPen(pen);
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->drawEllipse(m_handler_rect);
	painter->restore();
}

void QetGraphicsHandlerItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	Q_UNUSED(event);

	if (m_size_animation) {
		m_size_animation->stop();
		m_size_animation->deleteLater();
	}
	auto animation_ = new QPropertyAnimation(this, "currentSize", this);
	m_size_animation = animation_;
	animation_->setStartValue(m_original_size);
	animation_->setEndValue(m_original_size*1.5);
	animation_->setDuration(200);
	animation_->setEasingCurve(QEasingCurve::OutBack);
	animation_->start(QAbstractAnimation::DeleteWhenStopped);
}

void QetGraphicsHandlerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	Q_UNUSED(event);

	if (m_size_animation) {
		m_size_animation->stop();
		m_size_animation->deleteLater();
	}
	auto animation_ = new QPropertyAnimation(this, "currentSize", this);
	m_size_animation = animation_;
	animation_->setStartValue(m_current_size);
	animation_->setEndValue(m_original_size);
	animation_->setDuration(200);
	animation_->setEasingCurve(QEasingCurve::OutBack);
	animation_->start(QAbstractAnimation::DeleteWhenStopped);
}

void QetGraphicsHandlerItem::setCurrentSize(qreal size)
{
	prepareGeometryChange();
	m_current_size = size;
	m_handler_rect.setRect(0-m_current_size/2, 0-m_current_size/2, m_current_size, m_current_size);
	m_br.setRect(-1-m_current_size/2, -1-m_current_size/2, m_current_size+2, m_current_size+2);
}

/**
	@brief QetGraphicsHandlerItem::handlerForPoint
	@param points
	@param size
	@return A list of handler with pos at point
*/
QVector<QetGraphicsHandlerItem *> QetGraphicsHandlerItem::handlerForPoint(
		const QVector<QPointF> &points,
		qreal size)
{
	QVector <QetGraphicsHandlerItem *> list_;
	for (QPointF point : points)
	{
		QetGraphicsHandlerItem *qghi = new QetGraphicsHandlerItem(size);
		qghi->setPos(point);
		list_ << qghi;
	}

	return list_;
}
