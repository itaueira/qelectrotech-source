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
#include "papergraphicsview.h"

#include "qetpalette.h"

#include <QApplication>
#include <QEvent>
#include <QPaintEvent>
#include <QStyleHintReturnMask>
#include <QStyleOptionRubberBand>
#include <QtMath>

namespace {
	/**
		QGraphicsScene::drawItems() is protected; naming it through a
		derived class is the standard way to a pointer to it. The scene is
		called with the viewport as its widget while the painter is on the
		off-screen image, which is what makes it record where each item was
		painted (see PaletteGraphicsView, which does the same).
	*/
	struct SceneAccess : QGraphicsScene
	{
		using DrawItems = void (QGraphicsScene::*)(QPainter *, int, QGraphicsItem *[],
		                                           const QStyleOptionGraphicsItem[], QWidget *);
		static DrawItems drawItemsPointer() { return &SceneAccess::drawItems; }
	};
}

PaperGraphicsView::PaperGraphicsView(QWidget *parent) :
	QGraphicsView(parent)
{
	qApp->installEventFilter(this);
}

PaperGraphicsView::PaperGraphicsView(QGraphicsScene *scene, QWidget *parent) :
	QGraphicsView(scene, parent)
{
	qApp->installEventFilter(this);
}

/**
	@brief PaperGraphicsView::invertsLightness
	@return true when the scene is shown with its lightness inverted, i.e.
	when the application palette is dark. The application palette and not
	the view's own, for the reason PaletteGraphicsView::invertsLightness()
	gives: under a style sheet, palette() can be the one in force before a
	live light/dark switch.
*/
bool PaperGraphicsView::invertsLightness() const
{
	return QET::Palette::isDark(QApplication::palette());
}

bool PaperGraphicsView::isPaintingInvertedOn(const QWidget *widget)
{
	if (!widget)
		return false;
	const auto *view = qobject_cast<const PaperGraphicsView *>(widget->parentWidget());
	return view && view->viewport() == widget && view->m_inverting;
}

/**
	@brief PaperGraphicsView::eventFilter
	Repaint the whole viewport when the application palette changes: the
	one notification Qt always sends, whatever the style sheets above the
	view (see PaletteGraphicsView::eventFilter()).
*/
bool PaperGraphicsView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange)
		viewport()->update();
	return QGraphicsView::eventFilter(watched, event);
}

/**
	@brief PaperGraphicsView::paintEvent
	Paints as QGraphicsView on a light palette, inverted on a dark one.
*/
void PaperGraphicsView::paintEvent(QPaintEvent *event)
{
	if (invertsLightness())
	{
		paintInverted(event);
		return;
	}
	m_buffer = QImage();
	QGraphicsView::paintEvent(event);
}

/**
	@brief PaperGraphicsView::paintInverted
	Run QGraphicsView::paintEvent() with the drawing hooks redirected to an
	off-screen image of the viewport laid on white paper, invert the
	lightness of the exposed part between the palette's Base and Text
	colors, blit it, and draw the rubber band over it: the one
	QGraphicsView::paintEvent() drew went under the blit.
	@param event the paint event, for the exposed area
*/
void PaperGraphicsView::paintInverted(QPaintEvent *event)
{
	const QRect exposed = event->rect().intersected(viewport()->rect());
	if (exposed.isEmpty())
		return;

	const qreal ratio = viewport()->devicePixelRatioF();
	const QSize size(qCeil(viewport()->width() * ratio), qCeil(viewport()->height() * ratio));
	if (m_buffer.size() != size || m_buffer.devicePixelRatio() != ratio)
	{
		m_buffer = QImage(size, QImage::Format_ARGB32_Premultiplied);
		m_buffer.setDevicePixelRatio(ratio);
	}

	m_buffer_painter.begin(&m_buffer);
		// What the scene leaves blank is the white paper, which the
		// inversion turns into the Base color.
	m_buffer_painter.fillRect(exposed, Qt::white);
	m_buffer_painter.setClipRect(exposed);
	m_buffer_painter.setRenderHints(renderHints());
	m_buffer_painter.setWorldTransform(viewportTransform());

	m_inverting = true;
	const OptimizationFlags flags = optimizationFlags();
	setOptimizationFlag(QGraphicsView::IndirectPainting, true);
	QGraphicsView::paintEvent(event);
	setOptimizationFlags(flags);
	m_inverting = false;
	m_buffer_painter.end();

	const QRect pixels = QRectF(exposed.topLeft() * ratio, exposed.size() * ratio).toAlignedRect();
	QImage part = m_buffer.copy(pixels);
	part.setDevicePixelRatio(ratio);
	const QPalette palette = QApplication::palette();
	QET::Palette::invertLightness(part, palette.color(QPalette::Base),
	                              palette.color(QPalette::Text));

	QPainter painter(viewport());
	painter.drawImage(QPointF(pixels.topLeft()) / ratio, part);
	drawRubberBand(painter);
}

/**
	@brief PaperGraphicsView::drawBackground
	Into the off-screen image while painting inverted, else as
	QGraphicsView.
*/
void PaperGraphicsView::drawBackground(QPainter *painter, const QRectF &rect)
{
	QGraphicsView::drawBackground(scenePainter(painter), rect);
}

/**
	@brief PaperGraphicsView::drawItems
	Into the off-screen image while painting inverted, with the viewport as
	the scene's widget (see SceneAccess), else as QGraphicsView.
*/
void PaperGraphicsView::drawItems(QPainter *painter, int count, QGraphicsItem *items[],
                                  const QStyleOptionGraphicsItem options[])
{
	if (m_inverting && scene())
		(scene()->*SceneAccess::drawItemsPointer())(&m_buffer_painter, count, items, options, viewport());
	else
		QGraphicsView::drawItems(painter, count, items, options);
}

/**
	@brief PaperGraphicsView::drawForeground
	Into the off-screen image while painting inverted, else as
	QGraphicsView.
*/
void PaperGraphicsView::drawForeground(QPainter *painter, const QRectF &rect)
{
	QGraphicsView::drawForeground(scenePainter(painter), rect);
}

/**
	@brief PaperGraphicsView::drawRubberBand
	Draw the selection rubber band the way QGraphicsView::paintEvent() does,
	after the inversion, in the palette colors.
	@param painter a painter on the viewport
*/
void PaperGraphicsView::drawRubberBand(QPainter &painter)
{
	const QRect band = rubberBandRect();
	if (band.isNull())
		return;

	QStyleOptionRubberBand option;
	option.initFrom(viewport());
	option.rect = band;
	option.shape = QRubberBand::Rectangle;

	QStyleHintReturnMask mask;
	if (viewport()->style()->styleHint(QStyle::SH_RubberBand_Mask, &option,
	                                   viewport(), &mask))
		painter.setClipRegion(mask.region, Qt::IntersectClip);
	viewport()->style()->drawControl(QStyle::CE_RubberBand, &option,
	                                 &painter, viewport());
}
