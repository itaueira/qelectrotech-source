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
#include <catch2/catch.hpp>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "qt_catch_tostring.h"

#include "../../../sources/utils/previousversions.h"

/*
	T25 - where the versions of a project kept at each save go.

	A copy left beside the project at every save, or at every opening as the
	program used to make, fills the folder people share and send with files
	nobody asked for: seven of fifteen megabytes in one afternoon, measured.
	The versions go in a folder of the user's, one sub-folder per project.

	Every case puts the folder of versions in a temporary directory, so that
	the documents of whoever runs the suite are never written to.
*/

namespace
{
		/// @p path holding @p text, saved at @p when
	void write(const QString &path, const QByteArray &text, const QDateTime &when)
	{
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		file.write(text);
		file.flush();
		REQUIRE(file.setFileTime(when, QFileDevice::FileModificationTime));
	}

		/// The folder of versions in @p dir for the time of a case, and the
		/// one of the run back after it (main.cpp)
	struct VersionsIn
	{
		explicit VersionsIn(const QTemporaryDir &dir) : m_before(PreviousVersions::root())
		{
			PreviousVersions::setRoot(dir.filePath(QStringLiteral("versions")));
		}
		~VersionsIn() { PreviousVersions::setRoot(m_before); }
		const QString m_before;
	};
}

TEST_CASE("T25 — the versions of a project go to the folder of versions, and nothing beside it",
		  "[T25][previousversions]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const VersionsIn versions(dir);
	const QString project = dir.filePath(QStringLiteral("projects/plant.qetz"));
	const QDateTime start(QDate(2026, 10, 16), QTime(8, 0, 0));

	write(project, "first", start);
	REQUIRE(PreviousVersions::keep(project, 5));
	write(project, "second", start.addSecs(60));
	REQUIRE(PreviousVersions::keep(project, 5));

	const QStringList kept = PreviousVersions::kept(project);
	REQUIRE(kept.size() == 2);
	for (const QString &version : kept) {
		CHECK(version.startsWith(PreviousVersions::root() + QLatin1Char('/')));
	}
	CHECK(QFileInfo(PreviousVersions::folder(project)).fileName().startsWith(QStringLiteral("plant (")));

		//The project's folder holds the project, and nothing else
	const QStringList beside = QDir(QFileInfo(project).absolutePath())
			.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
	CHECK(beside == QStringList{QStringLiteral("plant.qetz")});
}

TEST_CASE("T25 — two projects of the same name in two folders keep their versions apart",
		  "[T25][previousversions]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const VersionsIn versions(dir);
	const QString first = dir.filePath(QStringLiteral("client A/panel.qetz"));
	const QString second = dir.filePath(QStringLiteral("client B/panel.qetz"));
	CHECK(PreviousVersions::folder(first) != PreviousVersions::folder(second));

		//Three versions of each with two kept: neither drops the other's
	const QDateTime start(QDate(2026, 10, 16), QTime(9, 0, 0));
	for (int i = 0 ; i < 3 ; ++i) {
		write(first, "A " + QByteArray::number(i), start.addSecs(60 * i));
		REQUIRE(PreviousVersions::keep(first, 2));
		write(second, "B " + QByteArray::number(i), start.addSecs(60 * i + 30));
		REQUIRE(PreviousVersions::keep(second, 2));
	}
	CHECK(PreviousVersions::kept(first).size() == 2);
	CHECK(PreviousVersions::kept(second).size() == 2);
}

TEST_CASE("T25 — the folder of a project does not depend on how its path is written",
		  "[T25][previousversions]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const VersionsIn versions(dir);
	const QString plain = dir.filePath(QStringLiteral("projects/plant.qetz"));
	const QString dotted = dir.filePath(QStringLiteral("projects/./other/../plant.qetz"));
	CHECK(PreviousVersions::folder(plain) == PreviousVersions::folder(dotted));
		//A .qet and a .qetz of the same project are versions of it alike
	CHECK(PreviousVersions::folder(plain)
		  == PreviousVersions::folder(dir.filePath(QStringLiteral("projects/plant.qet"))));
#ifdef Q_OS_WIN
		//The same file on Windows, and the same folder: the name keeps the
		//case it was written in, which Windows does not tell apart
	CHECK(PreviousVersions::folder(plain).compare(PreviousVersions::folder(plain.toUpper()),
												  Qt::CaseInsensitive) == 0);
#endif
}

TEST_CASE("T25 — by default the versions are in the user's documents",
		  "[T25][previousversions]")
{
	const QString run = PreviousVersions::root();
	PreviousVersions::setRoot(QString());
	const QString root = PreviousVersions::root();
	PreviousVersions::setRoot(run);

	const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	REQUIRE_FALSE(documents.isEmpty());
	CHECK(root == QDir(documents).filePath(QStringLiteral("QElectroTech/Previous versions")));
		//And the run itself writes into none of it
	CHECK_FALSE(run.startsWith(documents));
}
