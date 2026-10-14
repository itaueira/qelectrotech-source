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

#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/ElementsCollection/xmlelementcollection.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/swapelementcommand.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QSet>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <functional>

/*
	Another element in the place of one on a folio, its wires handed over
	(T54).

	The replacing symbols are made here from the definition of the element
	being replaced, and added to the project's own collection: a copy with a
	uuid of its own is a different symbol to the program, with the very same
	terminals - which is the case the command is for, a symbol made from
	another one - and each case then changes in that copy the one thing it
	is about: a terminal taken out, a terminal moved, the terminal uuids
	dropped, a link type, a piece of information.
*/

namespace
{
	/// The elements of @p project, folio after folio, each folio by uuid
	QList<Element *> elementsOf(QETProject *project)
	{
		QList<Element *> found;
		for (Diagram *diagram : project->diagrams())
		{
			QList<Element *> elements = diagram->elements();
			std::sort(elements.begin(), elements.end(),
				  [](const Element *a, const Element *b) {
				return a->uuid().toString() < b->uuid().toString();
			});
			found << elements;
		}
		return found;
	}

	/// How many terminals of @p element carry a wire
	int wiredTerminals(const Element *element)
	{
		int count = 0;
		for (Terminal *terminal : element->terminals()) {
			if (!terminal->conductors().isEmpty()) {
				++count;
			}
		}
		return count;
	}

	/// The first simple element of @p project with wires on @p min terminals
	Element *wiredElement(QETProject *project, int min = 2)
	{
		for (Element *element : elementsOf(project))
		{
			if (element->linkType() == Element::Simple
			    && wiredTerminals(element) >= min) {
				return element;
			}
		}
		return nullptr;
	}

	/// The wires on @p element, each once
	QList<Conductor *> wiresOf(const Element *element)
	{
		QList<Conductor *> wires;
		for (Terminal *terminal : element->terminals()) {
			for (Conductor *conductor : terminal->conductors()) {
				if (!wires.contains(conductor)) {
					wires << conductor;
				}
			}
		}
		return wires;
	}

	/// What a wire looks like: its two docking points and its number
	struct WireShot
	{
		Conductor *conductor = nullptr;
		QUuid uuid;
		QString text;
		QPointF p1;
		QPointF p2;
	};

	QList<WireShot> shoot(const QList<Conductor *> &wires)
	{
		QList<WireShot> shots;
		for (Conductor *conductor : wires)
		{
			WireShot shot;
			shot.conductor = conductor;
			shot.uuid = conductor->uuid();
			shot.text = conductor->properties().text;
			shot.p1 = conductor->terminal1->dockConductor();
			shot.p2 = conductor->terminal2->dockConductor();
			shots << shot;
		}
		return shots;
	}

	bool same(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
	}

	bool hangsOn(const Conductor *conductor, const Element *element)
	{
		return conductor->terminal1->parentElement() == element
				|| conductor->terminal2->parentElement() == element;
	}

	/// The texts of @p element, as the folio shows them: where, and what
	QStringList textsOf(const Element *element)
	{
		QStringList texts;
		for (DynamicElementTextItem *text : element->dynamicTextItems()) {
			texts << QStringLiteral("%1 @ (%2, %3)")
				 .arg(text->toPlainText())
				 .arg(text->pos().x())
				 .arg(text->pos().y());
		}
		return texts;
	}

	/**
		A copy of the definition of @p source, under @p name in the
		project's own collection, with a uuid of its own and whatever
		@p edit changes in it.
	*/
	ElementsLocation addVariant(QETProject *project, const Element *source,
				    const QString &name,
				    const std::function<void(QDomElement &)> &edit = {})
	{
		QDomDocument document;
		QDomElement definition = document.importNode(
					source->location().xml(), true).toElement();
		document.appendChild(definition);

		QDomElement uuid = definition.firstChildElement(QStringLiteral("uuid"));
		if (uuid.isNull()) {
			uuid = document.createElement(QStringLiteral("uuid"));
			definition.appendChild(uuid);
		}
		uuid.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());

