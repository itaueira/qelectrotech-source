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
#ifndef STRIPFRAMEITEM_H
#define STRIPFRAMEITEM_H

#include "qetgraphicsitem.h"

#include <QPointer>
#include <QUuid>

class Diagram;
class DynamicElementTextItem;
class Element;
class QDomDocument;
class QDomElement;
class QUndoCommand;
class QWidget;

/**
	@brief The StripFrameItem class
	The frame drawn round the terminals of a strip on a folio, and the name
	of the strip in it (T85). Nobody draws it: the person chooses the
	terminals, and the frame is worked out from them - round what they
	draw, short of the ends their wires come to -, so it follows them when
	they move, shrinks when one is deleted and is gone with the last.

	@par It keeps its terminals, and not only a name
	A folio may show one strip in two places - the same strip, X1, with two
	of its terminals at the top of the folio and three at the bottom - and
	each place has its frame. The terminals a frame holds are therefore a
	list of its own, and the name, which the terminals carry too in their
	comment, is not what puts a terminal in a frame.

	@par It is selected by its border, and dragging it moves its terminals
	A click on its border or its name selects the frame - and the
	properties dock shows its name, the spacing of its terminals and the
	list of them, where one is picked without hitting it on the folio,
	the terminals of a strip standing too close for the mouse. Dragging
	it moves its terminals with their wires, in one undo step; the frame
	follows them. It is not copied with the components: it is notation
	round them. A double click renames it, and the right button has a menu
	of its own - rename, rotate, delete.

	@par Turned and deleted as one piece
	Selected, the frame is what the Rotate command turns: its terminals
	turn round their middle, together, with their wires - the strip as one
	piece -, and the frame is worked out again round them, its name
	upright, at the left of a strip that lies and at the top of one that
	stands. The Delete key takes the frame off the folio and leaves its
	terminals on it, so that a strip drawn the wrong way round is drawn
	again without placing its terminals again (asked on 23/10/2026).
	Terminals selected without their frame turn and go as they always did.
*/
class StripFrameItem : public QetGraphicsItem
{
	Q_OBJECT

	public:
			///Enable qgraphicsitem_cast. Out of the run the upstream numbers
			///its items in, for the reason LocationAreaItem::Type gives.
		enum { Type = UserType + 1451 };
		explicit StripFrameItem(const QString &strip_name = QString(),
					QGraphicsItem *parent = nullptr);
		~StripFrameItem() override = default;

		int type() const override {return Type;}
		QString name() const override;
		QString stripName() const {return m_name;}
		void setStripName(const QString &strip_name);
		QList<Element *> terminals() const;
		void setTerminals(const QList<Element *> &terminals);
		QRectF frameRect() const;
		QRectF captionRect() const;
		void terminalsChanged();
		QList<Element *> orderedTerminals() const;
		bool lies() const;
		qreal spacing() const;
		void setSpacing(qreal pitch);
		void moveTerminals(const QPointF &movement);

		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
			   QWidget *widget) override;
		void editProperty() override;
		void rename(const QString &strip_name);
		void rotate(qreal angle = 90);
		void remove();
		QUndoCommand *removeCommand();

		QDomElement toXml(QDomDocument &document) const;
		static StripFrameItem *fromXml(const QDomElement &e, const QList<Element *> &elements);
		static QString tagName();
		static QString listTagName();

		static bool isStripTerminal(const Element *element);
		static QList<Element *> stripTerminals(const QList<QGraphicsItem *> &items);
		static StripFrameItem *frameOf(const Element *terminal);
		static void refreshAll(Diagram *diagram);
		static QString suggestedName(const QList<Element *> &terminals);
		static QString askName(QWidget *parent, const QString &suggestion, bool *ok);
		static void drawAround(Diagram *diagram, const QList<Element *> &terminals, const QString &strip_name);
		static void takeOut(Diagram *diagram, const QList<Element *> &terminals);
		static QList<StripFrameItem *> stripFrames(const QList<QGraphicsItem *> &items);
		static bool rotateSelection(Diagram *diagram, qreal angle = 90);
		static QUndoCommand *deleteCommand(const QList<StripFrameItem *> &frames,
						   const QList<QUndoCommand *> &others);

	signals:
			/// Its name, its terminals or where they are changed
		void frameChanged();

	protected:
		void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
		QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
		void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

	private:
		void computeGeometry() const;
		void turnTerminals(qreal angle, QUndoCommand *undo);
		void hideComments();
		void showComments();
		void selectTerminals();
		void selectSelf();

		QString m_name;
		QList<QPointer<Element>> m_terminals;
		QList<QMetaObject::Connection> m_connections;
			/// The texts of the comment of its terminals this frame hides
		QList<QPointer<DynamicElementTextItem>> m_hidden;
		mutable QRectF m_frame;
		mutable QRectF m_caption;
		mutable bool m_dirty = true;
			/// A drag of the frame: where it started, and the terminal whose
			/// place on the grid the others keep their distance from
		bool m_dragging = false;
		QPointF m_drag_from;
		QPointer<Element> m_anchor;
		QPointF m_anchor_from;
			/// The point the last turn went round, and where it left the
			/// terminals: turned again with none of them moved in between,
			/// the strip turns round that same point (turnTerminals())
		QPointF m_turn_pivot;
		QList<QPointF> m_turned_to;
};

#endif // STRIPFRAMEITEM_H
