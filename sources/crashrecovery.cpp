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
#include "crashrecovery.h"

#include "environment/projectlock.h"
#include "logging/qetlogger.h"
#include "qetapp.h"

#ifdef BUILD_WITHOUT_KF
#	include "ui/nokde/kautosavefile.h"
#else
#	include <KAutoSaveFile>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>

namespace
{
	/// Latched by beginSession(), read by everything else.
	bool s_previous_ended_abruptly = false;
	/// The marker this run wrote, empty while no session is open.
	QString s_marker_path;
	/// Empty unless a test pointed the markers elsewhere.
	QString s_session_directory;

	const auto marker_prefix = QStringLiteral("session-");
	const auto marker_suffix = QStringLiteral(".marker");

	/**
		What a session marker says. The same four fields, in the same
		plain "key=value" shape, as the lock ProjectLock writes next to a
		project -- and for the same reason: when something goes wrong at
		six in the evening, a file somebody can open in Notepad and delete
		beats a clever mechanism nobody can inspect.
	*/
	class Marker
	{
		public:
			QString user;
			QString machine;
			QString since;    ///< ISO 8601
			qint64 pid = 0;

			bool isReadable() const {return !machine.isEmpty() && pid > 0;}
	};

	/// The machine name, with anything that has no business in a file name
	/// replaced. A marker of one machine must not overwrite the marker of
	/// another when the data directory happens to be a roaming profile.
	QString fileNameSafe(const QString &text)
	{
		QString safe;
		safe.reserve(text.size());
		for (const QChar &character : text)
		{
			const bool plain = character.isLetterOrNumber()
					   || character == QLatin1Char('-')
					   || character == QLatin1Char('_');
			safe += plain ? character : QLatin1Char('_');
		}
		return safe.isEmpty() ? QStringLiteral("machine") : safe;
	}

	QString markerFileName(const QString &machine, qint64 pid)
	{
		return marker_prefix + fileNameSafe(machine) + QStringLiteral("-")
		       + QString::number(pid) + marker_suffix;
	}

	Marker readMarker(const QString &path)
	{
		Marker marker;
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			return marker;
		}

		QTextStream stream(&file);
		while (!stream.atEnd())
		{
			const QString line = stream.readLine();
			const int separator = line.indexOf(QLatin1Char('='));
			if (separator <= 0) {
				continue;
			}
			const QString key = line.left(separator).trimmed();
			const QString value = line.mid(separator + 1).trimmed();

			if (key == QLatin1String("user")) {
				marker.user = value;
			} else if (key == QLatin1String("machine")) {
				marker.machine = value;
			} else if (key == QLatin1String("since")) {
				marker.since = value;
			} else if (key == QLatin1String("pid")) {
				marker.pid = value.toLongLong();
			}
		}
		return marker;
	}

	bool writeMarker(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly
			       | QIODevice::Truncate
			       | QIODevice::Text)) {
			return false;
		}

		QTextStream stream(&file);
		stream << "user=" << ProjectLock::currentUser() << '\n'
		       << "machine=" << ProjectLock::currentMachine() << '\n'
		       << "pid=" << QCoreApplication::applicationPid() << '\n'
		       << "since=" << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
		file.close();
		return file.error() == QFile::NoError;
	}

	/// The same path written two different ways still names one project.
	QString comparablePath(const QString &path)
	{
		if (path.isEmpty()) {
			return QString();
		}
		return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
	}

	bool samePath(const QString &left, const QString &right)
	{
		const QString a = comparablePath(left);
		const QString b = comparablePath(right);
		if (a.isEmpty() || b.isEmpty()) {
			return false;
		}
#ifdef Q_OS_WIN
		return a.compare(b, Qt::CaseInsensitive) == 0;
#else
		return a == b;
#endif
	}

	/**
		Whether the session that wrote this marker is over, and ended
		without saying so.

		Three answers, and the middle one is the one that keeps a second
		copy of the program from being declared dead by the first.
	*/
	bool markerIsOfADeadSession(const Marker &marker)
	{
		if (!marker.isReadable()) {
			// Half a marker is a process that died while writing it.
			// Reading that as an abrupt ending is the safe side: the
			// only mistake that costs anything here is not offering.
			return true;
		}
		if (marker.machine.compare(ProjectLock::currentMachine(),
					   Qt::CaseInsensitive) != 0) {
			// Another machine, on a shared or roaming data directory.
			// There is no way to tell from here whether that session
			// is alive, so it is not judged -- the same answer
			// ProjectLock gives to the same question.
			return false;
		}
		if (marker.pid == QCoreApplication::applicationPid()) {
			// Our own id in somebody else's marker: we write ours
			// last, so that session is over and the system has
			// already handed its id to us.
			return true;
		}
		return !ProjectLock::processIsAlive(marker.pid);
	}
}

