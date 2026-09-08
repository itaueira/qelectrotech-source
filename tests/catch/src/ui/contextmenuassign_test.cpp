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

#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramview.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/independenttextitem.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QAction>
#include <QGraphicsItem>
#include <QList>

/*
	« Attribuer une pièce » in the context menu of the folio.

	The action itself is not new and nothing here proves it: what it writes on
	a component is proved by assignpart_test.cpp, with a project open, and the
	arithmetic behind it without one. What is new is where it is offered, and
	the one rule that governs it: a catalog part is something a component
	carries, so the entry appears when a component is selected and stays away
	when the selection is a conductor, a free text, or nothing at all.

	Why the rule is proved through DiagramView::selectionHasElement() and not
	through the list contextMenuActions() returns - which is the direct
	question, and the one worth asking - is measured below, in the last case
	of this file rather than asserted in this comment.
*/

namespace {

	const char *reference_example = "convertisseur.qet";

	/// The first folio of @a project carrying a component and a conductor.
	Diagram *folioWithBoth(QETProject *project)
	{
		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			if (!folio->conductors().isEmpty()
					&& !folio->elements().isEmpty()) {
				return folio;
			}
		}

		return nullptr;
	}
}

/*
	T13 - what the folio offers when the right button is pressed.

	A designer validating on screen asked for this: with the component in
	front of him, assigning a part should not send him up to the Catalog menu
	and back. The catch is that the right button is pressed over conductors
	and texts just as often, and there a part means nothing.
*/
TEST_CASE("T13 - assigning a part is offered on a component and on nothing else",
	  "[catalog][contextmenu]")
{
	UiBench::Project project{QLatin1String(reference_example)};
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	Diagram *folio = folioWithBoth(project.project());
	REQUIRE(folio != nullptr);

	DiagramView view(folio);

	const QList<Element *> elements = folio->elements();
	const QList<Conductor *> conductors = folio->conductors();
	REQUIRE_FALSE(elements.isEmpty());
	REQUIRE_FALSE(conductors.isEmpty());

	folio->clearSelection();

	SECTION("nothing selected: the entry does not apply")
	{
		CHECK_FALSE(view.selectionHasElement());
	}

	SECTION("a component selected: the entry applies")
	{
		elements.first()->setSelected(true);
		CHECK(view.selectionHasElement());
	}

	SECTION("a conductor selected: the entry does not apply")
	{
		conductors.first()->setSelected(true);
		REQUIRE(folio->selectedItems().count() == 1);
		CHECK_FALSE(view.selectionHasElement());
	}

	SECTION("every conductor of the folio selected: still does not apply")
	{
		for (Conductor *conductor : conductors) {
			conductor->setSelected(true);
		}

			//A rubber band over a bundle of wires is the ordinary way to end
			//up with a selection this size, so the answer must not change
			//with the count.
		REQUIRE(folio->selectedItems().count() == conductors.count());
		CHECK_FALSE(view.selectionHasElement());
	}

	SECTION("a free text selected: the entry does not apply")
	{
		IndependentTextItem *text = new IndependentTextItem();
		folio->addItem(text);
		text->setSelected(true);

		REQUIRE(folio->selectedItems().count() == 1);
		CHECK_FALSE(view.selectionHasElement());
	}

	SECTION("a component among conductors: the entry applies")
	{
			//The selection a rubber band actually produces. The rule is « at
			//least one component », not « only components »: assignCatalogPart()
			//takes the components out of the selection and ignores the rest,
			//and the menu has to agree with it.
		for (Conductor *conductor : conductors) {
			conductor->setSelected(true);
		}
		elements.first()->setSelected(true);

		CHECK(view.selectionHasElement());
	}

	SECTION("deselecting the component takes the entry away again")
	{
			//The menu is rebuilt at every click, so what matters is that the
			//answer follows the selection rather than being decided once.
		elements.first()->setSelected(true);
		REQUIRE(view.selectionHasElement());

		elements.first()->setSelected(false);
		CHECK_FALSE(view.selectionHasElement());
	}
}

/*
	T13 - why the list itself is not what the case above asserts.

	The direct question is « is m_catalog_assign in contextMenuActions() »,
	and it cannot be asked here. contextMenuActions() answers through
	diagramEditor(), which walks up to the top level window and casts it to
	QETDiagramEditor; with no such window the whole body is skipped and the
	list comes back empty, whatever is selected.

	Building that window is not an option for this suite: its constructor
	reaches QETApp - documentDir(), the elements panel, the collection widget -
	and the suite runs on a plain QApplication with QETApp::instance() null,
	deliberately, because a QETApp of its own would load collections, parse
	the arguments of the test runner and register a system tray.

	So this is measured instead of assumed, and it is measured so that the
	day the suite can build an editor the case above can be raised to the
	real question. It fails then, which is the point: a boundary recorded as
	a passing test is a boundary nobody revisits.
*/
TEST_CASE("T13 - contextMenuActions needs an editor window, and says nothing without one",
	  "[catalog][contextmenu]")
{
	UiBench::Project project{QLatin1String(reference_example)};
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	Diagram *folio = folioWithBoth(project.project());
	REQUIRE(folio != nullptr);

	DiagramView view(folio);
	folio->clearSelection();
	folio->elements().first()->setSelected(true);

	REQUIRE(view.selectionHasElement());
	CHECK(view.diagramEditor() == nullptr);
	CHECK(view.contextMenuActions().isEmpty());
}
