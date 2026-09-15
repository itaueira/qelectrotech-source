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

#include "../../../../sources/SearchAndReplace/ui/searchandreplacewidget.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/qetgraphicsitem.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QGraphicsItem>
#include <QList>
#include <QRectF>
#include <QString>
#include <algorithm>

/*
	A search that finds and does not show is half an answer. Until now the
	double click on a result opened the right folio and stopped there: on a
	folio with forty components and a hundred wires, the reader was left
	looking for the thing with their eyes, which is the very work the search
	was asked to do.

	What answers is SearchAndReplaceWidget::showResult(), and the cases below
	ask it directly rather than through the tree. That is not a shortcut: the
	tree is filled by fillItemsList(), which walks the folios of the project
	the widget was given by a QETDiagramEditor - a window no case of this
	bench opens, and one that needs a QETApp. The click that reaches the slot
	is left to the screen; what the slot decides is proved here.

	The three kinds of result do not share a parent class, and that is the
	whole reason this function exists:

	  - a component is a QetGraphicsItem, and QetGraphicsItem::showItem()
	    already knew how to point at it;
	  - a conductor is a QGraphicsObject and an independent text is a
	    QGraphicsTextItem. Neither is a QetGraphicsItem, so neither could be
	    handed to showItem() at all - and a search for a wire number finds
	    conductors.

	The conductor stands for both of the second kind here: the two go down
	the same branch, by the same test, and the example used has conductors
	on every folio while no example of the collection ships an independent
	text worth pointing at. Said out loud because a case that skips what it
	cannot find is a case that proves nothing on the day it finds nothing.

	Nothing is drawn and nothing is counted in pixels: the selection of a
	QGraphicsScene is a list, and the cases read the list.

	Labelled T31 and not CU-31.9: the case of use is typing in the search
	box and seeing the folio move, and neither the box nor the folio is
	opened here.
*/

namespace
{
		/// Four folios, 619 components and 156 conductors: the same example
		/// the other navigation cases use.
	const char *search_example = "perceuse.qet";

	/**
		The components of @a folio in an order that does not move between
		runs: QGraphicsScene::items() answers in an order of its own.
	*/
	QList<Element *> sortedElements(Diagram *folio)
	{
		QList<Element *> elements = folio->elements();

		std::sort(elements.begin(), elements.end(),
			  [](const Element *a, const Element *b)
		{
			return a->uuid().toString() < b->uuid().toString();
		});

		return elements;
	}

	/**
		The conductors of @a folio, sorted by where they are drawn, so that
		the same one is picked on the next run. A conductor carries no uuid
		of its own, and its number is not unique on a folio - two ends of
		the same potential share it - so the position is what orders them.
	*/
	QList<Conductor *> sortedConductors(Diagram *folio)
	{
		QList<Conductor *> conductors = folio->conductors();

		std::sort(conductors.begin(), conductors.end(),
			  [](const Conductor *a, const Conductor *b)
		{
			const QRectF ra = a->sceneBoundingRect();
			const QRectF rb = b->sceneBoundingRect();

			if (ra.x() != rb.x()) {
				return ra.x() < rb.x();
			}
			if (ra.y() != rb.y()) {
				return ra.y() < rb.y();
			}
			if (ra.width() != rb.width()) {
				return ra.width() < rb.width();
			}
			return ra.height() < rb.height();
		});

		return conductors;
	}

	/**
		The first folio of @a project that draws at least one conductor and
		two components, nullptr when the project has none.
	*/
	Diagram *folioWithWires(QETProject *project)
	{
		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			if (folio->conductors().count() >= 1
					&& folio->elements().count() >= 2) {
				return folio;
			}
		}

		return nullptr;
	}
}

TEST_CASE("T31 — o resultado da busca fica apontado, e sozinho, na folha dele",
	  "[navigation][search]")
{
	UiBench::Project bench(search_example);
	REQUIRE(bench.isOpen());

	Diagram *folio = folioWithWires(bench.project());
	REQUIRE(folio != nullptr);

	const QList<Element *> elements = sortedElements(folio);
	const QList<Conductor *> conductors = sortedConductors(folio);
	REQUIRE(elements.count() >= 2);
	REQUIRE(conductors.count() >= 1);

	SECTION("um componente encontrado passa pelo primitivo de sempre")
	{
			//Something else selected from before is the defect this guards:
			//two items highlighted the same way is an answer the reader
			//cannot read.
		folio->clearSelection();
		elements.at(1)->setSelected(true);

		SearchAndReplaceWidget::showResult(elements.first());

		CHECK(folio->selectedItems().count() == 1);
		CHECK(folio->selectedItems().contains(elements.first()));
		CHECK(elements.at(1)->isSelected() == false);
	}

	SECTION("um condutor encontrado também fica apontado")
	{
			/* The branch that did not exist: a conductor is not a
			 * QetGraphicsItem, so it could not be handed to showItem() -
			 * and the old code did not try, it just opened the folio.
			 * Cut the generic half of showResult() and this section goes
			 * red while the one above stays green. */
		folio->clearSelection();
		elements.first()->setSelected(true);

		SearchAndReplaceWidget::showResult(conductors.first());

		CHECK(folio->selectedItems().count() == 1);
		CHECK(folio->selectedItems().contains(conductors.first()));
		CHECK(elements.first()->isSelected() == false);
	}

	SECTION("a folha por onde o leitor passou antes fica como estava")
	{
			//Only the folio of the answer is cleared, never the project:
			//folios are tabs, one on screen at a time, so a selection on
			//another tab is not competing with the answer being shown.
		Diagram *other = nullptr;
		const QList<Diagram *> folios = bench.diagrams();
		for (Diagram *candidate : folios)
		{
			if (candidate != folio && candidate->elements().count() > 0) {
				other = candidate;
				break;
			}
		}
		REQUIRE(other != nullptr);

		Element *left_behind = sortedElements(other).first();
		other->clearSelection();
		left_behind->setSelected(true);

		SearchAndReplaceWidget::showResult(conductors.first());

		CHECK(left_behind->isSelected() == true);
		CHECK(other->selectedItems().count() == 1);
	}

	SECTION("um resultado que evaporou não leva a lugar nenhum, e não derruba nada")
	{
			//The widget holds its results in QPointers, and a component
			//deleted between the search and the click leaves a null one.
			//The guard is inside showResult so that no caller needs one.
		folio->clearSelection();
		elements.first()->setSelected(true);

		SearchAndReplaceWidget::showResult(nullptr);

		CHECK(elements.first()->isSelected() == true);
	}
}
