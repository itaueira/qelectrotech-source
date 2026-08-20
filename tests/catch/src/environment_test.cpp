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
#include "../../../sources/environment/filediskstate.h"
#include "../../../sources/environment/projectlock.h"
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

	const QString wanted = share.filePath(QStringLiteral("ambiente-acme"));
	CHECK_FALSE(QETEnvironment::looksLikeEnvironment(wanted));

	QString error;
	REQUIRE(QETEnvironment::setPath(wanted, &error));
	CHECK(QETEnvironment::isConfigured());
	CHECK(QDir(QETEnvironment::path()) == QDir(wanted));

	// Pointing at an empty folder is enough: the skeleton is created, so
	// nobody has to know which six sub-folders to make by hand.
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

TEST_CASE("CU-38.4 — le verrou dit qui a le projet, et depuis quand")
{
	QTemporaryDir share;
	REQUIRE(share.isValid());
	const QString project = share.filePath(QStringLiteral("quadro.qet"));
	writeFile(project, QStringLiteral("<project/>"));

	ProjectLock lock(project);
	CHECK(lock.lockFilePath() == project + ProjectLock::lockSuffix());
	CHECK_FALSE(lock.exists());

	QString error;
	REQUIRE(lock.acquire(&error));
	CHECK(lock.exists());

	const ProjectLock::Holder holder = lock.holder();
	CHECK(holder.user == ProjectLock::currentUser());
	CHECK(holder.machine == ProjectLock::currentMachine());
	CHECK(holder.pid == QCoreApplication::applicationPid());
	CHECK_FALSE(holder.isNull());
	CHECK(holder.isThisMachine());
	CHECK(holder.isThisProcess());
	CHECK_FALSE(holder.description().isEmpty());
	CHECK(holder.description().contains(ProjectLock::currentUser()));

	// Our own lock never blocks us: reopening after a crash must not need a
	// trip to the file manager.
	CHECK(lock.isStale());
	ProjectLock same_file(project);
	CHECK(same_file.acquire());

	CHECK(lock.release());
	CHECK_FALSE(lock.exists());
	// Releasing a lock that is not there is success, not failure.
	CHECK(lock.release());
}

TEST_CASE("CU-38.4 — un verrou d'une autre machine n'est jamais déclaré périmé")
{
	QTemporaryDir share;
	REQUIRE(share.isValid());
	const QString project = share.filePath(QStringLiteral("quadro.qet"));
	writeFile(project, QStringLiteral("<project/>"));

	// A colleague's session, on a machine this one knows nothing about. There
	// is no way to tell from here whether it is alive, and guessing wrong is
	// exactly the loss this mechanism exists to prevent - so it is never
	// guessed.
	writeFile(project + ProjectLock::lockSuffix(),
		  QStringLiteral("user=colega\n"
				 "machine=POSTO-DA-ENGENHARIA\n"
				 "pid=4242\n"
				 "since=2026-08-20T08:15:00\n"));

	ProjectLock lock(project);
	REQUIRE(lock.exists());
	CHECK_FALSE(lock.isStale());

	const ProjectLock::Holder holder = lock.holder();
	CHECK(holder.user == QStringLiteral("colega"));
	CHECK(holder.machine == QStringLiteral("POSTO-DA-ENGENHARIA"));
	CHECK(holder.pid == 4242);
	CHECK_FALSE(holder.isThisMachine());
	CHECK_FALSE(holder.isThisProcess());
	CHECK(holder.description().contains(QStringLiteral("colega")));
	CHECK(holder.description().contains(QStringLiteral("POSTO-DA-ENGENHARIA")));

	// Acquiring is refused, and the reason names who has it.
	QString error;
	CHECK_FALSE(lock.acquire(&error));
	CHECK(error.contains(QStringLiteral("colega")));

	// Releasing somebody else's lock takes a decision, so release() will not.
	CHECK_FALSE(lock.release());
	CHECK(lock.exists());
	CHECK(lock.forceRelease());
	CHECK_FALSE(lock.exists());
}

TEST_CASE("CU-38.4 — un verrou mort de cette machine ne bloque personne")
{
	QTemporaryDir share;
	REQUIRE(share.isValid());
	const QString project = share.filePath(QStringLiteral("quadro.qet"));
	writeFile(project, QStringLiteral("<project/>"));

	// This machine, a process id that cannot be running: the machine that
	// crashed yesterday must not lock its own project forever.
	writeFile(project + ProjectLock::lockSuffix(),
		  QStringLiteral("user=%1\nmachine=%2\npid=999999999\nsince=2026-08-19T18:00:00\n")
			  .arg(ProjectLock::currentUser(), ProjectLock::currentMachine()));

	ProjectLock lock(project);
	REQUIRE(lock.exists());
	CHECK(lock.isStale());
	CHECK(lock.acquire());
	CHECK(lock.holder().isThisProcess());
}

TEST_CASE("CU-38.4 — enregistrer par-dessus la modification d'un autre est refusé")
{
	QTemporaryDir share;
	REQUIRE(share.isValid());
	const QString project = share.filePath(QStringLiteral("quadro.qet"));
	writeFile(project, QStringLiteral("<project>uma folha</project>"));

	const FileDiskState opened = FileDiskState::of(project);
	CHECK_FALSE(opened.isNull());
	CHECK_FALSE(FileDiskState::changedSince(project, opened));

	// The colleague saves. Size differs, so it is seen even inside the same
	// second, which is the case a timestamp alone would miss.
	writeFile(project, QStringLiteral("<project>uma folha e mais outra</project>"));
	CHECK(FileDiskState::changedSince(project, opened));

	// After taking the new state as ours, saving is allowed again.
	const FileDiskState reread = FileDiskState::of(project);
	CHECK_FALSE(FileDiskState::changedSince(project, reread));

	// Nothing to compare against is not "changed": a project never saved, or
	// a file that disappeared, must not be blocked from being written.
	CHECK_FALSE(FileDiskState::changedSince(QString(), reread));
	CHECK_FALSE(FileDiskState::changedSince(project, FileDiskState()));
	REQUIRE(QFile::remove(project));
	CHECK_FALSE(FileDiskState::changedSince(project, reread));
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
	writeFile(QETEnvironment::companyElementsDir() + QStringLiteral("/acme.elmt"),
		  QStringLiteral("<definition/>"));
	writeFile(QETEnvironment::titleBlocksDir() + QStringLiteral("/acme.titleblock"),
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
