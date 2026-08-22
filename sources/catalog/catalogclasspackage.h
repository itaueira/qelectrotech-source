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
#ifndef CATALOGCLASSPACKAGE_H
#define CATALOGCLASSPACKAGE_H

#include <QString>
#include <QStringList>

class Catalog;
class QDomDocument;
class QDomElement;

/**
	@brief The CatalogClassPackage class
	A branch of the class tree in one file, so that a class can be set up
	once and read anywhere.

	A class is a node plus the typed properties declared on it. Before this,
	only the node could travel - a part package names the class of its part
	and nothing more, so importing into a catalog that lacks the class
	created an empty node and the values landed as loose text. What travels
	here is the declaration: type, unit, initial value, controlled list, the
	house tag root and the IEC one, and the numbering format.

	> **Decision recorded:** identity across catalogs is the **key**, not the
	> `uuid`. Every catalog seeds its own tree with the same keys and a
	> freshly drawn `uuid` per row, so two catalogs that both have Disjoncteur
	> have the same key and different uuids. The uuid is written into the file
	> as a trace of where the branch came from and is never read back.

	Import creates what is missing and **modifies nothing**. A class or a
	property that is already there is recognised and left alone, which is why
	importing the same file twice changes nothing the second time. A property
	whose key already exists with another type is not overwritten in silence:
	it is named in the report and the import goes on, because changing the
	type of a property reinterprets the value of every part that uses it.
*/
class CatalogClassPackage
{
	public:
		/**
			@brief What an import did, or what it would do.
			Counted rather than logged: whoever receives a branch of classes
			has the right to read what they are accepting before it goes in,
			and a dialog needs numbers and names, not a stream of messages.
		*/
		class Report
		{
			public:
				int classes_created = 0;
				int classes_found = 0;
				int properties_created = 0;
				int properties_found = 0;
				int lists_created = 0;
				int lists_found = 0;
				/// The classes that are missing here, by user visible name
				QStringList missing_classes;
				/// What was not applied, and why, in one line each
				QStringList refused;

				/// true when the target catalog already has all of it
				bool changesNothing() const;
				/// A human readable summary, for the confirmation dialog
				QString toText() const;
		};

		static QString fileExtension();
		static QString fileFilter();
		/// A file name derived from the class name, safe on every file system
		static QString suggestedFileName(const QString &class_name);

		/**
			@param document : the document the element will belong to
			@param catalog
			@param class_id : the class to export, with everything below it
			@param include_descendants : false to carry the class and its
			ancestry only. A part package needs the class of its part, not
			the subclasses of it the sender happens to have.
			@return the block that describes the branch, to write on its own
			or to embed in another package.

			The ancestry comes along, root first, so that the branch can be
			put back in the same place. An ancestor is a node with its own
			declared properties, same as any other class in the file.
		*/
		static QDomElement toXml(QDomDocument &document,
					 const Catalog &catalog,
					 int class_id,
					 bool include_descendants = true);

		/**
			@param element : a block written by toXml()
			@param catalog
			@param report : filled in when not null
			@param error
			@return true when the block was read; false only when the block
			is not one of ours. A class the file could not place, or a
			property whose type clashes, is reported and does not fail the
			import - the rest of the branch is still worth having.
		*/
		static bool applyXml(const QDomElement &element,
				     Catalog &catalog,
				     Report *report = nullptr,
				     QString *error = nullptr);

		/**
			@param element : a block written by toXml()
			@param catalog
			@return what applyXml() would do, without doing it.
		*/
		static Report plan(const QDomElement &element, const Catalog &catalog);

		static bool write(const QString &file_path,
				  const Catalog &catalog,
				  int class_id,
				  QString *error = nullptr);

		static bool read(const QString &file_path,
				 Catalog &catalog,
				 Report *report = nullptr,
				 QString *error = nullptr);

		/// @return what read() would do to @a catalog, without doing it
		static Report summary(const QString &file_path,
				      const Catalog &catalog,
				      QString *error = nullptr);

		/// The tag name of the block, so that other packages can find it
		static QString blockTagName();
};

#endif // CATALOGCLASSPACKAGE_H
