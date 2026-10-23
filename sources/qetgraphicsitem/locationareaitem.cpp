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
#include "../TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "../diagram.h"
#include "../diagramview.h"
#include "../elementsmover.h"
#include "../location/locatableelement.h"
#include "../location/locationtree.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "ViewItem/mountinglayoutviewitem.h"
#include "ViewItem/qetgraphicstableitem.h"
#include "diagramimageitem.h"
#include "element.h"
#include "independenttextitem.h"

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
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
		@brief The faint wash inside an area of the project, when nobody
		chose another fill for it.
	*/
	QColor areaWash()
	{
		QColor wash{areaInk()};
		wash.setAlpha(fill_alpha);
		return wash;
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

	One exception since T85, and it is the fill: an area whose location is
	out of the project - the field, another panel - is a mask over what is
	not ours, light grey by default, and its colour and transparency are
	chosen in the properties of the area and saved with it (maskColor).

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

	setBrush(QBrush(areaWash()));

	setZValue(-10);
}

/**
	@brief LocationAreaItem::setMaskColor
	@param color the fill of this area, invalid for the default one
*/
void LocationAreaItem::setMaskColor(const QColor &color)
{
	if (m_mask_color == color) {
		return;
	}
	m_mask_color = color;
	update();
}

/**
	@brief LocationAreaItem::defaultMaskColor
	@return the fill of an area out of the project nobody chose another
	fill for: light grey, half see-through, so that the symbols and the
	wires drawn over it read as they do on white paper
*/
QColor LocationAreaItem::defaultMaskColor()
{
	return QColor(192, 192, 192, 128);
}

/**
	@brief LocationAreaItem::isOutsideProject
	@return true when the location of this area is out of the project
	(LocationTree::isOutsideProject), asked of the tree each time: marking
	the location in the manager is what turns the area into a mask
*/
bool LocationAreaItem::isOutsideProject() const
{
	const Diagram *diagram_ptr = diagram();
	if (!diagram_ptr || !diagram_ptr->project()) {
		return false;
	}
	return diagram_ptr->project()->locationTree().isOutsideProject(m_location_path);
}

/**
	@brief LocationAreaItem::fillColor
	@return what the inside of the area is painted with: the colour chosen
	for it, else the grey mask when it is out of the project, else the
	faint wash of every area
*/
QColor LocationAreaItem::fillColor() const
{
	if (m_mask_color.isValid()) {
		return m_mask_color;
	}
	return isOutsideProject() ? defaultMaskColor() : areaWash();
}

/**
	@brief LocationAreaItem::refreshAll
	@param diagram the folio whose areas are painted again
	The tree of locations changed: an area may have become a mask or
	stopped being one, and its caption may name another thing, of another
	width - hence the geometry and not only the picture.
*/
void LocationAreaItem::refreshAll(Diagram *diagram)
{
	if (!diagram) {
		return;
	}
	const QList<QGraphicsItem *> items{diagram->items()};
	for (QGraphicsItem *item : items)
	{
		if (auto *area = qgraphicsitem_cast<LocationAreaItem *>(item))
		{
			area->prepareGeometryChange();
			area->update();
		}
	}
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

	Reached by the context menu of the area, and it is the only way in. Not by
	the double click any more, which the base class wires to this method:
	since T85 the double click puts the area in edition
	(mouseDoubleClickEvent()). An area is not a shape to the editor, so the
	shape properties action stays switched off for it and there is no docked
	editor that knows about one.

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

	QString path{m_location_path};
	QColor mask{m_mask_color};
	if (!askForProperties(diagram_ptr, dialog_parent, &path, &mask)) {
		return;
	}
	applyProperties(path, mask);
}

