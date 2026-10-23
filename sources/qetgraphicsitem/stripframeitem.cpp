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
#include "stripframeitem.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../cable/editcablecommand.h"
#include "../diagram.h"
#include "../diagramcontent.h"
#include "../elementsmover.h"
#include "../properties/elementdata.h"
#include "../qetinformation.h"
#include "../undocommand/changeelementinformationcommand.h"
#include "../undocommand/movegraphicsitemcommand.h"
#include "../undocommand/rotateselectioncommand.h"
#include "conductor.h"
#include "dynamicelementtextitem.h"
#include "element.h"
#include "terminal.h"

#include <QApplication>
#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QHash>
#include <QFontMetricsF>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>
#include <QInputDialog>
#include <QMenu>
#include <QPainter>
#include <QPainterPathStroker>
#include <QTextDocument>
#include <QSet>
#include <QSignalBlocker>
#include <QUndoCommand>
#include <QUndoStack>
#include <QtMath>

namespace
{
		///Kept round what the terminals draw
	constexpr qreal padding{2.0};
		///Kept short of the end of a terminal, where its wire comes
	constexpr qreal clearance{3.0};
		///Round the name
	constexpr qreal caption_padding{2.0};
		///How wide the band along the border that answers the mouse is
	constexpr qreal grab_band_width{6.0};

	QFont captionFont()
	{
		QFont font(QStringLiteral("Liberation Sans"));
		font.setStyleHint(QFont::SansSerif);
		font.setPointSizeF(7.0);
		return font;
	}

		/// The frames of each folio: a component coming onto a folio or
		/// leaving it asks them to be worked out again, and looking through
		/// every item of the folio for them would cost a project that opens
		/// the square of its components
	QHash<const QGraphicsScene *, QList<QPointer<StripFrameItem>>> &framesOn()
	{
		static QHash<const QGraphicsScene *, QList<QPointer<StripFrameItem>>> frames;
		return frames;
	}

		/// What a frame is at one time: its name, its terminals and whether
		/// it is on the folio
	struct FrameState
	{
		QString name;
		QList<QPointer<Element>> terminals;
		bool on_folio = true;
	};

	FrameState stateOf(const StripFrameItem *frame)
	{
		FrameState state;
		state.name = frame->stripName();
		const QList<Element *> terminals = frame->terminals();
		for (Element *terminal : terminals) {
			state.terminals << terminal;
		}
		state.on_folio = frame->scene() != nullptr;
		return state;
	}

		/// A frame from one state to another, and back with the undo
	class StripFrameChange : public QUndoCommand
	{
		public:
			StripFrameChange(StripFrameItem *frame, Diagram *diagram,
					 const FrameState &before, const FrameState &after,
					 QUndoCommand *parent) :
				QUndoCommand(parent),
				m_frame(frame),
				m_diagram(diagram),
				m_before(before),
				m_after(after)
			{
				m_diagram->qgiManager().manage(frame);
			}
			~StripFrameChange() override
			{
				if (m_diagram && m_frame) m_diagram->qgiManager().release(m_frame);
			}
			void undo() override {apply(m_before);}
			void redo() override {apply(m_after);}

		private:
			void apply(const FrameState &state)
			{
				if (!m_frame || !m_diagram) return;
				QList<Element *> terminals;
				for (const QPointer<Element> &terminal : state.terminals) {
					if (terminal) terminals << terminal;
				}
				m_frame->setStripName(state.name);
				m_frame->setTerminals(terminals);
				if (state.on_folio && m_frame->scene() != m_diagram) {
					m_diagram->addItem(m_frame);
				} else if (!state.on_folio && m_frame->scene() == m_diagram) {
					m_diagram->removeItem(m_frame);
				}
			}

			QPointer<StripFrameItem> m_frame;
			QPointer<Diagram> m_diagram;
			FrameState m_before;
			FrameState m_after;
	};

		/// The name of the strip written in the comment of @a terminals,
		/// where they say which strip they are of (T85), in @a undo
	void writeName(const QList<QPointer<Element>> &terminals, const QString &name, QUndoCommand *undo)
	{
		QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> changes;
		for (const QPointer<Element> &terminal : terminals)
		{
			if (!terminal) continue;
			const DiagramContext before = terminal->elementInformations();
			if (before.value(QETInformation::ELMT_COMMENT).toString() == name) continue;
			DiagramContext after = before;
			after.addValue(QETInformation::ELMT_COMMENT, name,
				       before.contains(QETInformation::ELMT_COMMENT)
				       && before.keyMustShow(QETInformation::ELMT_COMMENT));
			changes.insert(terminal, qMakePair(before, after));
		}
		if (!changes.isEmpty()) {
			new ChangeElementInformationCommand(changes, undo);
		}
	}

