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
#include "../../../../sources/editor/ui/elementpropertieseditorwidget.h"
#include "../../../../sources/properties/elementdata.h"
#include "../../../../sources/properties/xrefproperties.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/masterelement.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/linksingleelementwidget.h"
#include "../../../../sources/ui/masterpropertieswidget.h"
#include "../../../../sources/undocommand/linkelementcommand.h"

#include <catch2/catch.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QUndoStack>

#include <algorithm>

/*
	The terminal drawn on a terminal strip sheet is the master of the
	terminal drawn in the schematic, the way a coil is the master of its
	contacts.

	Two roles meet on one sheet here, and every case keeps an eye on both
	sides of the line between them: the terminal that links has to stay a
	terminal - in its strip, carrying the potential across - and the new
	kinds must not leak into the old ones. A coil is offered no terminal, a
	strip terminal no contact, and a terminal without the slave role in its
	definition links to nothing, as before.

	The fixture: one coil and one contact, two strip terminals (one level
	and two levels), three terminals that can be slaves, one that cannot,
	and two pins wired to both sides of the first terminal.
*/

namespace
{
	const QString coil_uuid    = QStringLiteral("{7e43f000-0000-4000-8000-000000000001}");
	const QString contact_uuid = QStringLiteral("{7e43f000-0000-4000-8000-000000000002}");
	const QString x1_uuid      = QStringLiteral("{7e43f000-0000-4000-8000-000000000011}");
	const QString x2_uuid      = QStringLiteral("{7e43f000-0000-4000-8000-000000000012}");
	const QString t1_uuid      = QStringLiteral("{7e43f000-0000-4000-8000-000000000021}");
	const QString t2_uuid      = QStringLiteral("{7e43f000-0000-4000-8000-000000000022}");
	const QString t3_uuid      = QStringLiteral("{7e43f000-0000-4000-8000-000000000023}");
	const QString plain_uuid   = QStringLiteral("{7e43f000-0000-4000-8000-000000000031}");
	const QString pin1_uuid    = QStringLiteral("{7e43f000-0000-4000-8000-000000000041}");
	const QString pin2_uuid    = QStringLiteral("{7e43f000-0000-4000-8000-000000000042}");