/**
	@brief CrashRecovery::sessionDirectory
	@return where the session markers live
*/
QString CrashRecovery::sessionDirectory()
{
	if (!s_session_directory.isEmpty()) {
		return s_session_directory;
	}
	return QETApp::dataDir() + QStringLiteral("/sessions");
}

/**
	@brief CrashRecovery::setSessionDirectory
	@param path
*/
void CrashRecovery::setSessionDirectory(const QString &path)
{
		//Our marker belongs to the directory it was written in.
	endSession();
	s_session_directory = path;
	s_previous_ended_abruptly = false;
}

/**
	@brief CrashRecovery::beginSession
	Reads what the previous runs left behind, then declares this one open.
*/
void CrashRecovery::beginSession()
{
	const QString directory = sessionDirectory();
	if (directory.isEmpty() || !QDir().mkpath(directory)) {
		return;
	}

		//Looked at before our own marker exists, so this run can never
		//find itself and call itself dead.
	bool abrupt = false;
	const QDir dir(directory);
	const QStringList entries = dir.entryList(
		{marker_prefix + QStringLiteral("*") + marker_suffix},
		QDir::Files);
	for (const QString &entry : entries)
	{
		const QString path = dir.absoluteFilePath(entry);
		if (!markerIsOfADeadSession(readMarker(path))) {
			continue;
		}
		abrupt = true;
			//Answered once. Leaving it would make every later run
			//repeat the same offer for the same dead session.
		QFile::remove(path);
	}

		//A crash the handler did catch is the same answer reached from the
		//other side: that file only exists because the process died on a
		//signal. Read before QETApp::checkCrashDump() clears it.
	if (QetLogger::instance().hasPendingCrashDump()) {
		abrupt = true;
	}

	s_previous_ended_abruptly = abrupt;

	const QString marker_path = dir.absoluteFilePath(
		markerFileName(ProjectLock::currentMachine(),
			       QCoreApplication::applicationPid()));
	if (writeMarker(marker_path)) {
		s_marker_path = marker_path;
	}
}

/**
	@brief CrashRecovery::endSession
	Removes this run's marker, and only this run's.
*/
void CrashRecovery::endSession()
{
	if (s_marker_path.isEmpty()) {
		return;
	}
	QFile::remove(s_marker_path);
	s_marker_path.clear();
}

/**
	@brief CrashRecovery::previousSessionEndedAbruptly
	@return what beginSession() found
*/
bool CrashRecovery::previousSessionEndedAbruptly()
{
	return s_previous_ended_abruptly;
}

/**
	@brief CrashRecovery::newestCopyOf
	@param copies
	@param project_path
	@return the index of the newest copy of project_path, -1 when there is none
*/
int CrashRecovery::newestCopyOf(const QList<Copy> &copies,
				const QString &project_path)
{
	if (project_path.isEmpty()) {
		return -1;
	}

	int newest = -1;
	for (int i = 0 ; i < copies.size() ; ++i)
	{
		const Copy &copy = copies.at(i);
		if (!samePath(copy.project_file, project_path)) {
			continue;
		}
		if (newest < 0) {
			newest = i;
			continue;
		}
			//Strictly newer, so that a tie keeps the earlier entry
			//and the answer does not move between runs.
		if (copy.written > copies.at(newest).written) {
			newest = i;
		}
	}
	return newest;
}