		if (edit) {
			edit(definition);
		}

		REQUIRE(project->embeddedElementCollection()->addElementDefinition(
				QStringLiteral("import"), name, definition));
		const ElementsLocation location(QStringLiteral("embed://import/") + name,
						project);
		REQUIRE(location.exist());
		return location;
	}

	/// The terminal of @p definition that carries @p uuid
	QDomElement terminalOf(const QDomElement &definition, const QUuid &uuid)
	{
		const QDomNodeList terminals =
				definition.elementsByTagName(QStringLiteral("terminal"));
		for (int i = 0 ; i < terminals.count() ; ++i)
		{
			QDomElement terminal = terminals.at(i).toElement();
			if (QUuid(terminal.attribute(QStringLiteral("uuid"))) == uuid) {
				return terminal;
			}
		}
		return QDomElement();
	}

	SwapElementCommand::Plan planFor(Element *element,
					 const ElementsLocation &location)
	{
		SwapElementCommand::Plan plan = SwapElementCommand::plan(element, location);
		INFO(plan.error.toStdString());
		REQUIRE(plan.error.isEmpty());
		REQUIRE(plan.new_element != nullptr);
		return plan;
	}

	/// @p element replaced by @p location, the way Alt+Q does it; returns the new element
	Element *swap(Element *element, const ElementsLocation &location)
	{
		const SwapElementCommand::Plan plan = planFor(element, location);
		element->diagram()->undoStack().push(new SwapElementCommand(plan));
		return plan.new_element;
	}

	/// The element of @p project carrying @p uuid
	Element *byUuid(QETProject *project, const QUuid &uuid)
	{
		for (Element *element : elementsOf(project)) {
			if (element->uuid() == uuid) {
				return element;
			}
		}
		return nullptr;
	}
}

