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
#ifndef CATALOGREPOSITORY_H
#define CATALOGREPOSITORY_H

#include "catalogpart.h"

#include <QList>
#include <QString>
#include <QStringList>

class Catalog;

/**
	@brief The CatalogRepositoryEntry class
	One part package as the repository listing sees it: enough to search and
	to show, not the whole part.
*/
class CatalogRepositoryEntry
{
	public:
		QString file_path;
		QString code;
		QString class_key;
		QString class_name;
		QString designation;
		QString manufacturer;
		QString image;

		bool isNull() const;
		/// true when @a text appears in any of the searchable fields
		bool matches(const QString &text) const;
};

/**
	@brief The CatalogRepository class
	A folder of part packages, shared by whoever can reach it.

	It starts as a folder of the shared environment - the internal repository
	of the office - and the path is a setting, so it can later be a network
	address without anything else changing.

	> **Decision recorded:** contributing is an explicit action, never
	> automatic. A part registered here carries purchasing and engineering
	> decisions of the company, and it leaves only when somebody sends it.

	The listing reads only the head of each file, with a streaming parser.
	Parsing every package in full to draw a list of a thousand rows is paid on
	every keystroke of the search box, and the head is all the list shows.
*/
class CatalogRepository
{
	public:
		static QString path();
		static bool setPath(const QString &path, QString *error = nullptr);
		static QString defaultFolderName();

		/// Every package in @a root, sub-folders included
		static QList<CatalogRepositoryEntry> entries(const QString &root);

		/**
			@param entries
			@param text : free text on code, designation, manufacturer, class
			@param class_key : empty for every class
			@param manufacturer : empty for every manufacturer
			@return the entries matching every given criterion. Combinable,
			and each one droppable on its own - refining a search you cannot
			undo is worse than no filter at all.
		*/
		static QList<CatalogRepositoryEntry> search(const QList<CatalogRepositoryEntry> &entries,
							    const QString &text,
							    const QString &class_key,
							    const QString &manufacturer);

		static QStringList manufacturersOf(const QList<CatalogRepositoryEntry> &entries);
		static QStringList classKeysOf(const QList<CatalogRepositoryEntry> &entries);

		/**
			Write @a part into @a root as a package. The one way anything
			leaves this office, and it takes a click.
		*/
		static bool contribute(const QString &root,
				       const Catalog &catalog,
				       const CatalogPart &part,
				       QString *error = nullptr);

		/// Read the head of one package
		static CatalogRepositoryEntry readEntry(const QString &file_path);
};

#endif // CATALOGREPOSITORY_H
