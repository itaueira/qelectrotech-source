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
#include "../../../../sources/properties/xrefproperties.h"
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/masterelement.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QPointF>
#include <QString>
#include <QUndoCommand>
#include <QUndoStack>

/*
	The cross reference moved by hand, out of the way of what it was drawn
	over: the cross under a coil, and the folio position a contact shows.

	What is kept is how far it was moved from its automatic place, and not
	the place: so every case compares the position with the offset to the
	position without it, at the same moment. That is also what makes "it
	follows the element" checkable - the automatic place moves with the
	element, and the moved one must keep the same distance to it.

	The gesture is Shift and a drag, as for a text of an element. The press,
	the moves and the release are sent to the item itself, the way the scene
	sends them to the item under the mouse: which item is on top at a point
	is the scene's business, and not what these cases are about.

	Labelled T50 and not CU-50.x: the context menu that offers to put a
	cross reference back needs the editor window, which no case of this
	suite builds, and the use cases are walked on the folio of the
	draughtsman (see the specification).
*/

namespace
{
	const char *coil_uuid = "{7e500001-0000-4000-8000-000000000001}";
	const char *contact_uuids[] = {
		"{7e500001-0000-4000-8000-000000000011}",
		"{7e500001-0000-4000-8000-000000000012}"};

	QString instance(const QString &type, const QString &uuid, int x, int y,
			 int first_terminal_id, const QString &label,
			 const QStringList &links)
	{
		QString links_xml;
		for (const QString &link : links)
			links_xml += QStringLiteral("<link_uuid uuid=\"%1\"/>").arg(link);

		return QStringLiteral(
			       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/%3\" uuid=\"%4\">"
			       "<terminals>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"0\" id=\"%5\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"2\" id=\"%6\"/>"
			       "</terminals>"
			       "<inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%7"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<links_uuids>%8</links_uuids>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(x).arg(y).arg(type, uuid)
			.arg(first_terminal_id).arg(first_terminal_id + 1)
			.arg(label, links_xml);
	}

	QString definition(const QString &name, const QString &link_type,
			   const QString &kind)
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

	QString fixtureXml()
	{
		QStringList contacts;
		for (const char *uuid : contact_uuids)
			contacts << QLatin1String(uuid);

		QString elements = instance(QStringLiteral("coil.elmt"),
					    QLatin1String(coil_uuid), 200, 200, 0,
					    QStringLiteral("K1"), contacts);
		int x = 400;
		int id = 10;
		for (const QString &uuid : contacts)
		{
			elements += instance(QStringLiteral("contact.elmt"), uuid,
					     x, 200, id, QString(),
					     {QLatin1String(coil_uuid)});
			x += 100;
			id += 2;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%3</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
			.arg(definition(QStringLiteral("coil.elmt"), QStringLiteral("master"),
					QStringLiteral("<kindInformation name=\"type\">coil"
						       "</kindInformation>")),
			     definition(QStringLiteral("contact.elmt"), QStringLiteral("slave"),
					QStringLiteral("<kindInformation name=\"type\">simple"
						       "</kindInformation>"
						       "<kindInformation name=\"state\">NO"
						       "</kindInformation>")),
			     elements);
	}

	MasterElement *coil(Diagram *sheet)
	{
		if (!sheet)
			return nullptr;
		for (Element *element : sheet->elements())
			if (element->uuid() == QUuid(QLatin1String(coil_uuid)))
				return qobject_cast<MasterElement *>(element);
		return nullptr;
	}

	QList<Element *> contacts(Diagram *sheet)
	{
		QList<Element *> found;
		for (Element *element : sheet->elements())
			if (element->linkType() == Element::Slave)
				found << element;
		return found;
	}

	/// The cross reference drawn for @a master, wherever it hangs
	CrossRefItem *xrefOf(Element *master)
	{
		if (!master)
			return nullptr;
		QList<QGraphicsItem *> todo = master->childItems();
		while (!todo.isEmpty())
		{
			QGraphicsItem *item = todo.takeFirst();
			if (auto xref = qgraphicsitem_cast<CrossRefItem *>(item))
				return xref;
			todo += item->childItems();
		}
		return nullptr;
	}

	DynamicElementTextItem *addLabelText(Element *element)
	{
		auto text = new DynamicElementTextItem(element);
		text->setTextFrom(DynamicElementTextItem::ElementInfo);
		text->setInfoName(QStringLiteral("label"));
		element->addDynamicTextItem(text);
		return text;
	}

	/// The label text of @a contact, the one that shows where its coil is
	DynamicElementTextItem *labelOf(Element *contact)
	{
		for (DynamicElementTextItem *text : contact->dynamicTextItems())
			if (text->slaveXrefItem())
				return text;
		return nullptr;
	}

	void setSnapTo(QETProject *project, XRefProperties::SnapTo snap)
	{
		XRefProperties xrp = project->defaultXRefProperties(QStringLiteral("coil"));
		xrp.setSnapTo(snap);
		project->setDefaultXRefProperties(QStringLiteral("coil"), xrp);
	}

	/// The fixture, with the cross references under their element and
	/// every contact carrying a label text
	struct Bench
	{
		UiBench::ScratchProject project{fixtureXml(), QStringLiteral("xrefmove.qet")};

		bool prepare()
		{
			if (!project.isOpen())
				return false;
			setSnapTo(project.project(), XRefProperties::Bottom);
			for (Element *element : project.diagram(0)->elements())
				if (element->linkType() == Element::Slave)
					addLabelText(element);
			return true;
		}
	};

	/**
		A mouse event of @a type at @a scene_pos, sent to @a item as the scene
		sends it to the item under the mouse: through the scene, so that the
		event filters an item has installed see it first.
	*/
	bool sendMouse(QGraphicsItem *item, QEvent::Type type, const QPointF &scene_pos,
		       const QPointF &pressed_at, Qt::KeyboardModifiers modifiers)
	{
		QGraphicsSceneMouseEvent event(type);
		event.setScenePos(scene_pos);
		event.setPos(item->mapFromScene(scene_pos));
		event.setButtonDownScenePos(Qt::LeftButton, pressed_at);
		event.setButtonDownPos(Qt::LeftButton, item->mapFromScene(pressed_at));
		event.setButton(type == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
		event.setModifiers(modifiers);
		event.setAccepted(false);
		item->scene()->sendEvent(item, &event);
		return event.isAccepted();
	}

	/// A whole drag of @a item by @a by, in two moves, with @a modifiers
	/// held all along. @return whether the press was taken
	bool drag(QGraphicsItem *item, const QPointF &by, Qt::KeyboardModifiers modifiers)
	{
		const QPointF from = item->sceneBoundingRect().center();
		const bool taken = sendMouse(item, QEvent::GraphicsSceneMousePress, from, from, modifiers);
		sendMouse(item, QEvent::GraphicsSceneMouseMove, from + by / 2, from, modifiers);
		sendMouse(item, QEvent::GraphicsSceneMouseMove, from + by, from, modifiers);
		sendMouse(item, QEvent::GraphicsSceneMouseRelease, from + by, from, modifiers);
		return taken;
	}

	/// Where the cross reference of @a master sits with no offset, without
	/// leaving the offset changed
	QPointF automaticPos(MasterElement *master)
	{
		const QPointF kept = master->xrefOffset();
		master->setXrefOffset(QPointF());
		const QPointF automatic = xrefOf(master)->pos();
		master->setXrefOffset(kept);
		return automatic;
	}
}

TEST_CASE("T50 — the offset of a master moves its cross reference from the automatic place, and follows the element",
	  "[t50][xref]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	QETProject *project = bench.project.project();
	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);
	REQUIRE(k1->xrefOffset().isNull());

	const QPointF offset(40, 20);

	SECTION("under the element, at the bottom of the folio")
	{
		CrossRefItem *xref = xrefOf(k1);
		REQUIRE(xref != nullptr);
		REQUIRE(xref->parentItem() == k1);
		const QPointF automatic = xref->pos();

		k1->setXrefOffset(offset);
		CHECK(xref->pos() == automatic + offset);

			//The element goes elsewhere: the automatic place goes with it,
			//and the moved cross reference keeps its distance to it
		k1->setPos(k1->pos() + QPointF(150, -50));
		CHECK(xref->pos() == automaticPos(k1) + offset);

		k1->setXrefOffset(QPointF());
		CHECK(xref->pos() == automaticPos(k1));
	}

	SECTION("under the label text")
	{
		setSnapTo(project, XRefProperties::Label);
		DynamicElementTextItem *label = addLabelText(k1);
		CrossRefItem *xref = xrefOf(k1);
		REQUIRE(xref != nullptr);
		REQUIRE(xref->parentItem() == label);
		const QPointF automatic = xref->pos();

		k1->setXrefOffset(offset);
		CHECK(xref->pos() == automatic + offset);

			//A label growing places the cross reference again: the offset
			//stays on top of the new automatic place
		k1->setElementInformations([&] {
			DiagramContext info = k1->elementInformations();
			info.addValue(QStringLiteral("label"), QStringLiteral("K1-LONGER-LABEL"));
			return info;
		}());
		CHECK(xref->pos() == automaticPos(k1) + offset);
	}
}

TEST_CASE("T50 — Shift and a drag move the cross reference alone, on the text grid, as one undo step",
	  "[t50][xref][undo]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);
	CrossRefItem *xref = xrefOf(k1);
	REQUIRE(xref != nullptr);
	REQUIRE(xref->isVisible());

	QUndoStack *stack = bench.project.project()->undoStack();
	const int before = stack->index();
	const QPointF element_at = k1->pos();
	const QPointF automatic = xref->pos();
	const QPointF by(33, 17);

	SECTION("with Shift: the cross reference moves, on the grid, and the element stays")
	{
		REQUIRE(drag(xref, by, Qt::ShiftModifier));

		CHECK(k1->xrefOffset() == Diagram::snapToTextGrid(by));
		CHECK(xref->pos() == automatic + Diagram::snapToTextGrid(by));
		CHECK(k1->pos() == element_at);
		REQUIRE(stack->index() == before + 1);

		stack->undo();
		CHECK(k1->xrefOffset().isNull());
		CHECK(xref->pos() == automatic);

		stack->redo();
		CHECK(k1->xrefOffset() == Diagram::snapToTextGrid(by));
		CHECK(xref->pos() == automatic + Diagram::snapToTextGrid(by));
	}

	SECTION("with Shift and Ctrl: freely, off the grid")
	{
		REQUIRE(drag(xref, by, Qt::ShiftModifier | Qt::ControlModifier));
		CHECK(k1->xrefOffset() == by);
		CHECK(stack->index() == before + 1);
	}

	SECTION("without Shift: the press is not taken, and nothing moves")
	{
		CHECK_FALSE(drag(xref, by, Qt::NoModifier));
		CHECK(k1->xrefOffset().isNull());
		CHECK(xref->pos() == automatic);
		CHECK(stack->index() == before);
	}

	SECTION("a press that moves nothing leaves no undo step")
	{
		REQUIRE(drag(xref, QPointF(), Qt::ShiftModifier));
		CHECK(k1->xrefOffset().isNull());
		CHECK(stack->index() == before);
	}

	SECTION("on a read-only folio, nothing moves")
	{
		bench.project.project()->setReadOnly(true);
		CHECK_FALSE(drag(xref, by, Qt::ShiftModifier));
		CHECK(k1->xrefOffset().isNull());
		CHECK(stack->index() == before);
	}

	SECTION("on a rotated element the cross reference follows the mouse on the folio")
	{
		k1->setRotation(90);
		const QPointF scene_before = xref->scenePos();
		REQUIRE(drag(xref, QPointF(30, 0), Qt::ShiftModifier | Qt::ControlModifier));
		const QPointF moved = xref->scenePos() - scene_before;
		CHECK(qAbs(moved.x() - 30) < 0.01);
		CHECK(qAbs(moved.y()) < 0.01);
	}
}

TEST_CASE("T50 — a moved cross reference stays moved after save and reopen, and an unmoved one writes nothing",
	  "[t50][xref][save]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);

	SECTION("a master never moved writes no attribute")
	{
		QHash<Terminal *, int> table;
		QDomDocument document;
		const QDomElement xml = k1->toXml(document, table);
		CHECK_FALSE(xml.hasAttribute(QStringLiteral("xref_dx")));
		CHECK_FALSE(xml.hasAttribute(QStringLiteral("xref_dy")));
		DynamicElementTextItem *label = labelOf(contacts(bench.project.diagram(0)).first());
		REQUIRE(label != nullptr);
		CHECK_FALSE(label->toXml(document).hasAttribute(QStringLiteral("slave_xref_dx")));
	}

	SECTION("the offsets of a master and of a contact come back")
	{
		k1->setXrefOffset(QPointF(40, -20));
		Element *contact = contacts(bench.project.diagram(0)).first();
		const QUuid contact_uuid = contact->uuid();
		DynamicElementTextItem *label = labelOf(contact);
		REQUIRE(label != nullptr);
		label->setSlaveXrefOffset(QPointF(-10, 5));

		REQUIRE(bench.project.saveAndReopen());
		INFO(bench.project.error().toStdString());

		k1 = coil(bench.project.diagram(0));
		REQUIRE(k1 != nullptr);
		CHECK(k1->xrefOffset() == QPointF(40, -20));
		CrossRefItem *xref = xrefOf(k1);
		REQUIRE(xref != nullptr);
		CHECK(xref->pos() == automaticPos(k1) + QPointF(40, -20));

		Element *reopened = nullptr;
		for (Element *element : contacts(bench.project.diagram(0)))
			if (element->uuid() == contact_uuid)
				reopened = element;
		REQUIRE(reopened != nullptr);
		DynamicElementTextItem *reopened_label = labelOf(reopened);
		REQUIRE(reopened_label != nullptr);
		CHECK(reopened_label->slaveXrefOffset() == QPointF(-10, 5));
	}
}

TEST_CASE("T50 — Shift and a drag move the folio position a contact shows, alone",
	  "[t50][xref]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *contact = contacts(bench.project.diagram(0)).first();
	DynamicElementTextItem *label = labelOf(contact);
	REQUIRE(label != nullptr);
	QGraphicsTextItem *position = label->slaveXrefItem();
	REQUIRE(position != nullptr);
	REQUIRE_FALSE(position->toPlainText().isEmpty());

	QUndoStack *stack = bench.project.project()->undoStack();
	const int before = stack->index();
	const QPointF automatic = position->pos();
	const QPointF label_at = label->pos();
	const QPointF by(20, 30);

	REQUIRE(drag(position, by, Qt::ShiftModifier));
	CHECK(label->slaveXrefOffset() == Diagram::snapToTextGrid(by));
	CHECK(position->pos() == automatic + Diagram::snapToTextGrid(by));
		//Alone: the label it belongs to stays
	CHECK(label->pos() == label_at);
	REQUIRE(stack->index() == before + 1);

	stack->undo();
	CHECK(label->slaveXrefOffset().isNull());
	CHECK(position->pos() == automatic);
	stack->redo();
	CHECK(position->pos() == automatic + Diagram::snapToTextGrid(by));

	SECTION("without Shift the press goes on as before, and nothing moves")
	{
		stack->undo();
			//Who takes a plain press is the text item's business, as it
			//was before: what matters is that nothing moved
		drag(position, by, Qt::NoModifier);
		CHECK(label->slaveXrefOffset().isNull());
		CHECK(position->pos() == automatic);
	}
}

TEST_CASE("T50 — a right click finds the element of a cross reference, and one step puts every moved one back",
	  "[t50][xref][undo]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Diagram *sheet = bench.project.diagram(0);
	MasterElement *k1 = coil(sheet);
	REQUIRE(k1 != nullptr);
	CrossRefItem *xref = xrefOf(k1);
	REQUIRE(xref != nullptr);
	Element *contact = contacts(sheet).first();
	DynamicElementTextItem *label = labelOf(contact);
	REQUIRE(label != nullptr);

	CHECK(CrossRefItem::ownerOf(xref) == k1);
	CHECK(CrossRefItem::ownerOf(label->slaveXrefItem()) == contact);
		//Anything else is nobody's cross reference
	CHECK(CrossRefItem::ownerOf(k1) == nullptr);
	CHECK(CrossRefItem::ownerOf(label) == nullptr);
	CHECK(CrossRefItem::ownerOf(nullptr) == nullptr);

	QList<QGraphicsItem *> selection{k1, contact};
	CHECK(CrossRefItem::resetPlacementCommand(selection) == nullptr);

	k1->setXrefOffset(QPointF(40, 20));
	label->setSlaveXrefOffset(QPointF(10, 10));

	QUndoCommand *reset = CrossRefItem::resetPlacementCommand(selection);
	REQUIRE(reset != nullptr);
	QUndoStack *stack = bench.project.project()->undoStack();
	const int before = stack->index();
	stack->push(reset);
	CHECK(stack->index() == before + 1);
	CHECK(k1->xrefOffset().isNull());
	CHECK(label->slaveXrefOffset().isNull());
	CHECK(xref->pos() == automaticPos(k1));

	stack->undo();
	CHECK(k1->xrefOffset() == QPointF(40, 20));
	CHECK(label->slaveXrefOffset() == QPointF(10, 10));

		//Only the selected elements are put back
	stack->redo();
	k1->setXrefOffset(QPointF(40, 20));
	label->setSlaveXrefOffset(QPointF(10, 10));
	QUndoCommand *only_k1 = CrossRefItem::resetPlacementCommand({k1});
	REQUIRE(only_k1 != nullptr);
	stack->push(only_k1);
	CHECK(k1->xrefOffset().isNull());
	CHECK(label->slaveXrefOffset() == QPointF(10, 10));
}
