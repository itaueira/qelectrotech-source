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
#include "mountinglayoutviewitem.h"

#include "../../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../../diagram.h"
#include "../../location/layout/mountedpartitem.h"
#include "../../location/layout/mountedprofileitem.h"
#include "../../location/layout/mountingscene.h"
#include "../../qetproject.h"

#include <QBrush>
#include <QColor>
#include <QDomDocument>
#include <QDomElement>
#include <QFont>
#include <QGraphicsView>
#include <QInputDialog>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOptionGraphicsItem>

#include <cmath>

namespace
{
	/// folio units: how tall the caption under the drawing is written. In
	/// folio units and not in millimetre on purpose - the caption belongs
	/// to the sheet, so it has to stay the same size whatever the scale of
	/// the panel underneath it.
	const qreal CAPTION_FOLIO_HEIGHT = 9.0;

	/// folio units: how wide the notice drawn in place of a missing face
	/// is. The same width QetGraphicsTableItem gives the reason a query
	/// failed, for the same reason: there is no column to take one from.
	const qreal NOTICE_FOLIO_WIDTH = 300.0;

	/// folio units: the slack the bounding rectangle leaves around what is
	/// painted, so a cosmetic outline is never clipped by its own item.
	const qreal OUTLINE_FOLIO_SLACK = 2.0;

	/// folio units: below this, the grid of the plate is a grey smear and
	/// is left out. Measured on the folio and not on the panel, because
	/// what decides is how far apart the lines land on the paper, and the
	/// panel is drawn at whatever factor somebody typed in.
	const qreal GRID_VISIBLE_FOLIO_STEP = 4.0;
}

/**
	@brief MountingLayoutViewItem::MountingLayoutViewItem
	@param parent parent graphics item
*/
MountingLayoutViewItem::MountingLayoutViewItem(QGraphicsItem *parent) :
	QetGraphicsItem(parent)
{
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setFlag(QGraphicsItem::ItemIsMovable, true);
	setDrawingScale(defaultDrawingScale());
}

/**
	@brief MountingLayoutViewItem::~MountingLayoutViewItem

	The parts are taken down here rather than left to the base destructor,
	which would take them down too. The difference is the order: clearParts
	empties the list before it deletes anything, so nothing can read a
	pointer to a part that is already gone. Letting ~QGraphicsItem do it
	would leave this list full of addresses while the framework walks the
	children, and boundingRect() reads that list.
*/
MountingLayoutViewItem::~MountingLayoutViewItem()
{
	clearParts();
}

/**
	@brief MountingLayoutViewItem::setProject
	@param project the project holding the face
*/
void MountingLayoutViewItem::setProject(QETProject *project)
{
	if (m_project.data() == project) {
		return;
	}

	if (m_project)
	{
		disconnect(m_project.data(), &QETProject::mountingLayoutChanged,
			   this, &MountingLayoutViewItem::reload);
	}

	m_project = project;

	if (m_project)
	{
		connect(m_project.data(), &QETProject::mountingLayoutChanged,
			this, &MountingLayoutViewItem::reload);
	}

	reload();
}

/**
	@brief MountingLayoutViewItem::project
	@return the project this item reads the layout of
*/
QETProject *MountingLayoutViewItem::project() const
{
	return m_project.data();
}

/**
	@brief MountingLayoutViewItem::setSurfaceUuid
	@param surface_uuid which face to show
*/
void MountingLayoutViewItem::setSurfaceUuid(const QString &surface_uuid)
{
	if (m_surface_uuid == surface_uuid) {
		return;
	}

	m_surface_uuid = surface_uuid;
	reload();
}

/**
	@brief MountingLayoutViewItem::surfaceUuid
	@return which face this item shows
*/
QString MountingLayoutViewItem::surfaceUuid() const
{
	return m_surface_uuid;
}

/**
	@brief MountingLayoutViewItem::hasSurface
	@return true when the project holds the face named
*/
bool MountingLayoutViewItem::hasSurface() const
{
	return m_has_surface;
}

