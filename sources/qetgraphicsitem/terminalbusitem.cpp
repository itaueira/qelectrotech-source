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
#include "terminalbusitem.h"

#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../TerminalStrip/terminalstripbridge.h"
#include "../TerminalStrip/UndoCommand/bridgeterminalscommand.h"
#include "../TerminalStrip/UndoCommand/changeterminalstripcolor.h"
#include "../wirecolorname.h"
#include "../qetmessagebox.h"
#include "../diagram.h"
#include "../diagramcontent.h"
#include "../properties/elementdata.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "../undocommand/deleteqgraphicsitemcommand.h"
#include "conductor.h"
#include "element.h"
#include "stripframeitem.h"
#include "terminal.h"
#include "terminalelement.h"

#include <QCollator>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>
#include <QHash>
#include <QIcon>
#include <QMenu>
#include <QPixmap>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QUndoCommand>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
		///How far the trunk is from the ends of the terminals
	constexpr qreal trunk_offset{8.0};
		///The trunk wider than the teeth, both wider than a wire
	constexpr qreal trunk_width{3.5};
	constexpr qreal tooth_width{2.2};
		///The trunk is left open this far on each side of a tooth, for the
		///hops: a wire that ends on a joined terminal meets the trunk at its
		///own tooth, and does not hop there
	constexpr qreal tooth_gap{1.5};
		///How wide the band that answers the mouse is
	constexpr qreal grab_width{6.0};
		///The triangle on the feed wire
	constexpr qreal mark_length{6.0};
	constexpr qreal mark_half_width{3.0};

		///How much further out each bus bar is from the one inside it, when
		///two of one side of a strip run along each other
	constexpr qreal lane_pitch{12.0};
		///The hop of a tooth over the trunk of another bus bar: clear of
		///that trunk by 3 on each side
	constexpr qreal tooth_hop_radius{trunk_width / 2 + 3.0};
		///How wide the white round the arc of a tooth's hop is, past the
		///tooth: it cuts the trunk under it, so that two grey bars do not
		///look joined where one passes over the other
	constexpr qreal hop_clearance{1.5};

		/// @a tooth with a hop of radius @a radius over each point of
		/// @a hops: above a tooth that lies, right of one that stands, as a
		/// wire hops (WireHops::path()). With @a arcs_only, the arcs alone
	QPainterPath toothPath(const QLineF &tooth, QList<QPointF> hops, qreal radius, bool arcs_only = false)
	{
		const QPointF a = tooth.p1();
		const QPointF b = tooth.p2();
		const bool lying = std::abs(tooth.dy()) < 0.01;
		const qreal direction = lying ? (b.x() > a.x() ? 1 : -1) : (b.y() > a.y() ? 1 : -1);
		std::sort(hops.begin(), hops.end(), [&a](const QPointF &p, const QPointF &q) {
			return QLineF(a, p).length() < QLineF(a, q).length();
		});
		QPainterPath path(a);
		for (const QPointF &hop : std::as_const(hops))
		{
			const qreal r = std::max(1.5, std::min(radius, std::min(QLineF(a, hop).length(),
										  QLineF(hop, b).length()) - 0.5));
			const QRectF circle(hop.x() - r, hop.y() - r, 2 * r, 2 * r);
			const QPointF start = lying ? QPointF(hop.x() - direction * r, hop.y())
						    : QPointF(hop.x(), hop.y() - direction * r);
			if (arcs_only) {
				path.moveTo(start);
			} else {
				path.lineTo(start);
			}
			if (lying) {
				path.arcTo(circle, direction > 0 ? 180 : 0, direction > 0 ? -180 : 180);
			} else {
				path.arcTo(circle, direction > 0 ? 90 : 270, direction > 0 ? -180 : 180);
			}
		}
		if (!arcs_only) {
			path.lineTo(b);
		}
		return path;
	}

		/// The bus bars of each folio
	QHash<const QGraphicsScene *, QList<QPointer<TerminalBusItem>>> &busesOnScene()
	{
		static QHash<const QGraphicsScene *, QList<QPointer<TerminalBusItem>>> buses;
		return buses;
	}

		/// Where a bus bar is, before it is drawn: the ends of its joined
		/// terminals by its side, the way they point, how far out they reach,
		/// and the stretch of the strip it runs along
	struct Footprint
	{
		bool valid = false;
		struct End { QPointF dock; Qet::Orientation orientation; };
		QList<End> ends;
		Qet::Orientation orientation = Qet::North;
			/// The trunk runs up and down, the ends pointing to the side
		bool vertical = true;
			/// -1 when the ends point left or up
		qreal outward = 1;
			/// The furthest end, across the trunk
		qreal across = 0;
		qreal from = 0;
		qreal to = 0;
		int level = 0;
	};

	Footprint footprintOf(const TerminalBusItem *bus)
	{
		Footprint f;
		const int side = bus->side();
		const QList<Element *> list = bus->terminals();
		for (const Element *element : list)
		{
			const Terminal *terminal = element->terminals().value(side);
			if (terminal) f.ends << Footprint::End{terminal->dockConductor(), terminal->orientation()};
		}
		if (f.ends.isEmpty()) {
			return f;
		}
		f.valid = true;
		f.orientation = f.ends.first().orientation;
		f.vertical = f.orientation == Qet::West || f.orientation == Qet::East;
		f.outward = (f.orientation == Qet::West || f.orientation == Qet::North) ? -1 : 1;
		f.across = f.vertical ? f.ends.first().dock.x() : f.ends.first().dock.y();
		f.from = std::numeric_limits<qreal>::max();
		f.to = std::numeric_limits<qreal>::lowest();
		for (const Footprint::End &end : std::as_const(f.ends))
		{
			const qreal a = f.vertical ? end.dock.x() : end.dock.y();
			const qreal along = f.vertical ? end.dock.y() : end.dock.x();
			f.across = f.outward < 0 ? std::min(f.across, a) : std::max(f.across, a);
			f.from = std::min(f.from, along);
			f.to = std::max(f.to, along);
		}
		const QSharedPointer<TerminalStripBridge> bridge = bus->bridge();
		const QVector<QSharedPointer<RealTerminal>> joined = bridge ? bridge->realTerminals()
									     : QVector<QSharedPointer<RealTerminal>>();
		f.level = joined.isEmpty() || !joined.first() ? 0 : joined.first()->level();
		return f;
	}

		/**
			The lane of @a bus: 0 for the trunk next to the terminals, 1 for
			one further out, and so on. Bus bars of one side of a strip
			whose stretches overlap - the bridges of two levels of the same
			terminals, asked by the Renan on 23/10/2026 - each take one, the
			lower level inside; bus bars that do not overlap share lane 0, as
			two bridges one after the other on one level. Worked out from all
			the bus bars of the folio, the same way by each of them.
		*/
	int laneOf(const TerminalBusItem *bus)
	{
		const Footprint own = footprintOf(bus);
		if (!own.valid || !bus->scene()) {
			return 0;
		}
		struct Placed { const TerminalBusItem *bus; Footprint f; };
		QList<Placed> group;
		const QList<TerminalBusItem *> buses = TerminalBusItem::busesOn(bus->scene());
		for (const TerminalBusItem *other : buses)
		{
			const Footprint f = other == bus ? own : footprintOf(other);
			if (f.valid && f.vertical == own.vertical && f.outward == own.outward
			    && std::abs(f.across - own.across) < 1) {
				group << Placed{other, f};
			}
		}
		std::stable_sort(group.begin(), group.end(), [](const Placed &a, const Placed &b) {
			return a.f.level != b.f.level ? a.f.level < b.f.level : a.f.from < b.f.from;
		});
		QHash<const TerminalBusItem *, int> lanes;
		for (int i = 0 ; i < group.size() ; ++i)
		{
			const Footprint &f = group.at(i).f;
			int lane = 0;
			bool taken = true;
			while (taken)
			{
				taken = false;
				for (int j = 0 ; j < i && !taken ; ++j)
				{
					const Footprint &g = group.at(j).f;
					taken = lanes.value(group.at(j).bus) == lane
						&& f.from <= g.to + lane_pitch && g.from <= f.to + lane_pitch;
				}
				if (taken) ++lane;
			}
			lanes.insert(group.at(i).bus, lane);
		}
		return lanes.value(bus);
	}

		/// The terminal of @a conductor on @a element, or nullptr
	Terminal *endOn(const Conductor *conductor, const Element *element)
	{
		if (conductor->terminal1 && conductor->terminal1->parentElement() == element) return conductor->terminal1;
		if (conductor->terminal2 && conductor->terminal2->parentElement() == element) return conductor->terminal2;
		return nullptr;
	}

		/// How a person names the end @a terminal: the strip and the
		/// terminal, X1:5, or the label of the component
	QString endName(const Terminal *terminal)
	{
		Element *element = terminal ? terminal->parentElement() : nullptr;
		if (!element) return QString();
		QString label = element->actualLabel();
		if (label.isEmpty()) label = element->name();
		QString strip;
		if (const QSharedPointer<RealTerminal> real = TerminalBusItem::realTerminalOf(element)) {
			if (real->parentStrip()) strip = real->parentStrip()->name();
		}
		if (strip.isEmpty() && TerminalBusItem::realTerminalOf(element)) {
			strip = element->elementInformations().value(QETInformation::ELMT_COMMENT).toString().trimmed();
		}
		return strip.isEmpty() ? label : QStringLiteral("%1:%2").arg(strip, label);
	}

		/// A label N.M split in the physical terminal N and its level M
	bool splitLevel(const QString &label, QString *physical, int *level)
	{
		static const QRegularExpression re(QStringLiteral("^(.+)\\.(\\d+)$"));
		const QRegularExpressionMatch match = re.match(label.trimmed());
		if (!match.hasMatch()) {
			return false;
		}
		*physical = match.captured(1);
		*level = match.captured(2).toInt();
		return true;
	}

		/**
			The terminals of a strip made now, in the physical terminals
			they make: those labelled N.M are the levels of the physical
			terminal N, in the order of M (the Renan's X4: 1.1 and 1.2,
			the signal and the common, are the two levels of terminal 1);
			any other is a physical terminal of its own. The physical
			terminals in the order of their names, numbers as numbers.
			Those already in a strip are left out.
		*/
	QVector<QVector<QSharedPointer<RealTerminal>>> groupsOf(const QList<Element *> &members)
	{
		struct Level { int level; QSharedPointer<RealTerminal> real; };
		QStringList keys;
		QHash<QString, QString> sort_key_of;
		QHash<QString, QList<Level>> levels_of;
		for (Element *element : members)
		{
			const QSharedPointer<RealTerminal> real = TerminalBusItem::realTerminalOf(element);
			if (!real || real->parentStrip()) continue;
			QString physical;
			int level = 0;
			QString key;
			if (splitLevel(element->actualLabel(), &physical, &level)) {
				key = QStringLiteral("N:") + physical;
			} else {
				physical = element->actualLabel().trimmed();
				key = QStringLiteral("U:") + element->uuid().toString();
			}
			if (!levels_of.contains(key))
			{
				keys << key;
				sort_key_of.insert(key, physical);
			}
			levels_of[key] << Level{level, real};
		}
		QCollator collator;
		collator.setNumericMode(true);
		std::stable_sort(keys.begin(), keys.end(), [&](const QString &a, const QString &b) {
			return collator.compare(sort_key_of.value(a), sort_key_of.value(b)) < 0;
		});
		QVector<QVector<QSharedPointer<RealTerminal>>> groups;
		for (const QString &key : std::as_const(keys))
		{
			QList<Level> levels = levels_of.value(key);
			std::stable_sort(levels.begin(), levels.end(), [](const Level &a, const Level &b) {
				return a.level < b.level;
			});
			QVector<QSharedPointer<RealTerminal>> group;
			for (const Level &level : std::as_const(levels)) {
				group << level.real;
			}
			groups << group;
		}
		return groups;
	}

		/**
			The terminals of @a diagram in no strip that are levels of the
			terminals N of @a elements - labelled N.M - and in line with the
			first of them: in the same column when @a line spreads taller
			than wide, in the same row else. @a line is what tells the way
			the strip runs: the chosen terminals, a terminal added to a bus
			bar alone included
		*/
	QList<Element *> levelsOf(Diagram *diagram, const QList<Element *> &elements,
				  const QList<Element *> &line)
	{
		QList<Element *> members;
		if (elements.isEmpty()) return members;
		QSet<QString> numbers;
		for (Element *element : elements)
		{
			QString physical;
			int level = 0;
			if (splitLevel(element->actualLabel(), &physical, &level)) numbers << physical;
		}
		QRectF spread;
		for (Element *element : line) {
			spread |= QRectF(element->pos(), QSizeF(0.001, 0.001));
		}
		const bool column = spread.height() >= spread.width();
		const QPointF first_at = elements.first()->pos();
		const QList<Element *> all = diagram->elements();
		for (Element *element : all)
		{
			const QSharedPointer<RealTerminal> real = TerminalBusItem::realTerminalOf(element);
			QString physical;
			int level = 0;
			if (!real || real->parentStrip() || !splitLevel(element->actualLabel(), &physical, &level)
			    || !numbers.contains(physical)) continue;
			const qreal off = column ? element->pos().x() - first_at.x() : element->pos().y() - first_at.y();
			if (qAbs(off) < 1) members << element;
		}
		return members;
	}

		/// The name of the physical terminal @a group makes: N for the
		/// levels N.M, else the label of its terminal
	QString physicalName(const QVector<QSharedPointer<RealTerminal>> &group)
	{
		Element *element = group.isEmpty() || !group.first() ? nullptr : group.first()->element();
		if (!element) return QString();
		QString physical;
		int level = 0;
		return splitLevel(element->actualLabel(), &physical, &level) ? physical
									     : element->actualLabel().trimmed();
	}

		/**
			@a added, the physical terminals of terminals put in a strip that
			has @a groups already, each put before the first of the strip
			whose name comes after its own - numbers as numbers -, at the end
			when none does: terminal 5 added to the strip 1 to 4 goes after
			4, terminal 0 before 1. @a groups becomes the strip made; the
			places, in the order of @a added, are where each went, one after
			the other (TerminalBusItem::join(), JoinBusCommand).
		*/
	QVector<int> insertInOrder(QVector<QVector<QSharedPointer<RealTerminal>>> &groups,
				   const QVector<QVector<QSharedPointer<RealTerminal>>> &added)
	{
		QCollator collator;
		collator.setNumericMode(true);
		QVector<int> places;
		for (const auto &group : added)
		{
			const QString name = physicalName(group);
			int at = groups.size();
			for (int i = 0 ; i < groups.size() ; ++i)
			{
				if (collator.compare(physicalName(groups.at(i)), name) > 0)
				{
					at = i;
					break;
				}
			}
			groups.insert(at, group);
			places << at;
		}
		return places;
	}

		/// Whether the strip made of @a groups lets @a joined be bridged:
		/// one level, neighbouring physical terminals (TerminalStrip::isBridgeable())
	bool plannedBridgeable(const QVector<QVector<QSharedPointer<RealTerminal>>> &groups,
			       const QVector<QSharedPointer<RealTerminal>> &joined)
	{
		QList<int> positions;
		int level = -1;
		for (const QSharedPointer<RealTerminal> &real : joined)
		{
			int found_group = -1, found_level = -1;
			for (int g = 0 ; g < groups.size() && found_group < 0 ; ++g)
			{
				const int l = groups.at(g).indexOf(real);
				if (l >= 0)
				{
					found_group = g;
					found_level = l;
				}
			}
			if (found_group < 0 || positions.contains(found_group)) return false;
			if (level >= 0 && found_level != level) return false;
			level = found_level;
			positions << found_group;
		}
		std::sort(positions.begin(), positions.end());
		return positions.size() >= 2 && positions.last() - positions.first() == positions.size() - 1;
	}

		/**
			Joining terminals into a bus bar, in one undo step (T87): the
			strip made if none of them is in one, the terminals put in it,
			the bridge, its side and its feed; the wires the bridge makes
			redundant go with the children of this command.

			Done in redo and not in the constructor: the strip and its
			physical terminals do not exist before the first redo, and the
			bridge command takes its terminals then (BridgeTerminalsCommand).
		*/
	class JoinBusCommand : public QUndoCommand
	{
		public:
				/// @a places: where each group of @a to_add goes in a
				/// strip that exists, see insertInOrder(); none: at the end
			JoinBusCommand(Diagram *diagram, TerminalStrip *strip, const QString &new_strip_name,
				       const QVector<QVector<QSharedPointer<RealTerminal>>> &to_add,
				       const QVector<QSharedPointer<RealTerminal>> &joined,
				       int side, const QUuid &feed, const QVector<int> &places = QVector<int>()) :
				m_diagram(diagram),
				m_project(diagram ? diagram->project() : nullptr),
				m_strip(strip),
				m_new_name(new_strip_name),
				m_to_add(to_add),
				m_places(places),
				m_joined(joined),
				m_side(side),
				m_feed(feed)
			{
				setText(TerminalBusItem::tr("Join into a bus bar"));
				for (const auto &group : to_add) {
					m_added << group;
				}
			}
			~JoinBusCommand() override {delete m_bridge_command;}

			void redo() override
			{
				if (!m_project) return;
				if (!m_strip)
				{
					m_strip = m_project->newTerminalStrip(QString(), QString(), m_new_name);
					m_created = true;
				}
				else if (m_created) {
					m_project->addTerminalStrip(m_strip);
				}
				if (!m_to_add.isEmpty()) {
					m_strip->addAndGroupTerminals(m_to_add);
				}
					//The physical terminals the strip made last, each put
					//where its name goes: before the bridge is made, which
					//joins neighbours only
				if (m_places.size() == m_to_add.size() && !m_places.isEmpty())
				{
					QVector<QSharedPointer<PhysicalTerminal>> order = m_strip->physicalTerminal();
					const int count = m_to_add.size();
					if (order.size() >= count)
					{
						const QVector<QSharedPointer<PhysicalTerminal>> made = order.mid(order.size() - count);
						order.resize(order.size() - count);
						for (int k = 0 ; k < count ; ++k) {
							order.insert(std::min<int>(m_places.at(k), order.size()), made.at(k));
						}
						m_strip->setOrderTo(order);
					}
				}
				if (!m_bridge_command) {
					m_bridge_command = new BridgeTerminalsCommand(m_strip, m_joined);
				}
				m_bridge_command->redo();
				if (const QSharedPointer<TerminalStripBridge> bridge = m_strip->isBridged(m_joined.first()))
				{
					if (m_first)
					{
						m_old_side = bridge->side();
						m_old_feed = bridge->feed();
					}
					m_bridge = bridge;
					bridge->setSide(m_side);
					bridge->setFeed(m_feed);
				}
				m_first = false;
					//The redundant wires
				QUndoCommand::redo();
				TerminalBusItem::syncFolio(m_diagram);
			}

			void undo() override
			{
				QUndoCommand::undo();
				if (const QSharedPointer<TerminalStripBridge> bridge = m_bridge.toStrongRef())
				{
					bridge->setSide(m_old_side);
					bridge->setFeed(m_old_feed);
				}
				if (m_bridge_command) {
					m_bridge_command->undo();
				}
				if (m_strip && !m_added.isEmpty()) {
					m_strip->removeTerminals(m_added);
				}
				if (m_created && m_strip && m_project) {
					m_project->removeTerminalStrip(m_strip);
				}
				TerminalBusItem::syncFolio(m_diagram);
			}

		private:
			QPointer<Diagram> m_diagram;
			QPointer<QETProject> m_project;
			QPointer<TerminalStrip> m_strip;
			QString m_new_name;
			bool m_created = false;
			bool m_first = true;
			QVector<QVector<QSharedPointer<RealTerminal>>> m_to_add;
			QVector<int> m_places;
			QVector<QSharedPointer<RealTerminal>> m_added;
			QVector<QSharedPointer<RealTerminal>> m_joined;
			BridgeTerminalsCommand *m_bridge_command = nullptr;
			QWeakPointer<TerminalStripBridge> m_bridge;
			int m_side = -1;
			int m_old_side = -1;
			QUuid m_feed;
			QUuid m_old_feed;
	};

		/// The wire that feeds a bridge, chosen, and back with the undo
	class BusFeedCommand : public QUndoCommand
	{
		public:
			BusFeedCommand(const QSharedPointer<TerminalStripBridge> &bridge, Diagram *diagram,
				       const QUuid &feed) :
				m_bridge(bridge),
				m_diagram(diagram),
				m_before(bridge->feed()),
				m_after(feed)
			{
				setText(TerminalBusItem::tr("Choose the feed wire of the bus bar"));
			}
			void redo() override {apply(m_after);}
			void undo() override {apply(m_before);}

		private:
			void apply(const QUuid &feed)
			{
				if (const QSharedPointer<TerminalStripBridge> bridge = m_bridge.toStrongRef()) {
					bridge->setFeed(feed);
				}
				TerminalBusItem::syncFolio(m_diagram);
			}

			QWeakPointer<TerminalStripBridge> m_bridge;
			QPointer<Diagram> m_diagram;
			QUuid m_before;
			QUuid m_after;
	};
}

