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

	@par It is no part of the selection
	A frame is not selected with a rectangle, moved, copied or deleted with
	the components: it is notation round them. Its border and its name
	answer the mouse, a double click to rename it and the right button for
	a menu of its own - rename, delete. The cost: a frame cannot be removed
	with the Delete key.
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
		void terminalsChanged();

		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
			   QWidget *widget) override;
		void editProperty() override;
		void rename(const QString &strip_name);
		void remove();

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

	protected:
		void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
		QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

	private:
		void computeGeometry() const;
		void hideComments();
		void showComments();

		QString m_name;
		QList<QPointer<Element>> m_terminals;
		QList<QMetaObject::Connection> m_connections;
			/// The texts of the comment of its terminals this frame hides
		QList<QPointer<DynamicElementTextItem>> m_hidden;
		mutable QRectF m_frame;
		mutable QRectF m_caption;
		mutable bool m_dirty = true;
};

#endif // STRIPFRAMEITEM_H
