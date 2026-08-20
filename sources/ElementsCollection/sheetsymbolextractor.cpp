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
#include "sheetsymbolextractor.h"

#include <algorithm>

#include <QCoreApplication>
#include <QVector>

#include "../diagramcontent.h"
#include "../qetgraphicsitem/independenttextitem.h"
#include "../qetgraphicsitem/qetshapeitem.h"

QString SheetSymbolExtractor::colorName(const QColor &color)
{
		//Only the names the element definition knows, and only the ones a
		//schematic actually uses. An unknown colour falls back to black,
		//which is a symbol that is drawn rather than a symbol that is lost.
	static const QList<QPair<QColor, QString>> named = {
		{QColor(Qt::black),   QStringLiteral("black")},
		{QColor(Qt::white),   QStringLiteral("white")},
		{QColor(Qt::green),   QStringLiteral("green")},
		{QColor(Qt::red),     QStringLiteral("red")},
		{QColor(Qt::blue),    QStringLiteral("blue")},
		{QColor(Qt::gray),    QStringLiteral("gray")},
		{QColor(Qt::yellow),  QStringLiteral("yellow")},
		{QColor(Qt::cyan),    QStringLiteral("cyan")},
		{QColor(Qt::magenta), QStringLiteral("magenta")},
		{QColor(Qt::lightGray), QStringLiteral("lightgray")}
	};
	for (const QPair<QColor, QString> &entry : named) {
		if (entry.first.rgb() == color.rgb()) {
			return entry.second;
		}
	}
	return QString();
}

QString SheetSymbolExtractor::styleOf(const QPen &pen, const QBrush &brush)
{
	QString line_style = QStringLiteral("normal");
	switch (pen.style()) {
		case Qt::DashLine:       line_style = QStringLiteral("dashed"); break;
		case Qt::DotLine:        line_style = QStringLiteral("dotted"); break;
		case Qt::DashDotLine:
		case Qt::DashDotDotLine: line_style = QStringLiteral("dashdotted"); break;
		default: break;
	}

		//The four weights the element definition has, chosen by where the pen
		//width falls. "hight" and "eleve" are spelled the way the file format
		//spells them, not the way English does.
	QString line_weight = QStringLiteral("normal");
	if (pen.style() == Qt::NoPen) {
		line_weight = QStringLiteral("none");
	} else if (pen.widthF() <= 0.5) {
		line_weight = QStringLiteral("thin");
	} else if (pen.widthF() >= 4.0) {
		line_weight = QStringLiteral("eleve");
	} else if (pen.widthF() >= 2.0) {
		line_weight = QStringLiteral("hight");
	}

	QString filling = QStringLiteral("none");
	if (brush.style() != Qt::NoBrush) {
		const QString name = colorName(brush.color());
		if (!name.isEmpty()) {
			filling = name;
		}
	}

	QString color = colorName(pen.color());
	if (color.isEmpty()) {
		color = QStringLiteral("black");
	}

	return QStringLiteral("line-style:%1;line-weight:%2;filling:%3;color:%4")
			.arg(line_style, line_weight, filling, color);
}

SymbolShape SheetSymbolExtractor::shapeOf(const QetShapeItem *item)
{
	SymbolShape shape;
	shape.style = styleOf(item->pen(), item->brush());
	shape.antialias = true;
	shape.closed = item->isClosed();

	switch (item->shapeType()) {
		case QetShapeItem::Line:
			shape.type = SymbolShapeType::Line;
			shape.points << item->mapToScene(item->line().p1())
				     << item->mapToScene(item->line().p2());
			break;
		case QetShapeItem::Rectangle: {
			shape.type = SymbolShapeType::Rectangle;
			const QRectF rect = item->rect().normalized();
			shape.points << item->mapToScene(rect.topLeft())
				     << item->mapToScene(rect.bottomRight());
			break;
		}
		case QetShapeItem::Ellipse: {
			shape.type = SymbolShapeType::Ellipse;
			const QRectF rect = item->rect().normalized();
			shape.points << item->mapToScene(rect.topLeft())
				     << item->mapToScene(rect.bottomRight());
			break;
		}
		case QetShapeItem::Polygon: {
			shape.type = SymbolShapeType::Polygon;
			const QPolygonF polygon = item->polygon();
			for (const QPointF &point : polygon) {
				shape.points << item->mapToScene(point);
			}
			break;
		}
	}
	return shape;
}

QString SheetSymbolExtractor::refusal(const DiagramContent &content)
{
	if (content.m_shapes.isEmpty()) {
		return QCoreApplication::translate("SheetSymbolExtractor",
			"Rien n'est dessiné dans la sélection. Un symbole se fait "
			"avec les outils de dessin : ligne, rectangle, ellipse, "
			"polygone.");
	}
	if (!content.m_elements.isEmpty()) {
		return QCoreApplication::translate("SheetSymbolExtractor",
			"La sélection contient des composants. Un symbole est un "
			"dessin ; un morceau de schéma avec des composants se "
			"garde en groupement.");
	}
	if (!content.conductors().isEmpty()) {
		return QCoreApplication::translate("SheetSymbolExtractor",
			"La sélection contient des conducteurs. Les points de "
			"raccordement du symbole prennent leur place.");
	}
	return QString();
}

SymbolDefinition SheetSymbolExtractor::fromSelection(
		const DiagramContent &content, const SymbolGrid &grid)
{
	SymbolDefinition symbol;

		//Sorted by position so the same drawing always produces the same
		//file: a definition whose shapes come out in a different order on
		//every save is a definition that shows a diff for no reason.
	QList<QetShapeItem *> shapes = content.m_shapes.values();
	std::sort(shapes.begin(), shapes.end(),
		  [](const QetShapeItem *a, const QetShapeItem *b)
	{
		const QPointF pa = a->sceneBoundingRect().topLeft();
		const QPointF pb = b->sceneBoundingRect().topLeft();
		if (!qFuzzyCompare(pa.y() + 1.0, pb.y() + 1.0)) {
			return pa.y() < pb.y();
		}
		return pa.x() < pb.x();
	});
	for (const QetShapeItem *item : shapes) {
		symbol.shapes << shapeOf(item);
	}

		//Free text drawn next to the drawing becomes fixed text of the
		//symbol. It is what a "PE" or a "24 V" written beside a terminal is,
		//and it belongs to the symbol rather than to the sheet.
	QList<IndependentTextItem *> texts = content.m_text_fields.values();
	std::sort(texts.begin(), texts.end(),
		  [](const IndependentTextItem *a, const IndependentTextItem *b)
	{
		if (!qFuzzyCompare(a->scenePos().y() + 1.0, b->scenePos().y() + 1.0)) {
			return a->scenePos().y() < b->scenePos().y();
		}
		return a->scenePos().x() < b->scenePos().x();
	});
	for (IndependentTextItem *item : texts) {
		SymbolText text;
			//Scene coordinates, like the shapes: the whole symbol is read in
			//one coordinate system and translated once, at the end.
		text.position = item->scenePos();
		text.text = item->toPlainText();
		text.rotation = item->rotation();
		symbol.texts << text;
	}

	symbol.terminals = symbol.suggestedTerminals(grid);
	symbol.hotspot = symbol.suggestedHotspot(grid);

		//Every symbol gets the field that shows the tag, placed above the
		//drawing. Without it the component is on the sheet with nothing
		//written next to it, and that is the first thing anyone notices.
	const QRectF box = symbol.bounds();
	symbol.texts.prepend(SymbolText::tagField(
				     QPointF(box.left(), box.top() - 12.0)));

	return symbol;
}
