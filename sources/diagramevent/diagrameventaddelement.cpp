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
#include "diagrameventaddelement.h"
#include "../qetproject.h"
#include "../conductorautonumerotation.h"
#include "../diagram.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "../undocommand/deleteqgraphicsitemcommand.h"
#include "../ElementsCollection/xmlelementcollection.h"
#include "../factory/elementfactory.h"
#include "../qetapp.h"
#include "../qetdiagrameditor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qet.h"
#include "../autobreakconductor.h"
#include <QPainterPath>
#include <QStatusBar>
#include <limits>

namespace
{
		/// How long the status bar says why an element was not created:
		/// longer than a hint, since nothing else on the folio shows that
		/// the drop did nothing
	const int failure_message_timeout = 15000;

	/**
		The categories of @p collection that importing @p location would
		create, because they do not exist yet: the folders of its path under
		the import category, outermost first. XmlElementCollection::addElement()
		creates them as it walks the path, without a signal, and leaves them
		behind when it fails half way.
	*/
	QStringList missingImportDirectories(const XmlElementCollection *collection,
					     const ElementsLocation &location)
	{
		QStringList missing;
		const QDomElement import_category = collection->importCategory();
		if (import_category.isNull()) {
			return missing;
		}

		QStringList folders = location.collectionPath(false).split(
					QLatin1Char('/'), Qt::SkipEmptyParts);
		if (!folders.isEmpty()) {
			folders.removeLast(); //The symbol itself
		}
		QString path = import_category.attribute(QStringLiteral("name"));
		for (const QString &folder : folders)
		{
			path += QLatin1Char('/') + folder;
			if (!collection->exist(path)) {
				missing << path;
			}
		}
		return missing;
	}
}

/**
	@brief DiagramEventAddElement::DiagramEventAddElement
	Defaut constructor
	@param location :location of diagram
	@param diagram : diagram owner of this event
	@param pos : first pos of item ( optional, by defaut QPointF(0,0) )
*/
DiagramEventAddElement::DiagramEventAddElement(ElementsLocation &location, Diagram *diagram, QPointF pos) :
	DiagramEventInterface(diagram),
	m_location(location),
	m_element(nullptr)
{
		//Check if there is an element at this location
	if (location.isElement() && location.exist())
	{
			//location is an element, we build it, if build fail,
			//m_running stay to false (by default), so this interface will be deleted at next event
		if (buildElement())
		{
			init();
			m_element -> setPos(pos);
			m_element -> displayHelpLine(true);
			m_element -> setFlag(QGraphicsItem::ItemIsSelectable, false);
			m_diagram -> addItem(m_element);
			m_running = true;

			if (!diagram->views().isEmpty()) {
				const auto qde = QETApp::diagramEditorAncestorOf(diagram->views().at(0));
				if (qde) {
					m_status_bar = qde->statusBar();
				}
			} else {
				m_status_bar.clear();
			}
		}
	}
	else {
			//What ElementFactory::createElement() says of such a location
		reportFailure(1);
	}
}

/**
	@brief DiagramEventAddElement::~DiagramEventAddElement
	Destructor
	Enable context menu for each view of diagram
*/
DiagramEventAddElement::~DiagramEventAddElement()
{
	if (m_element)
	{
		m_diagram->removeItem(m_element);
		m_element->deleteLater();
	}

	if (m_status_bar) {
		m_status_bar->clearMessage();
	}

	for (auto view : m_diagram->views())
		view -> setContextMenuPolicy(Qt::DefaultContextMenu);
}

/**
	@brief DiagramEventAddElement::mouseMoveEvent
	Move the element to new pos of mouse
	the event is always accepted, unless nothing could be built
	(see endIfNothingToPlace())
	@param event
*/
void DiagramEventAddElement::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (endIfNothingToPlace()) {
		return;
	}

	if (m_element)
	{
		const auto pos_{Diagram::snapToGrid(event->scenePos())};
		m_element->setPos(pos_);

		if (m_status_bar) {
			m_status_bar->showMessage(QString("x %1 : y %2").arg(QString::number(pos_.x()), QString::number(pos_.y())));
		}
	}
	event->setAccepted(true);
}

/**
	@brief DiagramEventAddElement::mousePressEvent
	Do nothing, but return true for not transit the event to other thing in diagram.
	the event is always accepted, unless nothing could be built
	(see endIfNothingToPlace())
	@param event
*/
void DiagramEventAddElement::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	if (endIfNothingToPlace()) {
		return;
	}
	event->setAccepted(true);
}

