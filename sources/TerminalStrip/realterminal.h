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
#ifndef REALTERMINAL_H
#define REALTERMINAL_H

#include <QSharedPointer>
#include <QDomElement>
#include <QStringList>
#include "../properties/elementdata.h"

class TerminalStrip;
class Element;
class TerminalElement;
class PhysicalTerminal;
class TerminalStripBridge;

/**
 * @brief The RealTerminal class
 * Represent a real terminal.
 * A real terminal can be a drawn terminal in a folio
 * or a terminal set by user but not present
 * on any folio (for example a reserved terminal).
 *
 * When create a new instance of RealTerminal you must
 * call sharedRef() and only use the returned QSharedPointer
 * instead of the raw pointer
 */
class RealTerminal
{
		friend class TerminalElement;
		friend class PhysicalTerminal;

	private:
		RealTerminal(Element *element);

		QSharedPointer<RealTerminal> sharedRef();
		QSharedPointer<RealTerminal> sharedRef() const;
		QWeakPointer<RealTerminal> weakRef();

		void setPhysicalTerminal(const QSharedPointer<PhysicalTerminal> &phy_t);

	public:
		~RealTerminal();
		TerminalStrip *parentStrip() const noexcept;
		QSharedPointer<PhysicalTerminal> physicalTerminal() const noexcept;

		QDomElement toXml(QDomDocument &parent_document) const;

		int level() const;
		QString label() const;
		QString Xref() const;

		/**
			@return the label of every cable one of the conductors of this
			terminal belongs to, once each, in the order the conductors are
			docked.

			Empty for a terminal no cable reaches, which is the common case
			and the honest answer: a project without cables has nothing to
			say here.
		*/
		QStringList cables() const;

		/**
			@return how each conductor of this terminal is identified inside
			its cable - the colour of the wire, its number failing that - one
			entry per conductor that a cable carries, in the same order.

			Colour first and number second because a cable is one or the
			other and not both: a manufacturer either colours the wires or
			numbers them, which is what the column of the strip manager says
			with its own name.
		*/
		QStringList cableWires() const;

		/**
			@return the number of every conductor docked to this terminal, in
			the order they are docked - top to bottom, left to right, as
			Element::conductors() gives them.

			One entry per conductor, including one that carries no number:
			dropping it would be the very thing this answer exists against,
			and a count that does not match the drawing is worse than an
			entry with nothing in it.
		*/
		QStringList conductors() const;

		/// @return cables(), written for a single cell
		QString cable() const;
		/// @return cableWires(), written for a single cell
		QString cableWire() const;
		/// @return conductors(), written for a single cell
		QString conductor() const;

		ElementData::TerminalType type() const;
		ElementData::TerminalFunction function() const;

		bool isLed() const;
		bool isElement() const;
		bool isBridged() const;

		QSharedPointer<TerminalStripBridge> bridge() const;

		Element* element() const;
		QUuid elementUuid() const;

		static QString xmlTagName();

	private :
		QPointer<Element> m_element;
		QWeakPointer<RealTerminal> m_this_weak;
		QSharedPointer<PhysicalTerminal> m_physical_terminal;
};

#endif // REALTERMINAL_H
