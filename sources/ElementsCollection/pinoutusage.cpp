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
#include "pinoutusage.h"

#include <QCoreApplication>

namespace
{
		/// how many conflicts are spelled out before the message stops
		/// counting them one by one. A block of thirty two points that
		/// lands on top of another would otherwise answer with a wall of
		/// thirty two identical sentences, which says less than five.
	const int MESSAGE_LIMIT = 5;
}

bool PinoutUsageEntry::isNull() const
{
	return component.isEmpty() && terminal.isEmpty();
}

QString PinoutUsageEntry::whereItIs() const
{
	if (sheet_number > 0 && !sheet.isEmpty())
	{
		return QCoreApplication::translate("PinoutUsageEntry",
				"folio %1 (%2)").arg(sheet_number).arg(sheet);
	}
	if (sheet_number > 0)
	{
		return QCoreApplication::translate("PinoutUsageEntry",
				"folio %1").arg(sheet_number);
	}
	return sheet;
}

bool PinoutUsageConflict::isNull() const
{
	return terminal.isEmpty();
}

QString PinoutUsageConflict::message() const
{
	if (isNull()) {
		return QString();
	}

	const QString place = existing.whereItIs();
		//Without a component there is nothing to attribute the terminal
		//to, so what is left to say is that the drawing repeats itself.
	if (existing.component.isEmpty())
	{
		if (place.isEmpty())
		{
			return QCoreApplication::translate("PinoutUsageConflict",
					"La borne %1 est dessinee deux fois "
					"dans ce bloc.").arg(terminal);
		}
		return QCoreApplication::translate("PinoutUsageConflict",
				"La borne %1 est deja dessinee sur le %2.")
				.arg(terminal, place);
	}
	if (place.isEmpty())
	{
		return QCoreApplication::translate("PinoutUsageConflict",
				"La borne %1 de %2 est deja dessinee dans ce "
				"projet.").arg(terminal, existing.component);
	}
	return QCoreApplication::translate("PinoutUsageConflict",
			"La borne %1 de %2 est deja dessinee sur le %3.")
			.arg(terminal, existing.component, place);
}

QString PinoutUsage::keyOf(const QString &component, const QString &terminal)
{
		//A line feed, because it is the one character a label typed on a
		//sheet cannot contain.
	return component + QStringLiteral("\n") + terminal;
}

bool PinoutUsage::add(const PinoutUsageEntry &entry,
		      PinoutUsageConflict *conflict)
{
	if (conflict) {
		*conflict = PinoutUsageConflict();
	}
		//An entry that names no component or no terminal registers
		//nothing and conflicts with nothing: false here means there was
		//nothing to hold on to, and the caller tells the two apart by
		//the conflict having stayed null.
	if (entry.component.isEmpty() || entry.terminal.isEmpty()) {
		return false;
	}

	const QString key = keyOf(entry.component, entry.terminal);
	if (m_index.contains(key))
	{
			//The first drawing wins, because it is the one already
			//on a folio and already wired.
		if (conflict)
		{
			*conflict = PinoutUsageConflict(entry.terminal,
					m_entries.at(m_index.value(key)));
		}
		return false;
	}

	m_index.insert(key, m_entries.size());
	m_entries << entry;
	return true;
}

int PinoutUsage::addSymbol(const PinoutUsageEntry &place,
			   const SymbolDefinition &symbol,
			   QList<PinoutUsageConflict> *conflicts)
{
	int added = 0;
	for (const QString &terminal : terminalsOf(symbol))
	{
		PinoutUsageEntry entry = place;
		entry.terminal = terminal;

		PinoutUsageConflict conflict;
		if (add(entry, &conflict))
		{
			added++;
			continue;
		}
		if (conflicts && !conflict.isNull()) {
			*conflicts << conflict;
		}
	}
	return added;
}

void PinoutUsage::clear()
{
	m_entries.clear();
	m_index.clear();
}

bool PinoutUsage::isEmpty() const
{
	return m_entries.isEmpty();
}

