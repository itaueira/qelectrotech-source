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
#ifndef TERMINALSTRIPBRIDGE_H
#define TERMINALSTRIPBRIDGE_H

#include <QSharedPointer>
#include <QUuid>
#include <QPointer>
#include <QColor>
#include <QDomDocument>
#include <QVector>

class RealTerminal;
class TerminalStrip;

class TerminalStripBridge
{
		friend class TerminalStrip;

	public:
		static QVector<QColor> bridgeColor() {return QVector<QColor>{Qt::red, Qt::blue, Qt::white, Qt::darkGray, Qt::black};}

		TerminalStripBridge(TerminalStrip *parent_strip = nullptr);
		QSharedPointer<TerminalStripBridge> sharedRef();
		QWeakPointer<TerminalStripBridge> weakRef();

		QColor color() const;
		void setColor(const QColor &color);
		QVector<QSharedPointer<RealTerminal>> realTerminals() const;

			/// The wire that feeds the bridge, by its uuid: one of the
			/// wires on its terminals, which the person marks (T87)
		QUuid feed() const noexcept {return m_feed;}
		void setFeed(const QUuid &conductor_uuid);
			/// The terminal of its components the bridge is drawn by on
			/// a folio - the index in Element::terminals(), the side of the
			/// strip -, -1 when none was chosen (T87)
		int side() const noexcept {return m_side;}
		void setSide(int side);

		static QString xmlTagName() {return QStringLiteral("terminal_strip_bridge");}
		QDomElement toXml(QDomDocument &parent_document) const;
		void fromXml(const QDomElement &dom_element);
		QUuid uuid() const noexcept;

	private:
		bool addTerminals(const QVector<QSharedPointer<RealTerminal>> &real_terminals);
		void removeTerminals(const QVector<QSharedPointer<RealTerminal>> &real_terminals);
		void removeTerminal(const QSharedPointer<RealTerminal> &real_terminal);


	private:
		QPointer<TerminalStrip> m_strip;
		QVector<QSharedPointer<RealTerminal>> m_real_terminals;
		QColor m_color = Qt::darkGray;
		QUuid m_uuid = QUuid::createUuid();
		QUuid m_feed;
		int m_side = -1;
		QWeakPointer<TerminalStripBridge> m_this_weak;
};



#endif // TERMINALSTRIPBRIDGE_H
