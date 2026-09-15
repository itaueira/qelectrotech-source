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
#ifndef STRETCHMOUNTEDPROFILECOMMAND_H
#define STRETCHMOUNTEDPROFILECOMMAND_H

#include "../location/layout/mountingscene.h"

#include <QPointer>
#include <QRectF>
#include <QString>
#include <QUndoCommand>

/**
	@brief The StretchMountedProfileCommand class
	One piece of rail or of duct cut to another length, undoably.

	Two rectangles and the identity of the piece. A rectangle and not a
	length, because pulling the left end of a rail changes where it starts
	as well as how long it is, and a step that wrote the length first and
	the corner afterwards would pass through a rail that existed at no
	moment of the gesture - which is what an undo would then take back.

	It addresses the piece by identity and never by a pointer to what draws
	it, for the reason the move command gives: the drawing is rebuilt
	whenever the face is shown again, and a stack holding pointers to items
	that no longer exist is a crash waiting for the first undo after a
	reload.

	Nothing is refused and nothing is clamped here. A piece cut longer than
	the plate stays as long as it was cut: "this does not fit where you put
	it" is a report the rule already knows how to make, and the shortest a
	gesture may leave a piece is a bound of the gesture - it belongs to the
	arithmetic of the stretch, not to the step that records it.
*/
class StretchMountedProfileCommand : public QUndoCommand
{
	public:
		StretchMountedProfileCommand(MountingScene *scene,
					     const QString &item_uuid,
					     const QRectF &before_mm,
					     const QRectF &after_mm,
					     QUndoCommand *parent = nullptr);
		~StretchMountedProfileCommand() override;

		void undo() override;
		void redo() override;

			/// @return which piece this cuts
		QString itemUuid() const;
			/// @return the room it took, millimetre
		QRectF before() const;
			/// @return the room it takes, millimetre
		QRectF after() const;

		/**
			@return true when pushing this would leave a step that
			undoes nothing.

			The same slack the fit and the dimension use, on all four
			numbers. A gesture that ended where it began is the
			ordinary case - somebody took hold of an end, thought
			better of it, and let go - and it has to leave the stack
			as it found it.
		*/
		bool isNull() const;

	private:
		QPointer<MountingScene> m_scene;
		QString m_uuid;
		QRectF m_before;
		QRectF m_after;
};

#endif // STRETCHMOUNTEDPROFILECOMMAND_H
