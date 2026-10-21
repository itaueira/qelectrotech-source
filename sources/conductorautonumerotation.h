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
#ifndef CONDUCTORAUTONUMEROTATION_H
#define CONDUCTORAUTONUMEROTATION_H

#include "autoNum/assignvariables.h"
#include "qetgraphicsitem/conductor.h"

#include <QList>
#include <QPointer>

class AssemblyState;
class Diagram;
class Element;
class QUndoCommand;
class ConductorProperties;

/**
	@brief The ConductorAutoNumerotation class

	@par A project marked as built (T29)

	Once the panel is wired, the number of a wire is a printed sleeve on a
	length of wire lying in a duct. So the numbering leaves alone every
	conductor the photograph of the marking holds: a potential drawn after
	the marking is numbered as it always was, and a potential the
	photograph already holds is never written over. Nothing is stored on
	the conductor - the freezing is read from the photograph, the same way
	it is for the components.

	A new conductor is not the only thing that joins two potentials: linking
	two folio reports does too. LinkElementCommand answers that question by
	the same rule, through settleFrozenPotential().

	@par A wire left out of the numbering (T77)

	Not the same as frozen, and the difference is the point. A frozen wire
	keeps its number and gives it to its potential: whoever joins it takes
	that number. A wire left out (Conductor::isFreezeLabel()) keeps its text
	and gives nothing: the other wires of its potential are numbered as if
	it were not there, nobody takes its name, and its number is not
	counted. counted() is the potential the numbering sees.
*/
class ConductorAutoNumerotation
{
	public:
		//constructors & destructor
		ConductorAutoNumerotation (Conductor *conductor,
					   Diagram *diagram,
					   QUndoCommand *undo_parent = nullptr);

		//methods
		void        numerate       ();
		void        applyText      (const QString&);
		static void newProperties(Diagram *diagram,
					  ConductorProperties &cp,
					  autonum::sequentialNumbers &seq);

		/**
			@param conductor
			@return true when the numbering has to leave the text of
			@a conductor where it is: the project it is drawn in is marked
			as built, and this conductor was already drawn when it was.

			The project is reached through the sheet @a conductor is on. A
			conductor on no sheet - one just created and not added yet - is
			in no photograph, and is therefore never frozen.
		*/
		static bool isFrozen(const Conductor *conductor);
		/**
			@param conductor
			@param state : the assembly state to judge @a conductor against
			@return the same answer as above, against a state handed in.

			Both halves, as for a component: the stage says the panel is
			built, and the photograph says this conductor existed then. A
			photograph left over from a marking that was taken back freezes
			nothing.
		*/
		static bool isFrozen(const Conductor *conductor,
				     const AssemblyState &state);
		/**
			@return whether @a conductor is left out of the automatic
			numbering: marked so (T77), or with one end on a component
			kept out of the bill of materials (T81). The second is read
			from the component each time, and nothing is written on the
			wire: the component back on the list brings its wires back.
		*/
		static bool isIgnored(const Conductor *conductor);
		/**
			@return whether @a element is kept out of the bill of
			materials: its exclude_from_bom information, read as the list
			reads it (QET::infoFlagIsTrue())
		*/
		static bool isOutOfBom(const Element *element);
		/// @a conductors without the ones left out of the numbering
		template <typename Container>
		static Container counted(Container conductors)
		{
			for (auto it = conductors.begin() ; it != conductors.end() ; ) {
				if (isIgnored(*it)) {
					it = conductors.erase(it);
				} else {
					++it;
				}
			}
			return conductors;
		}
		/**
			@param potential : every conductor of the potential two
			potentials are being joined into, whatever joins them - a new
			conductor, or two folio reports being linked
			@param diagram : a sheet of the project, read for its assembly
			state, and the sheet whose undo stack takes the change when
			@a parent_undo is null
			@param parent_undo : the command the change is added to, as a
			child; when null, the change is pushed on its own
			@return true when the potential holds a frozen conductor and is
			settled here, without asking; false when nothing in it is
			frozen, and the caller asks which potential to keep, as it did.

			The rule a built panel answers that question by: every frozen
			conductor keeps its text; when the frozen conductors all carry
			one number, the others take it; when they carry two numbers or
			more, nothing is written at all.
		*/
		static bool settleFrozenPotential(const QList<Conductor *> &potential,
						  Diagram *diagram,
						  QUndoCommand *parent_undo = nullptr);

	private:
		//methods
		void numeratePotential ();
		void numerateNewConductor ();
		bool keepFrozenPotential ();
		bool potentialIsFrozen () const;

		//attributes
		Diagram            *m_diagram;
		QPointer<Conductor> m_conductor;
		QList <Conductor *>  conductor_list;
		QUndoCommand       *m_parent_undo;
};

#endif // CONDUCTORAUTONUMEROTATION_H
