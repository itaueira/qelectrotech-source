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
#ifndef EDITOPTIONTREECOMMAND_H
#define EDITOPTIONTREECOMMAND_H

#include "../options/optiontree.h"

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUndoCommand>

class QETProject;

/**
	@brief The EditOptionTreeCommand class
	Replace the option tree of the project, undoably.

	Both trees are kept whole and swapped, never recomputed, the way
	EditLocationTreeCommand does with the tree of locations and for the
	same reason: the edit is on the tree and not on one node. Creating an
	option moves nothing else, but nesting one carries a whole branch and
	deleting one takes its branch away, and a command that remembered only
	the difference would have to rebuild those - which is how an undo
	stack starts telling a story the project never lived.

	Keeping the tree whole is also what protects the one state this
	feature exists for. A sub-option can be switched on while the option
	above it is off: the drawing does not show it, and the choice is still
	there - OptionTree::isSwitchedOn against OptionTree::isActive. Undo
	and redo put the tree back field for field, so that choice survives
	them, and it survives the deletion of the branch it stands in too.
	A command that replayed the edit instead of restoring the tree would
	quietly flatten the stored choice into the effective one, and nothing
	would complain until the person turned the parent back on and found
	the brake gone.

	The five things a person does to an option - create it, rename it,
	nest it elsewhere, delete it, switch it - come in through the static
	makers below and not through the constructor. They are the ones that
	hand back nullptr when the tree did not move, which is how an
	operation that changes nothing is kept out of the undo stack: a
	Ctrl+Z that undoes nothing is worse than no entry at all, because the
	person counts the steps back and lands one short.

	Refused and unchanged are both nullptr, and error tells them apart,
	exactly as OptionTree::update tells them apart: filled, the edit was
	refused and the panel has something to say; empty, the edit was the
	state already there and the panel says nothing - the same guard that
	keeps a dialogue opened and closed from marking a project modified.

	What an option is worth - the difference it makes to the drawing - is
	not carried here, because it does not exist yet. That does not make
	this command provisional: what it stores is the tree and what is
	switched on, never their effect, so the day differences arrive they
	come in as a child command and this one keeps meaning what it means.

	No mergeWith. Switching an option on and off is what the person is
	there to try, and each try is a step they should be able to walk back
	over one at a time.
*/
class EditOptionTreeCommand : public QUndoCommand
{
	public:
		EditOptionTreeCommand(QETProject *project,
				      const OptionTree &tree,
				      const QString &label,
				      QUndoCommand *parent = nullptr);

		/**
			@brief Put a new option in the tree.
			@param project the project owning the tree
			@param option the option as the panel filled it in, its
			parent_uuid saying what it refines
			@param created_uuid filled with the uuid the tree gave it
			@param error filled with why it was refused
			@return the command to push, nullptr when nothing was
			created
		*/
		static EditOptionTreeCommand *createOption(
				QETProject *project,
				const ProjectOption &option,
				QString *created_uuid = nullptr,
				QString *error = nullptr);

		/**
			@brief Write an option back, however the panel changed it.
			@param project the project owning the tree
			@param option the option as it should now be, matched by
			uuid
			@param error filled with why nothing was written
			@return the command to push, nullptr when the tree did not
			move

			One door for renaming, describing and nesting, because
			OptionTree::update is one door and because a panel that
			edits name and description together should leave one step
			in the stack and not two. The caption says which of them
			happened when one of them did.
		*/
		static EditOptionTreeCommand *editOption(
				QETProject *project,
				const ProjectOption &option,
				QString *error = nullptr);

		/**
			@brief Take an option, and everything under it, out.
			@param project the project owning the tree
			@param uuid what to remove
			@param removed filled with the uuid of every option that
			went, the branch included
			@param error filled when there was no such option
			@return the command to push, nullptr when nothing was
			removed

			The caller is handed the list because it is the one that
			will have to answer for what was recorded against those
			uuids, once there is anything to record. Undoing brings
			the branch back as it stood, switches included.
		*/
		static EditOptionTreeCommand *removeOption(
				QETProject *project,
				const QString &uuid,
				QStringList *removed = nullptr,
				QString *error = nullptr);

		/**
			@brief Turn one option on or off.
			@param project the project owning the tree
			@param uuid the option
			@param on what it should now be
			@param error filled when there was no such option
			@return the command to push, nullptr when it was already
			that way

			Its sub-options are not touched, which is the whole point:
			switching a parent off takes its branch off the drawing
			and leaves every choice made inside it exactly where it
			was.
		*/
		static EditOptionTreeCommand *switchOption(
				QETProject *project,
				const QString &uuid,
				bool on,
				QString *error = nullptr);

		void undo() override;
		void redo() override;

		/**
			@return true when this command would put back the tree it
			already found

			The makers never build one of these; it is here for the
			caller that builds the command itself, so that it can ask
			before pushing rather than leave a step that steps
			nowhere.
		*/
		bool isNull() const;

	private:
		static QString editLabel(const OptionTree &tree,
					 const ProjectOption &before,
					 const ProjectOption &after);
		void apply(const OptionTree &tree);

	private:
		QPointer<QETProject> m_project;
		OptionTree m_old_tree;
		OptionTree m_new_tree;
};

#endif // EDITOPTIONTREECOMMAND_H
