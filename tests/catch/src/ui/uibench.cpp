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
#include "uibench.h"

#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/diagramtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/independenttextitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetresult.h"
#include "../../../../sources/qetversion.h"

#include <QColor>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QPainter>
#include <QUndoStack>
#include <QVersionNumber>

#ifndef QET_EXAMPLES_DIR
#error "QET_EXAMPLES_DIR is not defined: the test target must set it, otherwise \
the fixtures would be looked up relative to whatever directory the suite is run from."
#endif

QString UiBench::examplePath(const QString &file_name)
{
	return QDir(QStringLiteral(QET_EXAMPLES_DIR)).absoluteFilePath(file_name);
}

bool UiBench::opensWithoutDialog(const QString &file_path, QString *reason)
{
	const auto refuse = [reason](const QString &text) {
		if (reason) {
			*reason = text;
		}
		return false;
	};

	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly)) {
		return refuse(QStringLiteral("cannot be read: %1").arg(file_path));
	}

	QDomDocument document;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
	if (const auto result = document.setContent(&file); !result) {
		return refuse(QStringLiteral("is not well-formed XML (line %1: %2): %3")
					  .arg(result.errorLine).arg(result.errorMessage, file_path));
	}
#else
	QString error;
	int line = 0;
	if (!document.setContent(&file, &error, &line)) {
		return refuse(QStringLiteral("is not well-formed XML (line %1: %2): %3")
					  .arg(line).arg(error, file_path));
	}
#endif

	const QDomElement root = document.documentElement();
	if (root.tagName() != QLatin1String("project")) {
		return refuse(QStringLiteral("is not a project (root element is <%1>): %2")
					  .arg(root.tagName(), file_path));
	}

	// The same call readProjectXml() makes. A project with no version
	// attribute at all takes neither branch, and so raises no dialog.
	const QVersionNumber version = QetVersion::fromXmlAttribute(root);
	if (version.isNull()) {
		return true;
	}

	if (QetVersion::currentVersion() < version) {
		return refuse(QStringLiteral("was saved by a newer version (%1 > %2) and would "
									 "raise the compatibility dialog: %3")
					  .arg(version.toString(),
						   QetVersion::currentVersion().toString(),
						   file_path));
	}

	if (version <= QetVersion::versionZeroDotSix()) {
		return refuse(QStringLiteral("comes from QElectroTech %1, which is 0.6 or lower, "
									 "and would raise the compatibility dialog: %2")
					  .arg(version.toString(), file_path));
	}

	return true;
}

UiBench::Project::Project(const QString &example_file_name)
{
	// The examples directory is part of the source tree: a test that opens a
	// file from it must not drop a backup file next to it. main.cpp does the
	// same for the command line export path.
	QETProject::setBackupEnabled(false);

	const QString path = examplePath(example_file_name);

	QString reason;
	if (!opensWithoutDialog(path, &reason)) {
		m_error = reason;
		return;
	}

	m_project = new QETProject(path);
	if (m_project->state() != QETProject::Ok) {
		m_error = QStringLiteral("opening returned state %1 for %2")
				  .arg(static_cast<int>(m_project->state())).arg(path);
	}
}

UiBench::Project::~Project()
{
	delete m_project;
}

bool UiBench::Project::isOpen() const
{
	return m_project && m_project->state() == QETProject::Ok;
}

QList<Diagram *> UiBench::Project::diagrams() const
{
	return m_project ? m_project->diagrams() : QList<Diagram *>();
}

Diagram *UiBench::Project::diagram(int index) const
{
	const QList<Diagram *> list = diagrams();
	return (index >= 0 && index < list.count()) ? list.at(index) : nullptr;
}

int UiBench::Project::diagramCount() const
{
	return diagrams().count();
}

Diagram *UiBench::Project::busiestSheet() const
{
	Diagram *best = nullptr;
	int best_count = 0;
	const QList<Diagram *> list = diagrams();
	for (Diagram *diagram : list) {
		if (!diagram) {
			continue;
		}
		// Strictly greater, so a tie keeps the earlier sheet and the answer
		// is the same on every run.
		const int count = diagram->elements().count();
		if (count > best_count) {
			best_count = count;
			best = diagram;
		}
	}
	return best;
}

QString UiBench::fileContent(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return QString();
	}
	return QString::fromUtf8(file.readAll());
}

UiBench::ScratchProject::ScratchProject(const QString &xml_content,
					const QString &file_name)
{
	QETProject::setBackupEnabled(false);

	if (!m_dir.isValid())
	{
		m_error = QStringLiteral("no temporary directory: %1").arg(m_dir.errorString());
		return;
	}

	m_file_path = QDir(m_dir.path()).absoluteFilePath(file_name);

	QFile file(m_file_path);
	if (!file.open(QIODevice::WriteOnly))
	{
		m_error = QStringLiteral("cannot write %1: %2")
			  .arg(m_file_path, file.errorString());
		return;
	}
	file.write(xml_content.toUtf8());
	file.close();

	open();
}

