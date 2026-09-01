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
#include "diagrameventaddlocationarea.h"

#include "../diagram.h"
#include "../qetgraphicsitem/locationareaitem.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "../undocommand/assignlocationcommand.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QUndoStack>

namespace
{
	/**
		@brief Whether a rectangle was dragged out rather than merely clicked.
		@param rect the rectangle as it stands, item coordinates
		@return true when both sides are long enough to be an area

		The two ways of drawing an area share their first half - a press -
		and the release is what tells them apart. A release that arrives with
		the rectangle still flat is the first press of press, move, press
		again, and the tool has to keep waiting for the second one; a release
		that arrives with a rectangle already in it is the end of a drag, and
		the tool has to finish.

		Half a grid step in each direction is the line between them. It is
		not zero on purpose: the person who means to click rarely holds the
		mouse perfectly still, and with the control key held the position
		does not snap, so a two pixel tremble would otherwise commit an area
		the size of a full stop and open the dialogue over it.

		Both sides are required, not the surface: a rectangle one grid step
		tall and half a folio wide has surface, and is not an enclosure.
	*/
	bool wasDraggedOut(const QRectF &rect)
	{
		const QRectF r{rect.normalized()};
		return r.width()  >= (Diagram::xGrid / 2.0)
		    && r.height() >= (Diagram::yGrid / 2.0);
	}
}

/**
	@brief DiagramEventAddLocationArea::DiagramEventAddLocationArea
	Default constructor
	@param diagram : the diagram where this event must operate
*/
DiagramEventAddLocationArea::DiagramEventAddLocationArea(Diagram *diagram) :
	DiagramEventInterface(diagram)
{
	m_running = true;
	init();
}

/**
	@brief DiagramEventAddLocationArea::~DiagramEventAddLocationArea

	The rectangle being drawn belongs to this event and to nothing else
	until it is committed, and commitArea sets the pointer to nullptr the
	moment ownership passes to the undo command. So a pointer still here at
	destruction time is a rectangle nobody ever accepted, and it goes with
	the tool - which is what has to happen even when the diagram deletes the
	event under us, hence the m_abort test the interface provides for it.
*/
DiagramEventAddLocationArea::~DiagramEventAddLocationArea()
{
	if ((m_running || m_abort) && m_area) {
		discardArea();
	}

	delete m_help_horiz;
	delete m_help_verti;

	if (m_diagram)
	{
		const QList<QGraphicsView *> views{m_diagram->views()};
		for (QGraphicsView *v : views) {
			v->setContextMenuPolicy(Qt::DefaultContextMenu);
		}
	}
}

/**
	@brief DiagramEventAddLocationArea::mousePressEvent
	Action when mouse is pressed
	@param event : event of mouse press
*/
void DiagramEventAddLocationArea::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (Q_UNLIKELY(!m_diagram || m_diagram->isReadOnly())) {
		return;
	}

	if (event->button() == Qt::LeftButton)
	{
		QPointF pos = event->scenePos();
		if (event->modifiers() != Qt::ControlModifier) {
			pos = Diagram::snapToGrid(pos);
		}

			//First press: the first corner. The rectangle goes on the scene
			//straight away so that it can be seen growing, and it is not on
			//the undo stack yet - it is not something the person did until
			//they have said where it ends.
		if (!m_area)
		{
			m_area = new LocationAreaItem(pos, pos);
			m_diagram->addItem(m_area);
			event->setAccepted(true);
			return;
		}

			//Second press: the other corner, for the hand that draws the way
			//the shape tool taught it.
		m_area->setP2(pos);
		commitArea();
		event->setAccepted(true);
		return;
	}

	if (event->button() == Qt::RightButton) {
		event->setAccepted(true);
	}
}

/**
	@brief DiagramEventAddLocationArea::mouseMoveEvent
	Action when mouse move
	@param event : event of mouse move

	The second corner follows the cursor whether or not a button is held,
	which is the one place this tool deliberately parts company with the
	shape tool: that one only follows with no button down, so a press and
	drag leaves the rectangle flat until the button comes up and the area
	appears from nowhere at the end of the movement. Both gestures are
	supported here, so both have to be visible while they happen.
*/
void DiagramEventAddLocationArea::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	updateHelpCross(event->scenePos());

	if (!m_area) {
		return;
	}

	QPointF pos = event->scenePos();
	if (event->modifiers() != Qt::ControlModifier) {
		pos = Diagram::snapToGrid(pos);
	}

	m_area->setP2(pos);
	event->setAccepted(true);
}

/**
	@brief DiagramEventAddLocationArea::mouseReleaseEvent
	Action when mouse button is released
	@param event : event of mouse release
*/
void DiagramEventAddLocationArea::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
	{
			//A release over a rectangle that has surface ends a drag. A
			//release over a flat one is the press of press, move, press
			//again, and there is nothing to do but wait for the second one.
		if (m_area && wasDraggedOut(m_area->rect()))
		{
			commitArea();
			event->setAccepted(true);
		}
		return;
	}

	if (event->button() != Qt::RightButton) {
		return;
	}

		//A rectangle in progress is thrown away, and the tool stays armed:
		//the person changed their mind about this area, not about drawing
		//areas. Only a right click with nothing in progress puts the tool
		//away, which is the same convention the shape tool uses.
	if (m_area)
	{
		discardArea();
		event->setAccepted(true);
		return;
	}

	m_running = false;
	emit finish();
	event->setAccepted(true);
}

