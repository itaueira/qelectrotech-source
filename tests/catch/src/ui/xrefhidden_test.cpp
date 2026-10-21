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
#include "../../../../sources/qetgraphicsitem/elementtextitemgroup.h"
#include "../../../../sources/qetgraphicsitem/masterelement.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/masterpropertieswidget.h"
#include "../../../../sources/ui/elementpropertieswidget.h"
#include "../../../../sources/PropertiesEditor/propertieseditordockwidget.h"

#include <catch2/catch.hpp>

#include <QApplication>
#include <QMainWindow>
#include <QCheckBox>
#include <QDomDocument>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QTabWidget>
#include <QTreeWidget>
#include <QUndoStack>

/*
	The cross reference of a master kept off the folio, and nothing else.

	What the draughtsman asked for is "hide, not disable": the table drawn
	under a coil goes away, and the links, the text each contact shows and
	the navigation stay as they are. So every case below checks the two
	halves together - the table is gone, and what the cross reference does
	besides drawing it is still there.

	The fixture is one coil with three contacts, all on one sheet. The
	contacts carry a label text because that is the text a slave shows its
	master's position in: without it, "the contact still shows the coil"
	would be true of a contact that shows nothing.
*/

namespace
{
	const char *coil_uuid = "{7e430001-0000-4000-8000-000000000001}";
	const char *contact_uuids[] = {
		"{7e430001-0000-4000-8000-000000000011}",
		"{7e430001-0000-4000-8000-000000000012}",
		"{7e430001-0000-4000-8000-000000000013}"};

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

	/// The cross reference drawn for @a master, wherever it hangs: under the
	/// element, under its label text or under a texts group.
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

	/// How many contacts still show where their coil is.
	int contactsShowingTheCoil(Diagram *sheet)
	{
		int count = 0;
		for (Element *element : sheet->elements())
		{
			if (element->linkType() != Element::Slave)
				continue;
			for (DynamicElementTextItem *text : element->dynamicTextItems())
				if (text->slaveXrefItem() && !text->slaveXrefItem()->toPlainText().isEmpty())
					++count;
		}
		return count;
	}

	void setSnapTo(QETProject *project, XRefProperties::SnapTo snap)
	{
		XRefProperties xrp = project->defaultXRefProperties(QStringLiteral("coil"));
		xrp.setSnapTo(snap);
		project->setDefaultXRefProperties(QStringLiteral("coil"), xrp);
	}

	/// Tick or untick the box of the master's tab and apply, the way the
	/// properties dialog does it.
	void applyHidden(Element *master, bool hidden)
	{
		MasterPropertiesWidget tab(master);
		auto box = tab.findChild<QCheckBox *>(QStringLiteral("m_hide_xref_cb"));
		REQUIRE(box != nullptr);
		box->setChecked(hidden);
		tab.apply();
	}

