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
#ifndef MOUNTEDPARTITEM_H
#define MOUNTEDPARTITEM_H

#include "../enclosuretransfer.h"
#include "../../factory/elementpicturefactory.h"

#include <QGraphicsObject>
#include <QHash>

class MountingLayout;
class QETProject;
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

/**
	@brief One thing screwed to a mounting surface, drawn on a scene whose
	unit is the millimetre.

	It descends from QGraphicsObject and not from QetGraphicsItem, and that
	is not a matter of taste: QetGraphicsItem::setPos passes every point
	through Diagram::snapToGrid before positioning it, and that grid is an
	integer number of folio units read from the settings. A breaker of
	22,5 mm put through that arithmetic comes back at 20 or at 25 - the
	measurement is not displaced, it is lost, and no zoom brings it back.
	What is inherited here instead is what the Qt framework gives everyone:
	selection, dragging, the rubber band and the transform of the view.

	Geometry in, geometry out, in millimetre. pos() is the top left corner
	of the footprint, in the frame MountingArea declares - x right, y down,
	origin at the top left corner of the surface - so pos() and
	MountedItem::position are the same pair of numbers, and
	mapRectToScene(drawnRect()) and MountedItem::footprint() are the same
	rectangle. Nothing here multiplies by a scale factor: the factor belongs
	to the insertion of the layout into a folio, which is another step and
	another file.

	The part nobody has measured is the case worth reading twice. A branch
	of a third party drew such a part as a box of 20 units, which at its own
	scale is a part of 10 mm that nobody ever bought: it passes a fit check
	and fails on the bench. Here the box drawn for it is a marker - hatched,
	dashed, of a size this class names and owns - and the model is never
	told about it. declaredSize() keeps folding the unknown to zero,
	hasDeclaredSize() keeps saying the check was made on a point, and the
	part stays in every list it belongs to, reported rather than dropped.
	What is on the screen must never become what is in the file.
*/
class MountedPartItem : public QGraphicsObject
{
	Q_OBJECT

	public:
			/// what qgraphicsitem_cast answers about this item
		enum { Type = UserType + 1400 };

		explicit MountedPartItem(const MountedItem &mounted_item,
					 QGraphicsItem *parent = nullptr);
		~MountedPartItem() override;

		int type() const override;

			/// @return the identity the layout stitches everything onto
		QString uuid() const;

		/**
			@return the item as the model holds it, its position read
			back from where it has been dragged to.

			The one way out of this class, and it is a value: whoever
			writes the layout down takes this, never the painter.
		*/
		MountedItem mountedItem() const;

		/**
			@brief Draw another item here, identity included.
			@param mounted_item what is mounted, millimetre

			The position travels with it: what the caller hands in is
			where the item goes, which is what makes this the one
			entry point an undo command needs.

			Virtual because the size travels with it too, and an item
			that draws handles on its own ends has to move them when
			it is resized. The framework does not help there: a
			stretch that leaves the top left corner where it was
			changes no position, so ItemPositionHasChanged never
			fires and the handles would stay behind the drawing.
		*/
		virtual void setMountedItem(const MountedItem &mounted_item);

			/// @return true when both dimensions of the part are known
		bool hasDeclaredSize() const;
			/// @return the room the part takes, millimetre, zero when unmeasured
		QSizeF declaredSize() const;

		/**
			@return what is painted, in the coordinates of this item.

			The footprint for a measured part, and the marker for a
			part nobody has measured - which is exactly the difference
			this class exists to keep on the screen and out of the
			file.
		*/
		QRectF drawnRect() const;

			/// @return where the top left corner sits, millimetre
		QPointF millimetrePosition() const;
			/// @brief Put the top left corner at @a position_mm, millimetre
		void setMillimetrePosition(const QPointF &position_mm);

		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		void paint(QPainter *painter,
			   const QStyleOptionGraphicsItem *option,
			   QWidget *widget = nullptr) override;

		/**
			@return the side of the square drawn for a part nobody has
			measured, millimetre.

			A size to point at something with, not a size anything was
			bought at. It is here, named, so that the day it changes
			nobody has to wonder whether some file was written with the
			old one - no file ever holds it.
		*/
		static qreal unmeasuredMarkerSide();

		/**
			@brief Give the part the 2D drawing of its symbol.
			@param drawing the layout_drawing of the element it is,
			millimetre; a null one puts the plain box back

			The drawing is shown at its real size and never scaled: it
			is drawn in millimetre, at the scale of the panel, to be the
			part as it is. With a declared size it is centred on the
			catalogue box, which stays the room the fit check reads, and
			a size that disagrees with the box is marked on the part and
			said in its tooltip - never corrected. Without one the part
			stays unmeasured: a drawing is not a catalogue entry, and
			the fit check keeps treating the part as a point.
		*/
		void setLayoutDrawing(const ElementPictureFactory::LayoutDrawing &drawing);
		bool hasLayoutDrawing() const;
			/// @return where the drawing is painted, item coordinates
		QRectF layoutDrawingRect() const;
			/// @return true when drawing and box differ, in either
			/// dimension, by more than the larger of drawingTolerance()
			/// of the box and drawingToleranceMm()
		bool drawingSizeDiffers() const;
			/// @return the gap in size that is marked, as a fraction of the box
		static qreal drawingTolerance();
			/// @return the gap in size below which nothing is marked, millimetre
		static qreal drawingToleranceMm();

		/**
			@return the drawings of the parts @a items are, by part
			code, read from the copies @a layout keeps. An item with no
			code, or whose code has no copy, is not in it - and draws
			the box with its name.
		*/
		static QHash<QString, ElementPictureFactory::LayoutDrawing>
		layoutDrawingsOf(const MountingLayout &layout, const QList<MountedItem> &items);

	signals:
		/**
			@brief Emitted when a drag of this item has just ended.
			@param previous_position_mm where it was when the drag began

			The item has already moved itself by then - that is the
			framework doing the dragging - so what is reported is where
			it came from. Whoever listens turns that into one undo
			step; the item deliberately does not push anything itself,
			because which stack this belongs to is not its business.
		*/
		void dragged(const QPointF &previous_position_mm);

	protected:
		void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

	private:
		void updateDrawingToolTip();
		void paintDesignationOver(QPainter *painter, const QRectF &body) const;

		MountedItem m_item;
		QPointF m_press_position;
		bool m_pressed = false;
		ElementPictureFactory::LayoutDrawing m_drawing;
};

#endif // MOUNTEDPARTITEM_H
