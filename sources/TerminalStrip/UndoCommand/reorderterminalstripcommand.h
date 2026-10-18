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
#ifndef REORDERTERMINALSTRIPCOMMAND_H
#define REORDERTERMINALSTRIPCOMMAND_H

#include <QPointer>
#include <QSharedPointer>
#include <QString>
#include <QUndoCommand>
#include <QVector>

class TerminalStrip;
class PhysicalTerminal;
class TerminalStripBridge;

/**
	@brief Put the physical terminals of a strip in an order chosen by hand.

	SortTerminalStripCommand computes its order from the labels; this one
	is handed the order. It comes from a button of the strip editor - move
	up, move down, move to a position - and the static functions below are
	how the editor computes it. They live here, beside the command that
	applies their answer, so that the three moves and the bridge rule they
	answer to are written once.

	Only the order changes. No label is renumbered: a terminal moved from
	the fifth place to the fourth keeps its label, because which label a
	terminal should carry is not decided by where it sits on the rail.

	The order is the whole of what undo gives back, and it is enough: a
	bridge holds terminals, not places, so a bridge that was whole before
	the move is whole again after the undo.
*/
class ReorderTerminalStripCommand : public QUndoCommand
{
	public:
		using PhysicalVector = QVector<QSharedPointer<PhysicalTerminal>>;

		ReorderTerminalStripCommand(TerminalStrip *strip,
					    const PhysicalVector &new_order,
					    const QString &text,
					    QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

		/**
			@return @a order with every terminal of @a selected one place
			closer to the start, each run of selected terminals stepping
			over the terminal just before it.

			A run already at the start stays where it is, and so does a
			selected terminal right after it, which has nowhere to go; the
			other runs move all the same. The relative order of the selected
			terminals is kept.
		*/
		static PhysicalVector movedUp(const PhysicalVector &order,
					      const PhysicalVector &selected);

		/// movedUp, towards the end.
		static PhysicalVector movedDown(const PhysicalVector &order,
						const PhysicalVector &selected);

		/**
			@return @a order with the terminals of @a selected taken out and
			put back as one block whose first terminal sits at @a index,
			counted from 0 like TerminalStrip::pos(). The block keeps the
			relative order the selected terminals had on the rail, wherever
			they were. An @a index past the last place the block fits in is
			that last place.
		*/
		static PhysicalVector movedTo(const PhysicalVector &order,
					      const PhysicalVector &selected,
					      int index);

		/**
			@return the bridges of @a strip that run along adjacent terminals
			today and would not in @a new_order - the bridges the new order
			would leave with a terminal in their middle.

			Adjacent is what TerminalStrip::isBridgeable asks of a bridge
			when it is made: the physical terminals holding it follow one
			another on the rail, with no other between them, whatever their
			levels. A bridge already left with a gap - a strip sorted after
			it was bridged - is not reported: the move is not what opened
			it, and refusing every move of such a strip would leave no way
			to close the gap by hand.
		*/
		static QVector<QSharedPointer<TerminalStripBridge>> openedBridges(const TerminalStrip *strip,
										  const PhysicalVector &new_order);

	private:
		QPointer<TerminalStrip> m_strip;
		PhysicalVector m_old_order,
			       m_new_order;
};

#endif // REORDERTERMINALSTRIPCOMMAND_H
