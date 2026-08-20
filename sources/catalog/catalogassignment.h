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
#ifndef CATALOGASSIGNMENT_H
#define CATALOGASSIGNMENT_H

#include "catalogpart.h"

#include <QHash>
#include <QString>
#include <QStringList>

class Catalog;

/**
	@brief The CatalogAssignment class
	What assigning a catalog part to a component means, worked out before
	anything on the folio is touched.

	Assigning a part is not copying a reference. It brings everything the part
	knows: manufacturer, description, the real pin numbers, the physical
	dimensions, the data sheet, the accessories. That is why the catalog had to
	be a class model in the first place - the part is the whole package, and
	every list downstream depends on it arriving complete.

	This class holds no state and touches no graphics item, which is what
	makes it testable without a project open. The undo command
	(AssignCatalogPartCommand) is what applies the result.
*/
class CatalogAssignment
{
	public:
		/**
			@return the element information keys a part assignment never
			writes. The tag, the numbering formula and the freeze flag belong
			to the component, not to the product bought for it; the
			installation and location come from the project structure. A
			catalog property that happens to be named after one of them must
			not silently renumber the drawing.
		*/
		static QStringList protectedElementKeys();

		/**
			@param catalog
			@param part
			@return every value the assignment writes into the information of
			the component, the part code and revision included.

			Keys that coincide with the fields QElectroTech already has land
			exactly where the program already shows and exports them; the
			others are stored as extra properties in the .qet, which is what
			lets a project keep its catalog data on a machine that cannot
			reach the catalog.
		*/
		static QHash<QString, QString> valuesForElement(const Catalog &catalog,
								const CatalogPart &part);

		/**
			@param part
			@param group : which sub symbol of the part is being assigned,
			empty for a part drawn as one symbol
			@param terminal_count : how many terminals the symbol has
			@return one name per terminal, in terminal order. An empty string
			means "keep the label the symbol already had": a part sheet that
			only lists the coil must not blank the contacts.
		*/
		static QStringList terminalNames(const CatalogPart &part,
						 const QString &group,
						 int terminal_count);

		/**
			@param values : the information of a component
			@return true when the component has no catalog part assigned.
			This is what the end of project report counts, and the reason it
			can be trusted is that it looks at one key only.
		*/
		static bool isWithoutPart(const QHash<QString, QString> &values);

		/// The information keys that carry the link to the catalog
		static QString partCodeKey();
		static QString partRevisionKey();
};

#endif // CATALOGASSIGNMENT_H
