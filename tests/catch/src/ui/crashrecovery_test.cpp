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
#include "../../../../sources/crashrecovery.h"
#include "../../../../sources/environment/projectlock.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>

/*
	When a recovery copy is offered, and which one.

	Five cases, and they are the specification itself. Four of them say
	"do not offer"; the one that says "offer" is the one that must never
	be allowed to break, because the only mistake here that costs the
	user anything is failing to offer a recovery to somebody who needs
	it. The other four cost a click.

	  1. a normal shutdown, with a copy on the disk: no offer;
	  2. an abrupt shutdown, with a copy of the project being opened: offer;
	  3. copies of two different projects, one of them being opened:
	     only the copy of that one;
	  4. five copies of the same project: one, the newest;
	  5. the program started without opening a project: nothing at all.

	What is exercised is the function that decides, and not the dialog.
	Under the offscreen platform a modal exec() does not fail, it waits
	forever for a click nobody is there to give (see uibench.h) - and
	beyond that, a test that had to open a message box would be testing
	QMessageBox rather than the four rules.

	The session markers are written into a temporary directory of the
	case, so nothing here touches the data directory of the installed
	program.
*/

namespace
{
	/**
		Points CrashRecovery at a directory of its own for the duration
		of a case, and gives it back afterwards however the case ends.
	*/
	class SessionDirectory
	{
		public:
			SessionDirectory()
			{
				CrashRecovery::setSessionDirectory(m_dir.path());
			}

			~SessionDirectory()
			{
					//Drops this run's marker with it: the next case
					//starts from nothing.
				CrashRecovery::setSessionDirectory(QString());
			}

			SessionDirectory(const SessionDirectory &) = delete;
			SessionDirectory &operator=(const SessionDirectory &) = delete;

			bool isValid() const {return m_dir.isValid();}
			QString path() const {return m_dir.path();}

			QStringList markers() const
			{
				return QDir(m_dir.path()).entryList(
					{QStringLiteral("session-*.marker")}, QDir::Files);
			}

		private:
			QTemporaryDir m_dir;
	};

	/**
		A session marker as a running instance would have left it: the
		same four fields the program writes.

		@param pid the process it claims to belong to. Whether that
		process is alive is the whole question the scan answers.
	*/
	bool writeMarker(const QString &directory, qint64 pid,
			 const QString &machine = ProjectLock::currentMachine())
	{
		QString safe_machine;
		for (const QChar &character : machine)
		{
			safe_machine += character.isLetterOrNumber() ? character
								    : QLatin1Char('_');
		}

		const QString path = directory + QStringLiteral("/session-")
				     + safe_machine + QStringLiteral("-")
				     + QString::number(pid)
				     + QStringLiteral(".marker");
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
			return false;
		}

		QTextStream stream(&file);
		stream << "user=someone\n"
		       << "machine=" << machine << '\n'
		       << "pid=" << pid << '\n'
		       << "since=" << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
		return true;
	}

	/**
		The process id of a process that really did run and really is
		gone.

		Not a made-up number: an id nobody ever used answers the
		question "is this process running" the same way a dead one does,
		but it does not prove the mechanism was ever pointed at a real
		process. So one is started, waited for, and its id taken once it
		has exited.
	*/
	qint64 pidOfADeadProcess()
	{
		QProcess process;
#ifdef Q_OS_WIN
		process.start(QStringLiteral("cmd.exe"),
			      {QStringLiteral("/c"), QStringLiteral("exit")});
#else
		process.start(QStringLiteral("/bin/sh"),
			      {QStringLiteral("-c"), QStringLiteral("exit")});
#endif
		if (!process.waitForStarted(10000)) {
			return -1;
		}
		const qint64 pid = process.processId();
		if (!process.waitForFinished(10000)) {
			return -1;
		}
		return pid;
	}

	/// A process that is still running while the case looks at its marker.
	class LiveProcess
	{
		public:
			LiveProcess()
			{
#ifdef Q_OS_WIN
					//ping needs no console and no input, which
					//"timeout" and "pause" both do.
				m_process.start(QStringLiteral("ping.exe"),
						{QStringLiteral("-n"), QStringLiteral("60"),
						 QStringLiteral("127.0.0.1")});
#else
				m_process.start(QStringLiteral("/bin/sh"),
						{QStringLiteral("-c"), QStringLiteral("sleep 60")});
#endif
				m_started = m_process.waitForStarted(10000);
			}

			~LiveProcess()
			{
				if (m_process.state() != QProcess::NotRunning) {
					m_process.kill();
					m_process.waitForFinished(10000);
				}
			}

			LiveProcess(const LiveProcess &) = delete;
			LiveProcess &operator=(const LiveProcess &) = delete;

			bool isRunning() const
			{
				return m_started && m_process.state() == QProcess::Running;
			}
			qint64 pid() const {return m_process.processId();}

		private:
			QProcess m_process;
			bool m_started = false;
	};

	/// A recovery copy of `project`, written `minutes_ago` minutes ago.
	CrashRecovery::Copy copyOf(const QString &project, int minutes_ago)
	{
		CrashRecovery::Copy copy;
		copy.project_file = project;
		copy.copy_file = project + QStringLiteral(".autosave")
				 + QString::number(minutes_ago);
		copy.written = QDateTime::currentDateTime().addSecs(-60 * minutes_ago);
		return copy;
	}
}

