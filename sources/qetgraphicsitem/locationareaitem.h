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
#ifndef LOCATIONAREAITEM_H
#define LOCATIONAREAITEM_H

#include "../location/locationcontainment.h"
#include "../undocommand/assignlocationcommand.h"
#include "qetshapeitem.h"

class Diagram;
class QWidget;

/**
	@brief The LocationAreaItem class
	A rectangle drawn on a folio, saying that whatever sits inside it is
	mounted in one location.

	It is a QetShapeItem built as a Rectangle, and that is not laziness: the
	handles, the resizing, the pen, the brush and the selection feedback of
	the drawn shapes are exactly what an area needs, and they are worth more
	inherited than rewritten.

	@par Why it carries a type of its own

	The class overrides type() with a value nothing else uses, and that one
	line is what keeps an area out of every path that handles shapes. Every
	dispatch in the program is a switch on item->type() - the folio content,
	the copy, the properties editor - and not one of them reaches an item by
	dynamic_cast, so to all of them an area is simply not a shape: it is not
	offered in the shape style editor, not counted as a shape in a
	selection, not carried into the DXF export.

	The cost is stated plainly, because it is what will surprise whoever
	comes next. The isolation rests on a convention and not on the type
	system: the day somebody writes dynamic_cast<QetShapeItem *> somewhere,
	an area becomes a shape again in that one place, silently. The other
	half of the cost is the mirror image - a new folio-wide feature written
	as a switch will not know areas exist until somebody adds the case.

	@par The area holds a path, and nothing else

	It never keeps a list of the components inside it. That list is derived
	from the geometry, by the pure rule in locationcontainment, on every
	query. A stored list would be a second answer to the same question, and
	it would go stale in silence the first time somebody dragged a component
	one millimetre.
*/
class LocationAreaItem : public QetShapeItem
{
	Q_OBJECT

	Q_PROPERTY(QString locationPath
		   READ locationPath
		   WRITE setLocationPath
		   NOTIFY locationPathChanged)

	signals:
		void locationPathChanged();

	public:
			///Enable qgraphicsitem_cast, and keep areas out of the shape paths
		enum { Type = UserType + 1012 };

		explicit LocationAreaItem(const QPointF &p1,
					  const QPointF &p2 = QPointF(),
					  QGraphicsItem *parent = nullptr);
		~LocationAreaItem() override = default;

		int type() const override { return Type; }

		QString locationPath() const { return m_location_path; }
		void setLocationPath(const QString &path);

		QRectF sceneRect() const;

		QString name() const override;
		void editProperty() override;

		QDomElement toXml(QDomDocument &document) const override;
		bool fromXml(const QDomElement &e) override;

		QPainterPath shape() const override;

			///The tag an area is written under, so that the readers and the
			///writer never spell it apart
		static QString tagName();

		static QVector<LocationArea> areasOf(const Diagram *diagram);
		static QList<LocationAssignment> pendingAssignments(
				const Diagram *diagram,
				const QVector<LocationArea> &areas);
		static QString askForPath(const Diagram *diagram,
					  const QString &current,
					  QWidget *parent,
					  bool *accepted);

	protected:
		void paint(QPainter *painter,
			   const QStyleOptionGraphicsItem *option,
			   QWidget *widget) override;
		void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

	private:
		QRectF captionRect() const;
		QString captionText() const;

		///ATTRIBUTES
	private:
		QString m_location_path;
};

#endif // LOCATIONAREAITEM_H
