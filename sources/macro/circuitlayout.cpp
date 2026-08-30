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
#include "circuitlayout.h"

#include <QtMath>

namespace
{
		/**
			A circuit measured at exactly two columns must spend two, not
			three. The measurement comes from a union of bounding rectangles,
			so it lands a hair over or a hair under the round number; this is
			how much "a hair" is allowed to be before it costs a column.
		*/
	const qreal kColumnTolerance = 1e-6;
}

/**
	@brief CircuitLayout::columnsFor
	@param width
	@param sheet
	@return the number of columns @a width spends on @a sheet
*/
int CircuitLayout::columnsFor(qreal width, const Sheet &sheet)
{
	if (!sheet.isValid() || width <= 0.0) {
		return 1;
	}

	const int columns = qCeil(width / sheet.columns_width - kColumnTolerance);
	return columns < 1 ? 1 : columns;
}

/**
	@brief CircuitLayout::place
	@param widths
	@param sheet
	@param circuits_per_sheet
	@return one placement per width, in the same order
*/
QList<CircuitLayout::Placement> CircuitLayout::place(const QList<int> &widths,
						     const Sheet &sheet,
						     int circuits_per_sheet)
{
	QList<Placement> placements;
	if (!sheet.isValid()) {
		return placements;
	}
	placements.reserve(widths.count());

		//Where the next circuit would go if it fits.
	int sheet_index = 0;
	int column = 0;
		//How many circuits the current folio already holds. Zero also means
		//"nothing has landed here yet", which is what keeps the first folio
		//from being skipped and what keeps two consecutive oversized
		//circuits from leaving an empty folio between them.
	int on_sheet = 0;

	for (const int requested : widths)
	{
		Placement placement;
		placement.columns = requested < 1 ? 1 : requested;
		placement.oversized = placement.columns > sheet.columns_count;

		if (placement.oversized)
		{
				//It will overrun the border wherever it goes, so it goes
				//alone: sharing a folio with it would bury a circuit that
				//is fine under one that is not.
			if (on_sheet > 0) {
				++sheet_index;
			}
			placement.sheet = sheet_index;
			placement.column = 0;

			++sheet_index;
			column = 0;
			on_sheet = 0;
		}
		else
		{
			const bool no_room = (column + placement.columns > sheet.columns_count);
			const bool sheet_full = (circuits_per_sheet > 0 && on_sheet >= circuits_per_sheet);
			if (on_sheet > 0 && (no_room || sheet_full))
			{
				++sheet_index;
				column = 0;
				on_sheet = 0;
			}

			placement.sheet = sheet_index;
			placement.column = column;

			column += placement.columns;
			++on_sheet;
		}

		placement.pos = sheet.origin
				+ QPointF(placement.column * sheet.columns_width, 0.0);
		placements.append(placement);
	}

	return placements;
}

/**
	@brief CircuitLayout::sheetsUsed
	@param placements
	@return how many folios @a placements take
*/
int CircuitLayout::sheetsUsed(const QList<Placement> &placements)
{
	int last = -1;
	for (const Placement &placement : placements)
	{
		if (placement.sheet > last) {
			last = placement.sheet;
		}
	}
	return last + 1;
}
