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
#ifndef MOVEMOUNTEDRAILCOMMAND_H
#define MOVEMOUNTEDRAILCOMMAND_H

#include "../location/layout/mountingscene.h"

#include <QList>
#include <QPair>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUndoCommand>

/**
	@brief The MoveMountedRailCommand class
	One rail of a mounting surface put somewhere else, undoably, and
	everything clipped onto it with it.

	It is the answer to the one mistake of a panel drawing that is only
	found in the workshop: a rail dragged to another place with the twelve
	breakers left behind where it used to be. The plate is then drilled from
	a drawing in which twelve parts sit where nothing holds them, and
	nothing on the screen ever said so.

	One step and not thirteen. A person who drags a rail did one thing, and
	an undo that gave back the rail and left the breakers where the drag put
	them would be a drawing nobody can get back to a known state - thirteen
	undos later, maybe, if they are counted right. What is pushed is this,
	once.

	@par What is stored, and why it is the before and not the movement

	The position each carried part had when the gesture began, one per part,
	and the two positions of the rail. Undo writes those positions back as
	they were; redo writes each of them plus the movement of the rail. The
	tempting shape is the other one - keep the movement only, and add or
	subtract it from wherever the part is now - and it is wrong twice over:
	it reads the drawing at undo time instead of the moment the step was
	made, and adding then subtracting a double does not always land on the
	number it started from. A part that came back a nanometre away from
	where it was would make a face that is identical to itself ask to be
	saved.

	Which parts those are is decided by the scene, before the rail has
	moved, and handed in. It has to be before: a rail that has already
	travelled fifty millimetres no longer covers the breakers it left
	behind, so a command that worked out its own list would faithfully carry
	nothing at all. That is the whole defect this class exists against,
	arrived at from the inside.

	Parts are addressed by identity and never by a pointer to what draws
	them, for the reason MoveMountedPartCommand states: the drawing is
	rebuilt whenever the surface is set again, and a stack holding pointers
	to items that no longer exist is a crash waiting for the first undo
	after a reload.

	Nothing is ever refused and nothing is ever clamped here either. A rail
	dragged half off the plate takes its breakers half off the plate, and
	the rule that says so is MountingCheck, on a panel, in words - not a
	drawing quietly pushing things back where they fit.
*/
class MoveMountedRailCommand : public QUndoCommand
{
	public:
		MoveMountedRailCommand(MountingScene *scene,
				       const QString &rail_uuid,
				       const QPointF &before_mm,
				       const QPointF &after_mm,
				       const QStringList &carried_uuids,
				       QUndoCommand *parent = nullptr);
		~MoveMountedRailCommand() override;

		void undo() override;
		void redo() override;

			/// @return which rail this moves
		QString itemUuid() const;
			/// @return where the rail was, millimetre
		QPointF before() const;
			/// @return where the rail goes, millimetre
		QPointF after() const;
			/// @return how far it travels, millimetre
		QPointF delta() const;

			/// @return what it takes with it, in the order handed over
		QStringList carried() const;
			/// @return how many parts it takes with it
		int carriedCount() const;

		/**
			@param item_uuid one of the parts carried
			@return where that part was when the gesture began,
			millimetre

			A position that is not one when this command carries no
			such part, so that a caller asking about the wrong
			identity gets an answer it cannot mistake for a place.
		*/
		QPointF carriedBefore(const QString &item_uuid) const;

		/**
			@return true when pushing this would leave a step that
			undoes nothing.

			Asked of the rail and of nothing else. A rail that has
			not moved has carried nothing anywhere, however many
			parts are clipped onto it, and a step on the stack for
			that would make the next undo look like it failed.
		*/
		bool isNull() const;

	private:
		QPointer<MountingScene> m_scene;
		QString m_uuid;
		QPointF m_before;
		QPointF m_after;
		/// what is carried, and where each of them was, in millimetre
		QList<QPair<QString, QPointF>> m_carried;
};

#endif // MOVEMOUNTEDRAILCOMMAND_H