/**
	@brief MountingLayoutViewItem::error
	@return why there is nothing to draw, empty when there is something

	Three answers and not one, because the three states are three different
	things to do about it: attach the item to a project, say which face, or
	find out what happened to a face that was there yesterday.
*/
QString MountingLayoutViewItem::error() const
{
	if (m_has_surface) {
		return QString();
	}

	if (!m_project) {
		return tr("Ce calepinage n'est rattaché à aucun projet.");
	}

	if (m_surface_uuid.isEmpty()) {
		return tr("Aucune platine n'est désignée pour ce calepinage.");
	}

	return tr("La platine de ce calepinage n'est plus dans le projet.");
}

/**
	@brief MountingLayoutViewItem::surface
	@return the face as the project holds it
*/
MountingSurface MountingLayoutViewItem::surface() const
{
	return m_surface;
}

/**
	@brief MountingLayoutViewItem::partCount
	@return how many parts are drawn
*/
int MountingLayoutViewItem::partCount() const
{
	return m_parts.count();
}

/**
	@brief MountingLayoutViewItem::partItem
	@param item_uuid which part
	@return the item drawn for it, nullptr when there is none
*/
MountedPartItem *MountingLayoutViewItem::partItem(const QString &item_uuid) const
{
	if (item_uuid.isEmpty()) {
		return nullptr;
	}

	for (MountedPartItem *part : m_parts)
	{
		if (part && part->uuid() == item_uuid) {
			return part;
		}
	}

	return nullptr;
}

/**
	@brief MountingLayoutViewItem::drawnItemUuids
	@return the identity of every part drawn, in the order of the face
*/
QStringList MountingLayoutViewItem::drawnItemUuids() const
{
	QStringList uuids;
	for (MountedPartItem *part : m_parts)
	{
		if (part) {
			uuids << part->uuid();
		}
	}
	return uuids;
}

/**
	@brief MountingLayoutViewItem::drawingScale
	@return how many folio units one millimetre of panel is drawn as
*/
qreal MountingLayoutViewItem::drawingScale() const
{
	return m_drawing_scale;
}

/**
	@brief MountingLayoutViewItem::setDrawingScale
	@param folio_units_per_millimetre the factor

	The one crossing between the millimetre of the panel and the unit of the
	folio, and the only place the framework transform of this item is ever
	set. Anything that calls QGraphicsItem::setScale on one of these behind
	its back leaves the item drawing at one factor and answering with
	another.
*/
void MountingLayoutViewItem::setDrawingScale(qreal folio_units_per_millimetre)
{
	qreal wanted = folio_units_per_millimetre;
	if (!std::isfinite(wanted)) {
		wanted = defaultDrawingScale();
	}
	wanted = qBound(minimumDrawingScale(), wanted, maximumDrawingScale());

	if (qFuzzyCompare(wanted, m_drawing_scale)
	    && qFuzzyCompare(QGraphicsItem::scale(), m_drawing_scale))
	{
		return;
	}

	prepareGeometryChange();
	m_drawing_scale = wanted;
	QGraphicsItem::setScale(m_drawing_scale);
	update();
}

/**
	@brief MountingLayoutViewItem::folioLengthFromMillimetre
	@param length_mm a length of panel, millimetre
	@return what it measures on the folio
*/
qreal MountingLayoutViewItem::folioLengthFromMillimetre(qreal length_mm) const
{
	return length_mm * m_drawing_scale;
}

/**
	@brief MountingLayoutViewItem::millimetreFromFolioLength
	@param length_folio a length on the folio, folio units
	@return what it stands for on the panel, millimetre
*/
qreal MountingLayoutViewItem::millimetreFromFolioLength(qreal length_folio) const
{
	if (qFuzzyIsNull(m_drawing_scale)) {
		return 0.0;
	}
	return length_folio / m_drawing_scale;
}

/**
	@brief MountingLayoutViewItem::folioSize
	@return the room the whole drawing takes on the folio
*/
QSizeF MountingLayoutViewItem::folioSize() const
{
	const QRectF drawn = contentRect().united(captionRect());
	return QSizeF(folioLengthFromMillimetre(drawn.width()),
		      folioLengthFromMillimetre(drawn.height()));
}

/**
	@brief MountingLayoutViewItem::plateRect
	@return the sheet metal, millimetre, empty when nobody measured it

	Empty and not guessed. A plate whose useful area nobody has typed in is
	drawn as the parts on it and nothing else, which is honest: inventing a
	rectangle here would put a sheet of metal on a workshop drawing that
	nobody ever cut.
*/
QRectF MountingLayoutViewItem::plateRect() const
{
	if (!m_has_surface || !m_surface.area.isValid()) {
		return QRectF();
	}
	return m_surface.area.rect();
}