TEST_CASE("T40 — a recuperação só é oferecida depois de um fechamento abrupto",
	  "[crashrecovery][T40]")
{
	SessionDirectory session;
	REQUIRE(session.isValid());

	const QString project = QDir::tempPath() + QStringLiteral("/quadro.qet");

	SECTION("1. fechamento normal, com cópia no disco: não oferece")
	{
			//A whole run: it starts, and it says so on the way out.
		CrashRecovery::beginSession();
		REQUIRE(session.markers().size() == 1);
		CrashRecovery::endSession();
		CHECK(session.markers().isEmpty());

			//The next run finds nothing left behind.
		CrashRecovery::beginSession();
		CHECK_FALSE(CrashRecovery::previousSessionEndedAbruptly());

		const QList<CrashRecovery::Copy> copies = {copyOf(project, 5)};
			//The copy is there, and it is a copy of this very project:
			//what refuses it is rule 4 alone.
		CHECK(CrashRecovery::newestCopyOf(copies, project) == 0);
		CHECK(CrashRecovery::chooseCopy(copies, project) == -1);
	}

	SECTION("2. fechamento abrupto, com cópia do projeto que se abre: oferece")
	{
		const qint64 dead_pid = pidOfADeadProcess();
		INFO("pid do processo morto: " << dead_pid);
		REQUIRE(dead_pid > 0);
		REQUIRE(writeMarker(session.path(), dead_pid));

		CrashRecovery::beginSession();
		REQUIRE(CrashRecovery::previousSessionEndedAbruptly());

		const QList<CrashRecovery::Copy> copies = {copyOf(project, 5)};
		CHECK(CrashRecovery::chooseCopy(copies, project) == 0);

			//Answered once: the marker of that dead session is gone, so
			//the run after this one is not asked the same question again.
		CrashRecovery::endSession();
		CHECK(session.markers().isEmpty());
		CrashRecovery::beginSession();
		CHECK_FALSE(CrashRecovery::previousSessionEndedAbruptly());
	}

	SECTION("3. cópias de dois projetos, abrindo um: só a do projeto aberto")
	{
		const qint64 dead_pid = pidOfADeadProcess();
		REQUIRE(dead_pid > 0);
		REQUIRE(writeMarker(session.path(), dead_pid));
		CrashRecovery::beginSession();
		REQUIRE(CrashRecovery::previousSessionEndedAbruptly());

		const QString other = QDir::tempPath() + QStringLiteral("/outro.qet");
		const QList<CrashRecovery::Copy> copies = {
			copyOf(other, 1),        //the newest of the two, and irrelevant
			copyOf(project, 30)};

		const int chosen = CrashRecovery::chooseCopy(copies, project);
		REQUIRE(chosen == 1);
		CHECK(copies.at(chosen).project_file == project);

			//And the other way round, so that the answer is not just
			//"the second one".
		CHECK(CrashRecovery::chooseCopy(copies, other) == 0);
	}

	SECTION("4. cinco cópias do mesmo projeto: uma, a mais recente")
	{
		const qint64 dead_pid = pidOfADeadProcess();
		REQUIRE(dead_pid > 0);
		REQUIRE(writeMarker(session.path(), dead_pid));
		CrashRecovery::beginSession();
		REQUIRE(CrashRecovery::previousSessionEndedAbruptly());

			//Five, which is the number the retention policy keeps, and
			//deliberately not in order: the answer is the newest one,
			//not the last one written into the list.
		const QList<CrashRecovery::Copy> copies = {
			copyOf(project, 100),
			copyOf(project, 20),
			copyOf(project, 7),       //the newest
			copyOf(project, 60),
			copyOf(project, 40)};
		REQUIRE(copies.size() == 5);

		const int chosen = CrashRecovery::chooseCopy(copies, project);
		REQUIRE(chosen == 2);

			//One answer, and it is the newest: no other entry of the
			//five is younger than the one that was picked.
		for (int i = 0 ; i < copies.size() ; ++i)
		{
			if (i == chosen) {
				continue;
			}
			CHECK(copies.at(i).written < copies.at(chosen).written);
		}
	}

	SECTION("5. abrir o programa sem abrir projeto: nada é oferecido")
	{
		const qint64 dead_pid = pidOfADeadProcess();
		REQUIRE(dead_pid > 0);
		REQUIRE(writeMarker(session.path(), dead_pid));
		CrashRecovery::beginSession();
		REQUIRE(CrashRecovery::previousSessionEndedAbruptly());

			//Everything else that could produce an offer is in place:
			//the session ended abruptly, and there are copies waiting.
		const QList<CrashRecovery::Copy> copies = {
			copyOf(project, 5),
			copyOf(QDir::tempPath() + QStringLiteral("/outro.qet"), 5)};

		CHECK(CrashRecovery::chooseCopy(copies, QString()) == -1);
		CHECK(CrashRecovery::newestCopyOf(copies, QString()) == -1);
			//The same answer against the disk, which is the path the
			//program takes: with no project there is nothing to read
			//and nothing to hand back.
		CHECK(CrashRecovery::candidateFor(QString()) == nullptr);
		CHECK(CrashRecovery::copiesOnDisk(QString()).isEmpty());
	}
}

