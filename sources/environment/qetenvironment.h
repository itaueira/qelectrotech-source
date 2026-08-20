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
#ifndef QETENVIRONMENT_H
#define QETENVIRONMENT_H

#include <QString>
#include <QStringList>

/**
	@brief The QETEnvironment class
	One folder holding everything: projects, the symbol library, the company
	library, the catalog, the title blocks and the macros.

	One path, everything inside. The alternative - one configurable path per
	kind of data - looks more flexible and is worse: whoever wants to share
	has to get six paths right, and will get one wrong. Sharing has to be
	pointing at a folder.

	The default is where the data already lives today, so an installation that
	never heard of an environment keeps working and nothing moves behind the
	user's back. Moving to a network share is copying that folder there and
	pointing the program at it.

	This class deliberately knows nothing about QETApp: it is the piece the
	test suite can exercise. QETApp calls setDefaultPath() once at startup so
	that the fallback and QETApp::dataDir() cannot drift apart.
*/
class QETEnvironment
{
	public:
		/// The environment in use: the configured one, or the default
		static QString path();
		/// true when the user chose a path instead of taking the default
		static bool isConfigured();
		static QString defaultPath();
		static void setDefaultPath(const QString &path);

		/**
			@param path
			@param error : when not nullptr, receives the reason on failure
			@return true when @a path is usable and was stored. Creates the
			folder skeleton when it is missing, so pointing at an empty
			network folder is enough to start.
		*/
		static bool setPath(const QString &path, QString *error = nullptr);

		static QString projectsDir();
		static QString elementsDir();
		static QString companyElementsDir();
		static QString titleBlocksDir();
		static QString companyTitleBlocksDir();
		static QString macrosDir();
		/**
			@brief Where the graphic groupings are filed (T35).
			Apart from the macros on purpose: a macro has variables and asks
			questions when inserted, a grouping is a finished piece of
			schematic that comes in as it was drawn. Mixing them in one
			folder would mean the projectist has to remember which is which.
		*/
		static QString groupingsDir();
		static QString catalogFile();

		/// The relative names of the folders an environment holds
		static QStringList skeletonFolders();
		static bool createSkeleton(const QString &root, QString *error = nullptr);
		/// true when @a root looks like an environment (has the skeleton)
		static bool looksLikeEnvironment(const QString &root);

		/**
			Copy the whole environment into @a destination, which must be
			**outside** it - a copy inside the folder it protects is not a
			copy, and this refuses to make one. Naming the destination is the
			caller's business; the dialog puts the date in it.
			@return how many files were copied, -1 on failure
		*/
		static int copyTo(const QString &destination, QString *error = nullptr);

	private:
		static QString subDir(const QString &name);
		static QString m_default_path;
};

#endif // QETENVIRONMENT_H
