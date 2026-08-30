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
#ifndef ASSIGNIOPOINTSCOMMAND_H
#define ASSIGNIOPOINTSCOMMAND_H

#include "../plc/iolist.h"
#include "../properties/elementdata.h"

#include <QPointer>
#include <QUndoCommand>

class Element;
class QETProject;

/**
	@brief The AssignIoPointsCommand class
	Put I/O points into the channels of a PLC card, undoably, or take them
	back out.

	Assigning is two writes that have to move together: the point of the
	project list learns which card and which row it took, and the row of that
	card learns the function text the point carries. Half of it undone is a
	point that says it is in a card the card knows nothing about, so the two
	are one command.

	The card half is delegated to a ChangeElementDataCommand built as a
	child, because that is what already knows how to hand an ElementData back
	to an Element and let it tell its linked slaves. The list half is here,
	and works the way ImportIoPointsCommand does: both states kept whole and
	swapped, never recomputed.

	What the redraw needs is one more thing the child does not do: a PLC
	master draws its channel table in Element::paint, and setElementData does
	not ask for a repaint. So this command does, on both sides.

	Releasing is the same command with the other pair of lists. The caller
	has already worked out what release() leaves behind - in particular that
	a cell edited by hand since is kept - and hands the result over the same
	way. Only the caption changes.
*/
class AssignIoPointsCommand : public QUndoCommand
{
	public:
		AssignIoPointsCommand(QETProject *project,
				      Element *element,
				      const IoList &list,
				      const ElementData &data,
				      int count,
				      bool releasing = false,
				      QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		void apply(const IoList &list);
		void refresh();

	private:
		QPointer<QETProject> m_project;
		QPointer<Element> m_element;
		IoList m_old_list;
		IoList m_new_list;
};

#endif // ASSIGNIOPOINTSCOMMAND_H
