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
#include "../qetgraphicsitem/conductor.h"
#include "../shownkinds.h"
#include "../ui/assemblychangemarks.h"
#include "../lastusedstyle.h"
#include "../qetproject.h"
#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../autoNum/numerotationcontextcommands.h"
#include "../conductorautonumerotation.h"
#include "../conductorsegment.h"
#include "../conductortextfit.h"
#include "../wirepath.h"
#include "../conductorsegmentprofile.h"
#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../dataBase/projectdatabase.h"
#include "../diagramcommands.h"
#include "../location/locationboundary.h"
#include "../qetdiagrameditor.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetinformation.h"
#include "../ui/conductorpropertiesdialog.h"
#include "../undocommand/groupedupdatecommand.h"
#include "conductortextitem.h"
#include "dynamicelementtextitem.h"
#include "element.h"
#include "qetshapeitem.h"
#include "../QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../utils/qetutils.h"
#include "../wirehops.h"

#include <QFontMetricsF>
#include <QLineF>
#include <QMultiHash>
#include <QPainterPathStroker>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTransform>
#include <QtDebug>
#include <QUndoCommand>
#include <QUndoStack>

#include <algorithm>
#include <limits>

namespace {
		//Bumped whenever a conductor changes shape, moves, or enters or
		//leaves a folio: the hops of every conductor may then change.
		//0 is never current, so a fresh cache is always rebuilt.
	quint64 s_conductor_geometry_generation = 1;
	void conductorGeometryChanged() { ++s_conductor_geometry_generation; }

		//The conductors of the last folio whose hops were computed, as
		//scene points and plain path bounds. Rebuilt only when the folio
		//or the generation changes, so a repaint (pan, zoom, selection)
		//costs no scene query: asking the scene for the conductors in a
		//rect strokes each one's shape, and on a folio of a few hundred
		//conductors that made every repaint 0.6 s slower.
	struct HopCandidate {
		const Conductor *conductor;
		QRectF rect;
		QVector<QPointF> points;
	};
	const Diagram *s_hop_candidates_diagram = nullptr;
	quint64 s_hop_candidates_generation = 0;
	QVector<HopCandidate> s_hop_candidates;

		/// The own piece of a branch dragged by hand, and back with the
		/// undo (T86): where it comes off the arm, and its shape
	class BranchShapeCommand : public QUndoCommand
	{
		public:
			BranchShapeCommand(Conductor *branch,
					   const QPointF &old_point, const QList<QPointF> &old_shape,
					   const QPointF &new_point, const QList<QPointF> &new_shape) :
				QUndoCommand(QObject::tr("Shape a branch")),
				m_branch(branch),
				m_old_point(old_point), m_old_shape(old_shape),
				m_new_point(new_point), m_new_shape(new_shape)
			{}
			void undo() override {if (m_branch) m_branch->setBranchShape(m_old_point, m_old_shape);}
			void redo() override {if (m_branch) m_branch->setBranchShape(m_new_point, m_new_shape);}

		private:
			QPointer<Conductor> m_branch;
			QPointF m_old_point;
			QList<QPointF> m_old_shape;
			QPointF m_new_point;
			QList<QPointF> m_new_shape;
	};
}

#define PR(x) qDebug() << #x " = " << x;

namespace
{
	/**
		@brief The location path one end of a conductor stands in.
		@param terminal one end of the conductor
		@return the path the component carrying that terminal declares, empty
		when there is nothing to declare

		A terminal belongs to a component and a component carries the path, so
		the answer is two hops away and either hop can be missing. Both are
		tested rather than asserted: Terminal::parentElement() returns a member
		initialised to nullptr, and a terminal on a free-standing conductor has
		no component behind it.

		An empty answer is not a failure and needs no separate signal for one:
		crossesLocationBoundary() already treats an unknown end as crossing
		nothing, so "no component" and "component with no location" arrive at
		the same, conservative, answer.
	*/
	QString terminalLocationPath(const Terminal *terminal)
	{
		if (!terminal) {
			return QString();
		}

		const Element *element = terminal->parentElement();
		if (!element) {
			return QString();
		}

		return element->elementInformations()
				.value(QETInformation::ELMT_LOCATION_PATH).toString();
	}

	/**
		@brief Draw the text of a conductor at @a size points.
		Only the font of the item changes: the size written in the file is
		ConductorProperties::text_size, and it is not touched here.
		A size of 0 or less is left alone, as QFont::setPointSize() would
		refuse it anyway.
	*/
	void drawTextAt(ConductorTextItem *text, int size)
	{
		if (size <= 0) {
			return;
		}
		QFont font = text->font();
		if (font.pointSizeF() == size) {
			return;
		}
		font.setPointSize(size);
		text->setFont(font);
	}

	/**
		@brief The length @a text takes along a run going @a run, at the
		size and rotation it has: the width of its box when it is written
		along the run (0 on a horizontal run, 270 on a vertical one), its
		height when it is written across.
	*/
	qreal extentAlong(const ConductorTextItem *text, Qt::Orientations run)
	{
		QTransform turn;
		turn.rotate(text->rotation());
		const QRectF box = turn.mapRect(text->boundingRect());
		return run == Qt::Vertical ? box.height() : box.width();
	}

	/**
		@brief Draw @a text at the largest size, from @a recorded down to
		ConductorTextFit::smallest, at which it fits a run going @a run
		of @a run_length
	*/
	void fitTextToRun(ConductorTextItem *text, int recorded, Qt::Orientations run, qreal run_length)
	{
		drawTextAt(text, ConductorTextFit::fittingSize(
				   recorded,
				   ConductorTextFit::room(run_length),
				   [text, run](int size) {
					   drawTextAt(text, size);
					   return extentAlong(text, run);
				   }));
	}

		/// How close two things may come before they touch, in scene units
	constexpr qreal touch_clearance = 0.5;

	/**
		@brief The box of the letters of @a text, in scene coordinates: the
		ink of each line of the text, from its baseline, as the square of
		the colour of a wire is placed (ConductorTextItem::colorSquareRects).
		Not the box of the text item: it carries the blank margin of the
		document, and each line the room above the capitals and below the
		parentheses - measured on a folio of the projects, a name between
		two wires 13 apart, 2 clear of the next one, touched it by that
		box. The box of the item less its margin when there are no letters
		to measure.
	*/
	QRectF inkBox(const ConductorTextItem *text)
	{
		const QTextDocument *document = text->document();
		const QFontMetricsF metrics(document->defaultFont());
		QRectF ink;
		for (QTextBlock block = document->begin() ; block.isValid() ; block = block.next())
		{
			const QTextLayout *layout = block.layout();
			if (!layout) {
				continue;
			}
			const QString words = block.text();
			for (int i = 0 ; i < layout->lineCount() ; ++i)
			{
				const QTextLine line = layout->lineAt(i);
				const QString part = words.mid(line.textStart(), line.textLength());
				if (part.trimmed().isEmpty()) {
					continue;
				}
				const QRectF letters = metrics.tightBoundingRect(part).translated(
							layout->position()
							+ QPointF(line.x(), line.y() + line.ascent()));
				ink = ink.isNull() ? letters : ink.united(letters);
			}
		}
		if (ink.isNull())
		{
			const qreal margin = document->documentMargin();
			ink = text->boundingRect().adjusted(margin, margin, -margin, -margin);
		}
		return text->mapRectToScene(ink);
	}

		/// @a path, in the coordinates of @a item, as a stroke @a width wide in the scene
	QPainterPath strokeInScene(const QGraphicsItem *item, const QPainterPath &path, qreal width)
	{
		QPainterPathStroker stroker;
		stroker.setWidth(width);
		return stroker.createStroke(item->mapToScene(path));
	}

	/**
		@brief The outline of @a shape, in its coordinates: its sides, and
		not its inside - a name may be in a frame, it may not cross its
		side. Empty for a kind of shape that has no plain outline.
	*/
	QPainterPath outlineOf(const QetShapeItem *shape)
	{
		QPainterPath outline;
		switch (shape->shapeType())
		{
			case QetShapeItem::Line:
				outline.moveTo(shape->line().p1());
				outline.lineTo(shape->line().p2());
				break;
			case QetShapeItem::Rectangle:
				outline.addRect(shape->rect().normalized());
				break;
			case QetShapeItem::Ellipse:
				outline.addEllipse(shape->rect().normalized());
				break;
			case QetShapeItem::Polygon:
				outline.addPolygon(shape->polygon());
				if (shape->isClosed()) {
					outline.closeSubpath();
				}
				break;
			default:
				break;
		}
		return outline;
	}

	/**
		@brief Whether the letters of @a text, the name of @a own, touch
		what a name keeps clear of on @a scene: another conductor, the
		name of another conductor, the side of a shape (T71), a component
		and a text of one (T67). Asked of the
		items in the box only, through the index of the scene, so that it
		costs nothing to the drag of a wire.
	*/
	bool nameTouches(const Conductor *own, const ConductorTextItem *text, const QGraphicsScene *scene)
	{
		const QRectF around = inkBox(text).adjusted(-touch_clearance, -touch_clearance,
							    touch_clearance, touch_clearance);
		const QList<QGraphicsItem *> near = scene->items(around, Qt::IntersectsItemBoundingRect);
		for (QGraphicsItem *item : near)
		{
			if (item == own || item == text || !item->isVisible()) {
				continue;
			}
			if (const auto *other = qgraphicsitem_cast<const Conductor *>(item))
			{
				if (strokeInScene(other, other->path(), 1).intersects(around)) {
					return true;
				}
			}
			else if (const auto *name = qgraphicsitem_cast<const ConductorTextItem *>(item))
			{
				if (!name->toPlainText().isEmpty() && inkBox(name).intersects(around)) {
					return true;
				}
			}
			else if (const auto *shape = qgraphicsitem_cast<const QetShapeItem *>(item))
			{
				const QPainterPath outline = outlineOf(shape);
				if (!outline.isEmpty()
				    && strokeInScene(shape, outline, qMax<qreal>(1, shape->pen().widthF()))
						.intersects(around)) {
					return true;
				}
			}
				//Nor a component, nor a text of one (T67, seventh
				//adjustment): what the symbol draws, and not the larger box
				//its definition declares, which leaves blank room a name may
				//take
			else if (const auto *element = qgraphicsitem_cast<const Element *>(item))
			{
				if (element->mapRectToScene(element->drawnRect()).intersects(around)) {
					return true;
				}
			}
			else if (const auto *label = qgraphicsitem_cast<const DynamicElementTextItem *>(item))
			{
				const qreal margin = label->document()->documentMargin();
				if (!label->toPlainText().trimmed().isEmpty()
				    && label->sceneBoundingRect().adjusted(margin, margin, -margin, -margin)
						.intersects(around)) {
					return true;
				}
			}
		}
		return false;
	}
}

namespace
{
	/// A place the name of a conductor may be written along, as
	/// Conductor::TextRun has it
	struct NamePlace
	{
		QPointF middle;
		Qt::Orientations along;
		qreal length;
	};

	/**
		@brief Whether the piece from @a a to @a b and the one from @a p to
		@a q, in scene coordinates, lie along one line for some length: both
		horizontal at one height, or both vertical at one x, overlapping -
		the line two conductors of a point share (T67, tenth adjustment)
	*/
	bool alongOneLine(const QPointF &a, const QPointF &b, const QPointF &p, const QPointF &q)
	{
		const auto overlap = [](qreal a1, qreal a2, qreal b1, qreal b2) {
			return std::min(std::max(a1, a2), std::max(b1, b2)) - std::max(std::min(a1, a2), std::min(b1, b2));
		};
		const bool first_across = std::abs(a.y() - b.y()) < 0.01 && std::abs(a.x() - b.x()) > 0.5;
		const bool second_across = std::abs(p.y() - q.y()) < 0.01 && std::abs(p.x() - q.x()) > 0.5;
		if (first_across && second_across) {
			return std::abs(a.y() - p.y()) < 0.5 && overlap(a.x(), b.x(), p.x(), q.x()) > 0.5;
		}
		const bool first_up = std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) > 0.5;
		const bool second_up = std::abs(p.x() - q.x()) < 0.01 && std::abs(p.y() - q.y()) > 0.5;
		if (first_up && second_up) {
			return std::abs(a.x() - p.x()) < 0.5 && overlap(a.y(), b.y(), p.y(), q.y()) > 0.5;
		}
		return false;
	}

	/**
		@brief The length of @a place, a piece of @a own, that every
		conductor of @a group runs along: the line the whole point shares,
		as the line from its common terminal to the first arm, and not the
		part of a trunk only the farther arms go down (T67, tenth
		adjustment). 0 when one of them does not run along it.
	*/
	qreal sharedByAll(const Conductor *own, const NamePlace &place, const QList<Conductor *> &group)
	{
		const bool horizontal = place.along == Qt::Horizontal;
		const QPointF half = horizontal ? QPointF(place.length / 2, 0) : QPointF(0, place.length / 2);
		const QPointF a = own->mapToScene(place.middle - half);
		const QPointF b = own->mapToScene(place.middle + half);
		qreal low = horizontal ? std::min(a.x(), b.x()) : std::min(a.y(), b.y());
		qreal high = horizontal ? std::max(a.x(), b.x()) : std::max(a.y(), b.y());
		const qreal at = horizontal ? a.y() : a.x();
		for (const Conductor *c : group)
		{
			qreal from = high;
			qreal to = low;
			const QList<ConductorSegment *> pieces = c->segmentsList();
			for (const ConductorSegment *piece : pieces)
			{
				const QPointF p = c->mapToScene(piece->firstPoint());
				const QPointF q = c->mapToScene(piece->secondPoint());
				const qreal p_at = horizontal ? p.y() : p.x();
				const qreal q_at = horizontal ? q.y() : q.x();
				if (std::abs(p_at - q_at) > 0.01 || std::abs(p_at - at) > 0.5) {
					continue;
				}
				const qreal p_along = horizontal ? p.x() : p.y();
				const qreal q_along = horizontal ? q.x() : q.y();
				const qreal lo = std::max(low, std::min(p_along, q_along));
				const qreal hi = std::min(high, std::max(p_along, q_along));
				if (hi - lo > 0.5)
				{
					from = std::min(from, lo);
					to = std::max(to, hi);
				}
			}
			if (to - from <= 0.5) {
				return 0;
			}
			low = std::max(low, from);
			high = std::min(high, to);
			if (high - low <= 0.5) {
				return 0;
			}
		}
		return high - low;
	}

	/**
		@brief The places of @a runs, in the coordinates of @a own, cut
		where a conductor of another potential crosses or meets them, and
		without those a conductor of @a same - the potential of @a own -
		runs along for some length: the line several conductors of one
		point share is no one's to carry a name, and the name of each goes
		on its own arm (T67, fifth adjustment). The longest first. Empty
		when nothing cuts nor shares any of them: @a runs are then the
		places, as they always were.
		With @a shared_ones, the other way round: only the runs a
		conductor of @a same runs along, cut where a conductor of another
		potential crosses them - where the one name of a point goes when
		no arm holds it (T67, tenth adjustment).
	*/
	QVector<NamePlace> freePlaces(const Conductor *own, const QVector<NamePlace> &runs,
					      const QSet<Conductor *> &same, bool shared_ones = false,
					      bool *any_shared = nullptr)
	{
		if (any_shared) {
			*any_shared = false;
		}
		const QGraphicsScene *on = own->scene();
		if (!on) {
			return {};
		}
		QVector<NamePlace> free;
		bool changed = false;
		for (const NamePlace &run : runs)
		{
			const bool horizontal = run.along == Qt::Horizontal;
			const qreal at = horizontal ? run.middle.y() : run.middle.x();
			const qreal middle = horizontal ? run.middle.x() : run.middle.y();
			const qreal low = middle - run.length / 2;
			const qreal high = middle + run.length / 2;
			const QRectF box = own->mapRectToScene(horizontal ? QRectF(low, at, run.length, 0)
								     : QRectF(at, low, 0, run.length))
					.adjusted(-1, -1, 1, 1);
			bool shared = false;
			QVector<qreal> cuts;
			const QList<QGraphicsItem *> near = on->items(box, Qt::IntersectsItemBoundingRect);
			for (QGraphicsItem *item : near)
			{
				auto *other = qgraphicsitem_cast<Conductor *>(item);
				if (!other || other == own || !other->isVisible()) {
					continue;
				}
				const bool of_point = same.contains(other);
				const QList<ConductorSegment *> pieces = other->segmentsList();
				for (const ConductorSegment *piece : pieces)
				{
					const QPointF p = own->mapFromScene(other->mapToScene(piece->firstPoint()));
					const QPointF q = own->mapFromScene(other->mapToScene(piece->secondPoint()));
					const qreal p_at = horizontal ? p.y() : p.x();
					const qreal q_at = horizontal ? q.y() : q.x();
					const qreal p_along = horizontal ? p.x() : p.y();
					const qreal q_along = horizontal ? q.x() : q.y();
					if (std::abs(p_at - q_at) < 0.5)
					{
							//Along the run: the line of a point
						if (of_point && std::abs(p_at - at) < 0.5
						    && std::min(high, std::max(p_along, q_along))
						       - std::max(low, std::min(p_along, q_along)) > 0.5) {
							shared = true;
						}
					}
					else if (std::abs(p_along - q_along) < 0.5)
					{
							//Across it: the run is cut where the other meets it
						if (!of_point && p_along > low + 0.5 && p_along < high - 0.5
						    && at > std::min(p_at, q_at) - 0.5 && at < std::max(p_at, q_at) + 0.5) {
							cuts << p_along;
						}
					}
				}
					//Every cut is wanted of a shared run
				if (shared && !shared_ones) {
					break;
				}
			}
			if (shared && any_shared) {
				*any_shared = true;
			}
			if (shared != shared_ones) {
				changed = true;
				continue;
			}
			if (cuts.isEmpty()) {
				free << run;
				continue;
			}
			changed = true;
			std::sort(cuts.begin(), cuts.end());
			cuts << high;
			qreal from = low;
			for (qreal cut : std::as_const(cuts))
			{
				if (cut - from >= 1)
				{
					const qreal centre = (from + cut) / 2;
					free << NamePlace{horizontal ? QPointF(centre, at) : QPointF(at, centre),
							run.along, cut - from};
				}
				from = std::max(from, cut);
			}
		}
		if (!changed && !shared_ones) {
			return {};
		}
		std::stable_sort(free.begin(), free.end(),
				 [](const NamePlace &a, const NamePlace &b) {return a.length > b.length;});
		return free;
	}
}

bool Conductor::pen_and_brush_initialized = false;
int Conductor::s_live_moves = 0;
QList<QPointer<Conductor>> Conductor::s_live_touched;

void Conductor::beginLiveMove()
{
	++s_live_moves;
}

/**
	@brief Conductor::endLiveMove
	The end of a live move: the names of the conductors placed during it are
	placed once more, and their points decide which shows the name. Each
	point once, by the first of its conductors placed again.
*/
void Conductor::endLiveMove()
{
	if (s_live_moves > 0) --s_live_moves;
	if (s_live_moves > 0) {
		return;
	}
	const QList<QPointer<Conductor>> touched = s_live_touched;
	s_live_touched.clear();
	QSet<const Conductor *> done;
	for (const QPointer<Conductor> &c : touched)
	{
		if (!c || done.contains(c)) {
			continue;
		}
		c->calculateTextItemPosition();
		done << c;
		for (const QPointer<Conductor> &o : std::as_const(c->m_point_group)) {
			if (o) done << o;
		}
	}
}
QPen Conductor::conductor_pen = QPen();
QBrush Conductor::conductor_brush = QBrush();


class ConductorXmlRetroCompatibility
{
		friend class Conductor;

		static void loadSequential(const QDomElement &dom_element, const QString& seq, QStringList* list)
		{
			int i = 0;
			while (!dom_element.attribute(seq + QString::number(i+1)).isEmpty())
			{
					list->append(dom_element.attribute(seq + QString::number(i+1)));
					i++;
			}
		}