/**
	@brief TerminalBusItem::TerminalBusItem
	The bus bar of @a bridge. Over the components, whose terminals its teeth
	reach, and under the wires (Conductor: 11): a wire that crosses the bar
	hops over it, and the hop is drawn on the bar and not hidden by it
	(asked by the Renan on 23/10/2026, the bar drawn over the hops).
*/
TerminalBusItem::TerminalBusItem(const QSharedPointer<TerminalStripBridge> &bridge, QGraphicsItem *parent) :
	QetGraphicsItem(parent),
	m_bridge(bridge)
{
	setMovable(false);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setZValue(10.5);
}

QString TerminalBusItem::name() const
{
	return tr("a bus bar");
}

QSharedPointer<TerminalStripBridge> TerminalBusItem::bridge() const
{
	return m_bridge.toStrongRef();
}

/**
	@brief TerminalBusItem::realTerminalOf
	@return the terminal of a strip @a element is, when it is a component of
	the terminal type; null otherwise
*/
QSharedPointer<RealTerminal> TerminalBusItem::realTerminalOf(const Element *element)
{
	if (!element || element->elementData().m_type != ElementData::Terminal) {
		return QSharedPointer<RealTerminal>();
	}
	const auto *terminal_element = qobject_cast<const TerminalElement *>(element);
	return terminal_element ? terminal_element->realTerminal() : QSharedPointer<RealTerminal>();
}

