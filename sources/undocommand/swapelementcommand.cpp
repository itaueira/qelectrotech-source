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
#include "swapelementcommand.h"

#include <QDomDocument>
#include <QObject>
#include <QSet>
#include <QVector>

#include "../ElementsCollection/elementslocation.h"
#include "../ElementsCollection/terminalmatch.h"
#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../factory/elementfactory.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "../qetproject.h"
#include "../qgimanager.h"

namespace
{
	TerminalMatch::Spot spotOf(const Terminal *terminal)
	{
		TerminalMatch::Spot spot;
		spot.uuid = terminal->uuid();
			//The symbol's own name: a name given on the folio is carried
			//over to the match, it does not choose it
		spot.name = terminal->baseName();
		spot.point = terminal->dockConductor();
		spot.orientation = static_cast<int>(terminal->orientation());
		return spot;
	}
}

/**
	@brief SwapElementCommand::isSwappable
	@param element
	@return false for a terminal held by a terminal strip. The strip keeps
	the element itself, not its uuid, and handing it over to another one is
	a job of its own.
*/
bool SwapElementCommand::isSwappable(Element *element)
{
	if (auto *terminal_element = dynamic_cast<TerminalElement *>(element)) {
		return terminal_element->parentTerminalStrip() == nullptr;
	}
	return element != nullptr;
}

