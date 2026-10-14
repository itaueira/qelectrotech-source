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
#ifndef TERMINALMATCH_H
#define TERMINALMATCH_H

#include <QPointF>
#include <QString>
#include <QUuid>
#include <QVector>

/**
	@brief Which terminal of a replacing element each terminal of the
	replaced one becomes, so that its wires can follow it.

	Pure on purpose: two lists in, one list out, no element and no folio.
	The element on the folio is read into these lists by whoever calls, see
	SwapElementCommand::plan().
*/
namespace TerminalMatch
{
	/**
		@brief A terminal, as far as matching is concerned.
	*/
	struct Spot
	{
			/// the uuid the symbol gives the terminal, null when none
		QUuid uuid;
			/// the name the symbol gives the terminal, empty when none
		QString name;
			/// where wires dock, in folio coordinates
		QPointF point;
		int orientation = 0;
	};

	QVector<int> match(const QVector<Spot> &old_spots,
			   const QVector<Spot> &new_spots);
}

#endif // TERMINALMATCH_H
