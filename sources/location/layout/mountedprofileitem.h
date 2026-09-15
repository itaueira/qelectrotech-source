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
#ifndef MOUNTEDPROFILEITEM_H
#define MOUNTEDPROFILEITEM_H

#include "mountedpartitem.h"

#include <QRectF>
#include <QVector>

class QetGraphicsHandlerItem;
class QGraphicsSceneMouseEvent;

/**
	@brief A piece of rail or of cable duct on a mounting surface: a part
	like any other, with the one difference that its length is cut here.

	It is a MountedPartItem, and that is the whole of its cheapness. It is
	dragged, selected, written back and undone by the machinery the part
	already has, it reports its position in millimetre the same way, and the
	rules underneath see a rectangle exactly as they always did. What it
	adds is one gesture: two handles, one on each end, and what they do to
	the model when they are pulled.

	@par The handles are the ones the program already has

	QetGraphicsHandlerItem is what every shape of a folio is resized by, and
	it is borrowed whole rather than imitated: it draws itself at a constant
	size on the screen whatever the zoom - ItemIgnoresTransformations - which
	a scene counted in millimetre needs even more than a folio does, since
	one unit here is a millimetre and a handle of ten units would be a blob
	the width of a breaker. The way it is wired is the way QetShapeItem
	wires it, down to the sceneEventFilter: the handles are items of the
	scene, they are given to this item as an event filter, and the three
	mouse events are dispatched to the three private functions below.

	@par Two handles and not four

	A piece cut to length has one dimension that is a cut and one that is
	the bar it came from. Four handles would offer to change the second,
	which no gesture on a drawing should be allowed to do - a 40 mm duct
	pulled a little wider is a duct nobody sells, silently. So the ends are
	grabbable and the sides are not, and which of the two axes is which is
	the run the item carries.

	@par Why the stretch is not pushed from here

	The item ends the gesture by saying where the piece was when it began,
	and stops there. Which undo stack this belongs to is not its business -
	it is the scene that owns one - and it is the same contract the drag
	already has: dragged() reports where it came from, stretched() reports
	the rectangle it came from. One class emits, one class pushes.
*/
class MountedProfileItem : public MountedPartItem
{
	Q_OBJECT

	public:
			/// what qgraphicsitem_cast answers about this item
		enum { Type = UserType + 1401 };

		explicit MountedProfileItem(const MountedItem &mounted_item,
					    QGraphicsItem *parent = nullptr);
		~MountedProfileItem() override;

		int type() const override;

		void setMountedItem(const MountedItem &mounted_item) override;

			/// @return the bar this piece was cut from
		MountingProfile profile() const;
			/// @return which way it runs
		MountingRun run() const;
			/// @return how long it is, millimetre, zero when uncut
		qreal cutLength() const;

		/**
			@return the room the piece takes, millimetre, in the
			frame of the surface.

			The same rectangle MountedItem::footprint() answers, read
			off the drawing rather than off the copy - so it is right
			in the middle of a gesture, which is exactly when the undo
			command asks.
		*/
		QRectF footprintRect() const;

		/**
			@brief Cut this piece to another rectangle.
			@param footprint_mm the room it takes afterwards,
			millimetre

			The one way the geometry of this item changes from the
			outside, and the hand an undo command holds. It writes
			the corner and the size at once, because a stretch from
			the left end is both: writing them one after the other
			would draw a piece that existed at no moment of the
			gesture.
		*/
		void setFootprintRect(const QRectF &footprint_mm);

	signals:
		/**
			@brief Emitted when a stretch of this piece has just
			ended.
			@param previous_footprint_mm the room it took when the
			gesture began

			The piece has already been cut by then - the handles do
			that as the mouse moves, so that what is on the screen is
			what is being decided - so what is reported is what it
			was. Whoever listens turns that into one undo step.
		*/
		void stretched(const QRectF &previous_footprint_mm);

	protected:
		void paint(QPainter *painter,
			   const QStyleOptionGraphicsItem *option,
			   QWidget *widget = nullptr) override;
		QVariant itemChange(GraphicsItemChange change,
				    const QVariant &value) override;
		bool sceneEventFilter(QGraphicsItem *watched,
				      QEvent *event) override;

	private:
		void addHandles();
		void removeHandles();
		void adjustHandles();
		void handlerMousePressEvent();
		void handlerMouseMoveEvent(QGraphicsSceneMouseEvent *event);
		void handlerMouseReleaseEvent();
			/// @return the two ends of the piece, in the coordinates of this item
		QVector<QPointF> endPoints() const;

		QVector<QetGraphicsHandlerItem *> m_handlers;
		/// which handle is being pulled, -1 when none is
		int m_handler_index = -1;
		/// the room the piece took when the gesture began, millimetre
		QRectF m_before;
		/// true between the press on a handle and the release
		bool m_stretching = false;
};

#endif // MOUNTEDPROFILEITEM_H
