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
#ifndef CATALOGPACKAGE_H
#define CATALOGPACKAGE_H

#include "catalogclasspackage.h"
#include "catalogpart.h"

#include <QString>
#include <QStringList>

class Catalog;

/**
	@brief The CatalogPackage class
	One catalog part in one file, for trading parts between catalogs.

	> **Decision recorded:** the package carries **data, not drawing**. It
	> takes the properties, the pins, the physical view, the reference image
	> and the accessories, and it does **not** take the schematic symbol.
	>
	> The symbol is generated locally from the pinout, because whoever
	> receives the part may need it horizontal and not vertical, or split
	> across several blocks instead of one. Packing the drawing along looks
	> more convenient and forces the receiver to accept the graphical
	> standard of whoever exported.

	Price and commercial terms are left out too, and for a different reason:
	a price belongs to a company and to a date, not to a part.

	What it does carry is the **declaration of its class**: type, unit, initial
	value, controlled list and numbering format, with the ancestry so that the
	receiver can put the class back where it belongs. Before this a package
	named its class and nothing more, so importing into a catalog that lacked
	the class produced an empty node under Composant and the values landed
	there as loose text. Same serialization and same identity rule as
	CatalogClassPackage: the **key** identifies, the `uuid` only traces where
	the class came from.
*/
class CatalogPackage
{
	public:
		static QString fileExtension();
		static QString fileFilter();
		/// A file name derived from the part code, safe on every file system
		static QString suggestedFileName(const CatalogPart &part);

		/**
			Property keys a package never carries, whatever the class
			declares. Said in one place so that adding a commercial field
			to a class does not quietly start exporting it.
		*/
		static QStringList excludedKeys();

		static bool write(const QString &file_path,
				  const Catalog &catalog,
				  const CatalogPart &part,
				  QString *error = nullptr);

		/**
			@param file_path
			@param catalog : used to resolve the class of the package by key
			@param error
			@return the part the package describes, with class_id resolved
			and id 0. A null part when the file cannot be read; a part with
			class_id 0 when the class does not exist here, which the caller
			has to decide about rather than guess.
		*/
		static CatalogPart read(const QString &file_path,
					const Catalog &catalog,
					QString *error = nullptr);

		/// The class key the package names, whether or not it exists here
		static QString classKeyOf(const QString &file_path);

		/**
			@param file_path
			@param catalog
			@param report : filled in when not null
			@param error : set only when the file cannot be read at all
			@return true when the package carries the declaration of its
			class. false with no error means a package written before this
			existed: it names its class and nothing more, and an empty node
			is all anyone can make of it.

			Nothing is written to @a catalog. This is what applyClass() would
			do, so that whoever imports reads it before saying yes.
		*/
		static bool classPlan(const QString &file_path,
				      const Catalog &catalog,
				      CatalogClassPackage::Report *report = nullptr,
				      QString *error = nullptr);

		/**
			@param file_path
			@param catalog
			@param report : filled in when not null
			@param error
			@return true when the declaration was applied.

			Creates in @a catalog what the package declares and is missing
			here, and modifies nothing that is already here - importing the
			same package twice changes nothing the second time.
		*/
		static bool applyClass(const QString &file_path,
				       Catalog &catalog,
				       CatalogClassPackage::Report *report = nullptr,
				       QString *error = nullptr);
};

#endif // CATALOGPACKAGE_H
