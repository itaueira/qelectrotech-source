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
#include "wireremoval.h"

#include "conductorautonumerotation.h"
#include "diagram.h"
#include "diagramcontent.h"
#include "qetgraphicsitem/conductor.h"
#include "qetproject.h"
#include "undocommand/deleteqgraphicsitemcommand.h"
#include "undocommand/groupedupdatecommand.h"

#include <QCoreApplication>
#include <QUndoStack>

namespace
{
		/// The wires of @p folio that go, the ones that stay added to @p count
	QList<Conductor *> goingOf(Diagram *folio, WireRemoval::Count &count)
	{
		QList<Conductor *> going;
		const QList<Conductor *> wires = folio->conductors();
		for (Conductor *wire : wires)
		{
			if (ConductorAutoNumerotation::isFrozen(wire)) {
				++count.frozen;
				//The mark the user put, and not the rule of a component out
				//of the list (T81): what stays is what was chosen to stay
			} else if (wire->isFreezeLabel()) {
				++count.left_out;
			} else {
				going << wire;
			}
		}
		count.removed += going.size();
		return going;
	}
}

WireRemoval::Count WireRemoval::count(const QList<Diagram *> &folios)
{
	Count count;
	for (Diagram *folio : folios)
	{
		if (folio) {
			goingOf(folio, count);
		}
	}
	return count;
}

WireRemoval::Count WireRemoval::removeWires(QETProject *project, const QList<Diagram *> &folios)
{
	Count count;
	if (!project || project->isReadOnly()) {
		return count;
	}

		//One gesture for the data base, every folio in it
	auto *undo = new GroupedUpdateCommand(
			project, QCoreApplication::translate("WireRemoval", "Remove the wires"));
	for (Diagram *folio : folios)
	{
		if (!folio || folio->project() != project) {
			continue;
		}
		DiagramContent content;
		content.m_other_conductors = goingOf(folio, count);
		if (!content.m_other_conductors.isEmpty()) {
			new DeleteQGraphicsItemCommand(folio, content, undo);
		}
	}
	if (undo->childCount()) {
		project->undoStack()->push(undo);
	} else {
		delete undo;
	}
	return count;
}
