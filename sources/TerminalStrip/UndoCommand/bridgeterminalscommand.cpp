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
#include "bridgeterminalscommand.h"
#include "../physicalterminal.h"
#include "../realterminal.h"
#include "../terminalstripbridge.h"

#include <algorithm>

BridgeTerminalsCommand::BridgeTerminalsCommand(TerminalStrip *strip,
											   QVector<QSharedPointer<RealTerminal>> real_terminal,
											   QUndoCommand *parent):
	QUndoCommand(parent),
	m_strip(strip),
	m_real_terminal_vector(real_terminal)
{
	setText(QObject::tr("Bridging terminals together"));
}

/**
 * @brief BridgeTerminalsCommand::undo
 * Unbridge the terminals this command bridged, and only them.
 *
 * The terminals that were already bridged before the command belong to
 * a bridge that was there before, and unbridging them too would leave
 * the strip with less than it had: a bridge made by hand and then
 * extended would be gone after one undo.
 *
 * TerminalStrip::unBridge only lets go of terminals at one end of a
 * bridge, so the added terminals are given back block by block: one
 * block before the bridge, one after it, or both.
 *
 * A bridge with a gap in it, as a strip reordered after the bridge was
 * made can leave it, may be filled by this command but cannot be given
 * back as it was: the strip neither unbridges a terminal from the middle
 * of a bridge nor makes a bridge with a gap. Then undo does what it
 * always did, and unbridges every terminal the command was handed.
 */
void BridgeTerminalsCommand::undo()
{
	if (!m_strip || !m_bridge) {
		return;
	}

	const auto blocks = addedBlocks();
	if (blocks.isEmpty()) {
		return;
	}

	for (const auto &block : blocks)
	{
		if (!m_strip->canUnBridge(block))
		{
			m_strip->unBridge(m_real_terminal_vector);
			return;
		}
	}

	for (const auto &block : blocks) {
		m_strip->unBridge(block);
	}
}

/**
 * @brief BridgeTerminalsCommand::redo
 * The first redo bridges the terminals and notes which of them were not
 * bridged yet: those are the ones undo gives back. It is noted here and
 * not in the constructor because this is the state the command acts on;
 * a command built as the child of another is built before its elder
 * siblings have run.
 * Each later redo gives back to the same bridge the terminals undo took
 * away, so that the bridge keeps its color.
 */
void BridgeTerminalsCommand::redo()
{
	if (!m_strip) {
		return;
	}

	if (m_bridge)
	{
		QVector<QSharedPointer<RealTerminal>> missing;
		for (const auto &real_t : std::as_const(m_real_terminal_vector)) {
			if (m_strip->isBridged(real_t) != m_bridge) {
				missing.append(real_t);
			}
		}
		m_strip->setBridge(m_bridge, missing);
	}
	else
	{
		QVector<QSharedPointer<RealTerminal>> added;
		for (const auto &real_t : std::as_const(m_real_terminal_vector)) {
			if (m_strip->isBridged(real_t).isNull()) {
				added.append(real_t);
			}
		}

		if (m_strip->setBridge(m_real_terminal_vector))
		{
			m_bridge = m_strip->isBridged(m_real_terminal_vector.first());
			m_added_terminals = added;
		}
	}
}

/**
 * @brief BridgeTerminalsCommand::addedBlocks
 * @return The terminals this command bridged, in the order of the strip,
 * cut into blocks of adjacent terminals. The terminals handed to the
 * command come in any order: from the editor they come out of a set.
 */
QVector<QVector<QSharedPointer<RealTerminal>>> BridgeTerminalsCommand::addedBlocks() const
{
	QVector<QVector<QSharedPointer<RealTerminal>>> blocks;
	if (!m_strip) {
		return blocks;
	}

	const auto pos_of = [this](const QSharedPointer<RealTerminal> &real_t) {
		return m_strip->pos(real_t->physicalTerminal());
	};

	auto sorted = m_added_terminals;
	std::sort(sorted.begin(), sorted.end(),
			  [&pos_of](const QSharedPointer<RealTerminal> &a,
						const QSharedPointer<RealTerminal> &b) {
		return pos_of(a) < pos_of(b);
	});

	int previous_pos = 0;
	for (const auto &real_t : std::as_const(sorted))
	{
		const int pos_ = pos_of(real_t);
		if (blocks.isEmpty() || pos_ != previous_pos + 1) {
			blocks.append(QVector<QSharedPointer<RealTerminal>>());
		}
		blocks.last().append(real_t);
		previous_pos = pos_;
	}

	return blocks;
}

UnBridgeTerminalsCommand::UnBridgeTerminalsCommand(TerminalStrip *strip,
												   QVector<QSharedPointer<RealTerminal>> real_terminal,
												   QUndoCommand *parent):
	QUndoCommand(parent),
	m_strip(strip)
{
	setText(QObject::tr("Delete terminal bridges"));

	if (strip->canUnBridge(real_terminal))
	{
		m_bridge = strip->isBridged(real_terminal.first());

			//If bridge have one more terminals than @real_terminal
			//that mean every terminals of the bridge must be unbridged by this undo command,
			//else the single terminal who's not umbridged by this undo command
			//continue to have a bridge (to nothing) and this nowhere bridge is visible
			//in the terminal strip graphic item and terminal strip editor dialog.
		if (m_bridge->realTerminals().size() == real_terminal.size() + 1) {
			m_terminals = m_bridge->realTerminals();
		} else {
			m_terminals = real_terminal;
		}
	}
}

void UnBridgeTerminalsCommand::undo()
{
	if (m_strip && m_bridge) {
		m_strip->setBridge(m_bridge, m_terminals);
	}
}

void UnBridgeTerminalsCommand::redo()
{
	if (m_strip) {
		m_strip->unBridge(m_terminals);
	}
}
