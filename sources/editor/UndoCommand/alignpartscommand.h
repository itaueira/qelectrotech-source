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
#ifndef ALIGNPARTSCOMMAND_H
#define ALIGNPARTSCOMMAND_H

#include <QUndoCommand>

class ElementScene;

/**
	@brief Align, distribute or centre on the axis of the symbol the parts
	selected in the symbol editor, in one undo step (T73).
	The geometry is the folio's (Alignment): each part gives a box and a
	point - a text the box of its letters (PartLetters), a terminal its
	point, any other part the box of its drawing -, and the rule gives the
	movement of each. A text moves by the exact amount, so that its letters
	land on the line; any other part by whole numbers, as the grid of the
	editor places it, and a terminal stays on whole numbers (T72).
*/
class AlignPartsCommand : public QUndoCommand
{
	public:
		enum Mode {
			AlignLeft,
			AlignHCenter,
			AlignRight,
			AlignTop,
			AlignVCenter,
			AlignBottom,
			DistributeVertical,   ///< centre to centre, the top one staying
			DistributeHorizontal, ///< centre to centre, the left one staying
			CentreOnVerticalAxis,   ///< the selection's centre on x = 0
			CentreOnHorizontalAxis  ///< the selection's centre on y = 0
		};

		AlignPartsCommand(ElementScene *scene, Mode mode, QUndoCommand *parent = nullptr);

			/// How many parts of @a scene the mode can work on: aligning
			/// and distributing take two, centring one
		static int partCount(ElementScene *scene);
		static int partsNeeded(Mode mode);
};

#endif // ALIGNPARTSCOMMAND_H
