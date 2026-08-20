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
#ifndef ASSIGNCATALOGPARTCOMMAND_H
#define ASSIGNCATALOGPARTCOMMAND_H

#include "../catalog/catalogpart.h"
#include "../diagramcontext.h"

#include <QList>
#include <QPointer>
#include <QStringList>
#include <QUndoCommand>

class Catalog;
class Element;

/**
	@brief The AssignCatalogPartCommand class
	Assign one catalog part to one or several components, in a single
	undoable step.

	One command and not two, because assigning a part changes two things at
	once - the information of the component and the names of its terminals -
	and a user who presses Ctrl+Z expects the drawing to go back to what it
	was, not halfway.
*/
class AssignCatalogPartCommand : public QUndoCommand
{
	public:
		AssignCatalogPartCommand(const QList<Element *> &elements,
					 const Catalog &catalog,
					 const CatalogPart &part,
					 QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

		/// @return how many components the command touches
		int componentCount() const;

	private:
		/// What one component looked like before, and what it becomes.
		struct Assignment
		{
			QPointer<Element> element;
			DiagramContext old_information;
			DiagramContext new_information;
			QStringList old_terminal_names;
			QStringList new_terminal_names;
		};

		void apply(bool forward);
		void updateProjectDataBase();

	private:
		QList<Assignment> m_assignments;
};

#endif // ASSIGNCATALOGPARTCOMMAND_H
