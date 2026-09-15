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
#include "editoptiontreecommand.h"

#include "../options/projectoption.h"
#include "../qetproject.h"

#include <QObject>

/**
	@brief EditOptionTreeCommand::EditOptionTreeCommand
	@param project the project owning the tree
	@param tree the tree as the edit left it
	@param label how the caption calls what happened
	@param parent parent undo command
*/
EditOptionTreeCommand::EditOptionTreeCommand(QETProject *project,
					     const OptionTree &tree,
					     const QString &label,
					     QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project)
{
	if (m_project) {
		m_old_tree = m_project.data()->optionTree();
	}
	m_new_tree = tree;

	setText(label.isEmpty()
		? QObject::tr("Modifier les options du projet")
		: label);
}

/**
	@brief EditOptionTreeCommand::createOption
	@param project the project owning the tree
	@param option the option as the panel filled it in
	@param created_uuid filled with the uuid the tree gave it
	@param error filled with why it was refused
	@return the command to push, nullptr when nothing was created
*/
EditOptionTreeCommand *EditOptionTreeCommand::createOption(
		QETProject *project,
		const ProjectOption &option,
		QString *created_uuid,
		QString *error)
{
	if (created_uuid) {
		created_uuid->clear();
	}
	if (error) {
		error->clear();
	}
	if (!project) {
		return nullptr;
	}

	OptionTree tree = project->optionTree();
	const QString uuid = tree.append(option, error);
	if (uuid.isEmpty()) {
		return nullptr;
	}
	if (created_uuid) {
		*created_uuid = uuid;
	}

		//The name is read back from the tree and not from the option
		//handed in, because append folds the spaces a person leaves
		//behind - and a caption that quoted the raw typing would not
		//be quoting what the tree now shows.
	const ProjectOption added = tree.option(uuid);
	const QString parent_name = tree.option(added.parent_uuid).name;

	return new EditOptionTreeCommand(
			project, tree,
			parent_name.isEmpty()
				? QObject::tr("Créer l'option « %1 »")
					.arg(added.name)
				: QObject::tr("Créer l'option « %1 » dans "
					      "« %2 »")
					.arg(added.name, parent_name));
}

/**
	@brief EditOptionTreeCommand::editOption
	@param project the project owning the tree
	@param option the option as it should now be, matched by uuid
	@param error filled with why nothing was written
	@return the command to push, nullptr when the tree did not move

	Both ways out are nullptr, and error tells them apart the way
	OptionTree::update tells them apart: filled, refused; empty, the
	option handed in was the one already there.
*/
EditOptionTreeCommand *EditOptionTreeCommand::editOption(
		QETProject *project,
		const ProjectOption &option,
		QString *error)
{
	if (error) {
		error->clear();
	}
	if (!project) {
		return nullptr;
	}

	OptionTree tree = project->optionTree();
	const ProjectOption before = tree.option(option.uuid);
	if (!tree.update(option, error)) {
		return nullptr;
	}

	return new EditOptionTreeCommand(
			project, tree,
			editLabel(tree, before, tree.option(option.uuid)));
}

/**
	@brief EditOptionTreeCommand::removeOption
	@param project the project owning the tree
	@param uuid what to remove
	@param removed filled with the uuid of every option that went
	@param error filled when there was no such option
	@return the command to push, nullptr when nothing was removed
*/
EditOptionTreeCommand *EditOptionTreeCommand::removeOption(
		QETProject *project,
		const QString &uuid,
		QStringList *removed,
		QString *error)
{
	if (removed) {
		removed->clear();
	}
	if (error) {
		error->clear();
	}
	if (!project) {
		return nullptr;
	}

	OptionTree tree = project->optionTree();
	const QString name = tree.option(uuid).name;

	QStringList going;
	if (!tree.remove(uuid, &going))
	{
			//Not the harmless kind of nothing-happened: the panel is
			//showing an option the project no longer has, and saying
			//so is better than a button that does nothing.
		if (error) {
			*error = QObject::tr("Cette option n'est plus dans le "
					     "projet.");
		}
		return nullptr;
	}
	if (removed) {
		*removed = going;
	}

	return new EditOptionTreeCommand(
			project, tree,
			QObject::tr("Supprimer l'option « %1 »").arg(name));
}

