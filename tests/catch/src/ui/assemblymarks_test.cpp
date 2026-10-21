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

#include "../qt_catch_tostring.h"

#include "../../../../sources/autoNum/assemblystate.h"
#include "../../../../sources/autoNum/projectrenumberer.h"
#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/assemblychangemarks.h"
#include "../../../../sources/undocommand/assemblystatecommand.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"

#include <catch2/catch.hpp>

#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTest>
#include <QUndoStack>

#include <algorithm>

/*
	The mark, on the folio, of what changed since the panel was marked as
	wired (T29, step 5). The example is one the order of service of step 6
	is proved on; a copy is opened, so nothing in examples/ changes.
*/
namespace
{
	const QString EXAMPLE = QStringLiteral("tremie_vibrante.qet");

	void markAsAssembled(QETProject *project)
	{
		project->undoStack()->push(new AssemblyStateCommand(
			project,
			AssemblyStateCommand::photograph(project, AssemblyStage::Assembled)));
	}

		/// The components, in the order of their uuid, which is written in
		/// the file and the same on every run
	QList<Element *> components(QETProject *project)
	{
		QList<Element *> list = ProjectRenumberer::components(project);
		std::sort(list.begin(), list.end(), [](Element *a, Element *b) {
			return a->uuid().toString() < b->uuid().toString();
		});
		return list;
	}

	QList<QGraphicsItem *> marked(QETProject *project)
	{
		QList<QGraphicsItem *> found;
		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets)
		{
			const QList<QGraphicsItem *> items = sheet->items();
			for (QGraphicsItem *item : items) {
				if (AssemblyChangeMarks::isMarked(item)) {
					found << item;
				}
			}
		}
		return found;
	}

	void retag(QETProject *project, Element *element, const QString &label)
	{
		const DiagramContext before = element->elementInformations();
		DiagramContext after = before;
		after.remove(QStringLiteral("formula"));
		after.addValue(QStringLiteral("label"), label);
		project->undoStack()->push(new ChangeElementInformationCommand(element, before, after));
	}

		/// How many pixels of the mark's orange @a image holds
	int orangePixels(const QImage &image)
	{
		int count = 0;
		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				const QColor c = image.pixelColor(x, y);
				if (c.red() > 200 && c.green() > 90 && c.green() < 170 && c.blue() < 60) {
					++count;
				}
			}
		}
		return count;
	}
}

TEST_CASE("T29 — what changed since the assembly is marked on the folio",
	  "[T29][assembly][marks]")
{
	UiBench::ScratchProject scratch(
		UiBench::fileContent(UiBench::examplePath(EXAMPLE)), EXAMPLE);
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	QETProject *project = scratch.project();

	AssemblyChangeMarks *marks = AssemblyChangeMarks::of(project);
	REQUIRE(marks != nullptr);
	CHECK(AssemblyChangeMarks::of(project) == marks);

	const QList<Element *> list = components(project);
	REQUIRE(list.size() >= 2);
	Element *retagged = list.first();
	Diagram *sheet = retagged->diagram();
	REQUIRE(sheet != nullptr);
	REQUIRE_FALSE(sheet->conductors().isEmpty());
	Conductor *renumbered = sheet->conductors().first();

	SECTION("a project never marked as wired marks nothing")
	{
		retag(project, retagged, QStringLiteral("K-CHANGED"));
		marks->refresh();
		CHECK(marked(project).isEmpty());
	}

	SECTION("marked: a component retagged and a wire renumbered, and only them")
	{
		markAsAssembled(project);
		marks->refresh();
			//Marked and untouched: nothing changed, nothing marked - the
			//walk reads the drawing as the photograph was read
		REQUIRE(marked(project).isEmpty());

		retag(project, retagged, QStringLiteral("K-CHANGED"));
		ConductorProperties properties = renumbered->properties();
		properties.m_formula.clear();
		properties.text = QStringLiteral("W-CHANGED");
		renumbered->setProperties(properties);
			//No refresh asked: the retagging is a step of the project, and
			//the marks follow it a moment later
		QTest::qWait(400);
		const QList<QGraphicsItem *> now = marked(project);
		CHECK(now.size() == 2);
		CHECK(now.contains(retagged));
		CHECK(now.contains(renumbered));

		SECTION("the retagging undone takes its mark off")
		{
			project->undoStack()->undo();
			QTest::qWait(400);
			const QList<QGraphicsItem *> after = marked(project);
			CHECK_FALSE(after.contains(retagged));
			CHECK(after.contains(renumbered));
		}

		SECTION("the marking undone takes every mark off")
		{
			project->undoStack()->undo();
			project->undoStack()->undo();
			QTest::qWait(400);
			CHECK(marked(project).isEmpty());
		}
	}

	SECTION("on the screen, and not on the folio printed")
	{
		markAsAssembled(project);
		retag(project, retagged, QStringLiteral("K-CHANGED"));
		marks->refresh();
		REQUIRE(AssemblyChangeMarks::isMarked(retagged));
		const QRectF around = retagged->sceneBoundingRect().adjusted(-30, -30, 30, 30);

			//Printed or exported: the scene painted with no view
		QImage printed(around.size().toSize() * 2, QImage::Format_ARGB32);
		printed.fill(Qt::white);
		{
			QPainter painter(&printed);
			sheet->render(&painter, QRectF(), around);
		}
		CHECK(orangePixels(printed) == 0);

			//On the screen: through a view
		QGraphicsView view(sheet);
		view.resize(500, 500);
		view.centerOn(retagged);
		const int on_screen = orangePixels(view.viewport()->grab().toImage());
		CAPTURE(on_screen);
		CHECK(on_screen > 20);
	}
}