		static void loadSequential(const QDomElement &dom_element, Conductor *conductor)
		{
			autonum::sequentialNumbers sn;

			loadSequential(dom_element,"sequ_",&sn.unit);
			loadSequential(dom_element,"sequf_",&sn.unit_folio);
			loadSequential(dom_element,"seqt_",&sn.ten);
			loadSequential(dom_element,"seqtf_",&sn.ten_folio);
			loadSequential(dom_element,"seqh_",&sn.hundred);
			loadSequential(dom_element,"seqhf_",&sn.hundred_folio);

			conductor->rSequenceNum() = sn;
		}
};

/**
	@brief Conductor::Conductor
	Default constructor.
	@param p1 : first terminal of this conductor.
	@param p2 : second terminal of this conductor.
*/
Conductor::Conductor(Terminal *p1, Terminal* p2) :
	terminal1(p1),
	terminal2(p2),
	m_mouse_over(false),
	m_text_item(nullptr),
	segments(nullptr),
	m_moving_segment(false),
	modified_path(false),
	has_to_save_profile(false),
	must_highlight_(Conductor::None)
{
		//set Zvalue at 11 to be upper than the DiagramImageItem and element
	setZValue(11);
	m_previous_z_value = zValue();
	m_uuid = QUuid::createUuid();
	static quint64 created = 0;
	m_creation_rank = ++created;

		//Add this conductor to the list of conductors of each of the two terminals
	bool ajout_p1 = terminal1 -> addConductor(this);
	bool ajout_p2 = terminal2 -> addConductor(this);
		//m_valid become false if the conductor can't be added to terminal (conductor already exist)
	m_valid = (!ajout_p1 || !ajout_p2) ? false : true;

		//Listen to the two components this wire hangs off, because the
		//stroke is decided from their locations at paint time and nothing
		//else would tell this conductor that one of them moved.
	watchLocationOf(terminal1);
	watchLocationOf(terminal2);

		//Default attribute to paint a conductor
	if (!pen_and_brush_initialized)
	{
		conductor_pen.setJoinStyle(Qt::MiterJoin);
		conductor_pen.setCapStyle(Qt::SquareCap);
		conductor_pen.setColor(Qt::black);
		conductor_pen.setStyle(Qt::SolidLine);
		conductor_pen.setWidthF(1.0);
		conductor_brush.setColor(Qt::white);
		conductor_brush.setStyle(Qt::NoBrush);
		pen_and_brush_initialized = true;
	}

		//By default, the 4 profiles are nuls -> we must use priv_calculeConductor
	conductor_profiles.insert(Qt::TopLeftCorner,     ConductorProfile());
	conductor_profiles.insert(Qt::TopRightCorner,    ConductorProfile());
	conductor_profiles.insert(Qt::BottomLeftCorner,  ConductorProfile());
	conductor_profiles.insert(Qt::BottomRightCorner, ConductorProfile());

		//Generate the path of this conductor.
	generateConductorPath(terminal1 -> dockConductor(), terminal1 -> orientation(), terminal2 -> dockConductor(), terminal2 -> orientation());
	setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsScenePositionChanges);
	setAcceptHoverEvents(true);

		// Add the text field
	m_text_item = new ConductorTextItem(m_properties.text, this);
	connect(m_text_item, &ConductorTextItem::textEdited, this, &Conductor::displayedTextChanged);

		//Set the default conductor properties. The color, specifically, is
		//overridden by the last one applied via the F2 color editor this
		//session (#879), the same way LastUsedStyle already does for shapes.
	Diagram *dia = p1->diagram() ? p1->diagram() : p2->diagram();
	if (dia)
	{
		ConductorProperties properties = dia->defaultConductorProperties;
		if (LastUsedStyle::hasConductorColor())
			properties.color = LastUsedStyle::conductorColor();
		setProperties(properties);
	}
}

/**
	@brief Conductor::watchLocationOf
	@param terminal one of the two ends of this conductor

	Subscribe to the component that carries @a terminal, so that this
	conductor is told when the information of that component changes.

	Nothing happens for an end that carries no component: a conductor drawn
	between two free terminals has no location to follow, and
	terminalLocationPath() already answers "nothing" for it.

	Qt::UniqueConnection is what makes a wire looping back onto the same
	component safe. Both ends then name the same Element and the second
	call is refused, so the handler runs once per change instead of twice.
	It works here because the slot is a pointer to member function and not
	a lambda - a lambda would be accepted by connect() and silently
	duplicated.

	Neither side of the connection needs to be undone by hand. It dies with
	the conductor, which is a QObject, and with the component, which is
	another one; and the case that would matter - deleting a component -
	deletes the conductors hanging off its terminals anyway.
*/
void Conductor::watchLocationOf(Terminal *terminal)
{
	if (!terminal) {
		return;
	}

	Element *element = terminal->parentElement();
	if (!element) {
		return;
	}

	connect(element, &Element::elementInfoChange,
		this, &Conductor::elementInformationChanged,
		Qt::UniqueConnection);
}

/**
	@brief Conductor::replaceTerminal
	Dock at @a to the end of this conductor that is docked at @a from,
	keeping everything else: uuid, number, properties, path profile. Used
	when an element is replaced by another (SwapElementCommand).

	Only the pointer changes here. Take the conductor off its folio before
	and put it back after (Diagram::removeItem() and Diagram::addItem()):
	that is what removes it from the conductors of @a from, adds it to those
	of @a to and keeps the project database in step, and nothing should be
	done twice.
	@param from : a terminal this conductor is docked at
	@param to : the terminal to dock at instead
	@return false, and nothing changed, when @a from is not one of the two
	ends
*/
bool Conductor::replaceTerminal(Terminal *from, Terminal *to)
{
	if (!from || !to) {
		return false;
	}
	if (terminal1 == from) {
		terminal1 = to;
	} else if (terminal2 == from) {
		terminal2 = to;
	} else {
		return false;
	}

	watchLocationOf(to);
	return true;
}

/**
	@brief Conductor::dockAt
	Dock the two ends of this conductor at @a end1 and @a end2, keeping
	everything else, as replaceTerminal() does for one. Both at once, for a
	conductor whose two ends change together: changed one after the other,
	the first change can leave both ends on the same terminal, and the second
	would then find the wrong one (ExchangeElementsCommand).
	Take the conductor off its folio before and put it back after, as for
	replaceTerminal().
	@param end1 : the terminal for terminal1
	@param end2 : the terminal for terminal2
*/
void Conductor::dockAt(Terminal *end1, Terminal *end2)
{
	if (!end1 || !end2) {
		return;
	}
	terminal1 = end1;
	terminal2 = end2;
	watchLocationOf(end1);
	watchLocationOf(end2);
}

/**
	@brief Conductor::elementInformationChanged
	@param old_info what the component said before
	@param new_info what it says now

	Ask for a repaint when, and only when, one of the two ends changed
	location.

	@par Why the conductor listens instead of the component telling it

	The rule that turns two locations into a dashed stroke is read at paint
	time, in paint() below, out of terminalLocationPath() above: this file
	is the only one that knows the stroke depends on a location at all.
	Element::setElementInformations() would have to learn it, and so would
	Element::setElementData(), which writes the same information by another
	road - the terminal strip editors and the channel table of a PLC master
	go through it. Two writers, one rule: fixing the first and forgetting
	the second is the very defect being fixed here, so the subscription
	hangs off the signal both of them already emit.

	@par Why the location is compared instead of repainting on any change

	setElementInformations() runs in a loop over the whole folio when
	components are renumbered and when a catalogue part is assigned to a
	selection. Those write labels, manufacturer codes and cross references,
	none of which the stroke depends on, and a repaint per wire per
	component would be paid on every one of them. The comparison costs two
	lookups and stops there.

	@par What is deliberately not tested here

	Diagram::dashExternalWires(). paint() reads that flag first so that a
	folio with the option off never pays for a location lookup, and the
	mirror image of that test would fit here - a location that moves while
	the option is off changes no pixel. It is left out because it would tie
	this handler to the one thing that happens to read the location today,
	and because the saving is not real: a location changes when somebody
	deliberately assigns one, not in a loop, and one update() of one item
	is cheaper than the bug of forgetting to widen this test the day
	something else on the wire starts reading where its ends stand.

	No undo command is pushed and none is needed: update() schedules a
	repaint and changes nothing that is saved. The change of location
	itself is already on the stack - AssignLocationCommand,
	ChangeElementInformationCommand, EditLocationTreeCommand - and undoing
	it emits this same signal the other way round, so the wire is repainted
	on the way back too.
*/
void Conductor::elementInformationChanged(const DiagramContext &old_info,
					  const DiagramContext &new_info)
{
	if (old_info.value(QETInformation::ELMT_LOCATION_PATH)
			== new_info.value(QETInformation::ELMT_LOCATION_PATH)) {
		return;
	}

	update();
}

/**
	@brief Conductor::~Conductor
	Destructor. The conductor is removed from its terminal
*/
Conductor::~Conductor()
{
	if (m_arm) {
		m_arm->m_branches.removeAll(this);
	}
	conductorGeometryChanged();
	removeHandler();
	terminal1->removeConductor(this);
	terminal2->removeConductor(this);
	deleteSegments();
}

/**
	@brief Conductor::isValid
	@return true if conductor is valid else false;
	A non valid conductor, is a conductor without two terminal
*/
bool Conductor::isValid() const
{
	return m_valid;
}

/**
	Met a jour la representation graphique du conducteur en recalculant son
	trace. Cette fonction est typiquement appelee lorsqu'une seule des bornes du
	conducteur a change de position.
	@param rect Rectangle a mettre a jour
	@see QGraphicsPathItem::update()
*/
void Conductor::updatePath(const QRectF &rect) {
		//A branch has the path its arm gives it (T86)
	if (arm())
	{
		followArm();
		QGraphicsObject::update(rect);
		return;
	}
	QPointF p1, p2;
	p1 = terminal1 -> dockConductor();
	p2 = terminal2 -> dockConductor();
	if (segmentsCount() && !conductor_profiles[currentPathType()].isNull())
		updateConductorPath(p1, terminal1 -> orientation(), p2, terminal2 -> orientation());
	else
		generateConductorPath(p1, terminal1 -> orientation(), p2, terminal2 -> orientation());
	calculateTextItemPosition();
	QGraphicsObject::update(rect);
}

/**
	@brief Conductor::segmentsToPath
	Generate the QPainterPath from the list of points
*/
void Conductor::segmentsToPath()
{
	QPainterPath path;

	if (segments == nullptr)
		setPath(path);

		//Start the path
	path.moveTo(segments -> firstPoint());
		//Each segments
	ConductorSegment *segment = segments;
	while(segment -> hasNextSegment()) {
		path.lineTo(segment -> secondPoint());
		segment = segment -> nextSegment();
	}
		//Finish the path
	path.lineTo(segment -> secondPoint());

	setPath(path);

		//The branches of this wire follow it (T86)
	for (const QPointer<Conductor> &branch : std::as_const(m_branches))
	{
		if (branch && branch->m_arm == this && branch->arm()) {
			branch->followArm();
		}
	}

		//If conductor is selected and he's not being modified
		//we update the position of the handlers
	if (isSelected() && !m_moving_segment)
	{
		if(handlerPoints().size() == m_handler_vector.size())
			adjustHandlerPos();
		else
		{
			removeHandler();
			addHandler();
		}
	}
}

/**
	Gere les updates
	@param p1 Coordonnees du point d'amarrage de la borne 1
	@param o1 Orientation de la borne 1
	@param p2 Coordonnees du point d'amarrage de la borne 2
	@param o2 Orientation de la borne 2
*/
void Conductor::updateConductorPath(const QPointF &p1, Qet::Orientation o1, const QPointF &p2, Qet::Orientation o2) {
	ConductorProfile &conductor_profile = conductor_profiles[currentPathType()];

	Q_ASSERT_X(conductor_profile.segmentsCount(QET::Both) > 1, "Conductor::priv_modifieConductor", "pas de points a modifier");
	Q_ASSERT_X(!conductor_profile.isNull(),                 "Conductor::priv_modifieConductor", "pas de profil utilisable");

	// recupere les coordonnees fournies des bornes
	// retrieve the coordinates provided for the terminals
	QPointF new_p1 = mapFromScene(p1);
	QPointF new_p2 = mapFromScene(p2);
	QRectF new_rect = QRectF(new_p1, new_p2);

	// recupere la largeur et la hauteur du profil
	// retrieve the width and height of the profile
	qreal profile_width  = conductor_profile.width();
	qreal profile_height = conductor_profile.height();

	// calcule les differences verticales et horizontales a appliquer
	// calculates the vertical and horizontal differences to be applied
	qreal h_diff = (qAbs(new_rect.width())  - qAbs(profile_width) ) * getSign(profile_width);
	qreal v_diff = (qAbs(new_rect.height()) - qAbs(profile_height)) * getSign(profile_height);

	// A profile with no segment of non-zero length along an axis has nowhere
	// to put a difference along that axis: it would be dropped and the last
	// point would join the terminal diagonally. Every straight conductor
	// loaded from a file is like this, as a zero-length segment is saved as
	// horizontal (bugtracker #105). Generate a new path instead, as when
	// there is no profile for this path type.
	const auto can_absorb = [](qreal diff, const QList<ConductorSegmentProfile *> &segments_list) {
		if (qAbs(diff) <= 0.01) return(true);
		for (ConductorSegmentProfile *csp : segments_list) {
			if (csp -> length) return(true);
		}
		return(false);
	};
	if (!can_absorb(h_diff, conductor_profile.horizontalSegments()) ||
		!can_absorb(v_diff, conductor_profile.verticalSegments())) {
		generateConductorPath(p1, o1, p2, o2);
		return;
	}

	// applique les differences aux segments
	// apply the differences to the segments
	QMultiHash<ConductorSegmentProfile *, qreal> segments_lengths;
	segments_lengths.unite(shareOffsetBetweenSegments(h_diff, conductor_profile.horizontalSegments()));
	segments_lengths.unite(shareOffsetBetweenSegments(v_diff, conductor_profile.verticalSegments()));

	// en deduit egalement les coefficients d'inversion (-1 pour une inversion, +1 pour conserver le meme sens)
	// also deduce the inversion coefficients (-1 for inversion, +1 to keep the same direction)
	int horiz_coeff = getCoeff(new_rect.width(),  profile_width);
	int verti_coeff = getCoeff(new_rect.height(), profile_height);

	// genere les nouveaux points
	// generate the new points
	QList<QPointF> points;
	points << new_p1;
	int limit = conductor_profile.segments.count() - 1;
	for (int i = 0 ; i < limit ; ++ i) {
		// dernier point
		// last point
		QPointF previous_point = points.last();

		// profil de segment de conducteur en cours
		// current conductor segment profile
		ConductorSegmentProfile *csp = conductor_profile.segments.at(i);

		// coefficient et offset a utiliser pour ce point
		// coefficient and offset to be used for this point
		qreal coeff = csp -> isHorizontal ? horiz_coeff : verti_coeff;
		qreal offset_applied = segments_lengths.value(csp);

		// applique l'offset et le coeff au point
		// apply coefficient and offset to point
		if (csp -> isHorizontal) {
			points << QPointF (
				previous_point.x() + (coeff * offset_applied),
				previous_point.y()
			);
		} else {
			points << QPointF (
				previous_point.x(),
				previous_point.y() + (coeff * offset_applied)
			);
		}
	}
	points << new_p2;
	pointsToSegments(points);
	segmentsToPath();
}

/**
	@param offset Longueur a repartir entre les segments
	@param segments_list Segments sur lesquels il faut repartir la longueur
	@param precision seuil en-deca duquel on considere qu'il ne reste rien a repartir
*/
QHash<ConductorSegmentProfile *, qreal> Conductor::shareOffsetBetweenSegments(
	const qreal &offset,
	const QList<ConductorSegmentProfile *> &segments_list,
	const qreal &precision
) const
{
	// construit le QHash qui sera retourne
	QHash<ConductorSegmentProfile *, qreal> segments_hash;
	foreach(ConductorSegmentProfile *csp, segments_list) {
		segments_hash.insert(csp, csp -> length);
	}

	// memorise le signe de la longueur de chaque segement
	QHash<ConductorSegmentProfile *, int> segments_signs;
	foreach(ConductorSegmentProfile *csp, segments_hash.keys()) {
		segments_signs.insert(csp, getSign(csp -> length));
	}

	//qDebug() << "repartition d'un offset de" << offset << "px sur" << segments_list.count() << "segments";

	// repartit l'offset sur les segments
	qreal remaining_offset = offset;
	while (remaining_offset > precision || remaining_offset < -precision) {
		// recupere le nombre de segments differents ayant une longueur non nulle
		uint segments_count = 0;
		foreach(ConductorSegmentProfile *csp, segments_hash.keys()) if (segments_hash[csp]) ++ segments_count;
		//qDebug() << "  remaining_offset =" << remaining_offset;
		qreal local_offset = remaining_offset / segments_count;
		//qDebug() << "  repartition d'un offset local de" << local_offset << "px sur" << segments_count << "segments";
		remaining_offset = 0.0;
		foreach(ConductorSegmentProfile *csp, segments_hash.keys()) {
			// ignore les segments de longueur nulle
			if (!segments_hash[csp]) continue;
			// applique l'offset au segment
			//qreal segment_old_length = segments_hash[csp];
			segments_hash[csp] += local_offset;

			// (la longueur du segment change de signe) <=> (le segment n'a pu absorbe tout l'offset)
			if (segments_signs[csp] != getSign(segments_hash[csp])) {

				// on remet le trop-plein dans la reserve d'offset
				remaining_offset += qAbs(segments_hash[csp]) * getSign(local_offset);
				//qDebug() << "    trop-plein de" << qAbs(segments_hash[csp]) * getSign(local_offset) << "remaining_offset =" << remaining_offset;
				segments_hash[csp] = 0.0;
			} else {
				//qDebug() << "    offset local de" << local_offset << "accepte";
			}
		}
	}

	return(segments_hash);
}

/**
	Calcule un trajet "par defaut" pour le conducteur
	@param p1 Coordonnees du point d'amarrage de la borne 1
	@param o1 Orientation de la borne 1
	@param p2 Coordonnees du point d'amarrage de la borne 2
	@param o2 Orientation de la borne 2
*/
void Conductor::generateConductorPath(const QPointF &p1, Qet::Orientation o1, const QPointF &p2, Qet::Orientation o2) {
	pointsToSegments(automaticPath(mapFromScene(p1), o1, mapFromScene(p2), o2));
	segmentsToPath();
}

/**
	@brief Conductor::automaticPathPoints
	@return the path generateConductorPath() gives this conductor, in scene
	coordinates, whatever path it has now: what WireArrangement starts from
*/
QList<QPointF> Conductor::automaticPathPoints() const
{
	QList<QPointF> points;
	if (!terminal1 || !terminal2) return points;
	const QList<QPointF> local = automaticPath(
			mapFromScene(terminal1->dockConductor()), terminal1->orientation(),
			mapFromScene(terminal2->dockConductor()), terminal2->orientation());
	for (const QPointF &p : local) points << mapToScene(p);
	return points;
}

/**
	@brief Conductor::hasSavedPath
	@return true if this conductor has a path of its own for the way its
	terminals face each other now - dragged, loaded from the file, or
	arranged - instead of the one generateConductorPath() draws
*/
bool Conductor::hasSavedPath() const
{
	return !conductor_profiles.value(currentPathType()).isNull();
}

