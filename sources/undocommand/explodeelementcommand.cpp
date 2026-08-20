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
#include "explodeelementcommand.h"

#include <QCoreApplication>
#include <QGraphicsItem>
#include <QTransform>

#include "../ElementsCollection/elementslocation.h"
#include "../ElementsCollection/sheetsymbolextractor.h"
#include "../ElementsCollection/symbolbuilder.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/independenttextitem.h"
#include "../qetgraphicsitem/qetshapeitem.h"

namespace
{
	/**
		The pieces one component becomes, in scene coordinates.

		Read from the definition of the symbol rather than from what is on
		the sheet, because what is on the sheet is a painted picture: the
		element draws itself from a cached QPicture and holds no shape
		objects. The definition is where the lines still exist as lines.
	*/
	QList<QGraphicsItem *> piecesOf(Element *element)
	{
		QList<QGraphicsItem *> pieces;
		if (!element) {
			return pieces;
		}

		const QDomElement definition = element->location().xml();
		if (definition.isNull()) {
			return pieces;
		}
		const SymbolDefinition symbol = SymbolDefinition::fromXml(definition);
		if (symbol.shapes.isEmpty() && symbol.texts.isEmpty()) {
			return pieces;
		}

			//The definition places everything around its insertion point;
			//fromXml() gives it back relative to that point inside the box of
			//the file. Undo that, then put the whole thing where the component
			//stands, turned the way the component is turned.
		QTransform transform;
		transform.translate(element->pos().x(), element->pos().y());
		transform.rotate(element->rotation());
		const QPointF hotspot = symbol.hotspot;

		for (const SymbolShape &shape : symbol.shapes)
		{
			if (!shape.isValid()) {
				continue;
			}
			QPolygonF points;
			for (const QPointF &point : shape.points) {
				points << transform.map(point - hotspot);
			}

			QetShapeItem::ShapeType type = QetShapeItem::Line;
			switch (shape.type) {
				case SymbolShapeType::Line:
					type = QetShapeItem::Line; break;
				case SymbolShapeType::Rectangle:
					type = QetShapeItem::Rectangle; break;
				case SymbolShapeType::Ellipse:
					type = QetShapeItem::Ellipse; break;
				case SymbolShapeType::Polygon:
					type = QetShapeItem::Polygon; break;
			}

			QetShapeItem *item = new QetShapeItem(points.value(0),
							      points.value(1),
							      type);
			if (shape.type == SymbolShapeType::Polygon) {
				item->setPolygon(points);
				item->setClosed(shape.closed);
			}
			item->setPen(SheetSymbolExtractor::penFor(shape.style));
			item->setBrush(SheetSymbolExtractor::brushFor(shape.style));
			pieces << item;
		}

		for (const SymbolText &text : symbol.texts)
		{
				//Only the fixed texts. A field bound to component
				//information has nothing to be bound to once the component
				//is gone, and putting its current value on the sheet as
				//loose text would turn a live field into a stale string.
			if (!text.info_key.isEmpty() || text.text.isEmpty()) {
				continue;
			}
			IndependentTextItem *item = new IndependentTextItem(text.text);
			item->setPos(transform.map(text.position - hotspot));
			item->setRotation(element->rotation() + text.rotation);
			pieces << item;
		}

		return pieces;
	}
}

ExplodeElementCommand::ExplodeElementCommand(const QList<Element *> &elements,
					     QUndoCommand *parent) :
	QUndoCommand(parent)
{
	for (Element *element : elements)
	{
		if (!element || !refusal(element).isEmpty()) {
			continue;
		}
		Explosion explosion;
		explosion.element = element;
		explosion.diagram = element->diagram();
		explosion.pieces = piecesOf(element);
		if (explosion.pieces.isEmpty() || !explosion.diagram) {
				//Nothing came out, so nothing is put in: exploding a symbol
				//into no drawing at all would only lose the component.
			for (QGraphicsItem *piece : explosion.pieces) {
				delete piece;
			}
			continue;
		}
		m_explosions << explosion;
	}

	setText(QCoreApplication::translate("ExplodeElementCommand",
		"éclater %n symbole(s) en dessin", "", m_explosions.size()));
}

ExplodeElementCommand::~ExplodeElementCommand()
{
		//Whoever is not on a scene right now is ours to free. After a redo
		//the scene owns the pieces and still holds the component out of it;
		//after an undo it is the other way round.
	for (const Explosion &explosion : m_explosions)
	{
		if (m_exploded) {
			delete explosion.element.data();
		} else {
			for (QGraphicsItem *piece : explosion.pieces) {
				delete piece;
			}
		}
	}
}

int ExplodeElementCommand::elementCount() const
{
	return m_explosions.size();
}

int ExplodeElementCommand::pieceCount() const
{
	int count = 0;
	for (const Explosion &explosion : m_explosions) {
		count += explosion.pieces.size();
	}
	return count;
}

bool ExplodeElementCommand::isEmpty() const
{
	return m_explosions.isEmpty();
}

QString ExplodeElementCommand::refusal(Element *element)
{
	if (!element) {
		return QCoreApplication::translate("ExplodeElementCommand",
			"Rien à éclater.");
	}
	if (!element->conductors().isEmpty()) {
		return QCoreApplication::translate("ExplodeElementCommand",
			"« %1 » a des conducteurs branchés. Éclaté, il n'aurait plus de "
			"points de raccordement et les conducteurs ne tiendraient plus à "
			"rien : débranchez-les d'abord.")
				.arg(element->actualLabel());
	}
	if (element->location().xml().isNull()) {
		return QCoreApplication::translate("ExplodeElementCommand",
			"La définition de « %1 » n'a pas pu être lue.")
				.arg(element->actualLabel());
	}
	return QString();
}

void ExplodeElementCommand::redo()
{
	for (const Explosion &explosion : m_explosions)
	{
		if (!explosion.diagram) {
			continue;
		}
		for (QGraphicsItem *piece : explosion.pieces) {
			explosion.diagram->addItem(piece);
		}
		if (explosion.element) {
			explosion.diagram->removeItem(explosion.element.data());
		}
	}
	m_exploded = true;
}

void ExplodeElementCommand::undo()
{
	for (const Explosion &explosion : m_explosions)
	{
		if (!explosion.diagram) {
			continue;
		}
		for (QGraphicsItem *piece : explosion.pieces) {
			explosion.diagram->removeItem(piece);
		}
		if (explosion.element) {
			explosion.diagram->addItem(explosion.element.data());
		}
	}
	m_exploded = false;
}