/**
	@brief MountingLayoutViewItem::contentRect
	@return everything drawn of the panel, millimetre

	The plate united with every part, so a part dragged past the edge of the
	plate is still inside the item that draws it - which is what keeps the
	folio from clipping exactly the mistake somebody needs to see.
*/
QRectF MountingLayoutViewItem::contentRect() const
{
	QRectF rect = plateRect();

	for (MountedPartItem *part : m_parts)
	{
		if (part) {
			rect = rect.united(part->mapRectToParent(part->drawnRect()));
		}
	}

	if (rect.isEmpty())
	{
			//A face with no measured plate and nothing on it still has
			//to occupy the sheet, or the person who just posed it sees
			//nothing at all and poses a second one.
		return QRectF(0.0, 0.0,
			      millimetreFromFolioLength(NOTICE_FOLIO_WIDTH),
			      millimetreFromFolioLength(CAPTION_FOLIO_HEIGHT * 3.0));
	}

	return rect;
}

/**
	@brief MountingLayoutViewItem::captionRect
	@return the band the caption is written in, millimetre
*/
QRectF MountingLayoutViewItem::captionRect() const
{
	const QRectF content = contentRect();
	const qreal height = millimetreFromFolioLength(CAPTION_FOLIO_HEIGHT);
	return QRectF(content.left(), content.bottom(), content.width(), height);
}

/**
	@brief MountingLayoutViewItem::captionText
	@return what is written under the drawing

	The scale is written out because a drawing whose scale is not on it is a
	drawing nobody can measure. The plate nobody measured says so, in the
	same line, because the drawing above cannot say it: parts floating with
	no sheet metal under them look like a mistake of the drawing rather than
	a gap in the data.
*/
QString MountingLayoutViewItem::captionText() const
{
	if (!m_has_surface) {
		return error();
	}

	const QString head = tr("Calepinage — %1").arg(m_surface.designation());
	const QString scale = tr("1 mm = %1 u")
			      .arg(QString::number(m_drawing_scale, 'g', 4));

	if (!m_surface.area.isValid()) {
		return tr("%1 · %2 · platine non mesurée").arg(head, scale);
	}

	return tr("%1 · %2 · %3 × %4 mm")
	       .arg(head, scale,
		    QString::number(m_surface.area.width, 'g', 6),
		    QString::number(m_surface.area.height, 'g', 6));
}

/**
	@brief MountingLayoutViewItem::boundingRect
	@return what is painted, plus the slack the outline needs
*/
QRectF MountingLayoutViewItem::boundingRect() const
{
	const qreal slack = millimetreFromFolioLength(OUTLINE_FOLIO_SLACK);
	return contentRect().united(captionRect())
	       .adjusted(-slack, -slack, slack, slack);
}

/**
	@brief MountingLayoutViewItem::shape
	@return the shape a click has to land in

	The whole drawing, caption included, and that is deliberate: on a folio
	this is one object. The part items inside it take no mouse button at
	all, so a click anywhere over the plate selects the view and never a
	breaker - which is the visible half of the rule that the folio does not
	edit the panel.
*/
QPainterPath MountingLayoutViewItem::shape() const
{
	QPainterPath path;
	path.addRect(contentRect().united(captionRect()));
	return path;
}

