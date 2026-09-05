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
#ifndef CRASHRECOVERY_H
#define CRASHRECOVERY_H

#include <QDateTime>
#include <QList>
#include <QString>

class KAutoSaveFile;
class QUrl;

/**
	@brief The CrashRecovery class
	When a recovery copy is offered, and which one.

	The backup timer of QETProject drops a recovery copy of every open
	project every twenty minutes. Deciding what to do with those copies
	used to be four words long -- offer all of them, at start-up -- and
	each of the four was wrong in a way that made the program unusable:
	a session that had accumulated thirty-four copies opened on a queue
	of questions instead of on a drawing, and every question listed
	projects the user was not opening, each of them several times, after
	a shutdown that had been perfectly normal.

	So the decision is made here, and it is four rules:

	1. Only the project being opened. Not every project the machine has
	   ever seen: a recovery copy is an answer to "open this file", and
	   nobody asked about the others.
	2. Only when a project is opened, never when the program starts.
	   That is why nothing in this class is called from QETApp's
	   constructor except beginSession(): the question belongs to
	   QETDiagramEditor::openAndAddProject().
	3. Only the newest copy of that project. The retention policy keeps
	   several (it must: the newest one can be the one that was being
	   written when the power went), but exactly one of them is worth
	   offering, and it is the last one.
	4. Only when the previous session ended abruptly. Without this the
	   first three only make the wrong question shorter -- somebody who
	   closed the program properly yesterday would still be asked to
	   recover today.

	Rule 4 is the only one that needs to remember something across runs,
	and the memory is a session marker: a small text file written when
	the program starts and removed when it stops. Finding one on the next
	run, whose owner is no longer running, is what "ended abruptly" means
	here.

	The marker is one file per process, named after the machine and the
	process id, and endSession() only ever removes the file this process
	wrote. That is deliberate and it is the whole answer to the obvious
	way of getting this wrong: with two copies of the program open at
	once, closing the second one must not erase the mark of the first,
	which is still running and may still crash. A shared marker would do
	exactly that.

	Judging somebody else's marker is the same problem ProjectLock
	already solved next door, and the same answer is used: a marker from
	another machine is never judged (there is no way to tell from here
	whether that session is alive), and a marker from this machine is
	dead when its process is gone. A marker carrying *our own* process
	id is dead too, and has to be: we write ours last, so any other
	marker holding our id belongs to a session the system has already
	buried and whose id it handed to us.

	The known blind spot is process id reuse in the other direction: a
	dead session whose id has since been taken by an unrelated process
	reads as alive, and its marker is left alone. That is a deferred
	offer and not a lost one -- the marker survives, and the next run
	that finds the id free asks the question. It is the safe side of the
	only mistake that costs the user anything, which is failing to offer
	a recovery to somebody who needs it.

	Not thread-safe, and does not need to be: beginSession()/endSession()
	bracket the whole run from the main thread, and the choice is made
	while a project is being opened, also from the main thread.
*/
class CrashRecovery
{
	public:
		/**
			One recovery copy sitting on the disk: which project it
			belongs to, where the copy itself is, and when it was
			last written.

			A plain value, so that the rules below can be read -- and
			tested -- without a disk, a lock file, or the two
			different layouts KAutoSaveFile has depending on whether
			the KDE implementation or the local one is compiled in.
		*/
		class Copy
		{
			public:
				QString project_file;   ///< the project this copy belongs to
				QString copy_file;      ///< the copy itself
				QDateTime written;      ///< when the copy was last written
		};

		// --- rule 4: how the previous session ended --------------------

		/// Reads what the previous runs left behind, then marks this run
		/// as running. Called once, at start-up, before anything can open
		/// a project.
		static void beginSession();

		/// Removes this run's marker, and only this run's. Called on a
		/// normal shutdown; a run that never reaches it is exactly what
		/// the next one will call an abrupt ending.
		static void endSession();

		/// The answer beginSession() latched. False until it has run.
		static bool previousSessionEndedAbruptly();

		/// Where the session markers live. QETApp::dataDir() + "/sessions"
		/// unless setSessionDirectory() said otherwise.
		static QString sessionDirectory();

		/// Points the markers somewhere else -- for a test, which must not
		/// write into the data directory of the installed program. Drops
		/// this run's marker first: it belongs to the directory it was
		/// written in.
		static void setSessionDirectory(const QString &path);

		// --- rules 1 and 3: which copy ---------------------------------

		/**
			The newest copy of project_path in copies, or -1 when there
			is none. Rules 1 and 3, and nothing else: it does not care
			how the previous session ended.

			Ties go to the earlier entry, so the answer does not move
			between runs.
		*/
		static int newestCopyOf(const QList<Copy> &copies,
					const QString &project_path);

		/**
			The whole question: which of copies should be offered when
			project_path is opened, or -1 when nothing should be.

			Rule 4 first -- a session that ended properly is offered
			nothing at all -- then rules 1 and 3. An empty project_path
			answers -1, which is rule 2 read from this side: with no
			project being opened there is nothing to offer.
		*/
		static int chooseCopy(const QList<Copy> &copies,
				      const QString &project_path);

		// --- the same, against the disk --------------------------------

		/// The recovery copies of project_path that are on the disk. Reads
		/// them, and leaves every one of them exactly where it is.
		static QList<Copy> copiesOnDisk(const QString &project_path);

		/**
			The recovery copy to offer when project_path is opened, or
			nullptr. The caller takes ownership; handing it to the
			QETProject(KAutoSaveFile *) constructor passes it on.

			Deleting the returned object without opening it leaves the
			copy on the disk -- see discardCopiesOf() for the other
			answer.
		*/
		static KAutoSaveFile *candidateFor(const QString &project_path);

		/// True when some project has a copy waiting to be offered. Only
		/// used to keep the crash-dump prompt out of the way at start-up.
		static bool hasPendingRecovery();

		/// Throws away every recovery copy of project_path. This is what
		/// "no, open the saved file" means: without it the same question
		/// would come back after the next abrupt ending.
		static void discardCopiesOf(const QString &project_path);

		/**
			The project path carried by a recovery copy, in the form the
			rest of the program can open.

			KAutoSaveFile stores it as a URL, and the two implementations
			do not agree on the shape: the KDE one keeps a real file URL,
			whose path on Windows is "/C:/dir/file.qet", while the local
			one stores a bare path, "C:/dir/file.qet". Handing the first
			shape to QFile opens nothing.
		*/
		static QString localPathOf(const QUrl &managed_file);

	private:
		CrashRecovery() = delete;
};

#endif // CRASHRECOVERY_H