int PinoutUsage::count() const
{
	return m_entries.size();
}

QList<PinoutUsageEntry> PinoutUsage::entriesOf(const QString &component) const
{
	QList<PinoutUsageEntry> found;
	for (const PinoutUsageEntry &entry : m_entries)
	{
		if (entry.component == component) {
			found << entry;
		}
	}
	return found;
}

PinoutUsageEntry PinoutUsage::entryOf(const QString &component,
				      const QString &terminal) const
{
	const QString key = keyOf(component, terminal);
	if (!m_index.contains(key)) {
		return PinoutUsageEntry();
	}
	return m_entries.at(m_index.value(key));
}

bool PinoutUsage::holds(const QString &component, const QString &terminal) const
{
	return m_index.contains(keyOf(component, terminal));
}

QStringList PinoutUsage::components() const
{
		//In the order they were registered, and not in the order a hash
		//happens to hold them: a list that changes between two runs of
		//the same project is a list nobody can compare.
	QStringList found;
	for (const PinoutUsageEntry &entry : m_entries)
	{
		if (!found.contains(entry.component)) {
			found << entry.component;
		}
	}
	return found;
}

QList<PinoutUsageConflict> PinoutUsage::conflicts(const QString &component,
		const QStringList &terminals) const
{
	QList<PinoutUsageConflict> found;
	if (component.isEmpty()) {
		return found;
	}
	for (const QString &terminal : terminals)
	{
		const PinoutUsageEntry existing = entryOf(component, terminal);
		if (!existing.isNull()) {
			found << PinoutUsageConflict(terminal, existing);
		}
	}
	return found;
}

QList<PinoutUsageConflict> PinoutUsage::conflicts(const QString &component,
		const SymbolDefinition &symbol) const
{
		//What the block repeats inside itself comes first, because it is
		//wrong wherever it is put and no project has to be open to know
		//it.
	QList<PinoutUsageConflict> found = selfConflicts(symbol);
	found << conflicts(component, terminalsOf(symbol));
	return found;
}

QString PinoutUsage::refusal(const QString &component,
			     const SymbolDefinition &symbol) const
{
	return messageFor(conflicts(component, symbol));
}

QStringList PinoutUsage::terminalsOf(const SymbolDefinition &symbol)
{
	QStringList names;
	for (const SymbolTerminal &terminal : symbol.terminals)
	{
			//A terminal with no name is skipped rather than counted
			//as one more empty name. A symbol drawn by hand has all
			//of them empty — the boards of the real project have one
			//hundred and ninety seven of those — and checking those
			//would make every hand drawn symbol collide with every
			//other one.
		const QString name = terminal.label.trimmed();
		if (!name.isEmpty() && !names.contains(name)) {
			names << name;
		}
	}
	return names;
}

QList<PinoutUsageConflict> PinoutUsage::selfConflicts(
		const SymbolDefinition &symbol)
{
	QList<PinoutUsageConflict> found;
	QStringList seen;
	for (const SymbolTerminal &terminal : symbol.terminals)
	{
		const QString name = terminal.label.trimmed();
		if (name.isEmpty()) {
			continue;
		}
		if (seen.contains(name))
		{
			PinoutUsageEntry existing;
			existing.terminal = name;
			found << PinoutUsageConflict(name, existing);
			continue;
		}
		seen << name;
	}
	return found;
}

QString PinoutUsage::messageFor(const QList<PinoutUsageConflict> &conflicts)
{
	QStringList lines;
	int hidden = 0;
	for (const PinoutUsageConflict &conflict : conflicts)
	{
		const QString line = conflict.message();
		if (line.isEmpty()) {
			continue;
		}
		if (lines.size() >= MESSAGE_LIMIT)
		{
			hidden++;
			continue;
		}
		lines << line;
	}
	if (lines.isEmpty()) {
		return QString();
	}
	if (hidden > 0)
	{
		lines << QCoreApplication::translate("PinoutUsage",
				"... et %n autre(s) borne(s).", "", hidden);
	}
	return lines.join(QStringLiteral("\n"));
}
