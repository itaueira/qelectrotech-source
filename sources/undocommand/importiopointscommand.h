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
#ifndef IMPORTIOPOINTSCOMMAND_H
#define IMPORTIOPOINTSCOMMAND_H

#include "../plc/iolist.h"

#include <QPointer>
#include <QUndoCommand>

class QETProject;

/**
	@brief The ImportIoPointsCommand class
	Replace the I/O list of a project, undoably.

	The merge itself happens in the dialogue, not here: a person about to
	import sixty points has to see what the merge will do before it happens,
	and that means the new list already exists by the time this command is
	built. So the command holds both states and swaps them, which is also
	what makes undo exact - the list comes back the way it was, including the
	points the sheet did not mention and that the merge deliberately kept.

	Nothing is drawn by this command. An imported point exists in the project
	before it exists on any folio, which is the state the task is about; what
	assigns it to a channel and draws it is a command of its own.
*/
class ImportIoPointsCommand : public QUndoCommand
{
	public:
		ImportIoPointsCommand(QETProject *project,
				      const IoList &list,
				      int touched,
				      QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		void apply(const IoList &list);

	private:
		QPointer<QETProject> m_project;
		IoList m_old_list;
		IoList m_new_list;
};

#endif // IMPORTIOPOINTSCOMMAND_H
