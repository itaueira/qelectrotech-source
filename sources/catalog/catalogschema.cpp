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
#include "catalogschema.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>    // Qt5 does not pull it in through QSqlQuery, Qt6 does

namespace
{
	/// Highest schema version this build writes and understands.
	const int CATALOG_SCHEMA_VERSION = 3;

	/// Run one statement, filling @a error with the driver message on failure.
	bool exec(QSqlDatabase &db, const QString &statement, QString *error)
	{
		QSqlQuery query(db);
		if (query.exec(statement)) {
			return true;
		}
		if (error)
		{
			*error = QCoreApplication::translate("CatalogSchema",
							     "Erreur SQL : %1\n%2")
				 .arg(query.lastError().text(), statement);
		}
		return false;
	}
}

/**
	@brief CatalogSchema::currentVersion
	@return the schema version this build writes and understands
*/
int CatalogSchema::currentVersion()
{
	return CATALOG_SCHEMA_VERSION;
}

/**
	@brief CatalogSchema::versionOf
	@param db
	@return the version of @a db, 0 when it has no catalog schema yet
*/
int CatalogSchema::versionOf(QSqlDatabase &db)
{
	if (!db.isOpen()) {
		return 0;
	}
	if (!db.tables().contains(QStringLiteral("catalog_meta"))) {
		return 0;
	}
	const QString value = meta(db, QStringLiteral("schema_version"));
	bool ok = false;
	const int version = value.toInt(&ok);
	return ok ? version : 0;
}

/**
	@brief CatalogSchema::applyUpTo
	@param db
	@param target_version
	@param error
	@return true when @a db is at @a target_version afterwards
*/
bool CatalogSchema::applyUpTo(QSqlDatabase &db, int target_version, QString *error)
{
	if (!db.isOpen())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogSchema",
							     "La base du catalogue n'est pas ouverte.");
		}
		return false;
	}

	if (target_version > CATALOG_SCHEMA_VERSION)
	{
		if (error)
		{
			*error = QCoreApplication::translate("CatalogSchema",
							     "Version de schéma demandée (%1) supérieure à celle connue par cette version du logiciel (%2).")
				 .arg(target_version).arg(CATALOG_SCHEMA_VERSION);
		}
		return false;
	}

	int version = versionOf(db);

	if (version > target_version)
	{
		// A newer catalog than this build understands. Refuse instead of
		// working on it: the machine that was not updated must not write.
		if (error)
		{
			*error = QCoreApplication::translate("CatalogSchema",
							     "Le catalogue est en version %1, cette version du logiciel connaît la version %2. Mettez le logiciel à jour avant d'ouvrir ce catalogue.")
				 .arg(version).arg(target_version);
		}
		return false;
	}

	while (version < target_version)
	{
		const int next = version + 1;
		if (!applyStep(db, next, error)) {
			return false;
		}
		version = next;
	}

	return true;
}

/**
	@brief CatalogSchema::applyStep
	@param db
	@param version : the version to move to, one step at a time
	@param error
	@return true on success. The whole step is one transaction: a step that
	fails halfway leaves the database at the previous version instead of
	somewhere in between.
*/
bool CatalogSchema::applyStep(QSqlDatabase &db, int version, QString *error)
{
	const bool in_transaction = db.transaction();

	bool ok = false;
	switch (version)
	{
		case 1:  ok = createVersion1(db, error); break;
		case 2:  ok = createVersion2(db, error); break;
		case 3:  ok = createVersion3(db, error); break;
		default:
			if (error)
			{
				*error = QCoreApplication::translate("CatalogSchema",
								     "Aucune migration connue vers la version %1.")
					 .arg(version);
			}
			ok = false;
			break;
	}

	if (ok) {
		ok = setMeta(db, QStringLiteral("schema_version"), QString::number(version));
	}

	if (in_transaction)
	{
		if (ok) {
			db.commit();
		} else {
			db.rollback();
		}
	}

	return ok;
}

