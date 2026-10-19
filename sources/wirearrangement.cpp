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
#include "wirearrangement.h"

#include "conductorsegment.h"
#include "diagram.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/qetshapeitem.h"
#include "undocommand/arrangeconductorscommand.h"
#include "wirelanes.h"

#include <QSet>
#include <QSettings>

#include <cmath>

namespace
{
		/// Kept clear around the body of a symbol, as ConductorRouter keeps
	constexpr double symbol_margin = 5.0;

	const char *setting_key = "diagrameditor/arrange_new_conductors";

		/// A conductor whose automatic path has a bridge, in the terms of
		/// WireLanes
	struct Described
	{
		Conductor *conductor = nullptr;
			/// The automatic path, in scene coordinates
		QList<QPointF> automatic;
			/// The bridge runs up and down, and its x is chosen; else it
			/// runs left and right, and its y is chosen
		bool vertical = true;
		WireLanes::Wire wire;
	};

	bool near(double a, double b) { return std::abs(a - b) < 0.01; }

		/// The path of @a c, in scene coordinates: the automatic one when
		/// it has none of its own - also for a conductor about to be added,
		/// whose segments are not built yet
	QList<QPointF> currentPoints(const Conductor *c)
	{
		if (!c->hasSavedPath()) return c->automaticPathPoints();
		QList<QPointF> points;
		const QList<ConductorSegment *> segments = c->segmentsList();
		for (ConductorSegment *segment : segments)
		{
			if (points.isEmpty()) points << c->mapToScene(segment->firstPoint());
			points << c->mapToScene(segment->secondPoint());
		}
		return points;
	}

	bool samePath(const QList<QPointF> &a, const QList<QPointF> &b)
	{
		if (a.size() != b.size()) return false;
		for (int i = 0 ; i < a.size() ; ++i) {
			if (std::abs(a.at(i).x() - b.at(i).x()) > 0.5
				|| std::abs(a.at(i).y() - b.at(i).y()) > 0.5) {
				return false;
			}
		}
		return true;
	}

		/// The automatic path of @a c is: terminal, its end, the two ends
		/// of the bridge, the other terminal's end, the other terminal.
		/// The two runs join the terminal ends to the bridge.
	bool describe(Conductor *c, Described &out)
	{
		const QList<QPointF> p = c->automaticPathPoints();
		if (p.size() != 6) return false;
		const QPointF a = p.at(1), b1 = p.at(2), b2 = p.at(3), z = p.at(4);
		out.conductor = c;
		out.automatic = p;
		if (near(b1.x(), b2.x()) && near(a.y(), b1.y()) && near(z.y(), b2.y()))
		{
			out.vertical = true;
			const bool a_low = a.x() <= z.x();
			out.wire.low = std::min(a.x(), z.x());
			out.wire.high = std::max(a.x(), z.x());
			out.wire.from = a_low ? a.y() : z.y();
			out.wire.to = a_low ? z.y() : a.y();
			return true;
		}
		if (near(b1.y(), b2.y()) && near(a.x(), b1.x()) && near(z.x(), b2.x()))
		{
			out.vertical = false;
			const bool a_low = a.y() <= z.y();
			out.wire.low = std::min(a.y(), z.y());
			out.wire.high = std::max(a.y(), z.y());
			out.wire.from = a_low ? a.x() : z.x();
			out.wire.to = a_low ? z.x() : a.x();
			return true;
		}
		return false;
	}

	QList<QPointF> withBridgeAt(const Described &d, double at)
	{
		QList<QPointF> p = d.automatic;
		if (d.vertical) {
			p[2].setX(at);
			p[3].setX(at);
		} else {
			p[2].setY(at);
			p[3].setY(at);
		}
		return p;
	}

