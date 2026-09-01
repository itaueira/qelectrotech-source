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
#include "locationareaitem.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../diagram.h"
#include "../location/locatableelement.h"
#include "../location/locationtree.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "element.h"

#include <QDomDocument>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QUndoStack>

namespace
{
		///How wide the grabbable band along the border is, in scene units
	constexpr qreal grab_band_width{8.0};
		///How thick the border line is drawn
	constexpr qreal border_width{1.0};
		///How far the caption text sits from the edge of its plate
	constexpr qreal caption_padding{1.6};
		///How see-through the wash inside the area is, out of 255
	constexpr int fill_alpha{26};
		///The size of the caption, in points, fixed on purpose - see below
	constexpr qreal caption_point_size{7.0};

	/**
		@brief The ink an area is drawn in.
		@return a dark blue grey

		Dark enough to survive a black and white print, and far enough from
		the black of the symbols and the red of a selection that an area does
		not read as part of the circuit.
	*/
	QColor areaInk()
	{
		return QColor(60, 84, 120);
	}

	/**
		@brief The font the caption is written in.
		@return a fixed sans serif, bold, seven points

		Deliberately not QETApp::diagramTextsFont, which every other text on
		the folio uses. That one is a setting, and it can be turned down to
		four points; the labels it governs are labels somebody can zoom into.
		This one is the only thing on the paper that says which cabinet the
		rectangle is, and it has to be readable on a dense folio that came
		out of a printer. The cost is that it does not follow the font the
		person chose for the rest of the drawing, and cannot be made bigger.
	*/
	QFont captionFont()
	{
		QFont font(QStringLiteral("Liberation Sans"));
		font.setStyleHint(QFont::SansSerif);
		font.setPointSizeF(caption_point_size);
		font.setBold(true);
		return font;
	}
}

/**
	@brief LocationAreaItem::LocationAreaItem
	@param p1 first corner, in scene coordinates
	@param p2 opposite corner, in scene coordinates
	@param parent parent item

	The pen and the brush are set here and are not part of the saved file: an
	area is a piece of notation and not a piece of drawing, so every area of
	every project looks the same. The cost is the obvious one - a person
	cannot restyle one area and have it stay restyled, and there is no shape
	style editor to reach it from either, since an area is not a shape.

	The stacking order starts low, below the elements at 10 and the
	conductors at 11, because the area is the paper the circuit is drawn on
	and not something drawn over it. That costs one thing worth knowing
	before chasing it as a bug: the selection handles inherited from the
	shapes sit at zValue() + 1, so they are below the components too, and a
	handle that falls exactly on a symbol cannot be grabbed. The corners of
	an enclosure are usually empty paper, which is what makes the price
	acceptable, and the depth actions can raise a particular area when it is
	not.
*/
LocationAreaItem::LocationAreaItem(const QPointF &p1, const QPointF &p2, QGraphicsItem *parent) :
	QetShapeItem(p1, p2, QetShapeItem::Rectangle, parent)
{
	QPen border_pen;
	border_pen.setStyle(Qt::DashLine);
	border_pen.setWidthF(border_width);
	border_pen.setColor(areaInk());
	setPen(border_pen);

	QColor wash{areaInk()};
	wash.setAlpha(fill_alpha);
	setBrush(QBrush(wash));

	setZValue(-10);
}

/**
	@brief LocationAreaItem::setLocationPath
	@param path the path of codes this area stands for, empty for none

	The caption is part of the geometry and not only of the picture: its
	plate is as wide as the text in it, so a new path is a new bounding
	rectangle. Hence prepareGeometryChange and not a bare update.
*/
void LocationAreaItem::setLocationPath(const QString &path)
{
	if (m_location_path == path) {
		return;
	}

	prepareGeometryChange();
	m_location_path = path;
	emit locationPathChanged();
	update();
}