	/// Open the fixture with the cross references under their element and
	/// every contact carrying a label text.
	struct Bench
	{
		UiBench::ScratchProject project{fixtureXml(), QStringLiteral("xrefhidden.qet")};

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
}

TEST_CASE("CU-43.1 — hiding the cross reference of a coil keeps its links and what the contacts show",
	  "[t43][xref]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	Diagram *sheet = bench.project.diagram(0);
	MasterElement *k1 = coil(sheet);
	REQUIRE(k1 != nullptr);
	REQUIRE(k1->linkedElements().size() == 3);

		//The case would prove nothing if there were no table to begin with
	CrossRefItem *xref = xrefOf(k1);
	REQUIRE(xref != nullptr);
	REQUIRE(xref->isVisible());
	REQUIRE_FALSE(k1->XrefBoundingRect().isEmpty());
	REQUIRE(contactsShowingTheCoil(sheet) == 3);

	applyHidden(k1, true);

	CHECK(k1->isXrefHidden());
	CHECK_FALSE(xref->isVisible());
		//It takes no room either: the label placed under it moves up
	CHECK(k1->XrefBoundingRect().isEmpty());

		//And the other half: hidden, not disabled
	CHECK(k1->linkedElements().size() == 3);
	CHECK(contactsShowingTheCoil(sheet) == 3);

	MasterPropertiesWidget tab(k1);
	auto linked = tab.findChild<QTreeWidget *>(QStringLiteral("m_link_tree_widget"));
	REQUIRE(linked != nullptr);
	CHECK(linked->topLevelItemCount() == 3);
		//The box reads the element back, so opening the tab again shows it ticked
	auto box = tab.findChild<QCheckBox *>(QStringLiteral("m_hide_xref_cb"));
	REQUIRE(box != nullptr);
	CHECK(box->isChecked());
}

TEST_CASE("CU-43.2 — hiding the cross reference is one undo step, and redo hides it again",
	  "[t43][xref][undo]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);
	CrossRefItem *xref = xrefOf(k1);
	REQUIRE(xref != nullptr);

	QUndoStack *stack = bench.project.project()->undoStack();
	const int before = stack->index();

	applyHidden(k1, true);
	REQUIRE(stack->index() == before + 1);
	REQUIRE_FALSE(xref->isVisible());

	stack->undo();
	CHECK_FALSE(k1->isXrefHidden());
	CHECK(xref->isVisible());
	CHECK(k1->linkedElements().size() == 3);

	stack->redo();
	CHECK(k1->isXrefHidden());
	CHECK_FALSE(xref->isVisible());
		//The links were not in the command, and undo and redo left them be
	CHECK(k1->linkedElements().size() == 3);
}

TEST_CASE("CU-43.3 — a hidden cross reference stays hidden after save and reopen, and only that one",
	  "[t43][xref][save]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);
	applyHidden(k1, true);
	REQUIRE(k1->isXrefHidden());

	REQUIRE(bench.project.saveAndReopen());
	INFO(bench.project.error().toStdString());

	k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);
	CHECK(k1->isXrefHidden());
	CHECK(k1->linkedElements().size() == 3);
	CrossRefItem *xref = xrefOf(k1);
	REQUIRE(xref != nullptr);
	CHECK_FALSE(xref->isVisible());

	SECTION("a project written before the attribute existed shows every cross reference")
	{
			//The fixture itself is such a project: no xref_hidden anywhere
		UiBench::ScratchProject old_project(fixtureXml(), QStringLiteral("old.qet"));
		REQUIRE(old_project.isOpen());
		MasterElement *old_k1 = coil(old_project.diagram(0));
		REQUIRE(old_k1 != nullptr);
		CHECK_FALSE(old_k1->isXrefHidden());
	}

	SECTION("a master that was never hidden writes no attribute at all")
	{
		applyHidden(k1, false);
		QHash<Terminal *, int> table;
		QDomDocument document;
		const QDomElement xml = k1->toXml(document, table);
		CHECK_FALSE(xml.hasAttribute(QStringLiteral("xref_hidden")));
	}
}

TEST_CASE("CU-43.4 — the box hides the cross reference in the three places it can hang",
	  "[t43][xref]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	QETProject *project = bench.project.project();
	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);

	SECTION("under the element")
	{
		CrossRefItem *xref = xrefOf(k1);
		REQUIRE(xref != nullptr);
		REQUIRE(xref->parentItem() == k1);

		applyHidden(k1, true);
		CHECK_FALSE(xref->isVisible());
		applyHidden(k1, false);
		CHECK(xref->isVisible());
	}

	SECTION("under the label text")
	{
			//Snap first: the label text of a master builds its cross
			//reference when its source is set, and does not listen to the
			//project settings afterwards
		setSnapTo(project, XRefProperties::Label);
		DynamicElementTextItem *label = addLabelText(k1);

		CrossRefItem *xref = xrefOf(k1);
		REQUIRE(xref != nullptr);
		REQUIRE(xref->parentItem() == label);
		REQUIRE(xref->isVisible());

		applyHidden(k1, true);
		CHECK_FALSE(xref->isVisible());

			//The project changing its cross reference settings used to show
			//the table again on its own: it must not bring back a hidden one
		XRefProperties xrp = project->defaultXRefProperties(QStringLiteral("coil"));
		xrp.setOffset(xrp.offset() + 1);
		project->setDefaultXRefProperties(QStringLiteral("coil"), xrp);
		CHECK_FALSE(xrefOf(k1)->isVisible());

		applyHidden(k1, false);
		CHECK(xrefOf(k1)->isVisible());
	}

	SECTION("under a texts group")
	{
			//Grouped while the snap is still at the bottom, so that the text
			//never builds a cross reference of its own; the group listens to
			//the project settings and builds its own when the snap changes
		DynamicElementTextItem *label = addLabelText(k1);
		ElementTextItemGroup *group = k1->addTextGroup(QStringLiteral("group"));
		REQUIRE(k1->addTextToGroup(label, group));
		setSnapTo(project, XRefProperties::Label);

		CrossRefItem *xref = xrefOf(k1);
		REQUIRE(xref != nullptr);
		REQUIRE(xref->parentItem() == group);
		REQUIRE(xref->isVisible());

		applyHidden(k1, true);
		CHECK_FALSE(xref->isVisible());
		applyHidden(k1, false);
		CHECK(xref->isVisible());
	}
}

