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
#include "exchangeelementscommand.h"

#include "../ElementsCollection/terminalmatch.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"

#include <QSet>

namespace
{
		/// A terminal where it is, for matching
	TerminalMatch::Spot spotOf(const Terminal *terminal, const QPointF &point)
	{
		TerminalMatch::Spot spot;
		spot.uuid = terminal->uuid();
		spot.name = terminal->baseName();
		spot.point = point;
		spot.orientation = static_cast<int>(terminal->orientation());
		return spot;
	}

	/**
		Hand the wires of the place of @a leaving over to the terminals of
		@a coming once it is there, into @a terminals.
		Where a terminal of @a coming will be is its point in @a coming,
		taken there by the transform of @a leaving: position, rotation and
		mirrors - both elements have their origin at their hotspot.
	*/
	void matchPlace(const Element *leaving, const Element *coming,
			QHash<Terminal *, Terminal *> &terminals)
	{
		const QList<Terminal *> old_terminals = leaving->terminals();
		const QList<Terminal *> new_terminals = coming->terminals();
		QVector<TerminalMatch::Spot> old_spots;
		QVector<TerminalMatch::Spot> new_spots;
		for (const Terminal *terminal : old_terminals) {
			old_spots << spotOf(terminal, terminal->dockConductor());
		}
		for (const Terminal *terminal : new_terminals) {
			new_spots << spotOf(terminal, leaving->mapToScene(
						    coming->mapFromScene(terminal->dockConductor())));
		}
		const QVector<int> matches = TerminalMatch::matchByPlace(old_spots, new_spots);
		for (int i = 0 ; i < matches.size() ; ++i) {
			if (matches.at(i) >= 0) {
				terminals.insert(old_terminals.at(i), new_terminals.at(matches.at(i)));
			}
		}
	}
}

/**
	@brief ExchangeElementsCommand::placeOf
	@return where @a element is: position, rotation and mirrors
*/
ExchangeElementsCommand::Place ExchangeElementsCommand::placeOf(const Element *element)
{
	Place place;
	place.pos = element->pos();
	place.rotation = element->rotation();
	place.horizontal_mirror = element->hasHorizontalMirror();
	place.vertical_mirror = element->hasVerticalMirror();
	return place;
}

/**
	@brief ExchangeElementsCommand::plan
	Work out where each wire of @a first and @a second would go if they
	exchanged places. Nothing on the folio changes.
	@return the plan; its error says why when nothing can be done
*/
ExchangeElementsCommand::Plan ExchangeElementsCommand::plan(Element *first, Element *second)
{
	Plan plan;
	plan.first = first;
	plan.second = second;

	if (!first || !second || first == second) {
		plan.error = QObject::tr("Select two elements to swap them.");
		return plan;
	}
	if (!first->diagram() || first->diagram() != second->diagram()) {
		plan.error = QObject::tr("The two elements have to be on the same folio.");
		return plan;
	}

	matchPlace(second, first, plan.terminals);
	matchPlace(first, second, plan.terminals);

	QSet<Conductor *> seen;
	for (Element *element : {first, second})
	{
		for (Terminal *terminal : element->terminals())
		{
			for (Conductor *conductor : terminal->conductors())
			{
				if (seen.contains(conductor)) {
					continue;
				}
				seen.insert(conductor);
					//Both ends count: a wire between the two, or a jumper on
					//one of them, moves only when every end of it on them has
					//somewhere to go
				bool docked = true;
				for (Terminal *end : {conductor->terminal1, conductor->terminal2})
				{
					const Element *owner = end->parentElement();
					if ((owner == first || owner == second) && !plan.terminals.contains(end)) {
						docked = false;
					}
				}
				if (docked) {
					plan.moved_conductors << conductor;
				} else {
					plan.lost_conductors << conductor;
				}
			}
		}
	}
	return plan;
}