/**
	@brief TerminalBusItem::terminals
	@return the terminals the bridge joins that are on the folio of this bus
	bar, in the order of the bridge
*/
QList<Element *> TerminalBusItem::terminals() const
{
	QList<Element *> list;
	const QSharedPointer<TerminalStripBridge> bridge_ = bridge();
	if (!bridge_) {
		return list;
	}
	const QVector<QSharedPointer<RealTerminal>> joined = bridge_->realTerminals();
	for (const QSharedPointer<RealTerminal> &real : joined)
	{
		Element *element = real ? real->element() : nullptr;
		if (element && element->scene() && (!scene() || element->scene() == scene())) {
			list << element;
		}
	}
	return list;
}

/**
	@brief TerminalBusItem::side
	@return the terminal of its components the bus bar is drawn by, the
	index in Element::terminals(): the side the bridge keeps, the first
	terminal when it keeps none
*/
int TerminalBusItem::side() const
{
	const QSharedPointer<TerminalStripBridge> bridge_ = bridge();
	return bridge_ && bridge_->side() >= 0 ? bridge_->side() : 0;
}

/**
	@brief TerminalBusItem::wires
	@return the wires on the terminals of the bus bar on its folio, but for
	one between two of them
*/
QList<Conductor *> TerminalBusItem::wires() const
{
	QList<Conductor *> list;
	const QList<Element *> joined = terminals();
	for (Element *element : joined)
	{
		const QList<Conductor *> conductors = element->conductors();
		for (Conductor *conductor : conductors)
		{
			if (list.contains(conductor)) continue;
			const Element *a = conductor->terminal1 ? conductor->terminal1->parentElement() : nullptr;
			const Element *b = conductor->terminal2 ? conductor->terminal2->parentElement() : nullptr;
			if (joined.contains(const_cast<Element *>(a)) && joined.contains(const_cast<Element *>(b))) continue;
			list << conductor;
		}
	}
	return list;
}