	/// Every element of the fixture has two terminals, one on top and one
	/// under; the ids are 2*index and 2*index+1.
	/// The instance writes each terminal at its docking point, four units
	/// inside the point the symbol declares (y = -10 and 10), because that
	/// is what loading matches them by. At the declared point they do not
	/// match, and the project opens with no conductor and no warning - see
	/// the fixture of wirenumberlist_test.cpp, which found it first.
	QString instance(const QString &type, const QString &uuid, int x, int y,
			 int index, const QString &label)
	{
		return QStringLiteral(
			       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/%3\" uuid=\"%4\">"
			       "<terminals>"
			       "<terminal x=\"0\" y=\"-6\" orientation=\"0\" id=\"%5\"/>"
			       "<terminal x=\"0\" y=\"6\" orientation=\"2\" id=\"%6\"/>"
			       "</terminals>"
			       "<inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%7"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(x).arg(y).arg(type, uuid)
			.arg(2 * index).arg(2 * index + 1)
			.arg(label);
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

	QString kind(const QString &name, const QString &value)
	{
		return QStringLiteral("<kindInformation name=\"%1\">%2</kindInformation>")
			.arg(name, value);
	}

	QString conductor(int terminal1, int terminal2)
	{
		return QStringLiteral(
			       "<conductor x=\"0\" y=\"0\" terminal1=\"%1\""
			       " terminal2=\"%2\" num=\"\" type=\"multi\""
			       " freezeLabel=\"false\" displaytext=\"1\""
			       " numsize=\"9\" condsize=\"1\"/>")
			.arg(terminal1).arg(terminal2);
	}

	QString fixtureXml()
	{
		const QString definitions =
			definition(QStringLiteral("coil.elmt"), QStringLiteral("master"),
				   kind(QStringLiteral("type"), QStringLiteral("coil")))
			+ definition(QStringLiteral("contact.elmt"), QStringLiteral("slave"),
				     kind(QStringLiteral("type"), QStringLiteral("simple"))
				     + kind(QStringLiteral("state"), QStringLiteral("NO")))
			+ definition(QStringLiteral("strip.elmt"), QStringLiteral("master"),
				     kind(QStringLiteral("type"), QStringLiteral("strip_terminal"))
				     + kind(QStringLiteral("max_slaves"), QStringLiteral("1")))
			+ definition(QStringLiteral("strip2.elmt"), QStringLiteral("master"),
				     kind(QStringLiteral("type"), QStringLiteral("strip_terminal"))
				     + kind(QStringLiteral("max_slaves"), QStringLiteral("2")))
			+ definition(QStringLiteral("terminal.elmt"), QStringLiteral("terminal"),
				     kind(QStringLiteral("type"), QStringLiteral("generic"))
				     + kind(QStringLiteral("function"), QStringLiteral("generic"))
				     + kind(QStringLiteral("link_role"), QStringLiteral("slave")))
			+ definition(QStringLiteral("plain.elmt"), QStringLiteral("terminal"),
				     kind(QStringLiteral("type"), QStringLiteral("generic"))
				     + kind(QStringLiteral("function"), QStringLiteral("generic")))
			+ definition(QStringLiteral("pin.elmt"), QStringLiteral("simple"),
				     QString());

		const QString elements =
			instance(QStringLiteral("coil.elmt"), coil_uuid, 100, 100, 0, QStringLiteral("K1"))
			+ instance(QStringLiteral("contact.elmt"), contact_uuid, 200, 100, 1, QStringLiteral("K1"))
			+ instance(QStringLiteral("strip.elmt"), x1_uuid, 100, 400, 2, QStringLiteral("S1"))
			+ instance(QStringLiteral("strip2.elmt"), x2_uuid, 200, 400, 3, QStringLiteral("S2"))
			+ instance(QStringLiteral("terminal.elmt"), t1_uuid, 400, 200, 4, QStringLiteral("1"))
			+ instance(QStringLiteral("terminal.elmt"), t2_uuid, 500, 200, 5, QStringLiteral("2"))
			+ instance(QStringLiteral("terminal.elmt"), t3_uuid, 600, 200, 6, QStringLiteral("3"))
			+ instance(QStringLiteral("plain.elmt"), plain_uuid, 700, 200, 7, QStringLiteral("4"))
			+ instance(QStringLiteral("pin.elmt"), pin1_uuid, 400, 100, 8, QStringLiteral("P1"))
			+ instance(QStringLiteral("pin.elmt"), pin2_uuid, 400, 300, 9, QStringLiteral("P2"));

			//Pin 1 above the first terminal, pin 2 under it
		const QString conductors = conductor(17, 8) + conductor(9, 18);

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors>%3</conductors>"
			       "</diagram>"
			       "</project>")
			.arg(definitions, elements, conductors);
	}

	Element *byUuid(Diagram *sheet, const QString &uuid)
	{
		if (!sheet)
			return nullptr;
		for (Element *element : sheet->elements())
			if (element->uuid() == QUuid(uuid))
				return element;
		return nullptr;
	}

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

	void setSnapTo(QETProject *project, const QString &type, XRefProperties::SnapTo snap)
	{
		XRefProperties xrp = project->defaultXRefProperties(type);
		xrp.setSnapTo(snap);
		project->setDefaultXRefProperties(type, xrp);
	}

	/// Link @a slave to @a master the way the tabs do it: one command on
	/// the project's stack.
	void link(QETProject *project, Element *slave, Element *master)
	{
		auto undo = new LinkElementCommand(slave);
		undo->setLink(master);
		project->undoStack()->push(undo);
	}

	struct Bench
	{
		UiBench::ScratchProject project{fixtureXml(), QStringLiteral("striplink.qet")};
		Diagram *sheet = nullptr;

		bool prepare()
		{
			if (!project.isOpen())
				return false;
			sheet = project.diagram(0);
			setSnapTo(project.project(), QStringLiteral("strip_terminal"),
				  XRefProperties::Bottom);
			return sheet != nullptr;
		}

		Element *operator()(const QString &uuid) const {return byUuid(sheet, uuid);}
	};
}

TEST_CASE("CU-43.7 — the slave role of a terminal is set in the symbol editor and written into the definition",
	  "[t43][striplink][editor]")
{
	ElementData data;
	data.m_type = ElementData::Terminal;
	data.m_terminal_type = ElementData::TTFuse;
	data.m_terminal_function = ElementData::TFPhase;

	ElementPropertiesEditorWidget dialog(data);
	auto box = dialog.findChild<QCheckBox *>(QStringLiteral("m_strip_slave_cb"));
	REQUIRE(box != nullptr);
	CHECK_FALSE(box->isChecked());

	box->setChecked(true);
	auto buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("m_buttonBox"));
	REQUIRE(buttons != nullptr);
	REQUIRE(buttons->button(QDialogButtonBox::Ok) != nullptr);
	buttons->button(QDialogButtonBox::Ok)->click();