/**
	@brief Conductor::automaticPath
	The "default" path between two docking points, in their own
	coordinates: generateConductorPath() without the conductor.
	@param sp1 Coordonnees du point d'amarrage de la borne 1
	@param o1 Orientation de la borne 1
	@param sp2 Coordonnees du point d'amarrage de la borne 2
	@param o2 Orientation de la borne 2
*/
QList<QPointF> Conductor::automaticPath(const QPointF &sp1, Qet::Orientation o1, const QPointF &sp2, Qet::Orientation o2) {
	QPointF depart, newp1, newp2, arrivee, depart0, arrivee0;
	Qet::Orientation ori_depart, ori_arrivee;

	// s'assure qu'il n'y a ni points
	QList<QPointF> points;

	// prolonge les bornes
	newp1 = extendTerminal(sp1, o1);
	newp2 = extendTerminal(sp2, o2);

	// distingue le depart de l'arrivee : le trajet se fait toujours de gauche a droite (apres prolongation)
	if (newp1.x() <= newp2.x()) {
		depart      = newp1;
		arrivee     = newp2;
		depart0     = sp1;
		arrivee0    = sp2;
		ori_depart  = o1;
		ori_arrivee = o2;
	} else {
		depart      = newp2;
		arrivee     = newp1;
		depart0     = sp2;
		arrivee0    = sp1;
		ori_depart  = o2;
		ori_arrivee = o1;
	}

	// debut du trajet
	points << depart0;

	// prolongement de la borne de depart
	// extend start terminal
	points << depart;

	// commence le vrai trajet
	// starts the real path
	//
	// Each cas "3"/"4" below inserts a two-point bridge along one axis, at a
	// coordinate on the OTHER axis computed as a midpoint and then snapped
	// to the grid by a loop that walks it strictly downward until it
	// divides evenly. Whenever depart and arrivee already agree on the axis
	// the bridge would run along, that bridge is unneeded -- the midpoint
	// starts on the correct value already, but the snap can still walk it
	// off, or, if it happens to already be on-grid, the two bridge points
	// just duplicate depart and arrivee outright. Either way the result is
	// a short out-and-back excursion, or a longer looping detour, at a join
	// that needed no bridge in the first place (bugtracker #734). Skip the
	// bridge in exactly that case; depart and arrivee already form the
	// direct run. Which axis each cas guards on is noted at each site below
	// -- descendant and montant are not mirror images of each other here.
	if (depart.y() < arrivee.y()) {
		// trajet descendant
		if ((ori_depart == Qet::North && (ori_arrivee == Qet::South || ori_arrivee == Qet::West)) || (ori_depart == Qet::East && ori_arrivee == Qet::West)) {
			// cas "3": bridge is vertical, at a shared x between depart.y()
			// and arrivee.y() -- unnecessary exactly when depart and
			// arrivee already share an x. Compared qRound()ed, the same
			// rounding the bridge coordinate itself is computed with below:
			// an exact != would miss a pair that's already grid-equal after
			// rounding but off by a sub-pixel remainder, and still route a
			// degenerate bridge for it.
			if (qRound(depart.x()) != qRound(arrivee.x())) {
				const int middle_x = qRound(depart.x() + arrivee.x()) / 2;
				const qreal ligne_inter_x = std::floor(middle_x / Diagram::xGrid + 1e-9) * Diagram::xGrid;
				points << QPointF(ligne_inter_x, depart.y());
				points << QPointF(ligne_inter_x, arrivee.y());
			}
		} else if ((ori_depart == Qet::South && (ori_arrivee == Qet::North || ori_arrivee == Qet::East)) || (ori_depart == Qet::West && ori_arrivee == Qet::East)) {
			// cas "4": bridge is horizontal, at a shared y between
			// depart.x() and arrivee.x() -- unnecessary exactly when depart
			// and arrivee already share a y. (Always true in this branch,
			// since "descendant" requires depart.y() < arrivee.y() strictly
			// -- kept for symmetry with the "montant" branch below.)
			if (qRound(depart.y()) != qRound(arrivee.y())) {
				const int middle_y = qRound(depart.y() + arrivee.y()) / 2;
				const qreal ligne_inter_y = std::floor(middle_y / Diagram::yGrid + 1e-9) * Diagram::yGrid;
				points << QPointF(depart.x(), ligne_inter_y);
				points << QPointF(arrivee.x(), ligne_inter_y);
			}
		} else if ((ori_depart == Qet::North || ori_depart == Qet::East) && (ori_arrivee == Qet::North || ori_arrivee == Qet::East)) {
			points << QPointF(arrivee.x(), depart.y()); // cas "2"
		} else {
			points << QPointF(depart.x(), arrivee.y()); // cas "1"
		}
	} else {
		// trajet montant
		if ((ori_depart == Qet::West && (ori_arrivee == Qet::East || ori_arrivee == Qet::South)) || (ori_depart == Qet::North && ori_arrivee == Qet::South)) {
			// cas "3": bridge is horizontal, at a shared y between
			// depart.x() and arrivee.x() -- unnecessary exactly when depart
			// and arrivee already share a y (the West->East case
			// diagnosed for #734: two stubs extended onto the same y run
			// straight into each other, no bridge needed). Compared
			// qRound()ed, same as the other three guards.
			if (qRound(depart.y()) != qRound(arrivee.y())) {
				const int middle_y = qRound(depart.y() + arrivee.y()) / 2;
				const qreal ligne_inter_y = std::floor(middle_y / Diagram::yGrid + 1e-9) * Diagram::yGrid;
				points << QPointF(depart.x(), ligne_inter_y);
				points << QPointF(arrivee.x(), ligne_inter_y);
			}
		} else if ((ori_depart == Qet::East && (ori_arrivee == Qet::West || ori_arrivee == Qet::North)) || (ori_depart == Qet::South && ori_arrivee == Qet::North)) {
			// cas "4": bridge is vertical, at a shared x between depart.y()
			// and arrivee.y() -- unnecessary exactly when depart and
			// arrivee already share an x.
			if (qRound(depart.x()) != qRound(arrivee.x())) {
				const int middle_x = qRound(depart.x() + arrivee.x()) / 2;
				const qreal ligne_inter_x = std::floor(middle_x / Diagram::xGrid + 1e-9) * Diagram::xGrid;
				points << QPointF(ligne_inter_x, depart.y());
				points << QPointF(ligne_inter_x, arrivee.y());
			}
		} else if ((ori_depart == Qet::West || ori_depart == Qet::North) && (ori_arrivee == Qet::West || ori_arrivee == Qet::North)) {
			points << QPointF(depart.x(), arrivee.y()); // cas "2"
		} else {
			points << QPointF(arrivee.x(), depart.y()); // cas "1"
		}
	}

	// fin du vrai trajet
	points << arrivee;

	// prolongement de la borne d'arrivee
	points << arrivee0;

	// inverse eventuellement l'ordre des points afin que le trajet soit exprime de la borne 1 vers la borne 2
	if (newp1.x() > newp2.x()) {
		QList<QPointF> points2;
		for (int i = points.size() - 1 ; i >= 0 ; -- i) points2 << points.at(i);
		points = points2;
	}

	return(points);
}

/**
 * @brief Conductor::extendTerminal
 * @param terminal : point to extend (must be the docking point of a terminal)
 * @param terminal_orientation : the orientation of the terminal
 * @param ext_size : how many to extrend (10 by default)
 * @return the point with an extension of @ext_size
 * and rounded to nearest multiple of ten
 * in order to be snapped to the grid of the diagram.
 */
QPointF Conductor::extendTerminal(const QPointF &terminal, Qet::Orientation terminal_orientation, qreal ext_size)
{
	QPointF extended_terminal;
	switch(terminal_orientation) {
		case Qet::North:
			extended_terminal = QPointF(terminal.x(),
										std::round((terminal.y() - ext_size)/10)*10);
			break;
		case Qet::East:
			extended_terminal = QPointF(std::round((terminal.x() + ext_size)/10)*10,
										terminal.y());
			break;
		case Qet::South:
			extended_terminal = QPointF(terminal.x(),
										std::round((terminal.y() + ext_size)/10)*10);
			break;
		case Qet::West:
			extended_terminal = QPointF(std::round((terminal.x() - ext_size)/10)*10,
										terminal.y());
			break;
		default: extended_terminal = terminal;
	}

	return(extended_terminal);
}

/**
	Dessine le conducteur sans antialiasing.
	@param painter Le QPainter a utiliser pour dessiner le conducteur
	@param options Les options de style pour le conducteur
	@param qw Le QWidget sur lequel on dessine

	@par The dashed external wire is an overlay, and never a stored property

	Decision G of the specification. A wire whose two ends sit in locations
	that neither share nor contain one another is painted dashed, and that is
	decided here, from the two ends and the folio flag, with nothing written
	back to m_properties. So the wire keeps the style its owner chose,
	un-ticking the folio option puts every wire back the way it was, and
	moving a component to another location changes the drawing with no
	conductor to edit.

	Nobody should be tempted to store the answer on the conductor later: it
	would be a second copy of a fact that already lives in the two
	components, and the two would drift the first time somebody reassigned a
	location.

	The cost is paid on every repaint of every conductor, which is why the
	folio flag is read before anything else and the two location paths are
	only fetched once it says yes. On a folio with the option off, the whole
	of it is one bool.

	@par The bicolor branch is a second stroke, not a contradiction

	Further down, the bicolor branch forces Qt::CustomDashLine for its own
	pass. That pass is painted after this one, over the same path and with
	its own pattern, so it neither reads nor overrules the style set here - a
	bicolor external wire comes out dashed in both colours. Whoever reads
	that branch should not go looking for the bug: there are two strokes, and
	each one sets its own style.
*/
void Conductor::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *qw)
{
		//What changed since the panel was wired, under the wire, on the
		//screen only: a folio printed or exported is painted with no
		//widget (T29)
	if (qw && AssemblyChangeMarks::isMarked(this)) {
		AssemblyChangeMarks::drawAlong(painter, path());
	}
	painter -> save();
	painter -> setRenderHint(QPainter::Antialiasing, false);

		// Set the color of conductor
	QColor final_conductor_color(m_properties.color);
	if (must_highlight_ == Normal) {
		final_conductor_color = QColor::fromRgb(69, 137, 255, 255);
	} else if (must_highlight_ == Alert) {
		final_conductor_color =QColor::fromRgb(255, 69, 0, 255);
	} else if (isSelected()) {
		final_conductor_color = Qt::red;
	} else {
		if (Diagram *parent_diagram = diagram()) {
			if (!parent_diagram -> drawColoredConductors()) {
				final_conductor_color = Qt::black;
			}
		}
	}

		//Draw the conductor bigger when is hovered
	conductor_pen.setWidthF(m_mouse_over? (m_properties.cond_size) +4 : (m_properties.cond_size));

		//Set the QPen and QBrush to the QPainter
	painter -> setBrush(conductor_brush);
	QPen final_conductor_pen = conductor_pen;

		//Set the conductor style
	final_conductor_pen.setColor(final_conductor_color);
	final_conductor_pen.setStyle(m_properties.style);

		//Overlay a dash on a wire that leaves its location, when the folio
		//asks for it. This is decided at paint time and stored nowhere, and
		//the bicolor branch further down keeps its own style: see the two
		//@par sections in the comment above this function before changing
		//either of them.
		//
		//The folio flag is tested first so that a folio with the option off
		//costs one bool and never a location lookup.
	if (Diagram *parent_diagram = diagram())
	{
		if (parent_diagram->dashExternalWires()
				&& crossesLocationBoundary(
					terminalLocationPath(terminal1),
					terminalLocationPath(terminal2)))
		{
			final_conductor_pen.setStyle(Qt::DashLine);
		}
	}

	final_conductor_pen.setJoinStyle(Qt::SvgMiterJoin); // better rendering with dot

		//Use a cosmetic line, below a certain zoom
	if (options && options->levelOfDetailFromTransform(painter->worldTransform()) < 0.5)
	{
		final_conductor_pen.setCosmetic(true);
	}

	painter -> setPen(final_conductor_pen);

		//Draw the conductor, with a hop at each crossing if the project asks for them
		//A branch draws its own piece: the rest is its arm's (T86)
	const QPainterPath painted_path = arm() ? ownPiecePaintedPath() : paintedPath();
	painter -> drawPath(painted_path);
		//Draw the second color
	if(m_properties.m_bicolor)
	{
		final_conductor_pen.setColor(m_properties.m_color_2);
		final_conductor_pen.setStyle(Qt::CustomDashLine);
		QVector<qreal> dash_pattern;
		dash_pattern << m_properties.m_dash_size << m_properties.m_dash_size *2;
		final_conductor_pen.setDashPattern(dash_pattern);
		painter->save();
		painter->setPen(final_conductor_pen);
		painter->drawPath(painted_path);
		painter->restore();
	}

	if (m_properties.type == ConductorProperties::Single) {
		painter -> setBrush(final_conductor_color);
		m_properties.singleLineProperties.draw(
			painter,
			middleSegment() -> isHorizontal() ? QET::Horizontal : QET::Vertical,
			QRectF(middleSegment() -> middle() - QPointF(12.0, 12.0), QSizeF(24.0, 24.0))
		);
		if (isSelected()) painter -> setBrush(Qt::NoBrush);
	}

		//Draw the junctions
	QList<QPointF> junctions_list = junctions();
	if (!junctions_list.isEmpty()) {
		final_conductor_pen.setStyle(Qt::SolidLine);
		QBrush junction_brush(final_conductor_color, Qt::SolidPattern);
		painter -> setPen(final_conductor_pen);
		painter -> setBrush(junction_brush);
		painter -> setRenderHint(QPainter::Antialiasing, true);
			// The junction dot has to read as a dot on top of the conductor
			// that carries it, so it scales with the conductor width instead
			// of being a fixed 3.0 across: on a wide conductor a 3.0 dot is
			// narrower than the line and simply disappears (bugtracker #108).
			//
			// Floored at the historic 3.0 so nothing changes for the default
			// width of 1.0 or anything thinner -- only the wide conductors
			// the report is about are affected. m_properties.cond_size is
			// used rather than the pen, whose width is inflated by 4 while
			// the mouse is over the conductor.
		const qreal junction_diameter = qMax(3.0, 3.0 * m_properties.cond_size);
		const qreal junction_radius = junction_diameter / 2.0;
		foreach(QPointF point, junctions_list) {
			painter -> drawEllipse(QRectF(point.x() - junction_radius,
						      point.y() - junction_radius,
						      junction_diameter,
						      junction_diameter));
		}
	}

	painter -> restore();
}

/// @return le Diagram auquel ce conducteur appartient, ou 0 si ce conducteur est independant
Diagram *Conductor::diagram() const
{
	return(qobject_cast<Diagram *>(scene()));
}

/**4
	@return le champ de texte associe a ce conducteur
*/
ConductorTextItem *Conductor::textItem() const
{
	return(m_text_item);
}

/**
	Methode de validation d'element XML
	@param e Un element XML sense represente un Conducteur
	@return true si l'element XML represente bien un Conducteur ; false sinon
*/
bool Conductor::valideXml(QDomElement &e){
	// verifie le nom du tag
	if (e.tagName() != "conductor") return(false);

	// verifie la presence des attributs minimaux
	if (!e.hasAttribute("terminal1")) return(false);
	if (!e.hasAttribute("terminal2")) return(false);

	bool conv_ok;
	// parse l'abscisse
	if (e.hasAttribute("element1")) {
		if (QUuid(e.attribute("element1")).isNull())
			return false;
		if (QUuid(e.attribute("terminal1")).isNull())
			return false;
	} else {
		e.attribute("terminal1").toInt(&conv_ok);
		if (!conv_ok) return(false);
	}

	// parse l'ordonnee
	if (e.hasAttribute("element2")) {
		if (QUuid(e.attribute("element2")).isNull())
			return false;
		if (QUuid(e.attribute("terminal2")).isNull())
			return false;
	} else {
		e.attribute("terminal2").toInt(&conv_ok);
		if (!conv_ok) return(false);
	}
	return(true);
}

/**
	@brief Conductor::mouseDoubleClickEvent
	Manage the mouse double click
	@param event
*/
void Conductor::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
	event->accept();
	editProperty();
}

/**
	@brief Conductor::mousePressEvent
	Manage the mouse press event
	@param event
*/
void Conductor::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	QGraphicsObject::mousePressEvent(event);

	if (event->modifiers() & Qt::ControlModifier)
		setSelected(!isSelected());
}

/**
	@brief Conductor::mouseReleaseEvent
	@param event
*/
void Conductor::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (!(event -> modifiers() & Qt::ControlModifier))
		QGraphicsObject::mouseReleaseEvent(event);
}

/**
	@brief Conductor::hoverEnterEvent
	Manage the hover enter event
	@param event
*/
void Conductor::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
	Q_UNUSED(event);
	m_mouse_over = true;
	update();
}

/**
	@brief Conductor::hoverLeaveEvent
	Manage the mouse leave event
	@param event
*/
void Conductor::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
	Q_UNUSED(event);
	update();
	m_mouse_over = false;
}

/**
	@brief Conductor::itemChange
	@param change
	@param value
	@return
*/
QVariant Conductor::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == QGraphicsItem::ItemSelectedChange)
	{
		if (value.toBool())
		{
			m_previous_z_value = zValue();
			setZValue(qAbs(m_previous_z_value) + 10000);
			addHandler();
		}
		else
		{
			setZValue(m_previous_z_value);
			removeHandler();
		}
	}
	else if (change == QGraphicsItem::ItemSceneHasChanged)
	{
		conductorGeometryChanged();
			//An arm deleted leaves its branches the paths they have, as wires
			//of their own; back with an undo, they follow it again. And a
			//branch back follows its arm (T86)
		for (const QPointer<Conductor> &branch : std::as_const(m_branches))
		{
			if (!branch || branch->m_arm != this) continue;
			if (branch->arm()) {
				branch->followArm();
			} else if (branch->segments) {
				branch->saveProfile(false);
				branch->prepareGeometryChange();
				branch->update();
			}
		}
		if (arm()) {
			followArm();
		}
		calculateTextItemPosition();

		if(!scene())
			removeHandler();
		else if (scene() && isSelected())
			addHandler();
	}
	else if (change == QGraphicsItem::ItemVisibleHasChanged) {
		calculateTextItemPosition();
	}
	else if (change == QGraphicsItem::ItemPositionHasChanged && isSelected()) {
		adjustHandlerPos();
	}
	else if (change == QGraphicsItem::ItemScenePositionHasChanged) {
		conductorGeometryChanged();
	}

	return(QGraphicsObject::itemChange(change, value));
}

/**
	@brief Conductor::sceneEventFilter
	@param watched
	@param event
	@return
*/
bool Conductor::sceneEventFilter(QGraphicsItem *watched, QEvent *event)
{
		//Watched must be an handler
	if(watched->type() == QetGraphicsHandlerItem::Type)
	{
		QetGraphicsHandlerItem *qghi = qgraphicsitem_cast<QetGraphicsHandlerItem *>(watched);

		if(m_handler_vector.contains(qghi)) //Handler must be in m_vector_index, then we can start resize
		{
			m_vector_index = m_handler_vector.indexOf(qghi);
			if (m_vector_index != -1)
			{
				if(event->type() == QEvent::GraphicsSceneMousePress) //Click
				{
					handlerMousePressEvent(qghi, static_cast<QGraphicsSceneMouseEvent *>(event));
					return true;
				}
				else if(event->type() == QEvent::GraphicsSceneMouseMove) //Move
				{
					handlerMouseMoveEvent(qghi, static_cast<QGraphicsSceneMouseEvent *>(event));
					return true;
				}
				else if (event->type() == QEvent::GraphicsSceneMouseRelease) //Release
				{
					handlerMouseReleaseEvent(qghi, static_cast<QGraphicsSceneMouseEvent *>(event));
					return true;
				}
				else if (event->type() == QEvent::GraphicsSceneMouseDoubleClick) //Double click
				{
					editProperty();
					return true;
				}
			}
		}
	}

	return false;
}

/**
	@brief Conductor::adjustHandlerPos
	Adjust the position of the handler item
*/
void Conductor::adjustHandlerPos()
{
	if (m_handler_vector.isEmpty())
		return;

	if (m_handler_vector.size() == handlerPoints().size())
	{
		QVector <QPointF> points_vector = mapToScene(handlerPoints());
		for (int i = 0 ; i < points_vector.size() ; ++i)
			m_handler_vector.at(i)->setPos(points_vector.at(i));
	}
}