/**
	@brief TerminalBusItem::feed
	@return the wire that feeds the bridge, when it is on this folio
*/
Conductor *TerminalBusItem::feed() const
{
	const QSharedPointer<TerminalStripBridge> bridge_ = bridge();
	if (!bridge_ || bridge_->feed().isNull()) {
		return nullptr;
	}
	const QList<Conductor *> list = wires();
	for (Conductor *conductor : list) {
		if (conductor->uuid() == bridge_->feed()) return conductor;
	}
	return nullptr;
}

QLineF TerminalBusItem::trunk() const
{
	computeGeometry();
	return m_trunk;
}

QVector<QLineF> TerminalBusItem::teeth() const
{
	computeGeometry();
	return m_teeth;
}

QPolygonF TerminalBusItem::feedMark() const
{
	computeGeometry();
	return m_feed_mark;
}

/**
	@brief TerminalBusItem::terminalsChanged
	A terminal moved, or the bridge changed: worked out again, and the hops
	of the wires crossing it with it
*/
void TerminalBusItem::terminalsChanged()
{
		//The others of the folio too: one moved or made longer may take or
		//give back the lane of another (laneOf())
	QList<TerminalBusItem *> buses = scene() ? busesOn(scene()) : QList<TerminalBusItem *>();
	if (!buses.contains(this)) {
		buses << this;
	}
	for (TerminalBusItem *bus : std::as_const(buses))
	{
		bus->prepareGeometryChange();
		bus->m_dirty = true;
		bus->update();
	}
	restack(buses);
	Conductor::othersChangedShape();
}

/**
	@brief TerminalBusItem::restack
	Each of @a buses a little higher the further out its lane (laneOf()):
	the tooth of a bus bar outside hops over the trunk of the one inside,
	and is drawn over it, whichever was made first
*/
void TerminalBusItem::restack(const QList<TerminalBusItem *> &buses)
{
	for (TerminalBusItem *bus : buses)
	{
		if (bus && bus->scene()) bus->setZValue(10.5 + 0.01 * laneOf(bus));
	}
}

/**
	@brief TerminalBusItem::toothHops
	@return where the teeth of this bus bar cross the trunk of another bus
	bar of the folio, in scene coordinates: the tooth of a bus bar further
	out passes over the trunk of the one inside it, and hops over it, as a
	wire does - a tooth drawn straight across it would join the two
*/
QList<QPointF> TerminalBusItem::toothHops() const
{
	QList<QPointF> hops;
	if (!scene()) {
		return hops;
	}
	const QVector<QLineF> own = teeth();
	const QList<TerminalBusItem *> buses = busesOn(scene());
	for (const TerminalBusItem *other : buses)
	{
		if (other == this) continue;
		const QLineF trunk_ = other->trunk();
		if (trunk_.isNull()) continue;
		const bool standing = std::abs(trunk_.dx()) < 0.01;
		for (const QLineF &tooth : own)
		{
			const bool lying_tooth = std::abs(tooth.dy()) < 0.01;
			if (standing != lying_tooth) continue;
			const QPointF crossing = standing ? QPointF(trunk_.p1().x(), tooth.p1().y())
							  : QPointF(tooth.p1().x(), trunk_.p1().y());
			const auto inside = [](qreal value, qreal a, qreal b, qreal margin) {
				return value > std::min(a, b) + margin && value < std::max(a, b) - margin;
			};
			const bool on_tooth = standing ? inside(crossing.x(), tooth.p1().x(), tooth.p2().x(), 1)
						       : inside(crossing.y(), tooth.p1().y(), tooth.p2().y(), 1);
			const bool on_trunk = standing ? inside(crossing.y(), trunk_.p1().y(), trunk_.p2().y(), -trunk_width / 2)
						       : inside(crossing.x(), trunk_.p1().x(), trunk_.p2().x(), -trunk_width / 2);
			if (on_tooth && on_trunk) hops << crossing;
		}
	}
	return hops;
}

void TerminalBusItem::watchTerminals()
{
	for (const QMetaObject::Connection &connection : std::as_const(m_connections)) {
		disconnect(connection);
	}
	m_connections.clear();
	const QList<Element *> list = terminals();
	for (Element *element : list)
	{
		m_connections << connect(element, &QGraphicsObject::xChanged, this, &TerminalBusItem::terminalsChanged);
		m_connections << connect(element, &QGraphicsObject::yChanged, this, &TerminalBusItem::terminalsChanged);
		m_connections << connect(element, &QGraphicsObject::rotationChanged, this, &TerminalBusItem::terminalsChanged);
		m_connections << connect(element, &QObject::destroyed, this, &TerminalBusItem::terminalsChanged);
	}
	terminalsChanged();
}

/**
	@brief TerminalBusItem::computeGeometry
	The trunk off the ends of the terminals, on the side of the strip the
	bus bar is drawn by, as long as the teeth reach; a tooth from each
	joined terminal to it. In scene coordinates: the item stays at the
	origin
*/
void TerminalBusItem::computeGeometry() const
{
	if (!m_dirty) {
		return;
	}
	m_dirty = false;
	m_trunk = QLineF();
	m_teeth.clear();
	m_feed_mark.clear();

	const Footprint footprint = footprintOf(this);
	if (!footprint.valid) {
		return;
	}
	const int side_ = side();
	const QList<Element *> list = terminals();
	const bool vertical_trunk = footprint.vertical;
	const qreal outward = footprint.outward;
	const qreal from = footprint.from;
	const qreal to = footprint.to;
		//Further out, past another bus bar of the same terminals (laneOf())
	const qreal trunk_at = footprint.across + outward * (trunk_offset + laneOf(this) * lane_pitch);
	m_trunk = vertical_trunk ? QLineF(trunk_at, from, trunk_at, to) : QLineF(from, trunk_at, to, trunk_at);
	for (const Footprint::End &end : std::as_const(footprint.ends))
	{
		const QPointF foot = vertical_trunk ? QPointF(trunk_at, end.dock.y()) : QPointF(end.dock.x(), trunk_at);
		m_teeth << QLineF(foot, end.dock);
	}

		//The feed: a triangle on its wire just outside the trunk, pointing
		//into the bus bar; on the other side of the strip, just outside the
		//terminal
	Conductor *feed_ = feed();
	if (!feed_) {
		return;
	}
	for (const Element *element : list)
	{
		const Terminal *terminal = endOn(feed_, element);
		if (!terminal) continue;
		const QPointF dock = terminal->dockConductor();
		const bool on_side = element->terminals().indexOf(const_cast<Terminal *>(terminal)) == side_;
		QPointF tip;
		QPointF into;
		if (on_side)
		{
			tip = vertical_trunk ? QPointF(trunk_at + outward * (trunk_width / 2 + 0.5), dock.y())
					     : QPointF(dock.x(), trunk_at + outward * (trunk_width / 2 + 0.5));
			into = vertical_trunk ? QPointF(-outward, 0) : QPointF(0, -outward);
		}
		else
		{
			const Qet::Orientation o = terminal->orientation();
			const QPointF away = o == Qet::West ? QPointF(-1, 0) : o == Qet::East ? QPointF(1, 0)
					   : o == Qet::North ? QPointF(0, -1) : QPointF(0, 1);
			tip = dock + away * 1.0;
			into = -away;
		}
		const QPointF back = tip - into * mark_length;
		const QPointF normal(-into.y(), into.x());
		m_feed_mark << tip << back + normal * mark_half_width << back - normal * mark_half_width;
		break;
	}
}