	ElementData edited = dialog.editedData();
	CHECK(edited.isStripTerminalSlave());
		//Added to the terminal, not in its place
	CHECK(edited.m_type == ElementData::Terminal);
	CHECK(edited.m_terminal_type == ElementData::TTFuse);
	CHECK(edited.m_terminal_function == ElementData::TFPhase);

		//What the editor writes into the .elmt (ElementScene::toXml)
	QDomDocument document;
	const QDomElement kind_xml = edited.kindInfoToXml(document);
	QStringList written;
	for (QDomElement info = kind_xml.firstChildElement(QStringLiteral("kindInformation"));
	     !info.isNull();
	     info = info.nextSiblingElement(QStringLiteral("kindInformation")))
		written << info.attribute(QStringLiteral("name")) + QLatin1Char('=') + info.text();
	CHECK(written.contains(QStringLiteral("link_role=slave")));
	CHECK(written.contains(QStringLiteral("type=fuse")));
	CHECK(written.contains(QStringLiteral("function=phase")));

		//Opened again, the editor shows it ticked
	ElementPropertiesEditorWidget again(edited);
	CHECK(again.findChild<QCheckBox *>(QStringLiteral("m_strip_slave_cb"))->isChecked());

	SECTION("a terminal without the role writes no link_role at all")
	{
		ElementData plain;
		plain.m_type = ElementData::Terminal;
		QDomDocument doc;
		const QDomElement plain_kind = plain.kindInfoToXml(doc);
		bool found = false;
		for (QDomElement info = plain_kind.firstChildElement();
		     !info.isNull(); info = info.nextSiblingElement())
			if (info.attribute(QStringLiteral("name")) == QLatin1String("link_role"))
				found = true;
		CHECK_FALSE(found);
	}

	SECTION("the strip terminal is a kind of master the editor offers, with its limit")
	{
		ElementData master;
		master.m_type = ElementData::Master;
		ElementPropertiesEditorWidget editor(master);
		auto kinds = editor.findChild<QComboBox *>(QStringLiteral("m_master_type_cb"));
		REQUIRE(kinds != nullptr);
		const int index = kinds->findData(ElementData::StripTerminal);
		REQUIRE(index >= 0);
		kinds->setCurrentIndex(index);
		editor.findChild<QCheckBox *>(QStringLiteral("max_slaves_checkbox"))->setChecked(true);
		editor.findChild<QSpinBox *>(QStringLiteral("max_slaves_spinbox"))->setValue(2);
		editor.findChild<QDialogButtonBox *>(QStringLiteral("m_buttonBox"))
			->button(QDialogButtonBox::Ok)->click();
		CHECK(editor.editedData().isStripTerminalMaster());
		CHECK(editor.editedData().m_max_slaves == 2);
	}
}

TEST_CASE("CU-43.8 — a terminal with the slave role stays a terminal, linked or not",
	  "[t43][striplink]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *t1 = bench(t1_uuid);
	Element *x1 = bench(x1_uuid);
	REQUIRE(t1 != nullptr);
	REQUIRE(x1 != nullptr);
	REQUIRE(t1->elementData().isStripTerminalSlave());
	REQUIRE(t1->conductors().size() == 2);

	auto check_still_a_terminal = [&]()
	{
		CHECK(t1->linkType() == Element::Terminale);
		auto terminal = qobject_cast<TerminalElement *>(t1);
		REQUIRE(terminal != nullptr);
			//The object the terminal strip manager lists
		CHECK(terminal->realTerminal() != nullptr);
			//The wire number crosses it: the wire above and the wire under
			//are one potential
		Conductor *above = t1->conductors().at(0);
		Conductor *under = t1->conductors().at(1);
		CHECK(above->relatedPotentialConductors().contains(under));
		CHECK(under->relatedPotentialConductors().contains(above));
	};

	SECTION("not linked")
	{
		check_still_a_terminal();
	}

	SECTION("linked to a strip terminal")
	{
		link(bench.project.project(), t1, x1);
		REQUIRE(t1->linkedElements() == QList<Element *>({x1}));
		check_still_a_terminal();
	}
}