/**
	@brief MountingLayoutViewItem::paint
	@param painter the painter to use
	@param option the style options
	@param widget the widget being painted on

	The sheet metal and the caption only. Every part is a child item and
	paints itself, which is not a shortcut: it is what makes the drawing on
	the folio the same drawing as the one in the layout window, by
	construction and not by two painters agreeing. A rail that changes shape
	changes shape in both places, because there is one painter.
*/
void MountingLayoutViewItem::paint(QPainter *painter,
				   const QStyleOptionGraphicsItem *option,
				   QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	if (!m_has_surface)
	{
		paintNotice(painter, error());
		return;
	}

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, false);

	const QRectF plate = plateRect();

	if (!plate.isEmpty())
	{
			//The same white sheet with a grey edge the layout window
			//draws as its background. It is painted here and is not a
			//background, because on a folio there is no canvas to be
			//the background of.
		QPen edge(QColor(80, 80, 80));
		edge.setCosmetic(true);
		painter->setPen(edge);
		painter->setBrush(QColor(255, 255, 255));
		painter->drawRect(plate);

			//The grid of the layout window, asked of it rather than
			//copied from it: the day that step changes, the plate on
			//the folio keeps reading as the same plate. It is left out
			//altogether when the lines would land closer together than
			//a person can tell apart on paper, because a grey smear
			//costs the same to print as a drawing.
		const qreal step = MountingScene::gridStep();
		if (step > 0.0
		    && folioLengthFromMillimetre(step) >= GRID_VISIBLE_FOLIO_STEP)
		{
			QPen grid(QColor(214, 214, 214));
			grid.setCosmetic(true);
			painter->setPen(grid);

			for (qreal x = plate.left() + step ;
			     x < plate.right() ;
			     x += step)
			{
				painter->drawLine(QPointF(x, plate.top()),
						  QPointF(x, plate.bottom()));
			}
			for (qreal y = plate.top() + step ;
			     y < plate.bottom() ;
			     y += step)
			{
				painter->drawLine(QPointF(plate.left(), y),
						  QPointF(plate.right(), y));
			}
		}
	}
	else
	{
			//No sheet metal to draw, so the outline of what is drawn is
			//dashed: a solid rectangle there would be read as a plate of
			//that size, which is the one thing nobody measured.
		QPen missing(QColor(176, 106, 0));
		missing.setCosmetic(true);
		missing.setStyle(Qt::DashLine);
		painter->setPen(missing);
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(contentRect());
	}

	if (isSelected())
	{
		QColor color(Qt::darkBlue);
		color.setAlpha(20);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QBrush(color));
		painter->drawRect(contentRect().united(captionRect()));
	}

		//The caption, sized in folio units and written in millimetre, so
		//that it reads the same whatever the panel is drawn at.
	const qreal text_height = millimetreFromFolioLength(CAPTION_FOLIO_HEIGHT);
	if (text_height > 0.0)
	{
		QFont font = painter->font();
		font.setPixelSize(qMax(1, qRound(text_height * 0.75)));
		painter->setFont(font);
		painter->setPen(QPen(QColor(40, 40, 40)));
		painter->setBrush(Qt::NoBrush);
		painter->drawText(captionRect(),
				  Qt::AlignLeft | Qt::AlignVCenter,
				  captionText());
	}

	painter->restore();
}

/**
	@brief MountingLayoutViewItem::paintNotice
	@param painter the painter to use
	@param notice what to say instead of a drawing

	A box with a sentence in it, in place of the plate that is not there.
	Without it the two states are the same drawing - no plate, no part, no
	frame of any size - and a folio that lost its plate would look like a
	folio somebody had left blank.
*/
void MountingLayoutViewItem::paintNotice(QPainter *painter, const QString &notice)
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, false);

	const QRectF box = contentRect();

	QPen frame(QColor(176, 106, 0));
	frame.setCosmetic(true);
	frame.setStyle(Qt::DashLine);
	painter->setPen(frame);
	painter->setBrush(QBrush(QColor(176, 106, 0), Qt::BDiagPattern));
	painter->drawRect(box);

	const qreal text_height = millimetreFromFolioLength(CAPTION_FOLIO_HEIGHT);
	if (text_height > 0.0)
	{
		QFont font = painter->font();
		font.setPixelSize(qMax(1, qRound(text_height * 0.75)));
		painter->setFont(font);
		painter->setPen(QPen(QColor(40, 40, 40)));
		painter->setBrush(Qt::NoBrush);
		painter->drawText(box.adjusted(text_height, text_height,
					       -text_height, -text_height),
				  Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				  notice);
	}

	painter->restore();
}

/**
	@brief MountingLayoutViewItem::name
	@return how the undo menu calls this item
*/
QString MountingLayoutViewItem::name() const
{
	if (m_has_surface) {
		return tr("le calepinage de %1").arg(m_surface.designation());
	}
	return tr("un calepinage");
}