QRectF TerminalBusItem::boundingRect() const
{
	computeGeometry();
	if (m_teeth.isEmpty()) {
		return QRectF();
	}
	QRectF rect = QRectF(m_trunk.p1(), m_trunk.p2()).normalized();
	for (const QLineF &tooth : std::as_const(m_teeth)) {
		rect |= QRectF(tooth.p1(), tooth.p2()).normalized();
	}
	rect |= m_feed_mark.boundingRect();
	const qreal margin = trunk_width + 4;
	return rect.adjusted(-margin, -margin, margin, margin);
}

/**
	@brief TerminalBusItem::shape
	@return the trunk and the teeth, widened into a band a hand can catch,
	and the mark of the feed
*/
QPainterPath TerminalBusItem::shape() const
{
	computeGeometry();
	QPainterPath lines;
	if (m_teeth.isEmpty()) {
		return lines;
	}
	lines.moveTo(m_trunk.p1());
	lines.lineTo(m_trunk.p2());
	for (const QLineF &tooth : std::as_const(m_teeth))
	{
		lines.moveTo(tooth.p1());
		lines.lineTo(tooth.p2());
	}
	QPainterPathStroker stroker;
	stroker.setWidth(grab_width);
	QPainterPath path = stroker.createStroke(lines);
	if (!m_feed_mark.isEmpty()) {
		path.addPolygon(m_feed_mark);
	}
	return path;
}

void TerminalBusItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)
	computeGeometry();
	if (m_teeth.isEmpty()) {
		return;
	}
	const QSharedPointer<TerminalStripBridge> bridge_ = bridge();
	QColor color = bridge_ ? bridge_->color() : QColor(Qt::darkGray);
		//A white bridge, which the strip draws on its grey, would not show
		//on the white of a folio
	if (color.lightness() > 200) {
		color = color.darker(160);
	}
	painter->save();
	painter->setBrush(Qt::NoBrush);
	if (isSelected() || isHovered())
	{
		QPen halo(QColor(0, 120, 215, isSelected() ? 110 : 60), trunk_width + 4, Qt::SolidLine, Qt::RoundCap);
		painter->setPen(halo);
		painter->drawLine(m_trunk);
		for (const QLineF &tooth : std::as_const(m_teeth)) {
			painter->drawLine(tooth);
		}
	}
	painter->setPen(QPen(color, tooth_width, Qt::SolidLine, Qt::FlatCap));
		//Over the trunk of a bus bar inside this one, a hop (toothHops()),
		//wider than that trunk: the two bars plainly apart
	const QList<QPointF> hops = toothHops();
	for (const QLineF &tooth : std::as_const(m_teeth))
	{
		QList<QPointF> here;
		for (const QPointF &hop : hops)
		{
			if (QLineF(tooth.p1(), hop).length() + QLineF(hop, tooth.p2()).length()
			    < tooth.length() + 0.01) here << hop;
		}
		if (here.isEmpty()) {
			painter->drawLine(tooth);
			continue;
		}
			//The colour of the folio round the arcs first, over the trunk
			//under them, then the tooth with its arcs, the joins round
		const QPen tooth_pen = painter->pen();
		painter->setPen(QPen(Diagram::background_color, tooth_width + 2 * hop_clearance, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
		painter->drawPath(toothPath(tooth, here, tooth_hop_radius, true));
		painter->setPen(QPen(color, tooth_width, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
		painter->drawPath(toothPath(tooth, here, tooth_hop_radius));
		painter->setPen(tooth_pen);
	}
	painter->setPen(QPen(color, trunk_width, Qt::SolidLine, Qt::SquareCap));
	painter->drawLine(m_trunk);
	if (!m_feed_mark.isEmpty())
	{
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor(Qt::black));
		painter->drawPolygon(m_feed_mark);
	}
	painter->restore();
}

QVariant TerminalBusItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == QGraphicsItem::ItemSceneChange && scene())
	{
		QList<QPointer<TerminalBusItem>> &list = busesOnScene()[scene()];
		list.removeAll(this);
			//Those left may take back the lane this one had (laneOf())
		QList<TerminalBusItem *> others;
		for (const QPointer<TerminalBusItem> &other : std::as_const(list))
		{
			if (!other) continue;
			other->prepareGeometryChange();
			other->m_dirty = true;
			other->update();
			others << other;
		}
		restack(others);
		if (list.isEmpty()) busesOnScene().remove(scene());
		Conductor::othersChangedShape();
	}
	else if (change == QGraphicsItem::ItemSceneHasChanged && scene())
	{
		busesOnScene()[scene()] << this;
		watchTerminals();
	}
	return QetGraphicsItem::itemChange(change, value);
}

void TerminalBusItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	Diagram *folio = diagram();
	if (!folio || folio->isReadOnly()) {
		QetGraphicsItem::contextMenuEvent(event);
		return;
	}
	QMenu menu;
	QMenu *feed_menu = menu.addMenu(tr("Feed wire"));
	const QList<Conductor *> list = wires();
	const Conductor *current = feed();
	QHash<QAction *, Conductor *> choices;
	for (Conductor *conductor : list)
	{
		const QList<Element *> joined = terminals();
		const Element *a = conductor->terminal1 ? conductor->terminal1->parentElement() : nullptr;
		const Terminal *other = joined.contains(const_cast<Element *>(a)) ? conductor->terminal2 : conductor->terminal1;
		QString number = conductor->properties().text.trimmed();
		if (number.isEmpty()) number = tr("(no number)");
		QAction *action = feed_menu->addAction(QStringLiteral("%1 — %2").arg(number, endName(other)));
		action->setCheckable(true);
		action->setChecked(conductor == current);
		choices.insert(action, conductor);
	}
	feed_menu->addSeparator();
	QAction *none = feed_menu->addAction(tr("None"));
	none->setCheckable(true);
	none->setChecked(!current);
		//The colour of the jumper, the one the strip manager offers
	QMenu *color_menu = menu.addMenu(tr("Color"));
	QHash<QAction *, QColor> colors;
	const QSharedPointer<TerminalStripBridge> bridge_ = bridge();
	const QVector<QColor> palette = TerminalStripBridge::bridgeColor();
	for (const QColor &color : palette)
	{
		QPixmap swatch(16, 16);
		swatch.fill(color);
		QAction *action = color_menu->addAction(QIcon(swatch), WireColorName::name(color));
		action->setCheckable(true);
		action->setChecked(bridge_ && bridge_->color() == color);
		colors.insert(action, color);
	}
		//The terminals selected with it added to it, as Join into a bus
		//bar does with this selection
	const QList<Element *> selected = joinable(folio->selectedItems());
	const QList<Element *> joined_now = terminals();
	bool more = false;
	for (const Element *element : selected) {
		if (!joined_now.contains(const_cast<Element *>(element))) more = true;
	}
	QAction *add = menu.addAction(tr("Add the selected terminals"));
	add->setEnabled(more);
	add->setStatusTip(tr("Select the bus bar and the terminals to add, then choose this"));
	menu.addSeparator();
	QAction *erase = menu.addAction(tr("Remove the bus bar"));
	QAction *chosen = menu.exec(event->screenPos());
	if (chosen == add)
	{
		QString why;
		if (!join(folio, selected, QString(), &why) && !why.isEmpty())
		{
			QWidget *parent = folio->views().isEmpty() ? nullptr : folio->views().first()->window();
			QET::QetMessageBox::information(parent, tr("Add the selected terminals"), why);
		}
	}
	else if (chosen == erase) {
		remove(this);
	} else if (chosen == none) {
		setFeed(this, nullptr);
	} else if (choices.contains(chosen)) {
		setFeed(this, choices.value(chosen));
	} else if (colors.contains(chosen)) {
		setColor(this, colors.value(chosen));
	}
	event->accept();
}