TEST_CASE("CU-43.9 — the terminal of the schematic is offered the free strip terminals, and links to one",
	  "[t43][striplink]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *t1 = bench(t1_uuid);
	Element *x1 = bench(x1_uuid);
	REQUIRE(t1 != nullptr);
	REQUIRE(x1 != nullptr);
	DynamicElementTextItem *label = addLabelText(t1);

	{
		LinkSingleElementWidget tab(t1);
		auto tree = tab.findChild<QTreeWidget *>(QStringLiteral("m_tree_widget"));
		REQUIRE(tree != nullptr);
		QStringList offered;
		for (int i = 0; i < tree->topLevelItemCount(); ++i)
			offered << tree->topLevelItem(i)->text(0);
		offered.sort();
			//The two strip terminals, and not the coil
		CHECK(offered == QStringList({QStringLiteral("S1"), QStringLiteral("S2")}));
	}

	link(bench.project.project(), t1, x1);

	CHECK(t1->linkedElements() == QList<Element *>({x1}));
	CHECK(x1->linkedElements() == QList<Element *>({t1}));

		//Each side says where the other is
	REQUIRE(label->slaveXrefItem() != nullptr);
	CHECK_FALSE(label->slaveXrefItem()->toPlainText().isEmpty());
	CrossRefItem *xref = xrefOf(x1);
	REQUIRE(xref != nullptr);
	CHECK(xref->hoveredContactsMap().contains(t1));
}

TEST_CASE("CU-43.10 — the strip terminal is offered the free terminals that can be its slave, and only those",
	  "[t43][striplink]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *x2 = bench(x2_uuid);
	REQUIRE(x2 != nullptr);

	auto offered = [&]()
	{
		MasterPropertiesWidget tab(x2);
		auto tree = tab.findChild<QTreeWidget *>(QStringLiteral("m_free_tree_widget"));
		REQUIRE(tree != nullptr);
		return tree->topLevelItemCount();
	};

		//The three terminals with the role: not the plain one, not the contact
	CHECK(offered() == 3);

	link(bench.project.project(), bench(t1_uuid), bench(x1_uuid));
		//The one now linked is no longer free
	CHECK(offered() == 2);

	SECTION("a coil is still offered its contact, and no terminal")
	{
		MasterPropertiesWidget tab(bench(coil_uuid));
		auto tree = tab.findChild<QTreeWidget *>(QStringLiteral("m_free_tree_widget"));
		REQUIRE(tree != nullptr);
		CHECK(tree->topLevelItemCount() == 1);
	}
}

TEST_CASE("CU-43.11 — a coil takes no terminal, and a strip terminal takes no contact",
	  "[t43][striplink]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *coil = bench(coil_uuid);
	Element *contact = bench(contact_uuid);
	Element *x1 = bench(x1_uuid);
	Element *t1 = bench(t1_uuid);
	Element *plain = bench(plain_uuid);
	REQUIRE(coil != nullptr);
	REQUIRE(contact != nullptr);
	REQUIRE(x1 != nullptr);
	REQUIRE(t1 != nullptr);
	REQUIRE(plain != nullptr);

	CHECK(LinkElementCommand::isLinkable(t1, x1));
	CHECK(LinkElementCommand::isLinkable(x1, t1));
	CHECK(LinkElementCommand::isLinkable(contact, coil));

	CHECK_FALSE(LinkElementCommand::isLinkable(t1, coil));
	CHECK_FALSE(LinkElementCommand::isLinkable(coil, t1));
	CHECK_FALSE(LinkElementCommand::isLinkable(contact, x1));
	CHECK_FALSE(LinkElementCommand::isLinkable(x1, contact));
		//And a terminal whose definition does not give it the role links
		//to nothing, as before
	CHECK_FALSE(LinkElementCommand::isLinkable(plain, x1));
	CHECK_FALSE(LinkElementCommand::isLinkable(x1, plain));

		//The elements refuse on their own too, whoever calls them
	coil->linkToElement(t1);
	x1->linkToElement(contact);
	plain->linkToElement(x1);
	CHECK(coil->isFree());
	CHECK(x1->isFree());
	CHECK(plain->isFree());
}

TEST_CASE("CU-43.12 — the limit of a strip terminal comes from its symbol",
	  "[t43][striplink]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	auto x1 = qobject_cast<MasterElement *>(bench(x1_uuid));
	auto x2 = qobject_cast<MasterElement *>(bench(x2_uuid));
	REQUIRE(x1 != nullptr);
	REQUIRE(x2 != nullptr);

	CHECK_FALSE(x1->isFull());
	link(bench.project.project(), bench(t1_uuid), x1);
	CHECK(x1->isFull());

	link(bench.project.project(), bench(t2_uuid), x2);
	CHECK_FALSE(x2->isFull());
	link(bench.project.project(), bench(t3_uuid), x2);
	CHECK(x2->isFull());
	CHECK(x2->linkedElements().size() == 2);
}

