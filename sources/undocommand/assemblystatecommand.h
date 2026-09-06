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
#ifndef ASSEMBLYSTATECOMMAND_H
#define ASSEMBLYSTATECOMMAND_H

#include "../autoNum/assemblystate.h"

#include <QPointer>
#include <QUndoCommand>

class QETProject;

/**
	@brief The AssemblyStateCommand class
	Mark a project as assembled, or take that marking back, with one Ctrl+Z.

	@par A photograph, not a loop

	The only precedent this program has for a change in bulk is
	RenumberCommand, which keeps one change per component: four hundred
	components, four hundred pairs of DiagramContext. That shape is right
	there, because a renumbering really does write on each one of them.

	Here nothing is written on any component at all. What the marking
	produces is one set of uuids with the label each of them carried, so the
	command keeps exactly two of those - the state before and the state after
	- and undo is putting the first one back. Not four hundred undos, and not
	four hundred stored objects either.

	How big that gets, counted rather than guessed: the largest project
	shipped in examples/ is m_000.qet, whose file holds 928 <element> and 457
	<conductor> nodes, so a photograph of it is at most 1 385 entries and the
	pair kept by the command at most 2 770. Each entry is two short strings -
	a uuid and a label - against the whole information map of a component,
	kept twice, for each of the changes a RenumberCommand over the same
	project would hold. The shape that is easier to undo is therefore also
	the smaller one, and there is no trade to argue about.

	@par Which stack it goes on

	QETProject::undoStack(), and it matters that this is not a choice between
	two stacks: Diagram::undoStack() returns the project's stack, so there is
	exactly one per project. A state that belongs to the project would be
	wrong on a per-sheet stack, and there is no per-sheet stack to be wrong
	on.

	@par The trap of the identity

	The photograph is a set of uuids, and a conductor that has none in the
	file is given a fresh one while the project is being opened - see
	Conductor::fromXml. So a project written by an older QElectroTech has
	conductor identities that change every time it is opened, and a
	photograph taken before it has been saved once by this version points, on
	the next opening, at conductors that no longer exist. The marking dialog
	is what refuses that case (decision P87); the command does not guess.
*/
class AssemblyStateCommand : public QUndoCommand
{
	public:
		AssemblyStateCommand(QETProject *project,
				     const AssemblyState &new_state,
				     QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

		/// How many items the state this command applies holds frozen.
		int frozenCount() const;
		/// Whether pushing this would change anything at all.
		bool changesAnything() const;

		/**
			@param project
			@param stage the stage to mark
			@return the state @a project would be put in: @a stage, plus the
			photograph of every component and every conductor drawn on it
			right now, each with the label it is carrying.

			Marking back to AssemblyStage::InProject returns an empty
			photograph, because there is then nothing to leave alone - and
			the file loses the node entirely rather than keeping a set
			nothing reads.
		*/
		static AssemblyState photograph(QETProject *project,
						AssemblyStage stage);

	private:
		void apply(const AssemblyState &state);

	private:
		QPointer<QETProject> m_project;
		AssemblyState m_old_state;
		AssemblyState m_new_state;
};

#endif // ASSEMBLYSTATECOMMAND_H