UiBench::ScratchProject::~ScratchProject()
{
	delete m_project;
}

void UiBench::ScratchProject::open()
{
	QString reason;
	if (!opensWithoutDialog(m_file_path, &reason))
	{
		m_error = reason;
		return;
	}

	m_project = new QETProject(m_file_path);
	if (m_project->state() != QETProject::Ok)
	{
		m_error = QStringLiteral("opening returned state %1 for %2")
			  .arg(static_cast<int>(m_project->state())).arg(m_file_path);
		return;
	}
	m_error.clear();
}

bool UiBench::ScratchProject::isOpen() const
{
	return m_project && m_project->state() == QETProject::Ok;
}

QList<Diagram *> UiBench::ScratchProject::diagrams() const
{
	return m_project ? m_project->diagrams() : QList<Diagram *>();
}

Diagram *UiBench::ScratchProject::diagram(int index) const
{
	const QList<Diagram *> list = diagrams();
	return (index >= 0 && index < list.count()) ? list.at(index) : nullptr;
}

int UiBench::ScratchProject::diagramCount() const
{
	return diagrams().count();
}

bool UiBench::ScratchProject::saveAndReopen()
{
	if (!m_project)
	{
		m_error = QStringLiteral("there is no project to save");
		return false;
	}

	const QETResult written = m_project->write();
	if (!written.isOk())
	{
		m_error = QStringLiteral("saving %1 failed: %2")
			  .arg(m_file_path, written.errorMessage());
		return false;
	}

	delete m_project;
	m_project = nullptr;
	open();
	return isOpen();
}

QStringList UiBench::displayedLabels(Diagram *diagram)
{
	QStringList labels;
	if (!diagram) {
		return labels;
	}

	const QList<Element *> elements = diagram->elements();
	for (Element *element : elements) {
		labels << element->displayedLabel();
	}
	return labels;
}

QStringList UiBench::textFields(Diagram *diagram)
{
	QStringList texts;
	if (!diagram) {
		return texts;
	}

	// DiagramContent holds the text fields in a QSet, whose iteration order
	// is not the order of the sheet and not stable between runs. Sorting is
	// what makes a comparison in a test mean something.
	const DiagramContent content = diagram->content();
	for (IndependentTextItem *item : content.m_text_fields) {
		texts << item->toPlainText();
	}
	texts.sort();
	return texts;
}

QStringList UiBench::information(Diagram *diagram, const QString &key)
{
	QStringList values;
	if (!diagram) {
		return values;
	}

	const QList<Element *> elements = diagram->elements();
	for (Element *element : elements) {
		values << element->elementInformations().value(key).toString();
	}
	return values;
}

QString UiBench::undoTopText(QETProject *project)
{
	if (!project) {
		return QString();
	}

	QUndoStack *stack = project->undoStack();
	if (!stack || stack->index() == 0) {
		return QString();
	}
	return stack->text(stack->index() - 1);
}

QImage UiBench::render(Diagram *diagram, int width)
{
	if (!diagram || width <= 0) {
		return QImage();
	}

	const QRectF scene_rect = diagram->sceneRect();
	if (scene_rect.width() <= 0. || scene_rect.height() <= 0.) {
		return QImage();
	}

	const int height = qMax(1, qRound(width * scene_rect.height() / scene_rect.width()));

	QImage image(width, height, QImage::Format_ARGB32);
	image.fill(Qt::white);
	diagram->toPaintDevice(image, width, height, Qt::KeepAspectRatio);
	return image;
}

namespace
{
	/// "K1 at (410, 230) 60x80" - what a failure has to say to be actionable.
	QString describe(Element *element)
	{
		const QRectF rect = element->sceneBoundingRect();
		QString name = element->displayedLabel();
		if (name.isEmpty()) {
			name = element->name();
		}
		return QStringLiteral("%1 at (%2, %3) %4x%5")
			   .arg(name)
			   .arg(rect.x()).arg(rect.y())
			   .arg(rect.width()).arg(rect.height());
	}
}

QRectF UiBench::pageRect(Diagram *diagram)
{
	return diagram ? diagram->border_and_titleblock.borderAndTitleBlockRect()
				   : QRectF();
}

QRectF UiBench::borderRect(Diagram *diagram)
{
	return diagram ? diagram->border_and_titleblock.outsideBorderRect() : QRectF();
}