/**
	@brief DiagramEventAddElement::mouseReleaseEvent
	Right button finish this event (isRunning = false) and emit finish.
	Left button add an element to diagram
	the event is always accepted, unless nothing could be built
	(see endIfNothingToPlace())
	@param event
*/
void DiagramEventAddElement::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (endIfNothingToPlace()) {
		return;
	}

	if (m_element)
	{
		if (event->button() == Qt::RightButton)
		{
			m_diagram->removeItem(m_element);
			m_element->deleteLater();
			m_element = nullptr;
			m_running = false;
			emit finish();
		}
		else if (event->button() == Qt::LeftButton)
		{
			addElement();
		}
	}

	event->setAccepted(true);
}

/**
	@brief DiagramEventAddElement::mouseDoubleClickEvent
	If mouse left double clic, finish this event (isRunning = false) and emit finish
	the event is always accepted, unless nothing could be built
	(see endIfNothingToPlace())
	@param event
*/
void DiagramEventAddElement::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (endIfNothingToPlace()) {
		return;
	}

	if (m_element && (event -> button() == Qt::LeftButton))
	{
		m_diagram->removeItem(m_element);
		m_element->deleteLater();
		m_element = nullptr;
		m_running = false;
		emit finish();
	}
	
	event->setAccepted(true);
}

/**
	@brief DiagramEventAddElement::keyPressEvent
	Press space key rotate the element to 90° (return true)
	else  call DiagramEventInterface::keyPressEvent(event), and return the value.
	@param event
*/
void DiagramEventAddElement::keyPressEvent(QKeyEvent *event)
{
	if (m_element && event->key() == Qt::Key_Space)
	{
		m_element->setRotation(m_element->rotation() + 90);
		event->setAccepted(true);
	}
	else {
		DiagramEventInterface::keyPressEvent(event);
	}
}

/**
	@brief DiagramEventAddElement::init
	Init this event.
*/
void DiagramEventAddElement::init()
{
	foreach(QGraphicsView *view, m_diagram->views())
		view->setContextMenuPolicy(Qt::NoContextMenu);
}

/**
	@brief DiagramEventAddElement::buildElement
	Build the element, if the element is build successfully, we return true, otherwise false.
	On failure, what the import of this attempt added to the project is
	taken out again, and the reason is reported (see reportFailure()).
*/
bool DiagramEventAddElement::buildElement()
{
	XmlElementCollection *collection = m_diagram->project()->embeddedElementCollection();

		//What this attempt adds to the project, to be taken out again if
		//the symbol cannot become an element: a definition the project
		//already had is not touched
	const QStringList new_directories = missingImportDirectories(collection, m_location);
	QStringList added_elements;
	const QMetaObject::Connection watch = connect(
				collection, &XmlElementCollection::elementAdded, this,
				[&added_elements](const QString &path) {added_elements << path;});
	ElementsLocation import_loc = m_diagram->project()->importElement(m_location);
	disconnect(watch);

	int state = ImportFailed;
	if (import_loc.exist())
	{
		const QString integrate_path = import_loc.projectCollectionPath();
		ElementsLocation loc(integrate_path);
		m_element = ElementFactory::Instance() -> createElement(loc, nullptr, &state);
		if (!state) {
				//Everything is good
			m_integrate_path = integrate_path;
			return true;
		}
			//The creation of element failed, we delete it. The pointer is
			//cleared too: the destructor and every mouse event test it
		delete m_element;
		m_element = nullptr;
	}

	undoImport(added_elements, new_directories);
	reportFailure(state);
	return false;
}

/**
	@brief DiagramEventAddElement::endIfNothingToPlace
	An element that could not be built leaves nothing to place: the first
	mouse event ends this interface and is left to the folio, instead of
	being swallowed by an interface that does nothing until Esc is pressed.
	@return true if this interface has ended. It may be deleted by then
	(Diagram deletes it on finish()), so the caller returns at once.
*/
bool DiagramEventAddElement::endIfNothingToPlace()
{
	if (m_element) {
		return false;
	}
	m_running = false;
	emit finish();
	return true;
}

/**
	@brief DiagramEventAddElement::undoImport
	Take out of the embedded collection of the project what an import that
	did not end in an element added to it: the definitions it added, then
	the folders it created, if nothing else is in them.
	@param added_elements : the collection paths of the added definitions
	@param new_directories : the folders that did not exist before the
	import, outermost first
*/
void DiagramEventAddElement::undoImport(const QStringList &added_elements,
					const QStringList &new_directories)
{
	XmlElementCollection *collection = m_diagram->project()->embeddedElementCollection();
	for (const QString &path : added_elements) {
		collection->removeElement(path);
	}
	for (auto it = new_directories.crbegin() ; it != new_directories.crend() ; ++it)
	{
		const QDomElement directory = collection->directory(*it);
		if (!directory.isNull()
			&& collection->elements(directory).isEmpty()
			&& collection->directories(directory).isEmpty()) {
			collection->removeDir(*it);
		}
	}
}

