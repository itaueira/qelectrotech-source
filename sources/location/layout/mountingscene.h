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

#include "../mountingalign.h"
#include "../mountinglayout.h"

#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QPointF>
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

	@par A rail moves with what is clipped onto it, in one step

	Dragging a rail takes the parts clipped onto it along, and that is one
	step of the stack and not thirteen: the person did one thing, and an
	undo giving the rail back while leaving the breakers where the drag put
	them would leave a drawing nobody can return to a known state. Which
	parts travel is read off the geometry - MountingClip, and nothing is
	stored anywhere - and it is read at the position the rail had when the
	gesture began, since a rail that has already moved covers nothing it
	left behind.

	Clipping itself is a gesture and not a rule over every position this
	scene writes. A number typed into a box goes where it was typed; a part
	let go over a rail is caught by it. See clipItem.
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
			@brief Say where the axis of each product sits inside its
			own body.
			@param axis_by_part_code the offset from the top left
			corner of the body, millimetre, keyed by product code

			What a part hangs by when it is clipped onto a rail: the
			axis of a breaker is not the corner of its box, and a row
			lined up by its corners is a row out of line by however
			far the two differ. A product code that is not in the
			table is a product the catalogue was never told about,
			and its parts line up by the corner - visibly, on the
			drawing, which is the point.

			It is handed in rather than read here because this is a
			drawing and a catalogue is a data base. The scene knows
			what it draws and nothing about products; whoever hosts
			it has a catalogue open and fills this in.

			Kept across a change of face on purpose: it says what
			products are, not what is on a plate.
		*/
		void setPartAxes(const QHash<QString, QPointF> &axis_by_part_code);
			/// @return where the axis of each product sits, millimetre
		QHash<QString, QPointF> partAxes() const;

		/**
			@brief Which rail carries a part.
			@param item_uuid which part
			@return the identity of the rail, empty when none does
			and when there is no such part

			Read off the geometry every time it is asked and never
			stored - see MountingClip, where the reasons are.
		*/
		QString carrierOf(const QString &item_uuid) const;

		/**
			@brief What a rail carries where it stands.
			@param rail_uuid which rail
			@return the identity of each part, in the order it stands
			along the rail
		*/
		QStringList carriedBy(const QString &rail_uuid) const;

		/**
			@brief What a rail would carry if it stood somewhere
			else.
			@param rail_uuid which rail
			@param rail_position_mm where its top left corner would
			be, millimetre
			@return the identity of each part, in order along the
			rail

			The overload a move needs, and the reason it exists is
			the defect this step is written against: a rail that has
			already been dragged fifty millimetres no longer covers
			the breakers it left behind, so asking it what it carries
			AFTER the drag answers nothing at all. What has to be
			asked is what it carried where the gesture began.
		*/
		QStringList carriedBy(const QString &rail_uuid,
				      const QPointF &rail_position_mm) const;

		/**
			@brief Where a part would land if it were clipped where
			it stands.
			@param item_uuid which part
			@return the top left corner it would take, millimetre

			Its own position when no rail is under it, and a position
			that is not one when there is no such part - so that the
			two answers cannot be mistaken for one another.
		*/
		QPointF clipTarget(const QString &item_uuid) const;

		/**
			@brief Clip a part onto the rail under it, undoably.
			@param item_uuid which part
			@param error filled with why nothing was clipped
			@return true when a step was pushed on the stack

			False with a reason when there is no such part and when
			nothing is under it; false with no reason when it is
			already where a clip would put it, which is the contract
			moveItem already has - a step that changes nothing must
			not reach the stack.

			It is a gesture, and not a rule applied to every position
			this scene writes, and that distinction is deliberate. A
			number typed into a box is honoured: moveItem puts a part
			exactly where it is told, because a drawing that quietly
			moves a part somewhere else is a drawing answering a
			question nobody asked. Letting go of a part over a rail
			is the gesture that means "hold it there", and that one
			clips.
		*/
		bool clipItem(const QString &item_uuid, QString *error = nullptr);

		/**
			@return the identity of every part selected on this
			scene, in no special order.

			The one thing a window above this needs to turn "what the
			person is pointing at" into a gesture, and it is here
			because the scene is what holds the selection. Anything
			selected that is not a part of this face - there is
			nothing else on this scene today - is left out rather
			than handed back as an empty identity.
		*/
		QStringList selectedUuids() const;

		/**
			@brief Line parts up on one edge, undoably.
			@param item_uuids which parts
			@param alignment which line they end up sharing
			@param error filled with why nothing was lined up
			@return true when a step was pushed on the stack

			One step for all of them, because it is one gesture. The
			line is taken from the parts themselves - see
			MountingAlign - so the outermost one stays where it is
			and the others come to it.

			Refused, with the reason, for fewer than two parts that
			can be moved. False with no reason when they were all on
			the line already: a step that moves nothing must not
			reach the stack.

			A rail or a duct in the selection is not moved, and that
			is deliberate rather than an oversight: a rail is what
			other parts are clipped onto, and moving it is the
			gesture that carries them with it. Whoever offers this
			can say so by comparing what it was given with
			MountingAlign::movableUuids.
		*/
		bool alignItems(const QStringList &item_uuids,
				MountingAlignment alignment,
				QString *error = nullptr);

		/**
			@brief Leave the same air between parts, undoably.
			@param item_uuids which parts
			@param run along which axis they are spread
			@param error filled with why nothing was spread
			@return true when a step was pushed on the stack

			Equal gaps between bodies, the two outermost parts left
			where they are. Refused, with the reason, for fewer than
			three parts that can be moved, and refused when they do
			not fit in the room they already stand in - spreading
			them then would leave a row of parts evenly overlapping,
			which looks deliberate and is the one answer nobody
			wants.
		*/
		bool distributeItems(const QStringList &item_uuids,
				     MountingRun run,
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

		/**
			@brief Everything on the face, with one part put
			somewhere else.
			@param moved_uuid which part is displaced, empty for none
			@param position_mm where its top left corner is put,
			millimetre
			@return the list, read off the drawing

			The input every question about clipping is asked over,
			and the displacement is what lets it be asked about a
			moment other than now: a move has to know what a rail
			carried where the gesture began, and by then the rail is
			already somewhere else.
		*/
		QList<MountedItem> itemsWith(const QString &moved_uuid,
					     const QPointF &position_mm) const;

			/// @return what is drawn for each of @a item_uuids, in that order
		QList<MountedItem> itemsOf(const QStringList &item_uuids) const;

			/// @return what the undo list says about an alignment of @a count parts
		QString alignCaption(MountingAlignment alignment,
				     int count) const;
			/// @return what it says about spreading @a count parts
		QString spreadCaption(MountingRun run, int count) const;

		bool pushMove(const QString &item_uuid,
			      const QPointF &before_mm,
			      const QPointF &after_mm);
		void endDrag(MountedPartItem *part,
			     const QPointF &previous_position_mm);
		void endStretch(MountedProfileItem *piece,
				const QRectF &previous_footprint_mm);

		MountingSurface m_surface;
		QHash<QString, MountedPartItem *> m_items;
		/// where the axis of each product sits inside its own body, millimetre
		QHash<QString, QPointF> m_axes;
		QUndoStack m_undo_stack;
};

#endif // MOUNTINGSCENE_H