TEST_CASE("CU-54.1 and CU-54.3 — The wires stay on the new element, and Ctrl+Z gives the old one back",
	  "[t54][swap]")
{
	UiBench::ScratchProject bench(
		UiBench::fileContent(UiBench::examplePath(QStringLiteral("industrial.qet"))),
		QStringLiteral("industrial.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Element *old_element = wiredElement(bench.project());
	REQUIRE(old_element != nullptr);
	Diagram *diagram = old_element->diagram();

	const QUuid uuid = old_element->uuid();
	const QPointF pos = old_element->pos();
	const qreal rotation = old_element->rotation();
	const QString label = old_element->actualLabel();
	const DiagramContext informations = old_element->elementInformations();
	const QStringList texts = textsOf(old_element);
	const QList<WireShot> before = shoot(wiresOf(old_element));
	const int wires_on_folio = diagram->conductors().count();
	const int elements_on_folio = diagram->elements().count();
	REQUIRE(before.count() >= 2);

	const ElementsLocation variant = addVariant(bench.project(), old_element,
						    QStringLiteral("t54_redrawn.elmt"));
	Element *new_element = swap(old_element, variant);

	SECTION("the new element takes the place, the identity and the texts of the old one")
	{
		CHECK(old_element->scene() == nullptr);
		CHECK(new_element->scene() == diagram);
		CHECK(new_element->location().path().endsWith(QStringLiteral("t54_redrawn.elmt")));
		CHECK(new_element->uuid() == uuid);
		CHECK(same(new_element->pos(), pos));
		CHECK(new_element->rotation() == rotation);
		CHECK(new_element->actualLabel() == label);
		CHECK(new_element->elementInformations() == informations);
		CHECK(textsOf(new_element) == texts);
		CHECK(diagram->elements().count() == elements_on_folio);
	}

	SECTION("each wire is the same wire, docked at the same point of the new element")
	{
		CHECK(diagram->conductors().count() == wires_on_folio);
		for (const WireShot &shot : before)
		{
			INFO("wire " << shot.text.toStdString());
			CHECK(shot.conductor->scene() == diagram);
			CHECK(hangsOn(shot.conductor, new_element));
			CHECK_FALSE(hangsOn(shot.conductor, old_element));
			CHECK(shot.conductor->uuid() == shot.uuid);
			CHECK(shot.conductor->properties().text == shot.text);
			CHECK(same(shot.conductor->terminal1->dockConductor(), shot.p1));
			CHECK(same(shot.conductor->terminal2->dockConductor(), shot.p2));
		}
	}

	SECTION("Ctrl+Z gives the old element back with its wires, Ctrl+Y the new one")
	{
		diagram->undoStack().undo();
		CHECK(old_element->scene() == diagram);
		CHECK(new_element->scene() == nullptr);
		CHECK(diagram->conductors().count() == wires_on_folio);
		for (const WireShot &shot : before)
		{
			CHECK(hangsOn(shot.conductor, old_element));
			CHECK(same(shot.conductor->terminal1->dockConductor(), shot.p1));
			CHECK(same(shot.conductor->terminal2->dockConductor(), shot.p2));
		}

		diagram->undoStack().redo();
		CHECK(old_element->scene() == nullptr);
		CHECK(new_element->scene() == diagram);
		CHECK(textsOf(new_element) == texts);
		for (const WireShot &shot : before) {
			CHECK(hangsOn(shot.conductor, new_element));
		}
	}

	SECTION("saved and opened again, the folio has the new symbol with the wires")
	{
		const int wires = before.count();
		REQUIRE(bench.saveAndReopen());
		Element *reopened = byUuid(bench.project(), uuid);
		REQUIRE(reopened != nullptr);
		CHECK(reopened->location().path().endsWith(QStringLiteral("t54_redrawn.elmt")));
		CHECK(wiresOf(reopened).count() == wires);
		CHECK(reopened->actualLabel() == label);
	}
}

TEST_CASE("CU-54.1 — A label made by a formula stays the label, and stays in place",
	  "[t54][swap]")
{
		//8F1 follows a numbering: its label is worked out from a formula
		//and the folio. Read off the folio, the formula itself would become
		//the label.
	UiBench::Project bench(QStringLiteral("industrial.qet"));
	REQUIRE(bench.isOpen());

	Element *old_element = nullptr;
	for (Element *element : elementsOf(bench.project())) {
		if (element->actualLabel() == QLatin1String("8F1")) {
			old_element = element;
			break;
		}
	}
	REQUIRE(old_element != nullptr);
	REQUIRE_FALSE(old_element->elementInformations()
		      .value(QStringLiteral("formula")).toString().isEmpty());
	const QStringList texts = textsOf(old_element);

	Element *new_element = swap(old_element,
				    addVariant(bench.project(), old_element,
					       QStringLiteral("t54_formula.elmt")));
	CHECK(new_element->actualLabel() == QLatin1String("8F1"));
	CHECK(new_element->elementInformations().value(QStringLiteral("label"))
	      .toString() == QLatin1String("8F1"));
	CHECK(textsOf(new_element) == texts);
}

TEST_CASE("CU-54.2 — A jumper between two terminals of the element moves with both ends",
	  "[t54][swap]")
{
	UiBench::Project bench(QStringLiteral("industrial.qet"));
	REQUIRE(bench.isOpen());

	Element *old_element = wiredElement(bench.project(), 1);
	REQUIRE(old_element != nullptr);
	const QList<Terminal *> terminals = old_element->terminals();
	REQUIRE(terminals.count() >= 2);
	Diagram *diagram = old_element->diagram();

	auto *jumper = new Conductor(terminals.at(0), terminals.at(1));
	diagram->addItem(jumper);
	const QPointF p1 = jumper->terminal1->dockConductor();
	const QPointF p2 = jumper->terminal2->dockConductor();

	Element *new_element = swap(old_element,
				    addVariant(bench.project(), old_element,
					       QStringLiteral("t54_jumper.elmt")));

	CHECK(jumper->scene() == diagram);
	CHECK(jumper->terminal1->parentElement() == new_element);
	CHECK(jumper->terminal2->parentElement() == new_element);
	CHECK(jumper->terminal1 != jumper->terminal2);
	CHECK(same(jumper->terminal1->dockConductor(), p1));
	CHECK(same(jumper->terminal2->dockConductor(), p2));

	int on_both_ends = 0;
	for (Conductor *conductor : diagram->conductors()) {
		if (conductor->terminal1->parentElement() == new_element
		    && conductor->terminal2->parentElement() == new_element) {
			++on_both_ends;
		}
	}
	CHECK(on_both_ends == 1);
}

TEST_CASE("CU-54.4 — A wire on a terminal the new symbol lacks leaves with the old element",
	  "[t54][swap]")
{
	UiBench::Project bench(QStringLiteral("industrial.qet"));
	REQUIRE(bench.isOpen());

	Element *old_element = wiredElement(bench.project());
	REQUIRE(old_element != nullptr);
	Diagram *diagram = old_element->diagram();

	Terminal *gone = nullptr;
	for (Terminal *terminal : old_element->terminals()) {
		if (!terminal->conductors().isEmpty()) {
			gone = terminal;
			break;
		}
	}
	REQUIRE(gone != nullptr);
	REQUIRE_FALSE(gone->uuid().isNull());
	const QList<Conductor *> lost = gone->conductors();
	const QList<Conductor *> all = wiresOf(old_element);

	const ElementsLocation variant = addVariant(
		bench.project(), old_element, QStringLiteral("t54_short.elmt"),
		[&](QDomElement &definition) {
			QDomElement terminal = terminalOf(definition, gone->uuid());
			REQUIRE_FALSE(terminal.isNull());
			terminal.parentNode().removeChild(terminal);
		});

	SwapElementCommand::Plan plan = planFor(old_element, variant);
	CHECK(plan.lost_conductors.count() == lost.count());
	for (Conductor *conductor : lost) {
		CHECK(plan.lost_conductors.contains(conductor));
	}
	CHECK(plan.moved_conductors.count() == all.count() - lost.count());

	SECTION("refused, nothing changes")
	{
		SwapElementCommand::discard(plan);
		CHECK(plan.new_element == nullptr);
		CHECK(old_element->scene() == diagram);
		for (Conductor *conductor : all) {
			CHECK(conductor->scene() == diagram);
			CHECK(hangsOn(conductor, old_element));
		}
	}

	SECTION("carried out, only that wire goes, and Ctrl+Z brings it back")
	{
		Element *new_element = plan.new_element;
		diagram->undoStack().push(new SwapElementCommand(plan));
		for (Conductor *conductor : all)
		{
			if (lost.contains(conductor)) {
				CHECK(conductor->scene() == nullptr);
			} else {
				CHECK(conductor->scene() == diagram);
				CHECK(hangsOn(conductor, new_element));
			}
		}

		diagram->undoStack().undo();
		for (Conductor *conductor : all) {
			CHECK(conductor->scene() == diagram);
			CHECK(hangsOn(conductor, old_element));
		}
		for (Conductor *conductor : lost) {
			CHECK(gone->conductors().contains(conductor));
		}
	}
}

TEST_CASE("CU-54.5 — On the folio, the uuid takes a wire to its moved terminal, and points match a symbol without uuids",
	  "[t54][swap]")
{
	UiBench::Project bench(QStringLiteral("industrial.qet"));
	REQUIRE(bench.isOpen());

	Element *old_element = wiredElement(bench.project());
	REQUIRE(old_element != nullptr);
	const QList<WireShot> before = shoot(wiresOf(old_element));

	SECTION("a terminal moved in the new symbol keeps its wires")
	{
		Terminal *moved = nullptr;
		for (Terminal *terminal : old_element->terminals()) {
			if (!terminal->conductors().isEmpty()) {
				moved = terminal;
				break;
			}
		}
		REQUIRE(moved != nullptr);
		const QUuid moved_uuid = moved->uuid();
		REQUIRE_FALSE(moved_uuid.isNull());
		const QList<Conductor *> its_wires = moved->conductors();
		const QPointF was = moved->dockConductor();

		const ElementsLocation variant = addVariant(
			bench.project(), old_element, QStringLiteral("t54_moved.elmt"),
			[&](QDomElement &definition) {
				QDomElement terminal = terminalOf(definition, moved_uuid);
				REQUIRE_FALSE(terminal.isNull());
				terminal.setAttribute(
					QStringLiteral("x"),
					terminal.attribute(QStringLiteral("x")).toDouble() + 20);
			});

		const SwapElementCommand::Plan plan = planFor(old_element, variant);
		CHECK(plan.lost_conductors.isEmpty());
		old_element->diagram()->undoStack().push(new SwapElementCommand(plan));

		for (Conductor *conductor : its_wires)
		{
			Terminal *end = conductor->terminal1->parentElement() == plan.new_element
					? conductor->terminal1 : conductor->terminal2;
			CHECK(end->parentElement() == plan.new_element);
			CHECK(end->uuid() == moved_uuid);
			CHECK_FALSE(same(end->dockConductor(), was));
		}
	}

	SECTION("a symbol whose terminals carry no uuid is matched by their points")
	{
		const ElementsLocation variant = addVariant(
			bench.project(), old_element, QStringLiteral("t54_nouuid.elmt"),
			[](QDomElement &definition) {
				const QDomNodeList terminals =
						definition.elementsByTagName(QStringLiteral("terminal"));
				for (int i = 0 ; i < terminals.count() ; ++i) {
					terminals.at(i).toElement().removeAttribute(QStringLiteral("uuid"));
				}
			});

		const SwapElementCommand::Plan plan = planFor(old_element, variant);
		CHECK(plan.lost_conductors.isEmpty());
		for (Terminal *terminal : plan.new_element->terminals()) {
			CHECK(terminal->uuid().isNull());
		}
		old_element->diagram()->undoStack().push(new SwapElementCommand(plan));
		for (const WireShot &shot : before)
		{
			CHECK(hangsOn(shot.conductor, plan.new_element));
			CHECK(same(shot.conductor->terminal1->dockConductor(), shot.p1));
			CHECK(same(shot.conductor->terminal2->dockConductor(), shot.p2));
		}
	}
}

TEST_CASE("CU-54.6 — The cross references follow the new element",
	  "[t54][swap][xref]")
{
	UiBench::Project bench(QStringLiteral("industrial.qet"));
	REQUIRE(bench.isOpen());

	Element *master = nullptr;
	for (Element *element : elementsOf(bench.project())) {
		if (element->linkType() == Element::Master && !element->isFree()) {
			master = element;
			break;
		}
	}
	REQUIRE(master != nullptr);
	const QList<Element *> slaves = master->linkedElements();
	REQUIRE_FALSE(slaves.isEmpty());
	Diagram *diagram = master->diagram();

	QHash<Element *, int> index_here;
	QHash<Element *, int> index_there;
	for (Element *slave : slaves) {
		index_here.insert(slave, master->groupIndexForElement(slave));
		index_there.insert(slave, slave->groupIndexForElement(master));
	}

	SECTION("a master replaced by another master keeps its slaves")
	{
		Element *new_master = swap(master,
					   addVariant(bench.project(), master,
						      QStringLiteral("t54_master.elmt")));
		const QList<Element *> linked = new_master->linkedElements();
		CHECK(QSet<Element *>(linked.cbegin(), linked.cend())
		      == QSet<Element *>(slaves.cbegin(), slaves.cend()));
		for (Element *slave : slaves)
		{
			CHECK(slave->linkedElements().contains(new_master));
			CHECK_FALSE(slave->linkedElements().contains(master));
			CHECK(new_master->groupIndexForElement(slave) == index_here.value(slave));
			CHECK(slave->groupIndexForElement(new_master) == index_there.value(slave));
		}

		diagram->undoStack().undo();
		for (Element *slave : slaves)
		{
			CHECK(slave->linkedElements().contains(master));
			CHECK_FALSE(slave->linkedElements().contains(new_master));
		}
		CHECK(master->linkedElements().count() == slaves.count());
	}

	SECTION("a slave replaced by another slave keeps its master")
	{
		Element *slave = slaves.first();
		Diagram *slave_diagram = slave->diagram();
		Element *new_slave = swap(slave,
					  addVariant(bench.project(), slave,
						     QStringLiteral("t54_slave.elmt")));
		CHECK(master->linkedElements().contains(new_slave));
		CHECK_FALSE(master->linkedElements().contains(slave));
		CHECK(new_slave->linkedElements().contains(master));

		slave_diagram->undoStack().undo();
		CHECK(master->linkedElements().contains(slave));
		CHECK_FALSE(master->linkedElements().contains(new_slave));
	}

	SECTION("a master replaced by a simple element loses its slaves, and Ctrl+Z gives them back")
	{
		const ElementsLocation variant = addVariant(
			bench.project(), master, QStringLiteral("t54_simple.elmt"),
			[](QDomElement &definition) {
				definition.setAttribute(QStringLiteral("link_type"),
							QStringLiteral("simple"));
			});
		const SwapElementCommand::Plan plan = planFor(master, variant);
		CHECK(plan.lost_links.count() == slaves.count());
		diagram->undoStack().push(new SwapElementCommand(plan));
		for (Element *slave : slaves)
		{
			CHECK_FALSE(slave->linkedElements().contains(master));
			CHECK_FALSE(slave->linkedElements().contains(plan.new_element));
		}

		diagram->undoStack().undo();
		for (Element *slave : slaves) {
			CHECK(slave->linkedElements().contains(master));
		}
	}
}

TEST_CASE("CU-54.7 — Several elements replaced at once, one Ctrl+Z puts them all back",
	  "[t54][swap]")
{
	UiBench::Project bench(QStringLiteral("industrial.qet"));
	REQUIRE(bench.isOpen());

		//Two elements of the same folio drawn with the same symbol
	Element *first = nullptr;
	Element *second = nullptr;
	for (Element *a : elementsOf(bench.project()))
	{
		if (a->linkType() != Element::Simple || wiresOf(a).isEmpty()) {
			continue;
		}
		for (Element *b : a->diagram()->elements())
		{
			if (b != a && b->location() == a->location() && !wiresOf(b).isEmpty()) {
				first = a;
				second = b;
				break;
			}
		}
		if (first) {
			break;
		}
	}
	REQUIRE(first != nullptr);
	Diagram *diagram = first->diagram();

	const ElementsLocation variant = addVariant(bench.project(), first,
						    QStringLiteral("t54_several.elmt"));
	const SwapElementCommand::Plan plan_first = planFor(first, variant);
	const SwapElementCommand::Plan plan_second = planFor(second, variant);
	auto *command = new QUndoCommand(QStringLiteral("t54"));
	new SwapElementCommand(plan_first, command);
	new SwapElementCommand(plan_second, command);
	diagram->undoStack().push(command);

	CHECK(first->scene() == nullptr);
	CHECK(second->scene() == nullptr);
	CHECK(plan_first.new_element->scene() == diagram);
	CHECK(plan_second.new_element->scene() == diagram);

	diagram->undoStack().undo();
	CHECK(first->scene() == diagram);
	CHECK(second->scene() == diagram);
	CHECK(plan_first.new_element->scene() == nullptr);
	CHECK(plan_second.new_element->scene() == nullptr);
	for (Conductor *conductor : wiresOf(first)) {
		CHECK(conductor->scene() == diagram);
	}
	for (Conductor *conductor : wiresOf(second)) {
		CHECK(conductor->scene() == diagram);
	}
}

TEST_CASE("CU-54.8 — What the old element left empty is taken from the new symbol",
	  "[t54][swap]")
{
	UiBench::Project bench(QStringLiteral("industrial.qet"));
	REQUIRE(bench.isOpen());

	Element *old_element = wiredElement(bench.project(), 1);
	REQUIRE(old_element != nullptr);

		//The folio says a label and a description, and no manufacturer. The
		//label is typed, not worked out by a formula: a formula would put
		//its own result in place of it
	DiagramContext informations = old_element->elementInformations();
	informations.remove(QStringLiteral("formula"));
	informations.addValue(QStringLiteral("label"), QStringLiteral("T54"));
	informations.addValue(QStringLiteral("description"), QStringLiteral("from the folio"));
	informations.remove(QStringLiteral("manufacturer"));
	old_element->setElementInformations(informations);
	REQUIRE(old_element->actualLabel() == QLatin1String("T54"));

	const ElementsLocation variant = addVariant(
		bench.project(), old_element, QStringLiteral("t54_informed.elmt"),
		[](QDomElement &definition) {
			QDomElement old = definition.firstChildElement(
						QStringLiteral("elementInformations"));
			if (!old.isNull()) {
				definition.removeChild(old);
			}
			QDomDocument document = definition.ownerDocument();
			QDomElement infos = document.createElement(
						QStringLiteral("elementInformations"));
			for (const auto &pair : {
				 std::make_pair(QStringLiteral("manufacturer"), QStringLiteral("ACME")),
				 std::make_pair(QStringLiteral("description"), QStringLiteral("from the symbol")),
				 std::make_pair(QStringLiteral("label"), QStringLiteral("SYM"))})
			{
				QDomElement info = document.createElement(
							QStringLiteral("elementInformation"));
				info.setAttribute(QStringLiteral("name"), pair.first);
				info.setAttribute(QStringLiteral("show"), 1);
				info.appendChild(document.createTextNode(pair.second));
				infos.appendChild(info);
			}
			definition.appendChild(infos);
		});

	Element *new_element = swap(old_element, variant);
	const DiagramContext result = new_element->elementInformations();
	CHECK(result.value(QStringLiteral("label")).toString() == QLatin1String("T54"));
	CHECK(result.value(QStringLiteral("description")).toString()
	      == QLatin1String("from the folio"));
	CHECK(result.value(QStringLiteral("manufacturer")).toString()
	      == QLatin1String("ACME"));
}

TEST_CASE("CU-54.10 — A terminal held by a terminal strip is not replaced",
	  "[t54][swap][terminalstrip]")
{
	UiBench::Project bench(QStringLiteral("perceuse.qet"));
	REQUIRE(bench.isOpen());

	Element *terminal_element = nullptr;
	for (Element *element : elementsOf(bench.project())) {
		if (element->linkType() & Element::Terminale) {
			terminal_element = element;
			break;
		}
	}
	REQUIRE(terminal_element != nullptr);
	Diagram *diagram = terminal_element->diagram();
	CHECK(SwapElementCommand::isSwappable(terminal_element));

	TerminalStrip *strip = bench->newTerminalStrip(QString(), QString(),
						       QStringLiteral("X54"));
	REQUIRE(strip != nullptr);
	REQUIRE(strip->addTerminal(terminal_element));

	CHECK_FALSE(SwapElementCommand::isSwappable(terminal_element));
	const SwapElementCommand::Plan plan = SwapElementCommand::plan(
		terminal_element,
		addVariant(bench.project(), terminal_element,
			   QStringLiteral("t54_strip.elmt")));
	CHECK_FALSE(plan.error.isEmpty());
	CHECK(plan.new_element == nullptr);
	CHECK(terminal_element->scene() == diagram);
}
