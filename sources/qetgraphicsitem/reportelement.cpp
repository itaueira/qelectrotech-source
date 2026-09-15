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
#include "reportelement.h"

#include "../diagram.h"
#include "../diagramposition.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetproject.h"
#include "dynamicelementtextitem.h"

ReportElement::ReportElement(const ElementsLocation &location, const QString& link_type,QGraphicsItem *qgi, int *state) :
	Element(location, qgi, state,
			link_type == "next_report"? Element::NextReport : Element::PreviousReport),
	m_inverse_report(link_type == "next_report"? Element::PreviousReport : Element::NextReport)
{}

/**
	@brief ReportElement::~ReportElement
	Destructor
*/
ReportElement::~ReportElement()
{
	unlinkAllElements();
	if (terminals().size())
		disconnect(terminals().first(), nullptr, nullptr, nullptr);
}

/**
	@brief ReportElement::linkToElement
	Link this element to the other element
	@param elmt
	element to be linked with this
*/
void ReportElement::linkToElement(Element * elmt)
{
	if (!diagram() && !elmt -> diagram())
	{
		qDebug() << "ReportElement : linkToElement : Unable to link this or element to link isn't in a diagram";
		return;
	}

		//ensure elmt isn't already linked
	bool i = true;
	if (!this -> isFree() && (connected_elements.first() == elmt))
		i = false;

		//ensure elmt is an inverse report of this element
	if ((elmt->linkType() == m_inverse_report) && i)
	{
		unlinkAllElements();
		connected_elements << elmt;
		elmt->linkToElement(this);
		emit linkedElementChanged();
	}
}

/**
	@brief ReportElement::unLinkAllElements
	Unlink all of the element in the QList connected_elements
*/
void ReportElement::unlinkAllElements()
{
	if (isFree())
		return;

	const QList <Element *> tmp_elmt = connected_elements;

	for (Element *elmt : tmp_elmt)
		connected_elements.removeAll(elmt);

	for(Element *elmt : tmp_elmt)
	{
		elmt -> setHighlighted(false);
		elmt -> unlinkAllElements();
	}
	
	emit linkedElementChanged();
}
/**
	@brief ReportElement::unlinkElement
 *unlink the specified element.
 *for reportelement, they must be only one linked element, so we call
 *unlinkAllElements for clear the connected_elements list.
	@param elmt
*/
void ReportElement::unlinkElement(Element *elmt) {
	Q_UNUSED (elmt);
	unlinkAllElements();
}

/**
	@brief ReportElement::referenced
	See the documentation of the declaration.
	@param name : filled with what was found, when not null
	@return what is on the other side of this reference
*/
ReportElement::Referenced ReportElement::referenced(QString *name) const
{
	if (name) {
		name->clear();
	}

	if (connected_elements.isEmpty()) {
		return NotLinked;
	}

	Element *other_report = connected_elements.first();
	if (!other_report) {
		return NotLinked;
	}

		//What is left when the walk below finds nothing to name: the
		//reference is tied, so a wire does continue over there.
	Referenced answer = Wire;
	QString found;

	const QList<Terminal *> terminals = other_report->terminals();
	for (Terminal *terminal : terminals)
	{
		const QList<Conductor *> conductors = terminal->conductors();
		for (Conductor *conductor : conductors)
		{
			const QString cable = conductor->properties().m_cable;
			if (!cable.isEmpty())
			{
				if (name) {
					*name = cable;
				}
				return Cable;
			}

			Terminal *far_terminal = conductor->terminal1 == terminal
					? conductor->terminal2
					: conductor->terminal1;
			Element *far_element = far_terminal
					? far_terminal->parentElement()
					: nullptr;

			if (!far_element || far_element == other_report) {
				continue;
			}

			const bool is_terminal =
					far_element->linkType() & Element::Terminale;

				//A terminal block already answered, and a component does
				//not take the answer back from it.
			if (!is_terminal && answer == TerminalBlock) {
				continue;
			}

			answer = is_terminal ? TerminalBlock : Component;

			const QString label = far_element->elementInformations()
					.value(QStringLiteral("label")).toString();
			found = label.isEmpty() ? far_element->name() : label;
		}
	}

	if (name) {
		*name = found;
	}

	return answer;
}

/**
	@brief ReportElement::referenceToolTip
	See the documentation of the declaration.
	@return one sentence naming what this reference leads to
*/
QString ReportElement::referenceToolTip() const
{
	QString name;

	switch (referenced(&name))
	{
		case Cable:
			return name.isEmpty()
					? tr("Report vers un câble")
					: tr("Report vers le câble %1").arg(name);
		case TerminalBlock:
			return name.isEmpty()
					? tr("Report vers une borne")
					: tr("Report vers la borne %1").arg(name);
		case Component:
			return name.isEmpty()
					? tr("Report vers un composant")
					: tr("Report vers %1").arg(name);
		case Wire:
			return tr("Report vers un conducteur");
		case NotLinked:
			break;
	}

	return tr("Report non relié");
}

/**
	@brief ReportElement::hoverEnterEvent
	Say what this reference leads to while the mouse is over it.

	Element::hoverEnterEvent writes the name of the symbol into the tooltip
	every time the mouse comes in - every arrow of every folio then says the
	same word and nothing else - so the sentence has to be written after it,
	and here rather than once at load time, because what is on the other side
	changes when a wire is drawn or a link is made.
	@param event
*/
void ReportElement::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	Element::hoverEnterEvent(event);
	setToolTip(referenceToolTip());
}
