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
#ifndef ALIGNMOUNTEDPARTSCOMMAND_H
#define ALIGNMOUNTEDPARTSCOMMAND_H

#include "../location/layout/mountingscene.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUndoCommand>

/**
	@brief The AlignMountedPartsCommand class
	Several parts of a mounting surface put somewhere else at once,
	undoably.

	One step for one gesture. Lining six contactors up on their left edges
	is one thing a person did, and an undo that gave back one contactor per
	press would be six presses to get back to a drawing they can recognise -
	by which time they have lost count of what else was undone on the way.

	It carries a table of destinations and reads every departure itself, at
	the moment the step is made. Both are kept: undo writes the departures
	back as they were and redo writes the destinations, so neither direction
	is a subtraction of the other. A position worked out by adding and then
	subtracting a double does not always land on the number it started
	from, and a part that came back a nanometre away from where it was would
	make a face that is identical to itself ask to be saved.

	The caption is handed in rather than built here, and that is the one
	thing this class deliberately does not know. What a person sees in the
	undo list is "Aligner 6 composants à gauche" or "Répartir 4 composants
	horizontalement", and which of those it was is the gesture, not the
	table: the table is six identities and six positions either way. Whoever
	offers the gesture writes the sentence.

	Parts are addressed by identity and never by a pointer to what draws
	them, for the reason MoveMountedPartCommand states: the drawing is
	rebuilt whenever the surface is set again, and a stack holding pointers
	to items that no longer exist is a crash waiting for the first undo
	after a reload.

	Nothing is refused and nothing is clamped. Parts lined up onto an edge
	that hangs off the plate stay off the plate, and the rule that says so
	is MountingCheck, on a panel, in words.
*/
class AlignMountedPartsCommand : public QUndoCommand
{
	public:
		AlignMountedPartsCommand(MountingScene *scene,
					 const QHash<QString, QPointF> &targets,
					 const QString &caption,
					 QUndoCommand *parent = nullptr);
		~AlignMountedPartsCommand() override;

		void undo() override;
		void redo() override;

			/// @return how many parts this moves
		int count() const;
			/// @return which parts this moves, in a settled order
		QStringList itemUuids() const;

		/**
			@param item_uuid one of the parts moved
			@return where it was when the step was made, millimetre;
			a position that is not one when this moves no such part
		*/
		QPointF before(const QString &item_uuid) const;

		/**
			@param item_uuid one of the parts moved
			@return where it goes, millimetre; a position that is not
			one when this moves no such part
		*/
		QPointF after(const QString &item_uuid) const;

		/**
			@return true when pushing this would leave a step that
			undoes nothing.

			True when nothing is left to move: the rule that built
			the table already dropped the parts that were on the line
			already, and a table emptied by that is a gesture that
			found everything where it wanted it.
		*/
		bool isNull() const;

	private:
		QPointer<MountingScene> m_scene;
		/// where each part was, in the order the identities sort
		QList<QPair<QString, QPointF>> m_before;
		/// where each part goes
		QHash<QString, QPointF> m_after;
};

#endif // ALIGNMOUNTEDPARTSCOMMAND_H
