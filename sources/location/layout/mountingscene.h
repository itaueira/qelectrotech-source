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
#ifndef MOUNTINGSCENE_H
#define MOUNTINGSCENE_H

#include "../mountinglayout.h"

#include <QGraphicsScene>
#include <QHash>
#include <QRectF>
#include <QStringList>
#include <QUndoStack>

class MountedPartItem;
class MountedProfileItem;
class QPainter;

/**
	@brief The canvas one mounting surface is laid out on, and whose unit is
	the millimetre.

	One scene unit is one millimetre. Not approximately, and not by
	convention: the scene is built over the frame MountingArea already
	declares - x runs right, y runs down, origin at the top left corner of
	the surface - so a position on this scene and a position in the file are
	the same pair of numbers, and no arithmetic stands between them. That is
	what makes a drawing comparable with the panel a person has a tape
	measure against.

	It is a second scene of the program and not a second kind of folio, in
	the shape the element editor already has: its own scene, its own undo
	stack, and later its own view and its own window. What it borrows from
	the framework is what the framework gives everyone - selection,
	dragging, the rubber band, the transform of the view. What it
	deliberately does not borrow is the folio: a folio counts in grid units
	and rounds every position to them, which does not displace a measurement
	of 22,5 mm, it loses it.

	The surface itself is painted as background and is not an item. That is
	the difference between "you should not move the plate" and "there is
	nothing here that can be moved": a branch of a third party made it an
	item and had to defend it with flags, and the day a flag is forgotten
	the plate follows the mouse. Here there is no object to select, to
	drag, to delete or to send behind anything, and the rule costs no
	vigilance at all.

	The undo stack is its own, and that is not free. The stack of a folio
	belongs to the project, and this window is not a folio - so undoing here
	undoes a move on the plate and never a change on a folio, which is
	exactly what is wanted and is also the reason nothing here can be undone
	from the schematic side.

	What the scene holds is a value, and it hands back a value: surface()
	reads the position of every part off the items themselves, so what is
	reported is always what is on the screen, drag in progress included. Who
	writes that into the project, and which undo stack that write goes
	through, is the business of whoever hosts this scene.

	@par Three things are steps, and setting the face is not one

	Moving a part, putting one on and taking one off are steps of the stack.
	Setting the face is not: it draws another surface, and a step that moved
	a part of the previous one has nothing left to move - so the stack is
	dropped there, and only there.

	That distinction is the whole reason mounting is a method of this class
	and not of the window above. The first half of this editor rebuilt the
	whole drawing to take one part in, which dropped the stack every time
	somebody added something: an undo that is there sometimes is worse than
	an undo that is never there. Taking one part in and out without touching
	the rest is what makes the composition of a face undoable, and what lets
	a rail be drawn by a gesture that can be taken back.
*/
class MountingScene : public QGraphicsScene
{
	Q_OBJECT

	public:
		explicit MountingScene(QObject *parent = nullptr);
		~MountingScene() override;

		/**
			@brief Lay out @a surface on this scene.
			@param surface the face and everything mounted on it

			Everything drawn before is dropped, and so is the undo
			stack: a step that moves a part of the previous surface
			has nothing left to move.
		*/
		void setSurface(const MountingSurface &surface);

		/**
			@return the face as it now stands, in millimetre, the
			position of every part read back off the drawing.
		*/
		MountingSurface surface() const;

			/// @return how much room there is to mount on, millimetre
		MountingArea area() const;
			/// @return true when the area of the face is a usable pair of lengths
		bool isAreaMeasured() const;

			/// @return how many parts are drawn
		int partCount() const;
			/// @return the identity of every part drawn, in no special order
		QStringList itemUuids() const;
			/// @return the item of @a item_uuid, nullptr when there is none
		MountedPartItem *partItem(const QString &item_uuid) const;
			/// @return what is mounted under @a item_uuid, a default one when nothing is
		MountedItem mountedItem(const QString &item_uuid) const;
			/// @return where @a item_uuid sits in the face, -1 when it is not on it
		int indexOfItem(const QString &item_uuid) const;

		/**
			@brief Screw a part onto this face, undoably.
			@param item what is mounted, position and size in
			millimetre
			@param error filled with why nothing was mounted
			@return true when a step was pushed on the stack

			Refused, with the reason, for a part with no identity and
			for one already on this face. The first is what an undo
			step could never take back, and the second is the
			invariant the layout holds for the whole project - one
			identity, one place - said again here, because the
			drawing must not be able to show what the file cannot
			hold.

			The identity is not given out here. This is a drawing,
			and MountingLayout is what hands out identities; a scene
			that made them up would put two of them on one part the
			first time somebody drew the same rail on two faces.
		*/
		bool mountItem(const MountedItem &item, QString *error = nullptr);

		/**
			@brief Take a part off this face, undoably.
			@param item_uuid which part
			@param error filled with why nothing was taken off
			@return true when a step was pushed on the stack

			The face stops saying where that part is screwed, and
			nothing else happens to it: what becomes of the component
			belongs to whoever owns it, which is the rule the layout
			already states for a face that is deleted.
		*/
		bool unmountItem(const QString &item_uuid, QString *error = nullptr);