/**
	@brief LocationAreaItem::applyProperties
	@param path the location chosen for this area, empty for none
	@param mask the fill chosen for it, invalid for the default one

	What editProperty() does once the person has answered: one undo step
	for the two, whichever of them changed. Apart from the dialog so that
	the bench, which cannot answer a modal window, proves all of the rest.
*/
void LocationAreaItem::applyProperties(const QString &path, const QColor &mask)
{
	Diagram *diagram_ptr = diagram();
	if (!diagram_ptr || diagram_ptr->isReadOnly()) {
		return;
	}

	const bool path_changed{path != m_location_path};
	const bool mask_changed{mask != m_mask_color};
	if (!path_changed && !mask_changed) {
		return;
	}

	auto *undo = new QUndoCommand(path_changed
				      ? tr("Modifier la localisation d'une zone")
				      : tr("Change the mask of an area"));

	if (mask_changed)
	{
		const QColor old_mask{m_mask_color};
		setMaskColor(mask);
		new QPropertyUndoCommand(this, "maskColor", old_mask, mask, undo);
	}

	if (path_changed)
	{
		const QString old_path{m_location_path};

		//Written onto the area before the rule is asked anything. The rule
		//reads the areas off the folio rather than being told about them, so
		//the folio has to already say what the person just chose.
		setLocationPath(path);
		new QPropertyUndoCommand(this, "locationPath", old_path, path, undo);

		const QList<LocationAssignment> assignments{
			pendingAssignments(diagram_ptr, areasOf(diagram_ptr))};

		//An empty list is not an empty command, it is no command at all: the
		//rule hands back nothing for a component already carrying the right
		//path, which is the same test the command itself applies, so an empty
		//list here means the command would have counted zero components and
		//said so.
		if (!assignments.isEmpty())
		{
			new AssignLocationCommand(assignments,
						  tr("Affecter les composants d'une zone"),
						  undo);
		}
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
	an area behind the drawing; and the mask, the other one (T85), only when
	somebody chose it, as #AARRGGBB - a file written before keeps opening
	with the default fill.

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
	if (m_mask_color.isValid()) {
		result.setAttribute("mask", m_mask_color.name(QColor::HexArgb));
	}

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
	const QString mask{e.attribute("mask")};
	setMaskColor(mask.isEmpty() ? QColor() : QColor::fromString(mask));

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
	locationChoices(tree, &labels, &paths);

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
	@brief LocationAreaItem::locationChoices
	@param tree the locations of the project
	@param labels filled with what the person reads, one per choice
	@param paths filled with the path of each choice, empty for none

	The first choice is always no location. The code is shown beside the
	name because the code is what the caption of the area will say on the
	paper, and because it is what tells two locations somebody named alike
	apart. It is derived from the path, so the labels are exactly as unique
	as the paths are, and looking a label up cannot land on the wrong one.
	A location out of the project says so (T85): choosing it turns the area
	into a mask.
*/
void LocationAreaItem::locationChoices(const LocationTree &tree,
				       QStringList *labels,
				       QStringList *paths)
{
	*labels << tr("(sans localisation)");
	*paths  << QString();

	const QStringList tree_paths{tree.paths()};
	for (const QString &path : tree_paths)
	{
		const QString uuid{tree.uuidOfPath(path)};
		const QString display{uuid.isEmpty() ? path : tree.displayPath(uuid)};

		QString label{QStringLiteral("%1  (%2)")
			      .arg(display, LocationTree::iecTag(path))};
		if (tree.isOutsideProject(path)) {
			label += QStringLiteral("  - ") + tr("out of project");
		}
		*labels << label;
		*paths  << path;
	}
}

/**
	@brief LocationAreaItem::askForProperties
	@param diagram the folio the area belongs to
	@param parent widget the dialogue belongs to, may be nullptr
	@param path the location of the area on the way in, the one chosen on
	the way out
	@param mask its fill on the way in, invalid for the default one, and
	the one chosen on the way out
	@return true when the person pressed OK

	The properties of an area: its location, as askForPath() offers it, and
	its fill - the colour and the transparency of the mask (T85), in the
	colour dialogue with its alpha channel, and a button back to the
	default, which follows the location: grey for a location out of the
	project, the faint wash for the others.
*/
bool LocationAreaItem::askForProperties(const Diagram *diagram,
					QWidget *parent,
					QString *path,
					QColor *mask)
{
	if (!diagram || !diagram->project() || !path || !mask) {
		return false;
	}

	const LocationTree tree{diagram->project()->locationTree()};
	QStringList labels;
	QStringList paths;
	locationChoices(tree, &labels, &paths);

	QDialog dialog(parent);
	dialog.setWindowTitle(tr("Zone de localisation"));

	auto *combo = new QComboBox(&dialog);
	combo->addItems(labels);
	const int index = paths.indexOf(*path);
	combo->setCurrentIndex(index < 0 ? 0 : index);

	QColor chosen{*mask};
	auto *color_button = new QPushButton(&dialog);
	auto *default_button = new QPushButton(tr("Default"), &dialog);

	const auto shown = [&]() -> QColor {
		if (chosen.isValid()) {
			return chosen;
		}
		return tree.isOutsideProject(paths.value(combo->currentIndex()))
			? defaultMaskColor()
			: areaWash();
	};
	const auto refresh = [&]() {
		QPixmap swatch(32, 16);
		swatch.fill(Qt::white);
		QPainter painter(&swatch);
		painter.fillRect(swatch.rect(), shown());
		painter.setPen(Qt::black);
		painter.drawRect(swatch.rect().adjusted(0, 0, -1, -1));
		painter.end();
		color_button->setIcon(QIcon(swatch));
		color_button->setText(chosen.isValid()
				      ? tr("%1 % opaque").arg(qRound(chosen.alphaF() * 100))
				      : tr("Default"));
		default_button->setEnabled(chosen.isValid());
	};

	QObject::connect(color_button, &QPushButton::clicked, &dialog, [&]() {
		const QColor picked{QColorDialog::getColor(
				shown(), &dialog, tr("Mask colour"),
				QColorDialog::ShowAlphaChannel)};
		if (picked.isValid()) {
			chosen = picked;
			refresh();
		}
	});
	QObject::connect(default_button, &QPushButton::clicked, &dialog, [&]() {
		chosen = QColor();
		refresh();
	});
	QObject::connect(combo, &QComboBox::currentIndexChanged, &dialog, [&]() {
		refresh();
	});
	refresh();

	auto *mask_row = new QHBoxLayout;
	mask_row->addWidget(color_button, 1);
	mask_row->addWidget(default_button);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
					     | QDialogButtonBox::Cancel, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	auto *form = new QFormLayout(&dialog);
	form->addRow(tr("Localisation de cette zone :"), combo);
	form->addRow(tr("Mask:"), mask_row);
	form->addRow(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}

	*path = paths.value(combo->currentIndex());
	*mask = chosen;
	return true;
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
	painter->setBrush(QBrush(fillColor()));
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

	Where the press was, and where the area stood, are kept for the drag
	that may follow, in either way of dragging it (mouseMoveEvent()).
*/
void LocationAreaItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	m_carrying = false;
	m_press_scene_pos = event->scenePos();
	m_pos_from = pos();
	QetGraphicsItem::mousePressEvent(event);
}

/**
	@brief LocationAreaItem::mouseMoveEvent
	@param event the move

	The drag of an area is the drag of what is in it (T85, asked by the
	Renan on 23/10/2026): the components, with their wires, the texts, the
	shapes, the tables and the areas inside it go with the rectangle, by the
	movement of the folio - the very one a selection dragged by hand goes
	through, which stretches a wire that leaves the area and carries whole
	the one whose two ends move. In edition (setEditing()) the area goes
	alone, as a drag of any item does.

	Nothing moves before the hand has gone the distance a drag starts at:
	the press of a double click must not shake the content.
*/
void LocationAreaItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_editing)
	{
		QetShapeItem::mouseMoveEvent(event);
		return;
	}

	Diagram *folio = diagram();
	if (!(event->buttons() & Qt::LeftButton) || !folio || folio->isReadOnly()
	    || !isMovable() || !isSelected())
	{
		event->ignore();
		return;
	}
	event->accept();

	if (!m_carrying)
	{
		if ((event->scenePos() - m_press_scene_pos).manhattanLength()
		    < QApplication::startDragDistance()) {
			return;
		}
		if (!beginCarry()) {
			return;
		}
		m_carrying = true;
	}
	carryTo(event->scenePos());
}

