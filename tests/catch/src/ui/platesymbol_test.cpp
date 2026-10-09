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
#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/catalog/catalogpart.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountinglayouteditor.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/layout/mountingview.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/location/undrawnparts.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"
#include <catch2/catch.hpp>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDropEvent>
#include <QListWidget>
#include <QMimeData>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTest>
#include <QUndoStack>

/*
	From a part on the plate to the element of the folio it is.

	The plate window does not open folios: it has no idea what a folio
	view is. It finds the element in the project and hands it out through
	a signal, and whoever opened the window - the schematic editor - shows
	it. So what this proves is the half that is the plate's: which part
	answers, which element comes out, and the case where the symbol has
	gone, which must say so and hand out nothing. The other half, the
	schematic editor following the signal and the context menu of the
	folio offering the way back, needs the main window, and stays in the
	queue of screen tests.
*/

namespace
{
	QAction *goToSymbolAction(MountingLayoutEditor &editor)
	{
		for (QAction *action : editor.findChildren<QAction *>()) {
			if (action->text() == QStringLiteral("Aller au symbole")) {
				return action;
			}
		}
		FAIL("no « Aller au symbole » action in the layout window");
		return nullptr;
	}

	void selectOnly(MountingLayoutEditor &editor, const QStringList &item_uuids)
	{
		editor.scene()->clearSelection();
		for (const QString &uuid : item_uuids) {
			MountedPartItem *part = editor.scene()->partItem(uuid);
			REQUIRE(part != nullptr);
			part->setSelected(true);
		}
	}
}

TEST_CASE("T19 — « Aller au symbole » hands out the element of the folio the part is",
	  "[t44][calepinagem]")
{
	UiBench::Project bench(QStringLiteral("tremie_vibrante.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	const QList<Element *> elements = sheet->elements();
	REQUIRE_FALSE(elements.isEmpty());
	Element *target = elements.first();

	MountingLayout layout;
	const QString face = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1"),
						QStringLiteral("plate"),
						MountingArea(600.0, 800.0)));

	MountedItem linked(QStringLiteral("-Q1"), QPointF(10, 10), QSizeF(22.5, 85));
	linked.uuid = QStringLiteral("q1");
	linked.element = target->uuid().toString();
		//Laid before its symbol was drawn: no element, an ordinary state
	MountedItem loose(QStringLiteral("-X9"), QPointF(100, 10), QSizeF(6, 45));
	loose.uuid = QStringLiteral("x9");
		//Its symbol was deleted from the folio after it was mounted
	MountedItem orphan(QStringLiteral("-K7"), QPointF(200, 10), QSizeF(45, 80));
	orphan.uuid = QStringLiteral("k7");
	orphan.element = QStringLiteral("{00000000-dead-4000-8000-000000000000}");
	for (const MountedItem &item : {linked, loose, orphan}) {
		REQUIRE_FALSE(layout.mountItem(face, item).isEmpty());
	}
	bench->setMountingLayout(layout);

	MountingLayoutEditor editor(bench.project());
	REQUIRE(editor.shownSurface() == face);
	QAction *go = goToSymbolAction(editor);
	QSignalSpy asked(&editor, &MountingLayoutEditor::goToElement);
	REQUIRE(asked.isValid());

	SECTION("a part that is a symbol gives out that very element")
	{
		selectOnly(editor, {QStringLiteral("q1")});
		REQUIRE(go->isEnabled());
		go->trigger();
		REQUIRE(asked.count() == 1);
		CHECK(asked.first().first().value<Element *>() == target);
	}

	SECTION("a part with no symbol has nowhere to go, and the entry says so by being grey")
	{
		selectOnly(editor, {QStringLiteral("x9")});
		CHECK_FALSE(go->isEnabled());
		go->trigger();
		CHECK(asked.isEmpty());
	}

	SECTION("a part whose symbol is gone says it, and hands out nothing")
	{
		selectOnly(editor, {QStringLiteral("k7")});
		REQUIRE(go->isEnabled());
		go->trigger();
		CHECK(asked.isEmpty());
		CHECK(editor.statusBar()->currentMessage().contains(QStringLiteral("n'existe plus")));
			//And the part keeps its link: the symbol may come back with an
			//undo on the folio, and an unlink here could not be undone there.
		CHECK(bench->mountingLayout().item(QStringLiteral("k7")).element == orphan.element);
	}

	SECTION("two parts selected are not one symbol")
	{
		selectOnly(editor, {QStringLiteral("q1"), QStringLiteral("k7")});
		CHECK_FALSE(go->isEnabled());
	}

	SECTION("the way back: the folio asks which part an element is")
	{
		CHECK(bench->mountingLayout().itemOfElement(target->uuid().toString())
		      == QStringLiteral("q1"));
			//An element mounted nowhere: the context menu offers nothing
		CHECK(bench->mountingLayout().itemOfElement(QStringLiteral("{00000000-0000-4000-8000-0000000000ff}")).isEmpty());
	}
}

