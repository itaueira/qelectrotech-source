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
#ifndef MOUNTINGLAYOUTVIEWITEM_H
#define MOUNTINGLAYOUTVIEWITEM_H

#include "../../location/mountinglayout.h"
#include "../qetgraphicsitem.h"

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

class MountedPartItem;
class QDomDocument;
class QDomElement;
class QETProject;

/**
	@brief One mounting surface of the project, drawn on a folio.

	The sister of QetGraphicsTableItem, and deliberately built to the same
	shape: an item that shows something the project holds, that is told
	which thing by an identifier rather than by a copy, and that redraws
	itself when that thing changes. A table listens to dataChanged and
	modelReset of its model; this item listens to
	QETProject::mountingLayoutChanged, which is the same signal for a model
	that is not a QAbstractItemModel.

	@par Why the live link is the whole point

	An item that drew the plate once, at the moment it was posed, would be a
	photograph. The project would then hold two answers about one panel -
	the layout, and the picture of the layout as it was that afternoon - and
	nothing would say which one the workshop is looking at. So the item
	stores the identifier of the surface and nothing else about it: every
	rectangle it draws is read from QETProject::mountingLayout() at the
	moment the layout changes, and a face that moved anywhere - in the
	layout window, by an undo, by a change of enclosure - is redrawn here
	without anybody asking.

	@par Nothing drawn here is ever written back

	The folio reads; the layout window writes. That is enforced three ways
	rather than promised once. The part items are built with ItemIsMovable
	and ItemIsSelectable cleared and with no accepted mouse button, so no
	gesture can reach them. This class never calls
	QETProject::setMountingLayout, and holds no mutable copy that anything
	could flush. And what it does own - where it sits on the folio, and at
	which scale it draws - is written into the folio, never into the layout.
	Two owners of one number is the failure this arrangement exists against:
	a plate that could be dragged on the folio would be a plate whose
	millimetre depends on which window you moved it in last.

	@par The scale, and where the conversion lives

	This is the one place in the program where a millimetre becomes a folio
	unit. Everything below - MountingLayout, MountingScene, MountedPartItem -
	counts in millimetre and multiplies by nothing, on purpose: a factor
	stored next to a length is a length nobody can compare with a panel any
	more. The factor appears here because here is where a drawing meets a
	sheet of paper, and it is carried by the item, in the folio, so that two
	views of one plate can sit on two folios at two scales and both be right.

	Do not confuse it with MountingView::pixelsPerMillimetre, which shares
	half a name and is another thing altogether: that one is the zoom of the
	layout window, read back out of the transform of a QGraphicsView and
	stored nowhere. This one is written to the file.

	Inside the item the coordinates are millimetre - so boundingRect(),
	shape() and everything the part items draw are the numbers the layout
	holds - and the factor is applied as the scale transform of the item.
	That is why this class descends from QetGraphicsItem while
	MountedPartItem could not: what QetGraphicsItem::setPos rounds to the
	grid is where the drawing sits on the sheet, which is a folio position
	and is supposed to be on the grid. No millimetre of the panel ever goes
	through it.

	@par What it shows when there is nothing to show

	A folio may name a surface the project no longer has - somebody deleted
	the plate, or the item was pasted into another project. That must not
	take the program down, and must not draw an empty rectangle either,
	which would read as an empty plate. It draws a notice saying what it was
	looking for and did not find, the way a table draws the reason a query
	failed in place of the table it cannot draw.
*/
class MountingLayoutViewItem : public QetGraphicsItem
{
	Q_OBJECT

	Q_PROPERTY(qreal drawingScale READ drawingScale WRITE setDrawingScale)

	public:
			/// what qgraphicsitem_cast answers about this item
		enum { Type = UserType + 1302 };

		explicit MountingLayoutViewItem(QGraphicsItem *parent = nullptr);
		~MountingLayoutViewItem() override;

		int type() const override {return Type;}

		/**
			@brief Read the layout of @a project from now on.
			@param project the project holding the face

			The connection to mountingLayoutChanged is made and unmade
			here, so an item that changed project stops listening to
			the one it left.
		*/
		void setProject(QETProject *project);
		QETProject *project() const;