/**
	@brief MountingLayoutViewItem::editProperty

	The scale, and nothing else. Which face this item shows is decided when
	it is posed and is not edited here: a view that could be pointed at
	another plate by a double click would make the sheet somebody printed
	yesterday show another panel today, with nothing in the drawing saying
	so.
*/
void MountingLayoutViewItem::editProperty()
{
	Diagram *diagram_ = diagram();
	if (!diagram_ || diagram_->isReadOnly()) {
		return;
	}

	QWidget *parent_widget = diagram_->views().isEmpty()
				 ? nullptr
				 : diagram_->views().first();

	bool accepted = false;
	const double chosen = QInputDialog::getDouble(
				parent_widget,
				tr("Échelle du calepinage"),
				tr("Unités de folio par millimètre de platine :"),
				m_drawing_scale,
				minimumDrawingScale(),
				maximumDrawingScale(),
				4,
				&accepted);

	if (!accepted || qFuzzyCompare(qreal(chosen), m_drawing_scale)) {
		return;
	}

	auto *undo = new QPropertyUndoCommand(this, "drawingScale",
					      QVariant(m_drawing_scale),
					      QVariant(qreal(chosen)));
	undo->setText(tr("modifier l'échelle d'un calepinage"));
	diagram_->undoStack().push(undo);
}

/**
	@brief MountingLayoutViewItem::reload

	Read the face again and redraw it. Called by the signal of the project,
	which is what makes this item a view and not a photograph.

	It compares before it rebuilds, and that matters more than it looks: the
	layout window writes the whole face back to the project after every
	step, so this runs on every drag of every part of every plate. A face
	that came back identical - because the step was on another face, or
	because it was undone - must cost nothing here.
*/
void MountingLayoutViewItem::reload()
{
	MountingSurface found;
	bool found_it = false;

	if (m_project && !m_surface_uuid.isEmpty())
	{
		const MountingLayout layout = m_project->mountingLayout();
		const int index = layout.indexOfSurface(m_surface_uuid);
		if (index >= 0)
		{
			found = layout.at(index);
			found_it = true;
		}
	}

	if (found_it == m_has_surface && found == m_surface) {
		return;
	}

	prepareGeometryChange();
	m_has_surface = found_it;
	m_surface = found;
	rebuild();
	update();
}

/**
	@brief MountingLayoutViewItem::rebuild

	Draw every part of the face again, from nothing.

	The whole drawing and not the difference, and the reason is that this
	one can afford it where MountingScene could not: there, rebuilding drops
	the undo stack of the layout, so an incremental path had to be written;
	here there is no stack, no selection to preserve and no gesture in
	progress - the parts take no mouse button at all. The guard in reload()
	is what keeps that cheap.
*/
void MountingLayoutViewItem::rebuild()
{
	clearParts();

	if (!m_has_surface) {
		return;
	}

	const QList<MountedItem> items = m_surface.items;
	for (const MountedItem &item : items)
	{
		MountedPartItem *part = item.isCutToLength()
					? new MountedProfileItem(item, this)
					: new MountedPartItem(item, this);

			//The three locks that make this a drawing and not an
			//editor. Cleared here rather than asked of those classes,
			//because on the layout scene they have to be set: the same
			//item is draggable there and inert here, and which of the
			//two it is belongs to whoever puts it on a scene.
		part->setFlag(QGraphicsItem::ItemIsMovable, false);
		part->setFlag(QGraphicsItem::ItemIsSelectable, false);
		part->setAcceptedMouseButtons(Qt::NoButton);

		m_parts << part;
	}
}

/**
	@brief MountingLayoutViewItem::clearParts
*/
void MountingLayoutViewItem::clearParts()
{
	const QVector<MountedPartItem *> parts = m_parts;
	m_parts.clear();

	for (MountedPartItem *part : parts)
	{
		delete part;
	}
}

/**
	@brief MountingLayoutViewItem::toXml
	@param document parent document
	@return the node that describes this item

	Strict where the read is tolerant: everything this item owns is written,
	every time. What is deliberately not written is the face itself - not
	one millimetre of it. The layout is written once, by the project, and a
	copy of it here would be a second answer that goes stale the first time
	somebody moves a breaker.
*/
QDomElement MountingLayoutViewItem::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	element.setAttribute(QStringLiteral("x"), QString::number(pos().x()));
	element.setAttribute(QStringLiteral("y"), QString::number(pos().y()));
	element.setAttribute(QStringLiteral("surface"), m_surface_uuid);
	element.setAttribute(QStringLiteral("scale"),
			     QString::number(m_drawing_scale, 'g', 10));
	return element;
}

