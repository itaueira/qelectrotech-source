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

#include "../diagram.h"
#include "../properties/elementdata.h"
#include "../qetinformation.h"
#include "../undocommand/changeelementinformationcommand.h"
#include "dynamicelementtextitem.h"
#include "element.h"
#include "terminal.h"

#include <QDomDocument>
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
#include <QUndoCommand>
#include <QUndoStack>

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
	setMovable(false);
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
	terminalsChanged();
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
	QPen pen(Qt::black);
	pen.setStyle(Qt::DashLine);
	pen.setWidthF(isHovered() ? 2.0 : 1.0);
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
	@brief StripFrameItem::remove
	The frame off the folio, in one undo step. The terminals keep the name
	in their comment: they are still of that strip
*/
void StripFrameItem::remove()
{
	Diagram *folio = diagram();
	if (!folio || folio->isReadOnly()) {
		return;
	}
	auto *undo = new QUndoCommand(tr("Delete the drawn terminal strip %1").arg(m_name));
	FrameState after = stateOf(this);
	after.on_folio = false;
	new StripFrameChange(this, folio, stateOf(this), after, undo);
	folio->undoStack().push(undo);
}

QVariant StripFrameItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == QGraphicsItem::ItemSceneChange && scene())
	{
		QList<QPointer<StripFrameItem>> &list = framesOn()[scene()];
		list.removeAll(this);
		if (list.isEmpty()) framesOn().remove(scene());
	}
	else if (change == QGraphicsItem::ItemSceneHasChanged && scene())
	{
		framesOn()[scene()] << this;
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
	QAction *erase = menu.addAction(tr("Delete the drawn strip"));
	QAction *chosen = menu.exec(event->screenPos());
	if (chosen == rename) {
		editProperty();
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