			/// @brief Show the face of @a surface_uuid
		void setSurfaceUuid(const QString &surface_uuid);
			/// @return which face this item shows
		QString surfaceUuid() const;

			/// @return true when the project holds the face named
		bool hasSurface() const;

		/**
			@return why there is nothing to draw, empty when there is
			something.

			The counterpart of QetGraphicsTableItem::modelError, and a
			sentence rather than a flag because the three ways to get
			here need three different answers.
		*/
		QString error() const;

			/// @return the face as the project holds it, a default one when none
		MountingSurface surface() const;

			/// @return how many parts are drawn
		int partCount() const;
			/// @return the item drawn for @a item_uuid, nullptr when there is none
		MountedPartItem *partItem(const QString &item_uuid) const;
			/// @return the identity of every part drawn, in the order of the face
		QStringList drawnItemUuids() const;

			/// @return how many folio units one millimetre of panel is drawn as
		qreal drawingScale() const;

		/**
			@brief Draw one millimetre of panel as @a folio_units_per_millimetre.
			@param folio_units_per_millimetre the factor

			Clamped rather than refused, because this is reached from a
			file as well as from a person: a factor of zero would draw a
			plate of no size at all, and a negative one would draw it
			inside out.
		*/
		void setDrawingScale(qreal folio_units_per_millimetre);

			/// @return what @a length_mm of panel measures on the folio
		qreal folioLengthFromMillimetre(qreal length_mm) const;
			/// @return what @a length_folio of folio stands for on the panel
		qreal millimetreFromFolioLength(qreal length_folio) const;

			/// @return the room the whole drawing takes on the folio
		QSizeF folioSize() const;

		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		void paint(QPainter *painter,
			   const QStyleOptionGraphicsItem *option,
			   QWidget *widget) override;

		QString name() const override;
		void editProperty() override;

		QDomElement toXml(QDomDocument &document) const;

		/**
			@brief Restore this item from @a element.
			@param element the node written by toXml
			@return true when the node was one of ours

			Tolerant, the way every read of this family is: a missing
			scale is the default one, an unreadable scale is the default
			one, a missing position is the corner the tables use, and a
			face this project does not have leaves the item saying so
			instead of leaving the project unable to open.
		*/
		bool fromXml(const QDomElement &element);

			/// @return the tag one of these is written under
		static QString tagName();

		/**
			@return the factor an item starts life with, folio units per
			millimetre.

			Half a folio unit per millimetre. The number is arbitrary the
			way a default has to be; what is not arbitrary is that it
			fits. The drawable area of a folio of the standard size is
			1020 by 640 folio units, and the commonest mounting plate
			here is 600 by 800 mm: at this factor it is drawn 300 by 400,
			which sits on the sheet with room to spare, while at one unit
			per millimetre it would be 800 tall on a folio 640 tall and
			would hang off the bottom of the paper.

			It is a default and not a policy. Which scale a workshop
			drawing is handed over at, and whether it goes on a sheet of
			its own, is a decision nobody has taken yet - so the factor
			is a property of the item, chosen by hand, and this number is
			only where the hand starts.
		*/
		static qreal defaultDrawingScale();
		static qreal minimumDrawingScale();
		static qreal maximumDrawingScale();

	private:
		void reload();
		void rebuild();
		void clearParts();
			/// @return the sheet metal, empty when nobody measured it
		QRectF plateRect() const;
			/// @return everything drawn of the panel, millimetre
		QRectF contentRect() const;
			/// @return the band the caption is written in, millimetre
		QRectF captionRect() const;
		QString captionText() const;
		void paintNotice(QPainter *painter, const QString &notice);

		QPointer<QETProject> m_project;
		QString m_surface_uuid;
		MountingSurface m_surface;
		bool m_has_surface = false;
		qreal m_drawing_scale = 0.5;
		QVector<MountedPartItem *> m_parts;
};

#endif // MOUNTINGLAYOUTVIEWITEM_H
