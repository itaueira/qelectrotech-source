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
#ifndef TERMINALBUSITEM_H
#define TERMINALBUSITEM_H

#include "qetgraphicsitem.h"

#include <QLineF>
#include <QPolygonF>
#include <QSharedPointer>
#include <QVector>
#include <QWeakPointer>

class Conductor;
class QColor;
class Diagram;
class Element;
class RealTerminal;
class TerminalStripBridge;

/**
	@brief The TerminalBusItem class
	A bus bar on a folio (T87): the bridge of a terminal strip - the comb
	or jumper the fitter puts on the terminals - drawn between the
	terminals it joins that are on the folio. One bridge, seen in two
	places: here, and on the strip the strip manager draws.

	@par It is drawn from the bridge, and kept nowhere
	The bridge, with the wire that feeds it and the side of the strip it is
	drawn by, is kept with its strip in the project. The folio makes one of
	these for each bridge that joins two or more of its terminals, and
	drops it when the bridge goes (syncFolio()): nothing about it is
	written in the folio.

	@par What it draws
	A trunk along the strip, off the ends of the terminals, and a tooth
	from each joined terminal to the trunk - the trunk wider than the
	teeth, and both wider than a wire, so that a black and white print
	tells them from the wires (the Renan's choice). A filled triangle on
	the wire that feeds it, pointing into it. A wire crossing the trunk
	hops over it (trunksOn()).
*/
class TerminalBusItem : public QetGraphicsItem
{
	Q_OBJECT

	public:
			///Enable qgraphicsitem_cast. Out of the run the upstream numbers
			///its items in, for the reason LocationAreaItem::Type gives.
		enum { Type = UserType + 1452 };
		explicit TerminalBusItem(const QSharedPointer<TerminalStripBridge> &bridge,
					 QGraphicsItem *parent = nullptr);
		~TerminalBusItem() override = default;

		int type() const override {return Type;}
		QString name() const override;
		QSharedPointer<TerminalStripBridge> bridge() const;
		QList<Element *> terminals() const;
		int side() const;
		Conductor *feed() const;
		QList<Conductor *> wires() const;
		QLineF trunk() const;
		QVector<QLineF> teeth() const;
		QPolygonF feedMark() const;
		void terminalsChanged();

		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
			   QWidget *widget) override;

		static QSharedPointer<RealTerminal> realTerminalOf(const Element *element);
		static QList<TerminalBusItem *> busesOn(const QGraphicsScene *scene);
		static QList<QVector<QPointF>> trunksOn(const QGraphicsScene *scene);
		static void syncFolio(Diagram *diagram);
		static QString nameForNewStrip(const QList<Element *> &selection);
		static bool needsStripName(const QList<Element *> &selection);
		static bool join(Diagram *diagram, const QList<Element *> &selection,
				 const QString &new_strip_name, QString *message = nullptr);
		static QList<Element *> joinable(const QList<QGraphicsItem *> &selection);
		static void setColor(TerminalBusItem *bus, const QColor &color);
		static void setFeed(TerminalBusItem *bus, Conductor *conductor);
		static void remove(TerminalBusItem *bus);

	protected:
		void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
		QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

	private:
		void computeGeometry() const;
		void watchTerminals();

		QWeakPointer<TerminalStripBridge> m_bridge;
		QList<QMetaObject::Connection> m_connections;
		mutable bool m_dirty = true;
		mutable QLineF m_trunk;
		mutable QVector<QLineF> m_teeth;
		mutable QPolygonF m_feed_mark;
};

#endif // TERMINALBUSITEM_H
