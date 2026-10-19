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
#ifndef ARRANGECONDUCTORSCOMMAND_H
#define ARRANGECONDUCTORSCOMMAND_H

#include "../conductorprofile.h"
#include "../qetgraphicsitem/conductor.h"
#include "../wirearrangement.h"

#include <QPointer>
#include <QUndoCommand>

class Diagram;

/**
	@brief Gives conductors the paths WireArrangement::plan() found, saved
	as the path of each one and marked as placed by the application.
	Undo puts back the paths they had, and the mark.
*/
class ArrangeConductorsCommand : public QUndoCommand
{
	public:
		ArrangeConductorsCommand(Diagram *diagram,
					 const QList<WireArrangement::Change> &changes,
					 QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;
		int count() const {return m_entries.size();}

	private:
		struct Entry
		{
			QPointer<Conductor> conductor;
			ConductorProfilesGroup profiles;
			bool arranged = false;
			QPointF text_pos;
			QList<QPointF> points;
		};

		QPointer<Diagram> m_diagram;
		QList<Entry> m_entries;
};

#endif // ARRANGECONDUCTORSCOMMAND_H
