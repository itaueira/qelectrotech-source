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
#include "renumbercommand.h"

#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QObject>

/**
	@brief RenumberCommand::RenumberCommand
	@param elements : the components the plan is about
	@param plan : what to write on each of them
	@param parent
*/
RenumberCommand::RenumberCommand(const QList<Element *> &elements,
				 const RenumberPlan &plan,
				 QUndoCommand *parent) :
	QUndoCommand(parent)
{
	for (Element *element : elements)
	{
		if (!element) {
			continue;
		}

		const QString uuid = element->uuid().toString();
		const QString label = plan.labelFor(uuid);
		if (label.isEmpty()) {
			continue;
		}

		DiagramContext information = element->elementInformations();
		if (information.value(QStringLiteral("label")).toString() == label) {
			continue;    // nothing to do for this one
		}

		Change change;
		change.element = QPointer<Element>(element);
		change.old_information = information;
		information.addValue(QStringLiteral("label"), label);
		change.new_information = information;
		m_changes.append(change);
	}

	setText(QObject::tr("Renuméroter %n composant(s)", "", m_changes.size()));
}

/**
	@brief RenumberCommand::changeCount
	@return how many components the command changes
*/
int RenumberCommand::changeCount() const
{
	return m_changes.size();
}

/**
	@brief RenumberCommand::undo
*/
void RenumberCommand::undo()
{
	apply(false);
}

/**
	@brief RenumberCommand::redo
*/
void RenumberCommand::redo()
{
	apply(true);
}

/**
	@brief RenumberCommand::apply
	@param forward
*/
void RenumberCommand::apply(bool forward)
{
	for (const Change &change : m_changes)
	{
		Element *element = change.element.data();
		if (!element) {
			continue;
		}
		element->setElementInformations(forward ? change.new_information
							: change.old_information);
	}
	updateProjectDataBase();
}

/**
	@brief RenumberCommand::updateProjectDataBase
*/
void RenumberCommand::updateProjectDataBase()
{
	QList<Element *> elements;
	for (const Change &change : m_changes)
	{
		if (change.element) {
			elements.append(change.element.data());
		}
	}
	if (elements.isEmpty()) {
		return;
	}

	Element *first = elements.first();
	if (first->diagram() && first->diagram()->project()
	    && first->diagram()->project()->dataBase())
	{
		first->diagram()->project()->dataBase()->elementInfoChanged(elements);
	}
}
