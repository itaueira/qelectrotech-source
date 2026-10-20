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
#include "qetgraphicsitem/terminal.h"
#include "undocommand/arrangeconductorscommand.h"
#include "wirelanes.h"

#include <QPair>
#include <QSet>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace
{
		/// Kept clear around the body of a symbol, as ConductorRouter keeps
	constexpr double symbol_margin = 5.0;

		/// How far a run may go out of its terminal before it leaves it to
		/// the side, at most: two of the gaps between bridges in conflict
	constexpr double longest_stub = 20.0;

	const char *setting_key = "diagrameditor/arrange_conductors";

		/// The conductor between a terminal that points up or down - of a
		/// component - and one that points to the side, towards it - of a
		/// terminal strip: it leaves the first to the side, at the height of
		/// the run, goes up or down at the bridge, and arrives at the second
		/// along its line. Its shape is the arrangement's to choose.
	struct Shaped
	{
		Terminal *component = nullptr;
		Terminal *strip = nullptr;
			/// The end of the terminal that points up or down
		QPointF start;
			/// Where the other terminal's own piece ends, on the automatic
			/// path, and the end of that terminal
		QPointF stub;
		QPointF end;
			/// 1 for a terminal that points down, -1 for one that points up
		double outward = 1;
			/// The terminal that points up or down is terminal2
		bool reversed = false;
			/// The across coordinate of the run that leaves it
		double run = 0;
	};

		/// A conductor whose automatic path has a bridge, in the terms of
		/// WireLanes, or whose shape is chosen here
	struct Described
	{
		Conductor *conductor = nullptr;
			/// The automatic path, in scene coordinates
		QList<QPointF> automatic;
			/// The bridge runs up and down, and its x is chosen; else it
			/// runs left and right, and its y is chosen
		bool vertical = true;
		WireLanes::Wire wire;
		bool shaped = false;
		Shaped shape;
	};

	bool near(double a, double b) { return std::abs(a - b) < 0.01; }

	bool pointsUpOrDown(const Terminal *t)
	{
		return t->orientation() == Qet::North || t->orientation() == Qet::South;
	}

		/// The room and the two across coordinates of @a d, its run where
		/// d.shape says
	void shapeWire(Described &d)
	{
		const Shaped &s = d.shape;
		const bool start_low = s.start.x() <= s.stub.x();
		d.wire.low = std::min(s.start.x(), s.stub.x());
		d.wire.high = std::max(s.start.x(), s.stub.x());
		d.wire.from = start_low ? s.run : s.end.y();
		d.wire.to = start_low ? s.end.y() : s.run;
	}

		/// Whether @a c is a conductor whose shape is chosen here (Shaped)
	bool describeShaped(Conductor *c, Described &out)
	{
		Terminal *v = c->terminal1;
		Terminal *h = c->terminal2;
		if (!v || !h) return false;
		if (!pointsUpOrDown(v)) std::swap(v, h);
		if (!pointsUpOrDown(v) || pointsUpOrDown(h)) return false;

		const QList<QPointF> p = c->automaticPathPoints();
		if (p.size() < 3) return false;
		const bool h_first = h == c->terminal1;
		Shaped s;
		s.component = v;
		s.strip = h;
		s.start = v->dockConductor();
		s.end = h->dockConductor();
		s.stub = h_first ? p.at(1) : p.at(p.size() - 2);
		s.outward = v->orientation() == Qet::South ? 1 : -1;
		s.reversed = h_first;
		s.run = s.start.y();
			//The strip's own piece points towards the component, which is
			//beyond it, at another height
		const double side = h->orientation() == Qet::West ? -1 : 1;
		if (!near(s.stub.y(), s.end.y()) || (s.stub.x() - s.end.x()) * side < 0.5
			|| (s.start.x() - s.stub.x()) * side < 1
			|| std::abs(s.start.y() - s.end.y()) < 1) {
			return false;
		}
		out.conductor = c;
		out.automatic = p;
		out.vertical = true;
		out.shaped = true;
		out.shape = s;
		shapeWire(out);
		return true;
	}

		/// The run of @a d as the automatic path has it: at the end of the
		/// piece of its terminal
	double automaticRun(const Described &d)
	{
		const QList<QPointF> &p = d.automatic;
		const QPointF q = d.shape.reversed ? p.at(p.size() - 2) : p.at(1);
		if (near(q.x(), d.shape.start.x())) return q.y();
		return d.shape.start.y() + d.shape.outward * 10;
	}

		/// The path of @a s with its bridge at @a lane, or with one bend
		/// when @a lane is NaN, from terminal1 to terminal2
	QList<QPointF> shapedPath(const Shaped &s, double lane)
	{
		QList<QPointF> p;
		p << s.start;
		if (std::isnan(lane)) {
			p << QPointF(s.start.x(), s.end.y());
		} else {
			if (!near(s.run, s.start.y())) p << QPointF(s.start.x(), s.run);
			p << QPointF(lane, s.run) << QPointF(lane, s.end.y());
		}
		p << s.stub << s.end;
		if (s.reversed) std::reverse(p.begin(), p.end());
		return p;
	}

		/// What the run of a shaped conductor keeps clear of, read from the
		/// folio, each thing with its owner - a conductor, an element or a
		/// terminal -, never held against itself
	struct Scene
	{
		struct Owned
		{
			WireLanes::Line line;
			const void *owner;
		};
			/// Pieces of conductors, and runs to come: at the y of the
			/// ones that run left and right, at the x of the others
		QVector<Owned> horizontal;
		QVector<Owned> vertical;
			/// The sides of the shapes, which a run may cross but not
			/// follow
		QVector<WireLanes::Line> frame_horizontal;
		QVector<WireLanes::Line> frame_vertical;
		QVector<QPair<WireLanes::Spot, const void *>> terminals;
		QVector<QPair<QRectF, const void *>> bodies;

		void addPath(const QList<QPointF> &points, const void *owner)
		{
			for (int i = 0 ; i + 1 < points.size() ; ++i)
			{
				const QPointF a = points.at(i), b = points.at(i + 1);
				if (std::abs(a.y() - b.y()) < 0.5 && std::abs(a.x() - b.x()) >= 0.5) {
					horizontal << Owned{{a.y(), a.x(), b.x()}, owner};
				} else if (std::abs(a.x() - b.x()) < 0.5 && std::abs(a.y() - b.y()) >= 0.5) {
					vertical << Owned{{a.x(), a.y(), b.y()}, owner};
				}
			}
		}

			/// What a piece running left and right keeps clear of - with
			/// @a up_and_down, one running up and down, the axes swapped
		WireLanes::Clearance clearance(const QSet<const void *> &own, bool up_and_down) const
		{
			WireLanes::Clearance c;
			for (const Owned &o : horizontal) {
				if (!own.contains(o.owner)) (up_and_down ? c.crossing : c.parallel) << o.line;
			}
			for (const Owned &o : vertical) {
				if (!own.contains(o.owner)) (up_and_down ? c.parallel : c.crossing) << o.line;
			}
			c.parallel << (up_and_down ? frame_vertical : frame_horizontal);
			for (const auto &t : terminals)
			{
				if (own.contains(t.second)) continue;
				c.spots << (up_and_down ? WireLanes::Spot{t.first.across, t.first.along} : t.first);
			}
			for (const auto &b : bodies)
			{
				if (own.contains(b.second)) continue;
				const QRectF &r = b.first;
				c.bodies << (up_and_down ? WireLanes::Block{r.top(), r.bottom(), r.left(), r.right()}
							 : WireLanes::Block{r.left(), r.right(), r.top(), r.bottom()});
			}
			return c;
		}
	};

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
	those @a scope says may move, and the shape of those between a terminal
	that points up or down and one that points to the side (Shaped): the
	person who asked for it saw such conductors drawn over the terminal of
	the next sensor down, crossing each other, and running along the side
	of a frame, and wanted them short and untangled. @a conductors may hold
	conductors not on the folio yet - a conductor about to be added by an
	undo command.
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
			case Scope::Folio:
			case Scope::Moved:         if (!placedByHand(c)) movable << c; break;
		}
	}

	WireLanes::Request vertical, horizontal;
	Scene scene;
	const bool only_their_groups = scope == Scope::NewConductors || scope == Scope::Moved;

	QList<Described> vertical_wires, horizontal_wires, shaped;
	for (Conductor *c : all)
	{
		Described d;
		if (movable.contains(c) && describeShaped(c, d))
		{
			shaped << d;
			continue;
		}
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
		const QList<QPointF> points = currentPoints(c);
		addInTheWay(points, vertical, horizontal);
		scene.addPath(points, c);
	}
	if (vertical_wires.isEmpty() && horizontal_wires.isEmpty() && shaped.isEmpty()) return changes;

	const QList<Element *> elements = diagram->elements();
	for (Element *e : elements)
	{
		const QRectF r = e->mapRectToScene(e->boundingRect())
				.adjusted(-symbol_margin, -symbol_margin, symbol_margin, symbol_margin);
		vertical.blocks << WireLanes::Block{r.left(), r.right(), r.top(), r.bottom()};
		horizontal.blocks << WireLanes::Block{r.top(), r.bottom(), r.left(), r.right()};
		scene.bodies << qMakePair(r, static_cast<const void *>(e));
		const QList<Terminal *> terminals = e->terminals();
		for (Terminal *t : terminals)
		{
			const QPointF p = t->dockConductor();
			scene.terminals << qMakePair(WireLanes::Spot{p.x(), p.y()}, static_cast<const void *>(t));
		}
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
				scene.frame_vertical << WireLanes::Line{a.x(), a.y(), b.y()};
			} else if (std::abs(a.y() - b.y()) < 0.5) {
				horizontal.frames << WireLanes::Frame{a.y(), a.y(), a.x(), b.x()};
				scene.frame_horizontal << WireLanes::Line{a.y(), a.x(), b.x()};
			}
			continue;
		}
		const QRectF r = drawnRect(shape);
		vertical.frames << WireLanes::Frame{r.left(), r.right(), r.top(), r.bottom()};
		horizontal.frames << WireLanes::Frame{r.top(), r.bottom(), r.left(), r.right()};
		scene.frame_horizontal << WireLanes::Line{r.top(), r.left(), r.right()}
				       << WireLanes::Line{r.bottom(), r.left(), r.right()};
		scene.frame_vertical << WireLanes::Line{r.left(), r.top(), r.bottom()}
				     << WireLanes::Line{r.right(), r.top(), r.bottom()};
	}

		//The runs of the bridges, and the lines on which the shaped
		//conductors arrive, where they will be: over the whole room, as
		//the lanes are not chosen yet
	for (const Described &d : vertical_wires) {
		scene.horizontal << Scene::Owned{{d.wire.from, d.wire.low, d.wire.high}, d.conductor}
				 << Scene::Owned{{d.wire.to, d.wire.low, d.wire.high}, d.conductor};
	}
	for (const Described &d : horizontal_wires) {
		scene.vertical << Scene::Owned{{d.wire.from, d.wire.low, d.wire.high}, d.conductor}
			       << Scene::Owned{{d.wire.to, d.wire.low, d.wire.high}, d.conductor};
	}
	for (const Described &d : shaped) {
		scene.horizontal << Scene::Owned{{d.shape.end.y(), d.wire.low, d.wire.high}, d.conductor};
	}

		//Whether a wire still to be arranged would cross the line at @a y
		//from @a lo to @a hi on its way up or down
	const double line_margin = WireLanes::Clearance().line;
	const auto lineCrossed = [&](double y, double lo, double hi, const Conductor *self) {
		const auto spans = [&](const Described &o) {
			return o.conductor != self
				&& std::min(hi, o.wire.high) - std::max(lo, o.wire.low) > 0.01
				&& y > std::min(o.wire.from, o.wire.to) - line_margin
				&& y < std::max(o.wire.from, o.wire.to) + line_margin;
		};
		return std::any_of(vertical_wires.cbegin(), vertical_wires.cend(), spans)
			|| std::any_of(shaped.cbegin(), shaped.cend(), spans);
	};

		//The shapes, the narrowest room first: with one bend when the
		//straight way is free, else leaving the terminal to the side, and
		//into the lanes with the bridges
	std::stable_sort(shaped.begin(), shaped.end(), [](const Described &a, const Described &b) {
		return a.wire.high - a.wire.low < b.wire.high - b.wire.low;
	});
	for (Described &d : shaped)
	{
		Shaped &s = d.shape;
		const QSet<const void *> own{d.conductor, s.component, s.strip,
					     s.component->parentElement(), s.strip->parentElement()};
		const WireLanes::Clearance across = scene.clearance(own, false);
		const bool towards = (s.end.y() - s.start.y()) * s.outward > 0;
		const auto bendOnce = [&]() {
			const QList<QPointF> points = shapedPath(s, std::nan(""));
			if ((!only_their_groups || conductors.contains(d.conductor))
				&& !samePath(points, currentPoints(d.conductor))) {
				changes << Change{d.conductor, points};
			}
			addInTheWay(points, vertical, horizontal);
			scene.addPath(points, d.conductor);
		};
		if (towards
			&& WireLanes::isClear(s.start.x(), s.start.y(), s.end.y(), scene.clearance(own, true))
			&& WireLanes::isClear(s.end.y(), s.start.x(), s.stub.x(), across)
			&& !lineCrossed(s.end.y(), d.wire.low, d.wire.high, d.conductor))
		{
			bendOnce();
			continue;
		}
		const double run = WireLanes::runAt(s.start.y(), s.outward, s.start.x(), s.stub.x(),
						    across, longest_stub);
		s.run = std::isnan(run) ? automaticRun(d) : run;
		if (towards && std::abs(s.run - s.end.y()) < 1)
		{
			bendOnce();
			continue;
		}
		shapeWire(d);
		scene.horizontal << Scene::Owned{{s.run, d.wire.low, d.wire.high}, d.conductor};
		if (!near(s.run, s.start.y())) {
			scene.vertical << Scene::Owned{{s.start.x(), s.start.y(), s.run}, d.conductor};
		}
		vertical_wires << d;
		vertical.wires << d.wire;
	}

	const auto collect = [&](const QList<Described> &described, const WireLanes::Request &request) {
		const WireLanes::Result result = WireLanes::arrange(request);
		QSet<int> wanted;
		if (only_their_groups) {
			for (int i = 0 ; i < described.size() ; ++i) {
				if (conductors.contains(described.at(i).conductor) && result.group.at(i) >= 0) {
					wanted << result.group.at(i);
				}
			}
		}
		for (int i = 0 ; i < described.size() ; ++i)
		{
			if (std::isnan(result.at.at(i))) continue;
			if (only_their_groups && !wanted.contains(result.group.at(i))) continue;
			const Described &d = described.at(i);
			const QList<QPointF> points = d.shaped ? shapedPath(d.shape, result.at.at(i))
							       : withBridgeAt(d, result.at.at(i));
			if (!samePath(points, currentPoints(d.conductor))) {
				changes << Change{d.conductor, points};
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
	@brief WireArrangement::arrangesAutomatically
	@return true if a conductor drawn by the user is arranged, with its
	group, as it is drawn, and the conductors of a component the user
	moved when the move ends. On unless switched off in the configuration.
*/
bool WireArrangement::arrangesAutomatically()
{
	QSettings settings;
	return settings.value(setting_key, true).toBool();
}

void WireArrangement::setArrangesAutomatically(bool arrange)
{
	QSettings settings;
	settings.setValue(setting_key, arrange);
}

/**
	@brief WireArrangement::arrangeMovedConductors
	The undo command that arranges again the conductors of @a elements,
	just moved on @a diagram, with their groups - a child of @a parent, the
	command of the move, so that one undo puts the components and the
	conductors back. A conductor placed by hand stays as the move left it.
	Without it, an arranged conductor would follow the move as one placed
	by hand does, its runs stretched in proportion: the bends drift apart
	unevenly, and the room the move opened is not used.
	@return the command, or nullptr if arranging is switched off or nothing
	moves
*/
QUndoCommand *WireArrangement::arrangeMovedConductors(Diagram *diagram,
						      const QList<Element *> &elements,
						      QUndoCommand *parent)
{
	if (!arrangesAutomatically() || elements.isEmpty()) return nullptr;
	QList<Conductor *> attached;
	for (Element *e : elements) {
		const QList<Terminal *> terminals = e->terminals();
		for (Terminal *t : terminals) {
			const QList<Conductor *> on = t->conductors();
			for (Conductor *c : on) {
				if (!attached.contains(c)) attached << c;
			}
		}
	}
	if (attached.isEmpty()) return nullptr;
	const QList<Change> changes = plan(diagram, attached, Scope::Moved);
	if (changes.isEmpty()) return nullptr;
	return new ArrangeConductorsCommand(diagram, changes, parent);
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
	if (!arrangesAutomatically()) return nullptr;
	const QList<Change> changes = plan(diagram, conductors, Scope::NewConductors);
	if (changes.isEmpty()) return nullptr;
	return new ArrangeConductorsCommand(diagram, changes, parent);
}