/**
	@brief Conductor::handlerMousePressEvent
	@param qghi
	@param event
*/
void Conductor::handlerMousePressEvent(QetGraphicsHandlerItem *qghi, QGraphicsSceneMouseEvent *event)
{
	Q_UNUSED(event);

		//we get the segment corresponding to the handler
	if (m_vector_index > -1)
	{
		qghi->setColor(Qt::cyan);
		m_moving_segment = true;
		m_drag_points = segmentsToPoints();
		m_drag_segment = m_vector_index + 1;
		m_drag_middle = segmentsList().at(m_drag_segment)->middle();
		before_mov_text_pos_ = m_text_item -> pos();
		if (!m_live_drag)
		{
			m_live_drag = true;
			beginLiveMove();
		}

			//The conductors of the potential that run along this segment go
			//with it (T67, tenth adjustment): dragging the line a point
			//shares drags it for all of its conductors
		m_drag_partners.clear();
		const ConductorSegment *grabbed = segmentsList().at(m_drag_segment);
		const QPointF a = mapToScene(grabbed->firstPoint());
		const QPointF b = mapToScene(grabbed->secondPoint());
		const QSet<Conductor *> same = relatedPotentialConductors(false);
		for (Conductor *other : same)
		{
			if (other == this || other->scene() != scene()) {
				continue;
			}
			const QList<ConductorSegment *> theirs = other->segmentsList();
			for (int i = 0 ; i < theirs.size() ; ++i)
			{
				if (alongOneLine(a, b, other->mapToScene(theirs.at(i)->firstPoint()),
						 other->mapToScene(theirs.at(i)->secondPoint())))
				{
					m_drag_partners << DragPartner{other, other->segmentsToPoints(), i};
					other->before_mov_text_pos_ = other->m_text_item->pos();
					break;
				}
			}
		}

		for(QetGraphicsHandlerItem *handler : m_handler_vector)
			if(handler != qghi)
				handler->hide();
	}
}

/**
	@brief Conductor::handlerMouseMoveEvent
	@param qghi
	@param event
*/
void Conductor::handlerMouseMoveEvent(QetGraphicsHandlerItem *qghi, QGraphicsSceneMouseEvent *event)
{
	if (m_moving_segment)
	{
			//Snap the mouse pos to grid
		QPointF pos_ = Diagram::snapToGrid(mapFromScene(event->scenePos()));

			//The path from where the drag started and the whole of the
			//movement, and not from where the last move left it: the
			//segment of a terminal gets shorter instead of a tail, and the
			//limit at the terminal holds through the whole drag (T68)
		QPointF middle;
		pointsToSegments(WirePath::dragged(m_drag_points, m_drag_segment, pos_ - m_drag_middle, &middle));

			//Apply the movement
		modified_path = true;
		has_to_save_profile = true;
		segmentsToPath();

			//The same movement for the conductors that share the line
			//(T67, tenth adjustment), each in its own coordinates
		const QPointF from = mapToScene(m_drag_middle);
		const QPointF to = mapToScene(pos_);
		for (const DragPartner &partner : std::as_const(m_drag_partners))
		{
			Conductor *c = partner.conductor;
			if (!c) {
				continue;
			}
			c->pointsToSegments(WirePath::dragged(partner.points, partner.segment,
							      c->mapFromScene(to) - c->mapFromScene(from)));
			c->modified_path = true;
			c->has_to_save_profile = true;
			c->segmentsToPath();
			c->calculateTextItemPosition();
		}
		calculateTextItemPosition();
		qghi->setPos(mapToScene(middle));
	}
}

/**
	@brief Conductor::handlerMouseReleaseEvent
	@param qghi
	@param event
*/
void Conductor::handlerMouseReleaseEvent(QetGraphicsHandlerItem *qghi, QGraphicsSceneMouseEvent *event)
{
	Q_UNUSED(event);
	Q_UNUSED(qghi);

	m_vector_index = -1;

	m_moving_segment = false;
	m_drag_points.clear();
	if (has_to_save_profile && arm())
	{
			//A branch keeps the shape of its own piece, and where it comes
			//off the arm, and not a profile: its path is redone from the arm
			//every time the arm moves (T86)
		has_to_save_profile = false;
		const QList<QPointF> shape = ownPiecePoints();
		if (shape.size() >= 2 && diagram())
		{
			diagram()->undoStack().push(new BranchShapeCommand(
				this, m_arm_point, m_own_points, shape.first(), shape));
		}
	}
	else if (has_to_save_profile)
	{
			//With the conductors that share the line, one undo step for
			//all of them (T67, tenth adjustment)
		QList<Conductor *> partners;
		for (const DragPartner &partner : std::as_const(m_drag_partners)) {
			if (partner.conductor && partner.conductor->has_to_save_profile) partners << partner.conductor;
		}
		Diagram *dia = diagram();
		const bool together = !partners.isEmpty() && dia;
		if (together) {
			dia->undoStack().beginMacro(tr("move the line a point shares", "undo caption"));
		}
		saveProfile();
		has_to_save_profile = false;
		for (Conductor *c : std::as_const(partners))
		{
			c->saveProfile();
			c->has_to_save_profile = false;
		}
		if (together) {
			dia->undoStack().endMacro();
		}
	}
	m_drag_partners.clear();
	if (m_live_drag)
	{
		m_live_drag = false;
		endLiveMove();
	}
		//When handler is released, the conductor can have more segment than before the handler was moved
		//then we remove all handles and new ones are added
	removeHandler();
	addHandler();
}

/**
	@brief Conductor::addHandler
	Add handlers for this item
*/
void Conductor::addHandler()
{
	if (m_handler_vector.isEmpty() && scene())
	{
		m_handler_vector = QetGraphicsHandlerItem::handlerForPoint(mapToScene(handlerPoints()), QETUtils::graphicsHandlerSize(this));

		for (int i = 0 ; i < m_handler_vector.size() ; ++i)
		{
			QetGraphicsHandlerItem *handler = m_handler_vector.at(i);
				//A branch is dragged by its own piece only: the rest of its
				//path is its arm's, and goes where the arm goes (T86). The
				//handlers of the rest are kept off the folio, so that the
				//index of a handler is still the index of its segment
			if (arm() && !isOwnSegment(i + 1)) {
				continue;
			}
			handler->setColor(Qt::blue);
			scene()->addItem(handler);
			handler->installSceneEventFilter(this);
			handler->setZValue(this->zValue()+1);
		}
	}
}

/**
	@brief Conductor::removeHandler
	Remove the handlers of this item
*/
void Conductor::removeHandler()
{
	if (!m_handler_vector.isEmpty())
	{
		qDeleteAll(m_handler_vector);
		m_handler_vector.clear();
	}
}

/**
	@brief Conductor::boundingRect
	@return
*/
QRectF Conductor::boundingRect() const
{
	QRectF br = shape().boundingRect();
	return br.adjusted(-10, -10, 10, 10);
}

/**
	@brief Conductor::shape
	@return the shape of conductor.
	The shape thickness is bigger when conductor is hovered
*/
QPainterPath Conductor::shape() const
{
	QPainterPathStroker pps;
	pps.setWidth(m_mouse_over? 5 : 1);
	pps.setJoinStyle(conductor_pen.joinStyle());

		//A branch catches the mouse on its own piece only: over the arm,
		//the arm is what is clicked (T86)
	QPainterPath shape_(pps.createStroke(arm() ? ownPiecePath() : path()));

	return shape_;
}

/**
	@brief Conductor::nearShape
	@return : An area in which it is considered a point is near this conductor.
*/
QPainterPath Conductor::nearShape() const
{
	QPainterPathStroker pps;
	pps.setWidth(1300);
	pps.setJoinStyle(conductor_pen.joinStyle());
	return pps.createStroke(path());
}

/**
	@param type Type de Segments
	@return Le nombre de segments composant le conducteur.
*/
uint Conductor::segmentsCount(QET::ConductorSegmentType type) const
{
	QList<ConductorSegment *> segments_list = segmentsList();
	if (type == QET::Both) return(segments_list.count());
	uint nb_seg = 0;
	foreach(ConductorSegment *conductor_segment, segments_list) {
		if (conductor_segment -> type() == type) ++ nb_seg;
	}
	return(nb_seg);
}

/**
	Genere une liste de points a partir des segments de ce conducteur
	@return La liste de points representant ce conducteur
*/
QList<QPointF> Conductor::segmentsToPoints() const
{
	// liste qui sera retournee
	QList<QPointF> points_list;

	// on retourne la liste tout de suite s'il n'y a pas de segments
	if (segments == nullptr) return(points_list);

	// recupere le premier point
	points_list << segments -> firstPoint();

	// parcourt les segments pour recuperer les autres points
	ConductorSegment *segment = segments;
	while(segment -> hasNextSegment()) {
		points_list << segment -> secondPoint();
		segment = segment -> nextSegment();
	}

	// recupere le dernier point
	points_list << segment -> secondPoint();

	//retourne la liste
	return(points_list);
}

/**
	Regenere les segments de ce conducteur a partir de la liste de points passee en parametre
	@param points_list Liste de points a utiliser pour generer les segments
*/
void Conductor::pointsToSegments(const QList<QPointF>& points_list) {
	// supprime les segments actuels
	deleteSegments();

	// cree les segments a partir de la liste de points
	ConductorSegment *last_segment = nullptr;
	for (int i = 0 ; i < points_list.size() - 1 ; ++ i) {
		last_segment = new ConductorSegment(points_list.at(i), points_list.at(i + 1), last_segment);
		if (!i) segments = last_segment;
	}
}

/**
	@brief Conductor::fromXml
	Load the conductor and its information from xml element
	@param dom_element
	@return true if loading succeeded else return false
*/
bool Conductor::fromXml(QDomElement &dom_element)
{
	const QUuid old_uuid = m_uuid;
		//Older project files have no conductor uuid attribute at all --
		//generate one on load, same treatment terminal uuids got when
		//that field was introduced (see terminal1/terminal2 handling in
		//toXml() below).
	m_uuid = QUuid(dom_element.attribute(QStringLiteral("uuid")));
	m_persist_uuid = !m_uuid.isNull();
	if (m_uuid.isNull()) {
			//Absent, empty or malformed: mint one. A null uuid is not a usable
			//identity -- every conductor carrying one would collide with every
			//other on the conductor table's primary key. It's runtime-only,
			//though: toXml() must not write it back out, or a legacy file
			//with no conductor uuids gets a freshly different one on every
			//single load-and-resave (see #754).
		m_uuid = QUuid::createUuid();
	}
	uuidChanged(old_uuid);

		//"nan" and "inf" parse as numbers; a non-finite position is kept at 0
	const qreal x = dom_element.attribute("x", nullptr).toDouble();
	const qreal y = dom_element.attribute("y", nullptr).toDouble();
	setPos(qIsFinite(x) ? x : 0, qIsFinite(y) ? y : 0);

	bool retval = pathFromXml(dom_element);
	m_auto_arranged = modified_path
			&& dom_element.attribute(QStringLiteral("autoarranged")) == QLatin1String("true");

	m_text_item -> fromXml(dom_element);
	ConductorProperties pr;
	pr.fromXml(dom_element);

		//Load Sequential Values
	if (dom_element.hasAttribute("sequ_1") || dom_element.hasAttribute("sequf_1") || dom_element.hasAttribute("seqt_1") || dom_element.hasAttribute("seqtf_1") || dom_element.hasAttribute("seqh_1") || dom_element.hasAttribute("seqhf_1"))
		ConductorXmlRetroCompatibility::loadSequential(dom_element, this);
	else
		m_autoNum_seq.fromXml(dom_element.firstChildElement("sequentialNumbers"));

	m_freeze_label = dom_element.attribute("freezeLabel") == "true" ? true : false;

		//The wire this one branches off: linked by linkArms() once the
		//folio has all its wires (T86)
	m_pending_arm = QUuid(dom_element.attribute(QStringLiteral("arm")));
	if (!m_pending_arm.isNull())
	{
		const qreal arm_x = dom_element.attribute(QStringLiteral("arm_x")).toDouble();
		const qreal arm_y = dom_element.attribute(QStringLiteral("arm_y")).toDouble();
		m_arm_point = QPointF(qIsFinite(arm_x) ? arm_x : 0, qIsFinite(arm_y) ? arm_y : 0);
			//Its own piece as the person left it, "x,y;x,y;...", none for
			//the automatic one
		m_own_points.clear();
		const QStringList corners = dom_element.attribute(QStringLiteral("own_shape"))
					    .split(QLatin1Char(';'), Qt::SkipEmptyParts);
		for (const QString &corner : corners)
		{
			const QStringList xy = corner.split(QLatin1Char(','));
			bool ok_x = false, ok_y = false;
			const qreal x = xy.value(0).toDouble(&ok_x);
			const qreal y = xy.value(1).toDouble(&ok_y);
			if (xy.size() != 2 || !ok_x || !ok_y || !qIsFinite(x) || !qIsFinite(y))
			{
				m_own_points.clear();
				break;
			}
			m_own_points << QPointF(x, y);
		}
		if (m_own_points.size() < 2) {
			m_own_points.clear();
		}
	}

		//Changed through the project database while its folio was not
		//built (lazy folios): the change, not what the file says
	if (diagram() && diagram()->project())
		if (const auto edited = diagram()->project()->dataBase()->takeUnbuiltWireEdit(m_uuid))
			pr = *edited;
	setProperties(pr);

	return retval;
}

/**
	@brief Conductor::toXml
	Exporte les caracteristiques du conducteur sous forme d'une element XML.
	@param dom_document :
	Le document XML a utiliser pour creer l'element XML
	@param table_adr_id :
	Hash stockant les correspondances entre les ids des
	bornes dans le document XML et leur adresse en memoire
	@param shared_uuids : uuids carried by more than one symbol of the
	folio. An end on such a symbol is written by its terminal id, as for a
	terminal without uuid: by uuid it would reopen on the first symbol
	carrying it (#1408).
	@return Un element XML representant le conducteur
*/
QDomElement Conductor::toXml(QDomDocument &dom_document,
				 QHash<Terminal *,
				 int> &table_adr_id,
				 const QSet<QUuid> &shared_uuids) const
{
	QDomElement dom_element = dom_document.createElement("conductor");

	if (m_persist_uuid)
		dom_element.setAttribute("uuid", m_uuid.toString());
	dom_element.setAttribute("x", QString::number(pos().x()));
	dom_element.setAttribute("y", QString::number(pos().y()));
	
	// Terminal is uniquely identified by the uuid of the terminal and the element
	if (terminal1->uuid().isNull()
		|| shared_uuids.contains(terminal1->parentElement()->uuid())) {
		// legacy method to identify the terminal
		dom_element.setAttribute("terminal1", table_adr_id.value(terminal1)); // for backward compatibility
	} else {
		dom_element.setAttribute("element1", terminal1->parentElement()->uuid().toString());
		dom_element.setAttribute("element1_label", terminal1->parentElement()->actualLabel());
		if (terminal1->parentElement()->linkedElements().isEmpty()) {
			//
		} else {
			dom_element.setAttribute("element1_linked", terminal1->parentElement()->linkedElements().last()->actualLabel());
		}
		dom_element.setAttribute("element1_name", terminal1->parentElement()->name());
		dom_element.setAttribute("terminal1", terminal1->uuid().toString());
		dom_element.setAttribute("terminalname1", terminal1->name());
	}

	if (terminal2->uuid().isNull()
		|| shared_uuids.contains(terminal2->parentElement()->uuid())) {
		// legacy method to identify the terminal
		dom_element.setAttribute("terminal2", table_adr_id.value(terminal2)); // for backward compatibility
	} else {
		dom_element.setAttribute("element2", terminal2->parentElement()->uuid().toString());
		dom_element.setAttribute("element2_label", terminal2->parentElement()->actualLabel());
		if (terminal2->parentElement()->linkedElements().isEmpty()) {
			//
		} else {
			dom_element.setAttribute("element2_linked", terminal2->parentElement()->linkedElements().last()->actualLabel());
		}
		dom_element.setAttribute("element2_name", terminal2->parentElement()->name());
		dom_element.setAttribute("terminal2", terminal2->uuid().toString());
		dom_element.setAttribute("terminalname2", terminal2->name());
	}
	dom_element.setAttribute("freezeLabel", m_freeze_label? "true" : "false");

	// on n'exporte les segments du conducteur que si ceux-ci ont
	// ete modifies par l'utilisateur
	if (modified_path)
	{
		// parcours et export des segments
		QDomElement current_segment;
		foreach(ConductorSegment *segment, segmentsList())
		{
			current_segment = dom_document.createElement("segment");
			current_segment.setAttribute("orientation", segment -> isHorizontal() ? "horizontal" : "vertical");
			current_segment.setAttribute("length", QString("%1").arg(segment -> length()));
			dom_element.appendChild(current_segment);
		}
	}

	if (modified_path && m_auto_arranged)
		dom_element.setAttribute(QStringLiteral("autoarranged"), QStringLiteral("true"));

		//The wire this one branches off, and where (T86)
	if (const Conductor *a = arm())
	{
		dom_element.setAttribute(QStringLiteral("arm"), a->uuid().toString());
		dom_element.setAttribute(QStringLiteral("arm_x"), QString::number(m_arm_point.x()));
		dom_element.setAttribute(QStringLiteral("arm_y"), QString::number(m_arm_point.y()));
		if (!m_own_points.isEmpty())
		{
			QStringList corners;
			for (const QPointF &p : std::as_const(m_own_points)) {
				corners << QString::number(p.x()) + QLatin1Char(',') + QString::number(p.y());
			}
			dom_element.setAttribute(QStringLiteral("own_shape"), corners.join(QLatin1Char(';')));
		}
	}

	QDomElement dom_seq = m_autoNum_seq.toXml(dom_document);
	dom_element.appendChild(dom_seq);

		// Export the properties and text
	properties().toXml(dom_element);
	if(m_text_item->wasMovedByUser())
	{
		dom_element.setAttribute("userx", QString::number(m_text_item->pos().x()));
		dom_element.setAttribute("usery", QString::number(m_text_item->pos().y()));
	}
	if(m_text_item->wasRotatedByUser())
		dom_element.setAttribute("rotation", QString::number(m_text_item->rotation()));

	return(dom_element);
}

/**
	@brief Conductor::pathFromXml
	Generate the path (of the line) from xml file by checking the segments in the xml
	file
	@param e
	@return true if generate path success else return false
*/
bool Conductor::pathFromXml(const QDomElement &e) {
	// parcourt les elements XML "segment" et en extrait deux listes de longueurs
	// les segments non valides sont ignores
	QList<qreal> segments_x, segments_y;
	for (QDomNode node = e.firstChild() ; !node.isNull() ; node = node.nextSibling()) {
		// on s'interesse aux elements XML "segment"
		QDomElement current_segment = node.toElement();
		if (current_segment.isNull() || current_segment.tagName() != "segment") continue;

		// le segment doit avoir une longueur
		if (!current_segment.hasAttribute("length")) continue;

		// cette longueur doit etre un reel
		bool ok;
		qreal segment_length = current_segment.attribute("length").toDouble(&ok);
			// toDouble() accepts "nan" and "inf". A NaN length would also pass
			// the coherence check below (every comparison with NaN is false)
			// and turn the whole path into NaN.
		if (!ok || !qIsFinite(segment_length)) continue;

		if (current_segment.attribute("orientation") == "horizontal") {
			segments_x << segment_length;
			segments_y << 0.0;
		} else {
			segments_x << 0.0;
			segments_y << segment_length;
		}
	}

	// If there is no segment, we generate an automatic path and return true
	if (!segments_x.size()) {
		generateConductorPath(terminal1 -> dockConductor(), terminal1 -> orientation(), terminal2 -> dockConductor(), terminal2 -> orientation());
		return(true);
	}

	// les longueurs recueillies doivent etre coherentes avec les positions des bornes
	// The collected lengths must be consistent with the positions of the terminals  
	qreal width = 0.0, height = 0.0;
	foreach (qreal t, segments_x) width  += t;
	foreach (qreal t, segments_y) height += t;
	QPointF t1 = terminal1 -> dockConductor();
	QPointF t2 = terminal2 -> dockConductor();
	qreal expected_width  = t2.x() - t1.x();
	qreal expected_height = t2.y() - t1.y();

	// on considere que le trajet est incoherent a partir d'une unite de difference avec l'espacement entre les bornes
	if (
		qAbs(expected_width  - width)  > 1.0 ||
		qAbs(expected_height - height) > 1.0
	) {
		qDebug() << "Conductor::fromXml : les segments du conducteur ne semblent pas coherents - utilisation d'un trajet automatique";
		return(false);
	}

	/* on recree les segments a partir des donnes XML */
	// cree la liste de points
	QList<QPointF> points_list;
	points_list << mapFromScene(t1);
	for (int i = 0 ; i < segments_x.size() ; ++ i) {
		points_list << QPointF(
			points_list.last().x() + segments_x.at(i),
			points_list.last().y() + segments_y.at(i)
		);
	}

	pointsToSegments(points_list);

	// initialise divers parametres lies a la modification des conducteurs
	modified_path = true;
	saveProfile(false);

	segmentsToPath();
	return(true);
}