		/// @a delta turned by @a angle degrees, the way
		/// QGraphicsItem::setRotation() turns - clockwise on the folio -;
		/// exact for the right angles, which the Rotate command only gives,
		/// as RotateSelectionCommand::addGroupPositionUndo() does
	QPointF turned(const QPointF &delta, qreal angle)
	{
		const int quadrant = qRound(angle / 90.0);
		if (qFuzzyCompare(angle, quadrant * 90.0))
		{
			switch (((quadrant % 4) + 4) % 4)
			{
				case 1:  return QPointF(-delta.y(),  delta.x());
				case 2:  return QPointF(-delta.x(), -delta.y());
				case 3:  return QPointF( delta.y(), -delta.x());
				default: return delta;
			}
		}
		const qreal radians = qDegreesToRadians(angle);
		return QPointF(delta.x() * qCos(radians) - delta.y() * qSin(radians),
			       delta.x() * qSin(radians) + delta.y() * qCos(radians));
	}
}

/**
	@brief StripFrameItem::StripFrameItem
	@param strip_name the name of the strip
	@param parent parent item

	Under the components and their wires, and over the location areas: the
	frame is drawn round the terminals and is no part of them.
*/
StripFrameItem::StripFrameItem(const QString &strip_name, QGraphicsItem *parent) :
	QetGraphicsItem(parent),
	m_name(strip_name)
{
		//Selected by its border, never moved itself: dragging it moves its
		//terminals, and it follows them (mouseMoveEvent())
	setMovable(false);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setZValue(5);
}

QString StripFrameItem::name() const
{
	return tr("a drawn terminal strip");
}

void StripFrameItem::setStripName(const QString &strip_name)
{
	if (m_name == strip_name) {
		return;
	}
	prepareGeometryChange();
	m_name = strip_name;
	m_dirty = true;
	update();
	emit frameChanged();
}

/**
	@brief StripFrameItem::terminals
	@return the terminals of this frame that are on its folio: one deleted,
	which an undo may bring back, counts again when it is back
*/
QList<Element *> StripFrameItem::terminals() const
{
	QList<Element *> list;
	for (const QPointer<Element> &terminal : m_terminals)
	{
		if (terminal && (!scene() || terminal->scene() == scene())) list << terminal;
	}
	return list;
}

/**
	@brief StripFrameItem::setTerminals
	The terminals the frame is drawn round, which it follows from now on
*/
void StripFrameItem::setTerminals(const QList<Element *> &terminals)
{
	showComments();
	for (const QMetaObject::Connection &connection : std::as_const(m_connections)) {
		disconnect(connection);
	}
	m_connections.clear();
	m_terminals.clear();
	for (Element *terminal : terminals)
	{
		if (!terminal) continue;
		m_terminals << terminal;
		m_connections << connect(terminal, &QGraphicsObject::xChanged, this, &StripFrameItem::terminalsChanged);
		m_connections << connect(terminal, &QGraphicsObject::yChanged, this, &StripFrameItem::terminalsChanged);
		m_connections << connect(terminal, &QGraphicsObject::rotationChanged, this, &StripFrameItem::terminalsChanged);
		m_connections << connect(terminal, &QObject::destroyed, this, &StripFrameItem::terminalsChanged);
	}
	if (scene()) {
		hideComments();
	}
	terminalsChanged();
}

/**
	@brief StripFrameItem::hideComments
	The texts that show the comment of its terminals hidden: the name of
	the strip is written there, and the frame already says it - the symbol
	of a terminal that shows its comment wrote it beside every terminal
	(T85, the Renan's choice). Only those it hides are shown again, so that
	a text hidden for another reason stays hidden
*/
void StripFrameItem::hideComments()
{
	for (const QPointer<Element> &terminal : std::as_const(m_terminals))
	{
		if (!terminal) continue;
		const QList<DynamicElementTextItem *> texts = terminal->dynamicTextItems();
		for (DynamicElementTextItem *text : texts)
		{
			if (text->textFrom() != DynamicElementTextItem::ElementInfo
			    || text->infoName() != QETInformation::ELMT_COMMENT || !text->isVisible()) continue;
			text->setVisible(false);
			m_hidden << text;
		}
	}
}

void StripFrameItem::showComments()
{
	for (const QPointer<DynamicElementTextItem> &text : std::as_const(m_hidden)) {
		if (text) text->setVisible(true);
	}
	m_hidden.clear();
}

/**
	@brief StripFrameItem::terminalsChanged
	A terminal moved, or went off the folio or came back: the frame is
	worked out again
*/
void StripFrameItem::terminalsChanged()
{
	prepareGeometryChange();
	m_dirty = true;
	update();
	emit frameChanged();
}

/**
	@brief StripFrameItem::lies
	@return true when the terminals are in a row, false when they stand in a
	column: their places spread wider than tall
*/
bool StripFrameItem::lies() const
{
	QRectF spread;
	const QList<Element *> list = terminals();
	for (const Element *terminal : list) {
		spread |= QRectF(terminal->pos(), QSizeF(0.001, 0.001));
	}
	return spread.width() >= spread.height();
}

