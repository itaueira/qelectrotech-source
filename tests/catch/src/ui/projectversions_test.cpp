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
#include "uibench.h"

#include "../../../../sources/projectlock.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetresult.h"
#include "../../../../sources/utils/previousversions.h"

#include <catch2/catch.hpp>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

/*
	T25 - a project saved through QETProject::write(), the way the program
	saves it, leaves nothing in its folder but itself: the version kept
	before each save is in the folder of versions. A project opened from
	a .qet is saved to that .qet (since the upstream's 89c397995; it used
	to be saved as a .qetz beside it, and the first save then kept
	nothing), so two saves keep two versions.
*/

TEST_CASE("CU-25.12 — Saving leaves nothing in the project's folder",
		  "[T25][previousversions]")
{
	QTemporaryDir versions;
	REQUIRE(versions.isValid());
	const QString run_root = PreviousVersions::root();
	PreviousVersions::setRoot(versions.path());
	struct Restore
	{
		QString root;
		~Restore() { PreviousVersions::setRoot(root); }
	} restore{run_root};

	UiBench::ScratchProject bench(UiBench::fileContent(UiBench::examplePath(QStringLiteral("741.qet"))),
								  QStringLiteral("741.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const QString opened = project->filePath();

	project->setTitle(QStringLiteral("first change"));
	REQUIRE(project->write().isOk());
	project->setTitle(QStringLiteral("second change"));
	REQUIRE(project->write().isOk());

	const QString saved = project->filePath();
	const QStringList kept = PreviousVersions::kept(saved);
	CHECK(kept.size() == 2);
	for (const QString &version : kept) {
		CHECK(version.startsWith(QDir(versions.path()).absolutePath()));
	}

		//The project's folder: the file opened, the one the program saves
		//it as (the same one now), and the lock while it is open - nothing
		//else
	const QStringList allowed = {QFileInfo(opened).fileName(),
								 QFileInfo(saved).fileName(),
								 QFileInfo(ProjectLock::lockPathFor(saved)).fileName()};
	const QStringList beside = QDir(QFileInfo(saved).absolutePath())
			.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
	REQUIRE_FALSE(beside.isEmpty());
	for (const QString &entry : beside) {
		CAPTURE(entry.toStdString());
		CHECK(allowed.contains(entry));
	}
}