/**
	@brief LocationAreaItem::mouseReleaseEvent
	@param event the release

	The end of a drag with the content is one undo step (endCarry()). The end
	of a drag in edition is the end of the movement of the folio, with a step
	of its own for the position of the area handed to it: that movement does
	not take an area back, and before T85 undoing such a drag put the
	components back on their old location while leaving the rectangle where
	it had been dragged.
*/
void LocationAreaItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_carrying)
	{
		m_carrying = false;
		endCarry();
		event->accept();
		return;
	}

	Diagram *folio = diagram();
	if (m_editing && folio && !folio->elementsMover().isReady()
	    && pos() != m_pos_from)
	{
			//A parent with no text of its own, so that it is never merged
			//with another step; the movement names the whole step
		auto *step = new QUndoCommand();
		new QPropertyUndoCommand(this, "pos", m_pos_from, pos(), step);
		folio->elementsMover().addExtraCommands({step});
	}
	QetShapeItem::mouseReleaseEvent(event);
}

/**
	@brief LocationAreaItem::mouseDoubleClickEvent
	@param event the double click

	Puts the area in edition, where its handles show and a drag moves it
	alone. It used to open the properties of the area, which are in its
	context menu since: the drag with the content became the drag of an
	area, and the drag of the rectangle alone needed a way in that a hand
	finds without a menu.
*/
void LocationAreaItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
	{
		setEditing(true);
			//A drag that follows the second press moves the area alone,
			//from where it stands now
		m_first_move = true;
		m_carrying = false;
		m_press_scene_pos = event->scenePos();
		m_pos_from = pos();
	}
	event->accept();
}