/**
	@brief DiagramEventAddElement::reportFailure
	Say that no element was created from m_location, and why: in the log,
	with the path of the symbol, and in the status bar of the editor.
	@param state : the state of the build, see failureReason()
*/
void DiagramEventAddElement::reportFailure(int state) const
{
	const QString reason = failureReason(state);
	QString symbol = m_location.toString();
	if (!m_location.fileSystemPath().isEmpty()) {
		symbol += QStringLiteral(" (") + m_location.fileSystemPath() + QStringLiteral(")");
	}
	qWarning().noquote() << QStringLiteral("Element not created from %1: %2 (state %3)")
				.arg(symbol, reason, QString::number(state));

	if (!m_diagram || m_diagram->views().isEmpty()) {
		return;
	}
	if (const auto qde = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst())) {
		qde->statusBar()->showMessage(
					tr("Element \"%1\" not created: %2.").arg(m_location.name(), reason),
					failure_message_timeout);
	}
}

/**
	@brief DiagramEventAddElement::failureReason
	@param state : the state ElementFactory::createElement() gave
	(see Element::buildFromXml()), or ImportFailed
	@return why the symbol did not become an element, in a few words
*/
QString DiagramEventAddElement::failureReason(int state)
{
	switch (state)
	{
		case ImportFailed:
			return tr("the symbol could not be copied into the project");
		case 1:
			return tr("the symbol was not found");
		case 4:
			return tr("the file is not a symbol definition");
		case 5:
			return tr("the symbol has no valid width, height or insertion point");
		case 6:
			return tr("the symbol definition is empty");
		case 7:
			return tr("a part of the symbol could not be read");
		case 8:
			return tr("the symbol has nothing to draw");
		default:
			return tr("unknown error %1").arg(state);
	}
}

/**
	@brief DiagramEventAddElement::addElement
	Add an element at the current pos and current rotation,
	if project autoconductor option is enable, and the element can be wired, we do it.
*/
void DiagramEventAddElement::addElement()
{
	int state;
	Element *element;
	if (m_integrate_path.isEmpty())
		element = ElementFactory::Instance() -> createElement(m_location, nullptr, &state);
	else
		element = ElementFactory::Instance() -> createElement(ElementsLocation(m_integrate_path), nullptr, &state);

		//Build failed
	if (state)
	{
		delete element;
		reportFailure(state);
		return;
	}

		//We must add item to scene (even if addItemCommand do this)
		//for create the autoconnection below
	element -> setPos(m_element->pos());
	element -> setRotation(m_element -> rotation());
	m_diagram -> addItem(element);

	QUndoCommand *undo_object = new QUndoCommand(tr("insert %1").arg(element->name()));
	new AddGraphicsObjectCommand(element, m_diagram, m_element -> pos(), undo_object);

	//When we search for free aligned terminal we temporally remove m_element to
	//avoid any interaction with the function Element::AlignedFreeTerminals
	//This is useful when an element has two (or more) terminals on opposite sides,
	//because m_element is exactly at the same pos of the new element
	//added to the scene so new conductor are created between terminal of the new element
	//and the opposite terminal of m_element.
	m_diagram->removeItem(m_element);

		//Auto break conductor: if a terminal of the new element lies on an existing
		//conductor, break the conductor and reconnect through the new element's terminal.
		//Track the endpoints of broken conductors so auto-connect doesn't create duplicates.
	QList<Conductor *> conductors_handled;
	QSet<Terminal *> used_terminals;
	QSet<Terminal *> broken_endpoints = autoBreakConductors(m_diagram, element, undo_object,
							    conductors_handled, used_terminals);

		//Auto-connect: collect all aligned pairs first, then filter and process.
	QList<QPair<Terminal *, Terminal *>> aligned_pairs;
	if (m_diagram->project()->autoConductor())
		aligned_pairs = element->AlignedFreeTerminals();

	for (const QPair<Terminal *, Terminal *> &pair : aligned_pairs)
	{
			//Skip if the other terminal was an endpoint of a broken conductor
		if (broken_endpoints.contains(pair.second))
			continue;

		Conductor *conductor = new Conductor(pair.first, pair.second);
		new AddGraphicsObjectCommand(conductor, m_diagram, QPointF(), undo_object);

			//Autonum the new conductor, the undo command associated for this, have for parent undo_object
		ConductorAutoNumerotation can  (conductor, m_diagram, undo_object);
		can.numerate();
		if (m_diagram->freezeNewConductors() || m_diagram->project()->isFreezeNewConductors()) {
			conductor->setFreezeLabel(true);
		}
	}
	m_diagram->addItem(m_element);

		//Autonum the new element before pushing undo_object, so the counter
		//change it triggers is part of the same undo macro as the element's
		//own placement (one Ctrl+Z reverts both, instead of silently
		//leaving the counter advanced).
	element->setUpFormula(true, undo_object);

	m_diagram -> undoStack().push(undo_object);
	element->freezeNewAddedElement();
}
