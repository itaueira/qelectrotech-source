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
#ifndef GROUPEDUPDATECOMMAND_H
#define GROUPEDUPDATECOMMAND_H

#include <QPointer>
#include <QString>
#include <QUndoCommand>

class QETProject;

/**
	@brief A composite command whose children are one gesture, as far as the
	project data base is concerned.

	Several places build one command out of many: renaming a wire renames
	every conductor of its potential, and each child writes a row. Every row
	written announces itself, and every announcement makes each list drawn on
	a folio re-run its query - so a gesture over a potential of thirty
	conductors redraws every list thirty times, all but the last of them
	showing a state the draughtsman never asked to see.

	Using this class as the parent of those children makes it one
	announcement, delivered when the last child is done.

	Deliberately in both directions: redo() and undo() both group. A guard
	placed around QUndoStack::push() instead would cover the first
	application and leave every later Ctrl+Z and Ctrl+Y storming, which is
	the same defect with a longer fuse.

	It adds nothing else - no text of its own beyond the one given, no merge
	id - so that swapping a plain QUndoCommand for this one changes what the
	data base is told and nothing else.
*/
class GroupedUpdateCommand : public QUndoCommand
{
	public:
		explicit GroupedUpdateCommand(QETProject *project,
					      const QString &text = QString(),
					      QUndoCommand *parent = nullptr);

		void redo() override;
		void undo() override;

	private:
		/**
			A QPointer and not a raw one: a command outlives its own push,
			and the project it belongs to can be closed while the stack is
			being torn down.
		*/
		QPointer<QETProject> m_project;
};

#endif // GROUPEDUPDATECOMMAND_H
