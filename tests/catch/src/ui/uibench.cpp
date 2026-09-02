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
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/independenttextitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetversion.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
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
