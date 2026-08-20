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
#ifndef FILEDISKSTATE_H
#define FILEDISKSTATE_H

#include <QDateTime>    // Qt5's QFileInfo does not pull it in, Qt6's does
#include <QFileInfo>
#include <QString>

/**
	@brief The FileDiskState class
	The size and the last-modified time of a file, as somebody last saw them.

	Comparing before writing is what turns "the last one who saves wins" into
	a refusal, which is the difference between sharing an environment and
	losing an afternoon to it.

	It lives on its own, header only, for one reason: this is the rule that
	decides whether a project gets overwritten, and it has to be testable
	without opening a project.

	Size **and** time, not a checksum: reading a whole project over a network
	share on every save would be paid on every save, and two different saves
	landing on the very same second *and* the very same byte count is not a
	case worth that price.
*/
class FileDiskState
{
	public:
		qint64 size = -1;
		qint64 modified = -1;

		bool isNull() const
		{
			return size < 0 && modified < 0;
		}

		bool operator==(const FileDiskState &other) const
		{
			return size == other.size && modified == other.modified;
		}

		bool operator!=(const FileDiskState &other) const
		{
			return !(*this == other);
		}

		/**
			@param file_path
			@return the state of @a file_path, a null state when it does not
			exist or when no path was given
		*/
		static FileDiskState of(const QString &file_path)
		{
			FileDiskState state;
			if (file_path.isEmpty()) {
				return state;
			}
			const QFileInfo info(file_path);
			if (!info.exists()) {
				return state;
			}
			state.size = info.size();
			state.modified = info.lastModified().toMSecsSinceEpoch();
			return state;
		}

		/**
			@param file_path
			@param previous : the state as it was last seen
			@return true when the file changed since then.

			False when there is nothing to compare - no path, nothing seen
			before, or a file that has since disappeared. Those are different
			problems with different answers, and answering "changed" to them
			would block a save that should go through.
		*/
		static bool changedSince(const QString &file_path, const FileDiskState &previous)
		{
			if (file_path.isEmpty() || previous.isNull()) {
				return false;
			}
			const FileDiskState current = of(file_path);
			if (current.isNull()) {
				return false;
			}
			return current != previous;
		}
};

#endif // FILEDISKSTATE_H