/**
	@brief CatalogSchema::createVersion1
	The class and property model of T12: the class tree, the typed properties
	declared on the classes, the controlled lists, and the parts with their
	values and pins.

	Two tables carry the whole idea:
	- catalog_property holds *declarations*, one row per field the user
	  created, attached to one class;
	- catalog_part_value holds *values*, one row per part and key.
	Neither of them changes shape when a field is added, which is why adding
	a field never needs a migration nor a rebuild.
*/
bool CatalogSchema::createVersion1(QSqlDatabase &db, QString *error)
{
	const QStringList statements = {
		QStringLiteral("CREATE TABLE catalog_meta ("
			       "key TEXT PRIMARY KEY NOT NULL,"
			       "value TEXT)"),

		QStringLiteral("CREATE TABLE catalog_class ("
			       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
			       "parent_id INTEGER REFERENCES catalog_class(id),"
			       "key TEXT NOT NULL UNIQUE,"
			       "name TEXT NOT NULL,"
			       "description TEXT,"
			       "root TEXT,"
			       "root_iec TEXT,"
			       "has_symbol INTEGER NOT NULL DEFAULT 1,"
			       "order_index INTEGER NOT NULL DEFAULT 0,"
			       "uuid TEXT NOT NULL UNIQUE)"),

		QStringLiteral("CREATE INDEX idx_catalog_class_parent "
			       "ON catalog_class(parent_id)"),

		QStringLiteral("CREATE TABLE catalog_list ("
			       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
			       "name TEXT NOT NULL UNIQUE)"),

		QStringLiteral("CREATE TABLE catalog_list_value ("
			       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
			       "list_id INTEGER NOT NULL REFERENCES catalog_list(id) ON DELETE CASCADE,"
			       "value TEXT NOT NULL,"
			       "order_index INTEGER NOT NULL DEFAULT 0,"
			       "UNIQUE(list_id, value))"),

		QStringLiteral("CREATE TABLE catalog_property ("
			       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
			       "class_id INTEGER NOT NULL REFERENCES catalog_class(id) ON DELETE CASCADE,"
			       "key TEXT NOT NULL,"
			       "name TEXT NOT NULL,"
			       "type TEXT NOT NULL DEFAULT 'text',"
			       "list_behaviour TEXT NOT NULL DEFAULT 'none',"
			       "list_name TEXT,"
			       "options TEXT,"
			       "default_value TEXT,"
			       "unit TEXT,"
			       "description TEXT,"
			       "order_index INTEGER NOT NULL DEFAULT 0,"
			       "UNIQUE(class_id, key))"),

		QStringLiteral("CREATE INDEX idx_catalog_property_class "
			       "ON catalog_property(class_id)"),

		QStringLiteral("CREATE TABLE catalog_part ("
			       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
			       "class_id INTEGER NOT NULL REFERENCES catalog_class(id),"
			       "code TEXT NOT NULL,"
			       "revision INTEGER NOT NULL DEFAULT 1,"
			       "is_current INTEGER NOT NULL DEFAULT 1,"
			       "created_at TEXT,"
			       "updated_at TEXT,"
			       "UNIQUE(code, revision))"),

		QStringLiteral("CREATE INDEX idx_catalog_part_code "
			       "ON catalog_part(code)"),

		QStringLiteral("CREATE INDEX idx_catalog_part_class "
			       "ON catalog_part(class_id)"),

		QStringLiteral("CREATE TABLE catalog_part_value ("
			       "part_id INTEGER NOT NULL REFERENCES catalog_part(id) ON DELETE CASCADE,"
			       "key TEXT NOT NULL,"
			       "value TEXT,"
			       "PRIMARY KEY(part_id, key))"),

		QStringLiteral("CREATE TABLE catalog_part_pin ("
			       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
			       "part_id INTEGER NOT NULL REFERENCES catalog_part(id) ON DELETE CASCADE,"
			       "label TEXT NOT NULL,"
			       "role TEXT NOT NULL DEFAULT 'unknown',"
			       "pair TEXT,"
			       "group_name TEXT,"
			       "order_index INTEGER NOT NULL DEFAULT 0)"),

		QStringLiteral("CREATE INDEX idx_catalog_part_pin_part "
			       "ON catalog_part_pin(part_id)")
	};

	for (const QString &statement : statements)
	{
		if (!exec(db, statement, error)) {
			return false;
		}
	}

	return true;
}

