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
#ifndef TERMINALSTRIPREADER_H
#define TERMINALSTRIPREADER_H

#include "terminalstripstructure.h"

#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVector>

class RealTerminal;
class TerminalStrip;
class TerminalStripBridge;

/**
	@brief A TerminalStrip read into the plain structure the rules of
	terminalstripstructure.h are written against, with the way back.

	Those rules answer in positions and levels and never touch a
	RealTerminal, which is what lets them be proved without a project open.
	Something has to turn a real strip into that structure, and turn an
	answer given in positions back into the terminals it is about. This
	class is that something, and the only one, so that the two directions
	are written side by side and cannot drift apart.

	@par A reading is a snapshot

	Nothing here follows the strip. A strip reordered, grouped or bridged
	after the reading is not the strip the reading describes, and the
	positions it hands back would point at other terminals. Read, use the
	answer, and read again after the next change.

	@par Where each field comes from

	The position is the index of the physical terminal in the strip, and the
	level the index of the real terminal in its physical terminal - the same
	two counts TerminalStrip::isBridgeable checks. The type and the function
	are those of the element, as saved: a value typed in the table of the
	editor and not yet applied is not part of the strip, and is not read.
	The bridge is an index into the bridges met while reading, opaque as
	StripLevel asks. The wire number is wireNumber() of the conductors
	docked to the terminal.
*/
class TerminalStripReader
{
	public:
		explicit TerminalStripReader(const TerminalStrip *strip);

		/// The strip, as the rules see it.
		const TerminalStripStructure &structure() const;

		/**
			@return the real terminal at @a level of the physical terminal
			at @a position, null when there is none
		*/
		QSharedPointer<RealTerminal> realTerminal(int position, int level) const;

		/**
			@return the real terminals a proposal covers, in the order of
			its positions; empty when any of them cannot be found

			All or nothing on purpose: a comb missing one of its terminals
			is not a shorter comb, it is a comb nobody proposed.
		*/
		QVector<QSharedPointer<RealTerminal>> realTerminals(const StripBridgeProposal &proposal) const;

		/**
			@brief The one wire number the conductors of a terminal agree on.
			@param conductor_texts the texts of the conductors docked to the
			terminal, as RealTerminal::conductors() gives them
			@return that number, trimmed; an empty string when they carry
			none, or more than one

			Several conductors on one terminal is the normal case - one from
			the field, one from the panel - and on a through terminal they
			carry one number. When they carry two, the drawing says two
			different things about one potential, and this answers that it
			does not know rather than picking one: the first conductor is
			only first by its place on the folio, and a bridge decided by
			where a wire was drawn would be a bridge nobody could explain.

			A conductor left without a number is not a second number: an
			empty text and the placeholder a new conductor is given are
			skipped, by DrcWireNumbers::numbersOf, the reading the
			project check already gives the same texts. Read as a number,
			the placeholder would make every pair of unnumbered terminals
			look like two terminals on one wire.
		*/
		static QString wireNumber(const QStringList &conductor_texts);

	private:
		TerminalStripStructure m_structure;
			/// Indexed like m_structure: [position][level].
		QVector<QVector<QSharedPointer<RealTerminal>>> m_real_terminals;
			/// Indexed by the opaque identifier written in StripLevel::bridge.
		QVector<QSharedPointer<TerminalStripBridge>> m_bridges;
};

#endif // TERMINALSTRIPREADER_H
