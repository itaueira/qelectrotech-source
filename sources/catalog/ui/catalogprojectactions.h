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
#ifndef CATALOGPROJECTACTIONS_H
#define CATALOGPROJECTACTIONS_H

#include "../catalogpart.h"

#include <QList>

class Catalog;
class Element;
class QETProject;
class QWidget;

/**
	@brief What the catalog does to a project.

	These are the three moves of the day. They live outside the dialogs
	because the same move is reached from the folio, from the part list and
	from the bill of material, and it has to behave the same way from all
	three.
*/
namespace CatalogProjectActions
{
	/**
		Every component of @a project that has no catalog part assigned.
		Reports, terminals and thumbnails are left out: they are not things
		that get bought.
	*/
	QList<Element *> componentsWithoutPart(QETProject *project);

	/// Every component of @a project, same filter as above
	QList<Element *> components(QETProject *project);

	/**
		Build a catalog part out of the components selected on the folio:
		one pin per terminal, in terminal order, tagged with the symbol it
		came from so that a contactor drawn as a coil plus four contacts
		gives each symbol its own numbers.

		This is the registration flow the specification asks for - draw the
		circuit, fix the terminal numbers, fill in the code, save. Nobody
		stops for a week to fill a catalog before drawing.
	*/
	CatalogPart partFromElements(const Catalog &catalog, const QList<Element *> &elements);

	/**
		Assign @a part to @a elements through the undo stack of the project,
		so that one Ctrl+Z takes the whole assignment back.
		@return how many components were touched
	*/
	int assignPart(const QList<Element *> &elements,
		       const Catalog &catalog,
		       const CatalogPart &part);

	/**
		Link @a accessory to the component it belongs to, asking which one.

		The other half of "accessory as a first class object": the accessory
		is drawn on the folio like any other symbol, in its own location, and
		this says whose it is. The link holds the uuid of the owner and not
		its tag, so renumbering the project does not break it.

		@return true when a link was made
	*/
	bool linkAccessory(Element *accessory, QWidget *parent);

	/// Every component of @a project that could own an accessory
	QList<Element *> possibleOwners(QETProject *project, Element *accessory);

	/// Show the end of project report of the components with no part
	void showMissingPartReport(QETProject *project, QWidget *parent);
}

#endif // CATALOGPROJECTACTIONS_H
