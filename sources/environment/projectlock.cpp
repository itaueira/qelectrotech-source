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
#include "projectlock.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QLocale>
#include <QTextStream>

#if defined(Q_OS_WIN)
	// Keep windows.h from defining min/max and dragging in the world: both
	// break Qt headers that come after it.
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#else
#	include <cerrno>
#	include <csignal>
#endif

/**
	@brief ProjectLock::lockSuffix
	@return what is appended to the project file name
*/
QString ProjectLock::lockSuffix()
{
	return QStringLiteral(".lock");
}

/**
	@brief ProjectLock::currentUser
	@return the login name of whoever runs this process
*/
QString ProjectLock::currentUser()
{
	const QStringList candidates = { QStringLiteral("USERNAME"),
					 QStringLiteral("USER"),
					 QStringLiteral("LOGNAME") };
	for (const QString &candidate : candidates)
	{
		const QString value = qEnvironmentVariable(candidate.toLatin1().constData());
		if (!value.isEmpty()) {
			return value;
		}
	}
	return QStringLiteral("?");
}

/**
	@brief ProjectLock::currentMachine
	@return the name of this machine
*/
QString ProjectLock::currentMachine()
{
	const QString name = QHostInfo::localHostName();
	return name.isEmpty() ? QStringLiteral("?") : name;
}

/**
	@brief ProjectLock::Holder::isNull
	@return true when this holder carries nothing
*/
bool ProjectLock::Holder::isNull() const
{
	return user.isEmpty() && machine.isEmpty() && pid == 0;
}

/**
	@brief ProjectLock::Holder::isThisMachine
	@return true when the lock was taken on this machine
*/
bool ProjectLock::Holder::isThisMachine() const
{
	return !machine.isEmpty()
	       && machine.compare(ProjectLock::currentMachine(), Qt::CaseInsensitive) == 0;
}

/**
	@brief ProjectLock::Holder::isThisProcess
	@return true when this very process holds the lock
*/
bool ProjectLock::Holder::isThisProcess() const
{
	return isThisMachine() && pid == QCoreApplication::applicationPid();
}

/**
	@brief ProjectLock::Holder::description
	@return one sentence naming who has the file and since when
*/
QString ProjectLock::Holder::description() const
{
	if (isNull()) {
		return QString();
	}

	QString when = since;
	const QDateTime parsed = QDateTime::fromString(since, Qt::ISODate);
	if (parsed.isValid()) {
		when = QLocale::system().toString(parsed, QLocale::ShortFormat);
	}

	if (machine.isEmpty()) {
		return QCoreApplication::translate("ProjectLock",
						   "%1, depuis %2").arg(user, when);
	}
	return QCoreApplication::translate("ProjectLock",
					   "%1 sur %2, depuis %3").arg(user, machine, when);
}

/**
	@brief ProjectLock::ProjectLock
	@param project_path
*/
ProjectLock::ProjectLock(const QString &project_path) :
	m_project_path(project_path)
{}

/**
	@brief ProjectLock::projectPath
	@return the project this lock is about
*/
QString ProjectLock::projectPath() const
{
	return m_project_path;
}

/**
	@brief ProjectLock::lockFilePath
	@return the lock file, next to the project
*/
QString ProjectLock::lockFilePath() const
{
	if (m_project_path.isEmpty()) {
		return QString();
	}
	return m_project_path + lockSuffix();
}

/**
	@brief ProjectLock::exists
	@return true when somebody declares the project open
*/
bool ProjectLock::exists() const
{
	const QString path = lockFilePath();
	return !path.isEmpty() && QFileInfo::exists(path);
}

/**
	@brief ProjectLock::holder
	@return who holds the lock, a null Holder when there is none
*/
ProjectLock::Holder ProjectLock::holder() const
{
	Holder found;
	QFile file(lockFilePath());
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return found;
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
			found.user = value;
		} else if (key == QLatin1String("machine")) {
			found.machine = value;
		} else if (key == QLatin1String("since")) {
			found.since = value;
		} else if (key == QLatin1String("pid")) {
			found.pid = value.toLongLong();
		}
	}
	return found;
}

namespace
{
	/// Whether a process of this machine is still running.
	bool processIsAlive(qint64 pid)
	{
		if (pid <= 0) {
			return false;
		}
#if defined(Q_OS_WIN)
		HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
					    static_cast<DWORD>(pid));
		if (!handle) {
			return false;
		}
		DWORD exit_code = 0;
		const bool alive = GetExitCodeProcess(handle, &exit_code)
				   && exit_code == STILL_ACTIVE;
		CloseHandle(handle);
		return alive;
#else
		return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
	}
}

/**
	@brief ProjectLock::isStale
	@return true when the lock can be taken over
*/
bool ProjectLock::isStale() const
{
	const Holder found = holder();
	if (found.isNull()) {
		return true;    // an empty or unreadable lock protects nothing
	}
	if (found.isThisProcess()) {
		return true;
	}
	if (!found.isThisMachine()) {
		// Another machine: there is no way to tell from here whether that
		// session is alive, and guessing wrong is exactly the loss this
		// class exists to prevent. Let the user decide.
		return false;
	}
	return !processIsAlive(found.pid);
}

/**
	@brief ProjectLock::acquire
	@param error
	@return true when this process now holds the lock
*/
bool ProjectLock::acquire(QString *error)
{
	const QString path = lockFilePath();
	if (path.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("ProjectLock",
							     "Le projet n'a pas encore de fichier.");
		}
		return false;
	}

	if (exists() && !isStale())
	{
		if (error) {
			*error = holder().description();
		}
		return false;
	}

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
	{
		// A share where the project can be read but not written: drawing has
		// to work anyway, so a lock that cannot be taken is not an error the
		// user has to solve.
		if (error) {
			*error = QCoreApplication::translate("ProjectLock",
							     "Impossible d'écrire le fichier de verrou %1.")
				 .arg(path);
		}
		return false;
	}

	QTextStream stream(&file);
	stream << "user=" << currentUser() << '\n'
	       << "machine=" << currentMachine() << '\n'
	       << "pid=" << QCoreApplication::applicationPid() << '\n'
	       << "since=" << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
	file.close();

	m_acquired = true;
	return true;
}

/**
	@brief ProjectLock::release
	@return true when the lock is gone. Only releases our own lock: releasing
	somebody else's is what forceRelease() is for, and it takes a decision.
*/
bool ProjectLock::release()
{
	if (!exists()) {
		m_acquired = false;
		return true;
	}
	if (!m_acquired && !holder().isThisProcess()) {
		return false;
	}
	const bool removed = QFile::remove(lockFilePath());
	if (removed) {
		m_acquired = false;
	}
	return removed;
}

/**
	@brief ProjectLock::forceRelease
	@return true when the lock is gone
*/
bool ProjectLock::forceRelease()
{
	if (!exists()) {
		return true;
	}
	const bool removed = QFile::remove(lockFilePath());
	if (removed) {
		m_acquired = false;
	}
	return removed;
}