TEST_CASE("T40 — a marca de sessão sobrevive a duas instâncias abertas",
	  "[crashrecovery][T40]")
{
	SessionDirectory session;
	REQUIRE(session.isValid());

	SECTION("uma segunda instância viva não é dada por morta")
	{
		LiveProcess other;
		if (!other.isRunning()) {
				//Nothing was proved, and saying so beats a green tick.
			WARN("não foi possível iniciar um processo auxiliar; "
			     "a guarda de instância viva não foi exercitada");
		}
		else
		{
			REQUIRE(writeMarker(session.path(), other.pid()));

			CrashRecovery::beginSession();
			CHECK_FALSE(CrashRecovery::previousSessionEndedAbruptly());
				//Left where it was: the instance that wrote it is
				//still running and may still crash.
			CHECK(session.markers().size() == 2);
		}
	}

	SECTION("fechar uma instância apaga a marca dela, e só a dela")
	{
		const qint64 dead_pid = pidOfADeadProcess();
		REQUIRE(dead_pid > 0);

		LiveProcess other;
		if (!other.isRunning()) {
			WARN("não foi possível iniciar um processo auxiliar");
		}
		else
		{
			REQUIRE(writeMarker(session.path(), other.pid()));

			CrashRecovery::beginSession();
			REQUIRE(session.markers().size() == 2);

			CrashRecovery::endSession();
				//The live instance's marker is still there. Erasing
				//it here is the way to get this wrong: it would tell
				//the next run that a session which is still open
				//ended properly.
			REQUIRE(session.markers().size() == 1);
			CHECK(session.markers().first().endsWith(
				      QString::number(other.pid())
				      + QStringLiteral(".marker")));
		}
	}

	SECTION("uma marca com o nosso próprio pid é de uma sessão morta")
	{
			//The system hands out an id only after its owner is gone,
			//and this run writes its own marker last: any other marker
			//carrying this id belongs to a session that is over.
		REQUIRE(writeMarker(session.path(), QCoreApplication::applicationPid()));

		CrashRecovery::beginSession();
		CHECK(CrashRecovery::previousSessionEndedAbruptly());
	}

	SECTION("uma marca de outra máquina não é julgada")
	{
		REQUIRE(writeMarker(session.path(), 4242,
				    QStringLiteral("uma-outra-maquina")));

		CrashRecovery::beginSession();
		CHECK_FALSE(CrashRecovery::previousSessionEndedAbruptly());
		CHECK(session.markers().size() == 2);
	}

	SECTION("uma marca ilegível conta como fechamento abrupto")
	{
			//Half a marker is a process that died while writing it, and
			//the safe side of that doubt is to offer.
		const QString path = session.path()
				     + QStringLiteral("/session-truncada-1.marker");
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
		file.write("user=some");
		file.close();

		CrashRecovery::beginSession();
		CHECK(CrashRecovery::previousSessionEndedAbruptly());
	}
}

TEST_CASE("T40 — o caminho do projeto guardado na cópia é o que o programa abre",
	  "[crashrecovery][T40]")
{
	/*
		The trap this closes is a real difference between the two
		KAutoSaveFile implementations compiled into this program. The KDE
		one keeps a file URL, whose path on Windows carries the drive
		letter behind a slash; the local one stores a bare path. Both
		reach the same code, and one of the two shapes opens nothing.
	*/
	const QString bare = QStringLiteral("/home/someone/quadro.qet");
	QUrl bare_url;
	bare_url.setPath(bare);
	CHECK(CrashRecovery::localPathOf(bare_url) == bare);

	const QString file_path =
#ifdef Q_OS_WIN
		QStringLiteral("C:/Users/someone/quadro.qet");
#else
		QStringLiteral("/home/someone/quadro.qet");
#endif
	CHECK(CrashRecovery::localPathOf(QUrl::fromLocalFile(file_path)) == file_path);

#ifdef Q_OS_WIN
		//The shape that used to be written into the project as its own
		//file path.
	QUrl url_path;
	url_path.setPath(QStringLiteral("/C:/Users/someone/quadro.qet"));
	CHECK(CrashRecovery::localPathOf(url_path) == file_path);
#endif
}
