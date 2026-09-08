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
#ifndef PHYSICALVIEW_H
#define PHYSICALVIEW_H

#include "catalogproperty.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>

/**
	@brief What one cell of the catalogue says about a length.

	Two facts and not one, because a length has one state more than a
	number: nobody filled it in. The catalogue keeps the two apart - an
	empty cell is not a zero - and this is the shape that carries the
	difference out of it. Collapsing them into a bare qreal is the one
	mistake this class exists to make impossible, and it is the mistake
	that turns a part nobody measured into a part measured as nothing.
*/
class CatalogMeasure
{
	public:
		CatalogMeasure() {}
		explicit CatalogMeasure(qreal cell_value)
			: value(cell_value), declared(true) {}

			/// @return true when the cell held a number at all
		bool isDeclared() const;
		/**
			@return true when the cell held a number and there is
			something of it

			Not the same question as isDeclared, and the two are
			asked of different fields on purpose - see the table on
			CatalogPhysicalView.
		*/
		bool isLength() const;

			/// the number the cell holds, zero when it holds none
		qreal value = 0.0;
			/// true when the cell held a number
		bool declared = false;
};

/**
	@brief What the catalogue says about the body of one bought product:
	how big it is, how much air it wants, where its axis is, and whether
	its outline is drawn.

	Millimetre throughout. A pure reading over the map
	Catalog::effectiveValues returns: no folio, no element, no project, and
	no database - handed a map and a table of declarations, it answers.
	That is what lets the question "is this part ready to be drawn" be
	settled in the dialogue that edits the part, in a report that walks a
	catalogue, and in the layout, without three files disagreeing about the
	answer.

	@par Zero and empty are told apart here, and not everywhere the same way

	The catalogue is the last place that still knows the difference:
	CatalogProperty::toVariant answers with an invalid QVariant rather than
	a zero, and reading a cell with a bare toDouble would collapse the two
	before anybody saw them. What each field then does with "nobody filled
	it in" is not one answer, and the difference is deliberate:

	| Field                | Missing reads as | Why |
	|----------------------|------------------|-----|
	| width, height, depth | not measured     | nothing is 0 mm wide, so zero and empty are one state |
	| clearance            | asks for nothing | a part nobody measured must not become a violation |
	| insertion            | not declared     | an offset of zero is the top left corner, which is a real answer somebody may have chosen |
	| draw_outline         | true             | a box in millimetre is what a part with no picture has to show |

	@par The axis is a pair, and half of it is not half an answer

	insertion_x and insertion_y are indivisible: an axis needs two numbers,
	so one of them alone is not a partial record, it is a record somebody
	started and did not finish. It is reported through hasHalfInsertionPoint
	rather than completed, because the missing half would have to be
	invented - and an invented axis clips a whole row of a rail at the
	wrong height, which is an error that only shows up when the plate comes
	back drilled.
*/
class CatalogPhysicalView
{
	public:
		/**
			@return the ten keys of the catalogue this class reads,
			in the order of the table above.

			Named here as well as where they are seeded: this is the
			reading end, and a key renamed on one end only would make
			every part read as unmeasured - a state the whole family
			is designed to tolerate, so nothing would break loudly.
		*/
		static QStringList keys();

		/**
			@return below what a length is not a length, millimetre.

			The same number MountingArea::tolerance() returns, and
			they are two symbols rather than one for a reason worth
			the repetition: the catalogue core is linked without the
			layout - the test binary that exercises the catalogue
			depends on Qt Core and Qt Sql and nothing else - so it
			cannot include the mounting header. That the two agree is
			not left to reading: a case of the suite compares them.
		*/
		static qreal tolerance();
			/// @return true when @a length is a number and there is something of it
		static bool isLength(qreal length);

		/**
			@brief What one cell says about a length.
			@param values every value of the part, the initial values
			of its class included
			@param properties the properties the class declares, by
			key
			@param key which measure
			@return the number and whether anybody typed one
		*/
		static CatalogMeasure measureIn(const QHash<QString, QString> &values,
						const QHash<QString, CatalogProperty> &properties,
						const QString &key);
		/**
			@brief What one cell says about a flag.
			@param values every value of the part
			@param key which flag
			@param fallback what it means when the cell says nothing
			usable
			@return what the flag says

			The fallback is decided by the caller and not by
			CatalogProperty, which answers "invalid" for anything
			else: that is the right answer for a conversion and no
			answer at all for a drawing.
		*/
		static bool flagIn(const QHash<QString, QString> &values,
				   const QString &key,
				   bool fallback);

		/**
			@brief Read the body of a part out of its values.
			@param values every value of the part, the initial values
			of its class included - what Catalog::effectiveValues
			returns
			@param properties the properties the class declares, by
			key, so that each cell is converted the way the catalogue
			itself converts it
			@return the view
		*/
		static CatalogPhysicalView read(const QHash<QString, QString> &values,
						const QHash<QString, CatalogProperty> &properties);
		/**
			@brief Read the body of a part out of nothing but its
			values.
			@param values every value of the part
			@return the view

			For the caller that holds the map and not the class -
			a report over a package, a value read back from a file.
			A measure is converted the way CatalogPropertyType::Measure
			is converted, which is what the ten keys are declared as
			where they are seeded.
		*/
		static CatalogPhysicalView read(const QHash<QString, QString> &values);

			/// @return true when the width is a length
		bool hasWidth() const;
			/// @return true when the height is a length
		bool hasHeight() const;
		/**
			@return true when both are, so the part can be drawn as
			a rectangle in millimetre

			This is the question the whole step is named after. A
			part that answers false is not a part with a small box:
			it is a part nobody measured, and it has to be reported
			as such rather than given an invented size.
		*/
		bool hasPhysicalView() const;
			/// @return true when the depth is a length
		bool hasDepth() const;

		/**
			@return true when at least one of the four clearances was
			filled in, whatever it says.

			Kept apart from the numbers because the numbers cannot
			hold it: a clearance of zero and a clearance nobody
			measured are one state to the mounting check, on purpose,
			so a part deliberately recorded as needing no air reads
			there exactly like a part nobody looked at.
		*/
		bool hasClearance() const;

			/// @return true when both offsets of the axis were filled in
		bool hasInsertionPoint() const;
			/// @return true when exactly one of the two was filled in
		bool hasHalfInsertionPoint() const;
		/**
			@return where the axis sits inside the body of the part:
			an offset from its top left corner, x running right and y
			running down.

			(0, 0) when the pair was not declared, which is the top
			left corner and is also what a caller aligning by the
			edge would use. hasInsertionPoint is what tells the two
			apart.
		*/
		QPointF insertionOffset() const;

			/// how wide the body is, millimetre
		CatalogMeasure width;
			/// how tall it is, millimetre
		CatalogMeasure height;
			/// how deep it is, millimetre
		CatalogMeasure depth;
			/// the air it asks for above, millimetre
		CatalogMeasure clearance_top;
			/// the air it asks for below, millimetre
		CatalogMeasure clearance_bottom;
			/// the air it asks for on its left, millimetre
		CatalogMeasure clearance_left;
			/// the air it asks for on its right, millimetre
		CatalogMeasure clearance_right;
			/// how far right of its own corner the axis sits, millimetre
		CatalogMeasure insertion_x;
			/// how far below its own corner the axis sits, millimetre
		CatalogMeasure insertion_y;
			/// whether the outline of the part is drawn
		bool draw_outline = true;
};

#endif // PHYSICALVIEW_H
