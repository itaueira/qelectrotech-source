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

#include "conductorpropertiesdialog.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetproject.h"
#include "../undocommand/groupedupdatecommand.h"
#include "conductorpropertieswidget.h"
#include "ui_conductorpropertiesdialog.h"

#include <QCheckBox>

#include "../qet.h"
/**
	@brief ConductorPropertiesDialog::ConductorPropertiesDialog
	Constructor
	@param conductor : conductor to edit properties
	@param parent : parent widget
*/
ConductorPropertiesDialog::ConductorPropertiesDialog(
		Conductor *conductor, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ConductorPropertiesDialog)
{
	ui->setupUi(this);
	QET::trackDialogGeometry(this);
	m_cpw = new ConductorPropertiesWidget(conductor->properties());
	m_cpw -> setHiddenOneTextPerFolio(true);
	m_cpw->setHiddenAvailableAutonum(true);
	m_cpw->setIgnoreBoxShown(true);
	m_cpw->setIgnoredInNumbering(conductor->isFreezeLabel() ? Qt::Checked : Qt::Unchecked);
	if (conductor -> diagram() -> defaultConductorProperties.m_one_text_per_folio == true &&
		conductor -> relatedPotentialConductors().size()) {
		m_cpw->setDisabledShowText();
	}
	ui -> main_layout -> insertWidget(0, m_cpw);
}

/**
	@brief ConductorPropertiesDialog::~ConductorPropertiesDialog
*/
ConductorPropertiesDialog::~ConductorPropertiesDialog()
{
	delete ui;
}

/**
	@brief ConductorPropertiesDialog::PropertiesDialog
	Static method for open and apply properties.
	@param conductor : conductor to edit properties
	@param parent : parent widget
*/
void ConductorPropertiesDialog::PropertiesDialog(Conductor *conductor,
						 QWidget *parent)
{
	ConductorPropertiesDialog cpd (conductor, parent);

	if (cpd.exec() == QDialog::Rejected) return;
	const bool ignore_changed = cpd.ignoredInNumbering() != conductor->isFreezeLabel();
	const bool properties_changed = !(cpd.properties() == conductor->properties());
	if (!ignore_changed && !properties_changed) return;

		//Left out of the numbering, or back in it (T77), before the
		//properties: a text typed together with the tick is kept, not worked
		//out again from the formula
	QUndoStack &stack = conductor->diagram()->undoStack();
	if (ignore_changed)
	{
		auto *ignore = new QPropertyUndoCommand(conductor, "freezeLabel",
							conductor->isFreezeLabel(),
							cpd.ignoredInNumbering());
		ignore->setText(cpd.ignoredInNumbering() ? tr("Leave out of automatic numbering")
							 : tr("Bring back into automatic numbering"));
		if (!properties_changed) {
			stack.push(ignore);
			return;
		}
		stack.beginMacro(tr("Edit conductor properties", "undo caption"));
		stack.push(ignore);
	}

	QVariant old_value, new_value;
	old_value.setValue(conductor->properties());
	new_value.setValue(cpd.properties());

		//Asked once and kept: the walk of the potential is not free, and it
		//used to be made twice here for the same answer.
	const QSet<Conductor *> potential = conductor->relatedPotentialConductors();
	QUndoCommand *undo = nullptr;

	if (!potential.isEmpty() && cpd.applyAll())
	{
			//The whole potential in one gesture, so one notice to the data
			//base rather than one per conductor. Only this branch is grouped:
			//a single conductor is a single row and has nothing to group, and
			//a bare QPropertyUndoCommand still merges with the next edit of
			//the same property -- a wrapper around it would quietly stop that.
		auto *group = new GroupedUpdateCommand(
				conductor->diagram() ? conductor->diagram()->project() : nullptr,
				tr("Edit the properties of several conductors", "undo caption"));
		new QPropertyUndoCommand(conductor, "properties", old_value, new_value, group);

		for (Conductor *potential_conductor : potential)
		{
				//A wire left out of the numbering keeps its own (T77)
			if (potential_conductor->isFreezeLabel())
				continue;
			old_value.setValue(potential_conductor->properties());
			new QPropertyUndoCommand (potential_conductor, "properties", old_value, new_value, group);
		}
		undo = group;
	}
	else
	{
		auto *single = new QPropertyUndoCommand(conductor, "properties", old_value, new_value);
		single->setText(tr("Edit conductor properties", "undo caption"));
		undo = single;
	}

		//A function separated terminal element (element information
		//potential_isolating) keeps the two sides of the terminal apart:
		//every property but one stays local to the side it was edited on.
		//The voltage / protocol still crosses, because it describes the
		//electrical node and not one side of it.
	if (cpd.applyAll())
	{
			//Only an edit that really changed the voltage / protocol passes
			//it on, so the comparison is the edit itself -- what the dialog
			//was filled with, against what the user left in it -- and never
			//the far wire's own value, which differs from it just as often
			//as not (a 24 V side against a 0 V one, say) and would then be
			//overwritten by every edit, voltage included.
		const ConductorProperties shown = conductor->properties();
		foreach (Conductor *separated_conductor, conductor->functionSeparatedConductors())
		{
			ConductorProperties separated_properties = separated_conductor->properties();
			separated_properties.applyTensionProtocolChange(
						shown,
						cpd.properties());
			if (separated_properties == separated_conductor->properties())
				continue;

			QVariant separated_old_value, separated_new_value;
			separated_old_value.setValue(separated_conductor->properties());
			separated_new_value.setValue(separated_properties);
			new QPropertyUndoCommand (separated_conductor, "properties",
						  separated_old_value, separated_new_value, undo);
		}
	}

	stack.push(undo);
	if (ignore_changed) {
		stack.endMacro();
	}
}

/**
	@brief ConductorPropertiesDialog::ignoredInNumbering
	@return whether the box "Leave out of automatic numbering" is ticked
*/
bool ConductorPropertiesDialog::ignoredInNumbering() const
{
	return m_cpw->ignoreBox()->isChecked();
}

/**
	@brief ConductorPropertiesDialog::properties
	@return the edited properties
*/
ConductorProperties ConductorPropertiesDialog::properties() const
{
	return m_cpw -> properties();
}

/**
	@brief ConductorPropertiesDialog::applyAll
	@return
	true -> must apply properties to all conductors at the same potential
	false -> must apply properties only for the edited conductor
*/
bool ConductorPropertiesDialog::applyAll() const
{
	return ui -> m_apply_all_cb -> isChecked();
}