		/// The box of @a shape as drawn, its pen included, in scene
		/// coordinates. Not its bounding rect: that one is wider by the room
		/// its handles take when it is selected, and wider again while the
		/// mouse is over it.
	QRectF drawnRect(const QetShapeItem *shape)
	{
		const double pen = shape->pen().widthF() / 2;
		QRectF r;
		switch (shape->shapeType())
		{
			case QetShapeItem::Rectangle:
			case QetShapeItem::Ellipse:
				r = shape->mapRectToScene(shape->rect().normalized());
				break;
			case QetShapeItem::Polygon:
				r = shape->mapRectToScene(shape->polygon().boundingRect());
				break;
			default:
				return shape->sceneBoundingRect().adjusted(6, 6, -6, -6);
		}
		return r.adjusted(-pen, -pen, pen, pen);
	}

	bool placedByHand(const Conductor *c)
	{
		return c->hasSavedPath() && !c->isAutoArranged();
	}

		/// The segments of a conductor that stays, in the way of the
		/// bridges that would run along them or end next to them
	void addInTheWay(const QList<QPointF> &points, WireLanes::Request &vertical,
			 WireLanes::Request &horizontal)
	{
		for (int i = 0 ; i + 1 < points.size() ; ++i)
		{
			const QPointF a = points.at(i), b = points.at(i + 1);
			if (std::abs(a.x() - b.x()) < 0.5 && std::abs(a.y() - b.y()) >= 0.5)
			{
				vertical.blocks << WireLanes::Block{
					a.x() - vertical.min_pitch, a.x() + vertical.min_pitch,
					std::min(a.y(), b.y()) - vertical.gap, std::max(a.y(), b.y()) + vertical.gap};
			}
			else if (std::abs(a.y() - b.y()) < 0.5 && std::abs(a.x() - b.x()) >= 0.5)
			{
				horizontal.blocks << WireLanes::Block{
					a.y() - horizontal.min_pitch, a.y() + horizontal.min_pitch,
					std::min(a.x(), b.x()) - horizontal.gap, std::max(a.x(), b.x()) + horizontal.gap};
			}
		}
	}
}