/**
	@brief MountingLayoutViewItem::fromXml
	@param element the node written by toXml
	@return true when the node was one of ours
*/
bool MountingLayoutViewItem::fromXml(const QDomElement &element)
{
	if (element.tagName() != tagName()) {
		return false;
	}

	bool read_x = false;
	bool read_y = false;
	const double x = element.attribute(QStringLiteral("x")).toDouble(&read_x);
	const double y = element.attribute(QStringLiteral("y")).toDouble(&read_y);
	setPos(read_x && std::isfinite(x) ? x : 10.0,
	       read_y && std::isfinite(y) ? y : 10.0);

	bool read_scale = false;
	const double scale_ = element.attribute(QStringLiteral("scale"))
			      .toDouble(&read_scale);
	setDrawingScale(read_scale ? qreal(scale_) : defaultDrawingScale());

	setSurfaceUuid(element.attribute(QStringLiteral("surface")));

		//The face may not be there, and that is not a failure of the
		//read: a folio that names a plate somebody deleted has to open,
		//and say so on the sheet. Returning false here would make the
		//project drop the item and lose where it was posed.
	return true;
}

/**
	@brief MountingLayoutViewItem::tagName
	@return the tag one of these is written under
*/
QString MountingLayoutViewItem::tagName()
{
	return QStringLiteral("mounting_layout_view");
}

/**
	@brief MountingLayoutViewItem::defaultDrawingScale
	@return the factor an item starts life with, folio units per millimetre
*/
qreal MountingLayoutViewItem::defaultDrawingScale()
{
	return 0.5;
}

/**
	@brief MountingLayoutViewItem::minimumDrawingScale
	@return the smallest factor that still draws something

	A thousandth of a folio unit per millimetre: a plate of two metres drawn
	two units wide. Far below anything useful, and that is the point - the
	bound is here to keep a zero or a negative out of the transform, not to
	have an opinion about how small a drawing may be.
*/
qreal MountingLayoutViewItem::minimumDrawingScale()
{
	return 0.001;
}

/**
	@brief MountingLayoutViewItem::maximumDrawingScale
	@return the largest factor accepted
*/
qreal MountingLayoutViewItem::maximumDrawingScale()
{
	return 1000.0;
}

/**
	@brief MountingLayoutViewItem::scaleThatFits
	@param plate_width  millimetre
	@param plate_height millimetre
	@param folio the area the drawing has to fit inside, folio units
	@return the largest scale at which the plate fits, rounded down to a
	step a person would say out loud

	Rounding down, and to a coarse step, is the whole point of the
	function. The exact ratio of two measurements is a number like
	1.0666666, and a drawing handed to a workshop at 1.0666666 folio units
	per millimetre is a drawing nobody can check with a rule. The steps
	below are the ones a person reads off a scale bar, and the answer is
	always the largest of them that still fits - never a value between
	two.

	A plate too large for even the smallest step still gets that smallest
	step, and will hang off the folio. Refusing to answer would be worse:
	the person put the plate down on purpose, and a drawing that overruns
	the border is visible, while a dialog that declines is a dead end.
*/
qreal MountingLayoutViewItem::scaleThatFits(qreal plate_width,
					    qreal plate_height,
					    const QRectF &folio)
{
		//Nothing measured on one side or the other: there is no ratio to
		//take, and the default is the honest answer.
	if (plate_width <= 0.0 || plate_height <= 0.0
	    || folio.width() <= 0.0 || folio.height() <= 0.0)
	{
		return defaultDrawingScale();
	}

		//The steps, largest first. Reading order is the search order:
		//the first one that fits is the answer.
	static const qreal steps[] =
		{4.0, 3.0, 2.0, 1.5, 1.2, 1.0, 0.8, 0.75, 0.6, 0.5,
		 0.4, 0.3, 0.25, 0.2, 0.15, 0.1, 0.05};

		//Room is left around the drawing, because a plate that ends
		//exactly on the border reads as a plate that was cut off by it.
	const qreal usable_width = folio.width() * 0.95;
	const qreal usable_height = folio.height() * 0.95;

	for (const qreal step : steps)
	{
		if (plate_width * step <= usable_width
		    && plate_height * step <= usable_height)
		{
			return step;
		}
	}

	return steps[sizeof(steps) / sizeof(steps[0]) - 1];
}