/**
	@brief LocationAreaItem::setEditing
	@param editing true to edit the area alone, false to leave the edition

	In edition the area is selected and shows the handles of the shapes it
	is built on; out of it, selected or not, it shows none, and a drag takes
	its content along. Leaving the selection leaves the edition
	(itemChange()), which is what a click elsewhere on the folio and Escape
	do. A folio that cannot be changed has no edition to offer.

	The handles are made by QetShapeItem when it is selected. An area already
	selected is told it was selected again, which builds them, and told it
	was deselected to take them off while it stays selected.
*/
void LocationAreaItem::setEditing(bool editing)
{
	if (editing == m_editing) {
		return;
	}

	if (editing)
	{
		const Diagram *folio = diagram();
		if (!folio || folio->isReadOnly()) {
			return;
		}
		m_editing = true;
		if (isSelected()) {
			QetShapeItem::itemChange(ItemSelectedHasChanged, true);
		} else {
			setSelected(true);
		}
	}
	else
	{
		m_editing = false;
		if (isSelected()) {
			QetShapeItem::itemChange(ItemSelectedHasChanged, false);
		}
	}

	updateToolTip();
	update();
}

/**
	@brief LocationAreaItem::itemChange
	@param change what changed
	@param value its new value
	@return what QGraphicsItem::itemChange returns

	Selected out of edition, the area skips what the shapes do when they are
	selected - build their handles -: its handles are those of the edition.
	Deselected, it leaves the edition, and the shapes take the handles off.
*/
QVariant LocationAreaItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemSelectedHasChanged)
	{
		if (value.toBool() && !m_editing)
		{
			updateToolTip();
			return QGraphicsItem::itemChange(change, value);
		}
		if (!value.toBool()) {
			m_editing = false;
		}
		const QVariant result{QetShapeItem::itemChange(change, value)};
		updateToolTip();
		return result;
	}
	return QetShapeItem::itemChange(change, value);
}