/**
	@brief CatalogSchema::createVersion2
	What T13 and T14 add on top of the model: the accessories a part brings
	along when it is assigned, and where the data of a part came from.

	This is also the step that proves the migration path works, because a
	catalog created by the version that only had step 1 has to reach here on
	its own, on open, without anybody running a script.
*/
bool CatalogSchema::createVersion2(QSqlDatabase &db, QString *error)
{
	const QStringList statements = {
		QStringLiteral("CREATE TABLE catalog_part_accessory ("
			       "part_id INTEGER NOT NULL REFERENCES catalog_part(id) ON DELETE CASCADE,"
			       "accessory_code TEXT NOT NULL,"
			       "quantity REAL NOT NULL DEFAULT 1,"
			       "PRIMARY KEY(part_id, accessory_code))"),

		QStringLiteral("ALTER TABLE catalog_part ADD COLUMN origin TEXT"),

		QStringLiteral("ALTER TABLE catalog_part ADD COLUMN origin_date TEXT")
	};

	for (const QString &statement : statements)
	{
		if (!exec(db, statement, error)) {
			return false;
		}
	}

	return true;
}

/**
	@brief CatalogSchema::createVersion3
	The column mapping profiles of the spreadsheet importer (T14).

	They live here and not in the settings of the station on purpose: the
	layout of a supplier's price list is a fact about that supplier, not about
	whoever happens to import it. Stored as the XML the profile serialises
	itself to, so that adding a field to a profile is not another migration.
*/
bool CatalogSchema::createVersion3(QSqlDatabase &db, QString *error)
{
	return exec(db, QStringLiteral("CREATE TABLE catalog_import_profile ("
				       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
				       "name TEXT NOT NULL UNIQUE,"
				       "payload TEXT NOT NULL,"
				       "updated_at TEXT)"), error);
}

/**
	@brief CatalogSchema::meta
	@param db
	@param key
	@return the value stored for @a key in catalog_meta, empty when absent
*/
QString CatalogSchema::meta(QSqlDatabase &db, const QString &key)
{
	QSqlQuery query(db);
	query.prepare(QStringLiteral("SELECT value FROM catalog_meta WHERE key = :key"));
	query.bindValue(QStringLiteral(":key"), key);
	if (query.exec() && query.next()) {
		return query.value(0).toString();
	}
	return QString();
}

/**
	@brief CatalogSchema::setMeta
	@param db
	@param key
	@param value
	@return true on success
*/
bool CatalogSchema::setMeta(QSqlDatabase &db, const QString &key, const QString &value)
{
	QSqlQuery query(db);
	query.prepare(QStringLiteral("INSERT INTO catalog_meta (key, value) VALUES (:key, :value) "
				     "ON CONFLICT(key) DO UPDATE SET value = :value2"));
	query.bindValue(QStringLiteral(":key"), key);
	query.bindValue(QStringLiteral(":value"), value);
	query.bindValue(QStringLiteral(":value2"), value);
	return query.exec();
}

/**
	@brief CatalogSchema::applyConnectionPragmas
	@param db
	Settings a catalog connection needs. Deliberately not WAL: the write
	ahead log needs shared memory the SMB protocol does not provide, so a
	catalog on a network share must stay on the rollback journal. The busy
	timeout is what turns "database is locked" from an error into a wait
	when a colleague is saving a part at the same moment.
*/
void CatalogSchema::applyConnectionPragmas(QSqlDatabase &db)
{
	QSqlQuery query(db);
	query.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
	query.exec(QStringLiteral("PRAGMA journal_mode = DELETE"));
	query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
}
