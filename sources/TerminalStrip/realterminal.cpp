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
#include "realterminal.h"
#include "terminalstrip.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "physicalterminal.h"
#include "../qetgraphicsitem/conductor.h"
#include "../cable/cable.h"
#include "../diagram.h"
#include "../qetproject.h"

#include <QHash>
#include <QVector>

namespace
{
		/**
			What a cell writes for something that is there and carries no
			name of its own.

			An empty entry is dropped by the eye at the very place the column
			exists to be read: a cell reading "W1, " looks like a stray
			separator, and a terminal whose only conductor has no number
			would read exactly like a terminal with no conductor at all.
			Telling those two apart is the whole point of listing them.
		*/
	QString unnamedEntry()
	{
		return QStringLiteral("?");
	}

		/// How several entries share one cell of the strip manager
	QString entrySeparator()
	{
		return QStringLiteral(", ");
	}

		/// One cell out of @a entries, an unnamed one written rather than dropped
	QString joinedCell(const QStringList &entries)
	{
		QStringList written;
		written.reserve(entries.count());

		for (const QString &entry : entries) {
			written << (entry.isEmpty() ? unnamedEntry() : entry);
		}

		return written.join(entrySeparator());
	}

		/// A wire of a cable, and the cable it is a wire of
	struct CarriedWire
	{
		Cable *cable = nullptr;
		CableWire wire;
	};

	/**
		@brief The cable wires that carry the conductors of @a element.

		@return one entry per conductor of @a element that a wire of a cable
		of the project names, in the order Element::conductors() gives them -
		docked top to bottom and left to right, which does not move between
		runs.

		Matched by uuid and never by the resolved pointer, on purpose: a wire
		read from a file knows its conductor by uuid alone until somebody
		calls Cable::resolve(), and a strip asked before that resolution has
		to answer the same thing as one asked after it. Conductor::fromXml()
		mints a uuid for a conductor that has none, so there is always one to
		match against.

		The index is built once per call rather than the cables being walked
		again for every conductor. Two wires naming the same conductor is a
		project already broken - Cable::resolve() counts it as a duplicate -
		and here the first one answers, which is a choice and not an
		accident.
	*/
	QVector<CarriedWire> carriedWiresOf(const Element *element)
	{
		QVector<CarriedWire> carried;

		if (!element ||
			!element->diagram() ||
			!element->diagram()->project()) {
			return carried;
		}

		const auto cables = element->diagram()->project()->cables();
		if (cables.isEmpty()) {
			return carried;
		}

		QHash<QUuid, CarriedWire> by_conductor;
		for (Cable *cable : cables)
		{
			if (!cable) {
				continue;
			}

			const auto wires = cable->wires();
			for (const CableWire &wire : wires)
			{
				if (wire.conductorUuid().isNull() ||
					by_conductor.contains(wire.conductorUuid())) {
					continue;
				}

				by_conductor.insert(wire.conductorUuid(),
						    CarriedWire{cable, wire});
			}
		}

		const auto conductors = element->conductors();
		for (const Conductor *conductor : conductors)
		{
			const auto uuid = conductor->uuid();
			if (uuid.isNull() ||
				!by_conductor.contains(uuid)) {
				continue;
			}

			carried.append(by_conductor.value(uuid));
		}

		return carried;
	}
}

/**
 * @brief RealTerminal
 * @param parent_strip : parent terminal strip
 * @param terminal : terminal element (if any) in a folio
 */
RealTerminal::RealTerminal(Element *terminal) :
	m_element(terminal)
{}

RealTerminal::~RealTerminal()
{
	if (m_physical_terminal) {
		m_physical_terminal->removeTerminal(sharedRef());
	}
}

/**
 * @brief RealTerminal::sharedRef
 * @return a QSharedPointer of this
 */
QSharedPointer<RealTerminal> RealTerminal::sharedRef()
{
	QSharedPointer<RealTerminal> this_shared(this->weakRef());
	if (this_shared.isNull())
	{
		this_shared = QSharedPointer<RealTerminal>(this);
		m_this_weak = this_shared.toWeakRef();
	}

	return this_shared;
}