/**
	@brief LocationAreaItem::hoverEnterEvent
	@param event the hover
	Skips the shapes, whose hints tell of a click cycling the modes of the
	handles, which an area does not do (mousePressEvent()). Its own hint is
	its tooltip (updateToolTip()). Leaving it goes through the shapes still,
	which clear the hint a handle put on the status bar.
*/
void LocationAreaItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	QetGraphicsItem::hoverEnterEvent(event);
}

/**
	@brief LocationAreaItem::updateToolTip
	What a drag of the area does, once it is selected: with its content, or
	alone in edition. Nothing while it is not selected, as the shapes.
*/
void LocationAreaItem::updateToolTip()
{
	if (!isSelected()) {
		setToolTip(QString());
	} else if (m_editing) {
		setToolTip(tr("Drag the border to move the area alone, a handle to resize it"));
	} else {
		setToolTip(tr("Drag to move the area with what is in it, double-click to edit it alone"));
	}
}

/**
	@brief LocationAreaItem::contextMenuEvent
	@param event the request of the menu

	The properties of the area - its location and its mask -, which the
	double click opened before T85, the edition of the area alone, and the
	commands of the folio on the selection, as the shapes give them. Not the
	conversion to a polyline and the mirrors the menu of the shapes offered
	an area: a converted area is no longer a rectangle, and toXml() writes
	one.
*/
void LocationAreaItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	Diagram *folio = diagram();
	if (!folio)
	{
		event->ignore();
		return;
	}

	if (!isSelected())
	{
		folio->clearSelection();
		setSelected(true);
	}

	QMenu menu;
	QAction *properties = menu.addAction(tr("Area properties..."));
	QAction *edit = menu.addAction(tr("Edit the area alone"));
	properties->setEnabled(!folio->isReadOnly());
	edit->setEnabled(!folio->isReadOnly() && !m_editing);

	QWidget *viewport = event->widget();
	auto *view = viewport ? qobject_cast<DiagramView *>(viewport->parentWidget())
			      : nullptr;
	if (view)
	{
		menu.addSeparator();
		menu.addActions(view->contextMenuActions());
	}

	QAction *chosen = menu.exec(event->screenPos());
	if (chosen == properties) {
		editProperty();
	} else if (chosen == edit) {
		setEditing(true);
	}
	event->accept();
}

/**
	@brief LocationAreaItem::carriedItems
	@return what a drag of this area takes along with it

	Inside means what it means for a location: the centre inside the
	rectangle, by the rule of locationcontainment. Carrying by another rule
	would let a drag leave behind a component the area holds, and the
	component would lose its location for having stayed where it was.
	Another area goes along when it lies wholly inside this one - a
	sub-plate in its enclosure -, and not by its centre: two areas drawn
	across each other are two places, not one in the other.

	The items of the folio itself only - a text of a component goes with its
	component -, and those that can move: a locked one stays, and is not
	handed to the movement, whose undo would otherwise move what never
	moved. No wire in the list: the movement takes those whose two ends
	move, and stretches those with an end left behind.
*/
QList<QGraphicsItem *> LocationAreaItem::carriedItems() const
{
	QList<QGraphicsItem *> carried;
	const Diagram *folio = diagram();
	if (!folio) {
		return carried;
	}

	const QRectF area{sceneRect()};
	const QList<QGraphicsItem *> candidates{
		folio->items(area, Qt::IntersectsItemBoundingRect)};
	for (QGraphicsItem *item : candidates)
	{
		if (item == this || item->parentItem()) {
			continue;
		}

		switch (item->type())
		{
			case LocationAreaItem::Type:
			{
				const auto *inner = qgraphicsitem_cast<LocationAreaItem *>(item);
				const QRectF inner_rect{inner->sceneRect()};
				if (inner->isMovable() && inner_rect != area
				    && area.contains(inner_rect)) {
					carried << item;
				}
				break;
			}
			case Element::Type:
			case DiagramImageItem::Type:
			case QetShapeItem::Type:
			case QetGraphicsTableItem::Type:
			case TerminalStripItem::Type:
			case MountingLayoutViewItem::Type:
			{
				const auto *movable = dynamic_cast<const QetGraphicsItem *>(item);
				if (movable && movable->isMovable()
				    && area.contains(item->sceneBoundingRect().center())) {
					carried << item;
				}
				break;
			}
			case IndependentTextItem::Type:
				if ((item->flags() & QGraphicsItem::ItemIsMovable)
				    && area.contains(item->sceneBoundingRect().center())) {
					carried << item;
				}
				break;
			default:
				break;
		}
	}

	return carried;
}