/**
	@brief DiagramEventAddLocationArea::init
	Take the context menu away from the views, so that the right click can
	mean discard and put away rather than opening a menu over the folio.
*/
void DiagramEventAddLocationArea::init()
{
	if (!m_diagram) {
		return;
	}

	const QList<QGraphicsView *> views{m_diagram->views()};
	for (QGraphicsView *v : views) {
		v->setContextMenuPolicy(Qt::NoContextMenu);
	}
}

/**
	@brief DiagramEventAddLocationArea::updateHelpCross
	Create and update the position of the cross to help user for draw new area
	@param p : the center of the cross
*/
void DiagramEventAddLocationArea::updateHelpCross(const QPointF &p)
{
	if (!m_diagram) {
		return;
	}

	if (!m_help_horiz || !m_help_verti)
	{
		QPen pen;
		pen.setWidthF(0.4);
		pen.setCosmetic(true);
		pen.setColor(Diagram::background_color == Qt::darkGray ? Qt::lightGray : Qt::darkGray);

		const QRectF rect{m_diagram->border_and_titleblock.insideBorderRect()};

		if (!m_help_horiz)
		{
			m_help_horiz = new QGraphicsLineItem(rect.topLeft().x(), 0, rect.topRight().x(), 0);
			m_help_horiz->setPen(pen);
			m_diagram->addItem(m_help_horiz);
		}

		if (!m_help_verti)
		{
			m_help_verti = new QGraphicsLineItem(0, rect.topLeft().y(), 0, rect.bottomLeft().y());
			m_help_verti->setPen(pen);
			m_diagram->addItem(m_help_verti);
		}
	}

	const QPointF point{Diagram::snapToGrid(p)};

	m_help_horiz->setY(point.y());
	m_help_verti->setX(point.x());
}

/**
	@brief DiagramEventAddLocationArea::discardArea
	Throw away the rectangle being drawn, without touching the undo stack.

	Nothing to undo, because nothing was done: the rectangle was never on the
	stack, and no component was ever assigned from it. That is the reason the
	assignment waits for the end of the gesture rather than following the
	rectangle while it grows - a rectangle that swept over the folio on its
	way to its final size would have written a path onto every component it
	passed, and cancelling would have to remember them all to take it back.
*/
void DiagramEventAddLocationArea::discardArea()
{
	if (!m_area) {
		return;
	}

	if (m_diagram) {
		m_diagram->removeItem(m_area);
	}

	delete m_area;
	m_area = nullptr;
}

/**
	@brief DiagramEventAddLocationArea::commitArea
	@return true when the area was accepted and pushed on the undo stack

	The end of the gesture, and the only place the drawing meets the model.
	Four things happen here in an order that matters.

	The rectangle is straightened first. It is stored as two corners in the
	order they were clicked, so a person who dragged right to left leaves a
	negative width behind, and every reader downstream would have to know
	that. Normalising once here is cheaper than a rule everybody has to
	remember.

	Then the person is asked which location the area stands for. Refusing
	means they did not mean to draw the rectangle at all - the header of
	askForPath says so, and cancelling therefore throws the rectangle away
	rather than leaving an unnamed one on the folio. Choosing to leave it
	unassigned is a different answer, offered inside the dialogue, and it
	commits the area with an empty path.

	The path is written onto the area before the rule is consulted, because
	the rule reads the areas off the folio instead of being told about them.
	An area whose path is still empty at that moment takes part in nothing,
	so asking first would have every component come back unassigned.

	Last, both halves go on the stack as one command: the area appears and
	the components it swallowed change location in a single step, because
	that is one thing the person did. The children are ordered so that undo
	runs backwards through them - the components are released first, then the
	rectangle goes away - since restoring components against a rectangle that
	had already vanished would have nothing to measure them against.

	The tool stays armed either way. An enclosure is rarely alone on a folio.
*/
bool DiagramEventAddLocationArea::commitArea()
{
	if (!m_area) {
		return false;
	}

	if (!m_diagram || m_diagram->isReadOnly())
	{
		discardArea();
		return false;
	}

	m_area->setRect(m_area->rect().normalized());

	QWidget *dialog_parent = m_diagram->views().isEmpty()
				 ? nullptr
				 : m_diagram->views().first();

	bool accepted = false;
	const QString path{LocationAreaItem::askForPath(m_diagram,
							QString(),
							dialog_parent,
							&accepted)};
	if (!accepted)
	{
		discardArea();
		return false;
	}

	m_area->setLocationPath(path);

	auto *undo = new QUndoCommand(tr("Ajouter une zone de localisation"));

	new AddGraphicsObjectCommand(m_area, m_diagram, QPointF(), undo);

	const QList<LocationAssignment> assignments{
		LocationAreaItem::pendingAssignments(
				m_diagram,
				LocationAreaItem::areasOf(m_diagram))};

		//An empty list is no command at all, not an empty one. The rule hands
		//back nothing for a component already carrying the right path, and an
		//area drawn over empty paper legitimately encloses nobody.
	if (!assignments.isEmpty())
	{
		new AssignLocationCommand(assignments,
					  tr("Affecter les composants d'une zone"),
					  undo);
	}

	m_diagram->undoStack().push(undo);

		//Ownership has passed to the command. Letting go of the pointer here
		//is what keeps the destructor from deleting an area that is on the
		//folio and on the stack.
	m_area = nullptr;
	return true;
}