/**
	@brief WireArrangement::plan
	Where the bridges of the conductors of @a diagram go (WireLanes), for
	those @a scope says may move. @a conductors may hold conductors not on
	the folio yet - a conductor about to be added by an undo command.
	@return the conductors whose path changes, with the new path
*/
QList<WireArrangement::Change> WireArrangement::plan(Diagram *diagram,
						      const QList<Conductor *> &conductors,
						      Scope scope)
{
	QList<Change> changes;
	if (!diagram) return changes;

	QList<Conductor *> all = diagram->conductors();
	for (Conductor *c : conductors) {
		if (!all.contains(c)) all << c;
	}

	QSet<Conductor *> movable;
	for (Conductor *c : all)
	{
		const bool given = conductors.contains(c);
		switch (scope)
		{
			case Scope::NewConductors: if (given || !placedByHand(c)) movable << c; break;
			case Scope::Selection:     if (given) movable << c; break;
			case Scope::Folio:         if (!placedByHand(c)) movable << c; break;
		}
	}

	WireLanes::Request vertical, horizontal;
	vertical.grid = Diagram::xGrid;
	horizontal.grid = Diagram::yGrid;

	QList<Described> vertical_wires, horizontal_wires;
	for (Conductor *c : all)
	{
		Described d;
		if (movable.contains(c) && describe(c, d))
		{
			if (d.vertical) {
				vertical_wires << d;
				vertical.wires << d.wire;
			} else {
				horizontal_wires << d;
				horizontal.wires << d.wire;
			}
			continue;
		}
		addInTheWay(currentPoints(c), vertical, horizontal);
	}
	if (vertical_wires.isEmpty() && horizontal_wires.isEmpty()) return changes;

	const QList<Element *> elements = diagram->elements();
	for (Element *e : elements)
	{
		const QRectF r = e->mapRectToScene(e->boundingRect())
				.adjusted(-symbol_margin, -symbol_margin, symbol_margin, symbol_margin);
		vertical.blocks << WireLanes::Block{r.left(), r.right(), r.top(), r.bottom()};
		horizontal.blocks << WireLanes::Block{r.top(), r.bottom(), r.left(), r.right()};
	}

	const QList<QGraphicsItem *> items = diagram->items();
	for (QGraphicsItem *item : items)
	{
		auto *shape = qgraphicsitem_cast<QetShapeItem *>(item);
		if (!shape) continue;
		if (shape->shapeType() == QetShapeItem::Line)
		{
			const QPointF a = shape->mapToScene(shape->line().p1());
			const QPointF b = shape->mapToScene(shape->line().p2());
			if (std::abs(a.x() - b.x()) < 0.5) {
				vertical.frames << WireLanes::Frame{a.x(), a.x(), a.y(), b.y()};
			} else if (std::abs(a.y() - b.y()) < 0.5) {
				horizontal.frames << WireLanes::Frame{a.y(), a.y(), a.x(), b.x()};
			}
			continue;
		}
		const QRectF r = drawnRect(shape);
		vertical.frames << WireLanes::Frame{r.left(), r.right(), r.top(), r.bottom()};
		horizontal.frames << WireLanes::Frame{r.top(), r.bottom(), r.left(), r.right()};
	}

	const auto collect = [&](const QList<Described> &described, const WireLanes::Request &request) {
		const WireLanes::Result result = WireLanes::arrange(request);
		QSet<int> wanted;
		if (scope == Scope::NewConductors) {
			for (int i = 0 ; i < described.size() ; ++i) {
				if (conductors.contains(described.at(i).conductor) && result.group.at(i) >= 0) {
					wanted << result.group.at(i);
				}
			}
		}
		for (int i = 0 ; i < described.size() ; ++i)
		{
			if (std::isnan(result.at.at(i))) continue;
			if (scope == Scope::NewConductors && !wanted.contains(result.group.at(i))) continue;
			const QList<QPointF> points = withBridgeAt(described.at(i), result.at.at(i));
			if (!samePath(points, currentPoints(described.at(i).conductor))) {
				changes << Change{described.at(i).conductor, points};
			}
		}
	};
	collect(vertical_wires, vertical);
	collect(horizontal_wires, horizontal);
	return changes;
}

/**
	@brief WireArrangement::arrange
	Arrange the conductors of @a diagram that @a scope says, as one step on
	its undo stack.
	@return how many conductors changed
*/
int WireArrangement::arrange(Diagram *diagram, const QList<Conductor *> &conductors, Scope scope)
{
	if (!diagram) return 0;
	const QList<Change> changes = plan(diagram, conductors, scope);
	if (changes.isEmpty()) return 0;
	diagram->undoStack().push(new ArrangeConductorsCommand(diagram, changes));
	return changes.size();
}

/**
	@brief WireArrangement::arrangesNewConductors
	@return true if a conductor drawn by the user is arranged, with its
	group, as it is drawn. On unless switched off in the configuration.
*/
bool WireArrangement::arrangesNewConductors()
{
	QSettings settings;
	return settings.value(setting_key, true).toBool();
}

void WireArrangement::setArrangesNewConductors(bool arrange)
{
	QSettings settings;
	settings.setValue(setting_key, arrange);
}

/**
	@brief WireArrangement::arrangeNewConductors
	The undo command that arranges @a conductors, just drawn on @a diagram,
	with their groups - a child of @a parent, so that one undo removes the
	conductors and puts the others back.
	@return the command, or nullptr if arranging is switched off or nothing
	moves
*/
QUndoCommand *WireArrangement::arrangeNewConductors(Diagram *diagram,
						    const QList<Conductor *> &conductors,
						    QUndoCommand *parent)
{
	if (!arrangesNewConductors()) return nullptr;
	const QList<Change> changes = plan(diagram, conductors, Scope::NewConductors);
	if (changes.isEmpty()) return nullptr;
	return new ArrangeConductorsCommand(diagram, changes, parent);
}