/**
	@brief LocationAreaItem::selectOnly
	@param items what is selected with this area

	This area and @a items selected, and nothing else: the movement of the
	folio moves the selection. In one change of the selection, as
	Diagram::completeGroupSelection() does it: the properties dock rebuilds
	itself on each, and an enclosure can hold a hundred components.
*/
void LocationAreaItem::selectOnly(const QList<QGraphicsItem *> &items)
{
	Diagram *folio = diagram();
	if (!folio) {
		return;
	}

	const QSet<QGraphicsItem *> kept(items.cbegin(), items.cend());
	const QList<QGraphicsItem *> selected{folio->selectedItems()};
	const bool was_blocked{folio->blockSignals(true)};
	for (QGraphicsItem *item : selected)
	{
		if (item != this && !kept.contains(item)) {
			item->setSelected(false);
		}
	}
	setSelected(true);
	for (QGraphicsItem *item : items) {
		item->setSelected(true);
	}
	folio->blockSignals(was_blocked);
	if (!was_blocked) {
		emit folio->selectionChanged();
	}
}

/**
	@brief LocationAreaItem::beginCarry
	@return false when the movement of the folio could not begin

	The content selected with the area, and the movement of the folio begun
	on it, the area driving it as any item dragged by hand does.

	The movement moves the components, the texts, the shapes and the
	pictures, and not the areas, the tables, the drawn strips and the views
	of a plate: those are moved here by the same amount (carryTo()). The
	tables, the strips and the views are taken back by the step of the
	movement, which counts them; the areas by a step of their own (endCarry()).

	The first component found is the one the others keep their distance
	from on the grid, as the first terminal of a strip frame is
	(StripFrameItem::mouseMoveEvent()).
*/
bool LocationAreaItem::beginCarry()
{
	Diagram *folio = diagram();
	if (!folio) {
		return false;
	}

	const QList<QGraphicsItem *> carried{carriedItems()};
	m_anchor.clear();
	m_followers.clear();
	for (QGraphicsItem *item : carried)
	{
		if (!m_anchor)
		{
			if (Element *element = qgraphicsitem_cast<Element *>(item))
			{
				m_anchor = element;
				m_anchor_from = element->pos();
			}
		}

		switch (item->type())
		{
			case LocationAreaItem::Type:
			case QetGraphicsTableItem::Type:
			case TerminalStripItem::Type:
			case MountingLayoutViewItem::Type:
				m_followers.append(Follower{item->toGraphicsObject(), item->pos()});
				break;
			default:
				break;
		}
	}
	m_pos_from = pos();

	selectOnly(carried);
	if (folio->elementsMover().beginMovement(folio, this) < 0)
	{
		selectOnly({});
		m_anchor.clear();
		m_followers.clear();
		return false;
	}
	return true;
}

