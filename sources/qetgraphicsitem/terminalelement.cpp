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
#include "terminalelement.h"
#include"../TerminalStrip/realterminal.h"
#include "../qetinformation.h"
#include "../diagram.h"
#include "../qetproject.h"
#include "../dataBase/projectdatabase.h"

#include <QScopedValueRollback>

/**
	@brief TerminalElement::TerminalElement
	Default constructor
	@param location location of xml definition
	@param qgi parent QGraphicItem
	@param state int used to know if the creation of element have error
*/
TerminalElement::TerminalElement(const ElementsLocation &location,
				 QGraphicsItem *qgi, int *state) :
	Element(location, qgi, state, Element::Terminale)
{
	auto rt = new RealTerminal(this);
	m_real_terminal = rt->sharedRef();

		//Whatever writes the label of a linked terminal - its properties,
		//the text edited on the folio, a script - the number goes back to
		//the one of its strip terminal: the two sides never disagree
	connect(this, &Element::elementInfoChange, this, &TerminalElement::followStripMaster);
}

TerminalElement::~TerminalElement()
{}

/**
	@brief TerminalElement::initLink
	@param project
*/
void TerminalElement::initLink(QETProject *project) {
	Element::initLink(project);
}

/**
	@brief TerminalElement::linkToElement
	Link this terminal to a strip terminal, the terminal drawn on the
	terminal strip sheet. Only a terminal whose definition gives it the
	slave role does, and only to a master of the strip terminal kind; it
	stays a terminal in everything else, and keeps its place in its strip.
	@param elmt
*/
void TerminalElement::linkToElement(Element *elmt)
{
	if (!elmt
	    || !m_data.isStripTerminalSlave()
	    || !elmt->elementData().isStripTerminalMaster()
	    || connected_elements.contains(elmt))
		return;

	if (!isFree())
		unlinkAllElements();

	connected_elements << elmt;
	elmt->linkToElement(this);

		//The number is typed once, on the strip terminal, and this one
		//receives it. As data, and not only on the screen: the strip and the
		//wiring list read the label of this terminal itself, and not what is
		//drawn of it. This runs on opening a project too, when the links
		//are made again - so the strip terminal wins over a label that was
		//changed outside this program.
	m_master_info_connection = connect(elmt, &Element::elementInfoChange,
					   this, &TerminalElement::followStripMaster);
	followStripMaster();
	emit linkedElementChanged();
}

/**
	@brief TerminalElement::unlinkAllElements
*/
void TerminalElement::unlinkAllElements()
{
	if (isFree())
		return;

	const auto linked = connected_elements;
	for (Element *elmt : linked)
		unlinkElement(elmt);
	emit linkedElementChanged();
}

/**
	@brief TerminalElement::unlinkElement
	@param elmt
*/
void TerminalElement::unlinkElement(Element *elmt)
{
	if (!connected_elements.contains(elmt))
		return;

	connected_elements.removeOne(elmt);
		//Unlinked, the terminal keeps the number it received: it has become
		//its own. Undoing the link is what gives the former one back, see
		//LinkElementCommand. Not listening to the former master changes no
		//behaviour - followStripMaster() asks who the master is first - but
		//without it every link made again would add one more connection.
	disconnect(m_master_info_connection);
	elmt->unlinkElement(this);
	elmt->setHighlighted(false);
	emit linkedElementChanged();
}

Element *TerminalElement::stripMaster() const
{
	if (!m_data.isStripTerminalSlave()) {
		return nullptr;
	}
	for (Element *elmt : connected_elements) {
		if (elmt && elmt->elementData().isStripTerminalMaster()) {
			return elmt;
		}
	}
	return nullptr;
}

bool TerminalElement::labelComesFromStrip() const
{
	return stripMaster() != nullptr;
}

/**
	@brief TerminalElement::followStripMaster
	Write the label of the strip terminal into this one. A formula goes
	too: it would compute this label again, over the one received.
	Nothing is written when there is nothing to change, which is what
	stops the call this one triggers through elementInfoChange. And that
	call is refused while this one runs: anything that left the label
	different after writing it would otherwise loop until the stack ran
	out - measured, by keeping the formula: the bench crashed.
*/
void TerminalElement::followStripMaster()
{
	Element *master = stripMaster();
	if (!master || m_following_strip) {
		return;
	}
	QScopedValueRollback<bool> following(m_following_strip, true);
	const QString number = master->actualLabel();
	DiagramContext information = elementInformations();
	if (information.value(QETInformation::ELMT_LABEL).toString() == number
	    && information.value(QETInformation::ELMT_FORMULA).toString().isEmpty()) {
		return;
	}
	information.remove(QETInformation::ELMT_FORMULA);
	information.addValue(QETInformation::ELMT_LABEL, number);
	setElementInformations(information);
		//The parts list, the lists and the labels read the data base of the
		//project, which a command tells of what it changed. This change is
		//made by no command of this terminal, and nobody else would tell it.
	if (diagram() && diagram()->project() && diagram()->project()->dataBase())
		diagram()->project()->dataBase()->elementInfoChanged(this);
}

/**
 * @brief TerminalElement::realTerminal
 * @return the real terminal of this terminal element.
 */
QSharedPointer<RealTerminal> TerminalElement::realTerminal() const {
	return m_real_terminal;
}

/**
 * @brief TerminalElement::parentTerminalStrip
 * @return The parent terminal strip of this
 * terminal element or nullptr if not.
 */
TerminalStrip *TerminalElement::parentTerminalStrip() const {
	if (m_real_terminal) {
		return m_real_terminal->parentStrip();
	}
	return nullptr;
}