/**
	@brief TerminalBusItem::busesOn
	@return the bus bars of @a scene
*/
QList<TerminalBusItem *> TerminalBusItem::busesOn(const QGraphicsScene *scene)
{
	QList<TerminalBusItem *> list;
	const QList<QPointer<TerminalBusItem>> buses = busesOnScene().value(scene);
	for (const QPointer<TerminalBusItem> &bus : buses) {
		if (bus) list << bus;
	}
	return list;
}

/**
	@brief TerminalBusItem::trunksOn
	@return the trunks of the bus bars of @a scene, in scene coordinates,
	for the hops of the wires that cross them: each one left open at its
	teeth, where the wires of the joined terminals meet it
*/
QList<QVector<QPointF>> TerminalBusItem::trunksOn(const QGraphicsScene *scene)
{
	QList<QVector<QPointF>> list;
	const QList<TerminalBusItem *> buses = busesOn(scene);
	for (const TerminalBusItem *bus : buses)
	{
		const QLineF line = bus->trunk();
		const QVector<QLineF> teeth_ = bus->teeth();
		if (line.length() <= 0) continue;
		const bool vertical = qAbs(line.dx()) < qAbs(line.dy());
		QList<qreal> stops;
		for (const QLineF &tooth : teeth_) {
			stops << (vertical ? tooth.p1().y() : tooth.p1().x());
		}
		std::sort(stops.begin(), stops.end());
		qreal start = vertical ? std::min(line.p1().y(), line.p2().y()) : std::min(line.p1().x(), line.p2().x());
		const qreal end = vertical ? std::max(line.p1().y(), line.p2().y()) : std::max(line.p1().x(), line.p2().x());
		const qreal across = vertical ? line.p1().x() : line.p1().y();
		const auto point = [vertical, across](qreal along) {
			return vertical ? QPointF(across, along) : QPointF(along, across);
		};
		for (const qreal stop : std::as_const(stops))
		{
			if (stop - tooth_gap > start + 0.01) list << QVector<QPointF>{point(start), point(stop - tooth_gap)};
			start = std::max(start, stop + tooth_gap);
		}
		if (end > start + 0.01) list << QVector<QPointF>{point(start), point(end)};
	}
	return list;
}

/**
	@brief TerminalBusItem::wireStart
	@return true when @a terminal is the end of a joined terminal by which
	its bus bar is drawn, with in @a start, in scene coordinates, where a
	wire leaving it starts to be drawn: just past the trunk, on the line of
	the terminal. The tooth joins the terminal to the trunk, and a wire drawn
	over the tooth and across the trunk hid them both (asked by the Renan on
	23/10/2026: the wire starts after the trunk). The wire is still joined
	to the end of the terminal: only its drawing starts there.
*/
bool TerminalBusItem::wireStart(const Terminal *terminal, QPointF *start)
{
	const Element *element = terminal ? terminal->parentElement() : nullptr;
	if (!element || !terminal->scene() || !start) {
		return false;
	}
	const QList<TerminalBusItem *> buses = busesOn(terminal->scene());
	for (const TerminalBusItem *bus : buses)
	{
		if (!bus->terminals().contains(const_cast<Element *>(element))) continue;
		if (element->terminals().value(bus->side()) != terminal) return false;
		const QLineF line = bus->trunk();
		if (line.isNull()) return false;
		const QPointF dock = terminal->dockConductor();
		const Qet::Orientation orientation = terminal->orientation();
		const qreal outward = (orientation == Qet::West || orientation == Qet::North) ? -1 : 1;
		*start = (orientation == Qet::West || orientation == Qet::East)
				? QPointF(line.p1().x() + outward * trunk_width / 2, dock.y())
				: QPointF(dock.x(), line.p1().y() + outward * trunk_width / 2);
		return true;
	}
	return false;
}

/**
	@brief TerminalBusItem::syncFolio
	A bus bar on @a diagram for each bridge that joins two or more of its
	terminals, and none for a bridge that is gone: what the bridges of the
	strips are now
*/
void TerminalBusItem::syncFolio(Diagram *diagram)
{
	if (!diagram) {
		return;
	}
	QHash<TerminalStripBridge *, QSharedPointer<TerminalStripBridge>> bridges;
	QHash<TerminalStripBridge *, int> count;
	const QList<Element *> elements = diagram->elements();
	for (Element *element : elements)
	{
		const QSharedPointer<RealTerminal> real = realTerminalOf(element);
		const QSharedPointer<TerminalStripBridge> bridge_ = real ? real->bridge() : QSharedPointer<TerminalStripBridge>();
		if (!bridge_) continue;
		bridges.insert(bridge_.data(), bridge_);
		++count[bridge_.data()];
	}
	bool changed = false;
	const QList<TerminalBusItem *> existing = busesOn(diagram);
	QSet<TerminalStripBridge *> drawn;
	for (TerminalBusItem *bus : existing)
	{
		const QSharedPointer<TerminalStripBridge> bridge_ = bus->bridge();
		if (!bridge_ || count.value(bridge_.data()) < 2 || drawn.contains(bridge_.data()))
		{
			diagram->removeItem(bus);
			bus->deleteLater();
			changed = true;
			continue;
		}
		drawn.insert(bridge_.data());
		bus->watchTerminals();
	}
	for (auto it = bridges.cbegin() ; it != bridges.cend() ; ++it)
	{
		if (drawn.contains(it.key()) || count.value(it.key()) < 2) continue;
		diagram->addItem(new TerminalBusItem(it.value()));
		changed = true;
	}
	if (changed) {
		Conductor::othersChangedShape();
		diagram->update();
	}
}

/**
	@brief TerminalBusItem::nameForNewStrip
	@return the name of the strip @a selection would be put in when none of
	its terminals is in one: the drawn strip they are in, else what their
	comment says (T85)
*/
QString TerminalBusItem::nameForNewStrip(const QList<Element *> &selection)
{
	QList<Element *> terminals;
	for (Element *element : selection) {
		if (realTerminalOf(element)) terminals << element;
	}
	return StripFrameItem::suggestedName(terminals).trimmed();
}

/**
	@brief TerminalBusItem::needsStripName
	@return true when joining @a selection makes a strip that has no name
	to take: the person is asked for one
*/
bool TerminalBusItem::needsStripName(const QList<Element *> &selection)
{
	for (Element *element : selection)
	{
		const QSharedPointer<RealTerminal> real = realTerminalOf(element);
		if (real && real->parentStrip()) return false;
	}
	return nameForNewStrip(selection).isEmpty();
}