QRectF UiBench::drawingRect(Diagram *diagram)
{
	return diagram ? diagram->border_and_titleblock.insideBorderRect() : QRectF();
}

QRectF UiBench::titleBlockRect(Diagram *diagram)
{
	return diagram ? diagram->border_and_titleblock.titleBlockRect() : QRectF();
}

QList<DiagramTextItem *> UiBench::texts(Diagram *diagram)
{
	QList<DiagramTextItem *> list;
	if (!diagram) {
		return list;
	}

	// Every kind at once - the texts of a component, the free ones, the ones a
	// conductor carries - because they are one thing to whoever reads the sheet:
	// something written on it. DiagramContent sorts them by kind, which is the
	// wrong question here, and its content() does not collect the component
	// texts at all (see Diagram::content(), which only looks for three types).
	const QList<QGraphicsItem *> items = diagram->items();
	for (QGraphicsItem *item : items) {
		if (DiagramTextItem *text = dynamic_cast<DiagramTextItem *>(item)) {
			list << text;
		}
	}
	return list;
}

QStringList UiBench::elementsOffPage(Diagram *diagram)
{
	QStringList off_page;
	if (!diagram) {
		return off_page;
	}

	const QRectF page = pageRect(diagram);
	if (page.isEmpty()) {
		return off_page;
	}

	const QList<Element *> elements = diagram->elements();
	for (Element *element : elements) {
		if (!page.contains(element->sceneBoundingRect())) {
			off_page << describe(element);
		}
	}
	off_page.sort();
	return off_page;
}

QStringList UiBench::elementsWithoutSurface(Diagram *diagram)
{
	QStringList degenerate;
	if (!diagram) {
		return degenerate;
	}

	const QList<Element *> elements = diagram->elements();
	for (Element *element : elements) {
		const QRectF rect = element->boundingRect();
		if (rect.isEmpty() || !rect.isValid()) {
			degenerate << describe(element);
		}
	}
	degenerate.sort();
	return degenerate;
}

UiBench::Rendering::Rendering(Diagram *diagram, int width)
{
	if (!diagram || width <= 0) {
		return;
	}

	m_region = pageRect(diagram);
	if (m_region.isEmpty()) {
		return;
	}

	const int height = qMax(1, qRound(width * m_region.height() / m_region.width()));
	m_scale_x = width / m_region.width();
	m_scale_y = height / m_region.height();

	m_image = QImage(width, height, QImage::Format_ARGB32);
	m_image.fill(Qt::white);

	// Grid and guides off around the render, put back afterwards: the same
	// thing renderDiagram() does in sources/cli_export.cpp, and for the same
	// reason - neither of them is on the paper.
	const bool was_drawing_grid = diagram->displayGrid();
	const bool was_drawing_guides = diagram->displayGuides();
	diagram->setDisplayGrid(false);
	diagram->setDisplayGuides(false);

	QPainter painter(&m_image);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);
	// IgnoreAspectRatio, with a height already computed from the page, so that
	// the image is exactly the page and nothing is letterboxed into it: it is
	// what makes map() a plain scale instead of a guess.
	diagram->render(&painter,
					QRectF(QPointF(0., 0.), QSizeF(m_image.size())),
					m_region,
					Qt::IgnoreAspectRatio);
	painter.end();

	diagram->setDisplayGrid(was_drawing_grid);
	diagram->setDisplayGuides(was_drawing_guides);
}

QRect UiBench::Rendering::map(const QRectF &scene_rect) const
{
	if (m_image.isNull()) {
		return QRect();
	}

	const QRectF mapped((scene_rect.left() - m_region.left()) * m_scale_x,
						(scene_rect.top()  - m_region.top())  * m_scale_y,
						scene_rect.width()  * m_scale_x,
						scene_rect.height() * m_scale_y);
	return mapped.toAlignedRect().intersected(m_image.rect());
}

int UiBench::Rendering::ink() const
{
	return inkOfImageRect(m_image.rect());
}

int UiBench::Rendering::ink(const QRectF &scene_rect) const
{
	return inkOfImageRect(map(scene_rect));
}

int UiBench::Rendering::inkOfImageRect(const QRect &image_rect) const
{
	const QRect area = image_rect.intersected(m_image.rect());
	if (area.isEmpty()) {
		return 0;
	}

	// Counted, not compared: how much was drawn here, never what shape it had.
	// A count survives a change of font; a shape does not.
	int pixels = 0;
	const QColor background(Qt::white);
	for (int y = area.top() ; y <= area.bottom() ; ++y) {
		for (int x = area.left() ; x <= area.right() ; ++x) {
			if (m_image.pixelColor(x, y) != background) {
				++pixels;
			}
		}
	}
	return pixels;
}