/**
	@brief StripFrameItem::orderedTerminals
	@return its terminals in the order they stand on the folio: from the
	left in a row, from the top in a column
*/
QList<Element *> StripFrameItem::orderedTerminals() const
{
	QList<Element *> list = terminals();
	const bool row = lies();
	std::stable_sort(list.begin(), list.end(), [row](const Element *a, const Element *b) {
		return row ? a->pos().x() < b->pos().x() : a->pos().y() < b->pos().y();
	});
	return list;
}

/**
	@brief StripFrameItem::spacing
	@return the distance from one terminal to the next along the strip, the
	mean of them when they are not even, 0 with fewer than two
*/
qreal StripFrameItem::spacing() const
{
	const QList<Element *> list = orderedTerminals();
	if (list.size() < 2) {
		return 0;
	}
	const QPointF span = list.last()->pos() - list.first()->pos();
	return (lies() ? span.x() : span.y()) / (list.size() - 1);
}

/**
	@brief StripFrameItem::setSpacing
	The terminals of the frame put @a pitch apart along the strip, the first
	where it is and the order kept, each at its own height across the strip
	(T85, asked by the Renan on 23/10/2026: the terminals of a strip stand
	too close to pick one by hand). One undo step, the wires following.

	Each terminal is moved on its own, and not by the drag of the folio: the
	drag of a single component joins it to any free terminal it comes in
	line with (ElementsMover::endMovement()), and spacing a strip one
	terminal at a time would wire them to each other.
*/
void StripFrameItem::setSpacing(qreal pitch)
{
	Diagram *folio = diagram();
	const QList<Element *> list = orderedTerminals();
	if (!folio || folio->isReadOnly() || pitch <= 0 || list.size() < 2) {
		return;
	}
	const bool row = lies();
	const QPointF first = list.first()->pos();
	auto *undo = new QUndoCommand(tr("Space the terminals of the strip %1").arg(m_name));
	for (int i = 1 ; i < list.size() ; ++i)
	{
		Element *terminal = list.at(i);
		const QPointF at = terminal->pos();
		const QPointF target = row ? QPointF(first.x() + i * pitch, at.y())
					   : QPointF(at.x(), first.y() + i * pitch);
		const QPointF movement = target - at;
		if (movement.manhattanLength() < 0.001) continue;
			//The command finds it moved, and keeps the way back
		terminal->setPos(target);
		DiagramContent content;
		content.m_elements << terminal;
		for (Conductor *conductor : terminal->conductors()) {
			conductor->updatePath();
			content.m_conductors_to_update << conductor;
		}
		new MoveGraphicsItemCommand(folio, content, terminal->pos() - at, undo);
	}
	if (undo->childCount()) {
		folio->undoStack().push(undo);
	} else {
		delete undo;
	}
}

/**
	@brief StripFrameItem::moveTerminals
	Its terminals moved by @a movement, with their wires, in one undo step:
	what dragging the frame does, in one go
*/
void StripFrameItem::moveTerminals(const QPointF &movement)
{
	Diagram *folio = diagram();
	if (!folio || folio->isReadOnly() || terminals().isEmpty()) {
		return;
	}
	selectTerminals();
	folio->elementsMover().beginMovement(folio);
	folio->elementsMover().continueMovement(movement);
	folio->elementsMover().endMovement();
	selectSelf();
}

	/// Only its terminals selected: what the drag of the folio moves
void StripFrameItem::selectTerminals()
{
	if (Diagram *folio = diagram())
	{
		folio->clearSelection();
		const QList<Element *> list = terminals();
		for (Element *terminal : list) {
			terminal->setSelected(true);
		}
	}
}

	/// Only the frame selected, as after a click on it
void StripFrameItem::selectSelf()
{
	if (Diagram *folio = diagram()) {
		folio->clearSelection();
	}
	setSelected(true);
}

void StripFrameItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		QetGraphicsItem::mousePressEvent(event);
		return;
	}
	if (event->modifiers() & Qt::ControlModifier) {
		setSelected(!isSelected());
	} else {
		selectSelf();
	}
	m_dragging = false;
	m_drag_from = event->scenePos();
	event->accept();
}

	/// The drag of the frame is the drag of its terminals: they are what
	/// the folio moves, the first of them keeping to the grid
void StripFrameItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	Diagram *folio = diagram();
	if (!(event->buttons() & Qt::LeftButton) || !folio || folio->isReadOnly()) {
		event->ignore();
		return;
	}
	event->accept();
	if (!m_dragging)
	{
		const QList<Element *> list = orderedTerminals();
		if (list.isEmpty()
		    || (event->scenePos() - m_drag_from).manhattanLength() < QApplication::startDragDistance()) {
			return;
		}
		m_dragging = true;
		m_anchor = list.first();
		m_anchor_from = m_anchor->pos();
		selectTerminals();
		folio->elementsMover().beginMovement(folio);
	}
	if (!m_anchor) {
		return;
	}
	const QPointF wanted = Diagram::snapToGrid(m_anchor_from + event->scenePos() - m_drag_from);
	folio->elementsMover().continueMovement(wanted - m_anchor->pos());
}

void StripFrameItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_dragging)
	{
		if (Diagram *folio = diagram()) {
			folio->elementsMover().endMovement();
		}
		selectSelf();
	}
	m_dragging = false;
	m_anchor.clear();
	event->accept();
}

/**
	@brief StripFrameItem::frameRect
	@return the rectangle of the frame, in scene coordinates - the item
	stays at the origin -, null when none of its terminals is on the folio
*/
QRectF StripFrameItem::frameRect() const
{
	computeGeometry();
	return m_frame;
}

/**
	@brief StripFrameItem::captionRect
	@return the rectangle the name is written in, in scene coordinates:
	always upright - the frame itself never turns, its terminals do -, at
	the left of a strip that lies, at the top of one that stands
*/
QRectF StripFrameItem::captionRect() const
{
	computeGeometry();
	return m_caption;
}

/**
	@brief StripFrameItem::computeGeometry
	Round what the terminals draw, short of the end of each terminal - the
	frame of a strip crosses the line of a terminal and leaves its end, where
	the wire comes, out of it, as the frames drawn by hand do -, and room
	for the name: at the left of a strip that lies, at the top of one that
	stands. The texts of a terminal beside it - its number - are in the
	frame too, as in the frames drawn by hand
*/
void StripFrameItem::computeGeometry() const
{
	if (!m_dirty) {
		return;
	}
	m_dirty = false;
	m_frame = QRectF();
	m_caption = QRectF();

	QRectF drawn;
	QList<const Terminal *> ends;
	const QList<Element *> list = terminals();
	for (const Element *terminal : list)
	{
		const QRectF body = terminal->mapRectToScene(terminal->drawnRect());
		drawn |= body;
			//And its texts beside it - the number of the terminal -, which
			//the frames drawn by hand hold; not one taken far from it
		const QList<DynamicElementTextItem *> texts = terminal->dynamicTextItems();
		for (DynamicElementTextItem *text : texts)
		{
			if (!text->isVisible() || text->toPlainText().trimmed().isEmpty()) continue;
			const qreal m = text->document()->documentMargin();
			const QRectF box = text->sceneBoundingRect().adjusted(m, m, -m, -m);
			if (box.intersects(body.adjusted(-15, -15, 15, 15))) drawn |= box;
		}
		const QList<Terminal *> own = terminal->terminals();
		for (const Terminal *t : own) {
			ends << t;
		}
	}
	if (drawn.isNull()) {
		return;
	}

	qreal left = drawn.left() - padding, right = drawn.right() + padding;
	qreal top = drawn.top() - padding, bottom = drawn.bottom() + padding;
	for (const Terminal *t : std::as_const(ends))
	{
		const QPointF p = t->dockConductor();
		switch (t->orientation())
		{
			case Qet::North: top = std::max(top, p.y() + clearance); break;
			case Qet::South: bottom = std::min(bottom, p.y() - clearance); break;
			case Qet::West:  left = std::max(left, p.x() + clearance); break;
			default:         right = std::min(right, p.x() - clearance); break;
		}
	}
	QRectF frame(QPointF(left, top), QPointF(right, bottom));
	if (frame.width() < 1 || frame.height() < 1) {
		frame = drawn.adjusted(-padding, -padding, padding, padding);
	}

	const QFontMetricsF metrics{captionFont()};
	const qreal width = metrics.horizontalAdvance(m_name) + 2 * caption_padding;
	const qreal height = metrics.height();
	if (frame.width() >= frame.height())
	{
		frame.setLeft(frame.left() - width);
		m_caption = QRectF(frame.left(), frame.center().y() - height / 2, width, height);
	}
	else
	{
		frame.setTop(frame.top() - height - caption_padding);
		m_caption = QRectF(frame.center().x() - width / 2, frame.top() + caption_padding / 2, width, height);
	}
	m_frame = frame;
}

QRectF StripFrameItem::boundingRect() const
{
	computeGeometry();
	if (m_frame.isNull()) {
		return QRectF();
	}
	return m_frame.adjusted(-grab_band_width, -grab_band_width, grab_band_width, grab_band_width);
}

/**
	@brief StripFrameItem::shape
	@return the border, widened into a band a hand can catch, and the name:
	a click inside the frame reaches the terminals, as it must
*/
QPainterPath StripFrameItem::shape() const
{
	computeGeometry();
	QPainterPath path;
	if (m_frame.isNull()) {
		return path;
	}
	QPainterPath border;
	border.addRect(m_frame);
	QPainterPathStroker stroker;
	stroker.setWidth(grab_band_width);
	path = stroker.createStroke(border);
	path.addRect(m_caption);
	return path;
}

void StripFrameItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)
	computeGeometry();
	if (m_frame.isNull()) {
		return;
	}
	painter->save();
	QPen pen(isSelected() ? QColor(Qt::blue) : QColor(Qt::black));
	pen.setStyle(Qt::DashLine);
	pen.setWidthF(isHovered() || isSelected() ? 2.0 : 1.0);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawRect(m_frame);
	painter->setFont(captionFont());
	painter->setPen(QPen(Qt::black));
	painter->drawText(m_caption, Qt::AlignCenter, m_name);
	painter->restore();
}

/**
	@brief StripFrameItem::editProperty
	Rename the strip: the frame, and the comment of its terminals, in one
	undo step
*/
void StripFrameItem::editProperty()
{
	Diagram *folio = diagram();
	if (!folio || folio->isReadOnly()) {
		return;
	}
	QWidget *parent = folio->views().isEmpty() ? nullptr : folio->views().first();
	bool ok = false;
	const QString name = askName(parent, m_name, &ok);
	if (ok) {
		rename(name);
	}
}

/**
	@brief StripFrameItem::rename
	The frame and the comment of its terminals renamed @a strip_name, in one
	undo step
*/
void StripFrameItem::rename(const QString &strip_name)
{
	Diagram *folio = diagram();
	const QString name = strip_name.trimmed();
	if (!folio || folio->isReadOnly() || name.isEmpty() || name == m_name) {
		return;
	}
	auto *undo = new QUndoCommand(tr("Rename the terminal strip %1").arg(m_name));
	FrameState after = stateOf(this);
	after.name = name;
	new StripFrameChange(this, folio, stateOf(this), after, undo);
	writeName(after.terminals, name, undo);
	folio->undoStack().push(undo);
}

/**
	@brief StripFrameItem::rotate
	Its terminals turned by @a angle round their middle, with their wires,
	in one undo step: the strip turned as one piece, see turnTerminals().
	The frame follows them, and its name stays upright
*/
void StripFrameItem::rotate(qreal angle)
{
	Diagram *folio = diagram();
	if (!folio || folio->isReadOnly() || terminals().isEmpty()) {
		return;
	}
	auto *undo = new QUndoCommand(tr("Rotate the drawn terminal strip %1").arg(m_name));
	turnTerminals(angle, undo);
	if (undo->childCount()) {
		folio->undoStack().push(undo);
	} else {
		delete undo;
	}
}

/**
	@brief StripFrameItem::turnTerminals
	Into @a undo, the steps that turn each terminal by @a angle and carry
	it round the middle of what the terminals draw - snapped to the grid,
	as the group rotation of the folio does with its pivot, so that a strip
	on the grid stays on it. Each terminal turns round its own origin, the
	point its place is, so turning it there and carrying that point round
	the middle turns the strip as one piece; the wires follow their
	terminals by themselves (Element::Element()), and the frame too
	(terminalsChanged()). Not animated: the place of a terminal would go
	in a straight line while it turns.

	The middle of the terminals, and not of the frame: the name makes the
	frame wider on one side, at the left of a strip that lies and at the
	top of one that stands, and turning round the middle of the frame would
	carry the strip half the width of its name away at each turn. And
	round the point of the turn before when none of the terminals moved
	since: the middle of four terminals three steps apart is half a step
	off the grid, snapped one way before a turn and the other way after
	it, and four turns bring the strip back where it was only when they
	all go round one point
*/
void StripFrameItem::turnTerminals(qreal angle, QUndoCommand *undo)
{
	const QList<Element *> list = terminals();
	if (list.isEmpty()) {
		return;
	}
	QList<QPointF> at;
	QRectF drawn;
	for (const Element *terminal : list)
	{
		at << terminal->pos();
		drawn |= terminal->mapRectToScene(terminal->drawnRect());
	}
	const QPointF pivot = at == m_turned_to ? m_turn_pivot : Diagram::snapToGrid(drawn.center());
	QList<QPointF> to;
	for (int i = 0 ; i < list.size() ; ++i)
	{
		Element *terminal = list.at(i);
		const QPointF from = at.at(i);
		to << Diagram::snapToGrid(pivot + turned(from - pivot, angle));
		new QPropertyUndoCommand(terminal, "rotation", QVariant(terminal->rotation()),
					 QVariant(terminal->rotation() + angle), undo);
		if (to.last() != from) {
			new QPropertyUndoCommand(terminal, "pos", QVariant(from), QVariant(to.last()), undo);
		}
	}
	m_turn_pivot = pivot;
	m_turned_to = to;
}

/**
	@brief StripFrameItem::remove
	The frame off the folio, in one undo step. The terminals keep the name
	in their comment: they are still of that strip
*/
void StripFrameItem::remove()
{
	if (QUndoCommand *undo = removeCommand()) {
		diagram()->undoStack().push(undo);
	}
}