/**
	@brief Conductor::handlerPoints
	@return The points used to draw the handler square, used to modify
	the path of the conductor.
	The points stored in the QVector are the middle point of each segments that compose the conductor,
	at exception of the first and last segment because there just here to extend the terminal.
*/
QVector<QPointF> Conductor::handlerPoints() const
{
	QList <ConductorSegment *> sl = segmentsList();
	if (sl.size() >= 3)
	{
		sl.removeFirst();
		sl.removeLast();
	}

	QVector <QPointF> middle_points;

	foreach(ConductorSegment *segment, sl)
		middle_points.append(segment->middle());

	return middle_points;
}

/// @return les segments de ce conducteur
const QList<ConductorSegment *> Conductor::segmentsList() const
{
	if (segments == nullptr) return(QList<ConductorSegment *>());

	QList<ConductorSegment *> segments_vector;
	ConductorSegment *segment = segments;

	while (segment -> hasNextSegment()) {
		segments_vector << segment;
		segment = segment -> nextSegment();
	}
	segments_vector << segment;
	return(segments_vector);
}

/**
	@brief Conductor::moveSegment
	Move one segment of this conductor by (dx, dy), the same rule
	handlerMouseMoveEvent() applies on a manual drag (WirePath::dragged):
	only the part of the movement across the segment counts, and a static
	(terminal-anchored) segment does not move. Unlike a drag this commits
	the whole move as a single undo step.
	@param index a segmentsList() index
	@param dx @param dy the movement, in the diagram's own coordinates
	@return false if index is out of range
*/
bool Conductor::moveSegment(int index, qreal dx, qreal dy)
{
	const QList<ConductorSegment *> segs = segmentsList();
	if (index < 0 || index >= segs.count()) return false;

	before_mov_text_pos_ = m_text_item->pos();
	pointsToSegments(WirePath::dragged(segmentsToPoints(), index, QPointF(dx, dy)));
	modified_path = true;
	segmentsToPath();
	calculateTextItemPosition();
	saveProfile();
	return true;
}

/**
	@brief Conductor::setPathPoints
	Replace this conductor's path by the one through @p scene_points, and
	push one undo step for it -- the same ChangeConductorCommand a manual
	handle drag or moveSegment() pushes, so the path is saved as a
	modified one and survives a reload.
	@param scene_points in scene coordinates, from terminal1's docking
	point to terminal2's, every segment horizontal or vertical
	@return false, changing nothing, if the points do not make such a path
*/
bool Conductor::setPathPoints(const QList<QPointF> &scene_points)
{
	return applyPathPoints(scene_points, true);
}

/**
	@brief Conductor::setArrangedPath
	The same as setPathPoints(), with no undo step of its own - the caller,
	ArrangeConductorsCommand, is one - and the path marked as placed by the
	application (isAutoArranged()).
	@return false, changing nothing, if the points do not make a path
*/
bool Conductor::setArrangedPath(const QList<QPointF> &scene_points)
{
	if (!applyPathPoints(scene_points, false)) return false;
	m_auto_arranged = true;
	return true;
}

/**
	@brief Conductor::applyPathPoints
	@param by_hand : the change is the user's, with its undo step
*/
bool Conductor::applyPathPoints(const QList<QPointF> &scene_points, bool by_hand)
{
	if (scene_points.size() < 2 || !terminal1 || !terminal2) return false;
		//The path of a branch is the one its arm gives it (T86)
	if (arm()) return false;
	const auto isNear = [](const QPointF &a, const QPointF &b) {
		return qAbs(a.x() - b.x()) < 0.5 && qAbs(a.y() - b.y()) < 0.5;
	};
	if (!isNear(scene_points.first(), terminal1->dockConductor()) ||
		!isNear(scene_points.last(), terminal2->dockConductor()))
		return false;
	QList<QPointF> points;
	for (int i = 0; i < scene_points.size(); ++i) {
		const QPointF p = scene_points.at(i);
		if (i && qAbs(p.x() - scene_points.at(i - 1).x()) > 0.01
			  && qAbs(p.y() - scene_points.at(i - 1).y()) > 0.01)
			return false;
		points << mapFromScene(p);
	}
		// A two-point path has nothing to segment between: let the
		// application draw it, as for a conductor with no profile.
	if (points.size() < 3) return false;

	before_mov_text_pos_ = m_text_item->pos();
	pointsToSegments(points);
	modified_path = true;
	segmentsToPath();
	calculateTextItemPosition();
	saveProfile(by_hand);
	return true;
}

/**
	@brief Conductor::length
	@return the length of this conductor
*/
qreal Conductor::length() const{
	return path().length();
}

/**
	@return Le segment qui contient le point au milieu du conducteur
*/
ConductorSegment *Conductor::middleSegment()
{
	if (segments == nullptr) return(nullptr);

	qreal half_length = length() / 2.0;

	ConductorSegment *s = segments;
	qreal l = 0;

	while (s -> hasNextSegment()) {
		l += qAbs(s -> length());
		if (l >= half_length) break;
		s = s -> nextSegment();
	}
	// s est le segment qui contient le point au milieu du conducteur
	return(s);
}

/**
	@brief Conductor::posForText
	Calculate and return the better pos for text.
	@param flag : flag is used to know if text pos is near of
	a vertical or horizontal conductor segment.
	@param run_length : when given, receives the length of the run the
	text is centred on: the whole conductor when it is straight, the
	longest segment otherwise
*/
QPointF Conductor::posForText(Qt::Orientations &flag, qreal *run_length)
{
	ConductorSegment *segment        = segments;
	bool all_segments_are_vertical   = true;
	bool all_segments_are_horizontal = true;

		//Go to first segment
	while (!segment->isFirstSegment()) {
		segment = segment->previousSegment();
	}

	QPointF p1 = segment -> firstPoint(); //<First point of conductor
	ConductorSegment *longest_segment = segment; //<the longest segment of conductor.

		// check if all segments are horizontal or vertical ... first segment:
	if (segment -> firstPoint().x() != segment -> secondPoint().x())
		all_segments_are_vertical   = false;
	if (segment -> firstPoint().y() != segment -> secondPoint().y())
		all_segments_are_horizontal = false;
		// find longest segment
	while (segment -> hasNextSegment())
	{
		segment = segment -> nextSegment();
		if (segment -> firstPoint().x() != segment -> secondPoint().x())
			all_segments_are_vertical   = false;
		if (segment -> firstPoint().y() != segment -> secondPoint().y())
			all_segments_are_horizontal = false;

		if (qAbs(segment->length()) > qAbs(longest_segment -> length()))
			longest_segment = segment;
	}

	QPointF p2 = segment -> secondPoint(); //<Last point of conductor

	//If the conductor is horizontal or vertical
	//Return the point at the middle of conductor
	if (all_segments_are_vertical) { //<Vertical
		flag = Qt::Vertical;
		if (p1.y() > p2.y()) {
			p1.setY(p1.y() - (length()/2));
		} else {
			p1.setY(p1.y() + (length()/2));
		}
	} else if (all_segments_are_horizontal) { //<Horizontal
		flag = Qt::Horizontal;
		if (p1.x() > p2.x()) {
			p1.setX(p1.x() - (length()/2));
		} else {
			p1.setX(p1.x() + (length()/2));
		}
	} else { //Return the point at the middle of longest segment.
		p1 = longest_segment->middle();
		flag = (longest_segment->isHorizontal())? Qt::Horizontal : Qt::Vertical;
	}
	if (run_length)
	{
		*run_length = (all_segments_are_vertical || all_segments_are_horizontal)
				? length()
				: qAbs(longest_segment->length());
	}
	return p1;
}

/**
	@brief Conductor::updateTextVisibility
	Show the text of this conductor if its properties ask for it and wire
	numbers are not hidden (View > Show). With "one text per potential",
	call calculateTextItemPosition() afterwards: it hides all texts of the
	potential but the longest conductor's.
*/
void Conductor::updateTextVisibility()
{
		//Not when another conductor of its point shows the name for it
		//(T67, tenth adjustment)
	ShownKinds::setVisible(m_text_item,
						   m_properties.type == ConductorProperties::Multi
						   && m_properties.m_show_text
						   && !m_name_in_point);
}

/**
	@brief Conductor::calculateTextItemPosition
	Move the text at middle of conductor (if is vertical or horizontal)
	otherwise, move conductor at the middle of the longest segment of conductor.
	If text was moved by user, this function do nothing, except check if text is near conductor.
	The text is drawn at the largest size, from the one the properties hold
	down to ConductorTextFit::smallest, at which it fits that run - a text
	moved or turned by the user too, its place kept. The properties
	themselves are never changed.
*/
void Conductor::calculateTextItemPosition()
{
	if (!m_text_item || !diagram() || m_properties.type != ConductorProperties::Multi)
		return;

	if (diagram() -> defaultConductorProperties.m_one_text_per_folio == true &&
		relatedPotentialConductors(false).size() > 0)
	{

		Conductor *longest_conductor = longestConductorInPotential(this);

			//This conductor isn't the longest conductor?
			//We call calculateTextItemPosition for the longest conductor
		if(longest_conductor != this)
		{
			longest_conductor -> calculateTextItemPosition();
			return;
		}

			//At this point this conductor is the longest conductor we hide all text of conductor_list
		foreach (Conductor *c, relatedPotentialConductors(false)) {
					ShownKinds::setVisible(c -> textItem(), false);
		}
			//Make sure text item is visible, unless wire numbers are hidden
			//(View > Show)
		ShownKinds::setVisible(m_text_item, true);
	}

		//position
	if (m_text_item -> wasMovedByUser())
	{
			//The user's own place: never one name for the point
		if (m_name_in_point)
		{
			m_name_in_point = false;
			updateTextVisibility();
		}
			//Placed by the user: still drawn at the size that fits the
			//run the application centres it on, so that a drag never
			//makes it jump to another size the next time the conductor
			//is laid out
		Qt::Orientations along;
		qreal run_length = 0;
		posForText(along, &run_length);
		fitTextToRun(m_text_item, m_properties.text_size, along, run_length);

			//Text field was moved by user:
			//we check if text field is yet  near the conductor
		QPointF text_item_pos = m_text_item -> pos();
		QPainterPath near_shape = nearShape();
		if (!near_shape.contains(text_item_pos)) {
			m_text_item -> setPos(movePointIntoPolygon(text_item_pos, near_shape));
		}
	}
	else
	{
			//Position and rotation of text is calculated: on the longest
			//run the name fits without touching anything, and on the
			//longest run when there is none (T71). The longest first: on a
			//folio where nothing touches, the name is where it always was,
			//at the cost of one look at the scene.
		const QVector<TextRun> runs = textRuns();
		bool placed = false;
			//First the pieces of runs no conductor of another potential
			//crosses, and not the line a point of several conductors
			//shares (T67, fifth adjustment); then the runs, as before
		QVector<NamePlace> places;
		for (const TextRun &run : runs) {
			places << NamePlace{run.middle, run.along, run.length};
		}
		bool any_shared = false;
		const QVector<NamePlace> free = freePlaces(this, places, relatedPotentialConductors(false),
							   false, &any_shared);
			//The other side of a run, when the side the name is written on
			//touches something (T67, seventh adjustment): a wire that goes
			//round a component above it has its name under the run, on the
			//component, and nothing above it
		const auto otherSideIsClear = [this](const TextRun &run) {
			const Qt::Alignment horizontal = m_properties.m_horizontal_alignment;
			const Qt::Alignment vertical = m_properties.m_vertical_alignment;
			m_properties.m_horizontal_alignment = horizontal == Qt::AlignTop ? Qt::AlignBottom
					: horizontal == Qt::AlignBottom ? Qt::AlignTop : horizontal;
			m_properties.m_vertical_alignment = vertical == Qt::AlignLeft ? Qt::AlignRight
					: vertical == Qt::AlignRight ? Qt::AlignLeft : vertical;
			const bool moved = m_properties.m_horizontal_alignment != horizontal
					|| m_properties.m_vertical_alignment != vertical;
			bool clear = false;
			if (moved) {
				placeTextOn(run);
				clear = !nameTouches(this, m_text_item, diagram());
			}
			m_properties.m_horizontal_alignment = horizontal;
			m_properties.m_vertical_alignment = vertical;
			if (moved && !clear) {
				placeTextOn(run);
			}
			return clear;
		};
			//The side the name is written on first, on every run it fits,
			//and only then the other side: a name leaves a run for another
			//one before it changes sides (T71, CU-71.1)
		const auto otherSideOfOne = [&otherSideIsClear](const QVector<TextRun> &fitting) {
			for (const TextRun &run : fitting) {
				if (otherSideIsClear(run)) return true;
			}
			return false;
		};
		QVector<TextRun> fitting;
		for (const NamePlace &place : free)
		{
			const TextRun run{place.middle, place.along, place.length};
			if (!placeTextOn(run)) {
				break;
			}
			fitting << run;
			if (!nameTouches(this, m_text_item, diagram())) {
				placed = true;
				break;
			}
		}
		if (!placed) {
			placed = otherSideOfOne(fitting);
		}
			//None of its own pieces holds the name, and the conductor shares
			//a line with others of its potential: the point shows its name
			//once, on that line, by one of them - of those no piece of their
			//own holds it for, the one with the longest piece of the line,
			//then the first by uuid -, and the others hide theirs; each one
			//an arm holds it for shows it there (T67, tenth adjustment). Not
			//when the line does not hold it either: then each its own,
			//smaller, as before
		const bool one_per_folio = diagram()->defaultConductorProperties.m_one_text_per_folio;
			//In a live move, what was decided stays, and is decided again
			//once at its end (beginLiveMove()): a hidden name stays hidden,
			//the one that shows the name of the point keeps showing it, on
			//the line of the group it had. Deciding at each step cost a
			//drag of the common terminal of six wires 330 ms a step
		const bool live = s_live_moves > 0;
		if (live)
		{
			bool known = false;
			for (const QPointer<Conductor> &c : std::as_const(s_live_touched)) {
				if (c == this) known = true;
			}
			if (!known) s_live_touched << this;
		}
		QList<Conductor *> group;
		if (live)
		{
			if (!one_per_folio && !placed && fitting.isEmpty())
			{
				if (m_name_in_point) {
					placed = true;
				}
				else if (m_shows_point_name)
				{
					QList<Conductor *> known{this};
					for (const QPointer<Conductor> &c : std::as_const(m_point_group)) {
						if (c) known << c;
					}
					placed = placeNameOnSharedLine(known);
				}
			}
		}
		else if (!one_per_folio && (any_shared || !m_point_group.isEmpty())) {
			group = pointGroup();
		}
		bool in_point = false;
		bool shows = false;
		if (!live && !placed && fitting.isEmpty() && group.size() > 1)
		{
			Conductor *shower = this;
			qreal shower_piece = longestSharedPiece();
			for (Conductor *c : std::as_const(group))
			{
				if (c == this || !c->m_text_item || c->m_text_item->wasMovedByUser()
				    || c->nameHoldsOnOwnPiece()) {
					continue;
				}
				const qreal piece = c->longestSharedPiece();
				if (piece > shower_piece + 0.01
				    || (std::abs(piece - shower_piece) <= 0.01
					&& c->uuid().toString() < shower->uuid().toString()))
				{
					shower = c;
					shower_piece = piece;
				}
			}
			if (shower == this) {
				placed = placeNameOnSharedLine(group);
				shows = placed;
			} else if (shower->sharedLineHoldsName()) {
				in_point = true;
				placed = true;
			}
		}
		if (!live && !one_per_folio && in_point != m_name_in_point)
		{
			m_name_in_point = in_point;
			updateTextVisibility();
		}
		if (!live) {
			m_shows_point_name = shows;
		}
			//None of its own pieces takes the name: the longest of them,
			//the name drawn smaller - and not the line it shares
		if (!placed && !free.isEmpty())
		{
			const NamePlace &longest = free.first();
			placeTextOn(TextRun{longest.middle, longest.along, longest.length});
			placed = true;
		}
		fitting.clear();
		for (int i = 0 ; !placed && i < runs.size() ; ++i)
		{
				//A run the name does not fit: the shorter ones do not either
			if (!placeTextOn(runs.at(i))) {
				break;
			}
			fitting << runs.at(i);
			if (runs.size() == 1 || !nameTouches(this, m_text_item, diagram())) {
				placed = true;
			}
		}
		if (!placed) {
			placed = otherSideOfOne(fitting);
		}
		if (!placed && !runs.isEmpty()) {
			placeTextOn(runs.first());
		}

			//The others of the point, those of before and those of now,
			//placed again with this one: which of them shows the name
			//depends on all of them. Then this one again, the others in
			//their places. Not in a live move: its end does it
		if (live) {
			return;
		}
		QList<QPointer<Conductor>> again = m_point_group;
		m_point_group.clear();
		for (Conductor *c : std::as_const(group)) {
			if (c != this) m_point_group << c;
		}
		static bool regrouping = false;
		if (!regrouping && (!again.isEmpty() || !m_point_group.isEmpty()))
		{
			regrouping = true;
			for (const QPointer<Conductor> &c : std::as_const(m_point_group)) {
				if (!again.contains(c)) again << c;
			}
			for (const QPointer<Conductor> &c : std::as_const(again)) {
				if (c && c != this) c->calculateTextItemPosition();
			}
			calculateTextItemPosition();
			regrouping = false;
		}
	}
}

/**
	@brief Conductor::pointPartners
	@return the conductors of the potential of this one, on its folio, that
	run along a line of it for some length - the conductors of its point
	that share a trunk with it (T67, tenth adjustment)
*/
QList<Conductor *> Conductor::pointPartners()
{
	QList<Conductor *> partners;
	if (!scene()) {
		return partners;
	}
	const QList<ConductorSegment *> mine = segmentsList();
	const QSet<Conductor *> same = relatedPotentialConductors(false);
	for (Conductor *other : same)
	{
		if (other == this || !other->scene() || other->scene() != scene()) {
			continue;
		}
		const QList<ConductorSegment *> theirs = other->segmentsList();
		bool shares = false;
		for (const ConductorSegment *a : mine)
		{
			for (const ConductorSegment *b : theirs)
			{
				if (alongOneLine(mapToScene(a->firstPoint()), mapToScene(a->secondPoint()),
						 other->mapToScene(b->firstPoint()), other->mapToScene(b->secondPoint()))) {
					shares = true;
					break;
				}
			}
			if (shares) {
				break;
			}
		}
		if (shares) {
			partners << other;
		}
	}
	return partners;
}

/**
	@brief Conductor::pointGroup
	@return this conductor and those that share a line with it, or with
	one that does, and so on: the conductors whose names are one name when
	no arm holds it (T67, tenth adjustment)
*/
QList<Conductor *> Conductor::pointGroup()
{
	QList<Conductor *> group{this};
	for (int i = 0 ; i < group.size() ; ++i)
	{
		const QList<Conductor *> partners = group.at(i)->pointPartners();
		for (Conductor *c : partners) {
			if (!group.contains(c)) group << c;
		}
	}
	return group;
}

