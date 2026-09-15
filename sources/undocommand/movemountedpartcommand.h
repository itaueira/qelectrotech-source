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
#ifndef MOVEMOUNTEDPARTCOMMAND_H
#define MOVEMOUNTEDPARTCOMMAND_H

#include "../location/layout/mountingscene.h"

#include <QPointF>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

/**
	@brief The MoveMountedPartCommand class
	One part of a mounting surface put somewhere else, undoably.

	Two positions in millimetre and the identity of the part, which is
	everything a move on a plate is. The part is addressed by its identity
	and never by a pointer to what draws it: the drawing is rebuilt whenever
	the surface is set again, and a stack holding pointers to items that no
	longer exist is a crash waiting for the first undo after a reload.

	It goes on the stack of the scene, which is not the stack of the
	project. Undoing a move of a breaker on the plate must not undo the last
	thing that happened on a folio, and the two stacks being separate is
	what makes that true rather than hoped for.

	Nothing is ever refused here and nothing is ever clamped. A part dragged
	off the edge of the plate stays where it was dropped, because "this does
	not fit where you put it" is a report the rule already knows how to make
	and a drawing that silently pushes parts back inside is a drawing that
	answers a question nobody asked.
*/
class MoveMountedPartCommand : public QUndoCommand
{
	public:
		MoveMountedPartCommand(MountingScene *scene,
				       const QString &item_uuid,
				       const QPointF &before_mm,
				       const QPointF &after_mm,
				       QUndoCommand *parent = nullptr);
		~MoveMountedPartCommand() override;

		void undo() override;
		void redo() override;

			/// @return which part this moves
		QString itemUuid() const;
			/// @return where the part was, millimetre
		QPointF before() const;
			/// @return where the part goes, millimetre
		QPointF after() const;

		/**
			@return true when pushing this would leave a step that
			undoes nothing.

			The scene checks it before pushing, the way the command
			that assigns a location checks its own count: a step that
			changes nothing makes the next undo look like it failed.
		*/
		bool isNull() const;

	private:
		QPointer<MountingScene> m_scene;
		QString m_uuid;
		QPointF m_before;
		QPointF m_after;
};

#endif // MOVEMOUNTEDPARTCOMMAND_H
