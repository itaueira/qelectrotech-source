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
#ifndef EDITLOCATIONTREECOMMAND_H
#define EDITLOCATIONTREECOMMAND_H

#include "../location/locationtree.h"

#include <QList>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

class Element;
class QETProject;
struct LocationAssignment;

/**
	@brief The EditLocationTreeCommand class
	Replace the location tree of the project, undoably, dragging with it
	every component that was standing on a path that moved.

	Creating and deleting an enclosure are the easy half. Renaming one, or
	moving it into another, is where a location manager usually loses data:
	the tree says QCP1 and eighty components still say QCM1, and nothing
	complains until the material list comes out short months later. So the
	two halves are one command. The caller hands over the tree as the edit
	left it, plus the map update, move or remove reported, and the
	components follow in the same gesture and come back in the same undo.

	The map is read by plain lookup, never by prefix. update and move fill
	it with one entry per path of the whole branch - see
	LocationTree::rewrittenPath - so a component three levels down is found
	directly, and renaming QCM1 cannot reach into QCM10. A removal is fed
	in through LocationTree::lostPaths, which maps every path that stopped
	existing to an empty one, and empty means not assigned.

	Both trees are kept whole and swapped, never recomputed, the way
	EditIoPointCommand does with its list. The component half is a child
	AssignLocationCommand, which is what knows how to write an element
	information and tell the database once at the end; that is also why
	redo runs the children first and undo runs them last.

	No mergeWith. Renaming an enclosure is one deliberate act, and the undo
	stack should keep the boundaries the person expects to step back over.
*/
class EditLocationTreeCommand : public QUndoCommand
{
	public:
		EditLocationTreeCommand(QETProject *project,
					const LocationTree &tree,
					const QMap<QString, QString> &changed,
					const QString &label,
					QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

		int componentCount() const;

	private:
		static QList<LocationAssignment> followers(
				QETProject *project,
				const QMap<QString, QString> &changed);
		void apply(const LocationTree &tree);

	private:
		QPointer<QETProject> m_project;
		LocationTree m_old_tree;
		LocationTree m_new_tree;
		int m_component_count = 0;
};

#endif // EDITLOCATIONTREECOMMAND_H