TEST_CASE("CU-43.5 — a hidden cross reference keeps following its contacts",
	  "[t43][xref]")
{
		//Two contacts linked, a third one free: linking it while the table
		//is hidden is what the case is about
	QString xml = fixtureXml();
	xml.replace(QStringLiteral("<link_uuid uuid=\"%1\"/>")
			    .arg(QLatin1String(contact_uuids[2])),
		    QString());
	UiBench::ScratchProject project(xml, QStringLiteral("xreffollow.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	setSnapTo(project.project(), XRefProperties::Bottom);

	Diagram *sheet = project.diagram(0);
	MasterElement *k1 = coil(sheet);
	REQUIRE(k1 != nullptr);

	Element *third = nullptr;
	for (Element *element : sheet->elements())
		if (element->uuid() == QUuid(QLatin1String(contact_uuids[2])))
			third = element;
	REQUIRE(third != nullptr);
		//The contact still points at the coil in the fixture, so drop it
	third->unlinkAllElements();
	REQUIRE(k1->linkedElements().size() == 2);

	CrossRefItem *xref = xrefOf(k1);
	REQUIRE(xref != nullptr);
		//Counted by the clickable area of each contact the table draws, and
		//not by its height: the cross has a minimal height two contacts
		//and three fit in alike
	REQUIRE(xref->hoveredContactsMap().uniqueKeys().size() == 2);

	applyHidden(k1, true);
	k1->linkToElement(third);
	REQUIRE(k1->linkedElements().size() == 3);

	applyHidden(k1, false);
	REQUIRE(xref->isVisible());
		//Shown again, the table is rebuilt from the three contacts linked now
		//and not from the two it had when it was hidden
	CHECK(xref->hoveredContactsMap().uniqueKeys().size() == 3);
}

TEST_CASE("T43 — the properties dock scrolls, so the box of the master is reached on a short screen",
	  "[t43][xref]")
{
		//Seen on the screen on 2026-10-01: with the program maximised, the
		//dock of the automatic numbering was laid over the bottom of this
		//one, and the box that hides the cross reference could be neither
		//seen nor clicked. The dock could not be shorter than its tallest
		//editor - 452 px measured on Windows, set by the tab of the
		//informations, and not by this box.
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());
	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);

	PropertiesEditorDockWidget dock;
	auto editor = new ElementPropertiesWidget(k1);
	REQUIRE(dock.addEditor(editor));
	const int editor_minimum = editor->minimumSizeHint().height();
	REQUIRE(editor_minimum > 300);
		//The dock no longer asks for the height of what it shows
	CHECK(dock.minimumSizeHint().height() < editor_minimum / 2);

		//Given less room than its editor asks, it scrolls, and the editor
		//still takes the whole width it is given
	dock.resize(700, 250);
	dock.show();
	QApplication::processEvents();
	auto scroll_area = dock.findChild<QScrollArea *>();
	REQUIRE(scroll_area != nullptr);
	CHECK(scroll_area->verticalScrollBar()->maximum() > 0);
		//and says so: a bar that scrolls but is not shown is a dock nobody
		//knows has more below
	CHECK(scroll_area->verticalScrollBar()->isVisible());
	CHECK(scroll_area->widget()->width() == scroll_area->viewport()->width());

		//And the box of the master, at the bottom of its tab, is reached
	auto box = dock.findChild<QCheckBox *>(QStringLiteral("m_hide_xref_cb"));
	REQUIRE(box != nullptr);
	auto tabs = editor->findChild<QTabWidget *>();
	REQUIRE(tabs != nullptr);
	for (int i = 0; i < tabs->count(); ++i) {
		if (tabs->widget(i)->isAncestorOf(box)) {
			tabs->setCurrentIndex(i);
		}
	}
	QApplication::processEvents();
	scroll_area->ensureWidgetVisible(box);
	QApplication::processEvents();
	const QRect seen(box->mapTo(scroll_area->viewport(), QPoint(0, 0)), box->size());
	CHECK(scroll_area->viewport()->rect().contains(seen));
}

