/*
 *   Copyright 2006-2026 The QElectroTech Team
 *   This file is part of QElectroTech.
 */
#include "diagrameventaddmacro.h"

#include "../diagram.h"
#include "../qetapp.h"
#include "../qetdiagrameditor.h"
#include "../qetproject.h"
#include "../ElementsCollection/xmlelementcollection.h"
#include "../NameList/nameslist.h"
#include "../diagramcommands.h"
#include "../diagramcontent.h"
#include "../macro/macrosubstitution.h"
#include <QFile>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QMessageBox>
#include <QStatusBar>
#include <QPainter>

DiagramEventAddMacro::DiagramEventAddMacro(const ElementsLocation &location, Diagram *diagram, QPointF pos) :
DiagramEventInterface(diagram),
m_location(location),
m_preview_item(nullptr)
{
	if (loadMacro()) {
		init();

		QScopedPointer<QETProject> dummy_project(new QETProject());
		QDomElement root = m_macro_doc.documentElement();

		QDomElement collection_node = root.firstChildElement("collection");
		if (!collection_node.isNull()) {
			QDomNodeList elements = collection_node.elementsByTagName("element");
			for (int i = 0; i < elements.count(); ++i) {
				QDomElement elmt_node = elements.at(i).toElement();
				QString path = elmt_node.attribute("path");
				QDomElement definition = elmt_node.firstChildElement("definition");

				if (!path.isEmpty() && !definition.isNull()) {
					int last_slash = path.lastIndexOf('/');
					QString dir_path = (last_slash != -1) ? path.left(last_slash) : "";
					QString file_name = (last_slash != -1) ? path.mid(last_slash + 1) : path;

					if (!dir_path.isEmpty()) {
						QStringList parts = dir_path.split('/', Qt::SkipEmptyParts);
						QString current_path = "";
						for (const QString &part : parts) {
							QString parent_path = current_path;
							if (!current_path.isEmpty()) current_path += "/";
							current_path += part;
							if (current_path == "import") continue;
							NamesList empty_names;
							dummy_project->embeddedElementCollection()->createDir(parent_path, part, empty_names);
						}
					}
					dummy_project->embeddedElementCollection()->addElementDefinition(dir_path, file_name, definition);
				}
			}
		}

		Diagram *dummy_diagram = dummy_project->addNewDiagram();
		QDomElement diagram_node = root.firstChildElement("diagram_content").firstChildElement("diagram");

		if (!diagram_node.isNull()) {
			dummy_diagram->fromXml(diagram_node, QPointF(0, 0), false, nullptr);

			QRectF scene_rect = dummy_diagram->itemsBoundingRect();
			if (!scene_rect.isEmpty()) {
				QPixmap pixmap(scene_rect.toAlignedRect().size());
				pixmap.fill(Qt::transparent);
				QPainter painter(&pixmap);
				painter.setRenderHint(QPainter::Antialiasing);
				dummy_diagram->render(&painter, QRectF(QPointF(0,0), scene_rect.size()), scene_rect);

				m_preview_item = new QGraphicsPixmapItem(pixmap);
				m_preview_item->setOffset(scene_rect.topLeft());
			}
		}

		if (m_preview_item) {
			m_preview_item->setPos(Diagram::snapToGrid(pos));
			m_preview_item->setOpacity(0.6);
			m_diagram->addItem(m_preview_item);
			m_running = true;
		}

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

DiagramEventAddMacro::~DiagramEventAddMacro()
{
	if (m_preview_item) {
		m_diagram->removeItem(m_preview_item);
		delete m_preview_item;
	}

	if (m_status_bar) {
		m_status_bar->clearMessage();
	}

	for (auto view : m_diagram->views())
		view->setContextMenuPolicy(Qt::DefaultContextMenu);
}

void DiagramEventAddMacro::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_preview_item) {
		const auto pos_{Diagram::snapToGrid(event->scenePos())};
		m_preview_item->setPos(pos_);

		if (m_status_bar) {
			m_status_bar->showMessage(QString("x %1 : y %2 (Makro-Anker)").arg(QString::number(pos_.x()), QString::number(pos_.y())));
		}
	}
	event->setAccepted(true);
}

void DiagramEventAddMacro::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	event->setAccepted(true);
}

void DiagramEventAddMacro::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_preview_item) {
		if (event->button() == Qt::RightButton) {
			m_diagram->removeItem(m_preview_item);
			delete m_preview_item;
			m_preview_item = nullptr;
			m_running = false;
			emit finish();
		}
		else if (event->button() == Qt::LeftButton) {
			if (!addMacro(Diagram::snapToGrid(event->scenePos()))) {
					//Nothing was inserted, and the user has been told why.
					//Staying would only repeat the same refusal at every
					//click, since nothing here can change the values.
				m_diagram->removeItem(m_preview_item);
				delete m_preview_item;
				m_preview_item = nullptr;
				m_running = false;
				emit finish();
			}
		}
	}
	event->setAccepted(true);
}

void DiagramEventAddMacro::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_preview_item && (event->button() == Qt::LeftButton)) {
		m_diagram->removeItem(m_preview_item);
		delete m_preview_item;
		m_preview_item = nullptr;
		m_running = false;
		emit finish();
	}
	event->setAccepted(true);
}

void DiagramEventAddMacro::keyPressEvent(QKeyEvent *event)
{
	DiagramEventInterface::keyPressEvent(event);
}

