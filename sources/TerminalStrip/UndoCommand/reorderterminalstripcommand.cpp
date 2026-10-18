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
#include "reorderterminalstripcommand.h"
#include "../terminalstrip.h"
#include "../physicalterminal.h"
#include "../realterminal.h"
#include "../terminalstripbridge.h"

#include <QtGlobal>

#include <algorithm>

namespace {
/**
	@return true when the physical terminals holding @a bridge follow one
	another in @a order, with no other terminal between them.

	The same question TerminalStrip::isBridgeable asks when a bridge is
	made - positions of physical terminals, levels left aside - asked of
	an order that is not the strip's yet. A terminal of the bridge that
	@a order does not hold gives true: there is no rail to measure it on,
	and this answer only exists to refuse what would open a bridge.
*/
bool isAdjacentIn(const QSharedPointer<TerminalStripBridge> &bridge,
		  const ReorderTerminalStripCommand::PhysicalVector &order)
{
	QVector<int> positions;
	const auto real_terminals = bridge->realTerminals();
	for (const auto &real_t : real_terminals)
	{
		if (real_t.isNull()) {
			return true;
		}

		const int position = static_cast<int>(order.indexOf(real_t->physicalTerminal()));
		if (position < 0) {
			return true;
		}
		if (!positions.contains(position)) {
			positions.append(position);
		}
	}

	if (positions.size() < 2) {
		return true;
	}

	const auto min_max = std::minmax_element(positions.cbegin(), positions.cend());
	return (*min_max.second - *min_max.first) + 1 == positions.size();
}
}

/**
	@param strip : the strip to reorder
	@param new_order : every physical terminal of @a strip, in the order
	wanted; TerminalStrip::setOrderTo refuses anything else, and then
	redo does nothing.
	@param text : what the undo stack shows for this command
	@param parent : parent undo command
*/
ReorderTerminalStripCommand::ReorderTerminalStripCommand(TerminalStrip *strip,
							 const PhysicalVector &new_order,
							 const QString &text,
							 QUndoCommand *parent) :
	QUndoCommand(text, parent),
	m_strip(strip),
	m_new_order(new_order)
{
	if (m_strip) {
		m_old_order = m_strip->physicalTerminal();
	}
}

void ReorderTerminalStripCommand::undo()
{
	if (m_strip) {
		m_strip->setOrderTo(m_old_order);
	}
}

void ReorderTerminalStripCommand::redo()
{
	if (m_strip) {
		m_strip->setOrderTo(m_new_order);
	}
}

ReorderTerminalStripCommand::PhysicalVector ReorderTerminalStripCommand::movedUp(const PhysicalVector &order,
										   const PhysicalVector &selected)
{
	PhysicalVector moved = order;

		//Walked from the start, a selected terminal steps over the free
		//one before it, and the free one, now one place further, is met
		//again by the next selected terminal of the same run: the whole
		//run moves by one, and the free terminal ends up after it.
	for (int i = 1 ; i < moved.size() ; ++i)
	{
		if (selected.contains(moved.at(i))
			&& !selected.contains(moved.at(i - 1))) {
			moved.swapItemsAt(i, i - 1);
		}
	}

	return moved;
}

ReorderTerminalStripCommand::PhysicalVector ReorderTerminalStripCommand::movedDown(const PhysicalVector &order,
										     const PhysicalVector &selected)
{
	PhysicalVector moved = order;

		//movedUp, walked from the end.
	for (int i = static_cast<int>(moved.size()) - 2 ; i >= 0 ; --i)
	{
		if (selected.contains(moved.at(i))
			&& !selected.contains(moved.at(i + 1))) {
			moved.swapItemsAt(i, i + 1);
		}
	}

	return moved;
}

ReorderTerminalStripCommand::PhysicalVector ReorderTerminalStripCommand::movedTo(const PhysicalVector &order,
										   const PhysicalVector &selected,
										   int index)
{
	PhysicalVector block;
	PhysicalVector others;
	for (const auto &physical : order)
	{
		if (selected.contains(physical)) {
			block.append(physical);
		} else {
			others.append(physical);
		}
	}

	if (block.isEmpty()) {
		return order;
	}

	const int at = qBound(0, index, static_cast<int>(others.size()));
	for (int i = 0 ; i < block.size() ; ++i) {
		others.insert(at + i, block.at(i));
	}

	return others;
}

QVector<QSharedPointer<TerminalStripBridge>> ReorderTerminalStripCommand::openedBridges(const TerminalStrip *strip,
										       const PhysicalVector &new_order)
{
	QVector<QSharedPointer<TerminalStripBridge>> opened;
	if (!strip) {
		return opened;
	}

		//The strip keeps its bridges to itself; every one of them holds
		//at least two of its terminals, so walking the terminals meets
		//them all, and in the order of the rail.
	QVector<QSharedPointer<TerminalStripBridge>> bridges;
	const auto real_terminals = strip->realTerminals();
	for (const auto &real_t : real_terminals)
	{
		const auto bridge = strip->isBridged(real_t);
		if (!bridge.isNull() && !bridges.contains(bridge)) {
			bridges.append(bridge);
		}
	}

	const auto old_order = strip->physicalTerminal();
	for (const auto &bridge : std::as_const(bridges))
	{
		if (isAdjacentIn(bridge, old_order)
			&& !isAdjacentIn(bridge, new_order)) {
			opened.append(bridge);
		}
	}

	return opened;
}