/**
	@brief LocationAreaItem::sceneRect
	@return the rectangle this area covers, in scene coordinates, straightened

	The rule and everything that reads it work in scene coordinates, because
	an area and the components it holds are siblings on the folio and not
	parent and child. rect() is in item coordinates and may well arrive with
	a negative width - somebody dragged right to left - so the conversion
	lives here once rather than once per caller.

	This is the rectangle the area draws, and it is not sceneBoundingRect().
	That one is grown by the grab band and by the caption plate, ten units or
	so beyond the drawn line, and using it to decide what is inside an area
	would quietly swallow the components sitting just outside the border.
*/
QRectF LocationAreaItem::sceneRect() const
{
	return mapToScene(rect().normalized()).boundingRect();
}

/**
	@brief LocationAreaItem::name
	@return what this item is called in an undo caption

	The indefinite article is part of the string because the captions are
	built by concatenation - "Ajouter " + name() - and the French article
	cannot be guessed from outside the word.
*/
QString LocationAreaItem::name() const
{
	return tr("une zone de localisation");
}

/**
	@brief LocationAreaItem::editProperty
	Ask which location this area stands for, and follow through.

	Reached by a double click, which the base class wires to this method, and
	it is the only way in: an area is not a shape to the editor, so the shape
	properties action stays switched off for it and there is no docked editor
	that knows about one.

	It has to do two things at once - write the new path onto the area, and
	move the components that were inside it onto that path - and they go into
	a single undo command, because they are one thing the person did.

	The children are ordered so that undo runs backwards through them: the
	components are put back first, then the path of the area. Any other order
	would restore the components against a rectangle that had already changed
	its mind.
*/
void LocationAreaItem::editProperty()
{
	Diagram *diagram_ptr = diagram();
	if (!diagram_ptr || diagram_ptr->isReadOnly()) {
		return;
	}

	QWidget *dialog_parent = diagram_ptr->views().isEmpty()
				 ? nullptr
				 : diagram_ptr->views().first();

	bool accepted = false;
	const QString path{askForPath(diagram_ptr, m_location_path, dialog_parent, &accepted)};
	if (!accepted || path == m_location_path) {
		return;
	}

	const QString old_path{m_location_path};

	//Written onto the area before the rule is asked anything. The rule reads
	//the areas off the folio rather than being told about them, so the folio
	//has to already say what the person just chose.
	setLocationPath(path);

	auto *undo = new QUndoCommand(tr("Modifier la localisation d'une zone"));
	new QPropertyUndoCommand(this, "locationPath", old_path, path, undo);

	const QList<LocationAssignment> assignments{
		pendingAssignments(diagram_ptr, areasOf(diagram_ptr))};

	//An empty list is not an empty command, it is no command at all: the rule
	//hands back nothing for a component already carrying the right path,
	//which is the same test the command itself applies, so an empty list here
	//means the command would have counted zero components and said so.
	if (!assignments.isEmpty())
	{
		new AssignLocationCommand(assignments,
					  tr("Affecter les composants d'une zone"),
					  undo);
	}

	diagram_ptr->undoStack().push(undo);
}

/**
	@brief LocationAreaItem::toXml
	@param document parent xml document
	@return the element this area is saved as

	Its own tag, not the shape one, and only four coordinates, a path and a
	stacking order in it. Nothing about the pen, the brush or the corner
	radius, because none of those is a property of an area - see the
	constructor for what that costs. The stacking order is saved because it
	is the one piece of appearance a person can change on purpose, by sending
	an area behind the drawing.

	The rectangle is straightened on the way out, unlike the shapes, which
	store the two corners in the order they were clicked. A file that always
	holds a valid rectangle is a file the next reader does not have to know
	the rule about.
*/
QDomElement LocationAreaItem::toXml(QDomDocument &document) const
{
	QDomElement result = document.createElement(tagName());

	const QRectF area{sceneRect()};
	result.setAttribute("x1", QString::number(area.left()));
	result.setAttribute("y1", QString::number(area.top()));
	result.setAttribute("x2", QString::number(area.right()));
	result.setAttribute("y2", QString::number(area.bottom()));

	//Spelled the same as the element information key the components carry,
	//on purpose: one word for one idea, for whoever opens the .qet by hand.
	result.setAttribute("location_path", m_location_path);
	result.setAttribute("z", QString::number(zValue()));

	return result;
}