/**
	@brief Conductor::nameHoldsOnOwnPiece
	@return whether a piece of this conductor of its own - one no conductor
	of its potential runs along - holds its name, at a size it may be drawn
	at. Asked of the others of a point, and so the text is left as it was
	(T67, tenth adjustment).
*/
bool Conductor::nameHoldsOnOwnPiece()
{
	if (!m_text_item || !diagram()) {
		return true;
	}
	QVector<NamePlace> places;
	const QVector<TextRun> runs = textRuns();
	for (const TextRun &run : runs) {
		places << NamePlace{run.middle, run.along, run.length};
	}
	bool any_shared = false;
	QVector<NamePlace> own = freePlaces(this, places, relatedPotentialConductors(false),
					    false, &any_shared);
	if (own.isEmpty() && !any_shared) {
		own = places;
	}
	if (own.isEmpty()) {
		return false;
	}
	const QFont font = m_text_item->font();
	const qreal rotation = m_text_item->rotation();
	const QPointF pos = m_text_item->pos();
	const bool holds = placeTextOn(TextRun{own.first().middle, own.first().along, own.first().length});
	m_text_item->setFont(font);
	m_text_item->setRotation(rotation);
	m_text_item->setPos(pos);
	return holds;
}

/**
	@brief Conductor::longestSharedPiece
	@return the length of the longest piece of the line this conductor
	shares with others of its potential that no conductor of another
	potential crosses; 0 when it shares none (T67, tenth adjustment)
*/
qreal Conductor::longestSharedPiece()
{
	if (!diagram()) {
		return 0;
	}
	QVector<NamePlace> places;
	const QVector<TextRun> runs = textRuns();
	for (const TextRun &run : runs) {
		places << NamePlace{run.middle, run.along, run.length};
	}
	const QVector<NamePlace> shared = freePlaces(this, places, relatedPotentialConductors(false), true);
	return shared.isEmpty() ? 0 : shared.first().length;
}

/**
	@brief Conductor::sharedLineHoldsName
	@return whether the line this conductor shares with others of its
	potential holds its name, on its longest piece no conductor of another
	potential crosses. The text is left as it was (T67, tenth adjustment).
*/
bool Conductor::sharedLineHoldsName()
{
	if (!m_text_item || !diagram()) {
		return false;
	}
	QVector<NamePlace> places;
	const QVector<TextRun> runs = textRuns();
	for (const TextRun &run : runs) {
		places << NamePlace{run.middle, run.along, run.length};
	}
	const QVector<NamePlace> shared = freePlaces(this, places, relatedPotentialConductors(false), true);
	if (shared.isEmpty()) {
		return false;
	}
	const QFont font = m_text_item->font();
	const qreal rotation = m_text_item->rotation();
	const QPointF pos = m_text_item->pos();
	bool holds = false;
	for (const NamePlace &place : shared)
	{
		if (placeTextOn(TextRun{place.middle, place.along, place.length}))
		{
			holds = true;
			break;
		}
	}
	m_text_item->setFont(font);
	m_text_item->setRotation(rotation);
	m_text_item->setPos(pos);
	return holds;
}

/**
	@brief Conductor::placeNameOnSharedLine
	Put the name of this conductor on the line it shares with others of its
	potential, for all of them: on a piece no conductor of another potential
	crosses, the one most of which the whole point runs along first - the
	line from the common terminal, and not a trunk only the farther arms go
	down -, then the longest; the first that holds the name and touches
	nothing, else the first that holds it (T67, tenth adjustment).
	@return false when no piece of that line holds it
*/
bool Conductor::placeNameOnSharedLine(const QList<Conductor *> &group)
{
	QVector<NamePlace> places;
	const QVector<TextRun> runs = textRuns();
	for (const TextRun &run : runs) {
		places << NamePlace{run.middle, run.along, run.length};
	}
	QVector<NamePlace> shared = freePlaces(this, places, relatedPotentialConductors(false), true);
	if (shared.isEmpty()) {
		return false;
	}
		//In tenths, so that the order is a strict one
	QVector<int> by_all;
	for (int i = 0 ; i < shared.size() ; ++i) {
		by_all << qRound(sharedByAll(this, shared.at(i), group) * 10);
	}
	QVector<int> order;
	for (int i = 0 ; i < shared.size() ; ++i) order << i;
	std::stable_sort(order.begin(), order.end(), [&by_all](int a, int b) {
		return by_all.at(a) > by_all.at(b);
	});
	int holding = -1;
	for (int i : std::as_const(order))
	{
		const NamePlace &place = shared.at(i);
		if (!placeTextOn(TextRun{place.middle, place.along, place.length})) {
			continue;
		}
		if (holding < 0) {
			holding = i;
		}
		if (!nameTouches(this, m_text_item, diagram())) {
			return true;
		}
	}
	if (holding < 0) {
		return false;
	}
	const NamePlace &place = shared.at(holding);
	placeTextOn(TextRun{place.middle, place.along, place.length});
	return true;
}


/**
	@brief Conductor::textRuns
	@return the runs the name of this conductor may be written along, the
	longest first and, between two of one length, in the order of the
	path: the whole conductor when it is straight, as posForText() puts it
	there, and each of its segments otherwise. The first one is always
	where posForText() puts the name.
*/
QVector<Conductor::TextRun> Conductor::textRuns()
{
	QVector<TextRun> runs;
	Qt::Orientations along;
	qreal run_length = 0;
	const QPointF middle = posForText(along, &run_length);

	ConductorSegment *segment = segments;
	if (!segment) {
		runs << TextRun{middle, along, run_length};
		return runs;
	}
	while (!segment->isFirstSegment()) {
		segment = segment->previousSegment();
	}
	bool all_vertical = true;
	bool all_horizontal = true;
		//The two ends of each run: a segment in line with the one before it
		//goes on the same run (T71, 22/10/2026) - the straight line the eye
		//sees, and not the pieces the path is drawn in. The end of a
		//terminal's own piece is such a point, and the name went in the
		//middle of the longer piece, against the bend at its other end
	QVector<QPair<QPointF, QPointF>> lines;
	for (ConductorSegment *s = segment ; s ; s = s->hasNextSegment() ? s->nextSegment() : nullptr)
	{
		const QPointF a = s->firstPoint();
		const QPointF b = s->secondPoint();
		if (a.x() != b.x()) all_vertical = false;
		if (a.y() != b.y()) all_horizontal = false;
		if (!lines.isEmpty())
		{
			const QPointF from = lines.last().first;
			const QPointF to = lines.last().second;
			const bool in_line = s->isHorizontal()
					? (qAbs(from.y() - to.y()) < 0.01 && qAbs(a.y() - to.y()) < 0.01)
					: (qAbs(from.x() - to.x()) < 0.01 && qAbs(a.x() - to.x()) < 0.01);
			const bool same_way = (b - a).x() * (to - from).x() >= 0 && (b - a).y() * (to - from).y() >= 0;
			if (in_line && same_way && (a - to).manhattanLength() < 0.01)
			{
				lines.last().second = b;
				continue;
			}
		}
		lines << qMakePair(a, b);
	}
	for (const auto &line : std::as_const(lines))
	{
		const QPointF a = line.first;
		const QPointF b = line.second;
		runs << TextRun{(a + b) / 2,
				qAbs(a.y() - b.y()) < 0.01 ? Qt::Horizontal : Qt::Vertical,
				(b - a).manhattanLength()};
	}
	if (all_vertical || all_horizontal) {
		runs = {TextRun{middle, along, run_length}};
		return runs;
	}
	std::stable_sort(runs.begin(), runs.end(),
			 [](const TextRun &a, const TextRun &b) {return a.length > b.length;});
	return runs;
}

/**
	@brief Conductor::placeTextOn
	Put the name of this conductor on @a run: turned along it, unless the
	user turned it; drawn at the size that fits it (T70); centred on it;
	and pushed off the conductor itself.
	@return true when the name fits the run, at the size it is drawn at
*/
bool Conductor::placeTextOn(const TextRun &run)
{
	QPointF text_pos = run.middle;
	const Qt::Orientations rotation = run.along;

	if (!m_text_item -> wasRotatedByUser())
	{
		rotation == Qt::Vertical ? m_text_item -> setRotation(m_properties.verti_rotate_text):
								   m_text_item -> setRotation(m_properties.horiz_rotate_text);

	}
		//The name fits the run it is centred on: drawn smaller when
		//it has to, from the size the file holds, which does not
		//change - turned by the user too, so that nothing jumps. Before
		//the position below, which centres the text by its width at
		//the size it is drawn at.
	fitTextToRun(m_text_item, m_properties.text_size, rotation, run.length);

		//Adjust the position of text if his rotation
		//is 0° or 270°, to be exactly centered to the conductor
	if (m_text_item -> rotation() == 0)
	{
		text_pos.rx() -= m_text_item -> boundingRect().width()/2;
		if(m_properties.m_horizontal_alignment == Qt::AlignTop)
			text_pos.ry() -= m_text_item->boundingRect().height();
	}
	else if (m_text_item -> rotation() == 270)
	{
		text_pos.ry() += m_text_item -> boundingRect().width()/2;
		if(m_properties.m_vertical_alignment == Qt::AlignLeft)
			text_pos.rx() -= m_text_item->boundingRect().height();
	}

	m_text_item -> setPos(text_pos);

		//Ensure text item does not collide with the run it is written
		//along. The run, and not the whole conductor: a name a little
		//longer than its run reaches the corner, and pushed off the whole
		//conductor it went along the next segment, as far as that one
		//goes - off its arm and onto the common line of a point, seen on
		//a point of several conductors (T67, fifth adjustment)
	const qreal half = (m_mouse_over ? 5 : 1) / 2.0;
	const QRectF run_line = rotation == Qt::Horizontal
			? QRectF(run.middle.x() - run.length / 2, run.middle.y() - half,
				 run.length, 2 * half)
			: QRectF(run.middle.x() - half, run.middle.y() - run.length / 2,
				 2 * half, run.length);
	while (m_text_item->mapToParent(m_text_item->shape()).intersects(run_line))
	{
		if(rotation == Qt::Vertical)
		{
			if(m_properties.m_vertical_alignment == Qt::AlignRight)
				m_text_item->setX(m_text_item->x()+1);
			else if (m_properties.m_vertical_alignment == Qt::AlignLeft)
				m_text_item->setX(m_text_item->x()-1);
			else
				break; //avoid infinite loop
		}
		else if (rotation == Qt::Horizontal)
		{
			if(m_properties.m_horizontal_alignment == Qt::AlignTop)
				m_text_item->setY(m_text_item->y()-1);
			else if (m_properties.m_horizontal_alignment == Qt::AlignBottom)
				m_text_item->setY(m_text_item->y()+1);
			else
				break; //avoid infinite loop
		}
		else
			break;
	}
	return extentAlong(m_text_item, rotation) <= ConductorTextFit::room(run.length);
}

/**
	Sauvegarde le profil courant du conducteur pour l'utiliser ulterieurement
	dans priv_modifieConductor.
	Save the current conductors profile for later use in priv_modifieConductor.
*/
void Conductor::saveProfile(bool undo) {
	Qt::Corner current_path_type = currentPathType();
	ConductorProfile old_profile(conductor_profiles[current_path_type]);
	conductor_profiles[current_path_type].fromConductor(this);
		//Saved with an undo step, the change is the user's: the path is no
		//longer the application's to move (WireArrangement)
	const bool was_arranged = m_auto_arranged;
	if (undo) m_auto_arranged = false;
	Diagram *dia = diagram();
	if (undo && dia) {
		ChangeConductorCommand *undo_object = new ChangeConductorCommand(
			this,
			old_profile,
			conductor_profiles[current_path_type],
			current_path_type
		);
		undo_object -> setConductorTextItemMove(before_mov_text_pos_, m_text_item -> pos());
		if (was_arranged) undo_object -> setAutoArranged(true, false);
		dia -> undoStack().push(undo_object);
	}
}

/**
	@param value1 Premiere valeur
	@param value2 Deuxieme valeur
	@return 1 si les deux valeurs sont de meme signe, -1 sinon
*/
int Conductor::getCoeff(const qreal &value1, const qreal &value2) {
	return(getSign(value1) * getSign(value2));
}

/**
	@param value valeur
	@return 1 si valeur est negatif, 1 s'il est positif ou nul
*/
int Conductor::getSign(const qreal &value) {
	return(value < 0 ? -1 : 1);
}

/**
	Applique un nouveau profil a ce conducteur
	@param cp Profil a appliquer a ce conducteur
	@param path_type Type de trajet pour lequel ce profil convient
*/
void Conductor::setProfile(const ConductorProfile &cp, Qt::Corner path_type) {
	conductor_profiles[path_type] = cp;
	// si le type de trajet correspond a l'actuel
	if (currentPathType() == path_type) {
		if (conductor_profiles[path_type].isNull()) {
			generateConductorPath(terminal1 -> dockConductor(), terminal1 -> orientation(), terminal2 -> dockConductor(), terminal2 -> orientation());
			modified_path = false;
		} else {
			updateConductorPath(terminal1 -> dockConductor(), terminal1 -> orientation(), terminal2 -> dockConductor(), terminal2 -> orientation());
			modified_path = true;
		}
			//The kind of conductor, and not type(), the kind of graphics
			//item, which was never Multi: the name was never placed again
			//when an undo put a path back - and a point whose names are one
			//name kept the names of the path undone (T67, tenth adjustment)
		if (m_properties.type == ConductorProperties::Multi) {
			calculateTextItemPosition();
		}
	}
}

/// @return le profil de ce conducteur
ConductorProfile Conductor::profile(Qt::Corner path_type) const
{
	return(conductor_profiles[path_type]);
}

/**
	@brief Conductor::refreshText
	Refresh the text of this conductor.
	recalculate and set the text according to the formula.
*/
void Conductor::refreshText()
{
	const QString shown = m_text_item->toPlainText();

	if (m_freeze_label)
	{
		m_text_item->setPlainText(m_properties.text);
	}
	else
	{
		if (!m_properties.m_formula.isEmpty())
		{
			if (diagram())
			{
				QString text = autonum::AssignVariables::formulaToLabel(m_properties.m_formula, m_autoNum_seq, diagram(), nullptr, this);
				m_properties.text = text;
				m_text_item->setPlainText(text);
			}
			else
			{
				m_properties.text = m_properties.m_formula;
				m_text_item->setPlainText(m_properties.text);
			}
			storeProperties();
		}
		else
		{
			m_text_item->setPlainText(m_properties.text);
		}
	}

		//A name of another length is fitted to its run and centred on it
		//again: the size it is drawn at and its position both depend on
		//its width
	if (m_text_item->toPlainText() != shown)
		calculateTextItemPosition();
}

void Conductor::setPath(const QPainterPath &path)
{
	if(path == m_path)
		return;

	prepareGeometryChange();
	m_path = path;
	conductorGeometryChanged();
	update();
}

QPainterPath Conductor::path() const
{
	return m_path;
}

/**
	@brief Conductor::setPropertiesToPotential
	@param property
	@param only_text
	Set property to conductor and every conductors in
	the same potential of conductor.
	If only_text is true only formula, text,
	function and tension/protocol is set
	to other conductor in the same potential,
	the other values of property stay unmodified
*/
void Conductor::setPropertyToPotential(const ConductorProperties &property,
					   bool only_text)
{
		//One gesture as far as the data base is concerned, whatever the size
		//of the potential. No caller in this tree reaches here today, which is
		//exactly why the guard is written now rather than left to whoever adds
		//the first one.
	projectDataBase::Operation operation(
			diagram() ? diagram()->project() : nullptr);

	setProperties(property);
	QSet <Conductor *> potential_list = relatedPotentialConductors();

	foreach(Conductor *other_conductor, potential_list)
	{
		if (only_text)
		{
			ConductorProperties other_properties = other_conductor->properties();
			other_properties.m_formula = m_properties.m_formula;
			other_properties.text = m_properties.text;
			other_properties.m_function = m_properties.m_function;
			other_properties.m_tension_protocol = m_properties.m_tension_protocol;
			other_properties.m_wire_color = m_properties.m_wire_color;
			other_properties.m_wire_section = m_properties.m_wire_section;
			other_conductor->setProperties(other_properties);
		}
		else
		{
			other_conductor->setProperties(property);
		}
	}
}

/**
	@brief Conductor::setProperties
	Set property as current property of conductor
	@param property : properties
*/
void Conductor::setProperties(const ConductorProperties &property)
{
	if (m_properties == property) return;

	QString formula = m_properties.m_formula;
	m_properties = property;

	if (!m_properties.m_formula.isEmpty())
	{
			//Left out of the numbering: the text given is the text shown,
			//and the formula is not worked out
		if (!m_freeze_label)
		{
			if (diagram())
			{
				QString text = autonum::AssignVariables::formulaToLabel(m_properties.m_formula, m_autoNum_seq, diagram(), nullptr, this);
				m_properties.text = text;
			}
			else if (m_properties.text.isEmpty())
			{
				m_properties.text = m_properties.m_formula;
			}
		}

		setUpConnectionForFormula(formula, m_properties.m_formula);
	}

	m_text_item->setPlainText(m_properties.text);
	QFont font = m_text_item->font();
	font.setPointSize(m_properties.text_size);
	m_text_item->setFont(font);
	m_text_item->setColor(m_properties.text_color);

	updateTextVisibility();
	calculateTextItemPosition();
	update();

	storeProperties();
	emit propertiesChange();
}

/**
	@brief Conductor::properties
	@return the properties of this Conductor
*/
ConductorProperties Conductor::properties() const
{
	if (projectDataBase *store = placedStore())
		return store->conductorProperties(m_uuid);
	return(m_properties);
}

/**
	@brief Conductor::placedStore
	@return the project database holding this wire's properties
	or nullptr if it is not placed in a
	project, or shares its uuid with another wire for the moment a copy
	takes to get its own
*/
projectDataBase *Conductor::placedStore() const
{
	Diagram *d = diagram();
	QETProject *project = d ? d->project() : nullptr;
	projectDataBase *store = project ? project->dataBase() : nullptr;
	return store && store->hasConductorProperties(m_uuid)
			&& store->placedConductorCount(m_uuid) == 1 ? store : nullptr;
}

/**
	@brief Conductor::storeProperties
	Write this wire's properties to its project's store, once it is placed:
	every change of a placed wire's properties comes through here.
*/
void Conductor::storeProperties()
{
	Diagram *d = diagram();
	QETProject *project = d ? d->project() : nullptr;
	if (project && project->dataBase() && project->dataBase()->hasConductorProperties(m_uuid))
		project->dataBase()->conductorPropertiesStored(this, m_properties);
}

/**
	@brief Conductor::newUuid
	Make a new uuid for this conductor; its stored properties and its row
	follow it.
*/
void Conductor::newUuid()
{
	setUuid(QUuid::createUuid());
}

/**
	@brief Conductor::setUuid
	@param uuid : the uuid of this conductor, saved from now on
*/
void Conductor::setUuid(const QUuid &uuid)
{
	const QUuid old_uuid = m_uuid;
	m_uuid = uuid;
	m_persist_uuid = true;
	uuidChanged(old_uuid);
}

/**
	@brief Conductor::uuidChanged
	Tell the project's store this placed wire's uuid changed from
	@p old_uuid (a paste renews it, a file gives it).
*/
void Conductor::uuidChanged(const QUuid &old_uuid)
{
	Diagram *d = diagram();
	QETProject *project = d ? d->project() : nullptr;
	if (project && project->dataBase())
		project->dataBase()->conductorUuidChanged(this, old_uuid);
}

