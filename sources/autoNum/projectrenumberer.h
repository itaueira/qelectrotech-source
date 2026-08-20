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
#ifndef PROJECTRENUMBERER_H
#define PROJECTRENUMBERER_H

#include "numberingformat.h"
#include "renumberplan.h"

#include <QList>
#include <QString>

class Catalog;
class Element;
class QETProject;

/**
	@brief Between a project and the renumbering rules.

	The rules live in Renumberer, which knows nothing about a project and can
	therefore be tested. This is the part that goes and asks: which components
	are there, where are they, what root does each one's class give, and which
	of them did somebody number by hand.
*/
namespace ProjectRenumberer
{
	/**
		The components to renumber: those of @a project, or only those of
		@a selection when it is not empty.

		Selecting a control circuit that was just inserted and renumbering
		only that is the most frequent use of the day, which is why the
		selection is a first class case and not an afterthought.
	*/
	QList<Element *> components(QETProject *project,
				    const QList<Element *> &selection = QList<Element *>());

	/**
		@param catalog
		@param elements
		@return one input per element, everything resolved: position, folio,
		tag root, and whether it is frozen.
	*/
	QList<RenumberInput> inputsFor(const Catalog &catalog,
				       const QList<Element *> &elements,
				       const NumberingFormat &fallback = NumberingFormat());

	/**
		@param catalog
		@param element
		@param fallback : what to use when the class declares no format
		@return the numbering format of the class of @a element
	*/
	NumberingFormat formatFor(const Catalog &catalog,
				  const Element *element,
				  const NumberingFormat &fallback);

	/**
		@param catalog
		@param element
		@return the tag root to use for @a element.

		Three sources, in order, and the order is what keeps this from
		breaking projects that have no catalog part yet:
		1. the class of the catalog part assigned to it (T12);
		2. the letters at the start of the tag it already carries;
		3. nothing - the format then produces a bare number, which is what
		   QElectroTech does today for a symbol it knows nothing about.
	*/
	QString rootFor(const Catalog &catalog, const Element *element, bool iec);

	/// The letters at the start of a tag: "MTR12" gives "MTR"
	QString rootOfLabel(const QString &label);

	/// true when this component was numbered by hand and must not be touched
	bool isFrozen(const Element *element);

	/**
		@brief The component that already carries @a label, if any.
		@param project
		@param label : the tag being typed
		@param location : the `+` part of the component being typed into
		@param except : the component being edited, which does not count as a
		collision with itself
		@return the other component, or nullptr

		The location is part of the comparison and not ignored: two cabinets
		may each legitimately have their own -Q1, and refusing that would be
		refusing the way a real installation is tagged. Same rule as
		Renumberer::isLabelFree, said again here because this lookup answers
		one typed label rather than planning a whole renumbering.
	*/
	Element *elementWithLabel(QETProject *project,
				  const QString &label,
				  const QString &location,
				  Element *except = nullptr);

	/**
		Apply @a plan to @a elements through the undo stack of the project,
		so that one Ctrl+Z takes the whole renumbering back.
		@return how many components changed
	*/
	int applyPlan(const QList<Element *> &elements, const RenumberPlan &plan);
}

#endif // PROJECTRENUMBERER_H
