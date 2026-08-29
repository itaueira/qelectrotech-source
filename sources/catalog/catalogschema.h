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
#ifndef CATALOGSCHEMA_H
#define CATALOGSCHEMA_H

#include <QString>

class QSqlDatabase;

/**
	@brief The CatalogSchema class
	Creates and migrates the schema of the shared catalog database.

	The catalog is shared by the whole engineering office and lives on a
	network folder, so two builds of QElectroTech may open the same file. The
	rules that follow from that:

	- the schema carries its version in the catalog_meta table, and the
	  version only ever goes up, one migration step at a time;
	- a database older than the running build is migrated on open, without
	  asking and without a hand written script;
	- a database *newer* than the running build is refused, with a message
	  naming both versions. Silently ignoring a column this build does not
	  know is how a shared catalog gets corrupted by the machine that was
	  not updated;
	- adding a property to a class is never a migration. Properties are rows
	  in catalog_property and their values are rows in catalog_part_value,
	  which is the whole point of the model: the day the office wants a
	  field "internal stock code", nobody needs a programmer.
*/
class CatalogSchema
{
	public:
		/**
			@return the schema version this build writes and understands
		*/
		static int currentVersion();

		/**
			@param db
			@return the version of @a db, 0 for a database that has no
			catalog schema at all yet
		*/
		static int versionOf(QSqlDatabase &db);

		/**
			@param db
			@param target_version : version to stop at, currentVersion()
			for a normal open. A test may ask for an older one to build a
			database that then has to be migrated.
			@param error : when not nullptr, receives the reason on failure
			@return true when @a db is at @a target_version afterwards
		*/
		static bool applyUpTo(QSqlDatabase &db,
				      int target_version,
				      QString *error = nullptr);

		static QString meta(QSqlDatabase &db, const QString &key);
		static bool setMeta(QSqlDatabase &db,
				    const QString &key,
				    const QString &value);

		/**
			Apply the connection wide settings a shared SQLite file on a
			network share needs. Called once per connection by Catalog.
		*/
		static void applyConnectionPragmas(QSqlDatabase &db);

	private:
		static bool applyStep(QSqlDatabase &db, int version, QString *error);
		static bool createVersion1(QSqlDatabase &db, QString *error);
		static bool createVersion2(QSqlDatabase &db, QString *error);
		static bool createVersion3(QSqlDatabase &db, QString *error);
		static bool createVersion4(QSqlDatabase &db, QString *error);
		static bool createVersion5(QSqlDatabase &db, QString *error);
};

#endif // CATALOGSCHEMA_H