/**
	@brief LocationAreaItem::fromXml
	@param e element to read this area from
	@return true when the element was an area and was read

	The coordinates are in scene space and are written straight into the
	item, which only works while the item sits at the origin. The position is
	therefore set explicitly rather than assumed: the shapes rely on the same
	thing without saying so, and it is worth one line to make it true instead
	of lucky.
*/
bool LocationAreaItem::fromXml(const QDomElement &e)
{
	if (e.tagName() != tagName()) {
		return false;
	}

	const QPointF p1(e.attribute("x1").toDouble(), e.attribute("y1").toDouble());
	const QPointF p2(e.attribute("x2").toDouble(), e.attribute("y2").toDouble());

	setPos(0, 0);
	setRect(QRectF(p1, p2).normalized());
	setLocationPath(e.attribute("location_path"));
	setZValue(e.attribute("z", QString::number(zValue())).toDouble());

	return true;
}

/**
	@brief LocationAreaItem::shape
	@return a band along the border, plus the caption plate - not the inside

	This is the decision the whole item stands on. An area is drawn around a
	cabinet, and a cabinet is full of components: were the shape the filled
	rectangle, the area would be the topmost thing under the cursor across
	its whole surface, and nobody could click a component again. So the hit
	area is the border itself, widened into a band a few units across so that
	a hand can catch it, and the caption plate, which is the one spot where
	grabbing the area is what a person means by clicking there.

	The cost is worth knowing before hunting it as a bug: an area can only be
	selected and dragged by its border or its caption. A click in the middle
	of one reaches whatever is drawn there, or nothing at all.

	The band keeps a fixed width and does not swell on hover, unlike the
	shapes this inherits from. Areas nest, and a sub-plate is drawn against
	the wall of the enclosure holding it; two bands that grew under the cursor
	would trade places while the hand was still moving.
*/
QPainterPath LocationAreaItem::shape() const
{
	QPainterPath border;
	border.addRect(rect().normalized());

	QPainterPathStroker stroker;
	stroker.setWidth(grab_band_width);
	stroker.setJoinStyle(Qt::MiterJoin);

	QPainterPath path{stroker.createStroke(border)};
	path.addRect(captionRect());

	return path;
}

/**
	@brief LocationAreaItem::tagName
	@return the xml tag an area is stored under
*/
QString LocationAreaItem::tagName()
{
	return QStringLiteral("location_area");
}

/**
	@brief LocationAreaItem::areasOf
	@param diagram the folio to look at, or nullptr
	@return every area drawn on it, as the containment rule wants them

	The order is whatever the scene gives, which is deliberate: the rule
	settles a tie between two equal areas by stacking order and not by
	position in the list, precisely so that this function does not have to
	promise an order it cannot keep.
*/
QVector<LocationArea> LocationAreaItem::areasOf(const Diagram *diagram)
{
	QVector<LocationArea> areas;
	if (!diagram) {
		return areas;
	}

	const QList<QGraphicsItem *> items{diagram->items()};
	for (QGraphicsItem *item : items)
	{
		auto *area = qgraphicsitem_cast<LocationAreaItem *>(item);
		if (!area) {
			continue;
		}

		LocationArea entry;
		entry.rect = area->sceneRect();
		entry.path = area->locationPath();
		entry.z = area->zValue();
		areas.append(entry);
	}

	return areas;
}