TEST_CASE("CU-43.13 — linking a terminal is undone, redone, and kept across save and reopen",
	  "[t43][striplink][undo][save]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *t1 = bench(t1_uuid);
	Element *x1 = bench(x1_uuid);
	REQUIRE(t1 != nullptr);
	REQUIRE(x1 != nullptr);

	QUndoStack *stack = bench.project.project()->undoStack();
	link(bench.project.project(), t1, x1);
	REQUIRE_FALSE(t1->isFree());

	stack->undo();
	CHECK(t1->isFree());
	CHECK(x1->isFree());

	stack->redo();
	CHECK(t1->linkedElements() == QList<Element *>({x1}));
	CHECK(x1->linkedElements() == QList<Element *>({t1}));

	REQUIRE(bench.project.saveAndReopen());
	bench.sheet = bench.project.diagram(0);
	t1 = bench(t1_uuid);
	x1 = bench(x1_uuid);
	REQUIRE(t1 != nullptr);
	REQUIRE(x1 != nullptr);
	CHECK(t1->linkedElements() == QList<Element *>({x1}));
	CHECK(x1->linkedElements() == QList<Element *>({t1}));
		//And the terminal came back as a terminal with its role
	CHECK(t1->linkType() == Element::Terminale);
	CHECK(t1->elementData().isStripTerminalSlave());
}

TEST_CASE("CU-43.14 — the cross reference of a strip terminal lists its terminals, and draws no contact",
	  "[t43][striplink][xref]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *x2 = bench(x2_uuid);
	Element *t2 = bench(t2_uuid);
	Element *t3 = bench(t3_uuid);
	REQUIRE(x2 != nullptr);

		//The cross, the display a coil gets by default, sorts its slaves by
		//contact state: a terminal has none, and would not be drawn at all
	XRefProperties xrp = bench.project->defaultXRefProperties(QStringLiteral("strip_terminal"));
	xrp.setDisplayHas(XRefProperties::Cross);
	bench.project->setDefaultXRefProperties(QStringLiteral("strip_terminal"), xrp);

	link(bench.project.project(), t2, x2);
	link(bench.project.project(), t3, x2);

	CrossRefItem *xref = xrefOf(x2);
	REQUIRE(xref != nullptr);
	CHECK(xref->isVisible());
	CHECK_FALSE(xref->boundingRect().isEmpty());
	QList<Element *> listed = xref->hoveredContactsMap().uniqueKeys();
	std::sort(listed.begin(), listed.end());
	QList<Element *> expected({t2, t3});
	std::sort(expected.begin(), expected.end());
	CHECK(listed == expected);
}

TEST_CASE("CU-43.15 — a double click on either reference leads to the other terminal",
	  "[t43][striplink][navigate]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Element *t1 = bench(t1_uuid);
	Element *x1 = bench(x1_uuid);
	REQUIRE(t1 != nullptr);
	REQUIRE(x1 != nullptr);
	DynamicElementTextItem *label = addLabelText(t1);
	link(bench.project.project(), t1, x1);

	auto double_click = [&](QGraphicsItem *item, const QPointF &local)
	{
		QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseDoubleClick);
		event.setButton(Qt::LeftButton);
		event.setButtons(Qt::LeftButton);
		event.setPos(local);
		event.setScenePos(item->mapToScene(local));
		bench.sheet->sendEvent(item, &event);
	};

	SECTION("from the terminal of the schematic to the strip terminal")
	{
		QGraphicsTextItem *text = label->slaveXrefItem();
		REQUIRE(text != nullptr);
		bench.sheet->clearSelection();
		double_click(text, text->boundingRect().center());
		CHECK(x1->isSelected());
		CHECK_FALSE(t1->isSelected());
	}

	SECTION("from the strip terminal to the terminal of the schematic")
	{
		CrossRefItem *xref = xrefOf(x1);
		REQUIRE(xref != nullptr);
		REQUIRE(xref->hoveredContactsMap().contains(t1));
		bench.sheet->clearSelection();
		double_click(xref, xref->hoveredContactsMap().value(t1).center());
		CHECK(t1->isSelected());
		CHECK_FALSE(x1->isSelected());
	}
}
