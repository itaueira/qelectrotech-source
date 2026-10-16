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
#include "../../../sources/catalog/catalog.h"
#include "../../../sources/environment/qetenvironment.h"
#include "qt_catch_tostring.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
	/**
		Put the environment setting back where it was.

		The test binary has no organisation or application name of its own, so
		QSettings writes to a file named after this executable and not to the
		settings of QElectroTech - but leaving rubbish behind in one's own
		settings is still rubbish.
	*/
	class SettingsGuard
	{
		public:
			SettingsGuard()
			{
				QSettings settings;
				m_previous = settings.value(QStringLiteral("environment/path")).toString();
			}

			~SettingsGuard()
			{
				QSettings settings;
				if (m_previous.isEmpty()) {
					settings.remove(QStringLiteral("environment/path"));
				} else {
					settings.setValue(QStringLiteral("environment/path"), m_previous);
				}
			}

		private:
			QString m_previous;
	};

	void writeFile(const QString &path, const QString &content)
	{
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
		QTextStream stream(&file);
		stream << content;
	}

	int countFiles(const QString &root)
	{
		int count = 0;
		QDirIterator iterator(root, QDir::Files | QDir::NoDotAndDotDot,
				      QDirIterator::Subdirectories);
		while (iterator.hasNext())
		{
			iterator.next();
			++count;
		}
		return count;
	}
}

TEST_CASE("CU-38.1 — pointer le programme sur un dossier suffit")
{
	SettingsGuard guard;
	QTemporaryDir share;
	REQUIRE(share.isValid());

	const QString wanted = share.filePath(QStringLiteral("ambiente-da-empresa"));
	CHECK_FALSE(QETEnvironment::looksLikeEnvironment(wanted));

	QString error;
	REQUIRE(QETEnvironment::setPath(wanted, &error));
	CHECK(QETEnvironment::isConfigured());
	CHECK(QDir(QETEnvironment::path()) == QDir(wanted));

	// Pointing at an empty folder is enough: the skeleton is created, so
	// nobody has to know which sub-folders to make by hand.
	CHECK(QETEnvironment::looksLikeEnvironment(wanted));
	const QStringList folders = QETEnvironment::skeletonFolders();
	for (const QString &folder : folders) {
		CHECK(QDir(wanted).exists(folder));
	}

	// Everything hangs off that one path.
	CHECK(QETEnvironment::projectsDir().startsWith(wanted));
	CHECK(QETEnvironment::elementsDir().startsWith(wanted));
	CHECK(QETEnvironment::companyElementsDir().startsWith(wanted));
	CHECK(QETEnvironment::titleBlocksDir().startsWith(wanted));
	CHECK(QETEnvironment::macrosDir().startsWith(wanted));
	CHECK(QETEnvironment::groupingsDir().startsWith(wanted));
	CHECK(QETEnvironment::catalogFile().startsWith(wanted));

	// Emptying the setting goes back to the default instead of leaving the
	// program pointing at nothing.
	REQUIRE(QETEnvironment::setPath(QString(), &error));
	CHECK_FALSE(QETEnvironment::isConfigured());
	CHECK(QDir(QETEnvironment::path()) == QDir(QETEnvironment::defaultPath()));
}

TEST_CASE("CU-38.2 — le catalogue de l'environnement est celui que les deux postes ouvrent")
{
	SettingsGuard guard;
	QTemporaryDir share;
	REQUIRE(share.isValid());

	QString error;
	REQUIRE(QETEnvironment::setPath(share.path(), &error));

	// Two stations pointing at the same environment open the same catalog
	// file, which is the whole reason the catalog lives in there.
	Catalog first_station;
	Catalog second_station;
	REQUIRE(first_station.open(QETEnvironment::catalogFile(), &error));
	REQUIRE(second_station.open(QETEnvironment::catalogFile(), &error));

	const int contactor_id = first_station.classByKey(QStringLiteral("contactor")).id;
	REQUIRE(contactor_id > 0);
	CatalogPart part(QStringLiteral("CONT-AMBIENTE-1"), contactor_id);
	REQUIRE(first_station.savePart(part, &error));

	CHECK_FALSE(second_station.partByCode(QStringLiteral("CONT-AMBIENTE-1")).isNull());
	CHECK(first_station.filePath() == second_station.filePath());
}

TEST_CASE("CU-38.6 — la copie de sauvegarde emporte tout, et refuse d'être à l'intérieur")
{
	SettingsGuard guard;
	QTemporaryDir share;
	QTemporaryDir backup;
	REQUIRE(share.isValid());
	REQUIRE(backup.isValid());

	QString error;
	REQUIRE(QETEnvironment::setPath(share.path(), &error));

	// A library symbol, a company symbol, a title block and a project: the
	// backup is what protects these, not only the projects.
	writeFile(QETEnvironment::elementsDir() + QStringLiteral("/contator.elmt"),
		  QStringLiteral("<definition/>"));
	writeFile(QETEnvironment::companyElementsDir() + QStringLiteral("/simbolo-da-empresa.elmt"),
		  QStringLiteral("<definition/>"));
	writeFile(QETEnvironment::titleBlocksDir() + QStringLiteral("/carimbo-da-empresa.titleblock"),
		  QStringLiteral("<titleblock/>"));
	writeFile(QETEnvironment::projectsDir() + QStringLiteral("/quadro.qet"),
		  QStringLiteral("<project/>"));

	Catalog catalog;
	REQUIRE(catalog.open(QETEnvironment::catalogFile(), &error));
	catalog.close();

	const int expected = countFiles(share.path());
	CHECK(expected >= 5);    // four files above plus the catalog

	const QString destination = backup.filePath(QStringLiteral("ambiente-2026-08-20"));
	const int copied = QETEnvironment::copyTo(destination, &error);
	CHECK(error.isEmpty());
	CHECK(copied == expected);
	CHECK(countFiles(destination) == expected);
	CHECK(QFile::exists(destination + QStringLiteral("/elements/contator.elmt")));
	CHECK(QFile::exists(destination + QStringLiteral("/catalog.sqlite")));

	// Running it twice must end up with the current state, not fail.
	CHECK(QETEnvironment::copyTo(destination, &error) == expected);

	// A copy inside the folder it protects is not a copy: it goes down with it.
	error.clear();
	CHECK(QETEnvironment::copyTo(QETEnvironment::projectsDir()
				     + QStringLiteral("/copia"), &error) == -1);
	CHECK_FALSE(error.isEmpty());
	error.clear();
	CHECK(QETEnvironment::copyTo(share.path(), &error) == -1);
	CHECK_FALSE(error.isEmpty());
}
