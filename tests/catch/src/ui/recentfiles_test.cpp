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
#include "../qt_catch_tostring.h"

#include "../../../../sources/recentfiles.h"

#include <catch2/catch.hpp>

#include <QAction>
#include <QDir>
#include <QMenu>

/*
	A project opened from the list of recent files (T57).

	Opening the project puts it at the top of the list, which builds the menu
	again while the path handed over by the menu is still being read: the
	diagram editor reads it once more, to copy the file it has just opened.
	The menu keeps every item it ever made in its table of mappings, so that
	table grows on each rebuild, and when it grows past its capacity it moves
	elsewhere in memory. A path that pointed into it is then gone.
*/
TEST_CASE("CU-57.1 — Whoever opens from the list gets a path that stays valid",
	  "[t57][recentfiles]")
{
		//Nothing is written to the settings: the list is saved only by save()
	RecentFiles recent(QStringLiteral("t57-bench"), 10);
	recent.clear();

	auto path = [](const QString &name) {
		return QDir::toNativeSeparators(QStringLiteral("C:/projects/") + name);
	};
	for (int i = 9 ; i >= 0 ; --i) {
		recent.fileWasOpened(path(QStringLiteral("panel-%1.qet").arg(i)));
	}

	const QString wanted = path(QStringLiteral("panel-0.qet"));
	QAction *clicked = nullptr;
	for (QAction *action : recent.menu()->actions())
	{
		if (action->text() == wanted) {
			clicked = action;
		}
	}
	REQUIRE(clicked != nullptr);

	int requests = 0;
	QString read_afterwards;
	QObject::connect(&recent, &RecentFiles::fileOpeningRequested,
			 [&](const QString &requested) {
			//What the editor does while it opens, many times over, so that
			//the table of the menu grows past its capacity more than once
		for (int i = 0 ; i < 40 ; ++i) {
			recent.fileWasOpened(path(QStringLiteral("other-%1.qet").arg(i)));
		}
		read_afterwards = requested;
		++requests;
	});

	clicked->trigger();

	CHECK(requests == 1);
	CHECK(read_afterwards == wanted);
}
