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
#ifndef RENUMBERCOMMAND_H
#define RENUMBERCOMMAND_H

#include "../autoNum/renumberplan.h"
#include "../diagramcontext.h"

#include <QList>
#include <QPointer>
#include <QUndoCommand>

class Element;

/**
	@brief The RenumberCommand class
	Apply a renumbering plan, and take it all back with one Ctrl+Z.

	One command for the whole plan, not one per component. Renumbering a real
	project changes a few hundred tags; a user who has to press Ctrl+Z three
	hundred times has not been given undo, they have been given a punishment.
*/
class RenumberCommand : public QUndoCommand
{
	public:
		RenumberCommand(const QList<Element *> &elements,
				const RenumberPlan &plan,
				QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

		int changeCount() const;

	private:
		/// One component, its label before and after.
		class Change
		{
			public:
				QPointer<Element> element;
				DiagramContext old_information;
				DiagramContext new_information;
		};

		void apply(bool forward);
		void updateProjectDataBase();

	private:
		QList<Change> m_changes;
};

#endif // RENUMBERCOMMAND_H