/**
	@brief StripFrameItem::removeCommand
	@return the undo step that takes the frame off the folio and leaves its
	terminals, not yet run; nullptr when it cannot be taken off
*/
QUndoCommand *StripFrameItem::removeCommand()
{
	Diagram *folio = diagram();
	if (!folio || folio->isReadOnly()) {
		return nullptr;
	}
	auto *undo = new QUndoCommand(tr("Delete the drawn terminal strip %1").arg(m_name));
	FrameState after = stateOf(this);
	after.on_folio = false;
	new StripFrameChange(this, folio, stateOf(this), after, undo);
	return undo;
}

QVariant StripFrameItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == QGraphicsItem::ItemSceneChange && scene())
	{
		QList<QPointer<StripFrameItem>> &list = framesOn()[scene()];
		list.removeAll(this);
		if (list.isEmpty()) framesOn().remove(scene());
			//Off the folio, the texts of the comment show again
		showComments();
	}
	else if (change == QGraphicsItem::ItemSceneHasChanged && scene())
	{
		framesOn()[scene()] << this;
		hideComments();
		terminalsChanged();
	}
	return QetGraphicsItem::itemChange(change, value);
}

void StripFrameItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	Diagram *folio = diagram();
	if (!folio || folio->isReadOnly()) {
		QetGraphicsItem::contextMenuEvent(event);
		return;
	}
	QMenu menu;
	QAction *rename = menu.addAction(tr("Rename the strip..."));
	QAction *turn = menu.addAction(tr("Rotate the strip"));
	QAction *erase = menu.addAction(tr("Delete the drawn strip"));
	QAction *chosen = menu.exec(event->screenPos());
	if (chosen == rename) {
		editProperty();
	} else if (chosen == turn) {
		rotate();
	} else if (chosen == erase) {
		remove();
	}
	event->accept();
}

QString StripFrameItem::tagName()
{
	return QStringLiteral("strip_frame");
}

QString StripFrameItem::listTagName()
{
	return QStringLiteral("strip_frames");
}

/**
	@brief StripFrameItem::toXml
	Its own tag, with the name and the terminals it holds - by the uuid of
	their element. Not the rectangle: it is worked out from them
*/
QDomElement StripFrameItem::toXml(QDomDocument &document) const
{
	QDomElement result = document.createElement(tagName());
	result.setAttribute(QStringLiteral("name"), m_name);
	const QList<Element *> list = terminals();
	for (const Element *terminal : list)
	{
		QDomElement member = document.createElement(QStringLiteral("terminal"));
		member.setAttribute(QStringLiteral("element"), terminal->uuid().toString());
		result.appendChild(member);
	}
	return result;
}

/**
	@brief StripFrameItem::fromXml
	@return the frame @a e describes, round those of @a elements it names;
	nullptr when it names none of them
*/
StripFrameItem *StripFrameItem::fromXml(const QDomElement &e, const QList<Element *> &elements)
{
	if (e.tagName() != tagName()) {
		return nullptr;
	}
	QList<Element *> members;
	for (QDomElement member = e.firstChildElement(QStringLiteral("terminal")) ; !member.isNull() ;
	     member = member.nextSiblingElement(QStringLiteral("terminal")))
	{
		const QUuid uuid(member.attribute(QStringLiteral("element")));
		for (Element *element : elements)
		{
			if (element->uuid() == uuid && !members.contains(element)) {
				members << element;
				break;
			}
		}
	}
	if (members.isEmpty()) {
		return nullptr;
	}
	auto *frame = new StripFrameItem(e.attribute(QStringLiteral("name")));
	frame->setTerminals(members);
	return frame;
}

/**
	@brief StripFrameItem::isStripTerminal
	@return true for a terminal of a strip drawn in the schematic - a
	component of the terminal type
*/
bool StripFrameItem::isStripTerminal(const Element *element)
{
	return element && element->elementData().m_type == ElementData::Terminal;
}

QList<Element *> StripFrameItem::stripTerminals(const QList<QGraphicsItem *> &items)
{
	QList<Element *> list;
	for (QGraphicsItem *item : items)
	{
		auto *element = qgraphicsitem_cast<Element *>(item);
		if (isStripTerminal(element) && !list.contains(element)) list << element;
	}
	return list;
}

/**
	@brief StripFrameItem::frameOf
	@return the frame of its folio @a terminal is in, or nullptr
*/
StripFrameItem *StripFrameItem::frameOf(const Element *terminal)
{
	if (!terminal || !terminal->scene()) {
		return nullptr;
	}
	const QList<QPointer<StripFrameItem>> frames = framesOn().value(terminal->scene());
	for (const QPointer<StripFrameItem> &frame : frames)
	{
		if (!frame) continue;
		for (const QPointer<Element> &member : std::as_const(frame->m_terminals)) {
			if (member == terminal) return frame;
		}
	}
	return nullptr;
}