/**
	@brief Conductor::setCableReference
	Record the cable core this conductor shows, together with the label
	that assignment generates. Called only by CableManager, which works
	the values out from the cable itself: the user never types them.
	@param label the generated cable field, e.g. "15W1:br", empty to
	detach the conductor from its cable
	@param cable_uuid the cable this conductor is a core of
	@param slot which core of that cable it shows, -1 when none
*/
void Conductor::setCableReference(const QString &label,
								  const QUuid &cable_uuid,
								  int slot)
{
	if (m_properties.m_cable == label
		&& m_properties.m_cable_uuid == cable_uuid
		&& m_properties.m_cable_slot == slot) {
		return;
	}

	m_properties.m_cable = label;
	m_properties.m_cable_uuid = cable_uuid;
	m_properties.m_cable_slot = slot;
	storeProperties();
	emit propertiesChange();
}

/**
	@return true si le conducteur est mis en evidence
*/
Conductor::Highlight Conductor::highlight() const
{
	return(must_highlight_);
}

/**
	@param hl true pour mettre le conducteur en evidence, false sinon
*/
void Conductor::setHighlighted(Conductor::Highlight hl) {
	must_highlight_ = hl;
	update();
}

/**
	@brief Conductor::displayedTextChanged
	Update the properties (text) of this conductor and other conductors
	at the same potential of this conductor.
*/
void Conductor::displayedTextChanged()
{
	QVariant old_value, new_value;
	old_value.setValue(m_properties);
	ConductorProperties new_properties(m_properties);
	new_properties.m_formula = m_text_item->toPlainText();
	new_properties.text = m_text_item->toPlainText();
	new_value.setValue(new_properties);


		//One gesture, however many conductors the potential holds: the
		//children below each write a row, and the grouped command turns that
		//into one notice to the data base instead of one per row.
	QUndoCommand *undo = new GroupedUpdateCommand(
			diagram() ? diagram()->project() : nullptr,
			tr("Edit conductor properties", "undo caption"));
	new QPropertyUndoCommand(this, "properties", old_value, new_value, undo);

	if (!relatedPotentialConductors().isEmpty())
	{
		undo->setText(tr("Edit the properties of several conductors", "undo caption"));

		foreach (Conductor *potential_conductor, relatedPotentialConductors())
		{
				//A wire left out of the numbering keeps its own text
			if (potential_conductor->isFreezeLabel()) {
				continue;
			}
			old_value.setValue(potential_conductor->properties());
			ConductorProperties new_properties = potential_conductor->properties();
			new_properties.m_formula = m_text_item->toPlainText();
			new_properties.text = m_text_item->toPlainText();
			new_value.setValue(new_properties);
			new QPropertyUndoCommand (potential_conductor, "properties", old_value, new_value, undo);
		}
	}

	diagram()->undoStack().push(undo);
}


/**
	@brief Conductor::relatedPotentialConductors
	Return all conductors at the same potential of this conductor,
	this conductor isn't part of the returned QSet.
	@param all_diagram : if true search in all diagram of the project,
	false search only in the parent diagram of this conductor
	@param t_list : a list of terminal already found for this potential.
	@param ignore_function_separation : if true, a terminal element whose
	function separation is enabled does not stop the search, and the
	conductors of both its sides are returned as one potential. Only
	functionSeparatedConductors() passes true.
	@sa functionSeparatedConductors
	@return  a QSet of conductor at the same potential.
*/
QSet<Conductor *> Conductor::relatedPotentialConductors(const bool all_diagram,
							QList <Terminal *> *t_list,
							const bool ignore_function_separation)
{
	bool declar_t_list = false;
	if (t_list == nullptr)
	{
		declar_t_list = true;
		t_list = new QList <Terminal *>;
	}

	QSet <Conductor *> other_conductors;
	QList <Terminal *> this_terminal;
	this_terminal << terminal1 << terminal2;

		// Return all conductors of terminal 1 and 2
	for (Terminal *terminal : this_terminal)
	{
		if (!t_list->contains(terminal))
		{
			t_list->append(terminal);
			QList <Conductor *> other_conductors_list_t = terminal->conductors();

				//Get the other terminals of the parent element of @terminal, who share the same potential
				//This is use for element type "folio report" and "terminal element"
			for (Terminal *t : relatedPotentialTerminal(terminal, all_diagram,
								ignore_function_separation))
			{
				if (!t_list->contains(t))
				{
					t_list -> append(t);
					other_conductors_list_t += t->conductors();
				}
			}

			other_conductors_list_t.removeAll(this);
				//Get the conductors at the same potential for each conductors of other_conductors_list_t
			for (Conductor *c : other_conductors_list_t) {
				other_conductors += c->relatedPotentialConductors(all_diagram, t_list,
								  ignore_function_separation);
			}

			other_conductors += QSet<Conductor*>(other_conductors_list_t.begin(),other_conductors_list_t.end());
		}
	}

	other_conductors.remove(this);

	if (declar_t_list) delete t_list;
	return(other_conductors);
}

/**
	@brief Conductor::functionSeparatedConductors
	Return the conductors that belong to this conductor's potential only
	by crossing a function separated terminal element -- a terminal
	element whose function separation checkbox (element information
	potential_isolating) is enabled, the checkbox the user ticks to stop
	sharing a terminal's properties with the other side of the terminal.

	Those conductors are deliberately not part of
	relatedPotentialConductors(): no property may be copied over them
	by default. One does cross that separation, though: the voltage /
	protocol, which describes the electrical node itself. Callers that
	push it over the whole potential use this method to reach the other
	side (@see ConductorProperties::applyTensionProtocolChange).

	It is simply the potential computed with the separations ignored,
	minus the potential that respects them.
	@param all_diagram : if true search in all diagram of the project,
	false search only in the parent diagram of this conductor
	@return a QSet of conductor, this conductor excluded
*/
QSet<Conductor *> Conductor::functionSeparatedConductors(const bool all_diagram)
{
	QSet<Conductor *> separated = relatedPotentialConductors(all_diagram, nullptr, true);
	separated.subtract(relatedPotentialConductors(all_diagram));
	separated.remove(this);

	return separated;
}

/**
	@brief Conductor::diagramEditor
	@return The parent diagram editor or nullptr;
*/
QETDiagramEditor* Conductor::diagramEditor() const
{
	if (!diagram())                     return nullptr;
	if (diagram() -> views().isEmpty()) return nullptr;

	QWidget *w = const_cast<QGraphicsView *>(diagram() -> views().at(0));
	while (w -> parentWidget() && !w -> isWindow()) {
		w = w -> parentWidget();
	}
	return(qobject_cast<QETDiagramEditor *>(w));
}

/**
	@brief Conductor::editProperty
*/
void Conductor::editProperty()
{
	ConductorPropertiesDialog::PropertiesDialog(this, diagramEditor());
}

void Conductor::setSequenceNum(const autonum::sequentialNumbers& sn)
{
	m_autoNum_seq = sn;
	refreshText();
}

/**
	@brief Conductor::setUpConnectionForFormula
	setup connection according to the variable of formula
	@param old_formula
	@param new_formula
*/
void Conductor::setUpConnectionForFormula(QString old_formula, QString new_formula)
{
	Diagram *diagram_ {diagram()};
	if (!diagram_) {
		diagram_ = terminal1->diagram();
	}
	if (diagram_)
	{
			//Because the variable %F is a reference to another text which can contain variables,
			//we must replace %F by the real text, to check if the real text contains the variable %id
		if (old_formula.contains("%F"))
			old_formula.replace("%F", diagram_->border_and_titleblock.folio());

		if (old_formula.contains("%id"))
			disconnect(diagram_->project(), &QETProject::projectDiagramsOrderChanged, this, &Conductor::refreshText);

			//Label is frozen, so we don't update it.
		if (m_freeze_label == true)
			return;

		if (new_formula.contains("%F"))
			new_formula.replace("%F", diagram_->border_and_titleblock.folio());

		if (new_formula.contains("%id"))
			connect(diagram_->project(), &QETProject::projectDiagramsOrderChanged, this, &Conductor::refreshText);
	}
}

/**
	@param a point
	@param b point
	@param c point
	@return true si le point a est contenu dans le rectangle delimite par les points b et c
*/
bool isContained(const QPointF &a, const QPointF &b, const QPointF &c) {
	return(
		isBetween(a.x(), b.x(), c.x()) &&
		isBetween(a.y(), b.y(), c.y())
	);
}

/**
	@brief Conductor::paintedPath
	@return the path drawn for this conductor: path(), with a hop at each
	place it crosses another conductor when the project draws hops
	(issue #436, QETProject::wireHops()). Cached until a conductor changes
	shape, moves, or enters or leaves a folio, so a repaint costs nothing.
	A moved conductor still repaints the hops of the others: a crossing
	lies inside the bounding rect of both conductors, which Qt repaints
	when either moves.
*/
QPainterPath Conductor::paintedPath() const
{
	const Diagram *parent_diagram = diagram();
	if (!parent_diagram || !parent_diagram->project()) {
		return path();
	}
	const WireHops::Mode mode = parent_diagram->project()->wireHops();
	if (mode == WireHops::Mode::None) {
		return path();
	}
	if (m_hops_generation == s_conductor_geometry_generation
		&& m_hops_mode == int(mode)) {
		return m_hops_path;
	}

	if (s_hop_candidates_diagram != parent_diagram
		|| s_hop_candidates_generation != s_conductor_geometry_generation)
	{
		s_hop_candidates.clear();
		const QList<Conductor *> conductors = parent_diagram->conductors();
		for (const Conductor *conductor : conductors)
		{
			HopCandidate candidate;
			candidate.conductor = conductor;
			candidate.rect = conductor->mapRectToScene(conductor->path().boundingRect());
			for (const QPointF &point : conductor->segmentsToPoints()) {
				candidate.points.append(conductor->mapToScene(point));
			}
			s_hop_candidates.append(candidate);
		}
		s_hop_candidates_diagram = parent_diagram;
		s_hop_candidates_generation = s_conductor_geometry_generation;
	}

	m_hops_path = path();
	m_hops_generation = s_conductor_geometry_generation;
	m_hops_mode = int(mode);

	const QList<QPointF> points = segmentsToPoints();
	if (points.size() < 2) {
		return m_hops_path;
	}
	QVector<QPointF> wire;
	for (const QPointF &point : points) {
		wire.append(mapToScene(point));
	}

		//A conductor crossing this one has path bounds overlapping its own;
		//adjusted by 1 because a straight conductor's bounds have no width
	const QRectF rect = mapRectToScene(path().boundingRect()).adjusted(-1, -1, 1, 1);
	QList<QVector<QPointF>> others;
	for (const HopCandidate &candidate : std::as_const(s_hop_candidates))
	{
		if (candidate.conductor != this
			&& rect.intersects(candidate.rect.adjusted(-1, -1, 1, 1))) {
			others.append(candidate.points);
		}
	}

	const QList<QPointF> scene_hops = WireHops::crossings(wire, others, mode);
	if (!scene_hops.isEmpty())
	{
		QList<QPointF> hops;
		for (const QPointF &hop : scene_hops) {
			hops.append(mapFromScene(hop));
		}
		m_hops_path = WireHops::path(QVector<QPointF>(points.begin(), points.end()), hops, mode);
	}
	return m_hops_path;
}

/**
	@return true if \a scene_point, where this conductor bends with
	\a bend_type, lies on one of the segments of \a c and \a c does not
	bend the same way at the same point.
*/
bool Conductor::bendMakesJunction(const Conductor *c, const QPointF &scene_point,
								  Qt::Corner bend_type) const
{
		// exprime le point dans les coordonnees de l'autre conducteur
	const QPointF conductor_point = c -> mapFromScene(scene_point);
	bool on_conductor = false;
	for (ConductorSegment *segment : c -> segmentsList())
	{
		if (isContained(conductor_point, segment -> firstPoint(), segment -> secondPoint()))
		{
			on_conductor = true;
			break;
		}
	}
	if (!on_conductor)
		return false;

		// ce point commun ne doit pas etre une bifurcation identique a celle-ci
	for (const ConductorBend &cb : c -> bends())
	{
		if (cb.first == conductor_point && cb.second == bend_type)
			return false;
	}
	return true;
}

/**
	@return the positions, in this conductor's coordinates, of the junction
	dots this conductor draws: each of its bends that lies on another
	conductor of the same potential, unless that conductor bends the same
	way at the same point.
*/
QList<QPointF> Conductor::junctions() const
{
	QList<QPointF> junctions_list;

	QList<ConductorBend> bends_list = bends();
	if (bends_list.isEmpty()) {
		return(junctions_list);
	}

		// Every conductor of the potential on this folio, not only those on
		// this conductor's own terminals: a bend can lie on a conductor it
		// shares no terminal with, e.g. when the horizontal parts of a chain
		// of conductors are dragged over each other (issue #1280).
	const QList<Conductor *> other_conductors =
			const_cast<Conductor *>(this)->relatedPotentialConductors(false).values();
	if (other_conductors.isEmpty()) {
		return(junctions_list);
	}

	QList<QPointF> points = segmentsToPoints();
	for (int i = 1 ; i < (points.size() -1) ; ++ i) {
		QPointF point = points.at(i);

		// determine si le point est une bifurcation ou non
		bool is_bend = false;
		Qt::Corner current_bend_type = Qt::TopLeftCorner;
		foreach(ConductorBend cb, bends_list)
		{
			if (cb.first == point)
			{
				is_bend = true;
				current_bend_type = cb.second;
				break;
			}
		}
		// si le point n'est pas une bifurcation, il ne peut etre une jonction (enfin pas au niveau de ce conducteur)
		if (!is_bend) continue;

		QPointF scene_point = mapToScene(point);

		bool is_junction = false;
		for (Conductor *c : other_conductors)
		{
			if (bendMakesJunction(c, scene_point, current_bend_type))
			{
				is_junction = true;
				break;
			}
		}
		if (is_junction) junctions_list << point;
	}
	return(junctions_list);
}

/**
	@brief Conductor::sharedTerminal
	@return the terminal @a a and @a b both end at - the first one of @a a
	when they share two -, or nullptr
*/
Terminal *Conductor::sharedTerminal(const Conductor *a, const Conductor *b)
{
	if (!a || !b) {
		return nullptr;
	}
	for (Terminal *t : {a->terminal1, a->terminal2}) {
		if (t && (t == b->terminal1 || t == b->terminal2)) return t;
	}
	return nullptr;
}

Terminal *Conductor::nearerEndTo(const QPointF &scene_point) const
{
	const QList<QPointF> points = scenePathPoints();
	if (points.size() < 2) {
		return terminal1;
	}
	int segment = 0;
	const QPointF on = nearestOnPath(points, scene_point, &segment);
	qreal from_first = 0;
	for (int i = 0 ; i < segment ; ++i) {
		from_first += QLineF(points.at(i), points.at(i + 1)).length();
	}
	from_first += QLineF(points.at(segment), on).length();
	qreal total = 0;
	for (int i = 0 ; i + 1 < points.size() ; ++i) {
		total += QLineF(points.at(i), points.at(i + 1)).length();
	}
	return from_first <= total - from_first ? terminal1 : terminal2;
}

QList<QPointF> Conductor::scenePathPoints() const
{
	QList<QPointF> points;
	const QList<QPointF> local = segmentsToPoints();
	for (const QPointF &point : local) {
		points << mapToScene(point);
	}
	return points;
}

/**
	@brief Conductor::nearestOnPath
	@return the point of the path @a points nearest to @a point, and in
	@a segment the index of the segment it lies on, from 0
*/
QPointF Conductor::nearestOnPath(const QList<QPointF> &points, const QPointF &point, int *segment)
{
	QPointF nearest = points.isEmpty() ? point : points.first();
	qreal distance = std::numeric_limits<qreal>::max();
	int index = 0;
	for (int i = 0 ; i + 1 < points.size() ; ++i)
	{
		const QPointF a = points.at(i);
		const QPointF ab = points.at(i + 1) - a;
		const qreal length = QPointF::dotProduct(ab, ab);
		const qreal t = length > 0 ? std::clamp(QPointF::dotProduct(point - a, ab) / length, 0.0, 1.0) : 0.0;
		const QPointF on = a + ab * t;
		const qreal d = QLineF(on, point).length();
		if (d < distance)
		{
			distance = d;
			nearest = on;
			index = i;
		}
	}
	if (segment) *segment = index;
	return nearest;
}

/**
	@brief Conductor::arm
	@return the wire this one branches off (T86), when both are on the same
	folio: an arm deleted, which an undo may bring back, leaves its branches
	wires of their own until it is back
*/
Conductor *Conductor::arm() const
{
	if (!m_arm || !scene() || m_arm->scene() != scene()) {
		return nullptr;
	}
	return m_arm;
}

/**
	@brief Conductor::canBranchOff
	@return true when this wire may branch off @a arm: the two end at one
	terminal, the one the branch then shares the path of the arm from, and
	the arm does not branch off this wire, directly or through another
*/
bool Conductor::canBranchOff(const Conductor *arm) const
{
	if (!arm || arm == this || !sharedTerminal(this, arm)) {
		return false;
	}
	int depth = 0;
	for (const Conductor *c = arm ; c ; c = c->m_arm)
	{
		if (c == this || ++depth > 1000) return false;
	}
	return true;
}

/**
	@brief Conductor::setArm
	Make this wire a branch of @a arm, off the point of it nearest to
	@a scene_point: its path becomes the path of the arm from the terminal
	they share to that point, and its own piece from there (T86). What the
	wire connects does not change: in the panel, the wires of a point are
	joined at the terminal, and the branch is a way of drawing them.
	@return false, changing nothing, when it cannot (canBranchOff())
*/
bool Conductor::setArm(Conductor *arm, const QPointF &scene_point)
{
	if (!canBranchOff(arm)) {
		return false;
	}
	if (m_arm && m_arm != arm) {
		m_arm->m_branches.removeAll(this);
	}
	m_arm = arm;
	m_arm_point = scene_point;
	m_pending_arm = QUuid();
	if (!arm->m_branches.contains(this)) {
		arm->m_branches << this;
	}
	followArm();
	return true;
}

/**
	@brief Conductor::clearArm
	Make this branch a wire of its own again, with the path it has
*/
void Conductor::clearArm()
{
	if (m_arm) {
		m_arm->m_branches.removeAll(this);
	}
	m_arm = nullptr;
	m_pending_arm = QUuid();
	m_own_from = -1;
	if (segments) {
		saveProfile(false);
	}
	prepareGeometryChange();
	update();
}

QList<Conductor *> Conductor::branches() const
{
	QList<Conductor *> list;
	for (const QPointer<Conductor> &branch : m_branches) {
		if (branch && branch->m_arm == this) list << branch;
	}
	return list;
}

