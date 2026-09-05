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
#ifndef PROJECTLOCK_H
#define PROJECTLOCK_H

#include <QString>

/**
	@brief The ProjectLock class
	Who has this project open, and since when.

	This is not concurrency control, and does not pretend to be. What it
	removes is the one failure the office already knows the cost of: two
	people editing the same project, the second one saving, and the first
	one's afternoon disappearing without a message.

	It is a plain text file next to the project, on purpose:

	    user=jdupont
	    machine=DESKTOP-XYZ
	    pid=12345
	    since=2026-08-20T11:42:10

	Readable and deletable by a human. When something goes wrong on a network
	share at six in the evening, a file somebody can open in Notepad and
	delete beats a clever mechanism nobody can inspect.
*/
class ProjectLock
{
	public:
		/// Who holds a lock
		class Holder
		{
			public:
				QString user;
				QString machine;
				QString since;      ///< ISO 8601
				qint64 pid = 0;

				bool isNull() const;
				bool isThisProcess() const;
				bool isThisMachine() const;
				/// Human sentence: who, on which machine, since when
				QString description() const;
		};

		explicit ProjectLock(const QString &project_path);

		QString projectPath() const;
		QString lockFilePath() const;

		bool exists() const;
		Holder holder() const;

		/**
			@return true when the lock can be taken over: it is ours, or it
			belongs to a process of this machine that is no longer running.
			A lock from another machine is never called stale - there is no
			way to know from here, and guessing wrong is what this class
			exists to prevent.
		*/
		bool isStale() const;

		bool acquire(QString *error = nullptr);
		bool release();
		/// Delete the lock whoever holds it. Only for an explicit user choice.
		bool forceRelease();

		static QString lockSuffix();
		static QString currentUser();
		static QString currentMachine();

		/**
			@return true when a process of *this* machine is still running.
			Public because the same question, about the same kind of plain
			text file, is asked by the session marker of CrashRecovery:
			better one copy of the platform code than two.
		*/
		static bool processIsAlive(qint64 pid);

	private:
		QString m_project_path;
		bool m_acquired = false;
};

#endif // PROJECTLOCK_H