TEST_CASE("T43 — the properties dock grows to its editors when the window has room, never over its neighbour",
	  "[t43][xref]")
{
		//Seen on the screen on 2026-10-01, once the dock scrolled: it kept
		//the height it had when it was empty, a strip of a hundred pixels,
		//over a dock of automatic numbering half empty
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());
	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);

	QMainWindow window;
	window.setCentralWidget(new QWidget);
	auto dock = new PropertiesEditorDockWidget(&window);
	auto below = new QDockWidget(QStringLiteral("below"), &window);
	auto filler = new QWidget;
	filler->setMinimumHeight(150);
	below->setWidget(filler);
	window.addDockWidget(Qt::RightDockWidgetArea, dock);
	window.addDockWidget(Qt::RightDockWidgetArea, below);
	window.resize(900, 900);
	window.show();
	QApplication::processEvents();
	const int empty_height = dock->height();

	auto editor = new ElementPropertiesWidget(k1);
	REQUIRE(dock->addEditor(editor));
	QApplication::processEvents();
	QApplication::processEvents();

	auto scroll_area = dock->findChild<QScrollArea *>();
	REQUIRE(scroll_area != nullptr);
	const int asked = scroll_area->widget()->sizeHint().height();
	INFO("empty " << empty_height << ", asked " << asked << ", now " << dock->height()
	     << ", window " << window.height() << ", below " << below->height()
	     << ", below min " << below->minimumSizeHint().height());
		//It grew to what the editor asks, which fits in this window...
	CHECK(dock->height() > empty_height);
	CHECK(scroll_area->viewport()->height() >= asked);
		//...without being laid over the dock under it, which keeps its own
	CHECK_FALSE(dock->geometry().intersects(below->geometry()));
	CHECK(below->widget()->height() >= filler->minimumHeight());

		//Made taller by hand, it is not shrunk back by the next editor
	const int taller = dock->height() + 60;
	window.resizeDocks({dock}, {taller}, Qt::Vertical);
	QApplication::processEvents();
	const int kept = dock->height();
	REQUIRE(kept > asked);
	dock->growToContent();
	QApplication::processEvents();
	CHECK(dock->height() == kept);
}

TEST_CASE("T43 — the empty properties dock shrinks to its title, and gets its height back",
	  "[t43][xref]")
{
		//Asked by the designer on 2026-10-21 (P151), as the upstream does
		//since #1566: nothing selected, the room goes to the docks around;
		//something selected again, the dock has the height it was given.
		//The scroll area of the dock must not stand in the way
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());
	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);

	QMainWindow window;
	window.setCentralWidget(new QWidget);
	auto dock = new PropertiesEditorDockWidget(&window);
	auto below = new QDockWidget(QStringLiteral("below"), &window);
	auto filler = new QWidget;
	filler->setMinimumHeight(150);
	below->setWidget(filler);
	window.addDockWidget(Qt::RightDockWidgetArea, dock);
	window.addDockWidget(Qt::RightDockWidgetArea, below);
	window.resize(900, 900);
	window.show();
	QApplication::processEvents();

	REQUIRE(dock->addEditor(new ElementPropertiesWidget(k1)));
	QApplication::processEvents();
	QApplication::processEvents();
	const int with_editor = dock->height();
	REQUIRE(with_editor > 200);

	dock->clear();
	QApplication::processEvents();
	QApplication::processEvents();
	INFO("with an editor " << with_editor << ", empty " << dock->height());
	CHECK(dock->height() < with_editor / 4);
	CHECK(below->height() > filler->minimumHeight());

	REQUIRE(dock->addEditor(new ElementPropertiesWidget(k1)));
	QApplication::processEvents();
	QApplication::processEvents();
	INFO("back " << dock->height());
	CHECK(qAbs(dock->height() - with_editor) <= 2);
}
