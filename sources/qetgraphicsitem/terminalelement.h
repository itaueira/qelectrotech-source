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
#ifndef TERMINALELEMENT_H
#define TERMINALELEMENT_H


#include "element.h"
#include "../TerminalStrip/terminalstrip.h"

class QETProject;
class RealTerminal;
/**
	@brief The TerminalElement class
*/
class TerminalElement : public Element
{
		Q_OBJECT
	public:
		TerminalElement(const ElementsLocation &,
				QGraphicsItem * = nullptr, int * = nullptr);
		~TerminalElement() override;
		void initLink(QETProject *project) override;

			//A terminal whose definition says it can be the slave of a strip
			//terminal links to one such master; any other terminal does not
			//link at all, as before
		void linkToElement(Element *elmt) override;
		void unlinkAllElements() override;
		void unlinkElement(Element *elmt) override;

			/// @return the strip terminal this terminal is linked to, or
			/// nullptr when it is linked to none
		Element *stripMaster() const;
			/// @return true when the label of this terminal is the one of a
			/// strip terminal, and so is edited there and not here
		bool labelComesFromStrip() const;

		QSharedPointer<RealTerminal> realTerminal() const;
		void setParentTerminalStrip(TerminalStrip *strip);
		TerminalStrip *parentTerminalStrip() const;

	private:
		void followStripMaster();

		QSharedPointer<RealTerminal> m_real_terminal;
		QMetaObject::Connection m_master_info_connection;
		bool m_following_strip = false;
};

#endif // TERMINALELEMENT_H