		/**
			@brief Cut a piece to another length, undoably.
			@param item_uuid which piece
			@param footprint_mm the room it takes afterwards,
			millimetre
			@param error filled with why nothing was cut
			@return true when a step was pushed on the stack

			A rectangle and not a length, because one end of a piece
			can be pulled as well as the other and that moves its
			corner. Refused for a part that was bought as a piece:
			a breaker is the size the catalogue says it is, and a
			drawing that let somebody stretch one would be a drawing
			disagreeing with the product.
		*/
		bool stretchItem(const QString &item_uuid,
				 const QRectF &footprint_mm,
				 QString *error = nullptr);

		/**
			@brief Move a part, undoably.
			@param item_uuid which part
			@param position_mm where its top left corner goes, millimetre
			@param error filled with why nothing was moved
			@return true when a step was pushed on the stack

			False and an empty error means there was nothing to do:
			the part is already at that millimetre. A step that
			changes nothing must not reach the stack, because the
			next undo would then look like it did nothing.
		*/
		bool moveItem(const QString &item_uuid,
			      const QPointF &position_mm,
			      QString *error = nullptr);

		/**
			@brief Put a part at @a position_mm without touching the
			undo stack.
			@param item_uuid which part
			@param position_mm where its top left corner goes, millimetre
			@return true when there was such a part

			This is the hand of the undo command, and of whoever
			rebuilds the drawing from a model that changed elsewhere.
			Anything a person does goes through moveItem instead.
		*/
		bool applyItemPosition(const QString &item_uuid,
				       const QPointF &position_mm);

		/**
			@brief Draw a part at a place in the list, without
			touching the undo stack.
			@param item what is mounted, millimetre
			@param index where it goes in the list of the face, -1
			for the end of it
			@return true when it was drawn

			The hand of the undo command. The index is carried
			because the order of the list is compared when the
			project decides whether it has anything to save: a part
			taken off and put back at the end would make a face that
			is identical to itself ask to be saved.
		*/
		bool applyItemMounting(const MountedItem &item, int index = -1);

		/**
			@brief Take a part off the drawing, without touching the
			undo stack.
			@param item_uuid which part
			@return true when there was such a part
		*/
		bool applyItemUnmounting(const QString &item_uuid);

		/**
			@brief Give a part another rectangle, without touching
			the undo stack.
			@param item_uuid which part
			@param footprint_mm the room it takes, millimetre
			@return true when there was such a part

			The corner and the size at once, because a stretch from
			the far end is both of them, and writing one after the
			other would draw a piece that existed at no moment of the
			gesture.
		*/
		bool applyItemGeometry(const QString &item_uuid,
				       const QRectF &footprint_mm);

			/// @return the stack the moves of this surface are on
		QUndoStack &undoStack();

		/**
			@brief Where a millimetre position lands on this scene.
			@param position_mm a position in millimetre
			@return the same position

			The identity, and it is written down rather than left
			implicit for two reasons. It names the crossing, so every
			place a millimetre becomes a scene coordinate can be found
			by looking for it; and it says where the factor is not. A
			scale factor belongs to the insertion of this layout into
			a folio - that is where a drawing is fitted to a sheet of
			paper - and a factor stored next to a length is a length
			nobody can compare with a panel any more.
		*/
		static QPointF sceneFromMillimetre(const QPointF &position_mm);

			/// @return the millimetre a scene position stands for
		static QPointF millimetreFromScene(const QPointF &scene_position);

			/// @return the spacing of the grid drawn on the face, millimetre
		static qreal gridStep();
			/// @return the room left around the face on the scene, millimetre
		static qreal surroundingMargin();

	signals:
			/// @brief Emitted after a part has been put somewhere else
		void itemMoved(const QString &item_uuid);
			/// @brief Emitted after a part has been screwed onto the face
		void itemMounted(const QString &item_uuid);
			/// @brief Emitted after a part has been taken off the face
		void itemUnmounted(const QString &item_uuid);
			/// @brief Emitted after a piece has been cut to another length
		void itemStretched(const QString &item_uuid);
			/// @brief Emitted whenever what surface() would answer changed
		void surfaceChanged();

	protected:
		void drawBackground(QPainter *painter, const QRectF &rect) override;

	private:
		void rebuild();
		void updateSceneRect();
		MountedPartItem *drawItem(const MountedItem &item);
		bool pushMove(const QString &item_uuid,
			      const QPointF &before_mm,
			      const QPointF &after_mm);
		void endDrag(MountedPartItem *part,
			     const QPointF &previous_position_mm);
		void endStretch(MountedProfileItem *piece,
				const QRectF &previous_footprint_mm);

		MountingSurface m_surface;
		QHash<QString, MountedPartItem *> m_items;
		QUndoStack m_undo_stack;
};

#endif // MOUNTINGSCENE_H