/**
	@brief TerminalBusItem::join
	The terminals of @a selection joined into a bus bar, in one undo step
	(T87). They are put in one strip of the strip manager - the strip
	@a new_strip_name is made when none of them is in one, with the
	terminals of their drawn strip -, the bridge is made, and the wires it
	makes redundant go: a wire between two joined terminals, and of the
	wires that join the bus bar to one same point out of it, all but one -
	the arm, when they are the branches of an arm (T86). The bus bar is
	drawn by the side of the strip where the fewest wires are left, the
	inside of the panel - by the side of the other bus bars of the strip
	when it has some -; the one wire left on that side is its feed.

	A terminal of a bus bar among them brings the whole bus bar, and the
	others are added to it - by its side, with its feed - (asked by the
	Renan on 23/10/2026): the terminals in no strip yet go into the strip of
	the bus bar, with the other levels of theirs, each where its name goes
	among the terminals of the strip. Terminals of no strip whose strip the
	project has already, by name, go into it the same way: a strip has a
	bus bar per level, and several on one level.
	@param message : why nothing was done, when nothing was
	@return true when the terminals were joined
*/
bool TerminalBusItem::join(Diagram *diagram, const QList<Element *> &selection,
			   const QString &new_strip_name, QString *message)
{
	const auto fail = [message](const QString &why) {
		if (message) *message = why;
		return false;
	};
	if (!diagram || diagram->isReadOnly() || !diagram->project()) {
		return fail(QString());
	}
	QList<Element *> chosen;
	QVector<QSharedPointer<RealTerminal>> joined;
	const auto choose = [&](Element *element) {
		const QSharedPointer<RealTerminal> real = realTerminalOf(element);
		if (real && !chosen.contains(element) && element->diagram() == diagram)
		{
			chosen << element;
			joined << real;
		}
	};
	for (Element *element : selection) {
		choose(element);
	}
		//A terminal of a bus bar brings the whole bus bar: what is chosen
		//with it is added to it (asked by the Renan on 23/10/2026, the bus
		//bar could not be given another terminal)
	QSharedPointer<TerminalStripBridge> existing;
	for (int i = 0 ; i < joined.size() ; ++i)
	{
		const QSharedPointer<TerminalStripBridge> bridge_ = joined.at(i)->bridge();
		if (!bridge_) continue;
		if (existing && existing != bridge_) {
			return fail(tr("The terminals are in two bus bars."));
		}
		existing = bridge_;
		const QVector<QSharedPointer<RealTerminal>> members = bridge_->realTerminals();
		for (const QSharedPointer<RealTerminal> &member : members)
		{
			if (Element *element = member ? member->element() : nullptr) choose(element);
		}
	}
	if (chosen.size() < 2) {
		return fail(tr("Select two or more terminals of a strip."));
	}
	if (existing && chosen.size() == existing->realTerminals().size())
	{
		bool all_in = true;
		for (const QSharedPointer<RealTerminal> &real : std::as_const(joined)) {
			if (real->bridge() != existing) all_in = false;
		}
		if (all_in) {
			return fail(tr("Select the terminals to add with the bus bar."));
		}
	}

		//The strip
	QSet<TerminalStrip *> strips;
	bool free_one = false;
	for (const QSharedPointer<RealTerminal> &real : std::as_const(joined))
	{
		if (real->parentStrip()) strips << real->parentStrip();
		else free_one = true;
	}
	if (strips.size() > 1) {
		return fail(tr("The terminals are in different strips."));
	}
	TerminalStrip *strip = strips.isEmpty() ? nullptr : *strips.cbegin();
	QVector<QVector<QSharedPointer<RealTerminal>>> to_add;
	QVector<int> places;
	QString name;
		//None of them in a strip, and the project has the strip of their
		//name already: they go into it, with its other bus bars. A second
		//bus bar of a strip made a second strip of the same name before
		//(asked by the Renan on 23/10/2026: a strip has several bus bars)
	if (!strip)
	{
		QString wanted = nameForNewStrip(chosen);
		if (wanted.isEmpty()) wanted = new_strip_name.trimmed();
		const QVector<TerminalStrip *> existing_strips = diagram->project()->terminalStrip();
		for (TerminalStrip *candidate : existing_strips)
		{
			if (!wanted.isEmpty() && candidate && candidate->name() == wanted)
			{
				strip = candidate;
				break;
			}
		}
	}
	if (strip && free_one)
	{
			//Terminals put in the strip of the others, with the other levels
			//of theirs on the folio, each where its name goes in the strip
		QList<Element *> free_ones;
		for (Element *element : std::as_const(chosen)) {
			if (!realTerminalOf(element)->parentStrip()) free_ones << element;
		}
		QList<Element *> members = levelsOf(diagram, free_ones, chosen);
		for (Element *element : std::as_const(free_ones)) {
			if (!members.contains(element)) members << element;
		}
		to_add = groupsOf(members);
		QVector<QVector<QSharedPointer<RealTerminal>>> planned;
		const QVector<QSharedPointer<PhysicalTerminal>> physicals = strip->physicalTerminal();
		for (const QSharedPointer<PhysicalTerminal> &physical : physicals) {
			planned << physical->realTerminals();
		}
		places = insertInOrder(planned, to_add);
		if (!plannedBridgeable(planned, joined)) {
			return fail(tr("A bus bar joins neighbouring terminals of one level of the strip %1.").arg(strip->name()));
		}
	}
	else if (strip)
	{
		if (!strip->isBridgeable(joined)) {
			return fail(tr("A bus bar joins neighbouring terminals of one level of the strip %1.").arg(strip->name()));
		}
	}
	else
	{
		name = nameForNewStrip(chosen);
		if (name.isEmpty()) name = new_strip_name.trimmed();
		if (name.isEmpty()) {
			return fail(tr("Give the strip a name."));
		}
			//The terminals of the drawn strip the first one is in, else
			//those of the folio whose comment names the strip (T85), and
			//the chosen ones: the whole strip, so that its terminals are
			//put together in their levels
		QList<Element *> members;
		if (StripFrameItem *frame = StripFrameItem::frameOf(chosen.first())) {
			members = frame->orderedTerminals();
		}
		else
		{
			const QList<Element *> elements = diagram->elements();
			for (Element *element : elements)
			{
				if (realTerminalOf(element) && element->elementInformations()
						.value(QETInformation::ELMT_COMMENT).toString().trimmed() == name) {
					members << element;
				}
			}
		}
			//Else the other levels of the chosen ones
		if (members.isEmpty()) {
			members = levelsOf(diagram, chosen, chosen);
		}
		for (Element *element : std::as_const(chosen)) {
			if (!members.contains(element)) members << element;
		}
		to_add = groupsOf(members);
		if (!plannedBridgeable(to_add, joined)) {
			return fail(tr("A bus bar joins neighbouring terminals of one level of the strip %1.").arg(name));
		}
	}

		//The redundant wires
	QSet<Element *> in_bus(chosen.cbegin(), chosen.cend());
	QList<Conductor *> attached;
	for (Element *element : std::as_const(chosen)) {
		const QList<Conductor *> conductors = element->conductors();
		for (Conductor *conductor : conductors) {
			if (!attached.contains(conductor)) attached << conductor;
		}
	}
	QList<Conductor *> redundant;
	QList<Terminal *> outside_order;
	QHash<Terminal *, QList<Conductor *>> by_outside;
	for (Conductor *conductor : std::as_const(attached))
	{
		Element *a = conductor->terminal1 ? conductor->terminal1->parentElement() : nullptr;
		Element *b = conductor->terminal2 ? conductor->terminal2->parentElement() : nullptr;
		const bool a_in = in_bus.contains(a), b_in = in_bus.contains(b);
		if (a_in && b_in)
		{
			redundant << conductor;
			continue;
		}
		Terminal *outside = a_in ? conductor->terminal2 : conductor->terminal1;
		if (!by_outside.contains(outside)) outside_order << outside;
		by_outside[outside] << conductor;
	}
	for (Terminal *outside : std::as_const(outside_order))
	{
		const QList<Conductor *> list = by_outside.value(outside);
		if (list.size() < 2) continue;
		Conductor *kept = nullptr;
		for (Conductor *conductor : list) if (!kept && !conductor->branches().isEmpty()) kept = conductor;
		for (Conductor *conductor : list) if (!kept && !conductor->arm()) kept = conductor;
		if (!kept) kept = list.first();
		for (Conductor *conductor : list) if (conductor != kept) redundant << conductor;
	}

		//Of the sides that keep a wire, the one with the fewest - the
		//inside, the field wires being on the other -, and the one wire
		//left there
	QHash<int, QList<Conductor *>> on_side;
	for (Conductor *conductor : std::as_const(attached))
	{
		if (redundant.contains(conductor)) continue;
		for (Element *element : std::as_const(chosen))
		{
			Terminal *terminal = endOn(conductor, element);
			if (terminal) on_side[element->terminals().indexOf(terminal)] << conductor;
		}
	}
	int side_ = 0;
	const int on_0 = on_side.value(0).size(), on_1 = on_side.value(1).size();
	if (on_1 && (!on_0 || on_1 < on_0)) side_ = 1;
		//A bus bar given more terminals keeps its side, and its feed while
		//the wire stays
	if (existing && existing->side() >= 0) {
		side_ = existing->side();
	}
		//A bus bar more of a strip goes by the side of its others, in a lane
		//of its own when they run along each other (laneOf()): the Renan
		//prefers the bus bars of a strip together, on one side, and there
		//may be more than two (24/10/2026)
	else if (strip)
	{
		const QVector<QSharedPointer<RealTerminal>> reals = strip->realTerminals();
		for (const QSharedPointer<RealTerminal> &real : reals)
		{
			const QSharedPointer<TerminalStripBridge> other = real ? real->bridge() : QSharedPointer<TerminalStripBridge>();
			if (other && other->side() >= 0)
			{
				side_ = other->side();
				break;
			}
		}
	}
	Conductor *feed_wire = on_side.value(side_).size() == 1 ? on_side.value(side_).first() : nullptr;
	QUuid kept_feed;
	if (existing && !existing->feed().isNull())
	{
		bool gone = false;
		for (const Conductor *conductor : std::as_const(redundant)) {
			if (conductor->uuid() == existing->feed()) gone = true;
		}
		if (!gone)
		{
			kept_feed = existing->feed();
			feed_wire = nullptr;
		}
	}

		//The feed comes to the joined terminal nearest to where it comes
		//from: the bridge carries it to the others, and a feed landing on
		//the far one ran along the whole bus bar (T87, the folio 5 of the
		//request: the feed came down by the trunk to the last common). The
		//wire is drawn again, with its properties
	Conductor *new_feed = nullptr;
	if (feed_wire)
	{
		Terminal *own = nullptr;
		for (Element *element : std::as_const(chosen)) {
			if (!own) own = endOn(feed_wire, element);
		}
		Terminal *source = own == feed_wire->terminal1 ? feed_wire->terminal2 : feed_wire->terminal1;
		Terminal *nearest = own;
		qreal best = std::numeric_limits<qreal>::max();
		for (Element *element : std::as_const(chosen))
		{
			Terminal *terminal = element->terminals().value(side_);
			if (!terminal || !source) continue;
			const qreal distance = QLineF(terminal->dockConductor(), source->dockConductor()).length();
			if (distance < best - 0.01)
			{
				best = distance;
				nearest = terminal;
			}
		}
		if (nearest && own && nearest != own && source)
		{
			new_feed = new Conductor(source, nearest);
				//Its number too: the formula of the name and the count it
				//reads (%sequf_1), which the properties alone leave out
			new_feed->setSequenceNum(feed_wire->sequenceNum());
			new_feed->setProperties(feed_wire->properties());
			redundant << feed_wire;
		}
	}
	const QUuid feed_ = !kept_feed.isNull() ? kept_feed
			  : new_feed ? new_feed->uuid() : feed_wire ? feed_wire->uuid() : QUuid();

	auto *undo = new JoinBusCommand(diagram, strip, name, to_add, joined, side_, feed_, places);
	if (existing) {
		undo->setText(tr("Add terminals to the bus bar"));
	}
	if (!redundant.isEmpty())
	{
		DiagramContent content;
		content.m_conductors_to_move = redundant;
		new DeleteQGraphicsItemCommand(diagram, content, undo);
	}
	if (new_feed) {
		new AddGraphicsObjectCommand(new_feed, diagram, QPointF(), undo);
	}
	diagram->undoStack().push(undo);
	if (message) message->clear();
	return true;
}

