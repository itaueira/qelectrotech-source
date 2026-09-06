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
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/qetgraphicsitem.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QList>
#include <QMultiMap>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStyleOptionGraphicsItem>
#include <QUuid>
#include <algorithm>

/*
	QetGraphicsItem::showItem is the one thing every way of navigating this
	program shares: the cross reference of a master, the arrow to the next
	folio, the terminal strip window sending the reader back to the schematic.
	It raises the folio of an item, selects the item and zooms the views onto
	it.

	It used to select without clearing, and that is the defect these cases
	guard. Two items highlighted the same way is not a smaller mistake than
	pointing at the wrong one: the reader who followed a reference cannot tell
	the answer from whatever was selected before, so the reference is unusable
	either way. Everything built on top of this primitive - a Navigate command,
	a choice list, the search results - would inherit an answer that is right
	and unreadable.

	Nothing here is drawn on a screen, and nothing here counts pixels. The
	selection of a QGraphicsScene is a list, not an appearance, so the cases
	read the list. That also keeps them out of the reach of the one platform
	difference that bites this suite: this machine's Qt5 build rasterises no
	glyph without a screen.

	Two things are proved, and they fail apart on purpose:

	  - the rule: after showItem, the folio holds that item and nothing else,
	    and the folio the reader came from is left alone;
	  - the wiring: a real double click on a real cross reference of an example
	    project still goes through showItem. A rule nobody calls is a rule that
	    passes its own tests forever.
*/

namespace
{
		/// Four folios, 619 components and six links already made - the only
		/// example that ships with cross references actually tied together.
	const char *reference_example = "perceuse.qet";

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
		The first two folios of @a project that draw at least @a least
		components each. Empty when the project has no such pair.
	*/
	QList<Diagram *> twoDrawingFolios(QETProject *project, int least)
	{
		QList<Diagram *> found;
		const QList<Diagram *> folios = project->diagrams();

		for (Diagram *folio : folios)
		{
			if (folio->elements().count() >= least) {
				found.append(folio);
			}
			if (found.count() == 2) {
				break;
			}
		}

		return found;
	}

	/**
		A cross reference that knows where a double click on it would go.

		The rectangles come from CrossRefItem::hoveredContactsMap(), which the
		item fills in updateLabel() on a QImage-backed painter - the same
		rectangles it compares the cursor against, and the same ones the PDF
		export turns into hyperlinks. Reading them is therefore asking the item
		where it would send the reader, not guessing at it from the drawing.
	*/
	struct ClickableXref
	{
		CrossRefItem *item = nullptr;
		Diagram      *folio = nullptr;
		Element      *target = nullptr;
		QPointF       scene_pos;
	};

	/**
		A QetGraphicsItem that was never put on a folio.

		Built here rather than by taking a component out of the open project:
		removing an item from a Diagram goes through its QGIManager and its
		undo stack, and a case about a null pointer has no business leaving
		the project in a state the rest of the file would inherit.
	*/
	class OrphanItem : public QetGraphicsItem
	{
		public:
			QRectF boundingRect() const override
			{
				return QRectF(0, 0, 1, 1);
			}

			void paint(QPainter *, const QStyleOptionGraphicsItem *,
				   QWidget *) override
			{}
	};

	/**
		The clickable cross reference of @a project whose destination sorts
		first by uuid, so that the same one is picked on the next run.

		A degenerate rectangle is skipped: a zero-width one is not clickable
		in the program either, and picking it would prove nothing about a
		click.
	*/
	ClickableXref firstClickableXref(QETProject *project)
	{
		ClickableXref found;
		QString best_uuid;

		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			const QList<QGraphicsItem *> items = folio->items();
			for (QGraphicsItem *item : items)
			{
				if (item->type() != CrossRefItem::Type) {
					continue;
				}

				CrossRefItem *xref = static_cast<CrossRefItem *>(item);
				const QMultiMap<Element *, QRectF> &map =
						xref->hoveredContactsMap();

				for (auto it = map.constBegin() ; it != map.constEnd() ; ++it)
				{
					if (!it.key() || it.value().isEmpty()) {
						continue;
					}

					const QString uuid = it.key()->uuid().toString();
					if (found.item && uuid >= best_uuid) {
						continue;
					}

					best_uuid = uuid;
					found.item = xref;
					found.folio = folio;
					found.target = it.key();
					found.scene_pos =
							xref->mapToScene(it.value().center());
				}
			}
		}

		return found;
	}

	/**
		The double click the scene would hand to the item at @a scene_pos,
		sent the way QGraphicsView sends it.
	*/
	void doubleClickAt(Diagram *folio, const QPointF &scene_pos)
	{
		QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseDoubleClick);
		event.setButton(Qt::LeftButton);
		event.setButtons(Qt::LeftButton);
		event.setScenePos(scene_pos);
		event.setPos(scene_pos);
		event.setScreenPos(QPoint(0, 0));

		QCoreApplication::sendEvent(folio, &event);
	}
}