/**
	@brief SwapElementCommand::plan
	Build the element that would replace @a old_element, and work out where
	each wire would go. Nothing on the folio changes: the new element is not
	added anywhere, and is deleted by discard() when the plan is not used.

	The new element is given the uuid, the position, the rotation and the
	mirrors of the old one here, which is what matching the terminals needs.
	The rest - frozen label, numbering, texts, information - is read from
	@a old_element as it is saved, the way opening a project reads an
	element, and like opening a project it is read once the element is on
	its folio (the first redo): a label made by a formula is worked out from
	the folio, and off it the formula itself would become the label. Two
	things are left out of that reading: the terminals, which opening would
	match by position alone, and the links, which opening would make by uuid
	while the old element still holds them.
	@param old_element : an element on a folio
	@param project_location : the replacing symbol, already in the project
	(QETProject::importElement())
	@return the plan; its error says why when nothing can be done
*/
SwapElementCommand::Plan SwapElementCommand::plan(
		Element *old_element,
		const ElementsLocation &project_location)
{
	Plan plan;
	plan.old_element = old_element;

	if (!old_element || !old_element->diagram()) {
		plan.error = QObject::tr("L'élément n'est sur aucun folio.");
		return plan;
	}
	if (!isSwappable(old_element)) {
		plan.error = QObject::tr("« %1 » fait partie d'un bornier : "
					 "retirez-le du bornier pour le remplacer.")
				.arg(old_element->actualLabel().isEmpty()
				     ? old_element->name()
				     : old_element->actualLabel());
		return plan;
	}

	int state = 0;
	Element *new_element = ElementFactory::Instance()->createElement(
				project_location, nullptr, &state);
	if (state || !new_element) {
		delete new_element;
		plan.error = QObject::tr("L'élément choisi n'a pas pu être chargé.");
		return plan;
	}

		//Where the old one is, the way Element::fromXml() places an element:
		//through QGraphicsObject, since QetGraphicsItem::setPos() snaps to
		//the grid
	new_element->setUuid(old_element->uuid());
	new_element->QGraphicsObject::setPos(old_element->pos());
	new_element->setRotation(old_element->rotation());
	new_element->setHorizontalMirror(old_element->hasHorizontalMirror());
	new_element->setVerticalMirror(old_element->hasVerticalMirror());

	QDomDocument &document = plan.state;
	QHash<Terminal *, int> terminal_ids;
	QDomElement xml = old_element->toXml(document, terminal_ids);
	document.appendChild(xml);
	for (const QString &tag : {QStringLiteral("terminals"),
				   QStringLiteral("links_uuids")})
	{
		const QDomElement child = xml.firstChildElement(tag);
		if (!child.isNull()) {
			xml.removeChild(child);
		}
	}

		//What the folio says wins; what it leaves empty is taken from the
		//new symbol, which may say who makes it or what it is
	const QString infos_tag = QStringLiteral("elementInformations");
	const QString info_tag = QStringLiteral("elementInformation");
	DiagramContext informations;
	informations.fromXml(xml.firstChildElement(infos_tag), info_tag);
	const DiagramContext defaults = new_element->elementInformations();
	for (const QString &key : defaults.keys())
	{
		if (informations.value(key).toString().isEmpty()
		    && !defaults.value(key).toString().isEmpty()) {
			informations.addValue(key, defaults.value(key),
					      defaults.keyMustShow(key));
		}
	}
	const QDomElement old_infos = xml.firstChildElement(infos_tag);
	if (!old_infos.isNull()) {
		xml.removeChild(old_infos);
	}
	QDomElement new_infos = document.createElement(infos_tag);
	informations.toXml(new_infos, info_tag);
	xml.appendChild(new_infos);

	plan.new_element = new_element;

		//Where each wire goes
	const QList<Terminal *> old_terminals = old_element->terminals();
	const QList<Terminal *> new_terminals = new_element->terminals();
	QVector<TerminalMatch::Spot> old_spots;
	QVector<TerminalMatch::Spot> new_spots;
	for (const Terminal *terminal : old_terminals) {
		old_spots << spotOf(terminal);
	}
	for (const Terminal *terminal : new_terminals) {
		new_spots << spotOf(terminal);
	}
	const QVector<int> matches = TerminalMatch::match(old_spots, new_spots);
	for (int i = 0 ; i < matches.size() ; ++i)
	{
		if (matches.at(i) < 0) {
			continue;
		}
		Terminal *old_terminal = old_terminals.at(i);
		Terminal *new_terminal = new_terminals.at(matches.at(i));
		plan.terminals.insert(old_terminal, new_terminal);
		new_terminal->setInstanceName(old_terminal->instanceName());
	}

	QSet<Conductor *> seen;
	for (Terminal *terminal : old_terminals)
	{
		for (Conductor *conductor : terminal->conductors())
		{
			if (seen.contains(conductor)) {
				continue;
			}
			seen.insert(conductor);

				//Both ends count: a jumper between two terminals of the
				//element moves only when both have somewhere to go
			bool docked = true;
			for (Terminal *end : {conductor->terminal1, conductor->terminal2})
			{
				if (end->parentElement() == old_element
				    && !plan.terminals.contains(end)) {
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

		//The link rules live in the element classes and need a folio to
		//run; what they will say is known from the link type alone
	const QList<Element *> links = old_element->linkedElements();
	if (!links.isEmpty() && new_element->linkType() != old_element->linkType()) {
		plan.lost_links = links;
	}

	return plan;
}

/**
	@brief SwapElementCommand::discard
	Delete the element @a plan built, when the plan is not going to be
	carried out.
*/
void SwapElementCommand::discard(Plan &plan)
{
	if (plan.new_element && !plan.new_element->scene()) {
		delete plan.new_element;
	}
	plan.new_element = nullptr;
}

/**
	@brief SwapElementCommand::SwapElementCommand
	@param plan : a plan without error, from plan()
	@param parent
*/
SwapElementCommand::SwapElementCommand(const Plan &plan,
				       QUndoCommand *parent) :
	QUndoCommand(parent),
	m_diagram(plan.old_element->diagram()),
	m_old(plan.old_element),
	m_new(plan.new_element),
	m_state(plan.state),
	m_terminals(plan.terminals),
	m_moved(plan.moved_conductors),
	m_lost(plan.lost_conductors)
{
	m_links = m_old->linkedElements();
	for (Element *other : std::as_const(m_links))
	{
		m_link_informations.insert(other, other->elementInformations());
		m_group_index_here.insert(other, m_old->groupIndexForElement(other));
		m_group_index_there.insert(other, other->groupIndexForElement(m_old));
	}

	setText(QObject::tr("remplacer %1 par %2",
			    "undo caption - the names of the two elements")
		.arg(m_old->name(), m_new->name()));

	QVector<QGraphicsItem *> items {m_old, m_new};
	for (Conductor *conductor : std::as_const(m_lost)) {
		items << conductor;
	}
	m_diagram->qgiManager().manage(items);
}

/**
	@brief SwapElementCommand::~SwapElementCommand
	The element off the folio, and the wires that went with it, are deleted
	with the last command that can bring them back.
*/
SwapElementCommand::~SwapElementCommand()
{
	QVector<QGraphicsItem *> items {m_old, m_new};
	for (Conductor *conductor : std::as_const(m_lost)) {
		items << conductor;
	}
	m_diagram->qgiManager().release(items);
}

/**
	@brief SwapElementCommand::redo
*/
void SwapElementCommand::redo()
{
	swap(m_old, m_new, true);
	QUndoCommand::redo();
}

/**
	@brief SwapElementCommand::undo
*/
void SwapElementCommand::undo()
{
	swap(m_new, m_old, false);
	QUndoCommand::undo();
}

/**
	@brief SwapElementCommand::swap
	Take @a from off the folio and put @a to in its place, with the wires.
	@param forward : true from the old element to the new one, false back
*/
void SwapElementCommand::swap(Element *from, Element *to, bool forward)
{
	m_diagram->showMe();
	const bool selected = from->isSelected();

		//Off the folio first: that undocks each wire from its terminals and
		//from the project database, before its ends change. A wire already
		//off is left off - another replacement of the same command may have
		//taken it away because it had nowhere to go on its side.
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

		//Taking an element off also unlinks it (Diagram::removeItem())
	m_diagram->removeItem(from);
	m_diagram->addItem(to);

		//The project database took the element as it entered the folio;
		//whatever changes what it says after that is told to it below, as
		//the commands that change information do
	QList<Element *> changed;
	if (forward && !m_state_read)
	{
		QDomElement state = m_state.documentElement();
		QHash<int, Terminal *> no_terminals;
		to->fromXml(state, no_terminals);
		m_state_read = true;
		changed << to;
	}

	for (Conductor *conductor : std::as_const(m_moved))
	{
		for (Terminal *end : {conductor->terminal1, conductor->terminal2})
		{
			if (end->parentElement() != from) {
				continue;
			}
			conductor->replaceTerminal(end, forward
						   ? m_terminals.value(end)
						   : m_terminals.key(end));
		}
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

		//Unlinking may have changed what the linked elements say - a PLC
		//master clears the information of its slaves - so what they said
		//before is put back, along with the contact group of each link
	for (Element *other : std::as_const(m_links))
	{
		to->linkToElement(other);
		if (!to->linkedElements().contains(other)) {
			continue;
		}
		const int here = m_group_index_here.value(other, -1);
		if (here >= 0) {
			to->setGroupIndexForElement(other, here);
		}
		const int there = m_group_index_there.value(other, -1);
		if (there >= 0) {
			other->setGroupIndexForElement(to, there);
		}
		const DiagramContext informations = m_link_informations.value(other);
		if (other->elementInformations() != informations) {
			other->setElementInformations(informations);
			changed << other;
		}
	}

	if (!changed.isEmpty() && m_diagram->project()) {
		m_diagram->project()->dataBase()->elementInfoChanged(changed);
	}

	to->setSelected(selected);
}