/**
	@brief TerminalBusItem::joinable
	@return the terminals of strips among @a selection, and those of the bus
	bars among it: what Join into a bus bar joins - a bus bar selected with
	terminals is given them
*/
QList<Element *> TerminalBusItem::joinable(const QList<QGraphicsItem *> &selection)
{
	QList<Element *> list = StripFrameItem::stripTerminals(selection);
	for (QGraphicsItem *item : selection)
	{
		const auto *bus = qgraphicsitem_cast<TerminalBusItem *>(item);
		if (!bus) continue;
		const QList<Element *> joined = bus->terminals();
		for (Element *element : joined) {
			if (!list.contains(element)) list << element;
		}
	}
	return list;
}

/**
	@brief TerminalBusItem::setColor
	The bridge of @a bus given @a color, in one undo step: the colour of the
	jumper, which the strip manager shows and the bill of materials lists
	(asked by the Renan on 23/10/2026, in the menu of the bus bar)
*/
void TerminalBusItem::setColor(TerminalBusItem *bus, const QColor &color)
{
	Diagram *folio = bus ? bus->diagram() : nullptr;
	const QSharedPointer<TerminalStripBridge> bridge_ = bus ? bus->bridge() : QSharedPointer<TerminalStripBridge>();
	if (!folio || folio->isReadOnly() || !bridge_ || bridge_->color() == color) {
		return;
	}
	folio->undoStack().push(new ChangeTerminalStripColor(bridge_, color));
}

/**
	@brief TerminalBusItem::setFeed
	@a conductor feeds the bridge of @a bus, in one undo step; nullptr: no
	wire does
*/
void TerminalBusItem::setFeed(TerminalBusItem *bus, Conductor *conductor)
{
	Diagram *folio = bus ? bus->diagram() : nullptr;
	const QSharedPointer<TerminalStripBridge> bridge_ = bus ? bus->bridge() : QSharedPointer<TerminalStripBridge>();
	if (!folio || folio->isReadOnly() || !bridge_) {
		return;
	}
	const QUuid feed_ = conductor ? conductor->uuid() : QUuid();
	if (feed_ == bridge_->feed()) {
		return;
	}
	folio->undoStack().push(new BusFeedCommand(bridge_, folio, feed_));
}

/**
	@brief TerminalBusItem::remove
	The bridge of @a bus undone, in one undo step: the terminals stay in
	their strip
*/
void TerminalBusItem::remove(TerminalBusItem *bus)
{
	Diagram *folio = bus ? bus->diagram() : nullptr;
	const QSharedPointer<TerminalStripBridge> bridge_ = bus ? bus->bridge() : QSharedPointer<TerminalStripBridge>();
	if (!folio || folio->isReadOnly() || !bridge_) {
		return;
	}
	const QVector<QSharedPointer<RealTerminal>> joined = bridge_->realTerminals();
	TerminalStrip *strip = joined.isEmpty() || !joined.first() ? nullptr : joined.first()->parentStrip();
	if (!strip) {
		return;
	}
	auto *undo = new UnBridgeTerminalsCommand(strip, joined);
	undo->setText(tr("Remove the bus bar"));
	folio->undoStack().push(undo);
}
