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
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountinglayouteditor.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include <catch2/catch.hpp>
#include <QAction>
#include <QSignalSpy>
#include <QStatusBar>

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
