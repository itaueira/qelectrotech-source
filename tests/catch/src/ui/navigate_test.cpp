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
#include "../../../../sources/diagramposition.h"
#include "../../../../sources/qetdiagrameditor.h"
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/elementtextitemgroup.h"
#include "../../../../sources/qetgraphicsitem/qetgraphicsitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/navigatechoicedialog.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QList>
#include <QListWidget>
#include <QMultiMap>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
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

		/// The example that ships links and labels together: twelve folios,
		/// seventy links already made, and one coil answered by four
		/// contacts. perceuse.qet above has six links and every one of them
		/// is a pair, so it cannot say anything about the several case.
	const char *linked_example = "affuteuse_250h.qet";

	/**
		A contact of @a project that knows its coil and draws its own label.

		The three go together because the group needs the three: a slave
		answers a double click only when it is linked, and it only builds a
		cross reference when it holds a text taken from the label of the
		component.
	*/
	struct SlaveWithLabel
	{
		Element                *slave = nullptr;
		Element                *master = nullptr;
		DynamicElementTextItem *label = nullptr;
	};

	/**
		All of them, in an order that does not move between runs: folios in
		project order, components sorted by uuid inside each.

		A list and not the first one, because the case that clicks has one
		more condition to check and can only check it after building the
		group - see slaveXrefToClick().
	*/
	QList<SlaveWithLabel> slavesWithLabel(QETProject *project)
	{
		QList<SlaveWithLabel> found;

		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			const QList<Element *> elements = sortedElements(folio);
			for (Element *element : elements)
			{
					//setSelected() refuses a hidden or disabled item without
					//saying so, and a case built on one would look like a
					//rule that answers nothing.
				if (!element->isVisible() || !element->isEnabled()) {
					continue;
				}
				if (element->linkType() != Element::Slave) {
					continue;
				}

					//Exactly one, because that is what these cases are
					//about: a contact that answers one coil is the paired
					//case the command already handles, and a count of one is
					//then an assertion and not a hope.
				const QList<Element *> linked = element->linkedElements();
				if (linked.count() != 1 || !linked.first()) {
					continue;
				}

				const QList<DynamicElementTextItem *> texts =
						element->dynamicTextItems();
				for (DynamicElementTextItem *text : texts)
				{
					if (text->textFrom()
					    != DynamicElementTextItem::ElementInfo) {
						continue;
					}
					if (text->infoName() != QStringLiteral("label")) {
						continue;
					}
					if (!text->isVisible() || !text->isEnabled()) {
						continue;
					}

					SlaveWithLabel one;
					one.slave = element;
					one.master = linked.first();
					one.label = text;
					found << one;
					break;
				}
			}
		}

		return found;
	}

	/**
		A cross reference of a slave that a double click would actually reach.

		The group is built here - with the two public functions the program
		calls when a person groups a label - because no example ships one. The
		group then builds the cross reference itself, which is the item the
		click has to land on.

		"Would actually reach" is the part that needs looking for rather than
		assuming: the cross reference is laid out just under the label, which
		on a schematic is where conductors and neighbours are, and a click on
		a point covered by something else goes to that something else. So the
		candidates are tried in order and the first uncovered one is taken.
		Groups left on the ones passed over stay in memory and are dropped
		with the project; nothing is written to the file.
	*/
	struct ClickableSlaveXref
	{
		SlaveWithLabel        link;
		ElementTextItemGroup *group = nullptr;
		QGraphicsTextItem    *xref = nullptr;
		QPointF               scene_pos;
	};

	ClickableSlaveXref slaveXrefToClick(const QList<SlaveWithLabel> &candidates)
	{
		ClickableSlaveXref found;

		for (const SlaveWithLabel &candidate : candidates)
		{
			Diagram *folio = candidate.slave->diagram();
			if (!folio) {
				continue;
			}

			ElementTextItemGroup *group =
					candidate.slave->addTextGroup(QStringLiteral("bench"));
			if (!group
			    || !candidate.slave->addTextToGroup(candidate.label, group)) {
				continue;
			}

			QGraphicsTextItem *xref = group->slaveXrefItem();
			if (!xref || xref->boundingRect().isEmpty()) {
				continue;
			}

			const QPointF pos =
					xref->mapToScene(xref->boundingRect().center());
				//items() answers topmost first: anything else there would
				//take the click, and the case would be measuring something
				//other than what it says.
			if (folio->items(pos).value(0) != xref) {
				continue;
			}

			found.link = candidate;
			found.group = group;
			found.xref = xref;
			found.scene_pos = pos;
			break;
		}

		return found;
	}

	/// The first component of @a project linked to at least @a least others.
	Element *firstLinkedToSeveral(QETProject *project, int least)
	{
		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			const QList<Element *> elements = sortedElements(folio);
			for (Element *element : elements)
			{
				if (!element->isVisible() || !element->isEnabled()) {
					continue;
				}
				if (element->linkedElements().count() >= least) {
					return element;
				}
			}
		}

		return nullptr;
	}

	/// The first component of @a project that is linked to nothing at all.
	Element *firstUnlinked(QETProject *project)
	{
		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			const QList<Element *> elements = sortedElements(folio);
			for (Element *element : elements)
			{
				if (!element->isVisible() || !element->isEnabled()) {
					continue;
				}
				if (element->linkedElements().isEmpty()) {
					return element;
				}
			}
		}

		return nullptr;
	}

	/// A component of @a folio that is neither @a one nor @a other.
	Element *someOtherElement(Diagram *folio, Element *one, Element *other)
	{
		const QList<Element *> elements = sortedElements(folio);
		for (Element *element : elements)
		{
			if (!element->isVisible() || !element->isEnabled()) {
				continue;
			}
			if (element != one && element != other) {
				return element;
			}
		}

		return nullptr;
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

TEST_CASE("T31 — o duplo clique no rótulo de um escravo passa pelo mesmo "
	  "primitivo, e não por uma cópia dele",
	  "[uibench][navigate]")
{
	/*
		The group used to spell the going out again - deselect, ungrab, raise
		the folio, select, zoom - instead of calling the primitive, and the
		copy had already drifted: it selected without clearing, which is the
		very defect the primitive was fixed for. Handing it the primitive is
		an extraction and not a rewrite, so what is asserted here is that the
		reader lands exactly where the copy landed, plus the one thing the fix
		added: nothing else left highlighted.
	*/
	UiBench::Project project(linked_example);
	{
		INFO(project.error().toStdString());
		REQUIRE(project.isOpen());
	}

	const QList<SlaveWithLabel> candidates =
			slavesWithLabel(project.project());
	const ClickableSlaveXref clickable = slaveXrefToClick(candidates);
	{
		INFO("nenhuma referência de escravo clicável em " << linked_example
		     << ": foram tentados " << candidates.count()
		     << " contatos ligados, e passar seria pior que falhar");
		REQUIRE(clickable.group != nullptr);
		REQUIRE(clickable.xref != nullptr);
	}

	Element *slave = clickable.link.slave;
	Element *master = clickable.link.master;
	Diagram *slave_folio = slave->diagram();
	Diagram *master_folio = master->diagram();
	REQUIRE(slave_folio != nullptr);
	REQUIRE(master_folio != nullptr);

		//Something else already selected where the click is going to land,
		//and the group itself selected: the two things the copy got wrong.
	Element *stale = someOtherElement(master_folio, master, slave);
	REQUIRE(stale != nullptr);

	const QList<Diagram *> folios = project.diagrams();
	for (Diagram *folio : folios) {
		folio->clearSelection();
	}
	stale->setSelected(true);
	clickable.group->setSelected(true);
	REQUIRE(clickable.group->isSelected());

	doubleClickAt(slave_folio, clickable.scene_pos);

	CHECK(master->isSelected());
	CHECK_FALSE(stale->isSelected());
	CHECK(master_folio->selectedItems().count() == 1);

		//The group lets go of the selection and of the mouse whether or not
		//the coil is on its folio - that part stayed where it was, because it
		//is about the group and not about the destination.
	CHECK_FALSE(clickable.group->isSelected());
}

TEST_CASE("T31 — o comando Navegar sabe para onde ir, e se cala quando são "
	  "vários",
	  "[uibench][navigate]")
{
	/*
		QETDiagramEditor::navigationTargets is what the command uses to know
		where to go, and what the context menu uses - through the enabled
		state of the action - to know whether to offer the entry at all. It is
		asked here directly: the action, the key and the menu entry need a
		main window, which no case in this suite opens, but the answer all
		three rest on does not.
	*/
	UiBench::Project project(linked_example);
	{
		INFO(project.error().toStdString());
		REQUIRE(project.isOpen());
	}

	const SlaveWithLabel found =
			slavesWithLabel(project.project()).value(0);
	{
		INFO("nenhum contato ligado com rótulo próprio em " << linked_example
		     << ": sem ele este caso não olha ligação nenhuma");
		REQUIRE(found.slave != nullptr);
		REQUIRE(found.master != nullptr);
		REQUIRE(found.label != nullptr);
	}

	Diagram *slave_folio = found.slave->diagram();
	REQUIRE(slave_folio != nullptr);

	const QList<Diagram *> folios = project.diagrams();
	for (Diagram *folio : folios) {
		folio->clearSelection();
	}

	SECTION("sem seleção não há para onde ir")
	{
		CHECK(QETDiagramEditor::navigationTargets(slave_folio).isEmpty());
	}

	SECTION("um contato selecionado responde a bobina dele, uma vez só")
	{
		found.slave->setSelected(true);

		const QList<Element *> targets =
				QETDiagramEditor::navigationTargets(slave_folio);
		REQUIRE(targets.count() == 1);
		CHECK(targets.first() == found.master);
	}

	SECTION("o rótulo responde pelo componente que o carrega")
	{
			//The usual way of asking, and the reason a label is not refused:
			//clicking a cross reference selects the text drawn there, never
			//the component under it. A command that worked on everything
			//except the label would be a command that does not work where it
			//is used.
		found.label->setSelected(true);
		REQUIRE_FALSE(found.slave->isSelected());

		const QList<Element *> targets =
				QETDiagramEditor::navigationTargets(slave_folio);
		REQUIRE(targets.count() == 1);
		CHECK(targets.first() == found.master);
	}

	SECTION("o componente e o rótulo dele juntos não contam o destino "
		"duas vezes")
	{
		found.slave->setSelected(true);
		found.label->setSelected(true);

		const QList<Element *> targets =
				QETDiagramEditor::navigationTargets(slave_folio);
		CHECK(targets.count() == 1);
	}

	SECTION("vários destinos continuam vários: a escolha não se faz aqui")
	{
			//A coil answered by several contacts. The command refuses to act
			//on this - it is enabled on a count of one - and the choice list
			//is what will lift the refusal. The count is what the refusal is
			//made of: going silently to one of four would be a reference
			//pointing somewhere the reader did not choose.
		Element *several = firstLinkedToSeveral(project.project(), 2);
		{
			INFO("nenhum componente ligado a dois ou mais em "
			     << linked_example << ": sem ele este trecho não olha nada");
			REQUIRE(several != nullptr);
		}

		Diagram *folio = several->diagram();
		REQUIRE(folio != nullptr);
		several->setSelected(true);

		const QList<Element *> targets =
				QETDiagramEditor::navigationTargets(folio);
		CHECK(targets.count() >= 2);
		CHECK_FALSE(targets.contains(several));
	}

	SECTION("um componente sem ligação não responde nada")
	{
		Element *alone = firstUnlinked(project.project());
		REQUIRE(alone != nullptr);

		Diagram *folio = alone->diagram();
		REQUIRE(folio != nullptr);
		alone->setSelected(true);

		CHECK(QETDiagramEditor::navigationTargets(folio).isEmpty());
	}

	SECTION("uma folha que não existe responde vazio, e não estoura")
	{
		CHECK(QETDiagramEditor::navigationTargets(nullptr).isEmpty());
	}
}

TEST_CASE("T31 — com vários destinos a escolha é oferecida, uma linha por "
	  "destino, e o salto é o que foi escolhido",
	  "[uibench][navigate]")
{
	/*
		The case the command was greyed out on until now: a component that
		answers with more than one place. What is proved here is the window
		that asks - how many lines it offers, what each line says, and that
		nothing is picked when the reader says no.

		The jump itself is not repeated here: QetGraphicsItem::showItem has
		its own cases above, and this window does not jump, it answers with
		a component. That separation is the point of chosenTarget() being a
		getter instead of the dialog navigating on its own.
	*/
	UiBench::Project project(linked_example);
	{
		INFO(project.error().toStdString());
		REQUIRE(project.isOpen());
	}

	Element *several = firstLinkedToSeveral(project.project(), 2);
	{
		INFO("nenhum componente ligado a dois ou mais em " << linked_example
		     << ": sem ele este caso não olha escolha nenhuma");
		REQUIRE(several != nullptr);
	}

	Diagram *asked_from = several->diagram();
	REQUIRE(asked_from != nullptr);

	const QList<Diagram *> folios = project.diagrams();
	for (Diagram *folio : folios) {
		folio->clearSelection();
	}
	several->setSelected(true);

	const QList<Element *> targets =
			QETDiagramEditor::navigationTargets(asked_from);
	REQUIRE(targets.count() >= 2);

	const QList<Element *> ordered =
			NavigateChoiceDialog::inReadingOrder(targets);

	SECTION("uma linha por destino, e nenhuma a mais")
	{
		NavigateChoiceDialog dialog(targets);
		CHECK(dialog.targetCount() == targets.count());

		QListWidget *list = dialog.findChild<QListWidget *>();
		REQUIRE(list != nullptr);
		CHECK(list->count() == targets.count());

			//Something is always highlighted, so Enter has an answer the
			//moment the window opens: a list that starts on nothing makes
			//the keyboard route a two step one for no reason.
		CHECK(list->currentRow() == 0);
	}

	SECTION("cada linha termina na coordenada da borda daquela folha")
	{
			//Asserted on the coordinate and not on the wording around it:
			//what the reader needs from the line is where the thing is, and
			//a case that spelled the French sentence out would break on the
			//day somebody rewrites the sentence without changing the answer.
		for (Element *target : ordered)
		{
			Diagram *sheet = target->diagram();
			REQUIRE(sheet != nullptr);

			const QString line = NavigateChoiceDialog::describe(target);
			DiagramPosition where =
					sheet->convertPosition(target->scenePos());

			INFO(line.toStdString());
			CHECK(line.endsWith(QStringLiteral("(") + where.toString()
					    + QStringLiteral(")")));
			CHECK(line.contains(
				      QString::number(sheet->folioIndex() + 1)));

			const QString label = target->elementInformations()
					      .value(QStringLiteral("label")).toString();
			if (!label.isEmpty()) {
				CHECK(line.startsWith(label));
			}
		}
	}

	SECTION("a ordem é a da leitura, e não a ordem em que a folha "
		"entregou os itens")
	{
		int previous_folio = -2;
		for (Element *target : ordered)
		{
			const int folio = target->diagram()
					? target->diagram()->folioIndex() : -1;
			CHECK(folio >= previous_folio);
			previous_folio = folio;
		}

			//The same destinations asked for backwards come out the same
			//way round. Without this the list would be whatever order the
			//scene happened to hand its items over in, which is not the
			//same twice on the same project.
		QList<Element *> backwards = targets;
		std::reverse(backwards.begin(), backwards.end());
		CHECK(NavigateChoiceDialog::inReadingOrder(backwards) == ordered);
	}

	SECTION("cancelar não escolhe nada")
	{
		NavigateChoiceDialog dialog(targets);

		QDialogButtonBox *box = dialog.findChild<QDialogButtonBox *>();
		REQUIRE(box != nullptr);
		QPushButton *cancel = box->button(QDialogButtonBox::Cancel);
		REQUIRE(cancel != nullptr);

		cancel->click();

		CHECK(dialog.result() == QDialog::Rejected);
		CHECK(dialog.chosenTarget() == nullptr);
	}

	SECTION("confirmar devolve a linha destacada, e não a primeira")
	{
		NavigateChoiceDialog dialog(targets);

		QListWidget *list = dialog.findChild<QListWidget *>();
		REQUIRE(list != nullptr);
		REQUIRE(list->count() >= 2);
		list->setCurrentRow(1);

		QDialogButtonBox *box = dialog.findChild<QDialogButtonBox *>();
		REQUIRE(box != nullptr);
		QPushButton *ok = box->button(QDialogButtonBox::Ok);
		REQUIRE(ok != nullptr);

		ok->click();

		CHECK(dialog.result() == QDialog::Accepted);
		CHECK(dialog.chosenTarget() == ordered.at(1));
	}

	SECTION("um destino que já não existe não vira linha")
	{
			//navigationTargets answers with pointers, and the list is built
			//from them; a null one in the middle of it would be a line that
			//goes nowhere.
		QList<Element *> with_a_hole = targets;
		with_a_hole << nullptr;

		NavigateChoiceDialog dialog(with_a_hole);
		CHECK(dialog.targetCount() == targets.count());
		CHECK(NavigateChoiceDialog::describe(nullptr).isEmpty());
	}
}
