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
#include "mountingview.h"

#include "mountingscene.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QWheelEvent>

#include <cmath>

/**
	@brief MountingView::MountingView
	@param scene the surface to look at
	@param parent parent widget
*/
MountingView::MountingView(MountingScene *scene, QWidget *parent) :
	QGraphicsView(scene, parent),
	m_scene(scene)
{
	setInteractive(true);
	setMouseTracking(true);
	setRenderHint(QPainter::Antialiasing, true);
	setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	setResizeAnchor(QGraphicsView::AnchorUnderMouse);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

		//Pressing an item drags it, pressing the plate draws a
		//selection rectangle - which works only because the plate is
		//painted as background and is not an item. It is the same
		//gesture the folio and the element editor answer to, and none
		//of it had to be written.
	setDragMode(QGraphicsView::RubberBandDrag);

	zoomActualSize();
}

MountingView::~MountingView()
{}

/**
	@brief MountingView::scene
	@return the surface being looked at
*/
MountingScene *MountingView::scene() const
{
	return m_scene;
}

/**
	@brief MountingView::pixelsPerMillimetre
	@return how many pixels one millimetre is drawn as at this instant
*/
qreal MountingView::pixelsPerMillimetre() const
{
	return transform().m11();
}

/**
	@brief MountingView::zoomIn
	A third bigger, which is the step the two other editors take.
*/
void MountingView::zoomIn()
{
	scaleClamped(4.0 / 3.0);
}

/**
	@brief MountingView::zoomOut
	A quarter smaller, the inverse of the step above.
*/
void MountingView::zoomOut()
{
	scaleClamped(0.75);
}

/**
	@brief MountingView::zoomInSlowly
	The step of a trackpad, which reports many small ones.
*/
void MountingView::zoomInSlowly()
{
	scaleClamped(1.02);
}

/**
	@brief MountingView::zoomOutSlowly
*/
void MountingView::zoomOutSlowly()
{
	scaleClamped(0.98);
}

/**
	@brief MountingView::zoomFit
	Fit the plate and everything on it in the window.

	The rectangle asked for is the one the scene keeps, which takes in the
	parts that do not fit on the plate as well as the plate. That is
	deliberate over there and it matters here: an item sitting off the edge
	is the report, and a zoom to fit that framed only the plate would hide
	exactly what the person has to see.
*/
void MountingView::zoomFit()
{
	if (!m_scene) {
		return;
	}

	fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
	clampFit();
	emit zoomChanged(pixelsPerMillimetre());
}

/**
	@brief MountingView::zoomActualSize
	One pixel per millimetre.

	Not an arbitrary starting point: a scene unit is a millimetre, so this
	is the transform at which a plate of 600 by 800 is drawn 600 by 800
	pixels - about the size of the window it is being laid out in, and the
	scale at which a person recognises the panel.
*/
void MountingView::zoomActualSize()
{
	resetTransform();
	emit zoomChanged(pixelsPerMillimetre());
}

/**
	@brief MountingView::wheelEvent
	@param event

	The same two behaviours the element editor has, chosen by the same
	setting: with gestures on, the wheel scrolls and control plus wheel
	zooms slowly, because the wheel is then a trackpad reporting many small
	steps; with gestures off, the wheel zooms.
*/
void MountingView::wheelEvent(QWheelEvent *event)
{
	if (gestures())
	{
		if (event->modifiers() & Qt::ControlModifier) {
			event->angleDelta().y() > 0 ? zoomInSlowly()
						    : zoomOutSlowly();
		}
		else {
			QGraphicsView::wheelEvent(event);
		}
	}
	else {
		event->angleDelta().y() > 0 ? zoomIn() : zoomOut();
	}
}

/**
	@brief MountingView::mouseMoveEvent
	@param event

	The event is handed on first and reported afterwards: what is above
	this is a drag in progress, and a view that swallowed the move to read
	a coordinate would be a view nothing can be dragged on.
*/
void MountingView::mouseMoveEvent(QMouseEvent *event)
{
	QGraphicsView::mouseMoveEvent(event);

		//event->pos() and not event->position(): the second one is
		//Qt 6 only, and this file has to compile against both. It is
		//also what every other view of the program reads a mouse
		//position with.
	const QPointF scene_position = mapToScene(event->pos());

	emit pointed(MountingScene::millimetreFromScene(scene_position));
}

/**
	@brief MountingView::gestures
	@return true when the wheel is to be read as a trackpad

	Read from the settings of the folio view, and on purpose: it is one
	machine and one pointing device, so the person sets it once and every
	view of the program obeys it.
*/
bool MountingView::gestures() const
{
	QSettings settings;
	return settings.value("diagramview/gestures", false).toBool();
}

/**
	@brief MountingView::scaleClamped
	@param factor how much bigger to draw everything

	Refused rather than clamped when it would leave the range: a wheel
	notch that does nothing at the end of the range is what every zoom does,
	while a notch that silently jumps to the bound would move the drawing
	under the pointer for no reason the person gave.
*/
void MountingView::scaleClamped(qreal factor)
{
	const qreal target = pixelsPerMillimetre() * factor;
	if (target < m_min_zoom || target > m_max_zoom) {
		return;
	}

	scale(factor, factor);
	emit zoomChanged(pixelsPerMillimetre());
}

/**
	@brief MountingView::clampFit
	Keep a fit drawable, at the one end where it can run away.

	The upper end is kept: a face of two millimetres fitted into a window of
	a thousand pixels asks for a transform of five hundred, and growing is
	the direction a QGraphicsView transform overflows in.

	The lower end is deliberately not kept, and it took a test case to find
	out why it must not be. A minimum zoom is a rule about the wheel: a step
	that can be repeated for as long as somebody keeps turning has to stop
	somewhere. A fit is not a step - it is one absolute scale computed from
	a finite rectangle and a finite viewport, and no amount of asking for it
	again makes it smaller. Holding it up to the floor of the wheel makes a
	large face not fit, which is the single thing this command exists to do:
	the first version of this file did exactly that, and the plate came back
	cropped by a third with the scale sitting on the floor.

	What is refused here is only an answer that is not a number, and the
	fallback for it is one pixel per millimetre - the scale this view is
	built around rather than an arbitrary rescue value.
*/
void MountingView::clampFit()
{
	const qreal current = pixelsPerMillimetre();

	if (!std::isfinite(current) || current <= 0.0)
	{
		resetTransform();
		return;
	}

	if (current > m_max_zoom)
	{
		resetTransform();
		scale(m_max_zoom, m_max_zoom);
	}
}
