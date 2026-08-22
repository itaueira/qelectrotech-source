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
#include "catalogproperty.h"

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

		/**
			@param catalog
			@param table
			@return the header of the column of @a table that carries a
			class name, empty when no column does.

			Content decides, not the header: a column is taken for the
			class column only when every one of its cells names a class
			the catalog has. So guessing can never turn a row into a
			rejection - the reason the test for it is worth having.
			(CU-14.14)
		*/
		static QString guessClassColumn(const Catalog &catalog,
						const CatalogTable &table);

		/**
			@param catalog
			@param class_id : the destination class
			@param table
			@param class_column : the header carrying the class, if any
			@return the properties a mapping may target, deduped by key,
			the destination class first and then each class the column
			names, in the order the file names them.

			The destination class alone is not the answer when the sheet
			carries its own class: a property declared on a subclass is
			not inherited upwards, so mapping only what the destination
			declares leaves the technical columns of every sibling class
			with nowhere to go. Measured on the real project: 12 of the 14
			typed columns. (CU-14.14)
		*/
		static QList<CatalogProperty> mappableProperties(const Catalog &catalog,
								 int class_id,
								 const CatalogTable &table,
								 const QString &class_column);

		/**
			@param table
			@return the headers of @a table this profile reads nothing from:
			neither the code nor the class, and mapped to no property.

			A column nobody reads is loss, and it is silent loss: the
			mapping table has one row per property of the destination class,
			so a column with no property has nowhere to appear. Naming them
			is the rule of a refused row applied to the column instead.
			(CU-14.12)
		*/
		QStringList unmappedColumns(const CatalogTable &table) const;
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

		/**
			@brief A part the sheet moved to another class.
			Counted apart from an update because it is not the same event:
			a moved part answers differently to every question asked of the
			class tree afterwards, and "11 updated" reads as if nothing had
			moved at all. (CU-14.13)
		*/
		class ClassMove
		{
			public:
				QString code;
				QString from;   ///< the class it was in, by name
				QString to;     ///< the class the sheet declared, by name
		};

		/**
			@brief A value the destination class does not declare.
			Values are stored without being filtered by class, but a part
			dialog only ever shows the class it was handed - so a value
			outside the class is a value nobody sees and nobody can
			correct. Both halves are named: the cell that was refused, and
			the value the part already carried. (CU-14.12, CU-14.13)
		*/
		class UndeclaredValue
		{
			public:
				int row = 0;          ///< the row that brought it about
				QString code;
				QString key;
				QString class_name;   ///< the class that does not declare it
				/// true for a cell of the sheet, refused; false for a value
				/// the part already carried, kept and now out of sight
				bool from_sheet = true;
		};

		int created = 0;
		int updated = 0;
		int revised = 0;
		int ignored = 0;
		QList<Rejection> rejections;
		QList<ClassMove> class_moves;
		QList<UndeclaredValue> undeclared_values;
		/// Headers of the file the profile read nothing from (CU-14.12)
		QStringList unmapped_columns;
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
