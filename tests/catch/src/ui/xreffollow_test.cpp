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
#include "../../../../sources/properties/xrefproperties.h"
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QGraphicsTextItem>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QUuid>

/*
	A cross reference never says where something used to be.

	A coil on folio 1 and two of its contacts, one on folio 3 and one on
	folio 5. The coil draws, under itself, the folio of each contact; each
	contact draws, under its label, the folio of the coil. Both are written
	"%f", the number of the folio in the project and nothing else, so that
	every expected text below is a number that can be written down: the
	row and the column of a position depend on a setting of the machine,
	and the cases here are about folios.

	Two things move a contact to another folio number, and the use case asks
	for both. The folios can be put in another order, which renumbers them
	all; and the contact itself can be taken from its folio to another,
	which in this program is cut and paste - there is no other way to carry
	an element from one folio to another. Every case checks the two ends of
	the reference, the coil and the contact, and the contact that does not
	move, which must keep saying where it is: a reference that followed the
	moved contact by rewriting every reference would pass the first check
	and fail that one.

	What the coil draws is read from its cross reference item, for each
	contact the coil is linked to: CrossRefItem::paint() asks
	elementPositionText() of each linked element as it paints, so this is
	the text on the folio, and it can only be right for a contact the coil
	is still linked to. What the contact draws is the text item its label
	carries, written once and rewritten when something announces a change -
	the half that can go stale.
*/

namespace
{
	const char *coil_uuid = "{31060000-0000-4000-8000-000000000001}";
	/// The contact on folio 3, which never moves.
	const char *staying_uuid = "{31060000-0000-4000-8000-000000000003}";
	/// The contact on folio 5, the one the use case moves.
	const char *moving_uuid = "{31060000-0000-4000-8000-000000000005}";

	const int folio_count = 7;

