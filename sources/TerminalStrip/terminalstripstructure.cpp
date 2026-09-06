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
#include "terminalstripstructure.h"

namespace
{
	/**
		@brief A run of adjacent terminals being considered for one comb.

		The proposal being built, plus the two things that decide whether it
		is worth keeping: which bridge already holds part of it, and whether
		any of it is still free.
	*/
	struct BridgeRun
	{
		StripBridgeProposal proposal;
		int bridge = StripLevel::NoBridge;
		bool has_free_terminal = false;
	};

	/**
		@brief Finish a run, keep it if it is a bridge worth making, and start
		the next one at the same level.

		Two terminals are the minimum, since a comb over one terminal joins
		nothing. And at least one of them has to be free, because a run every
		terminal of which is already held by the same bridge is a bridge that
		exists: proposing it again would ask somebody to confirm an operation
		with no effect, and TerminalStrip::isBridgeable would refuse it.
	*/
	void closeRun(QVector<StripBridgeProposal> &proposals, BridgeRun &run)
	{
		if (run.proposal.positions.size() >= 2 && run.has_free_terminal) {
			proposals.append(run.proposal);
		}

		const int level = run.proposal.level;
		run = BridgeRun{};
		run.proposal.level = level;
	}
}

/**
	@brief terminalLevelLabel
	@sa the declaration, for why the count and the label start at different
	numbers and why the label is not translated.
*/
QString terminalLevelLabel(int level)
{
	if (level < 0) {
		return QString();
	}

	return QStringLiteral("T%1").arg(level + 1);
}

/**
	@brief maximumLevelCount
*/
int maximumLevelCount(const TerminalStripStructure &structure)
{
	int count = 0;
	for (const auto &terminal : structure.terminals) {
		count = qMax(count, static_cast<int>(terminal.levels.size()));
	}

	return count;
}

/**
	@brief isThroughTerminalType
	@sa the declaration, for what each type does to the wire crossing it.

	Written as a switch over every value with no default branch, so that a
	terminal type added later fails to compile here instead of quietly
	inheriting the answer given to the generic one.
*/
bool isThroughTerminalType(ElementData::TerminalType type)
{
	switch (type)
	{
		case ElementData::TTGeneric:
			return true;
		case ElementData::TTGround:
			return true;
		case ElementData::TTFuse:
			return false;
		case ElementData::TTSectional:
			return false;
		case ElementData::TTDiode:
			return false;
	}

	return false;
}

/**
	@brief proposeBridgesByWire
	@sa the declaration, for the rule and for what it deliberately refuses.
*/
QVector<StripBridgeProposal> proposeBridgesByWire(const TerminalStripStructure &structure)
{
	QVector<StripBridgeProposal> proposals;

	const int level_count = maximumLevelCount(structure);
	for (int level = 0 ; level < level_count ; ++level)
	{
		BridgeRun run;
		run.proposal.level = level;

		for (int position = 0 ; position < structure.terminals.size() ; ++position)
		{
			const auto &levels = structure.terminals.at(position).levels;

				// A terminal with no such level breaks the run: the comb
				// would have to step over it, and it cannot.
			if (level >= levels.size())
			{
				closeRun(proposals, run);
				continue;
			}

			const auto &current = levels.at(level);

				// A terminal whose two sides are two different wires never
				// joins a run, and cuts the one it stands in. Same answer
				// for a wire number nobody has filled in.
			if (!isThroughTerminalType(current.type) || current.wire.isEmpty())
			{
				closeRun(proposals, run);
				continue;
			}

				// Another wire number is another potential, so it is another
				// run, starting here.
			if (!run.proposal.positions.isEmpty()
				&& current.wire != run.proposal.wire) {
				closeRun(proposals, run);
			}

				// A level a second bridge already holds cannot join this run
				// either: one comb, one bridge.
			if (current.bridge != StripLevel::NoBridge
				&& run.bridge != StripLevel::NoBridge
				&& current.bridge != run.bridge) {
				closeRun(proposals, run);
			}

			if (run.proposal.positions.isEmpty()) {
				run.proposal.wire = current.wire;
			}
			run.proposal.positions.append(position);

			if (current.bridge == StripLevel::NoBridge) {
				run.has_free_terminal = true;
			} else {
				run.bridge = current.bridge;
			}
		}

		closeRun(proposals, run);
	}

	return proposals;
}