TEST_CASE("T31 — o destaque de navegação deixa a folha com um item selecionado, "
	  "e é o item para onde se foi",
	  "[uibench][navigate]")
{
	UiBench::Project project(reference_example);
	{
		INFO(project.error().toStdString());
		REQUIRE(project.isOpen());
	}

	const QList<Diagram *> folios = twoDrawingFolios(project.project(), 2);
	REQUIRE(folios.count() == 2);

	Diagram *first_folio = folios.at(0);
	Diagram *second_folio = folios.at(1);

	const QList<Element *> first_elements = sortedElements(first_folio);
	const QList<Element *> second_elements = sortedElements(second_folio);
	REQUIRE(first_elements.count() >= 2);
	REQUIRE(second_elements.count() >= 2);

	SECTION("o que estava selecionado na mesma folha não fica para trás")
	{
		Element *stale = first_elements.at(0);
		Element *target = first_elements.at(1);

		first_folio->clearSelection();
		stale->setSelected(true);
		REQUIRE(first_folio->selectedItems().count() == 1);

		QetGraphicsItem::showItem(target);

		CHECK(target->isSelected());
		CHECK_FALSE(stale->isSelected());
		CHECK(first_folio->selectedItems().count() == 1);
	}

	SECTION("e nem quando o que estava selecionado eram vários")
	{
			//A selection of several is what a rubber band leaves, and it is
			//the case the old code answered worst: the reader arrived at a
			//folio where a dozen components were highlighted exactly like
			//the one being pointed at.
		first_folio->clearSelection();
		for (Element *element : first_elements) {
			element->setSelected(true);
		}
		const int selected_before = first_folio->selectedItems().count();
		REQUIRE(selected_before >= 2);

		Element *target = first_elements.at(0);
		QetGraphicsItem::showItem(target);

		INFO("estavam selecionados " << selected_before);
		CHECK(target->isSelected());
		CHECK(first_folio->selectedItems().count() == 1);
	}

	SECTION("a folha de destino é limpa, a folha de onde se veio não")
	{
			//The scope of the clearing, written down as an assertion because
			//it is a decision and not an accident. Folios are tabs of a
			//QTabWidget, so only one of them is on screen: a selection on
			//another folio is not competing with the answer, and throwing it
			//away would cost the reader work nobody asked to leave. The two
			//other places of the program that navigate to a component -
			//QETDiagramEditor::showElement and the missing part report - clear
			//the destination folio alone, and this keeps the three of them
			//saying the same thing.
		Element *left_behind = first_elements.at(0);
		Element *stale = second_elements.at(0);
		Element *target = second_elements.at(1);

		first_folio->clearSelection();
		second_folio->clearSelection();
		left_behind->setSelected(true);
		stale->setSelected(true);

		QetGraphicsItem::showItem(target);

		CHECK(target->isSelected());
		CHECK_FALSE(stale->isSelected());
		CHECK(second_folio->selectedItems().count() == 1);

		CHECK(left_behind->isSelected());
		CHECK(first_folio->selectedItems().count() == 1);
	}

	SECTION("um item sem folha, ou nenhum item, não mexe em seleção nenhuma")
	{
			//Both are reachable: CrossRefItem hands over whatever was under
			//the cursor, which is null when the click missed every contact,
			//and an item taken out of its folio still exists while the undo
			//stack holds it.
		Element *selected = first_elements.at(0);
		first_folio->clearSelection();
		selected->setSelected(true);

		QetGraphicsItem::showItem(nullptr);

		CHECK(selected->isSelected());
		CHECK(first_folio->selectedItems().count() == 1);

		OrphanItem orphan;
		REQUIRE(orphan.diagram() == nullptr);

		QetGraphicsItem::showItem(&orphan);

		CHECK(selected->isSelected());
		CHECK(first_folio->selectedItems().count() == 1);
	}
}

TEST_CASE("T31 — o duplo clique numa referência cruzada real passa por showItem",
	  "[uibench][navigate]")
{
	/*
		The wiring, and it is a separate case because it fails for a separate
		reason. Every assertion of the case above goes on passing if a caller
		stops calling showItem - the rule would still be right, and no reader
		would ever reach it. So this one starts at the click.
	*/
	UiBench::Project project(reference_example);
	{
		INFO(project.error().toStdString());
		REQUIRE(project.isOpen());
	}

	const ClickableXref xref = firstClickableXref(project.project());

		//Braced so the message stays on the two assertions it explains: an
		//INFO lives until the end of its block, and left loose here it would
		//be printed under every failure of the case, including the ones about
		//the selection - saying "no cross reference was found" right beside an
		//assertion that only ran because one was.
	{
		INFO("nenhuma referência cruzada clicável em " << reference_example
		     << ": sem ela este caso não olha ligação nenhuma, e passar "
			"seria pior que falhar");
		REQUIRE(xref.item != nullptr);
		REQUIRE(xref.target != nullptr);
	}

	Diagram *target_folio = xref.target->diagram();
	REQUIRE(target_folio != nullptr);

		//Something else already selected where the click is going to land -
		//the stale selection the fix is about.
	Element *stale = nullptr;
	const QList<Element *> candidates = sortedElements(target_folio);
	for (Element *element : candidates)
	{
		if (element != xref.target) {
			stale = element;
			break;
		}
	}
	REQUIRE(stale != nullptr);

	const QList<Diagram *> folios = project.diagrams();
	for (Diagram *folio : folios) {
		folio->clearSelection();
	}
	stale->setSelected(true);
	REQUIRE(target_folio->selectedItems().count() == 1);

	doubleClickAt(xref.folio, xref.scene_pos);

	CHECK(xref.target->isSelected());
	CHECK_FALSE(stale->isSelected());
	CHECK(target_folio->selectedItems().count() == 1);
}