/**
 * @brief RealTerminal::sharedRef
 * @return a shared reference of this, not that because
 * this method is const, the shared reference can be null if not already
 * used in another part of the code.
 */
QSharedPointer<RealTerminal> RealTerminal::sharedRef() const {
	return QSharedPointer<RealTerminal>(m_this_weak);
}

/**
 * @brief RealTerminal::weakRef
 * @return a QWeakPointer of this, weak pointer can be bull
 */
QWeakPointer<RealTerminal> RealTerminal::weakRef() {
	return m_this_weak;
}

/**
 * @brief toXml
 * @param parent_document
 * @return this real terminal to xml
 */
QDomElement RealTerminal::toXml(QDomDocument &parent_document) const
{
	auto root_elmt = parent_document.createElement(this->xmlTagName());
	if (m_element)
		root_elmt.setAttribute(QStringLiteral("element_uuid"), m_element->uuid().toString());

	return root_elmt;
}

/**
 * @brief RealTerminal::setPhysicalTerminal
 * Set the parent physical terminal of this real terminal
 * @param phy_t
 */
void RealTerminal::setPhysicalTerminal(const QSharedPointer<PhysicalTerminal> &phy_t) {
	m_physical_terminal = phy_t;
}

/**
* @brief parentStrip
* @return parent terminal strip or nullptr
*/
TerminalStrip *RealTerminal::parentStrip() const noexcept {
	if (m_physical_terminal) {
		return m_physical_terminal->terminalStrip();
	} else {
		return nullptr;
	}
}

/**
 * @brief RealTerminal::physicalTerminal
 * @return The parent physical terminal of this terminal.
 * The returned QSharedPointer can be null
 */
QSharedPointer<PhysicalTerminal> RealTerminal::physicalTerminal() const noexcept{
	return m_physical_terminal;
}

/**
 * @brief RealTerminal::level
 * @return
 */
int RealTerminal::level() const
{
	if (m_physical_terminal &&
		sharedRef()) {
		return m_physical_terminal->levelOf(sharedRef());
	}

	return -1;
}

/**
 * @brief label
 * @return the label of this real terminal
 */
QString RealTerminal::label() const {
	if (!m_element.isNull()) {
		return m_element->actualLabel();
	} else {
		return QLatin1String();
	}
}

/**
 * @brief RealTerminal::Xref
 * @return Convenient method to get the XRef
 * formatted to string
 */
QString RealTerminal::Xref() const
{
	if (!m_element.isNull()) {
		return autonum::AssignVariables::genericXref(m_element.data());
	} else {
		return QString();
	}
}

/**
 * @brief RealTerminal::cables
 * @return the label of every cable reaching this terminal, once each.
 *
 * Once each because a cable that carries two conductors of the same terminal
 * is still one cable, and a column reading "W1, W1" says nothing the column
 * reading "W1" did not already say.
 *
 * A cable nobody named answers the unnamed mark rather than an empty string:
 * the terminal is in a cable either way, and a blank cell would say the
 * opposite.
 */
QStringList RealTerminal::cables() const
{
	QStringList list_;

	const auto carried = carriedWiresOf(m_element.data());
	for (const CarriedWire &entry : carried)
	{
		const auto label = entry.cable->label().isEmpty()
				   ? unnamedEntry()
				   : entry.cable->label();

		if (!list_.contains(label)) {
			list_ << label;
		}
	}

	return list_;
}

/**
 * @brief RealTerminal::cableWires
 * @return how each conductor of this terminal is identified inside its cable.
 *
 * The colour of the wire, its number failing that. A cable is coloured or
 * numbered and not both - there are manufacturers of each kind - which is why
 * the two share one column instead of having one each.
 *
 * A wire with neither answers the unnamed mark, so that the entry keeps its
 * place beside the conductor it belongs to: the entries of this list and the
 * conductors a cable carries are meant to be read side by side, and a dropped
 * entry would shift every one after it.
 */
