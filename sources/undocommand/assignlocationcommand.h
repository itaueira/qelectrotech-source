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
#ifndef ASSIGNLOCATIONCOMMAND_H
#define ASSIGNLOCATIONCOMMAND_H

#include <QList>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

class Element;

/**
	@brief The LocationAssignment struct
	One component and the path it should carry afterwards.

	The batch of a selection gives every component the same path and does
	not need this. A rename or a move gives each one a different path,
	because the selection was never the point: what moved was the tree.
*/
struct LocationAssignment
{
	Element *element = nullptr;
	QString path;
};

/**
	@brief The AssignLocationCommand class
	Write the location path onto components, undoably.

	The path lives on the component as an ordinary element information, the
	location_path key, and not as a link to the tree. That is decision C of
	the task, and the price of it is paid here: moving a location means
	rewriting every component that was inside it, one by one, which is why
	this command exists in a per-component shape as well as a batch one.

	What it takes is what it gives back. Each component keeps the path it
	had before, so undo restores exactly that, and a component that already
	carried the wanted path is left out of the list entirely - it has
	nothing to undo, and counting it would make the caption lie. A caller
	that finds componentCount at zero should not push the command at all.

	An empty path is not a missing value, it is the answer "not assigned",
	and it clears the key rather than storing emptiness. A project where
	nobody named an enclosure therefore has no location_path anywhere in
	its file, and opens in the stock QElectroTech byte for byte as before.

	The project database is told once at the end, not once per component:
	elementInfoChanged rebuilds rows, and the nomenclature does not need to
	be rebuilt eighty times to be right once.
*/
class AssignLocationCommand : public QUndoCommand
{
	public:
		AssignLocationCommand(const QList<Element *> &elements,
				      const QString &path,
				      QUndoCommand *parent = nullptr);
		AssignLocationCommand(const QList<LocationAssignment> &assignments,
				      const QString &label,
				      QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

		int componentCount() const;

	private:
		struct Assignment
		{
			QPointer<Element> element;
			QString old_path;
			QString new_path;
		};

		void take(Element *element, const QString &path);
		void apply(bool forward);
		void updateProjectDataBase();

	private:
		QList<Assignment> m_assignments;
};

#endif // ASSIGNLOCATIONCOMMAND_H
