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
#ifndef WIRESECTIONS_H
#define WIRESECTIONS_H

#include "conductorproperties.h"

#include <QList>
#include <QPointer>
#include <QString>

class Conductor;
class Diagram;
class Element;
class QETProject;
class QUndoCommand;

/**
	@brief The cross-section of the wires of an electrical point, taken from
	the components of the point (T70).

	A component says which cross-section its wires need in its information
	"wire_section" (QETInformation::ELMT_WIRE_SECTION). The section of a
	point is the largest the components of the point carry - the components
	being the parents of the terminals of every conductor of the potential
	(Conductor::relatedPotentialConductors(), the conductor itself
	included) - and a component that leaves the information empty, the
	terminal of a strip say, does not count. The rule on the numbers is
	WireGauge::largest().

	The section found goes in the section of every conductor of the point
	(ConductorProperties::m_wire_section), which the formula of a conductor
	text reads as %ws. A point no component says anything about is left
	alone: its conductors keep what was typed in them.

	A point holding a wire of a panel already built is left alone too,
	whole, as the numbering leaves it (ConductorAutoNumerotation::isFrozen()
	and applyText()): the section of a wire lying in a duct is the one it
	was cut with, and the wires drawn after the marking take the number of
	that wire, so they keep its section with it.

	Three moments give a point its section: a conductor being drawn
	(ConductorAutoNumerotation::numerate(), and Terminal when it copies the
	properties of the point), the information of a component being changed
	(ChangeElementInformationCommand, in the same undo step), and the whole
	project at once (updateProject(), the folio editor's "Update the wire
	cross-sections" and the script's updateWireSections()).
*/
namespace WireSections
{
		/// One conductor whose section changes, with its properties before
		/// and after
	struct Change
	{
		QPointer<Conductor> conductor;
		ConductorProperties before;
		ConductorProperties after;
	};

		/// What @p element says about the section of its wires, as typed
	QString componentSection(const Element *element);
		/// @p conductor first, then the others of its potential
	QList<Conductor *> potentialOf(Conductor *conductor);
		/// The section of the point of @p potential, formatted; empty when
		/// none of its components says one
	QString sectionOf(const QList<Conductor *> &potential);
		/// sectionOf(potentialOf(conductor))
	QString sectionOfPotential(Conductor *conductor);

		/**
			@return the conductors of the points of @p conductors whose
			section is not the section of their point, each point once,
			and what each one gets
		*/
	QList<Change> changesFor(const QList<Conductor *> &conductors);
		/// changesFor() the conductors of the terminals of @p elements
	QList<Change> changesForElements(const QList<Element *> &elements);

		/**
			Set the properties of @p changes: the after of each, in order,
			when @p forward; the before of each, in the reverse order,
			otherwise. For an undo command that owns the changes.
		*/
	void apply(const QList<Change> &changes, bool forward);
		/**
			Add one QPropertyUndoCommand per change as a child of
			@p parent, which must not be null.
			@return how many were added
		*/
	int addCommands(const QList<Change> &changes, QUndoCommand *parent);

		/**
			Give a conductor being added the section of its point at once,
			with no undo command: its addition is the undo step. A point
			that says no section, or holds a wire of a panel already built,
			leaves it as it is.
		*/
	void giveOwnSection(Conductor *conductor);
		/**
			Give a conductor being added the section of its point, at once
			(giveOwnSection()), and the rest of the point the same section,
			which the new conductor may have just raised: under
			@p parent_undo when there is one, else pushed on the stack of
			@p diagram on its own.
			@return how many conductors of the point, the new one aside,
			change
		*/
	int giveNewConductorItsSection(Conductor *conductor, Diagram *diagram,
				       QUndoCommand *parent_undo = nullptr);

		/**
			Give every conductor of @p project the section of its point:
			one undo step per folio holding a conductor that changes, and
			none at all when nothing changes. Folios not built yet are
			built, as a point can cross them.
			@return how many conductors changed
		*/
	int updateProject(QETProject *project);
}

#endif // WIRESECTIONS_H