TEST_CASE("T19 — the plate window frames the plate the first time it shows",
	  "[t44][calepinagem]")
{
		//Measured on the screen before the fix: 1 mm = 0.06 px, a plate of
		//600 by 300 drawn in a box of 36 pixels. The fit ran in the
		//constructor, before the view had a size.
	UiBench::Project bench(QStringLiteral("tremie_vibrante.qet"));
	REQUIRE(bench.isOpen());
	MountingLayout layout;
	REQUIRE_FALSE(layout.appendSurface(MountingSurface(QStringLiteral("QCM1"),
							   QStringLiteral("plate"),
							   MountingArea(600.0, 300.0))).isEmpty());
	bench->setMountingLayout(layout);

	MountingLayoutEditor editor(bench.project());
	editor.resize(1100, 800);
	editor.show();
	for (int i = 0; i < 5; ++i) {
		QCoreApplication::processEvents();
	}
		//600 mm across a view about a thousand pixels wide: over one pixel
		//a millimetre, and nowhere near the 0.06 of the bug
	CHECK(editor.view()->pixelsPerMillimetre() > 1.0);
}

/*
	Off the plate (passo 12 of T19): the elements of the folios that enter
	the bill of materials and are mounted on no face, and the drag that
	mounts one.
*/
namespace
{
	QString elementUuid(int index)
	{
		return QStringLiteral("{c0ffee00-0000-4000-8000-0000000000%1}")
				.arg(index, 2, 10, QLatin1Char('0'));
	}

	QString elementXml(int index, int x, const QString &informations)
	{
		return QStringLiteral(
			"<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/part.elmt\" uuid=\"%2\">"
			"<terminals/><inputs/><elementInformations>%3</elementInformations>"
			"<dynamic_texts/><texts_groups/></element>")
				.arg(x).arg(elementUuid(index), informations);
	}

	QString information(const QString &name, const QString &value)
	{
		return QStringLiteral("<elementInformation show=\"1\" name=\"%1\">%2</elementInformation>")
				.arg(name, value);
	}

	/// A breaker, a contactor with its part code, a terminal, and an element
	/// kept out of the bill of materials
	QString bomProjectXml()
	{
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">"
			"<element name=\"part.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"30\" height=\"20\" hotspot_x=\"15\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<uuid uuid=\"{7a1d0000-0000-4000-8000-000000000019}\"/>"
			"<names><name lang=\"en\">part</name></names>"
			"<description><rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"</description></definition></element>"
			"</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\""
			" cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\"><elements>")
			+ elementXml(1, 100, information(QStringLiteral("label"), QStringLiteral("Q1"))
					     + information(QStringLiteral("part_code"), QStringLiteral("GV2ME10")))
			+ elementXml(2, 250, information(QStringLiteral("label"), QStringLiteral("K1"))
					     + information(QStringLiteral("part_code"), QStringLiteral("LC1D09")))
			+ elementXml(3, 400, information(QStringLiteral("label"), QStringLiteral("X1")))
			+ elementXml(4, 550, information(QStringLiteral("label"), QStringLiteral("Y1"))
					     + information(QStringLiteral("exclude_from_bom"), QStringLiteral("true")))
			+ elementXml(5, 700, information(QStringLiteral("label"), QStringLiteral("Z1"))
					     + information(QStringLiteral("part_code"), QStringLiteral("DRAWN-ONLY")))
			+ QStringLiteral("</elements><inputs/><conductors/></diagram></project>");
	}

	/// The breaker on the plate, and the plate the face shown
	QString mountBreaker(QETProject *project)
	{
		MountingLayout layout;
		const QString face = layout.appendSurface(MountingSurface(QStringLiteral("QCM1"),
									  QStringLiteral("plate"),
									  MountingArea(600.0, 300.0)));
		MountedItem breaker(QStringLiteral("Q1"), QPointF(10, 10), QSizeF(45, 90));
		breaker.uuid = QStringLiteral("q1");
		breaker.part_code = QStringLiteral("GV2ME10");
		breaker.element = elementUuid(1);
		REQUIRE_FALSE(layout.mountItem(face, breaker).isEmpty());
		project->setMountingLayout(layout);
		return face;
	}

	MountedItem mountedOf(QETProject *project, const QString &element_uuid)
	{
		const MountingLayout layout = project->mountingLayout();
		const QString item = layout.itemOfElement(element_uuid);
		REQUIRE_FALSE(item.isEmpty());
		for (int index = 0 ; index < layout.count() ; ++index) {
			for (const MountedItem &mounted : layout.at(index).items) {
				if (mounted.uuid == item) {
					return mounted;
				}
			}
		}
		FAIL("the item is not on any face");
		return MountedItem();
	}
}