/**
	@brief StripFrameItem::refreshAll
	Every frame of @a diagram worked out again: a component came onto the
	folio or went off it, which moves nothing a frame listens to
*/
void StripFrameItem::refreshAll(Diagram *diagram)
{
	if (!diagram) {
		return;
	}
	const QList<QPointer<StripFrameItem>> frames = framesOn().value(diagram);
	for (const QPointer<StripFrameItem> &frame : frames)
	{
		if (frame) frame->terminalsChanged();
	}
}

/**
	@brief StripFrameItem::suggestedName
	@return the name to offer for a frame round @a terminals: the name of
	the frame one of them is in, else what all of them say in their comment
*/
QString StripFrameItem::suggestedName(const QList<Element *> &terminals)
{
	for (Element *terminal : terminals)
	{
		if (StripFrameItem *frame = frameOf(terminal)) return frame->stripName();
	}
	QString common;
	for (int i = 0 ; i < terminals.size() ; ++i)
	{
		const QString comment = terminals.at(i)->elementInformations()
					.value(QETInformation::ELMT_COMMENT).toString().trimmed();
		if (i == 0) common = comment;
		else if (comment != common) return QString();
	}
	return common;
}

QString StripFrameItem::askName(QWidget *parent, const QString &suggestion, bool *ok)
{
	bool accepted = false;
	const QString name = QInputDialog::getText(parent, tr("Drawn terminal strip"), tr("Name of the strip:"),
						   QLineEdit::Normal, suggestion, &accepted).trimmed();
	if (ok) *ok = accepted && !name.isEmpty();
	return name;
}

/**
	@brief StripFrameItem::drawAround
	Draw the frame of a strip @a strip_name round @a selection, those of it
	that are terminals of a strip; in one undo step, with the name written
	in their comment (T85). Terminals already in a frame make
	that frame grow - the one that holds the most of them -, under the
	name given; others start a new one - a strip
	shown in two places of a folio has two frames of one name. A terminal
	is in one frame only, and leaves the one it was in.
*/
void StripFrameItem::drawAround(Diagram *diagram, const QList<Element *> &selection, const QString &strip_name)
{
	const QString name = strip_name.trimmed();
	if (!diagram || diagram->isReadOnly() || name.isEmpty()) {
		return;
	}
	QList<Element *> chosen;
	for (Element *element : selection) {
		if (isStripTerminal(element) && !chosen.contains(element)) chosen << element;
	}
	if (chosen.isEmpty()) {
		return;
	}

		//The frame that holds the most of them grows; the first of two that
		//hold as many, in the order they were chosen
	StripFrameItem *target = nullptr;
	int most = 0;
	QHash<StripFrameItem *, int> held;
	for (Element *terminal : std::as_const(chosen))
	{
		StripFrameItem *frame = frameOf(terminal);
		if (!frame) continue;
		const int count = ++held[frame];
		if (count > most)
		{
			most = count;
			target = frame;
		}
	}

	auto *undo = new QUndoCommand(tr("Draw the terminal strip %1").arg(name));
	QSet<StripFrameItem *> others;
	for (Element *terminal : std::as_const(chosen))
	{
		StripFrameItem *frame = frameOf(terminal);
		if (frame && frame != target) others << frame;
	}
	for (StripFrameItem *frame : std::as_const(others))
	{
		const FrameState before = stateOf(frame);
		FrameState after = before;
		after.terminals.clear();
		for (const QPointer<Element> &member : before.terminals) {
			if (!chosen.contains(member.data())) after.terminals << member;
		}
		after.on_folio = !after.terminals.isEmpty();
		new StripFrameChange(frame, diagram, before, after, undo);
	}
	StripFrameItem *frame = target ? target : new StripFrameItem(name);
	const FrameState before = target ? stateOf(target) : FrameState{QString(), {}, false};
	FrameState after;
	after.name = name;
	after.terminals = target ? before.terminals : QList<QPointer<Element>>();
	for (Element *terminal : std::as_const(chosen))
	{
		bool known = false;
		for (const QPointer<Element> &member : std::as_const(after.terminals)) {
			if (member == terminal) known = true;
		}
		if (!known) after.terminals << terminal;
	}
	after.on_folio = true;
	new StripFrameChange(frame, diagram, before, after, undo);
	writeName(after.terminals, name, undo);
	diagram->undoStack().push(undo);
}

