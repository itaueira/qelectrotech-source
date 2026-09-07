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
			@param catalog
			@param part
			@param current : the information the component carries today
			@return the values to write, given what is already there.

			Same as the overload above, except about erasing. A part whose
			`comment` is empty says nothing about the comment; it does not say
			the comment is empty. So an empty value only overwrites what the
			part the component carried before had put in that field - which is
			the difference between clearing the manufacturer of a product it no
			longer is, and deleting the sentence somebody typed while looking
			at the panel. Assigning to a whole selection at once makes that
			difference matter twelve times over.
		*/
		static QHash<QString, QString> valuesForElement(const Catalog &catalog,
								const CatalogPart &part,
								const QHash<QString, QString> &current);

		/**
			@param catalog
			@param values : the information a component carries
			@return the part those values point to, brought up to date with
			what they say. A part in the Component class when they point to
			nothing.

			The inverse of valuesForElement, and it exists because of loss: a
			component that already has a part is corrected, never replaced by a
			bare one. Saving a part rewrites its value rows, so a bare part
			carrying the same code moves the stored part to Component and
			deletes every typed value it had - and silently, because a dialog
			only ever shows the class it was handed. (CU-13.10)
		*/
		static CatalogPart partFromValues(const Catalog &catalog,
						  const QHash<QString, QString> &values);

		/**
			@param part
			@param group : which sub symbol of the part is being assigned,
			empty for a part drawn as one symbol
			@param terminal_count : how many terminals the symbol has
			@return one pin per terminal, in terminal order, so that pin and
			terminal are matched the same way everywhere in the program. A
			default built CatalogPin means the part says nothing about that
			terminal.
		*/
		static QList<CatalogPin> terminalPins(const CatalogPart &part,
						     const QString &group,
						     int terminal_count);

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
			@param part
			@param tag : the component tag, as the drawing shows it
			@param symbol_name : the name of the symbol it was drawn with
			@return the sentence the undo list shows for a single assignment.
			It names the tag, never the symbol: the symbol name is shared by
			every instance of that symbol, so an undo list built on it cannot
			say which of them was touched. The symbol name is the fallback for
			a component with no tag yet.
		*/
		static QString commandLabel(const CatalogPart &part,
					    const QString &tag,
					    const QString &symbol_name);

		/**
			@param values : the information of a component
			@return true when the component has no catalog part assigned.
			This is what the end of project report counts, and the reason it
			can be trusted is that it looks at one key only.
		*/
		static bool isWithoutPart(const QHash<QString, QString> &values);

		/**
			@return how many accessories one assignment can carry onto a
			component.

			An accessory embedded in a part is a second article bought for the
			same component, and QElectroTech already has the four auxiliary
			blocks for exactly that: nine fields each, a column each in the
			project database, a variable each for the folio texts. Storing the
			set anywhere else would have meant a bill of material that only
			this fork can read.

			Four is therefore not a chosen number, it is the number of blocks
			that exist. A part saved with more accessories than that cannot
			hand them all over, which is why the part dialog says so while the
			set is being edited, and not after an assignment has silently
			dropped one.
		*/
		static int accessoryBlockCount();

		/**
			@param key : one of the fields an auxiliary block shares with the
			main one, empty for the block itself
			@param block : 1 to accessoryBlockCount()
			@return the element information key of that field in that block
		*/
		static QString accessoryBlockKey(const QString &key, int block);

		/**
			@param catalog
			@param part
			@return the auxiliary blocks the accessories of @a part fill in.

			Every block is written, the empty ones included: a component must
			not keep the fuse of the holder it no longer is. Which of those
			empty values actually erases anything is decided by the three
			argument overload of valuesForElement, by the same rule that
			guards every other field.

			The values come from the accessory's own part, read from the
			catalog by its code, so the accessory arrives with its
			manufacturer and its order number and not only with a reference.
			The quantity is the one recorded in the set, not the one on the
			accessory's own sheet: what the component needs is how many come
			with it.
		*/
		static QHash<QString, QString> accessoryValuesForElement(const Catalog &catalog,
									 const CatalogPart &part);

		/// The information keys that carry the link to the catalog
		/**
			@brief The element information key that says which component an
			accessory belongs to.

			Holds the uuid of the owner, not its tag: a tag is renumbered, and a
			link that breaks when the project is renumbered is worse than no
			link. The accessory keeps its own location - the handle is on the
			door while the breaker is on the plate - because it is an element of
			its own and carries its own `location`.

			This is the other accessory of the specification, and not the one
			accessoryValuesForElement carries: here the accessory is an
			element drawn on a folio, an auxiliary contact block being the
			case. An accessory embedded in a part has no symbol at all - a
			fuse, a door handle - and travels as an auxiliary block instead.
			Two mechanisms, one word, and reading either name as the other is
			how the assignment came to be described as working while it was
			not.
		*/
		static QString accessoryOwnerKey();
		static QString partCodeKey();
		static QString partRevisionKey();
};

#endif // CATALOGASSIGNMENT_H