QStringList RealTerminal::cableWires() const
{
	QStringList list_;

	const auto carried = carriedWiresOf(m_element.data());
	for (const CarriedWire &entry : carried)
	{
		const auto written = entry.wire.color().isEmpty()
				     ? entry.wire.number()
				     : entry.wire.color();

		list_ << (written.isEmpty() ? unnamedEntry() : written);
	}

	return list_;
}

/**
 * @brief RealTerminal::conductors
 * @return the number of every conductor docked to this terminal.
 *
 * Every one of them, and in the order Element::conductors() gives them, which
 * is the order they are docked: top to bottom, left to right. A terminal
 * carrying two wires used to answer for the first one alone, and the second
 * went missing from every list with nothing said - a terminal with one wire
 * and a terminal with two read exactly alike.
 *
 * A conductor with no number is kept as an empty entry rather than left out.
 * The count of this list is the count of the conductors on the drawing, and
 * a caller that shows the entries is free to mark the empty ones; a caller
 * that never saw them could not.
 */
QStringList RealTerminal::conductors() const
{
	QStringList list_;

	if (m_element.isNull()) {
		return list_;
	}

	const auto conductors_ = m_element->conductors();
	for (const Conductor *conductor_ : conductors_) {
		list_ << conductor_->properties().text;
	}

	return list_;
}

/**
 * @brief RealTerminal::cable
 * @return
 */
QString RealTerminal::cable() const {
	return joinedCell(cables());
}

/**
 * @brief RealTerminal::cableWire
 * @return
 */
QString RealTerminal::cableWire() const {
	return joinedCell(cableWires());
}

/**
 * @brief RealTerminal::conductor
 * @return
 */
QString RealTerminal::conductor() const {
	return joinedCell(conductors());
}

/**
 * @brief RealTerminal::type
 * @return
 */
ElementData::TerminalType RealTerminal::type() const {
	if (m_element) {
		return m_element->elementData().terminalType();
	} else {
		return ElementData::TTGeneric;
	}
}

/**
 * @brief RealTerminal::function
 * @return
 */
ElementData::TerminalFunction RealTerminal::function() const {
	if (m_element) {
		return m_element->elementData().terminalFunction();
	} else {
		return ElementData::TFGeneric;
	}
}

/**
 * @brief RealTerminal::isLed
 * @return
 */
bool RealTerminal::isLed() const {
	if (m_element) {
		return m_element->elementData().terminalLed();
	} else {
		return false;
	}
}

/**
 * @brief isElement
 * @return true if this real terminal is linked to a terminal element
 */
bool RealTerminal::isElement() const {
	return m_element.isNull() ? false : true;
}

/**
 * @brief RealTerminal::isBridged
 * @return true if is bridged.
 * @sa TerminalStrip::isBridged
 */
bool RealTerminal::isBridged() const
{
	if (parentStrip()) {
		return !parentStrip()->isBridged(m_this_weak.toStrongRef()).isNull();
	} else {
		return false;
	}
}

/**
 * @brief RealTerminal::bridge
 * @return
 */
QSharedPointer<TerminalStripBridge> RealTerminal::bridge() const
{
	if (parentStrip()) {
		return parentStrip()->isBridged(m_this_weak.toStrongRef());
	} else {
		return QSharedPointer<TerminalStripBridge>();
	}
}

/**
 * @brief element
 * @return the element linked to this real terminal
 * or nullptr if not linked to an Element.
 */
Element *RealTerminal::element() const {
	return m_element.data();
}

/**
 * @brief elementUuid
 * @return if this real terminal is an element
 * in a folio, return the uuid of the element
 * else return a null uuid.
 */
QUuid RealTerminal::elementUuid() const {
	if (!m_element.isNull()) {
		return m_element->uuid();
	} else {
		return QUuid();
	}
}

/**
 * @brief RealTerminal::RealTerminal::xmlTagName
 * @return
 */
QString RealTerminal::RealTerminal::xmlTagName() {
	return QStringLiteral("real_terminal");
}
