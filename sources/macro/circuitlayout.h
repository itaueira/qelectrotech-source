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
#ifndef CIRCUITLAYOUT_H
#define CIRCUITLAYOUT_H

#include <QList>
#include <QPointF>

/**
	@brief Where the generated circuits go: which folio, and which column of
	it.

	The unit is the column of the border, not the pixel. A schematic is read
	by column - "the contactor is on folio 4, column C" is how a person on
	the shop floor is told where to look - so a generator that dropped
	circuits every 380 pixels would be drawing something nobody can refer
	to. The capacity comes from the folio itself, through
	BorderTitleBlock::columnsCount() and columnsWidth(), which is why the
	same table generates differently on an A4 and on an A3 and is right
	both times.

	Everything here is arithmetic on integers and one QPointF: no diagram,
	no project, no file. That is what lets the test binary link it, and it
	is also what lets the dialogue of step 4 show how many folios a table
	will take before anything is drawn.
*/
namespace CircuitLayout
{
		/**
			@brief The capacity of one folio, as its border states it.
		*/
	struct Sheet
	{
			/// BorderTitleBlock::columnsCount()
		int columns_count = 0;
			/// BorderTitleBlock::columnsWidth(), in scene units
		qreal columns_width = 0.0;
			/// insideBorderRect().topLeft(): where column zero starts
		QPointF origin;

			/**
				@return whether a circuit can be placed on a folio like this

				A folio whose border draws no column can still be drawn on by
				hand, but it cannot be measured in columns, and a generator
				that guessed a capacity here would scatter twenty circuits on
				top of each other.
			*/
		bool isValid() const
		{
			return columns_count > 0 && columns_width > 0.0;
		}
	};

		/**
			@brief One circuit, and where it was put.
		*/
	struct Placement
	{
			/// zero based, counted from the first folio the generation makes
		int sheet = 0;
			/// zero based, the column of that folio the circuit starts at
		int column = 0;
			/// how many columns it spends, always at least one
		int columns = 1;
			/// the scene point the circuit's top left corner lands on
		QPointF pos;
			/// wider than a whole folio: placed alone, and worth saying so
		bool oversized = false;
	};

		/**
			@param width : a measured width, in scene units
			@param sheet
			@return how many whole columns that width spends, at least one

			Rounded up, and never down: half a column left over is still a
			column another circuit cannot have.
		*/
	int columnsFor(qreal width, const Sheet &sheet);

		/**
			Place circuits of the given widths, in order, left to right.
			@param widths : one width per circuit, in columns
			@param sheet
			@param circuits_per_sheet : the configured break, zero for none
			@return one placement per width, in the same order

			Two things start a new folio: the circuit not fitting in the
			columns left, and the folio already holding the number of circuits
			the person asked for. The second is the reason the option exists -
			ten feeders fit on an A3, but a person who reviews four at a time
			wants four.

			A circuit wider than the whole folio is placed alone on a folio of
			its own rather than refused. It will overrun the border, and the
			person is told which line did it, but a drawing that runs over the
			frame can be looked at and fixed; a circuit that was never drawn
			cannot.
		*/
	QList<Placement> place(const QList<int> &widths,
			       const Sheet &sheet,
			       int circuits_per_sheet = 0);

		/**
			@param placements
			@return how many folios they take, zero when there are none
		*/
	int sheetsUsed(const QList<Placement> &placements);
}

#endif // CIRCUITLAYOUT_H