/**
	@brief LocationAreaItem::pendingAssignments
	@param diagram the folio whose components are being judged
	@param areas the areas to judge them against, from areasOf
	@return one assignment per component whose path has to change

	This is the translation layer, and the only place that knows both halves.
	The rule works on opaque integer identifiers so that it can be tested
	without a scene; the identifiers handed to it here are indices into a
	list of Element pointers kept alongside, and the answer is turned back
	into pointers by the same indices.

	An empty result means nothing to do, not nothing found. A component that
	already carries the right path produces no entry, which is what lets the
	caller push no command at all.
*/
QList<LocationAssignment> LocationAreaItem::pendingAssignments(
		const Diagram *diagram,
		const QVector<LocationArea> &areas)
{
	QList<LocationAssignment> assignments;
	if (!diagram) {
		return assignments;
	}

	QList<Element *> elements;
	QVector<LocatableItem> locatables;

	const QList<Element *> folio_elements{diagram->elements()};
	for (Element *element : folio_elements)
	{
		if (!isLocatableElement(element)) {
			continue;
		}

		LocatableItem locatable;
		locatable.id = elements.size();
		//The bounding rectangle of a symbol holds the symbol and none of its
		//labels, which is what makes its centre the point the rule wants: a
		//long cross reference hanging off a contact must not drag the
		//component it belongs to into the next cabinet.
		locatable.rect = element->sceneBoundingRect();
		locatable.path = element->elementInformations()
				 .value(QETInformation::ELMT_LOCATION_PATH).toString();

		elements.append(element);
		locatables.append(locatable);
	}

	const QVector<LocationContainmentChange> changes{
		locationContainmentChanges(areas, locatables)};

	for (const LocationContainmentChange &change : changes)
	{
		if (change.id < 0 || change.id >= elements.size()) {
			continue;
		}

		LocationAssignment assignment;
		assignment.element = elements.at(change.id);
		assignment.path = change.path;
		assignments.append(assignment);
	}

	return assignments;
}

/**
	@brief LocationAreaItem::askForPath
	@param diagram the folio the area belongs to
	@param current the path to start on, empty for none
	@param parent widget the dialogue belongs to, may be nullptr
	@param accepted set to true only when the person chose something
	@return the chosen path, empty for an area left unassigned

	One list of the locations the project already has, and no way to type a
	new one. Creating a location is the manager's business, and a text field
	here would let somebody invent a code by hand that no location in the
	tree answers to - a path written onto components that leads nowhere.

	Leaving the area unassigned is offered as a choice rather than reached by
	cancelling, because an area drawn before its enclosure has been given a
	code is a legitimate state. Cancelling means the person did not mean to
	draw the rectangle at all, and the caller reads it that way.

	When the project has no locations yet, this says so and accepts nothing.
	That is the honest answer: a rectangle nobody can name is a rectangle
	that will sit on the folio doing nothing, and creating one in silence is
	how a person ends up with a drawing they cannot explain.
*/
QString LocationAreaItem::askForPath(const Diagram *diagram,
				     const QString &current,
				     QWidget *parent,
				     bool *accepted)
{
	if (accepted) {
		*accepted = false;
	}

	if (!diagram || !diagram->project()) {
		return QString();
	}

	const LocationTree tree{diagram->project()->locationTree()};

	if (tree.isEmpty())
	{
		QMessageBox::information(
				parent,
				tr("Aucune localisation"),
				tr("Ce projet n'a encore aucune localisation.\n"
				   "Definissez-en une dans le gestionnaire de "
				   "localisations avant de dessiner une zone."));
		return QString();
	}

	QStringList labels;
	QStringList paths;

	labels << tr("(sans localisation)");
	paths  << QString();

	const QStringList tree_paths{tree.paths()};
	for (const QString &path : tree_paths)
	{
		const QString uuid{tree.uuidOfPath(path)};
		const QString display{uuid.isEmpty() ? path : tree.displayPath(uuid)};

		//The code is shown beside the name because the code is what the
		//caption of the area will say on the paper, and because it is what
		//tells two locations somebody named alike apart. It is derived from
		//the path, so the labels are exactly as unique as the paths are, and
		//looking a label up below cannot land on the wrong one.
		labels << QStringLiteral("%1  (%2)")
			  .arg(display, LocationTree::iecTag(path));
		paths  << path;
	}

	int index = paths.indexOf(current);
	if (index < 0) {
		index = 0;
	}

	bool ok = false;
	const QString chosen{QInputDialog::getItem(
				parent,
				tr("Zone de localisation"),
				tr("Localisation de cette zone :"),
				labels,
				index,
				false,
				&ok)};
	if (!ok) {
		return QString();
	}

	const int chosen_index = labels.indexOf(chosen);
	if (chosen_index < 0) {
		return QString();
	}

	if (accepted) {
		*accepted = true;
	}

	return paths.at(chosen_index);
}

