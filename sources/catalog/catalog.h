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
#ifndef CATALOG_H
#define CATALOG_H

#include "catalogclass.h"
#include "catalogpart.h"
#include "catalogproperty.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QStringList>

/**
	@brief The Catalog class
	The shared catalog: a class tree, the typed properties declared on those
	classes, the controlled lists, and the parts.

	It is a database of its own, not the project database. The project
	database (sources/dataBase/projectdatabase.cpp) is derived from one .qet
	and lives in memory; the catalog is persistent, shared by the whole
	office, and normally sits in the shared environment folder so that
	everybody draws from the same parts.

	Three layers are kept apart on purpose, and the whole plan leans on it:

	| Layer     | What it is                        | Where it lives      |
	|-----------|-----------------------------------|---------------------|
	| Symbol    | the drawing and the terminals     | the .elmt file      |
	| Component | the object created on the folio   | the .qet file       |
	| Part      | the product bought from a maker   | this catalog        |

	The same contactor symbol serves twenty different contactors. The tag
	comes from the class. The pin numbers come from the part and replace the
	provisional labels of the symbol when a part is assigned.

	Reading is cached: classes, properties and lists are read once on open
	and kept in memory, so drawing does not depend on the network share
	staying up. Parts are queried on demand, because a real catalog has
	thousands of them.
*/
class Catalog : public QObject
{
	Q_OBJECT

	public:
		explicit Catalog(QObject *parent = nullptr);
		~Catalog() override;

		bool open(const QString &file_path, QString *error = nullptr);
		bool openInMemory(QString *error = nullptr);
		void close();
		bool isOpen() const;

		QString filePath() const;
		QString lastError() const;
		int schemaVersion() const;

		bool isWritable() const;
		void setWritable(bool writable);

		/**
			@return "mm" or "inch". Measure properties are always stored
			in millimetre; this only says what the user is shown, which
			is what lets the physical view work in both systems.
		*/
		QString unitSystem() const;
		bool setUnitSystem(const QString &system);

		// ---------------------------------------------------------------
		// Classes
		// ---------------------------------------------------------------
		QList<CatalogClass> classes() const;
		CatalogClass classById(int class_id) const;
		CatalogClass classByKey(const QString &key) const;
		QList<CatalogClass> childClasses(int parent_id) const;
		/**
			@param class_id
			@param ancestor_key
			@return true when @a class_id is @a ancestor_key or descends from it.
			Asked rather than compared against one key because a shop that adds
			"Accessoire de porte" under "Accessoire" still has accessories.
		*/
		bool isDescendantOf(int class_id, const QString &ancestor_key) const;
		/// @return @a class_id and every class below it
		QList<int> descendantClassIds(int class_id) const;
		/// @return the ancestry of @a class_id, root first, @a class_id last
		QList<int> classAncestry(int class_id) const;

		int addClass(const CatalogClass &catalog_class, QString *error = nullptr);
		bool updateClass(const CatalogClass &catalog_class, QString *error = nullptr);
		bool removeClass(int class_id, QString *error = nullptr);

		/**
			@param class_id
			@param iec : true for the IEC 81346 root, false for the house one
			@return the tag letter of @a class_id, inherited from the
			closest ancestor that declares one. Switching a project
			between the house standard and the norm is reading the other
			column, not rewriting tags.
		*/
		QString tagRoot(int class_id, bool iec) const;

		// ---------------------------------------------------------------
		// Properties
		// ---------------------------------------------------------------
		QList<CatalogProperty> ownProperties(int class_id) const;
		/// @return the properties of @a class_id, inherited ones first
		QList<CatalogProperty> effectiveProperties(int class_id) const;
		CatalogProperty effectiveProperty(int class_id, const QString &key) const;

		int addProperty(const CatalogProperty &property, QString *error = nullptr);
		bool updateProperty(const CatalogProperty &property, QString *error = nullptr);
		bool removeProperty(int property_id, QString *error = nullptr);
		bool setPropertyOrder(int class_id,
				      const QList<int> &property_ids,
				      QString *error = nullptr);