void DiagramEventAddMacro::init()
{
	foreach(QGraphicsView *view, m_diagram->views())
		view->setContextMenuPolicy(Qt::NoContextMenu);
}

bool DiagramEventAddMacro::loadMacro()
{
	QFile file(m_location.fileSystemPath());
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qDebug() << "Error: Macro file could not be read:" << m_location.fileSystemPath();
		return false;
	}

	if (!m_macro_doc.setContent(&file)) {
		qDebug() << "Error: Invalid XML in macro.";
		return false;
	}

	QDomElement root = m_macro_doc.documentElement();
	if (root.tagName() != "qet_macro") return false;

		//A macro written before this existed carries no <parameters>, and
		//reading none is not an error: that is every macro made so far.
	m_parameters.fromXml(root);
		//Until the dialogue of T06 asks the user, the values are the ones
		//the macro itself declares as default.
	m_values = m_parameters.defaults();

	QDomElement collection_node = root.firstChildElement("collection");
	if (!collection_node.isNull()) {
		QDomNodeList elements = collection_node.elementsByTagName("element");
		for (int i = 0; i < elements.count(); ++i) {
			QDomElement elmt_node = elements.at(i).toElement();
			QString path = elmt_node.attribute("path");
			QDomElement definition = elmt_node.firstChildElement("definition");

			if (!path.isEmpty() && !definition.isNull()) {
				int last_slash = path.lastIndexOf('/');
				QString dir_path = (last_slash != -1) ? path.left(last_slash) : "";
				QString file_name = (last_slash != -1) ? path.mid(last_slash + 1) : path;

				if (!dir_path.isEmpty()) {
					QStringList parts = dir_path.split('/', Qt::SkipEmptyParts);
					QString current_path = "";
					for (const QString &part : parts) {
						QString parent_path = current_path;
						if (!current_path.isEmpty()) current_path += "/";
						current_path += part;
						if (current_path == "import") continue;
						NamesList empty_names;
						m_diagram->project()->embeddedElementCollection()->createDir(parent_path, part, empty_names);
					}
				}
				m_diagram->project()->embeddedElementCollection()->addElementDefinition(dir_path, file_name, definition);
			}
		}
	}

	QDomElement diagram_node = root.firstChildElement("diagram_content").firstChildElement("diagram");
	if (!diagram_node.isNull()) {
		QDomNodeList instances = diagram_node.elementsByTagName("element");
		for (int i = 0; i < instances.count(); ++i) {
			QDomElement inst = instances.at(i).toElement();
			QString type = inst.attribute("type");
			if (type.startsWith("macro://")) {
				inst.setAttribute("type", type.replace("macro://", "embed://"));
			}
		}
	}

	return true;
}

/**
	@brief Insert the macro at @a final_pos, once its variables resolve.
	@param final_pos
	@return false when nothing was inserted, the user having been told why.
	Half a circuit is worse than none, because half a circuit looks finished.
*/
bool DiagramEventAddMacro::addMacro(QPointF final_pos)
{
	QDomElement root = m_macro_doc.documentElement();
	QDomElement diagram_node = root.firstChildElement("diagram_content").firstChildElement("diagram");

	if (diagram_node.isNull()) {
		return false;
	}

	QDomElement cloned_node = diagram_node.cloneNode(true).toElement();

		//A macro that declares nothing is left strictly alone: not scanned,
		//not audited, not touched. That is what keeps every macro made before
		//this existed inserting exactly the drawing it inserted yesterday.
	if (!m_parameters.isEmpty()) {
		const QStringList missing = m_parameters.missingRequired(m_values);
		if (!missing.isEmpty()) {
			QStringList shown;
			shown.reserve(missing.size());
			for (const QString &name : missing) {
				const MacroParameter parameter = m_parameters.parameter(name);
				shown << (parameter.label.isEmpty() ? name : parameter.label);
			}
			reportRefusal(shown.size() == 1
				      ? tr("La variable %1 est obligatoire et n'a pas de valeur.")
						.arg(shown.first())
				      : tr("Ces variables sont obligatoires et n'ont pas de valeur : %1.")
						.arg(shown.join(QStringLiteral(", "))));
			return false;
		}

			//The substitution happens on the clone, between it and fromXml:
			//the document kept in memory stays the macro as it was read, so
			//a second insertion starts from the markers and not from the
			//values the first one wrote.
		const MacroSubstitution::Result result = MacroSubstitution::apply(cloned_node, m_values);
		if (!result.ok) {
			reportRefusal(result.errorText());
			return false;
		}
	}

	QPointF target_pos = final_pos;

	DiagramContent pasted_content;

	m_diagram->fromXml(cloned_node, target_pos, false, &pasted_content);
	m_diagram->refreshContents();

	m_diagram->undoStack().push(new PasteDiagramCommand(m_diagram, pasted_content));
	return true;
}

/**
	@brief Say out loud why nothing was inserted.
	@param text
	The status bar alone would not do: the next mouse move writes the
	coordinates over it, and a refusal nobody sees reads as "nothing
	happened" - the same complaint as a circuit inserted half done.
*/
void DiagramEventAddMacro::reportRefusal(const QString &text)
{
	if (text.isEmpty()) {
		return;
	}

	if (m_status_bar) {
		m_status_bar->showMessage(text);
	}

	QWidget *parent_widget = m_diagram->views().isEmpty() ? nullptr
							      : m_diagram->views().first();
	QMessageBox::warning(parent_widget, tr("Insertion impossible"), text);
}