/**
	@brief LocationAreaItem::carryTo
	@param scene_pos where the mouse is

	The first component, or the area when it holds none, put on the grid
	where the mouse took it, and the rest of the content by the same: the
	movement of the folio moves what it moves, and the area and the items it
	leaves alone go by what the first component went by - the pull of a
	terminal onto the line of its wire included
	(ElementsMover::continueMovement()), or the area would leave its content
	a fraction of a step behind.

	The positions are written past QetGraphicsItem::setPos(), which puts
	each on the grid by itself: what is written here is an amount the
	content already went by, and must not be rounded again.
*/
void LocationAreaItem::carryTo(const QPointF &scene_pos)
{
	Diagram *folio = diagram();
	if (!folio) {
		return;
	}

	const QPointF reference_from{m_anchor ? m_anchor_from : m_pos_from};
	const QPointF reference_now{m_anchor ? m_anchor->pos() : pos()};
	const QPointF wanted{Diagram::snapToGrid(reference_from + scene_pos - m_press_scene_pos)};
	folio->elementsMover().continueMovement(wanted - reference_now);

	const QPointF moved{m_anchor ? m_anchor->pos() - m_anchor_from
				     : wanted - m_pos_from};
	QGraphicsItem::setPos(m_pos_from + moved);
	for (const Follower &follower : std::as_const(m_followers))
	{
		if (follower.item) {
			follower.item->setPos(follower.from + moved);
		}
	}
}

/**
	@brief LocationAreaItem::endCarry
	The end of a drag with the content: one undo step for all of it, and the
	area alone selected again, as after a click on it.

	The areas moved - this one and those inside it - are written in a step
	handed to the movement of the folio, which ends it with its own: one
	undo takes back the rectangles, the content and the locations the drag
	changed together.

	The project is told to make no wire while the movement ends. A movement
	of one component alone joins it to any free terminal it lands in line
	with (ElementsMover::endMovement()); an area holding one component is
	no component placed by hand, and a wire drawn by moving a rectangle
	would be a wire nobody asked for.
*/
void LocationAreaItem::endCarry()
{
	Diagram *folio = diagram();
	if (!folio) {
		return;
	}

		//A parent with no text of its own, so that it is never merged with
		//another step; the movement names the whole step
	auto *areas_step = new QUndoCommand();
	if (pos() != m_pos_from) {
		new QPropertyUndoCommand(this, "pos", m_pos_from, pos(), areas_step);
	}
	for (const Follower &follower : std::as_const(m_followers))
	{
		if (follower.item && follower.item->type() == LocationAreaItem::Type
		    && follower.item->pos() != follower.from)
		{
			new QPropertyUndoCommand(follower.item.data(), "pos", follower.from,
						 follower.item->pos(), areas_step);
		}
	}
	if (areas_step->childCount()) {
		folio->elementsMover().addExtraCommands({areas_step});
	} else {
		delete areas_step;
	}

	QETProject *project = folio->project();
	const bool auto_conductor{project && project->autoConductor()};
	if (auto_conductor) {
		project->setAutoConductor(false);
	}
	folio->elementsMover().endMovement();
	if (auto_conductor) {
		project->setAutoConductor(true);
	}

	selectOnly({});
	m_anchor.clear();
	m_followers.clear();
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

	An area out of the project (T85) says its name and not its code: it is
	no place of this panel, so it has no designation on the paper, and what
	the person wrote in the free text it replaces was a name - "Elementos de
	Campo", or the tag of another panel, which a code could not even hold.
	The code when the location has no name.
*/
QString LocationAreaItem::captionText() const
{
	if (m_location_path.isEmpty()) {
		return tr("(sans localisation)");
	}

	if (isOutsideProject())
	{
		const LocationTree tree{diagram()->project()->locationTree()};
		const QString name{tree.location(tree.uuidOfPath(m_location_path)).name.trimmed()};
		if (!name.isEmpty()) {
			return name;
		}
		const QStringList codes{LocationTree::splitPath(m_location_path)};
		return codes.isEmpty() ? m_location_path : codes.last();
	}

	return LocationTree::iecTag(m_location_path);
}