		/**
			Write the initial value of @a property_id into every part of
			its class and subclasses that never had that field filled.
			@return how many parts were touched, -1 on error
		*/
		int applyDefaultToExistingParts(int property_id, QString *error = nullptr);

		// ---------------------------------------------------------------
		// Controlled lists
		// ---------------------------------------------------------------
		QStringList listNames() const;
		QStringList listValues(const QString &list_name) const;
		bool setListValues(const QString &list_name,
				   const QStringList &values,
				   QString *error = nullptr);
		bool removeList(const QString &list_name, QString *error = nullptr);

		// ---------------------------------------------------------------
		// Parts
		// ---------------------------------------------------------------
		/**
			@param class_id : 0 for every class
			@param with_subclasses : also the parts of the subclasses
		*/
		QList<CatalogPart> parts(int class_id = 0, bool with_subclasses = true) const;
		CatalogPart part(int part_id) const;
		/// @return the current revision of @a code
		CatalogPart partByCode(const QString &code) const;
		/// @return exactly revision @a revision of @a code
		CatalogPart partByCode(const QString &code, int revision) const;
		QList<int> partRevisions(const QString &code) const;
		int partCount() const;

		bool savePart(CatalogPart &part, QString *error = nullptr);
		/**
			Save @a part as a new revision instead of in place. Projects
			that pinned the older revision keep the data they had; this is
			the answer to "the manufacturer changed the product", while
			savePart() is the answer to "the record was wrong".
		*/
		bool savePartAsNewRevision(CatalogPart &part, QString *error = nullptr);
		bool removePart(int part_id, QString *error = nullptr);

		QList<CatalogPart> searchParts(const QString &text,
					       int class_id = 0,
					       const QString &manufacturer = QString()) const;

		/**
			@return every value of @a part : its own values, and the
			initial value of the properties it never had filled. This is
			what a list, a bill of material or a part assignment reads.
		*/
		QHash<QString, QString> effectiveValues(const CatalogPart &part) const;

		// ---------------------------------------------------------------
		// Spreadsheet import profiles
		// ---------------------------------------------------------------
		/**
			The column mapping of a supplier's list, kept in the catalog so
			that the second person to import the same list does not map the
			columns again. The payload is whatever the profile serialises
			itself to; this class does not read it.
		*/
		QStringList importProfileNames() const;
		QString importProfile(const QString &name) const;
		bool saveImportProfile(const QString &name,
				       const QString &payload,
				       QString *error = nullptr);
		bool removeImportProfile(const QString &name, QString *error = nullptr);

		// ---------------------------------------------------------------
		// Model bootstrap
		// ---------------------------------------------------------------
		/**
			Create the class tree and the properties a fresh catalog needs
			to be usable. Called by open() when the catalog has no class at
			all; safe to call again, it does nothing when classes exist.
		*/
		bool seedDefaultModel(QString *error = nullptr);

		/// Keys of the properties seeded on the Component class
		static QStringList seededComponentPropertyKeys();

	signals:
		void classesChanged();
		void propertiesChanged(int class_id);
		void partsChanged();
		void importProfilesChanged();
		void listsChanged();

	private:
		bool finishOpen(QString *error);
		bool requireWritable(QString *error) const;
		void setError(QString *error, const QString &message) const;

		void reloadClasses();
		void reloadProperties();
		void reloadLists();

		CatalogPart readPart(int part_id) const;
		bool writePartRows(const CatalogPart &part, QString *error);
		bool validatePartValues(const CatalogPart &part, QString *error) const;
		int listIdForName(const QString &list_name) const;
		QList<CatalogPart> partsFromQuery(const QString &where,
						  const QVariantList &bindings) const;

	private:
		QSqlDatabase m_database;
		QString m_connection_name;
		QString m_file_path;
		mutable QString m_last_error;
		int m_schema_version = 0;
		bool m_writable = true;

		QList<CatalogClass> m_classes;                       ///< cache, order_index sorted
		QHash<int, QList<CatalogProperty>> m_properties;     ///< cache, per class id
		QHash<QString, QStringList> m_lists;                 ///< cache, per list name
};

#endif // CATALOG_H
