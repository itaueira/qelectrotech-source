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
#ifndef MOUNTPARTCOMMAND_H
#define MOUNTPARTCOMMAND_H

#include "../location/layout/mountingscene.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

/**
	@brief The MountPartCommand class
	One thing screwed onto a mounting surface, or taken off it, undoably.

	@par One class for the two directions

	Mounting and unmounting are the same step read the two ways round, and
	keeping them in one class is what stops the pair from drifting: the
	redo of one is the undo of the other, written once. A second class
	would have had to hold the same item, the same index and the same
	reasons, and the day one of them started restoring the position and the
	other did not, nothing would have said so.

	@par Why the item travels whole, and why its place in the list does too

	The command holds the item as a value - identity, mark, product, size,
	profile and all - because undoing a removal has to put back what was
	there and not something that looks like it. And it holds the index it
	sat at, because the order of the list is compared when the project
	decides whether it has anything to save: a piece taken off and put back
	at the end of the list would make a project that is identical to itself
	ask to be saved, for ever.

	@par What it does not do

	It does not refuse a part that does not fit, it does not push it back
	inside the plate, and it does not delete anything from the project. A
	surface stops saying where a component is screwed; what becomes of the
	component is the business of whoever owns it, which is the same rule
	MountingLayout::removeSurface states for a face.
*/
class MountPartCommand : public QUndoCommand
{
	public:
			/// which way round the step is read
		enum Way
		{
			Mount,  ///< redo puts the part on, undo takes it off
			Unmount ///< redo takes the part off, undo puts it back
		};

		MountPartCommand(MountingScene *scene,
				 const MountedItem &mounted_item,
				 Way way,
				 int index = -1,
				 QUndoCommand *parent = nullptr);
		~MountPartCommand() override;

		void undo() override;
		void redo() override;

			/// @return which part this mounts or unmounts
		QString itemUuid() const;
			/// @return the part, as it was when the step was made
		MountedItem mountedItem() const;
			/// @return which way round the step is read
		Way way() const;

		/**
			@return true when pushing this would leave a step that
			does nothing.

			A part with no identity is the case: the drawing has no
			way of addressing it afterwards, so the step could be
			done and never undone. The scene refuses to mount one for
			the same reason, and the two answers are deliberately
			given in both places - a command that cannot say no is a
			command that gets pushed by the next caller.
		*/
		bool isNull() const;

	private:
		void mount();
		void unmount();

		QPointer<MountingScene> m_scene;
		MountedItem m_item;
		Way m_way;
		/// where in the list of the face it sat, -1 for the end of it
		int m_index;
};

#endif // MOUNTPARTCOMMAND_H
