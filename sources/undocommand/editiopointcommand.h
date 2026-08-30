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
#ifndef EDITIOPOINTCOMMAND_H
#define EDITIOPOINTCOMMAND_H

#include "../plc/iolist.h"
#include "../properties/elementdata.h"

#include <QPointer>
#include <QUndoCommand>

class Element;
class QETProject;

/**
	@brief The EditIoPointCommand class
	Change one cell of one I/O point from the project list, undoably.

	A point that has already taken a channel says the same thing twice: the
	point of the project list carries the tag, the description, the address
	and the comment, and the row of the card carries the function text, the
	address and the comment it shows on the folio. Editing the list without
	editing the card leaves the folio saying the old thing, so the two are
	one command - and the card half is the same ChangeElementDataCommand
	child AssignIoPointsCommand builds, for the same reason: it is what
	knows how to hand an ElementData back to an Element and let it tell its
	linked slaves.

	A point that has not taken a channel yet has no card to write, and the
	element is then null. That is not an error: importing a list and typing
	a description into it before any assignment is the ordinary order of
	work.

	Both states of the list are kept whole and swapped, never recomputed,
	the way ImportIoPointsCommand and AssignIoPointsCommand do. The caller
	has already worked out the new list and the new element data; this only
	moves between them.

	No mergeWith: a cell of a QTreeWidget commits once, when the editor
	closes, not at every keystroke, so there is nothing to merge and the
	undo stack would only lose the boundaries the person expects.
*/
class EditIoPointCommand : public QUndoCommand
{
	public:
		EditIoPointCommand(QETProject *project,
				   Element *element,
				   const IoList &list,
				   const ElementData &data,
				   const QString &label,
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

#endif // EDITIOPOINTCOMMAND_H
