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
#include "conductortextfit.h"
#include "diagram.h"
#include "foliogrid.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/qetshapeitem.h"
#include "qetgraphicsitem/terminal.h"
#include "undocommand/arrangeconductorscommand.h"
#include "wirelanes.h"

#include <QHash>
#include <QLineF>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QSettings>
#include <QFontMetricsF>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace
{
		/// Kept clear around the body of a symbol, as ConductorRouter keeps
	constexpr double symbol_margin = 5.0;

		/// How far a run may go out of its terminal before it leaves it to
		/// the side, at most: two of the gaps between bridges in conflict
	constexpr double longest_stub = 20.0;

		/// The lanes of the conductors that turn round the end of a strip's
		/// terminal, from it and from each other: the name of a wire, at
		/// the size of 5, fits on each of them without touching the next
		/// (T71)
	constexpr double around_pitch = 15.0;
		/// The runs of two such conductors of one column, one from the other
	constexpr double around_run_gap = 10.0;
		/// Kept clear around a text of a component (sixth adjustment)
	constexpr double text_margin = 1.0;
		/// How far the common line of a point of several conductors may go
		/// out of the line of their terminal, to give the arms room for
		/// their names (T67, fifth adjustment)
	constexpr double longest_trunk_shift = 100.0;

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
			/// The component is on this side of the strip's own piece:
			/// the conductor turns round its end, in a lane beyond it
		bool around = false;
			/// The way the other terminal points, along x: -1 or 1
		double side = -1;
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
			/// Described with x and y swapped (fifth adjustment): the same
			/// shape turned, a component's terminal that points to the side
			/// and a strip's that points up or down. Everything above is
			/// then in the swapped frame, and a path is swapped back before
			/// it goes to the folio.
		bool transposed = false;
			/// A wire whose terminal points away from the other goes round
			/// its own component first (seventh adjustment): the points from
			/// that terminal to where it is a wire like any other, which
			/// automatic then starts at - on the folio, and in the order from
			/// that terminal
		QList<QPointF> lead;
			/// That terminal is terminal1
		bool lead_first = true;
	};

		/// What a wire with a lead goes round: its two terminals, and the frame
		/// in which they point to the side
	struct Detour
	{
		Conductor *conductor = nullptr;
		Terminal *away = nullptr;
		Terminal *toward = nullptr;
		bool transposed = false;
	};

	bool near(double a, double b) { return std::abs(a - b) < 0.01; }

	QPointF swapped(const QPointF &p) { return QPointF(p.y(), p.x()); }

	QList<QPointF> swapped(QList<QPointF> points)
	{
		for (QPointF &p : points) p = swapped(p);
		return points;
	}

		/// @a points, in the frame of @a d, as they go on the folio
	QList<QPointF> onFolio(const Described &d, const QList<QPointF> &points)
	{
		return d.transposed ? swapped(points) : points;
	}

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

		/// Whether @a c is a conductor whose shape is chosen here (Shaped),
		/// described as it is or, with @a transposed, turned (fifth
		/// adjustment): the terminal that points to the side is then the
		/// component's
	bool describeShaped(Conductor *c, Described &out, bool transposed = false)
	{
		Terminal *v = c->terminal1;
		Terminal *h = c->terminal2;
		if (!v || !h) return false;
		const auto upOrDown = [transposed](const Terminal *t) {
			return pointsUpOrDown(t) != transposed;
		};
		if (!upOrDown(v)) std::swap(v, h);
		if (!upOrDown(v) || upOrDown(h)) return false;

		QList<QPointF> p = c->automaticPathPoints();
		if (p.size() < 3) return false;
		if (transposed) p = swapped(p);
		const bool h_first = h == c->terminal1;
		Shaped s;
		s.component = v;
		s.strip = h;
		s.start = transposed ? swapped(v->dockConductor()) : v->dockConductor();
		s.end = transposed ? swapped(h->dockConductor()) : h->dockConductor();
		s.stub = h_first ? p.at(1) : p.at(p.size() - 2);
		s.outward = v->orientation() == (transposed ? Qet::East : Qet::South) ? 1 : -1;
		s.side = h->orientation() == (transposed ? Qet::North : Qet::West) ? -1 : 1;
		s.reversed = h_first;
		s.run = s.start.y();
			//The strip's own piece points towards the component, which is
			//beyond it, at another height
		const double side = s.side;
		if (!near(s.stub.y(), s.end.y()) || (s.stub.x() - s.end.x()) * side < 0.5
			|| std::abs(s.start.y() - s.end.y()) < 1) {
			return false;
		}
			//The component on this side of the strip's own piece: the
			//conductor goes past it and turns round it - when the
			//component's terminal points towards the line of the strip's
			//(fourth adjustment). Not turned.
		if ((s.start.x() - s.stub.x()) * side < 1)
		{
			if (transposed || (s.end.y() - s.start.y()) * s.outward <= 0) return false;
			s.around = true;
		}
		out.conductor = c;
		out.automatic = p;
		out.vertical = true;
		out.shaped = true;
		out.shape = s;
		out.transposed = transposed;
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

		/// Whether the strip's own piece of @a d, as the automatic path has
		/// it, passes the end of a terminal of @a elements other than its two
		/// - or comes to it as close as a wire may pass it, or closer
	bool stubPassesTerminal(const Described &d, const QList<Element *> &elements)
	{
		const Shaped &s = d.shape;
		const double spot = WireLanes::Clearance().spot;
		const double lo = std::min(s.end.x(), s.stub.x());
		const double hi = std::max(s.end.x(), s.stub.x());
		for (const Element *e : elements)
		{
			const QList<Terminal *> terminals = e->terminals();
			for (const Terminal *t : terminals)
			{
				if (t == s.component || t == s.strip) continue;
				const QPointF p = d.transposed ? swapped(t->dockConductor()) : t->dockConductor();
				if (std::abs(p.y() - s.end.y()) < spot && p.x() > lo - spot - 0.01 && p.x() < hi + spot + 0.01
				    && (p - s.end).manhattanLength() > 0.5) {
					return true;
				}
			}
		}
		return false;
	}

		/// The shape of @a c, as it is or turned: a conductor between a
		/// terminal that points up or down and one that points to the side
		/// is a shape both ways, the run chosen from the one and the other's
		/// own piece kept as the automatic path has it. The way whose kept
		/// piece passes no other terminal (ninth adjustment): kept, the piece
		/// of a terminal of two components stacked side by side went past
		/// the terminal of the other. Else as it is, as before.
	bool describeShapedEitherWay(Conductor *c, Described &out, const QList<Element *> &elements)
	{
		Described as_is;
		if (!describeShaped(c, as_is)) return describeShaped(c, out, true);
		Described turned;
		if (stubPassesTerminal(as_is, elements) && describeShaped(c, turned, true)
		    && !stubPassesTerminal(turned, elements)) {
			out = turned;
		} else {
			out = as_is;
		}
		return true;
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

			/// The same, in the frame of a shape described turned when
			/// @a transposed: what runs left and right there runs up and
			/// down on the folio
		WireLanes::Clearance clearanceIn(bool transposed, const QSet<const void *> &own,
						 bool up_and_down) const
		{
			return clearance(own, up_and_down != transposed);
		}

			/// A line of a shape, in its frame: one running left and right
			/// there when @a left_and_right, the other way otherwise
		void addLineIn(bool transposed, bool left_and_right, const WireLanes::Line &line,
			       const void *owner)
		{
			((left_and_right != transposed) ? horizontal : vertical) << Owned{line, owner};
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
	bool describeFrom(Conductor *c, const QList<QPointF> &p, Described &out)
	{
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

	bool describe(Conductor *c, Described &out)
	{
		return describeFrom(c, c->automaticPathPoints(), out);
	}

		/// Whether @a c joins two terminals that point the same way, one of
		/// them away from the other - the one behind it (seventh adjustment).
		/// The way they point is to the side in the frame, turned for two
		/// terminals that point up or down.
	bool detourOf(Conductor *c, Detour &out)
	{
		Terminal *t1 = c->terminal1;
		Terminal *t2 = c->terminal2;
		if (!t1 || !t2 || t1->orientation() != t2->orientation()) return false;
		const bool transposed = pointsUpOrDown(t1);
		const QPointF p1 = transposed ? swapped(t1->dockConductor()) : t1->dockConductor();
		const QPointF p2 = transposed ? swapped(t2->dockConductor()) : t2->dockConductor();
		const Qet::Orientation way = t1->orientation();
		const double s = (way == Qet::East || way == Qet::South) ? 1 : -1;
		if ((p2.x() - p1.x()) * s < -0.5) {
			out = Detour{c, t1, t2, transposed};
		} else if ((p1.x() - p2.x()) * s < -0.5) {
			out = Detour{c, t2, t1, transposed};
		} else {
			return false;
		}
		return true;
	}

		/// The path of @a bridge_path - from where a wire with a lead is a
		/// wire like any other, to its other terminal - with the lead before
		/// it, from terminal1 to terminal2
	QList<QPointF> withLead(const Described &d, const QList<QPointF> &bridge_path)
	{
		if (d.lead.isEmpty()) return bridge_path;
		QList<QPointF> path;
		for (const QPointF &q : d.lead + bridge_path) {
			if (path.isEmpty() || (path.last() - q).manhattanLength() > 0.01) path << q;
		}
		if (!d.lead_first) std::reverse(path.begin(), path.end());
		return path;
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
				//A selection is arranged with its groups, as a new conductor
				//is (T67, eighth adjustment): with the others in the way,
				//a selected conductor went into what room they left, close
				//against them
			case Scope::NewConductors:
			case Scope::Selection:     if (given || !placedByHand(c)) movable << c; break;
			case Scope::Folio:
			case Scope::Moved:         if (!placedByHand(c)) movable << c; break;
		}
	}

		//The potential of each conductor (fifth adjustment): two conductors
		//of one potential are one point, and a piece of the one is not in
		//the way of the other - they may share it
	QHash<const Conductor *, int> net;
	int next_net = 0;
	for (Conductor *c : std::as_const(all))
	{
		if (net.contains(c)) continue;
		const int id = next_net++;
		net.insert(c, id);
		const QSet<Conductor *> same = c->relatedPotentialConductors(false);
		for (Conductor *o : same) net.insert(o, id);
	}
	const auto sameNet = [&net](const Conductor *a, const Conductor *b) {
		return a == b || (net.contains(a) && net.value(a) == net.value(b, -1));
	};
		//What the shape of @a d does not keep clear of: itself, its two
		//terminals and their components, and the conductors of its point
	const auto ownOf = [&](const Described &d) {
		const Shaped &s = d.shape;
		QSet<const void *> own{d.conductor, s.component, s.strip,
				       s.component->parentElement(), s.strip->parentElement()};
		for (const Conductor *o : std::as_const(all)) {
			if (o != d.conductor && sameNet(o, d.conductor)) own << o;
		}
		return own;
	};

	WireLanes::Request vertical, horizontal;
		//The lanes on whole units, or on the grid when the person set it
		//finer than that
	QSettings settings;
	vertical.unit = FolioGrid::freeStep(FolioGrid::step(settings.value(FolioGrid::x_key), Diagram::xGrid));
	horizontal.unit = FolioGrid::freeStep(FolioGrid::step(settings.value(FolioGrid::y_key), Diagram::yGrid));
	const auto unitIn = [&](bool transposed) {
		return transposed ? horizontal.unit : vertical.unit;
	};
	Scene scene;
	const bool only_their_groups = scope == Scope::NewConductors || scope == Scope::Selection
			|| scope == Scope::Moved;

		//The points of several conductors (fifth adjustment): the conductors
		//of the shape that leave one terminal, all to one side and the same
		//way, described in the frame in which that terminal is the
		//component's - as they are when it points up or down, turned when it
		//points to the side. A conductor of two such points goes with the
		//larger.
	QList<QList<Described>> points;
	QSet<Conductor *> in_point;
	{
		QHash<QString, QList<Described>> by_terminal;
		for (Conductor *c : std::as_const(all))
		{
			if (!movable.contains(c)) continue;
			for (bool transposed : {false, true})
			{
				Described d;
				if (!describeShaped(c, d, transposed) || d.shape.around) continue;
				const Shaped &s = d.shape;
				if ((s.end.y() - s.start.y()) * s.outward <= 0) continue;
				by_terminal[QStringLiteral("%1/%2/%3/%4")
						.arg(quintptr(s.component)).arg(transposed)
						.arg(s.side).arg(s.outward)] << d;
			}
		}
		QList<QList<Described>> candidates = by_terminal.values();
		std::stable_sort(candidates.begin(), candidates.end(),
				 [](const QList<Described> &a, const QList<Described> &b) {
			if (a.size() != b.size()) return a.size() > b.size();
			return !a.first().transposed && b.first().transposed;
		});
		for (const QList<Described> &candidate : std::as_const(candidates))
		{
			QList<Described> point;
			for (const Described &d : candidate) {
				if (!in_point.contains(d.conductor)) point << d;
			}
			if (point.size() < 2) continue;
			for (const Described &d : std::as_const(point)) in_point << d.conductor;
			points << point;
		}
	}

	QList<Described> vertical_wires, horizontal_wires, shaped;
	QList<Detour> detours;
	const QList<Element *> folio_elements = diagram->elements();
	for (Conductor *c : all)
	{
		if (in_point.contains(c)) continue;
		Described d;
		if (movable.contains(c) && describeShapedEitherWay(c, d, folio_elements))
		{
			shaped << d;
			continue;
		}
		if (movable.contains(c) && describe(c, d))
		{
			if (d.vertical) {
				vertical_wires << d;
			} else {
				horizontal_wires << d;
			}
			continue;
		}
		Detour detour;
		if (movable.contains(c) && detourOf(c, detour))
		{
			detours << detour;
			continue;
		}
		const QList<QPointF> path = currentPoints(c);
		addInTheWay(path, vertical, horizontal);
		scene.addPath(path, c);
	}
		//The ones that turn round the end of a strip's terminal take their
		//own way (fourth adjustment): the lanes of the others do not fit
		//a conductor whose two runs leave the lane to the same side
	QList<Described> around;
	for (int i = shaped.size() - 1 ; i >= 0 ; --i)
	{
		if (shaped.at(i).shape.around) around.prepend(shaped.takeAt(i));
	}
	if (vertical_wires.isEmpty() && horizontal_wires.isEmpty() && shaped.isEmpty()
		&& around.isEmpty() && points.isEmpty() && detours.isEmpty()) return changes;

	const QList<Element *> elements = diagram->elements();
	QList<QRectF> text_boxes;
	for (Element *e : elements)
	{
		const QRectF r = e->mapRectToScene(e->boundingRect())
				.adjusted(-symbol_margin, -symbol_margin, symbol_margin, symbol_margin);
		vertical.blocks << WireLanes::Block{r.left(), r.right(), r.top(), r.bottom()};
		horizontal.blocks << WireLanes::Block{r.top(), r.bottom(), r.left(), r.right()};
		scene.bodies << qMakePair(r, static_cast<const void *>(e));
			//Its texts too, which lie beside the drawing of the symbol, and
			//which a wire must not cover any more than the drawing (sixth
			//adjustment) - the texts of the wire's own component too: each
			//is in the way of every wire
		const QList<DynamicElementTextItem *> texts = e->allDynamicTextItems();
		for (DynamicElementTextItem *text : texts)
		{
			if (!text->isVisible() || text->toPlainText().trimmed().isEmpty()) continue;
			const QRectF box = text->sceneBoundingRect()
					.adjusted(-text_margin, -text_margin, text_margin, text_margin);
			vertical.blocks << WireLanes::Block{box.left(), box.right(), box.top(), box.bottom()};
			horizontal.blocks << WireLanes::Block{box.top(), box.bottom(), box.left(), box.right()};
			scene.bodies << qMakePair(box, static_cast<const void *>(text));
			text_boxes << box;
		}
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

		//The wires whose terminal points away from the other go round their
		//own component first (seventh adjustment): out of the terminal, along
		//the side of the component to the first line clear of it - below or
		//above, the nearer to the other terminal first -, and from there they
		//are wires between two terminals that face each other, their bridge
		//in the room between the component and the other terminal. With no
		//way round clear, a wire stays as it is
	for (const Detour &t : std::as_const(detours))
	{
		const bool tr = t.transposed;
		const auto frame = [tr](const QPointF &q) { return tr ? swapped(q) : q; };
		const double unit = unitIn(tr);
		const QPointF a = frame(t.away->dockConductor());
		const QPointF b = frame(t.toward->dockConductor());
		const Qet::Orientation way = t.away->orientation();
		const double s = (way == Qet::East || way == Qet::South) ? 1 : -1;
		Element *component = t.away->parentElement();
		QRectF body = component->mapRectToScene(component->boundingRect())
				.adjusted(-symbol_margin, -symbol_margin, symbol_margin, symbol_margin);
		if (tr) body = QRectF(body.top(), body.left(), body.height(), body.width());
			//The other terminal's own piece, as the automatic path has it
		const QList<QPointF> automatic = t.conductor->automaticPathPoints();
		if (automatic.size() < 3) continue;
		const QPointF stub = frame(t.toward == t.conductor->terminal1 ? automatic.at(1)
									       : automatic.at(automatic.size() - 2));
		const double out_x = s > 0 ? std::ceil(std::max(a.x(), body.right()) / unit) * unit
					   : std::floor(std::min(a.x(), body.left()) / unit) * unit;
		const double near_side = s > 0 ? body.left() : body.right();
			//Room for a bridge between the other terminal and the component
		if ((near_side - stub.x()) * s < 2 * unit) continue;

		const QSet<const void *> own{t.conductor, t.away, t.toward, t.toward->parentElement()};
		const WireLanes::Clearance across = scene.clearanceIn(tr, own, false);
		const WireLanes::Clearance upright = scene.clearanceIn(tr, own, true);
		const double below = std::ceil(body.bottom() / unit) * unit;
		const double above = std::floor(body.top() / unit) * unit;
		const bool below_first = std::abs(b.y() - below) <= std::abs(b.y() - above);
		bool placed = false;
		for (double line : below_first ? QList<double>{below, above} : QList<double>{above, below})
		{
			if (!WireLanes::isClear(out_x, a.y(), line, upright)
			    || !WireLanes::isClear(line, near_side, out_x, across)) {
				continue;
			}
			const double middle = (stub.x() + near_side) / 2;
			QList<QPointF> bridge{QPointF(out_x, line), QPointF(out_x, line), QPointF(middle, line),
					      QPointF(middle, b.y()), stub, b};
			QList<QPointF> lead{a, QPointF(out_x, a.y())};
			if (tr) {
				bridge = swapped(bridge);
				lead = swapped(lead);
			}
			Described d;
			if (!describeFrom(t.conductor, bridge, d)) continue;
			d.lead = lead;
			d.lead_first = t.away == t.conductor->terminal1;
			addInTheWay(lead + QList<QPointF>{bridge.first()}, vertical, horizontal);
			scene.addPath(lead + QList<QPointF>{bridge.first()}, t.conductor);
			if (d.vertical) {
				vertical_wires << d;
			} else {
				horizontal_wires << d;
			}
			placed = true;
			break;
		}
		if (!placed)
		{
			const QList<QPointF> path = currentPoints(t.conductor);
			addInTheWay(path, vertical, horizontal);
			scene.addPath(path, t.conductor);
		}
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
		scene.addLineIn(d.transposed, true, {d.shape.end.y(), d.wire.low, d.wire.high}, d.conductor);
	}
	for (const QList<Described> &point : std::as_const(points)) {
		for (const Described &d : point) {
			scene.addLineIn(d.transposed, true, {d.shape.end.y(), d.wire.low, d.wire.high}, d.conductor);
		}
	}

		//Whether a wire still to be arranged, of another point, would cross
		//the line at @a y from @a lo to @a hi on its way up or down - in the
		//frame of a shape described turned when @a transposed
	const double line_margin = WireLanes::Clearance().line;
	const auto lineCrossed = [&](bool transposed, double y, double lo, double hi, const Conductor *self) {
		const auto spans = [&](const Described &o) {
			return !sameNet(o.conductor, self)
				&& std::min(hi, o.wire.high) - std::max(lo, o.wire.low) > 0.01
				&& y > std::min(o.wire.from, o.wire.to) - line_margin
				&& y < std::max(o.wire.from, o.wire.to) + line_margin;
		};
		const auto shapedSpans = [&](const Described &o) {
			return o.transposed == transposed && spans(o);
		};
		const QList<Described> &bridges = transposed ? horizontal_wires : vertical_wires;
		return std::any_of(bridges.cbegin(), bridges.cend(), spans)
			|| std::any_of(shaped.cbegin(), shaped.cend(), shapedSpans);
	};

		//The conductors that turn round the end of a strip's terminal, one
		//column at a time - one side, one direction, the ends of the strip's
		//terminals in one line (fourth adjustment). Outside goes the one
		//whose component is nearest the side of the lanes: it turns first
		//and goes to the farthest terminal, the one order in which no two
		//of them cross - the order of the terminals of the strip decides
		//the rest. Each lane a pitch out from the one inside it, the first a
		//pitch from the end of the terminals; each run as near its terminal
		//as is clear, and a gap past the run outside it.
	{
		QMap<QString, QList<Described>> columns;
		for (const Described &d : std::as_const(around))
		{
			const Shaped &s = d.shape;
			columns[QStringLiteral("%1/%2/%3").arg(s.side).arg(s.outward).arg(qRound(s.stub.x()))] << d;
		}
		for (auto column = columns.begin() ; column != columns.end() ; ++column)
		{
			QList<Described> &wires = column.value();
			const double side = wires.first().shape.side;
			std::stable_sort(wires.begin(), wires.end(), [side](const Described &a, const Described &b) {
				return (a.shape.start.x() - a.shape.stub.x()) * -side
						< (b.shape.start.x() - b.shape.stub.x()) * -side;
			});
			const int n = wires.size();
			double previous_run = std::nan("");
			for (int k = 0 ; k < n ; ++k)
			{
				Described &d = wires[k];
				Shaped &s = d.shape;
				const QSet<const void *> own = ownOf(d);
				double lane = s.stub.x() + side * around_pitch * (n - k);
				lane = std::round(lane / vertical.unit) * vertical.unit;

				const WireLanes::Clearance across = scene.clearance(own, false);
				double run = WireLanes::runAt(s.start.y(), s.outward, s.start.x(), lane,
							      across, longest_stub, vertical.unit);
				if (std::isnan(run)) run = automaticRun(d);
				if (!std::isnan(previous_run) && (run - previous_run) * s.outward < around_run_gap) {
					run = previous_run + s.outward * around_run_gap;
				}
					//Past the line of the strip's terminal, this is not the
					//shape to take: the conductor is left as it is
				if ((s.end.y() - run) * s.outward < 1) continue;
				s.run = run;
				previous_run = run;

					//The lane further out, within the pitch that is its own,
					//while its piece up or down is not clear
				const WireLanes::Clearance upright = scene.clearance(own, true);
				for (int t = 0 ; (t + 1) * vertical.unit < around_pitch
					     && !WireLanes::isClear(lane, std::min(run, s.end.y()),
								    std::max(run, s.end.y()), upright) ; ++t) {
					lane += side * vertical.unit;
				}

				const QList<QPointF> path = shapedPath(s, lane);
				if ((!only_their_groups || conductors.contains(d.conductor))
					&& !samePath(path, currentPoints(d.conductor))) {
					changes << Change{d.conductor, path};
				}
				addInTheWay(path, vertical, horizontal);
				scene.addPath(path, d.conductor);
			}
		}
	}

		//The points of several conductors (fifth adjustment): one common
		//line for all, straight out of their terminal as the reset of the
		//folio draws it, and each conductor's arm from it to its own
		//terminal - the name of each on its arm. A conductor of another
		//point may cross the common line, with the arc of a jump; nothing
		//may run along it, nor end next to it. When the shortest arm is too
		//short for the name of its conductor, the common line goes further
		//out, away from the strips, a unit at a time, as long as the way is
		//clear - and stops before what is in it, the name then drawn
		//smaller. A point with no clear way is arranged a conductor at a
		//time, as before.
		//The common lines placed, on the folio: the arm of another point may
		//cross them too (eighth adjustment) - else, of two points whose arms
		//take turns on one strip, the one farther from it found no way when
		//it was the smaller, and went a conductor at a time
	QList<QLineF> common_lines;
	const auto onCommonLine = [&common_lines](bool transposed, const WireLanes::Line &l) {
		for (const QLineF &line : std::as_const(common_lines))
		{
			const QPointF a = transposed ? swapped(line.p1()) : line.p1();
			const QPointF b = transposed ? swapped(line.p2()) : line.p2();
			if (std::abs(a.x() - b.x()) < 0.01 && std::abs(l.at - a.x()) < 0.01
			    && std::min(l.a, l.b) > std::min(a.y(), b.y()) - 0.01
			    && std::max(l.a, l.b) < std::max(a.y(), b.y()) + 0.01) {
				return true;
			}
		}
		return false;
	};
	for (QList<Described> &point : points)
	{
		const Described &first = point.first();
		const bool tr = first.transposed;
		const Shaped &s0 = first.shape;
		const QPointF start = s0.start;
		const double side = s0.side;
		const double outward = s0.outward;
		const double unit = unitIn(tr);
		QSet<const void *> own;
		bool wanted = !only_their_groups;
		double far_y = start.y();
		double near_y = std::nan("");
		for (const Described &d : std::as_const(point))
		{
			own += ownOf(d);
			if (conductors.contains(d.conductor)) wanted = true;
			const double y = d.shape.end.y();
			if ((y - far_y) * outward > 0) far_y = y;
			if (std::isnan(near_y) || (y - near_y) * outward < 0) near_y = y;
		}
			//The runs of the bridges still to be placed in this frame are
			//over their whole room, and not where they will be: they are
			//not in the way here, and keep clear of the arms afterwards
		QList<Described> &bridges = tr ? horizontal_wires : vertical_wires;
		for (const Described &o : std::as_const(bridges)) own << o.conductor;
		const WireLanes::Clearance across = scene.clearanceIn(tr, own, false);
		const WireLanes::Clearance upright = scene.clearanceIn(tr, own, true);
		const auto trunkClear = [&](double lane, double from) {
			WireLanes::Clearance c = upright;
			c.crossing.erase(std::remove_if(c.crossing.begin(), c.crossing.end(),
							[&](const WireLanes::Line &l) {
				return std::min(l.a, l.b) < lane - c.line && std::max(l.a, l.b) > lane + c.line;
			}), c.crossing.end());
			return WireLanes::isClear(lane, from, far_y, c);
		};
		const auto armsClear = [&](double lane) {
			for (const Described &d : std::as_const(point))
			{
				const double y = d.shape.end.y();
				WireLanes::Clearance c = across;
				c.crossing.erase(std::remove_if(c.crossing.begin(), c.crossing.end(),
								[&](const WireLanes::Line &l) {
					return std::min(l.a, l.b) < y - c.line && std::max(l.a, l.b) > y + c.line
						&& onCommonLine(tr, l);
				}), c.crossing.end());
				if (!WireLanes::isClear(y, lane, d.shape.stub.x(), c)) return false;
			}
			return true;
		};
		const Qt::Orientations arm_run = tr ? Qt::Vertical : Qt::Horizontal;
		const auto namesFit = [&](double lane) {
			for (const Described &d : std::as_const(point))
			{
				if (nameLength(d.conductor, arm_run)
				    > ConductorTextFit::room(std::abs(d.shape.stub.x() - lane)) + 0.01) {
					return false;
				}
			}
			return true;
		};

		bool reached = false;
		double lane_at = start.x();
		double run_at = start.y();
		for (int k = 0 ; k * unit <= longest_trunk_shift + 1e-6 ; ++k)
		{
			const double lane = start.x() + side * k * unit;
			double run = start.y();
			bool clear = false;
			if (k == 0) {
				clear = trunkClear(lane, start.y()) && armsClear(lane);
			} else {
				run = WireLanes::runAt(start.y(), outward, start.x(), lane, across,
						       longest_stub, unit);
				clear = !std::isnan(run) && (near_y - run) * outward >= 1
					&& (near(run, start.y())
					    || WireLanes::isClear(start.x(), start.y(), run, upright))
					&& trunkClear(lane, run) && armsClear(lane);
			}
			if (!clear)
			{
				if (reached) break;
				continue;
			}
			reached = true;
			lane_at = lane;
			run_at = run;
			if (namesFit(lane)) break;
		}
		if (!reached)
		{
			for (const Described &d : std::as_const(point)) shaped << d;
			continue;
		}
		for (Described &d : point)
		{
			d.shape.run = run_at;
			const QList<QPointF> path = onFolio(d, near(lane_at, start.x())
							       ? shapedPath(d.shape, std::nan(""))
							       : shapedPath(d.shape, lane_at));
			if (wanted && !samePath(path, currentPoints(d.conductor))) {
				changes << Change{d.conductor, path};
			}
			addInTheWay(path, vertical, horizontal);
			scene.addPath(path, d.conductor);

				//A bridge of another point whose run would go along the arm,
				//closer than a line, keeps to the side of it its run comes
				//from: the room of its bridge ends before the arm
			const double arm_y = d.shape.end.y();
			const double arm_low = std::min(lane_at, d.shape.stub.x());
			const double arm_high = std::max(lane_at, d.shape.stub.x());
			for (Described &o : bridges)
			{
				if (sameNet(o.conductor, d.conductor)
				    || std::min(arm_high, o.wire.high) - std::max(arm_low, o.wire.low) <= 0.01) {
					continue;
				}
				const double low = o.wire.low;
				const double high = o.wire.high;
				if (std::abs(o.wire.from - arm_y) < line_margin && arm_low > low) {
					o.wire.high = std::min(o.wire.high, arm_low - line_margin);
				}
				if (std::abs(o.wire.to - arm_y) < line_margin && arm_high < high) {
					o.wire.low = std::max(o.wire.low, arm_high + line_margin);
				}
			}
		}
		const QLineF common(QPointF(lane_at, run_at), QPointF(lane_at, far_y));
		common_lines << (tr ? QLineF(swapped(common.p1()), swapped(common.p2())) : common);

			//The room of its arms is a frame to the bridges of the other
			//points (eighth adjustment): a bridge in it crosses arms and
			//runs along them, where their names go; past the common line it
			//crosses none. The bridges take the room outside it when there
			//is enough of it, as they do the room outside a drawn frame
		double arms_low = lane_at;
		double arms_high = lane_at;
		for (const Described &d : std::as_const(point))
		{
			arms_low = std::min(arms_low, d.shape.stub.x());
			arms_high = std::max(arms_high, d.shape.stub.x());
		}
		(tr ? horizontal : vertical).frames
				<< WireLanes::Frame{arms_low, arms_high, std::min(near_y, far_y), std::max(near_y, far_y)};
	}

		//The shapes, the narrowest room first: with one bend when the
		//straight way is free, else leaving the terminal to the side, and
		//into the lanes with the bridges
	std::stable_sort(shaped.begin(), shaped.end(), [](const Described &a, const Described &b) {
		return a.wire.high - a.wire.low < b.wire.high - b.wire.low;
	});
	for (Described &d : shaped)
	{
		Shaped &s = d.shape;
		const bool tr = d.transposed;
		const QSet<const void *> own = ownOf(d);
		const WireLanes::Clearance across = scene.clearanceIn(tr, own, false);
		const bool towards = (s.end.y() - s.start.y()) * s.outward > 0;
		const auto bendOnce = [&]() {
			const QList<QPointF> path = onFolio(d, shapedPath(s, std::nan("")));
			if ((!only_their_groups || conductors.contains(d.conductor))
				&& !samePath(path, currentPoints(d.conductor))) {
				changes << Change{d.conductor, path};
			}
			addInTheWay(path, vertical, horizontal);
			scene.addPath(path, d.conductor);
		};
		if (towards
			&& WireLanes::isClear(s.start.x(), s.start.y(), s.end.y(), scene.clearanceIn(tr, own, true))
			&& WireLanes::isClear(s.end.y(), s.start.x(), s.stub.x(), across)
			&& !lineCrossed(tr, s.end.y(), d.wire.low, d.wire.high, d.conductor))
		{
			bendOnce();
			continue;
		}
			//A body at the far end of its room, round the strip's own piece,
			//is not in the way of the run (ninth adjustment): the run goes
			//only as far as the bridge, which keeps out of bodies, so that
			//body is always beyond it. Counted, it took the height of the
			//terminal's own end from two components stacked without a gap,
			//and the run went past the terminal of the other
		WireLanes::Clearance run_clearance = across;
		run_clearance.bodies.erase(std::remove_if(run_clearance.bodies.begin(), run_clearance.bodies.end(),
							  [&s](const WireLanes::Block &b) {
			return std::min(b.low, b.high) <= s.stub.x() + 0.01 && std::max(b.low, b.high) >= s.stub.x() - 0.01;
		}), run_clearance.bodies.end());
		const double run = WireLanes::runAt(s.start.y(), s.outward, s.start.x(), s.stub.x(),
						    run_clearance, longest_stub, unitIn(tr));
		s.run = std::isnan(run) ? automaticRun(d) : run;
		if (towards && std::abs(s.run - s.end.y()) < 1)
		{
			bendOnce();
			continue;
		}
		shapeWire(d);
		scene.addLineIn(tr, true, {s.run, d.wire.low, d.wire.high}, d.conductor);
		if (!near(s.run, s.start.y())) {
			scene.addLineIn(tr, false, {s.start.x(), s.start.y(), s.run}, d.conductor);
		}
		if (tr) {
			horizontal_wires << d;
		} else {
			vertical_wires << d;
		}
	}

		//A bridge alone in its group (fifth adjustment) stays in the middle
		//of its room, as before, unless no run of its wire would then hold
		//its name - a run cut where a line of another point crosses it, as
		//the name is (Conductor::calculateTextItemPosition()). It then
		//moves, without leaving the piece of room it was given, the least
		//that makes one run long enough: the wires that cross the common
		//line of a point keep their names off it
	const auto roomForName = [&](const Described &d, double at, const WireLanes::Request &request) {
		const qreal needed = nameLength(d.conductor, d.vertical ? Qt::Horizontal : Qt::Vertical)
				+ 2 * ConductorTextFit::end_clearance;
		if (needed <= 2 * ConductorTextFit::end_clearance + 0.01) return at;
		const WireLanes::Wire &w = d.wire;
		const double span_low = std::min(w.from, w.to);
		const double span_high = std::max(w.from, w.to);
			//The lines across the runs: pieces of the conductors of other
			//points, and the sides of the shapes
		QVector<WireLanes::Line> across;
		for (const Scene::Owned &o : (d.vertical ? scene.vertical : scene.horizontal)) {
			if (!sameNet(static_cast<const Conductor *>(o.owner), d.conductor)) across << o.line;
		}
		across << (d.vertical ? scene.frame_vertical : scene.frame_horizontal);
		const auto longest = [&](double bridge) {
			double best = 0;
			for (const auto &run : {std::make_tuple(w.from, w.low, bridge),
						std::make_tuple(w.to, bridge, w.high)})
			{
				const double y = std::get<0>(run);
				QVector<double> cuts{std::get<1>(run), std::get<2>(run)};
				for (const WireLanes::Line &l : std::as_const(across)) {
					if (l.at > std::get<1>(run) + 0.5 && l.at < std::get<2>(run) - 0.5
					    && y > std::min(l.a, l.b) - 0.5 && y < std::max(l.a, l.b) + 0.5) {
						cuts << l.at;
					}
				}
				std::sort(cuts.begin(), cuts.end());
				for (int k = 0 ; k + 1 < cuts.size() ; ++k) best = std::max(best, cuts.at(k + 1) - cuts.at(k));
			}
			return best;
		};
		if (longest(at) >= needed) return at;
			//Inside the piece of room it was given: no block, and no side
			//of a frame between it and where it was put
		const auto allowed = [&](double x) {
			if (x <= w.low + request.unit - 1e-6 || x >= w.high - request.unit + 1e-6) return false;
			for (const WireLanes::Block &b : request.blocks) {
				if (x > std::min(b.low, b.high) && x < std::max(b.low, b.high)
				    && std::min(span_high, std::max(b.from, b.to)) - std::max(span_low, std::min(b.from, b.to)) > 0) {
					return false;
				}
			}
			for (const WireLanes::Frame &f : request.frames) {
				for (double side : {f.low, f.high}) {
					if (side > std::min(at, x) && side < std::max(at, x)
					    && std::min(span_high, std::max(f.from, f.to)) - std::max(span_low, std::min(f.from, f.to)) > 0) {
						return false;
					}
				}
			}
			return true;
		};
		for (int k = 1 ; k * request.unit < w.high - w.low ; ++k)
		{
			for (double x : {at - k * request.unit, at + k * request.unit}) {
				if (allowed(x) && longest(x) >= needed) return x;
			}
		}
		return at;
	};

	const auto collect = [&](const QList<Described> &described, const WireLanes::Request &base) {
			//A point of several conductors between two columns (fifth
			//adjustment): the conductors that leave one terminal, to one
			//side and the same way, are one wire to WireLanes - they share
			//its bridge, their common line - in the room all of them have
		const int n = described.size();
		QVector<QString> key(n);
		{
			QVector<QPair<QString, QString>> ends(n);
			QHash<QString, int> count;
			for (int i = 0 ; i < n ; ++i)
			{
				const Described &d = described.at(i);
				if (d.shaped || !d.lead.isEmpty() || std::abs(d.wire.to - d.wire.from) < 1) continue;
				const QPointF a = d.automatic.at(1), z = d.automatic.at(4);
				const bool first_low = d.vertical ? a.x() <= z.x() : a.y() <= z.y();
				const Terminal *low = first_low ? d.conductor->terminal1 : d.conductor->terminal2;
				const Terminal *high = first_low ? d.conductor->terminal2 : d.conductor->terminal1;
				const int way = d.wire.to > d.wire.from ? 1 : -1;
				ends[i] = qMakePair(QStringLiteral("L%1/%2").arg(quintptr(low)).arg(way),
						    QStringLiteral("H%1/%2").arg(quintptr(high)).arg(way));
				++count[ends.at(i).first];
				++count[ends.at(i).second];
			}
			for (int i = 0 ; i < n ; ++i)
			{
				if (ends.at(i).first.isEmpty()) continue;
				if (count.value(ends.at(i).first) > 1) {
					key[i] = ends.at(i).first;
				} else if (count.value(ends.at(i).second) > 1) {
					key[i] = ends.at(i).second;
				}
			}
		}
		WireLanes::Request request = base;
		request.wires.clear();
		QVector<int> slot(n, -1);
		QHash<QString, int> slot_of;
		for (int i = 0 ; i < n ; ++i)
		{
			const WireLanes::Wire &w = described.at(i).wire;
			const QString &k = key.at(i);
			if (!k.isEmpty() && slot_of.contains(k))
			{
				WireLanes::Wire &m = request.wires[slot_of.value(k)];
				const bool longer = std::abs(w.to - w.from) > std::abs(m.to - m.from);
				if (k.startsWith(QLatin1Char('L'))) {
					m.high = std::min(m.high, w.high);
					if (longer) m.to = w.to;
				} else {
					m.low = std::max(m.low, w.low);
					if (longer) m.from = w.from;
				}
				slot[i] = slot_of.value(k);
				continue;
			}
			slot[i] = request.wires.size();
			request.wires << w;
			if (!k.isEmpty()) slot_of.insert(k, slot.at(i));
		}

		const WireLanes::Result result = WireLanes::arrange(request);
		QSet<int> wanted;
		if (only_their_groups) {
			for (int i = 0 ; i < n ; ++i) {
				if (conductors.contains(described.at(i).conductor) && result.group.at(slot.at(i)) >= 0) {
					wanted << result.group.at(slot.at(i));
				}
			}
		}
		QHash<int, int> group_size;
		for (int i = 0 ; i < n ; ++i) {
			if (result.group.at(slot.at(i)) >= 0) ++group_size[result.group.at(slot.at(i))];
		}
		for (int i = 0 ; i < n ; ++i)
		{
			double at = result.at.at(slot.at(i));
			if (std::isnan(at)) continue;
			if (!described.at(i).shaped && group_size.value(result.group.at(slot.at(i))) == 1) {
				at = roomForName(described.at(i), at, request);
			}
			if (only_their_groups && !wanted.contains(result.group.at(slot.at(i)))) continue;
			const Described &d = described.at(i);
			const QList<QPointF> path = d.shaped ? onFolio(d, shapedPath(d.shape, at))
							     : withLead(d, withBridgeAt(d, at));
			if (!samePath(path, currentPoints(d.conductor))) {
				changes << Change{d.conductor, path};
			}
		}
	};
		//The runs of a bridge keep off the texts of the components (sixth
		//adjustment): a run that would go along a text, from the end of its
		//terminal towards the bridge, ends its room before the text - as the
		//run along an arm of a point does. A text right at the terminal,
		//which no place of the bridge avoids, leaves the wire as it was
	const auto offTexts = [&](QList<Described> &wires, bool up_and_down, double unit) {
		for (Described &o : wires)
		{
			for (const QRectF &box : std::as_const(text_boxes))
			{
				const double low = up_and_down ? box.left() : box.top();
				const double high = up_and_down ? box.right() : box.bottom();
				const double across_low = up_and_down ? box.top() : box.left();
				const double across_high = up_and_down ? box.bottom() : box.right();
				WireLanes::Wire w = o.wire;
				if (o.wire.from > across_low && o.wire.from < across_high
				    && low > o.wire.low && low < o.wire.high) {
					w.high = std::min(w.high, low);
				}
				if (o.wire.to > across_low && o.wire.to < across_high
				    && high < o.wire.high && high > o.wire.low) {
					w.low = std::max(w.low, high);
				}
				if (w.high - w.low >= 2 * unit) o.wire = w;
			}
		}
	};
	offTexts(vertical_wires, true, vertical.unit);
	offTexts(horizontal_wires, false, horizontal.unit);
	collect(vertical_wires, vertical);
	collect(horizontal_wires, horizontal);
	return changes;
}

/**
	@brief WireArrangement::nameLength
	@return the length the letters of the name of @a conductor take along a
	run going @a run, drawn at the size its properties hold - not the
	smaller one they may be drawn at to fit - and turned as they would be
	on that run; 0 when it has no name drawn. The letters, and not the box
	of the text, which carries a blank margin: the name fits a run with 3
	to spare at each end of its letters (T67, fifth adjustment), as on the
	drawing the person approved, an arm of 35 for a name of 28.
*/
qreal WireArrangement::nameLength(const Conductor *conductor, Qt::Orientations run)
{
	const ConductorTextItem *text = conductor ? conductor->textItem() : nullptr;
	const ConductorProperties properties = conductor ? conductor->properties()
							  : ConductorProperties();
	if (!text || properties.type != ConductorProperties::Multi || !text->isVisible()
	    || text->toPlainText().isEmpty()) {
		return 0;
	}
	QFont font = text->font();
	font.setPointSize(properties.text_size);
	const QFontMetricsF metrics(font);
	qreal widest = 0;
	const QStringList lines = text->toPlainText().split(QLatin1Char('\n'));
	for (const QString &line : lines) {
		widest = std::max(widest, metrics.tightBoundingRect(line).width());
	}
	const qreal rotation = text->wasRotatedByUser()
			? text->rotation()
			: (run == Qt::Vertical ? properties.verti_rotate_text
					       : properties.horiz_rotate_text);
		//Written along the run, the letters take their width; across it,
		//the height of their lines
	const bool along = std::abs(std::remainder(rotation - (run == Qt::Vertical ? 90 : 0), 180)) < 45;
	return along ? widest : lines.size() * metrics.height();
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