/**
	@brief EditOptionTreeCommand::switchOption
	@param project the project owning the tree
	@param uuid the option
	@param on what it should now be
	@param error filled when there was no such option
	@return the command to push, nullptr when it was already that way
*/
EditOptionTreeCommand *EditOptionTreeCommand::switchOption(
		QETProject *project,
		const QString &uuid,
		bool on,
		QString *error)
{
	if (error) {
		error->clear();
	}
	if (!project) {
		return nullptr;
	}

	OptionTree tree = project->optionTree();
	if (tree.indexOfUuid(uuid) < 0)
	{
		if (error) {
			*error = QObject::tr("Cette option n'est plus dans le "
					     "projet.");
		}
		return nullptr;
	}

		//Asked for the state it is already in. Nothing is refused and
		//nothing is wrong, so error stays empty and no step is made:
		//this is the case a checkbox reached by keyboard produces all
		//day long.
	if (!tree.setSwitchedOn(uuid, on)) {
		return nullptr;
	}

	const QString name = tree.option(uuid).name;
	return new EditOptionTreeCommand(
			project, tree,
			on ? QObject::tr("Cocher l'option « %1 »").arg(name)
			   : QObject::tr("Décocher l'option « %1 »").arg(name));
}

/**
	@brief EditOptionTreeCommand::editLabel
	@param tree the tree as the edit left it, to read the new parent by name
	@param before the option as it stood
	@param after the option as the tree now holds it
	@return how the caption should call what happened

	Cocher and décocher, and not activer and désactiver: what the person
	did is tick a box, and whether the option applies is another question
	the tree answers on its own - see OptionTree::isActive. Ticking a
	sub-option whose parent is off changes nothing on the drawing, and a
	caption promising activation would be a caption the drawing does not
	honour.
*/
QString EditOptionTreeCommand::editLabel(const OptionTree &tree,
					 const ProjectOption &before,
					 const ProjectOption &after)
{
	const bool renamed   = before.name        != after.name;
	const bool nested    = before.parent_uuid != after.parent_uuid;
	const bool described = before.description != after.description;
	const bool switched  = before.switched_on != after.switched_on;

	const int changes = int(renamed) + int(nested)
			    + int(described) + int(switched);

		//Two fields at once, or none that this caption knows how to
		//name: say what is true of all of them rather than pick one and
		//hide the rest.
	if (changes != 1) {
		return QObject::tr("Modifier l'option « %1 »").arg(after.name);
	}

	if (renamed)
	{
		return QObject::tr("Renommer l'option « %1 » en « %2 »")
				.arg(before.name, after.name);
	}

	if (nested)
	{
		return after.parent_uuid.isEmpty()
			? QObject::tr("Déplacer l'option « %1 » au premier "
				      "niveau").arg(after.name)
			: QObject::tr("Déplacer l'option « %1 » dans « %2 »")
				.arg(after.name,
				     tree.option(after.parent_uuid).name);
	}

	if (described)
	{
		return QObject::tr("Modifier la description de l'option "
				   "« %1 »").arg(after.name);
	}

	return after.switched_on
		? QObject::tr("Cocher l'option « %1 »").arg(after.name)
		: QObject::tr("Décocher l'option « %1 »").arg(after.name);
}

/**
	@brief EditOptionTreeCommand::isNull
	@return true when both trees are the same one
*/
bool EditOptionTreeCommand::isNull() const
{
	return m_old_tree == m_new_tree;
}

/**
	@brief EditOptionTreeCommand::undo
	The tree goes back here; children, the day a difference becomes one,
	are walked by the base class afterwards - while the tree they were
	recorded against is the one they are being put back into.
*/
void EditOptionTreeCommand::undo()
{
	apply(m_old_tree);
	QUndoCommand::undo();
}

/**
	@brief EditOptionTreeCommand::redo
*/
void EditOptionTreeCommand::redo()
{
	QUndoCommand::redo();
	apply(m_new_tree);
}

/**
	@brief EditOptionTreeCommand::apply
	@param tree the state to put on the project

	Whole, never patched. QETProject::setOptionTree compares before it
	writes, so putting back a tree that is already there marks nothing.
*/
void EditOptionTreeCommand::apply(const OptionTree &tree)
{
	if (m_project) {
		m_project.data()->setOptionTree(tree);
	}
}
