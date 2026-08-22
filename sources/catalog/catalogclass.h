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
#ifndef CATALOGCLASS_H
#define CATALOGCLASS_H

#include <QString>

/**
	@brief The CatalogClass class
	One node of the catalog class tree. A class tells what kind of object
	something is - "Contactor" - never which model it is: twenty contactors
	of different ratings and codes all belong to the same class.

	What a class carries:
	- the tag root, i.e. the letter a tag starts with. There are two of them,
	  the house standard one and the IEC 81346 one, so that switching a
	  project between the two standards is reading another column instead of
	  rewriting every tag (see T10);
	- whether objects of this class ever have a schematic symbol. A terminal
	  end stop, a door handle or a fuse are real parts, they belong in the
	  bill of material and in the cabinet layout, and they have no symbol on
	  any folio;
	- the typed properties declared on it, which every subclass inherits.
	  Properties live in CatalogProperty and are read through
	  Catalog::effectiveProperties().
*/
class CatalogClass
{
	public:
		CatalogClass();
		CatalogClass(const QString &key, const QString &name);

		bool isNull() const;
		bool isValid(QString *error = nullptr) const;

		/**
			Turn a user visible name into a stable machine key. Same rule
			as CatalogProperty::keyFromName, kept as its own function so
			that reading one of the two headers is enough.
		*/
		static QString keyFromName(const QString &name);

	public:
		int id = 0;              ///< database identifier, 0 when not saved yet
		int parent_id = 0;       ///< owning class, 0 for a root class
		QString key;             ///< stable machine key
		QString name;            ///< user visible name
		QString description;
		QString root;            ///< tag letter, house standard
		QString root_iec;        ///< tag letter, IEC 81346
		bool has_symbol = true;  ///< false when objects of this class never have a symbol
		int order_index = 0;     ///< position among the siblings
		QString uuid;            ///< drawn by this catalog, and by this catalog only:
		                         ///< it traces where a branch came from. Identity across
		                         ///< catalogs is the key - see CatalogClassPackage.
		/**
			The numbering format of this class, as the XML a NumberingFormat
			serialises itself to. Empty means "whatever the renumbering offers as
			its default".

			On the class and not in the command, because that is the registered
			decision of T07: changing how a class is numbered has to change every
			object of that class, and a rule that lives in a dialog cannot.
		*/
		QString numbering_format;
};

#endif // CATALOGCLASS_H
