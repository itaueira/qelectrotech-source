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
#include "terminalstripreader.h"

#include "../drc/drcwirenumbers.h"
#include "physicalterminal.h"
#include "realterminal.h"
#include "terminalstrip.h"
#include "terminalstripbridge.h"

/**
	@brief TerminalStripReader::TerminalStripReader
	@param strip : the strip to read; a null one reads as a strip with no
	terminal
*/
TerminalStripReader::TerminalStripReader(const TerminalStrip *strip)
{
	if (!strip) {
		return;
	}

	const auto physical_terminals = strip->physicalTerminal();
	for (const auto &physical : physical_terminals)
	{
		StripTerminal terminal;
		QVector<QSharedPointer<RealTerminal>> real_terminals;

		if (physical)
		{
			const auto levels = physical->realTerminals();
			for (const auto &real_terminal : levels)
			{
				StripLevel level;
				if (real_terminal)
				{
					level.label    = real_terminal->label();
					level.wire     = wireNumber(real_terminal->conductors());
					level.type     = real_terminal->type();
					level.function = real_terminal->function();

					const auto bridge_ = strip->isBridged(real_terminal);
					if (bridge_)
					{
						auto id = m_bridges.indexOf(bridge_);
						if (id < 0)
						{
							id = m_bridges.size();
							m_bridges.append(bridge_);
						}
						level.bridge = static_cast<int>(id);
					}
				}

					//A level that could not be read stays in the vector,
					//with no wire number: dropping it would shift every
					//level above it down by one, and the rule would then
					//comb the wrong row.
				terminal.levels.append(level);
				real_terminals.append(real_terminal);
			}
		}

			//Same for a position: a hole in the strip is still a place on
			//the rail, and closing it up would make two terminals that do
			//not touch look like neighbours.
		m_structure.terminals.append(terminal);
		m_real_terminals.append(real_terminals);
	}
}

/**
	@brief TerminalStripReader::structure
*/
const TerminalStripStructure &TerminalStripReader::structure() const {
	return m_structure;
}

/**
	@brief TerminalStripReader::realTerminal
*/
QSharedPointer<RealTerminal> TerminalStripReader::realTerminal(int position, int level) const
{
	if (position < 0 || position >= m_real_terminals.size()) {
		return QSharedPointer<RealTerminal>();
	}

	const auto &levels = m_real_terminals.at(position);
	if (level < 0 || level >= levels.size()) {
		return QSharedPointer<RealTerminal>();
	}

	return levels.at(level);
}

/**
	@brief TerminalStripReader::realTerminals
*/
QVector<QSharedPointer<RealTerminal>> TerminalStripReader::realTerminals(const StripBridgeProposal &proposal) const
{
	QVector<QSharedPointer<RealTerminal>> terminals;
	for (const auto position : proposal.positions)
	{
		const auto real_terminal = realTerminal(position, proposal.level);
		if (real_terminal.isNull()) {
			return QVector<QSharedPointer<RealTerminal>>();
		}
		terminals.append(real_terminal);
	}

	return terminals;
}

/**
	@brief TerminalStripReader::wireNumber
	@sa the declaration, for why two numbers read as none and why an
	unnumbered conductor is not counted.
*/
QString TerminalStripReader::wireNumber(const QStringList &conductor_texts)
{
	const QStringList numbers = DrcWireNumbers::numbersOf(conductor_texts);
	if (numbers.size() == 1) {
		return numbers.first();
	}

	return QString();
}