/**
	@brief ExchangeElementsCommand::ExchangeElementsCommand
	@param plan : a plan without error, from plan()
	@param parent
*/
ExchangeElementsCommand::ExchangeElementsCommand(const Plan &plan, QUndoCommand *parent) :
	QUndoCommand(parent),
	m_diagram(plan.first->diagram()),
	m_first(plan.first),
	m_second(plan.second),
	m_first_place(placeOf(plan.first)),
	m_second_place(placeOf(plan.second)),
	m_forward(plan.terminals),
	m_moved(plan.moved_conductors),
	m_lost(plan.lost_conductors)
{
	for (auto it = m_forward.cbegin() ; it != m_forward.cend() ; ++it) {
		m_backward.insert(it.value(), it.key());
	}

	auto name = [](Element *element) {
		return element->actualLabel().isEmpty() ? element->name() : element->actualLabel();
	};
	setText(QObject::tr("swap %1 and %2", "undo caption - the names of the two elements")
		.arg(name(m_first), name(m_second)));

	QVector<QGraphicsItem *> items;
	for (Conductor *conductor : std::as_const(m_lost)) {
		items << conductor;
	}
	m_diagram->qgiManager().manage(items);
}

/**
	@brief ExchangeElementsCommand::~ExchangeElementsCommand
	The wires that had nowhere to go are deleted with the last command that
	can bring them back.
*/
ExchangeElementsCommand::~ExchangeElementsCommand()
{
	QVector<QGraphicsItem *> items;
	for (Conductor *conductor : std::as_const(m_lost)) {
		items << conductor;
	}
	m_diagram->qgiManager().release(items);
}

/**
	@brief ExchangeElementsCommand::redo
*/
void ExchangeElementsCommand::redo()
{
	exchange(true);
	QUndoCommand::redo();
}

/**
	@brief ExchangeElementsCommand::undo
*/
void ExchangeElementsCommand::undo()
{
	exchange(false);
	QUndoCommand::undo();
}

/**
	@brief ExchangeElementsCommand::setPlace
	Put @a element at @a place, through QGraphicsObject: QetGraphicsItem::setPos()
	snaps to the grid, and the place of the other element is already where it
	has to be.
*/
void ExchangeElementsCommand::setPlace(Element *element, const Place &place)
{
	element->QGraphicsObject::setPos(place.pos);
	element->setRotation(place.rotation);
	element->setHorizontalMirror(place.horizontal_mirror);
	element->setVerticalMirror(place.vertical_mirror);
}

/**
	@brief ExchangeElementsCommand::exchange
	@param forward : true to put each element in the place of the other,
	false to put them back
*/
void ExchangeElementsCommand::exchange(bool forward)
{
	m_diagram->showMe();

		//Off the folio first: that undocks each wire from its terminals and
		//from the project database, before its ends change
	QList<Conductor *> back_on;
	for (Conductor *conductor : std::as_const(m_moved))
	{
		if (conductor->scene() == m_diagram) {
			m_diagram->removeItem(conductor);
			back_on << conductor;
		}
	}
	if (forward)
	{
		m_lost_off.clear();
		for (Conductor *conductor : std::as_const(m_lost))
		{
			if (conductor->scene() == m_diagram) {
				m_diagram->removeItem(conductor);
				m_lost_off << conductor;
			}
		}
	}

	setPlace(m_first, forward ? m_second_place : m_first_place);
	setPlace(m_second, forward ? m_first_place : m_second_place);

	const QHash<Terminal *, Terminal *> &terminals = forward ? m_forward : m_backward;
	for (Conductor *conductor : std::as_const(m_moved))
	{
		Terminal *end1 = conductor->terminal1;
		Terminal *end2 = conductor->terminal2;
		conductor->dockAt(terminals.value(end1, end1), terminals.value(end2, end2));
	}
	for (Conductor *conductor : std::as_const(back_on))
	{
		m_diagram->addItem(conductor);
		conductor->updatePath();
	}
	if (!forward)
	{
		for (Conductor *conductor : std::as_const(m_lost_off)) {
			m_diagram->addItem(conductor);
		}
	}
}