/**
	@brief Conductor::branchPathPoints
	@return the path of this branch in scene coordinates, from terminal1
	to terminal2: the path of the arm from the terminal they share to the
	point, then the own piece, leaving the arm across it towards the other
	terminal, as an automatic path leaves a terminal. In @a own_from, the
	index of the point on the arm. Empty when this is no branch.
*/
QList<QPointF> Conductor::branchPathPoints(int *own_from) const
{
	const Conductor *a = arm();
	Terminal *shared = sharedTerminal(this, a);
	if (!a || !shared) {
		return {};
	}
	QList<QPointF> along = a->scenePathPoints();
	if (along.size() < 2) {
		return {};
	}
	if (shared != a->terminal1) {
		std::reverse(along.begin(), along.end());
	}
	Terminal *own = terminal1 == shared ? terminal2 : terminal1;
	const QPointF end = own->dockConductor();

		//Where it comes off the arm: where the person left it when they
		//shaped the piece by hand; else straight out of its terminal, onto
		//the arm, when the terminal faces it (T86, 23/10/2026: "the wire
		//leaves the terminal straight to the other wire"); else the point
		//of the arm nearest to where it was dropped
	int segment = 0;
	QPointF at;
	const bool shaped = m_own_points.size() >= 2;
	if (shaped || !straightOnto(along, end, own->orientation(), &at, &segment)) {
		at = nearestOnPath(along, m_arm_point, &segment);
	}

	QList<QPointF> points;
	for (int i = 0 ; i <= segment ; ++i)
	{
		if (points.isEmpty() || (along.at(i) - points.last()).manhattanLength() > 0.01) {
			points << along.at(i);
		}
	}
	if ((at - points.last()).manhattanLength() > 0.01) {
		points << at;
	}
	const int on_arm = int(points.size()) - 1;

	QList<QPointF> piece;
	if (shaped) {
		piece = keptShape(m_own_points, at, end);
	}
	if (piece.isEmpty())
	{
		const QPointF from = along.at(segment);
		const QPointF to = along.at(segment + 1);
		Qet::Orientation leave;
		if (qAbs(from.x() - to.x()) < 0.01) {
			leave = end.x() >= at.x() ? Qet::East : Qet::West;
		} else {
			leave = end.y() >= at.y() ? Qet::South : Qet::North;
		}
		const QList<QPointF> automatic = automaticPath(mapFromScene(at), leave, mapFromScene(end), own->orientation());
		for (const QPointF &point : automatic) {
			piece << mapToScene(point);
		}
	}
	for (int i = 1 ; i < piece.size() ; ++i)
	{
		const QPointF point = piece.at(i);
		if ((point - points.last()).manhattanLength() > 0.01) {
			points << point;
		}
	}

	int index = on_arm;
	if (terminal1 != shared)
	{
		std::reverse(points.begin(), points.end());
		index = int(points.size()) - 1 - on_arm;
	}
	if (own_from) *own_from = index;
	return points;
}

/**
	@brief Conductor::straightOnto
	Where a wire leaving @a end in the direction @a orientation first meets
	@a along, a path that turns at right angles: in @a at, with the segment
	of @a along in @a segment.
	@return false when it never meets it - the terminal does not face it
*/
bool Conductor::straightOnto(const QList<QPointF> &along, const QPointF &end,
			     Qet::Orientation orientation, QPointF *at, int *segment)
{
	QPointF direction;
	switch (orientation)
	{
		case Qet::North: direction = QPointF(0, -1); break;
		case Qet::South: direction = QPointF(0, 1); break;
		case Qet::East:  direction = QPointF(1, 0); break;
		default:         direction = QPointF(-1, 0); break;
	}
	const bool across_x = qFuzzyIsNull(direction.y());
	qreal nearest = std::numeric_limits<qreal>::max();
	bool found = false;
	for (int i = 0 ; i + 1 < along.size() ; ++i)
	{
		const QPointF a = along.at(i);
		const QPointF b = along.at(i + 1);
		QPointF meet;
		if (across_x)
		{
				//A wire across x meets a segment that runs along y
			if (qAbs(a.x() - b.x()) > 0.01
			    || end.y() < std::min(a.y(), b.y()) - 0.01
			    || end.y() > std::max(a.y(), b.y()) + 0.01) continue;
			meet = QPointF(a.x(), end.y());
		}
		else
		{
			if (qAbs(a.y() - b.y()) > 0.01
			    || end.x() < std::min(a.x(), b.x()) - 0.01
			    || end.x() > std::max(a.x(), b.x()) + 0.01) continue;
			meet = QPointF(end.x(), a.y());
		}
		const qreal ahead = QPointF::dotProduct(meet - end, direction);
		if (ahead > 0.01 && ahead < nearest)
		{
			nearest = ahead;
			if (at) *at = meet;
			if (segment) *segment = i;
			found = true;
		}
	}
	return found;
}

/**
	@brief Conductor::keptShape
	@return the own piece @a points the person shaped, from the arm to the
	terminal, its ends brought to where the arm @a at and the terminal
	@a end are now, the segments next to them kept upright - as a wire
	follows a component that moves; empty when there is nothing to keep
*/
QList<QPointF> Conductor::keptShape(QList<QPointF> points, const QPointF &at, const QPointF &end)
{
	const auto upright = [](const QPointF &a, const QPointF &b) {
		return qAbs(a.x() - b.x()) < 0.01;
	};
	const int n = int(points.size());
	if (n < 2) {
		return {};
	}
	if (n >= 4)
	{
		if (upright(points.at(0), points.at(1))) points[1].setX(at.x()); else points[1].setY(at.y());
		if (upright(points.at(n - 2), points.at(n - 1))) points[n - 2].setX(end.x()); else points[n - 2].setY(end.y());
	}
	else if (n == 3)
	{
		points[1] = upright(points.at(0), points.at(1)) ? QPointF(at.x(), end.y()) : QPointF(end.x(), at.y());
	}
	else if (qAbs(at.x() - end.x()) > 0.01 && qAbs(at.y() - end.y()) > 0.01)
	{
			//A straight piece that no longer lines up turns once
		points.insert(1, upright(points.at(0), points.at(1)) ? QPointF(at.x(), end.y()) : QPointF(end.x(), at.y()));
	}
	points.first() = at;
	points.last() = end;
	return points;
}

/**
	@brief Conductor::ownPiecePoints
	See the header. Read off the path, from the terminal back to the first
	point that lies on the arm: the person may have dragged the point along
	the arm, and the index of the point counted from the other end too
*/
QList<QPointF> Conductor::ownPiecePoints() const
{
	const Conductor *a = arm();
	Terminal *shared = sharedTerminal(this, a);
	if (!a || !shared) {
		return {};
	}
	QList<QPointF> points = scenePathPoints();
	if (shared == terminal1) {
		std::reverse(points.begin(), points.end());
	}
	const QList<QPointF> along = a->scenePathPoints();
	QList<QPointF> own;
	for (int i = 0 ; i < points.size() ; ++i)
	{
		own.prepend(points.at(i));
		if (i > 0 && QLineF(nearestOnPath(along, points.at(i)), points.at(i)).length() < 0.01) {
			break;
		}
	}
	return own;
}

/**
	@brief Conductor::isOwnSegment
	See the header. Segment @a index runs from point @a index to the next
*/
bool Conductor::isOwnSegment(int index) const
{
	if (!arm() || m_own_from < 0) {
		return false;
	}
	return sharedTerminal(this, m_arm) == terminal1 ? index >= m_own_from : index < m_own_from;
}

/**
	@brief Conductor::setBranchShape
	This branch off its arm at @a arm_point, its own piece @a own_points -
	in scene coordinates, from the arm to its terminal; none for the
	automatic one, straight out of the terminal (T86)
*/
void Conductor::setBranchShape(const QPointF &arm_point, const QList<QPointF> &own_points)
{
	m_arm_point = arm_point;
	m_own_points = own_points.size() >= 2 ? own_points : QList<QPointF>();
	if (arm()) {
		followArm();
	}
}

/**
	@brief Conductor::followArm
	Give this branch the path its arm gives it now (T86)
*/
void Conductor::followArm()
{
	int own_from = -1;
	const QList<QPointF> points = branchPathPoints(&own_from);
	if (points.size() < 2) {
		return;
	}
	QList<QPointF> local;
	for (const QPointF &point : points) {
		local << mapFromScene(point);
	}
	prepareGeometryChange();
	m_own_from = own_from;
		//The path is the arm's and its own: saved with the wire, for a
		//program that does not know branches to draw it the same
	modified_path = true;
	pointsToSegments(local);
	segmentsToPath();
	calculateTextItemPosition();
	update();
}

/**
	@brief Conductor::ownPiecePath
	@return the piece of this branch that is its own, from the point on the
	arm to its other terminal, in its own coordinates: what is drawn and
	what the mouse catches. The rest is the arm's
*/
QPainterPath Conductor::ownPiecePath() const
{
	const QList<QPointF> points = segmentsToPoints();
	if (!arm() || m_own_from < 0 || m_own_from >= points.size() || !terminal1) {
		return path();
	}
	const bool first_is_shared = sharedTerminal(this, m_arm) == terminal1;
	const int from = first_is_shared ? m_own_from : 0;
	const int to = first_is_shared ? int(points.size()) - 1 : m_own_from;
	QPainterPath piece;
	if (to <= from) {
		return piece;
	}
	piece.moveTo(points.at(from));
	for (int i = from + 1 ; i <= to ; ++i) {
		piece.lineTo(points.at(i));
	}
	return piece;
}

/**
	@brief Conductor::ownPiecePaintedPath
	@return the own piece of this branch as it is drawn: with a hop where a
	wire crosses it, as any wire has (T86, 23/10/2026: the wires crossing a
	branch crossed it with no hop)
*/
QPainterPath Conductor::ownPiecePaintedPath() const
{
	const QPainterPath plain = ownPiecePath();
	const Diagram *parent_diagram = diagram();
	if (!parent_diagram || !parent_diagram->project() || plain.elementCount() < 2) {
		return plain;
	}
	const WireHops::Mode mode = parent_diagram->project()->wireHops();
	if (mode == WireHops::Mode::None) {
		return plain;
	}
	QVector<QPointF> local;
	QVector<QPointF> wire;
	for (int i = 0 ; i < plain.elementCount() ; ++i)
	{
		const QPointF point(plain.elementAt(i).x, plain.elementAt(i).y);
		local << point;
		wire << mapToScene(point);
	}
	const QRectF rect = mapRectToScene(plain.boundingRect()).adjusted(-1, -1, 1, 1);
	QList<QVector<QPointF>> others;
	const QList<Conductor *> conductors = parent_diagram->conductors();
	for (const Conductor *conductor : conductors)
	{
		if (conductor == this
		    || !rect.intersects(conductor->mapRectToScene(conductor->path().boundingRect())
					.adjusted(-1, -1, 1, 1))) {
			continue;
		}
		QVector<QPointF> points;
		for (const QPointF &point : conductor->segmentsToPoints()) {
			points << conductor->mapToScene(point);
		}
		others << points;
	}
	const QList<QPointF> scene_hops = WireHops::crossings(wire, others, mode);
	if (scene_hops.isEmpty()) {
		return plain;
	}
	QList<QPointF> hops;
	for (const QPointF &hop : scene_hops) {
		hops << mapFromScene(hop);
	}
	return WireHops::path(local, hops, mode);
}

/**
	@brief Conductor::linkArms
	Link the branches read from a file to their arms, once all the wires of
	the folio are there: an arm may come after its branch in the file
*/
void Conductor::linkArms(const QList<Conductor *> &conductors)
{
	QHash<QUuid, Conductor *> by_uuid;
	for (Conductor *c : conductors) {
		by_uuid.insert(c->uuid(), c);
	}
	for (Conductor *c : conductors)
	{
		if (c->m_pending_arm.isNull()) continue;
		Conductor *a = by_uuid.value(c->m_pending_arm);
		if (!a || !c->setArm(a, c->m_arm_point)) {
			c->m_pending_arm = QUuid();
		}
	}
}

/**
	@return la liste des bifurcations de ce conducteur ; ConductorBend est un
	typedef pour une QPair\<QPointF, Qt::Corner\>. Le point indique la position
	(en coordonnees locales) de la bifurcation tandis que le Corner indique le
	type de bifurcation.
*/
QList<ConductorBend> Conductor::bends() const
{
	QList<ConductorBend> points;
	if (!segments) return(points);

	// recupere la liste des segments de taille non nulle
	QList<ConductorSegment *> visible_segments;
	ConductorSegment *segment = segments;
	while (segment -> hasNextSegment()) {
		if (!segment -> isPoint()) visible_segments << segment;
		segment = segment -> nextSegment();
	}
	if (!segment  -> isPoint()) visible_segments << segment;

	ConductorSegment *next_segment;
	for (int i = 0 ; i < visible_segments.count() -1 ; ++ i) {
		segment = visible_segments[i];
		next_segment = visible_segments[i + 1];
		if (!segment -> isPoint() && !next_segment -> isPoint()) {
			// si les deux segments ne sont pas dans le meme sens, on a une bifurcation
			if (next_segment -> type() != segment -> type()) {
				Qt::Corner bend_type;
				qreal sl = segment -> length();
				qreal nsl = next_segment -> length();

				if (segment -> isHorizontal()) {
					if (sl < 0 && nsl < 0) {
						bend_type = Qt::BottomLeftCorner;
					} else if (sl < 0 && nsl > 0) {
						bend_type = Qt::TopLeftCorner;
					} else if (sl > 0 && nsl < 0) {
						bend_type = Qt::BottomRightCorner;
					} else {
						bend_type = Qt::TopRightCorner;
					}
				} else {
					if (sl < 0 && nsl < 0) {
						bend_type = Qt::TopRightCorner;
					} else if (sl < 0 && nsl > 0) {
						bend_type = Qt::TopLeftCorner;
					} else if (sl > 0 && nsl < 0) {
						bend_type = Qt::BottomRightCorner;
					} else {
						bend_type = Qt::BottomLeftCorner;
					}
				}
				points << qMakePair(segment -> secondPoint(), bend_type);
			}
		}
	}
	return(points);
}

/**
	@param start Point de depart
	@param end Point d'arrivee
	@return le coin vers lequel se dirige le trajet de start vers end
*/
Qt::Corner Conductor::movementType(const QPointF &start, const QPointF &end) {
	Qt::Corner result = Qt::BottomRightCorner;
	if (start.x() <= end.x()) {
		result = start.y() <= end.y() ? Qt::BottomRightCorner : Qt::TopRightCorner;
	} else {
		result = start.y() <= end.y() ? Qt::BottomLeftCorner : Qt::TopLeftCorner;
	}
	return(result);
}

/// @return le type de trajet actuel de ce conducteur
Qt::Corner Conductor::currentPathType() const
{
	return(movementType(terminal1 -> dockConductor(), terminal2 -> dockConductor()));
}

/// @return les profils de ce conducteur
ConductorProfilesGroup Conductor::profiles() const
{
	return(conductor_profiles);
}

/**
	@brief Conductor::setProfiles
	@param cpg : the new profils of conductor
*/
void Conductor::setProfiles(const ConductorProfilesGroup &cpg) {
	conductor_profiles = cpg;
	if (conductor_profiles[currentPathType()].isNull()) {
		generateConductorPath(terminal1 -> dockConductor(), terminal1 -> orientation(), terminal2 -> dockConductor(), terminal2 -> orientation());
		modified_path = false;
	} else {
		updateConductorPath(terminal1 -> dockConductor(), terminal1 -> orientation(), terminal2 -> dockConductor(), terminal2 -> orientation());
		modified_path = true;
	}
	if (properties().type == ConductorProperties::Multi) {
		calculateTextItemPosition();
	}
}

/// Supprime les segments
void Conductor::deleteSegments()
{
	if (segments != nullptr) {
		while (segments -> hasNextSegment()) delete segments -> nextSegment();
		delete segments;
		segments = nullptr;
	}
}

/**
 * @brief Conductor::movePointIntoPolygon
 * @param point : A point located outside the polygon
 * @param polygon : The polygon in which we want to move the point
 * @return the position of the point, once brought back into the polygon,
 * or more exactly on the edge of the polygon
 */
QPointF Conductor::movePointIntoPolygon(const QPointF &point, const QPainterPath &polygon)
{
		// decomposes the polygon into lines and points
	const QList<QPolygonF> polygons = polygon.simplified().toSubpathPolygons();
	QList<QLineF> lines;
	QList<QPointF> points;

	for (QPolygonF polygon : polygons)
	{
		if (polygon.count() <= 1)
			continue;

			// lines and points are counted
		for (int i = 1 ; i < polygon.count() ; ++ i) {
			lines << QLineF(polygon.at(i - 1), polygon.at(i));
			points << polygon.at(i -1);
		}
	}

		// we make orthogonal projections of the point on the different
		// segments of the polygon, sorting them by increasing length
	QMap<qreal, QPointF> intersections;
	for (QLineF line : lines)
	{
		QPointF intersection_point;
		if (QET::orthogonalProjection(point, line, &intersection_point)) {
			intersections.insert(QLineF(intersection_point, point).length(), intersection_point);
		}
	}

	if (intersections.count())
	{
			// the shortest length for an orthogonal project is determined
		QPointF the_point = intersections[intersections.keys().first()];
		return(the_point);
	}
	else
	{
			// determines the corner of the polygon closest to the outer point
		qreal minimum_length = -1;
		int point_index = -1;
		for (int i = 0 ; i < points.count() ; ++ i)
		{
			qreal length = qAbs(QLineF(points.at(i), point).length());
			if (minimum_length < 0 || length < minimum_length) {
				minimum_length = length;
				point_index    = i;
			}
		}
			// we now know the closest corner of the text


		if (point_index == -1 ||
			point_index+1 > points.size()) {
			return QPointF(0,0);
		}

			// no orthogonal projection gave anything, we put the text on one of the corners of the polygon
		return(points.at(point_index));
	}
}

namespace {
	/// The two ends of @p conductor on the folio, smaller first, compared
	/// by x then y.
	QPair<QPointF, QPointF> conductorEnds(const Conductor *conductor)
	{
		auto less = [](const QPointF &a, const QPointF &b) {
			return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
		};
		QPointF a = conductor->terminal1 ? conductor->terminal1->dockConductor() : QPointF();
		QPointF b = conductor->terminal2 ? conductor->terminal2->dockConductor() : QPointF();
		if (less(b, a))
			std::swap(a, b);
		return qMakePair(a, b);
	}

	/// True if @p a comes before @p b in an order that is the same in every
	/// run: by where the conductor's ends are on the folio, then by uuid.
	/// Not by uuid first: a project generated again by a script gets new
	/// uuids each time, and a file with no conductor uuids gets new ones on
	/// every load, while the drawing is the same.
	bool stableConductorLess(const Conductor *a, const Conductor *b)
	{
		const auto ea = conductorEnds(a), eb = conductorEnds(b);
		const qreal ka[4] = {ea.first.x(), ea.first.y(), ea.second.x(), ea.second.y()};
		const qreal kb[4] = {eb.first.x(), eb.first.y(), eb.second.x(), eb.second.y()};
		for (int i = 0; i < 4; ++i)
			if (ka[i] != kb[i])
				return ka[i] < kb[i];
		return a->uuid() < b->uuid();
	}
}

/**
	@brief longestConductorInPotential
	@param conductor : a conductor in the potential to search
	@param all_diagram : true -> search in the whole project, false -> search only in the diagram of conductor
	@return the longest conductor in the same potential of conductor
*/
Conductor * longestConductorInPotential(Conductor *conductor, bool all_diagram) {
	Conductor *longest_conductor = conductor;
	qreal longest_length = conductor->length();
	//Search the longest conductor.
	//Conductors of equal length are common (a symmetrical layout), and the
	//set is iterated in pointer order, which changes from run to run: so a
	//tie is broken by an order that does not, or the potential's text lands
	//on a different conductor each time the same file is opened or exported.
	foreach (Conductor *c, conductor -> relatedPotentialConductors(all_diagram))
	{
		const qreal length = c->length();
		if (qAbs(length - longest_length) < 1e-6) {
			if (stableConductorLess(c, longest_conductor))
				longest_conductor = c;
		}
		else if (length > longest_length) {
			longest_conductor = c;
			longest_length = length;
		}
	}

	return longest_conductor;
}

/**
	@brief relatedConductors
	@param conductor
	@return return all conductors who share the same terminals
	of conductor given as parameter,
	except conductor itself.
*/
QList <Conductor *> relatedConductors(const Conductor *conductor) {
	QList<Conductor *> other_conductors_list = conductor -> terminal1 -> conductors();
	other_conductors_list << conductor -> terminal2->conductors();
	other_conductors_list.removeAll(const_cast<Conductor *> (conductor));
	return(other_conductors_list);
}

/**
	@brief Conductor::setFreezeLabel
	Freeze this conductor label if true
	Unfreeze this conductor label if false
	@param freeze
*/
void Conductor::setFreezeLabel(bool freeze)
{
		//The comparison came after the assignment, and the body never ran
	if (m_freeze_label == freeze) {
		return;
	}
	m_freeze_label = freeze;
	const QString formula = m_properties.m_formula;
	setUpConnectionForFormula(formula, formula);
		//Back in the numbering: the name its formula gives
	if (!m_freeze_label) {
		refreshText();
	}
	emit freezeLabelChanged();
}