/**
	@brief LocationAreaItem::paint
	@param painter painter to use
	@param option unused
	@param widget unused

	Three layers, in the order somebody looking at the paper reads them: the
	wash, the border, the caption.

	The wash is kept faint on purpose. An enclosure can cover most of a
	folio, and a fill dense enough to be obvious on screen is a folio that
	comes out of a black and white printer grey all over, with the circuit
	inside it a shade harder to read than it was. The border carries the
	information instead, and the wash only says where the border closes.

	The caption sits on an opaque plate, black on white, and that is the part
	that answers to legibility before style: it is the only thing on the
	drawing that names the cabinet, and a translucent label over a bundle of
	conductors is a label nobody reads twice.
*/
void LocationAreaItem::paint(QPainter *painter,
			     const QStyleOptionGraphicsItem *option,
			     QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	const QRectF area{rect().normalized()};

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(Qt::NoPen);
	painter->setBrush(brush());
	painter->drawRect(area);

	//Solid and thicker while hovered or selected. The border is the only
	//place the area can be grabbed, so it has to be findable by hand; the
	//dashes are what keep it from reading as a rectangle somebody drew.
	QPen border_pen{pen()};
	if (isSelected() || isHovered())
	{
		border_pen.setStyle(Qt::SolidLine);
		border_pen.setWidthF(border_pen.widthF() + border_width);
	}
	painter->setPen(border_pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawRect(area);

	const QRectF plate{captionRect()};

	painter->setBrush(QBrush(Qt::white));
	painter->setPen(QPen(areaInk(), 0));
	painter->drawRect(plate);

	painter->setFont(captionFont());
	painter->setPen(QPen(Qt::black));
	painter->drawText(plate, Qt::AlignCenter, captionText());

	painter->restore();
}

/**
	@brief LocationAreaItem::mousePressEvent
	@param event the press

	Deliberately skips QetShapeItem::mousePressEvent and calls its
	grandparent. The one it skips cycles the resize mode of the shapes on
	every left click, and the third mode of a rectangle drives the corner
	radius - which an area does not save. A click would then change the
	drawing in a way the file cannot remember, which is the worst kind of
	edit: the person watches it happen and it is gone next time they open
	the project.

	The price is that an area resizes by its corners and edges only, the
	first mode: no mirror resize, no rounded corner.
*/
void LocationAreaItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	QetGraphicsItem::mousePressEvent(event);
}

/**
	@brief LocationAreaItem::captionRect
	@return the plate the caption is written on, in item coordinates

	Anchored inside the top left corner. Inside rather than above, because
	areas nest and their top edges very nearly coincide: a plate sitting
	above the line would look like it named whatever is above it. The cost of
	being inside is the one the low stacking order implies - a component
	drawn in that corner covers the plate, since the area is under the
	components by design.

	It is allowed to hang over the right edge of a very small area rather
	than cut the text. A caption reading "+QCM..." is worse than one poking
	out of its rectangle.
*/
QRectF LocationAreaItem::captionRect() const
{
	const QRectF area{rect().normalized()};
	const QFontMetricsF metrics{captionFont()};

	const qreal width{metrics.horizontalAdvance(captionText()) + 2 * caption_padding};
	const qreal height{metrics.height() + 2 * caption_padding};

	return QRectF(area.topLeft(), QSizeF(width, height));
}

/**
	@brief LocationAreaItem::captionText
	@return what the corner of the area says

	The norm spelling of the path and not the names behind it, because that
	is the designation the folio is supposed to carry and the one the
	storeroom reads back off it. The cost is that the caption shows codes: an
	enclosure named "Armoire principale" whose code is QCM1 reads +QCM1 on
	the paper. The names stay where they are useful, in the picker and in the
	manager.

	An area with no path says so instead of saying nothing. Unnamed is a
	state somebody chose, and an empty corner reads as a defect.
*/
QString LocationAreaItem::captionText() const
{
	if (m_location_path.isEmpty()) {
		return tr("(sans localisation)");
	}

	return LocationTree::iecTag(m_location_path);
}