/**
	@brief CrashRecovery::chooseCopy
	@param copies
	@param project_path
	@return the index of the copy to offer, -1 when nothing should be offered
*/
int CrashRecovery::chooseCopy(const QList<Copy> &copies,
			      const QString &project_path)
{
	if (!previousSessionEndedAbruptly()) {
		return -1;
	}
	return newestCopyOf(copies, project_path);
}

/**
	@brief CrashRecovery::localPathOf
	@param managed_file
	@return the project path, in the form the rest of the program can open
*/
QString CrashRecovery::localPathOf(const QUrl &managed_file)
{
	QString path = managed_file.isLocalFile()
		       ? managed_file.toLocalFile()
		       : managed_file.path();
#ifdef Q_OS_WIN
		//A file URL keeps the drive letter behind a slash: "/C:/dir/x.qet".
		//QFile opens nothing with that.
	if (path.size() > 2
	    && path.at(0) == QLatin1Char('/')
	    && path.at(2) == QLatin1Char(':')) {
		path.remove(0, 1);
	}
#endif
	return path;
}

/**
	@brief CrashRecovery::copiesOnDisk
	@param project_path
	@return the recovery copies of project_path, left where they are
*/
QList<CrashRecovery::Copy> CrashRecovery::copiesOnDisk(const QString &project_path)
{
	QList<Copy> copies;
	if (project_path.isEmpty()) {
		return copies;
	}

	const QList<KAutoSaveFile *> stale_files =
		KAutoSaveFile::staleFiles(QUrl::fromLocalFile(project_path));
	for (KAutoSaveFile *stale : stale_files)
	{
		Copy copy;
		copy.project_file = localPathOf(stale->managedFile());
		copy.copy_file = stale->fileName();
		copy.written = QFileInfo(copy.copy_file).lastModified();
		copies << copy;

			//Never opened, so the lock was never taken and destroying
			//it leaves the copy on the disk. That asymmetry is how
			//KAutoSaveFile has always been told to keep or to throw
			//away: see discardCopiesOf().
		delete stale;
	}
	return copies;
}

/**
	@brief CrashRecovery::candidateFor
	@param project_path
	@return the recovery copy to offer, nullptr when there is nothing to offer
*/
KAutoSaveFile *CrashRecovery::candidateFor(const QString &project_path)
{
	if (project_path.isEmpty() || !previousSessionEndedAbruptly()) {
		return nullptr;
	}

	const QList<KAutoSaveFile *> stale_files =
		KAutoSaveFile::staleFiles(QUrl::fromLocalFile(project_path));

	QList<Copy> copies;
	copies.reserve(stale_files.size());
	for (KAutoSaveFile *stale : stale_files)
	{
		Copy copy;
		copy.project_file = localPathOf(stale->managedFile());
		copy.copy_file = stale->fileName();
		copy.written = QFileInfo(copy.copy_file).lastModified();
		copies << copy;
	}

	const int chosen = chooseCopy(copies, project_path);

	KAutoSaveFile *answer = nullptr;
	for (int i = 0 ; i < stale_files.size() ; ++i)
	{
		if (i == chosen) {
			answer = stale_files.at(i);
		} else {
			delete stale_files.at(i);
		}
	}
	return answer;
}

/**
	@brief CrashRecovery::hasPendingRecovery
	@return true when some project has a copy waiting to be offered
*/
bool CrashRecovery::hasPendingRecovery()
{
	if (!previousSessionEndedAbruptly()) {
		return false;
	}

	const QList<KAutoSaveFile *> stale_files = KAutoSaveFile::allStaleFiles();
	const bool waiting = !stale_files.isEmpty();
	qDeleteAll(stale_files);
	return waiting;
}

/**
	@brief CrashRecovery::discardCopiesOf
	@param project_path
*/
void CrashRecovery::discardCopiesOf(const QString &project_path)
{
	if (project_path.isEmpty()) {
		return;
	}

	const QList<KAutoSaveFile *> stale_files =
		KAutoSaveFile::staleFiles(QUrl::fromLocalFile(project_path));
	for (KAutoSaveFile *stale : stale_files)
	{
			//Opening takes the lock, and destroying an object that
			//holds the lock removes the file. This is how the
			//recovery copies have always been thrown away.
		stale->open(QIODevice::ReadWrite);
		delete stale;
	}
}