	QString instance(const QString &type, const QString &uuid, int first_terminal_id,
			 const QString &label, const QStringList &links)
	{
		QString links_xml;
		for (const QString &link : links) {
			links_xml += QStringLiteral("<link_uuid uuid=\"%1\"/>").arg(link);
		}

			//The terminals at their docking point, four units inside the
			//point the symbol declares: what the reading path matches an
			//instance terminal to a symbol terminal by.
		return QStringLiteral(
			       "<element x=\"200\" y=\"200\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/%1\" uuid=\"%2\">"
			       "<terminals>"
			       "<terminal x=\"0\" y=\"-6\" orientation=\"0\" id=\"%3\"/>"
			       "<terminal x=\"0\" y=\"6\" orientation=\"2\" id=\"%4\"/>"
			       "</terminals>"
			       "<inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%5"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<links_uuids>%6</links_uuids>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(type, uuid)
			.arg(first_terminal_id).arg(first_terminal_id + 1)
			.arg(label, links_xml);
	}

	QString definition(const QString &name, const QString &link_type, const QString &kind)
	{
		return QStringLiteral(
			       "<element name=\"%1\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"30\""
			       " hotspot_x=\"10\" hotspot_y=\"15\""
			       " orientation=\"dnnn\" link_type=\"%2\">"
			       "<names><name lang=\"en\">%1</name></names>"
			       "<kindInformations>%3</kindInformations>"
			       "<description>"
			       "<rect x=\"-5\" y=\"-10\" width=\"10\" height=\"20\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"n\" name=\"1\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"s\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>")
			.arg(name, link_type, kind);
	}

	/// Seven folios: the coil on 1, a contact on 3, a contact on 5.
	QString fixtureXml()
	{
		QString elements[folio_count];
		elements[0] = instance(QStringLiteral("coil.elmt"), QLatin1String(coil_uuid), 0,
				       QStringLiteral("K1"),
				       {QLatin1String(staying_uuid), QLatin1String(moving_uuid)});
		elements[2] = instance(QStringLiteral("contact.elmt"), QLatin1String(staying_uuid), 10,
				       QString(), {QLatin1String(coil_uuid)});
		elements[4] = instance(QStringLiteral("contact.elmt"), QLatin1String(moving_uuid), 20,
				       QString(), {QLatin1String(coil_uuid)});

		QString folios;
		for (int i = 0 ; i < folio_count ; ++i)
		{
			folios += QStringLiteral(
					  "<diagram title=\"Folio %1\" order=\"%1\" height=\"600\""
					  " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
					  " displaycols=\"true\" displayrows=\"true\">"
					  "<elements>%2</elements>"
					  "<inputs/>"
					  "<conductors/>"
					  "</diagram>")
				  .arg(QString::number(i + 1), elements[i]);
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2</category></collection>"
			       "%3"
			       "</project>")
			.arg(definition(QStringLiteral("coil.elmt"), QStringLiteral("master"),
					QStringLiteral("<kindInformation name=\"type\">coil"
						       "</kindInformation>")),
			     definition(QStringLiteral("contact.elmt"), QStringLiteral("slave"),
					QStringLiteral("<kindInformation name=\"type\">simple"
						       "</kindInformation>"
						       "<kindInformation name=\"state\">NO"
						       "</kindInformation>")),
			     folios);
	}

	Element *elementWithUuid(Diagram *folio, const char *uuid)
	{
		if (!folio) {
			return nullptr;
		}
		const QList<Element *> elements = folio->elements();
		for (Element *element : elements)
		{
			if (element->uuid() == QUuid(QLatin1String(uuid))) {
				return element;
			}
		}
		return nullptr;
	}

	/// The only element of @a folio, or nullptr when it holds none or several.
	Element *onlyElementOf(Diagram *folio)
	{
		if (!folio) {
			return nullptr;
		}
		const QList<Element *> elements = folio->elements();
		return elements.size() == 1 ? elements.first() : nullptr;
	}

	CrossRefItem *xrefOf(Element *master)
	{
		if (!master) {
			return nullptr;
		}
		QList<QGraphicsItem *> todo = master->childItems();
		while (!todo.isEmpty())
		{
			QGraphicsItem *item = todo.takeFirst();
			if (auto xref = qgraphicsitem_cast<CrossRefItem *>(item)) {
				return xref;
			}
			todo += item->childItems();
		}
		return nullptr;
	}

	/**
		What the coil draws under itself: the text of each contact it is
		linked to, sorted so that the order of the links does not count.
	*/
	QStringList drawnByCoil(Element *coil)
	{
		CrossRefItem *xref = xrefOf(coil);
		if (!xref) {
			return QStringList({QStringLiteral("<no cross reference>")});
		}
		QStringList texts;
		const QList<Element *> contacts = coil->linkedElements();
		for (const Element *contact : contacts) {
			texts << xref->elementPositionText(contact);
		}
		texts.sort();
		return texts;
	}

	/// What @a contact draws under its label: the folio of its coil, or nothing.
	QString drawnByContact(const Element *contact)
	{
		if (!contact) {
			return QStringLiteral("<no contact>");
		}
		const QList<DynamicElementTextItem *> texts = contact->dynamicTextItems();
		for (const DynamicElementTextItem *text : texts)
		{
			if (text->slaveXrefItem()) {
				return text->slaveXrefItem()->toPlainText();
			}
		}
		return QString();
	}

	/**
		The references written as bare folio numbers, under the element of
		the coil and under the label of each contact - set here and not left
		to the settings of the machine, which decide both the formula and
		where the reference hangs.
	*/
	void referenceByFolioNumber(QETProject *project)
	{
		XRefProperties xrp = project->defaultXRefProperties(QStringLiteral("coil"));
		xrp.setSnapTo(XRefProperties::Bottom);
		xrp.setXrefPos(Qt::AlignBottom);
		xrp.setMasterLabel(QStringLiteral("%f"));
		xrp.setSlaveLabel(QStringLiteral("(%f)"));
		project->setDefaultXRefProperties(QStringLiteral("coil"), xrp);
	}

	/// The text a contact shows its coil's folio in: the one of its label.
	void giveLabelText(Element *contact)
	{
		auto text = new DynamicElementTextItem(contact);
		text->setTextFrom(DynamicElementTextItem::ElementInfo);
		text->setInfoName(QStringLiteral("label"));
		contact->addDynamicTextItem(text);
	}

	/// The uuid of @a element as text, so that a failure prints both sides.
	QString uuidOf(const Element *element)
	{
		return element ? element->uuid().toString() : QStringLiteral("<no element>");
	}

	/// One of the uuids of the fixture, written the way uuidOf() writes it.
	QString uuidText(const char *uuid)
	{
		return QUuid(QLatin1String(uuid)).toString();
	}

	/// @a element alone selected on its folio.
	void selectOnly(Element *element)
	{
		element->diagram()->clearSelection();
		element->setSelected(true);
	}

	/// What is selected on @a folio cut, through a view of it, as Ctrl+X does.
	void cutSelection(Diagram *folio)
	{
		DiagramView view(folio);
		view.cut();
		QCoreApplication::processEvents();
	}

	/// What is selected on @a folio copied, through a view of it, as Ctrl+C does.
	void copySelection(Diagram *folio)
	{
		DiagramView view(folio);
		view.copy();
		QCoreApplication::processEvents();
	}

	/// The clipboard pasted on @a folio, through a view of it.
	void pasteOn(Diagram *folio)
	{
		DiagramView view(folio);
		view.paste(QPointF(200, 200));
		QCoreApplication::processEvents();
	}

	/**
		The project of the use case, opened and checked as the use case
		opens and checks it, for the cases below about what cutting and
		pasting does to the links: the coil on folio 1, its contacts on 3
		and 5, both ends saying so before anything moves.
	*/
	struct Fixture
	{
		UiBench::ScratchProject bench {fixtureXml(), QStringLiteral("xreffollow.qet")};
		QETProject *project = nullptr;
		Element *coil = nullptr;
		Element *staying = nullptr;
		Element *moving = nullptr;

		Fixture()
		{
			INFO(bench.error().toStdString());
			REQUIRE(bench.isOpen());
			REQUIRE(bench.diagramCount() == folio_count);

			project = bench.project();
			referenceByFolioNumber(project);

			coil = elementWithUuid(bench.diagram(0), coil_uuid);
			staying = elementWithUuid(bench.diagram(2), staying_uuid);
			moving = elementWithUuid(bench.diagram(4), moving_uuid);
			REQUIRE(coil != nullptr);
			REQUIRE(staying != nullptr);
			REQUIRE(moving != nullptr);
			giveLabelText(staying);
			giveLabelText(moving);

			REQUIRE(coil->linkedElements().size() == 2);
			REQUIRE(drawnByCoil(coil) == QStringList({QStringLiteral("3"), QStringLiteral("5")}));
			REQUIRE(drawnByContact(staying) == QStringLiteral("(1)"));
			REQUIRE(drawnByContact(moving) == QStringLiteral("(1)"));
		}
	};
}

TEST_CASE("CU-31.6 — mover um contato de folha e renumerar as folhas: toda referência a ele diz a folha nova, sem comando",
	  "[uibench][xref][t31]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("xreffollow.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == folio_count);

	QETProject *project = bench.project();
	referenceByFolioNumber(project);

	Element *coil = elementWithUuid(bench.diagram(0), coil_uuid);
	Element *staying = elementWithUuid(bench.diagram(2), staying_uuid);
	Element *moving = elementWithUuid(bench.diagram(4), moving_uuid);
	REQUIRE(coil != nullptr);
	REQUIRE(staying != nullptr);
	REQUIRE(moving != nullptr);
	giveLabelText(staying);
	giveLabelText(moving);

		//The fixture is what it says, and the references say it before
		//anything moves: written down here, not read back.
	REQUIRE(coil->linkedElements().size() == 2);
	REQUIRE(drawnByCoil(coil) == QStringList({QStringLiteral("3"), QStringLiteral("5")}));
	REQUIRE(drawnByContact(staying) == QStringLiteral("(1)"));
	REQUIRE(drawnByContact(moving) == QStringLiteral("(1)"));

	SECTION("as folhas renumeradas: o contato da folha 5 passa a ser da 7, e as duas pontas dizem 7")
	{
			//Folio 5 put last, the way the tabs of the project are
			//dragged: 6 and 7 become 5 and 6, and 5 becomes 7.
		project->diagramOrderChanged(4, 6);
		REQUIRE(moving->diagram()->folioIndex() + 1 == 7);
		REQUIRE(staying->diagram()->folioIndex() + 1 == 3);

		CHECK(drawnByCoil(coil) == QStringList({QStringLiteral("3"), QStringLiteral("7")}));
		CHECK(drawnByContact(moving) == QStringLiteral("(1)"));
		CHECK(drawnByContact(staying) == QStringLiteral("(1)"));
	}

	SECTION("a folha da bobina renumerada: os dois contatos passam a dizer a folha nova dela")
	{
			//Folio 1 put fourth: 2, 3 and 4 become 1, 2 and 3. The coil
			//goes to 4, the contact of folio 3 to 2, the one of folio 5
			//stays on 5 - so each number below changes for its own reason
			//or not at all.
		project->diagramOrderChanged(0, 3);
		REQUIRE(coil->diagram()->folioIndex() + 1 == 4);

		CHECK(drawnByContact(moving) == QStringLiteral("(4)"));
		CHECK(drawnByContact(staying) == QStringLiteral("(4)"));
		CHECK(drawnByCoil(coil) == QStringList({QStringLiteral("2"), QStringLiteral("5")}));
	}

	SECTION("o contato levado da folha 5 para a 6 por recortar e colar, e as folhas renumeradas até ele ser da 7")
	{
		Diagram *from = bench.diagram(4);
		Diagram *to = bench.diagram(5);
		REQUIRE(to->elements().isEmpty());

		from->clearSelection();
		moving->setSelected(true);
		{
			DiagramView view(from);
			view.cut();
			QCoreApplication::processEvents();
		}
		REQUIRE(from->elements().isEmpty());
		{
			DiagramView view(to);
			view.paste(QPointF(200, 200));
			QCoreApplication::processEvents();
		}

		Element *moved = onlyElementOf(to);
		REQUIRE(moved != nullptr);
		REQUIRE(moved->linkType() == Element::Slave);

			//The contact is the coil's contact still, and both ends say 6.
			//Read in this order on purpose: if the link did not survive
			//the paste, the first line says so, and the two after it show
			//what the folio then shows.
		CHECK(coil->linkedElements().contains(moved));
		CHECK(drawnByCoil(coil) == QStringList({QStringLiteral("3"), QStringLiteral("6")}));
		CHECK(drawnByContact(moved) == QStringLiteral("(1)"));

			//Folio 6 put last: it becomes 7, and so does the reference.
		project->diagramOrderChanged(5, 6);
		REQUIRE(moved->diagram()->folioIndex() + 1 == 7);

		CHECK(drawnByCoil(coil) == QStringList({QStringLiteral("3"), QStringLiteral("7")}));
		CHECK(drawnByContact(moved) == QStringLiteral("(1)"));
			//The contact that did not move says where it has always been.
		CHECK(drawnByContact(staying) == QStringLiteral("(1)"));
	}
}

/*
	What the cases below are about is what makes the last section of the use
	case possible: a paste that brings back an element a cut took away pastes
	the element itself, with its identity and its links, and not a copy of
	it. Each one checks a side of that the use case does not walk through -
	the undo, a second paste of the same cut, a copy, a coil carried instead
	of a contact.
*/

TEST_CASE("T31 — recortar e colar um contato, desfazer e refazer: a ligação e as duas referências voltam a cada passo",
	  "[uibench][xref][t31]")
{
	Fixture f;
	Diagram *from = f.bench.diagram(4);
	Diagram *to = f.bench.diagram(5);
	QUndoStack *stack = f.project->undoStack();
	REQUIRE(stack != nullptr);
	const int before_cut = stack->index();

	selectOnly(f.moving);
	cutSelection(from);
	REQUIRE(from->elements().isEmpty());
	REQUIRE(stack->index() == before_cut + 1);
	pasteOn(to);
	REQUIRE(stack->index() == before_cut + 2);

	Element *moved = onlyElementOf(to);
	REQUIRE(moved != nullptr);
		//The contact itself, carried to folio 6: its own uuid, not a new one
	CHECK(uuidOf(moved) == uuidText(moving_uuid));
	CHECK(f.coil->linkedElements().contains(moved));
	CHECK(drawnByCoil(f.coil) == QStringList({QStringLiteral("3"), QStringLiteral("6")}));
	CHECK(drawnByContact(moved) == QStringLiteral("(1)"));

	SECTION("a colagem desfeita e refeita: o contato sai da folha 6, volta, e volta ligado")
	{
		stack->undo();
		QCoreApplication::processEvents();
		REQUIRE(to->elements().isEmpty());
		CHECK(f.coil->linkedElements().size() == 1);
		CHECK(drawnByCoil(f.coil) == QStringList({QStringLiteral("3")}));

		stack->redo();
		QCoreApplication::processEvents();
		REQUIRE(onlyElementOf(to) == moved);
			//Read in this order on purpose, as in the use case: the link
			//first, then what each end draws of it
		CHECK(f.coil->linkedElements().contains(moved));
		CHECK(drawnByCoil(f.coil) == QStringList({QStringLiteral("3"), QStringLiteral("6")}));
		CHECK(drawnByContact(moved) == QStringLiteral("(1)"));
	}

	SECTION("a colagem e o recorte desfeitos e refeitos: o contato volta à folha 5, ligado, e de novo à 6, ligado")
	{
		stack->undo();
		stack->undo();
		QCoreApplication::processEvents();
		REQUIRE(to->elements().isEmpty());
		REQUIRE(onlyElementOf(from) == f.moving);
		CHECK(f.coil->linkedElements().contains(f.moving));
		CHECK(drawnByCoil(f.coil) == QStringList({QStringLiteral("3"), QStringLiteral("5")}));
		CHECK(drawnByContact(f.moving) == QStringLiteral("(1)"));

		stack->redo();
		stack->redo();
		QCoreApplication::processEvents();
		REQUIRE(from->elements().isEmpty());
		REQUIRE(onlyElementOf(to) == moved);
		CHECK(f.coil->linkedElements().contains(moved));
		CHECK(drawnByCoil(f.coil) == QStringList({QStringLiteral("3"), QStringLiteral("6")}));
		CHECK(drawnByContact(moved) == QStringLiteral("(1)"));
	}
}

TEST_CASE("T31 — o mesmo recorte colado duas vezes: a primeira colagem é o contato, ligado, e a segunda uma cópia solta",
	  "[uibench][xref][t31]")
{
	Fixture f;
	Diagram *first_folio = f.bench.diagram(5);
	Diagram *second_folio = f.bench.diagram(6);

	selectOnly(f.moving);
	cutSelection(f.bench.diagram(4));
	REQUIRE(f.coil->linkedElements().size() == 1);
	pasteOn(first_folio);
	pasteOn(second_folio);

	Element *first = onlyElementOf(first_folio);
	Element *second = onlyElementOf(second_folio);
	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	REQUIRE(first->linkType() == Element::Slave);
	REQUIRE(second->linkType() == Element::Slave);

		//The first paste is the contact: its uuid, and its coil
	CHECK(uuidOf(first) == uuidText(moving_uuid));
	CHECK(first->linkedElements().size() == 1);
	CHECK(first->linkedElements().contains(f.coil));
	CHECK(drawnByContact(first) == QStringLiteral("(1)"));

		//The second finds the contact on folio 6 already: a copy, with an
		//identity of its own, and no coil - never two contacts of one cut
	CHECK(uuidOf(second) != uuidText(moving_uuid));
	CHECK(second->isFree());
	CHECK(drawnByContact(second) == QString());

		//One contact more than after the cut, not two
	CHECK(f.coil->linkedElements().size() == 2);
	CHECK_FALSE(f.coil->linkedElements().contains(second));
	CHECK(drawnByCoil(f.coil) == QStringList({QStringLiteral("3"), QStringLiteral("6")}));
}

TEST_CASE("T31 — copiar e colar um contato dá uma cópia solta, como sempre deu",
	  "[uibench][xref][t31]")
{
	Fixture f;
	Diagram *from = f.bench.diagram(4);
	Diagram *to = f.bench.diagram(5);

	selectOnly(f.moving);
	copySelection(from);
	pasteOn(to);

	Element *copy = onlyElementOf(to);
	REQUIRE(copy != nullptr);
	REQUIRE(copy != f.moving);
	REQUIRE(copy->linkType() == Element::Slave);
	CHECK(uuidOf(copy) != uuidText(moving_uuid));
	CHECK(copy->isFree());
	CHECK(drawnByContact(copy) == QString());

		//The original stays on its folio, and stays the coil's
	CHECK(onlyElementOf(from) == f.moving);
	CHECK(f.coil->linkedElements().size() == 2);
	CHECK_FALSE(f.coil->linkedElements().contains(copy));
	CHECK(drawnByCoil(f.coil) == QStringList({QStringLiteral("3"), QStringLiteral("5")}));
	CHECK(drawnByContact(f.moving) == QStringLiteral("(1)"));
}

TEST_CASE("T31 — recortar e colar a bobina: os contatos continuam dela e dizem a folha nova, também depois de desfazer e refazer",
	  "[uibench][xref][t31]")
{
	Fixture f;
	Diagram *from = f.bench.diagram(0);
	Diagram *to = f.bench.diagram(1);
	REQUIRE(to->elements().isEmpty());
	QUndoStack *stack = f.project->undoStack();
	REQUIRE(stack != nullptr);

	selectOnly(f.coil);
	cutSelection(from);
	REQUIRE(from->elements().isEmpty());
		//Cut, the coil lets go of its contacts
	REQUIRE(f.staying->isFree());
	REQUIRE(f.moving->isFree());
	const int after_cut = stack->index();
	pasteOn(to);
	REQUIRE(stack->index() == after_cut + 1);

	Element *coil = onlyElementOf(to);
	REQUIRE(coil != nullptr);
	REQUIRE(coil->linkType() == Element::Master);
		//The coil itself: its uuid, and its label, which a copy's paste
		//erases
	CHECK(uuidOf(coil) == uuidText(coil_uuid));
	CHECK(coil->elementInformations().value(QStringLiteral("label")).toString() == QStringLiteral("K1"));

	CHECK(coil->linkedElements().size() == 2);
	CHECK(coil->linkedElements().contains(f.staying));
	CHECK(coil->linkedElements().contains(f.moving));
	CHECK(drawnByCoil(coil) == QStringList({QStringLiteral("3"), QStringLiteral("5")}));
	CHECK(drawnByContact(f.staying) == QStringLiteral("(2)"));
	CHECK(drawnByContact(f.moving) == QStringLiteral("(2)"));

		//The paste undone: the coil leaves folio 2, and its contacts are
		//left without it, as after the cut
	stack->undo();
	QCoreApplication::processEvents();
	REQUIRE(to->elements().isEmpty());
	CHECK(f.staying->isFree());
	CHECK(f.moving->isFree());
	CHECK(drawnByContact(f.staying) == QString());
	CHECK(drawnByContact(f.moving) == QString());

		//And redone: the coil back, with both contacts
	stack->redo();
	QCoreApplication::processEvents();
	REQUIRE(onlyElementOf(to) == coil);
	CHECK(coil->linkedElements().size() == 2);
	CHECK(drawnByCoil(coil) == QStringList({QStringLiteral("3"), QStringLiteral("5")}));
	CHECK(drawnByContact(f.staying) == QStringLiteral("(2)"));
	CHECK(drawnByContact(f.moving) == QStringLiteral("(2)"));
}