TEST_CASE("CU-19.15 — off the plate lists what enters the bill of materials and is not mounted",
	  "[t19][t44][calepinagem]")
{
	UiBench::ScratchProject scratch(bomProjectXml(), QStringLiteral("plate-off.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

		//The rule of the bill of materials: the element kept out of it is not
		//there, the other three are
	const QList<projectDataBase::BomElement> elements = scratch->dataBase()->bomElements();
	REQUIRE(elements.size() == 4);
	CHECK(elements.at(0).label == QStringLiteral("K1"));
	CHECK(elements.at(0).part_code == QStringLiteral("LC1D09"));

	mountBreaker(scratch.project());
	MountingLayoutEditor editor(scratch.project());
		//The breaker is on the plate, the excluded one is not in the list:
		//the contactor and the terminal are what is left to mount
	CHECK(editor.unmountedElements() == QStringList({elementUuid(2), elementUuid(3), elementUuid(5)}));
	auto *dock = editor.findChild<QDockWidget *>(QStringLiteral("unmounted_dock"));
	REQUIRE(dock);
	CHECK(dock->windowTitle().contains(QStringLiteral("3")));
}

TEST_CASE("T19 — mounting from off the plate: part code, box of the catalogue, label, link, one undo",
	  "[t19][t44][calepinagem]")
{
	Catalog catalog;
	QString error;
	REQUIRE(catalog.openInMemory(&error));
	CatalogPart contactor(QStringLiteral("LC1D09"), catalog.classByKey(QStringLiteral("contactor")).id);
	contactor.setValue(QStringLiteral("width"), QStringLiteral("45"));
	contactor.setValue(QStringLiteral("height"), QStringLiteral("80"));
	REQUIRE(catalog.savePart(contactor, &error));

	UiBench::ScratchProject scratch(bomProjectXml(), QStringLiteral("plate-mount.qet"));
	REQUIRE(scratch.isOpen());
	mountBreaker(scratch.project());
	MountingLayoutEditor editor(scratch.project());
	editor.setCatalog(&catalog);

	REQUIRE_FALSE(editor.mountElement(elementUuid(2), QPointF(100, 100), &error).isEmpty());
	const MountedItem mounted = mountedOf(scratch.project(), elementUuid(2));
	CHECK(mounted.label == QStringLiteral("K1"));
	CHECK(mounted.part_code == QStringLiteral("LC1D09"));
	CHECK(mounted.size == QSizeF(45, 80));
		//Centred where it was dropped
	CHECK(mounted.position == QPointF(77.5, 60));
	CHECK_FALSE(editor.unmountedElements().contains(elementUuid(2)));

		//Mounted once: the second time says so
	error.clear();
	CHECK(editor.mountElement(elementUuid(2), QPointF(200, 100), &error).isEmpty());
	CHECK_FALSE(error.isEmpty());
		//And what the bill of materials leaves out is not mounted from here
	error.clear();
	CHECK(editor.mountElement(elementUuid(4), QPointF(200, 100), &error).isEmpty());
	CHECK_FALSE(error.isEmpty());

		//One step of undo takes it away and puts it back on the list, and
		//the box of faces counts what the face holds again
	QComboBox *faces = nullptr;
	for (QComboBox *box : editor.findChildren<QComboBox *>()) {
		if (box->currentText().contains(QStringLiteral("composant"))) {
			faces = box;
		}
	}
	REQUIRE(faces);
	CHECK(faces->currentText().contains(QStringLiteral("2 composant")));
	editor.scene()->undoStack().undo();
	CHECK(scratch->mountingLayout().itemOfElement(elementUuid(2)).isEmpty());
	CHECK(editor.unmountedElements().contains(elementUuid(2)));
	CHECK(faces->currentText().contains(QStringLiteral("1 composant")));

		//A part nobody measured that has a drawing is drawn as its drawing,
		//and that is what lands centred where it was dropped
	CatalogPart drawn_only(QStringLiteral("DRAWN-ONLY"), catalog.classByKey(QStringLiteral("contactor")).id);
	drawn_only.layout_drawing = QStringLiteral(
		"<layout_drawing><rect x='0' y='0' width='30' height='60' rx='0' ry='0'"
		" style='line-style:normal;line-weight:normal;filling:none;color:black'/></layout_drawing>");
	REQUIRE(catalog.savePart(drawn_only, &error));
	const QString z1 = editor.mountElement(elementUuid(5), QPointF(300, 150), &error);
	REQUIRE_FALSE(z1.isEmpty());
	MountedPartItem *on_plate = editor.scene()->partItem(z1);
	REQUIRE(on_plate);
	REQUIRE(on_plate->hasLayoutDrawing());
	const QPointF centre = on_plate->mapRectToScene(on_plate->layoutDrawingRect()).center();
	CHECK(centre.x() == Approx(300));
	CHECK(centre.y() == Approx(150));
}

TEST_CASE("T19 — a row of off the plate dropped on the plate is mounted where it fell",
	  "[t19][t44][calepinagem]")
{
	UiBench::ScratchProject scratch(bomProjectXml(), QStringLiteral("plate-drop.qet"));
	REQUIRE(scratch.isOpen());
	mountBreaker(scratch.project());
	MountingLayoutEditor editor(scratch.project());
	editor.resize(1100, 800);
	editor.show();
	for (int i = 0 ; i < 5 ; ++i) {
		QCoreApplication::processEvents();
	}

		//What the list hands to a drag, as the list builds it
	auto *list = editor.findChild<QListWidget *>(QStringLiteral("unmounted_list"));
	REQUIRE(list);
	int row = -1;
	for (int index = 0 ; index < list->count() ; ++index) {
		if (list->item(index)->data(Qt::UserRole).toString() == elementUuid(3)) {
			row = index;
		}
	}
	REQUIRE(row >= 0);
	QMimeData *data = list->model()->mimeData({list->model()->index(row, 0)});
	REQUIRE(data);
	REQUIRE(data->hasFormat(MountingView::unmountedElementMimeType()));

		//And dropped on the plate, through the events a drop is made of
	QWidget *viewport = editor.view()->viewport();
	const QPoint at = editor.view()->mapFromScene(QPointF(300, 150));
	QDragEnterEvent enter(at, Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(viewport, &enter);
	CHECK(enter.isAccepted());
		//The move is what decides: the scene takes in every drag that
		//enters, and a drop is only let through where the last move was
		//accepted
	QDragMoveEvent move(at, Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(viewport, &move);
	CHECK(move.isAccepted());
	QDropEvent drop(QPointF(at), Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(viewport, &drop);
	delete data;

		//The terminal has no part code: a part nobody measured, centred
		//where it fell, and linked to its element
	const MountedItem mounted = mountedOf(scratch.project(), elementUuid(3));
	CHECK(mounted.label == QStringLiteral("X1"));
	const qreal side = MountedPartItem::unmeasuredMarkerSide();
	CHECK(mounted.position.x() + side / 2.0 == Approx(300).margin(2.0));
	CHECK(mounted.position.y() + side / 2.0 == Approx(150).margin(2.0));
	CHECK_FALSE(editor.unmountedElements().contains(elementUuid(3)));
}

/*
	Off the folios (passo 12 of T19, the other list): the parts mounted on a
	face that no folio draws, and why - laid with no symbol, or with their
	symbol deleted from the folio after they were mounted.
*/
namespace
{
	const QString GONE_ELEMENT = QStringLiteral("{00000000-dead-4000-8000-000000000000}");

	/// On the plate: the breaker and the contactor, whose symbols are on the
	/// folio; the element kept out of the bill of materials, drawn all the
	/// same; a part laid from the catalogue with no symbol; a rail. On the
	/// door: a lamp whose symbol no folio holds.
	MountingLayout undrawnLayout(QString *plate, QString *door)
	{
		MountingLayout layout;
		*plate = layout.appendSurface(MountingSurface(QStringLiteral("QCM1"),
							      QStringLiteral("plate"),
							      MountingArea(600.0, 300.0)));
		*door = layout.appendSurface(MountingSurface(QStringLiteral("QCM1"),
							     QStringLiteral("door"),
							     MountingArea(600.0, 800.0)));

		MountedItem breaker(QStringLiteral("Q1"), QPointF(10, 10), QSizeF(45, 90));
		breaker.uuid = QStringLiteral("q1");
		breaker.part_code = QStringLiteral("GV2ME10");
		breaker.element = elementUuid(1);

		MountedItem kept_out(QStringLiteral("Y1"), QPointF(60, 10), QSizeF(20, 20));
		kept_out.uuid = QStringLiteral("y1");
		kept_out.element = elementUuid(4);

		MountedItem contactor(QStringLiteral("K1"), QPointF(100, 10), QSizeF(45, 80));
		contactor.uuid = QStringLiteral("k1");
		contactor.part_code = QStringLiteral("LC1D09");
		contactor.element = elementUuid(2);

		MountedItem loose(QString(), QPointF(200, 10), QSizeF(6, 45));
		loose.uuid = QStringLiteral("x9");
		loose.part_code = QStringLiteral("ACC-PE");

		MountedItem rail;
		rail.uuid = QStringLiteral("rail");
		rail.profile = MountingProfile::standardRails().first();
		rail.position = QPointF(10, 150);
		rail.size = rail.profile.sizeFor(500.0, MountingRun::Across);

		MountedItem lamp(QStringLiteral("H1"), QPointF(10, 10), QSizeF(30, 30));
		lamp.uuid = QStringLiteral("h1");
		lamp.element = GONE_ELEMENT;

		for (const MountedItem &item : {breaker, kept_out, contactor, loose, rail}) {
			REQUIRE_FALSE(layout.mountItem(*plate, item).isEmpty());
		}
		REQUIRE_FALSE(layout.mountItem(*door, lamp).isEmpty());
		return layout;
	}

	/// What the folio of the bench project draws, of what the plate names
	QSet<QUuid> benchDrawn()
	{
		return {QUuid(elementUuid(1)), QUuid(elementUuid(2)), QUuid(elementUuid(4))};
	}

	QStringList uuidsOf(const QList<UndrawnPart> &parts)
	{
		QStringList uuids;
		for (const UndrawnPart &part : parts) {
			uuids << part.item.uuid;
		}
		return uuids;
	}

	QListWidgetItem *rowOf(QListWidget *list, const QString &item_uuid)
	{
		for (int row = 0 ; row < list->count() ; ++row) {
			if (list->item(row)->data(Qt::UserRole).toString() == item_uuid) {
				return list->item(row);
			}
		}
		FAIL("no row of the list for " << item_uuid.toStdString());
		return nullptr;
	}

	/// The window brought back to the front, as when somebody returns to it
	/// from a folio: another window first, so that there is a change of
	/// activation to come back with
	bool bringBack(QWidget &window)
	{
		QWidget elsewhere;
		elsewhere.resize(200, 100);
		elsewhere.show();
		elsewhere.activateWindow();
		if (!QTest::qWaitForWindowActive(&elsewhere)) {
			return false;
		}
		window.activateWindow();
		return QTest::qWaitForWindowActive(&window);
	}
}

TEST_CASE("T19 — off the folios: a part laid with no symbol and a part whose symbol is gone, told apart",
	  "[t19][calepinagem]")
{
	QString plate;
	QString door;
	const MountingLayout layout = undrawnLayout(&plate, &door);

	SECTION("the two reasons, and nothing drawn is listed")
	{
		const QList<UndrawnPart> undrawn = UndrawnPart::of(layout, benchDrawn());
			//Not the breaker nor the contactor, whose symbols are on the folio;
			//not the element kept out of the bill of materials, which is drawn
			//all the same; not the rail, which has no symbol by nature
		REQUIRE(uuidsOf(undrawn) == QStringList({QStringLiteral("x9"), QStringLiteral("h1")}));
		CHECK(undrawn.at(0).reason == UndrawnPart::Reason::NoSymbol);
		CHECK(undrawn.at(0).surface_uuid == plate);
		CHECK(undrawn.at(1).reason == UndrawnPart::Reason::SymbolGone);
		CHECK(undrawn.at(1).surface_uuid == door);
			//The link is handed back as it is: nothing here clears it
		CHECK(undrawn.at(1).item.element == GONE_ELEMENT);
	}

	SECTION("a symbol deleted after mounting makes its part a symbol gone, in the order of the plate")
	{
		QSet<QUuid> drawn = benchDrawn();
		drawn.remove(QUuid(elementUuid(2)));
		const QList<UndrawnPart> undrawn = UndrawnPart::of(layout, drawn);
		REQUIRE(uuidsOf(undrawn) == QStringList({QStringLiteral("k1"), QStringLiteral("x9"),
							 QStringLiteral("h1")}));
		CHECK(undrawn.at(0).reason == UndrawnPart::Reason::SymbolGone);
		CHECK(undrawn.at(1).reason == UndrawnPart::Reason::NoSymbol);
	}

	SECTION("with no folio at all, every linked part is gone and the loose one has no symbol")
	{
		const QList<UndrawnPart> undrawn = UndrawnPart::of(layout, {});
		REQUIRE(uuidsOf(undrawn) == QStringList({QStringLiteral("q1"), QStringLiteral("y1"),
							 QStringLiteral("k1"), QStringLiteral("x9"),
							 QStringLiteral("h1")}));
		CHECK(undrawn.at(3).reason == UndrawnPart::Reason::NoSymbol);
		for (int index : {0, 1, 2, 4}) {
			CHECK(undrawn.at(index).reason == UndrawnPart::Reason::SymbolGone);
		}
	}

	SECTION("a rail that names a gone element is a broken link like any other")
	{
		MountingLayout linked = layout;
		MountedItem rail = linked.item(QStringLiteral("rail"));
		rail.element = GONE_ELEMENT;
		REQUIRE(linked.updateItem(rail));
		const QList<UndrawnPart> undrawn = UndrawnPart::of(linked, benchDrawn());
		REQUIRE(uuidsOf(undrawn) == QStringList({QStringLiteral("x9"), QStringLiteral("rail"),
							 QStringLiteral("h1")}));
		CHECK(undrawn.at(1).reason == UndrawnPart::Reason::SymbolGone);
	}

	SECTION("the link is read leniently, and a mangled one is a symbol gone, not no symbol")
	{
		MountingLayout edited = layout;
			//The same element without its braces is still drawn
		MountedItem breaker = edited.item(QStringLiteral("q1"));
		breaker.element = elementUuid(1).mid(1, 36);
		REQUIRE(edited.updateItem(breaker));
			//A link that is not a uuid names something no folio answers to
		MountedItem kept_out = edited.item(QStringLiteral("y1"));
		kept_out.element = QStringLiteral("not a uuid");
		REQUIRE(edited.updateItem(kept_out));

		const QList<UndrawnPart> undrawn = UndrawnPart::of(edited, benchDrawn());
		REQUIRE(uuidsOf(undrawn) == QStringList({QStringLiteral("y1"), QStringLiteral("x9"),
							 QStringLiteral("h1")}));
		CHECK(undrawn.at(0).reason == UndrawnPart::Reason::SymbolGone);
	}

	SECTION("the folios are asked only about the elements the layout names, each once")
	{
		CHECK(UndrawnPart::namedElements(layout)
		      == QList<QUuid>({QUuid(elementUuid(1)), QUuid(elementUuid(4)),
				       QUuid(elementUuid(2)), QUuid(GONE_ELEMENT)}));
	}
}

TEST_CASE("T19 — « Hors des folios » in the plate window: the count, the reason, and a row selecting its part",
	  "[t19][calepinagem]")
{
	UiBench::ScratchProject scratch(bomProjectXml(), QStringLiteral("plate-undrawn.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	QString plate;
	QString door;
	scratch->setMountingLayout(undrawnLayout(&plate, &door));

	MountingLayoutEditor editor(scratch.project());
	REQUIRE(editor.shownSurface() == plate);
	auto *dock = editor.findChild<QDockWidget *>(QStringLiteral("undrawn_dock"));
	REQUIRE(dock);
	auto *list = editor.findChild<QListWidget *>(QStringLiteral("undrawn_list"));
	REQUIRE(list);

	REQUIRE(editor.undrawnItems() == QStringList({QStringLiteral("x9"), QStringLiteral("h1")}));
	CHECK(dock->windowTitle() == QStringLiteral("Hors des folios (2)"));
	CHECK(rowOf(list, QStringLiteral("x9"))->text() == QStringLiteral("ACC-PE (sans symbole)"));
	CHECK(rowOf(list, QStringLiteral("h1"))->text() == QStringLiteral("H1 (symbole supprimé)"));
	CHECK(rowOf(list, QStringLiteral("x9"))->data(Qt::UserRole + 1).toInt()
	      == static_cast<int>(UndrawnPart::Reason::NoSymbol));
	CHECK(rowOf(list, QStringLiteral("h1"))->data(Qt::UserRole + 1).toInt()
	      == static_cast<int>(UndrawnPart::Reason::SymbolGone));
		//And the list beside it is the other half, untouched: what the folios
		//draw and no face holds
	CHECK(editor.unmountedElements() == QStringList({elementUuid(3), elementUuid(5)}));

	SECTION("a row selects its part on the plate, the face changed first when it is on another one")
	{
			//A step on the stack first, so that drawing the door empties it
			//and that refreshes the list under the row being activated
		REQUIRE(editor.scene()->moveItem(QStringLiteral("q1"), QPointF(12, 12)));

		emit list->itemActivated(rowOf(list, QStringLiteral("h1")));
		QCoreApplication::processEvents();
		CHECK(editor.shownSurface() == door);
		CHECK(editor.scene()->selectedUuids() == QStringList({QStringLiteral("h1")}));

		emit list->itemActivated(rowOf(list, QStringLiteral("x9")));
		QCoreApplication::processEvents();
		CHECK(editor.shownSurface() == plate);
		CHECK(editor.scene()->selectedUuids() == QStringList({QStringLiteral("x9")}));
	}

	SECTION("the plate changes: a part taken off leaves the list at once, and an undo brings it back")
	{
		REQUIRE(editor.scene()->unmountItem(QStringLiteral("x9")));
		CHECK(editor.undrawnItems() == QStringList({QStringLiteral("h1")}));
		CHECK(dock->windowTitle() == QStringLiteral("Hors des folios (1)"));

		editor.scene()->undoStack().undo();
		CHECK(editor.undrawnItems() == QStringList({QStringLiteral("x9"), QStringLiteral("h1")}));
		CHECK(dock->windowTitle() == QStringLiteral("Hors des folios (2)"));
	}

	SECTION("the folio changes: a symbol deleted after mounting joins the list when the window "
		"comes back to the front, and its undo takes it out again")
	{
		editor.resize(1100, 800);
		editor.show();
		REQUIRE(bringBack(editor));

		Diagram *sheet = scratch.diagram(0);
		REQUIRE(sheet);
		Element *contactor = nullptr;
		for (Element *element : sheet->elements()) {
			if (element->uuid() == QUuid(elementUuid(2))) {
				contactor = element;
			}
		}
		REQUIRE(contactor);

			//Deleted the way the folio deletes it: a step of the project
		DiagramContent doomed;
		doomed.m_elements << contactor;
		sheet->undoStack().push(new DeleteQGraphicsItemCommand(sheet, doomed));
		REQUIRE_FALSE(sheet->elements().contains(contactor));

		REQUIRE(bringBack(editor));
		REQUIRE(editor.undrawnItems() == QStringList({QStringLiteral("k1"), QStringLiteral("x9"),
							      QStringLiteral("h1")}));
		CHECK(rowOf(list, QStringLiteral("k1"))->text()
		      == QStringLiteral("K1 — LC1D09 (symbole supprimé)"));
		CHECK(dock->windowTitle() == QStringLiteral("Hors des folios (3)"));
			//The part keeps its link: the undo below is what brings it back
		CHECK(scratch->mountingLayout().item(QStringLiteral("k1")).element == elementUuid(2));

			//The list and « Aller au symbole » ask the same question, and give
			//the same answer
		emit list->itemActivated(rowOf(list, QStringLiteral("k1")));
		QCoreApplication::processEvents();
		REQUIRE(editor.scene()->selectedUuids() == QStringList({QStringLiteral("k1")}));
		QAction *go = goToSymbolAction(editor);
		REQUIRE(go->isEnabled());
		go->trigger();
		CHECK(editor.statusBar()->currentMessage().contains(QStringLiteral("n'existe plus")));

		sheet->undoStack().undo();
		REQUIRE(sheet->elements().contains(contactor));
		REQUIRE(bringBack(editor));
		CHECK(editor.undrawnItems() == QStringList({QStringLiteral("x9"), QStringLiteral("h1")}));
	}
}
