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
#ifndef CATALOGIMPORT_H
#define CATALOGIMPORT_H

#include "catalogtable.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class Catalog;

/**
	@brief What to do with a part the catalog already has.
	The choice is explicit because the three answers are all right, on
	different days: a price list wants Update, a corrected data sheet wants
	NewRevision, and a re-run of last month's file wants Ignore.
*/
enum class CatalogDuplicatePolicy
{
	Ignore,      ///< leave what the catalog has
	Update,      ///< overwrite the fields the spreadsheet brings
	NewRevision  ///< keep the old data as an older revision
};

/**
	@brief The CatalogImportProfile class
	Which column of a spreadsheet means what.

	Every supplier sends a different layout, so the mapping is saved and
	reused - that is the whole reason this is an object and not a handful of
	arguments. Profiles live in the catalog, which means in the shared
	environment: the layout of a supplier's list is not a per-station fact.
*/
class CatalogImportProfile
{
	public:
		QString name;
		/// Destination class, by stable key. Empty when it comes from a column.
		QString class_key;
		/// Column holding the class name, when the spreadsheet carries it
		QString class_column;
		/// Column holding the part code. Mandatory: a part without one is not a part.
		QString code_column;
		/// property key -> column header
		QHash<QString, QString> value_columns;
		CatalogDuplicatePolicy policy = CatalogDuplicatePolicy::Update;
		/// Delimiter to force, null to let the reader guess
		QChar delimiter;

		bool isValid(QString *error = nullptr) const;

		QString toXml() const;
		static CatalogImportProfile fromXml(const QString &xml);

		static QString policyToString(CatalogDuplicatePolicy policy);
		static CatalogDuplicatePolicy policyFromString(const QString &string);
		static QString translatedPolicyName(CatalogDuplicatePolicy policy);

		/**
			Guess a mapping from the headers of @a table, by comparing them
			with the names and the keys of the properties of @a class_id.
			Not magic and not meant to be: it fills the obvious ones so that
			the user corrects a few instead of typing them all.
		*/
		static CatalogImportProfile guess(const Catalog &catalog,
						  int class_id,
						  const CatalogTable &table);
};

/**
	@brief The CatalogImportReport class
	What an import did, line by line when it refused.

	A silent import corrupts a catalog: the row that was dropped is exactly
	the part somebody will look for next month. So every rejection carries
	its row number and a reason a person can act on.
*/
class CatalogImportReport
{
	public:
		class Rejection
		{
			public:
				int row = 0;          ///< 1-based, counting the header as row 1
				QString code;
				QString reason;
		};

		int created = 0;
		int updated = 0;
		int revised = 0;
		int ignored = 0;
		QList<Rejection> rejections;
		QStringList notes;

		int rejected() const;
		int total() const;
		bool changedAnything() const;
		/// A summary a person reads, and can paste into an e-mail
		QString toText() const;
};

/**
	@brief The CatalogImporter class
	Write a spreadsheet into the catalog, and read the catalog back out.
*/
class CatalogImporter
{
	public:
		/**
			@param catalog
			@param table
			@param profile
			@param origin : where the data came from, kept on every part it
			touches. When a value turns out wrong, this is what says whether
			the mistake is the source's or the typist's.
			@return what was done
		*/
		static CatalogImportReport import(Catalog &catalog,
						  const CatalogTable &table,
						  const CatalogImportProfile &profile,
						  const QString &origin);

		/**
			@param catalog
			@param class_id : 0 for every class
			@return the catalog as a table, one row per part, the columns
			being the properties of the class. For checking, and for
			purchasing, which lives in a spreadsheet whatever anybody wishes.
		*/
		static CatalogTable exportToTable(const Catalog &catalog, int class_id = 0);
};

#endif // CATALOGIMPORT_H