/**
	@brief StripFrameItem::takeOut
	@a terminals out of the frames they are in, in one undo step; a frame
	left with none goes. Their comment loses the name of the strip
*/
void StripFrameItem::takeOut(Diagram *diagram, const QList<Element *> &terminals)
{
	if (!diagram || diagram->isReadOnly()) {
		return;
	}
	QSet<StripFrameItem *> frames;
	for (Element *terminal : terminals)
	{
		if (StripFrameItem *frame = frameOf(terminal)) frames << frame;
	}
	if (frames.isEmpty()) {
		return;
	}
	auto *undo = new QUndoCommand(tr("Take terminals out of their drawn strip"));
	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> comments;
	for (StripFrameItem *frame : std::as_const(frames))
	{
		const FrameState before = stateOf(frame);
		FrameState after = before;
		after.terminals.clear();
		for (const QPointer<Element> &member : before.terminals)
		{
			if (!terminals.contains(member.data()))
			{
				after.terminals << member;
				continue;
			}
			const DiagramContext was = member->elementInformations();
			if (was.value(QETInformation::ELMT_COMMENT).toString() == frame->stripName())
			{
				DiagramContext now = was;
				now.remove(QETInformation::ELMT_COMMENT);
				comments.insert(member, qMakePair(was, now));
			}
		}
		after.on_folio = !after.terminals.isEmpty();
		new StripFrameChange(frame, diagram, before, after, undo);
	}
	if (!comments.isEmpty()) {
		new ChangeElementInformationCommand(comments, undo);
	}
	diagram->undoStack().push(undo);
}

/**
	@brief StripFrameItem::stripFrames
	@return the frames among @a items, in their order
*/
QList<StripFrameItem *> StripFrameItem::stripFrames(const QList<QGraphicsItem *> &items)
{
	QList<StripFrameItem *> list;
	for (QGraphicsItem *item : items)
	{
		auto *frame = qgraphicsitem_cast<StripFrameItem *>(item);
		if (frame && !list.contains(frame)) list << frame;
	}
	return list;
}

/**
	@brief StripFrameItem::rotateSelection
	The Rotate command of @a diagram when its selection holds the frame of
	a drawn strip (asked on 23/10/2026: a strip drawn lying could not be
	stood up as one piece). Each frame selected turns its terminals by
	@a angle round their middle, see turnTerminals(), and the rest of the
	selection turns as the folio turns it, each item in place - all in one
	undo step. The terminals a selected frame holds, and what they carry,
	turn with their frame only, and not once more in place.
	@return false when no frame is selected, and nothing is done: the
	selection turns as it always did
*/
bool StripFrameItem::rotateSelection(Diagram *diagram, qreal angle)
{
	if (!diagram || diagram->isReadOnly()) {
		return false;
	}
	const QList<QGraphicsItem *> selection = diagram->selectedItems();
	const QList<StripFrameItem *> frames = stripFrames(selection);
	if (frames.isEmpty()) {
		return false;
	}

	QSet<const QGraphicsItem *> held;
	for (StripFrameItem *frame : frames)
	{
		const QList<Element *> list = frame->terminals();
		for (const Element *terminal : list) {
			held << terminal;
		}
	}
	QList<QGraphicsItem *> rest;
	for (QGraphicsItem *item : selection)
	{
		if (item->type() == Type) continue;
		bool carried = false;
		for (const QGraphicsItem *up = item ; up && !carried ; up = up->parentItem()) {
			carried = held.contains(up);
		}
		if (!carried) rest << item;
	}

	auto *undo = new QUndoCommand(frames.size() == 1 && rest.isEmpty()
				      ? tr("Rotate the drawn terminal strip %1").arg(frames.first()->stripName())
				      : tr("Rotate the selection"));
	for (StripFrameItem *frame : frames) {
		frame->turnTerminals(angle, undo);
	}
	if (!rest.isEmpty())
	{
			//RotateSelectionCommand turns the selection of the folio: while
			//it is built, only the rest is selected - quietly, the
			//selection being the same once it is
		const QSignalBlocker blocker(diagram);
		diagram->clearSelection();
		for (QGraphicsItem *item : std::as_const(rest)) {
			item->setSelected(true);
		}
		new RotateSelectionCommand(diagram, angle, undo);
		diagram->clearSelection();
		for (QGraphicsItem *item : selection) {
			item->setSelected(true);
		}
	}
	if (undo->childCount()) {
		diagram->undoStack().push(undo);
	} else {
		delete undo;
	}
	return true;
}

/**
	@brief StripFrameItem::deleteCommand
	@return the one undo step of the Delete command when the selection
	holds @a frames (asked on 23/10/2026: the frame of a strip could not be
	taken off without its terminals): @a others first - what the rest of
	the selection deletes, the components, the cable lines -, then the
	frames off the folio, their terminals left on it unless @a others takes
	them too; nullptr when there is nothing to do. The step owns @a others
*/
QUndoCommand *StripFrameItem::deleteCommand(const QList<StripFrameItem *> &frames,
					    const QList<QUndoCommand *> &others)
{
	QList<QUndoCommand *> steps;
	for (QUndoCommand *step : others) {
		if (step) steps << step;
	}
	for (StripFrameItem *frame : frames)
	{
		if (QUndoCommand *step = frame ? frame->removeCommand() : nullptr) steps << step;
	}
	if (steps.size() < 2) {
		return steps.isEmpty() ? nullptr : steps.first();
	}
		//One step, so that one undo brings the frames and the rest back
	return new BatchCommand(steps, tr("Delete"));
}
