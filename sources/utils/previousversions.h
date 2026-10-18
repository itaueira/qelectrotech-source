/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PREVIOUSVERSIONS_H
#define PREVIOUSVERSIONS_H

#include <QString>
#include <QStringList>

/**
	@brief The PreviousVersions namespace
	The versions of a project file kept before it is saved over: a copy of
	the file as it was, named by the time that version was saved
	("industrial 2026-10-16 08-30-12.qetz"), and only the newest are kept.
	The file is copied, never moved: a save that fails leaves it in place.

	The copies go in a folder of the user's, one sub-folder per project, and
	not beside the project: a project folder is what people share, send and
	look into, and a copy left in it at every save is clutter there. The
	sub-folder is named by the project and a short code of the folder it is
	in, so that two projects of the same name do not share their versions -
	the oldest of one would be dropped to keep the other's.
*/
namespace PreviousVersions
{
		/// How many versions to keep (Settings > General > Projects); 0 keeps none
	int count();
	void setCount(int count);
		/// The folder holding the versions of every project: by default
		/// "QElectroTech/Previous versions" in the user's documents, and in
		/// the data of the application in the test mode of QStandardPaths
	QString root();
		/// Put the versions under @p root instead, for this run only; an
		/// empty one goes back to the default. For the tests, which must not
		/// write into the documents of whoever runs them.
	void setRoot(const QString &root);
		/// The folder the versions of @p file_path go in, under root()
	QString folder(const QString &file_path);
		/// The versions kept of @p file_path, newest first
	QStringList kept(const QString &file_path);
		/// Copy @p file_path, about to be saved over, among its versions and
		/// keep only the @p count newest. Nothing is copied when the file does
		/// not exist, when @p count is 0, when the newest version kept holds
		/// the same bytes, or when the file is in the temporary folder of the
		/// system and setRoot() did not choose where the versions go - a copy
		/// something else owns, a test or an opened attachment, whose
		/// versions would pile up in the user's documents. @return false,
		/// with @p error, when the copy could not be made: the save goes on
		/// all the same.
	bool keep(const QString &file_path, int count, QString *error = nullptr);
}

#endif // PREVIOUSVERSIONS_H
