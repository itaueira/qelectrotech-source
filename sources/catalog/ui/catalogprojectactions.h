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
#include "../catalogvalueorigin.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

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

	/**
		@brief One component of a project whose product code cannot be
		drawn in millimetre yet.

		The sibling of "component with no part", and deliberately not the
		same list: a component nobody bought a product for is a question
		for whoever is buying, while a product nobody measured is a
		question for whoever keeps the catalogue. Answering both in one
		column would make the column mean two things, and the count under
		it mean neither.
	*/
	class MissingPhysicalView
	{
		Q_DECLARE_TR_FUNCTIONS(MissingPhysicalView)

		public:
			/// Why the product cannot be drawn
			enum State
			{
				UnknownPart, ///< the catalogue holds no such code
				NoMeasure,   ///< it holds it, and neither dimension
				WidthOnly,   ///< the width alone
				HeightOnly   ///< the height alone
			};

			/**
				@return what is missing, in one sentence, naming
				the class when the one dimension that is there was
				not typed on the product either.

				That clause is what keeps the report from being a
				search for empty cells: a width inherited from the
				initial value of a class is a number nobody measured
				on this product, and whoever is about to type the
				missing height has to know that the width beside it
				is the generic one.
			*/
			QString describe() const;

				/// the component on the folio, for the double click
			Element *element = nullptr;
				/// the product code it carries
			QString part_code;
				/// what the catalogue calls that product
			QString description;
				/// why it cannot be drawn
			State state = NoMeasure;
			/**
				where the one dimension it does have was written.

				Unread for the two states that have no dimension at
				all, which is not the same statement as Unset - see
				CatalogValueOrigin.
			*/
			CatalogValueOrigin measured_origin;
	};

	/**
		@brief What a project still needs measured before it can be laid
		out, and what it already has.

		Read from the catalogue and not from the copy the components
		carry, because the report exists to be worked through: the row has
		to leave the list when the product is measured, and the values a
		component carries are a copy taken when the part was assigned - by
		design, so that a delivered project keeps the numbers it was
		delivered with. That is also what the layout reads
		(MountingPartReader), so the report and the drawing cannot
		disagree about which product is ready.
	*/
	class PhysicalViewReport
	{
		public:
			/// @return the distinct product codes to measure, sorted
			QStringList codesToMeasure() const;

				/// one entry per component, sorted by code then tag
			QList<MissingPhysicalView> missing;
				/// how many components the project draws
			int components = 0;
				/// how many of them carry a product code
			int with_part = 0;
			/**
				how many of them are drawable on a number that
				came from their class and not from their product.

				Counted and not listed. Those products can be drawn,
				so putting them in the list would make it say two
				things; but a size inherited from a class is a size
				nobody measured for that product, and a report that
				never mentioned it would let inheritance pass for
				cataloguing.
			*/
			int inherited_size = 0;
			/**
				true when the catalogue answered at all.

				False leaves missing empty rather than filled with
				every component of the project: a share that is down
				holds no product code, and a list saying that
				everything needs measuring is a false alarm that
				looks exactly like a finding.
			*/
			bool catalog_read = false;
	};

	/**
		@brief Which components of @a project point at a product the
		catalogue cannot draw.
		@param project
		@param catalog the catalogue to ask
		@return the report

		A component with no product code at all is not in it. That is the
		one thing this report must not do: it is the whole subject of
		showMissingPartReport, it is answered there by a different move -
		buy something, or draw something that is not bought - and the two
		lists mixed would be a list of two problems with one heading.

		The insertion point is not read here either, although the same
		product record holds it: half an axis is refused where the product
		is edited, and a part measured in width and height is ready to be
		drawn whether or not anybody chose where it clips onto a rail.
	*/
	PhysicalViewReport physicalViewReport(QETProject *project,
					      const Catalog &catalog);

	/// Show the report of the products of the project with no physical view
	void showMissingPhysicalViewReport(QETProject *project, QWidget *parent);
}

#endif // CATALOGPROJECTACTIONS_H
