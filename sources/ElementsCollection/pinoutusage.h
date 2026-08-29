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
#ifndef PINOUTUSAGE_H
#define PINOUTUSAGE_H

#include "symbolbuilder.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief One terminal of one component, and where it is drawn.

	The unit the whole check is made of. A component is named by its label,
	the one the sheet shows: -U1. A terminal is named by the name it carries
	on the sheet, which for a generated block is the number the manufacturer
	printed: 01, 13, AI0.
*/
class PinoutUsageEntry
{
	public:
		PinoutUsageEntry() {}
		PinoutUsageEntry(const QString &component,
				 const QString &terminal) :
			component(component),
			terminal(terminal) {}

		/// the label of the element, which is what says "this is -U1"
		QString component;
		/// the name of the terminal inside that component
		QString terminal;
		/// the part it was drawn from, kept to say so in the message
		QString part_code;
		/// the title of the folio it is drawn on
		QString sheet;
		/// and its number, counted from one as the folio tab shows it
		int sheet_number = 0;
		/// the element it belongs to, so the caller can go and select it
		QString element_uuid;

		bool isNull() const;
		/**
			@return where this is drawn, in the words the sheet tab
			uses: "folio 3 (Commande)". Empty when nothing is known
			about the place, which happens on a bench and when a
			block has not been inserted yet.
		*/
		QString whereItIs() const;
};

/**
	@brief A terminal that would be drawn twice, and the drawing that has it.
*/
class PinoutUsageConflict
{
	public:
		PinoutUsageConflict() {}
		PinoutUsageConflict(const QString &terminal,
				    const PinoutUsageEntry &existing) :
			terminal(terminal),
			existing(existing) {}

		/// the name that is already taken
		QString terminal;
		/// and the entry that took it, which is where the original is
		PinoutUsageEntry existing;

		bool isNull() const;
		/// the sentence said to the person who tried, naming both
		QString message() const;
};

/**
	@brief What every component of a project already has drawn.

	The rule this exists for: the same terminal of the same component may
	not be drawn twice. A big drive is legitimately drawn as two blocks, one
	for the power and one for the control, and both are -U1; what is not
	legitimate is the terminal 01 appearing on both of them, because then
	the terminal list has two rows for one screw and the wiring of one of
	them is a guess.

	Identity here is the component label, and not the part code. Two blocks
	labelled -U1 are one drive cut in two. -U1 and -U2 are two drives, and
	each of them owns a terminal 01 of its own with nothing wrong about it.
	A block with no label is not attributed to any component, so it is not
	checked: refusing a drawing because it has not been named yet would
	refuse most drawings halfway through being made.

	The class holds no project and reads no file, which is why the rule can
	be proved on a bench. PinoutUsageScanner is what fills it from a real
	project.
*/
class PinoutUsage
{
	public:
		/**
			@brief Register @a entry.
			@param conflict filled with what already holds the place
			@return false when the terminal was already taken, and
			then nothing was registered: the first drawing wins,
			because it is the one already on a folio.
		*/
		bool add(const PinoutUsageEntry &entry,
			 PinoutUsageConflict *conflict = nullptr);
		/**
			@brief Register every named terminal of @a symbol.
			@param place the component, the part and the folio they
			all share. Its terminal field is ignored: the symbol is
			what names the terminals.
			@param conflicts appended with what could not be added
			@return how many were registered
		*/
		int addSymbol(const PinoutUsageEntry &place,
			      const SymbolDefinition &symbol,
			      QList<PinoutUsageConflict> *conflicts = nullptr);

		void clear();
		bool isEmpty() const;
		int count() const;

		QList<PinoutUsageEntry> entries() const {return m_entries;}
		QList<PinoutUsageEntry> entriesOf(const QString &component) const;
		/// what holds @a terminal of @a component, null when it is free
		PinoutUsageEntry entryOf(const QString &component,
					 const QString &terminal) const;
		bool holds(const QString &component,
			   const QString &terminal) const;
		/// the components that have something drawn, in no order
		QStringList components() const;

		/// what of @a terminals is already taken on @a component
		QList<PinoutUsageConflict> conflicts(const QString &component,
				const QStringList &terminals) const;
		QList<PinoutUsageConflict> conflicts(const QString &component,
				const SymbolDefinition &symbol) const;
		/**
			@return why @a symbol cannot be inserted as @a component,
			or an empty string when it can. Said before the insertion
			and not after, because a duplicated terminal found in the
			terminal list is a duplicated terminal that has already
			been wired.
		*/
		QString refusal(const QString &component,
				const SymbolDefinition &symbol) const;

		/// the names @a symbol draws, without the ones it left empty
		static QStringList terminalsOf(const SymbolDefinition &symbol);
		/// what @a symbol repeats inside itself, before anything else
		static QList<PinoutUsageConflict> selfConflicts(
				const SymbolDefinition &symbol);
		static QString messageFor(
				const QList<PinoutUsageConflict> &conflicts);

	private:
		static QString keyOf(const QString &component,
				     const QString &terminal);

		QList<PinoutUsageEntry> m_entries;
		QHash<QString, int> m_index;
};

#endif // PINOUTUSAGE_H
