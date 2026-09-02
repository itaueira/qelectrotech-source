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
#include "diagrampropertiesdialog.h"

#include "../diagram.h"
#include "../diagramcommands.h"
#include "../undocommand/changetitleblockcommand.h"
#include "borderpropertieswidget.h"
#include "conductorpropertieswidget.h"
#include "projectpropertiesdialog.h"
#include "titleblockpropertieswidget.h"

#include <QCheckBox>
#include <QGroupBox>

/**
	@brief DiagramPropertiesDialog::DiagramPropertiesDialog
	Default constructor
	@param diagram : diagram to edit properties
	@param parent : parent widget
*/
DiagramPropertiesDialog::DiagramPropertiesDialog(Diagram *diagram, QWidget *parent) :
	QDialog (parent),
	m_diagram (diagram)
{
	bool diagram_is_read_only = diagram -> isReadOnly();

	// Get some properties of edited diagram
	TitleBlockProperties titleblock = diagram -> border_and_titleblock.exportTitleBlock();
	BorderProperties     border     = diagram -> border_and_titleblock.exportBorder();
	ConductorProperties  conductors = diagram -> defaultConductorProperties;

	setWindowModality(Qt::WindowModal);
#ifdef Q_OS_MACOS
	setWindowFlags(Qt::Sheet);
#endif

	setWindowTitle(tr("Propriétés du folio", "window title"));

	//Border widget
	BorderPropertiesWidget *border_infos = new BorderPropertiesWidget(border, this);
	border_infos -> setReadOnly(diagram_is_read_only);

	//Title block widget
	TitleBlockPropertiesWidget  *titleblock_infos;

	if (QETProject *parent_project = diagram -> project())
		titleblock_infos = new TitleBlockPropertiesWidget(parent_project -> embeddedTitleBlockTemplatesCollection(), titleblock, false, diagram->project(), this);
	else
		titleblock_infos = new TitleBlockPropertiesWidget(titleblock, false, diagram->project(), this);

	titleblock_infos -> setReadOnly(diagram_is_read_only);
	connect(titleblock_infos, &TitleBlockPropertiesWidget::openAutoNumFolioEditor, this, &DiagramPropertiesDialog::editAutoFolioNum);
	//titleblock_infos->setMinimumSize(590,480); //Minimum Size needed for correct display

		//Conductor widget
	m_cpw = new ConductorPropertiesWidget(conductors, this);
	m_cpw -> setReadOnly(diagram_is_read_only);

	QComboBox *autonum_combobox = m_cpw->autonumComboBox();
	autonum_combobox->addItems(diagram->project()->conductorAutoNum().keys());
	autonum_combobox->setCurrentIndex(autonum_combobox->findText(diagram->conductorsAutonumName()));

	connect(m_cpw->editAutonumPushButton(), &QPushButton::clicked, this, &DiagramPropertiesDialog::editAutonum);

		//Display options of this folio.
		//
		//This dialog had no such box: everything in it until now described
		//what the folio *is* - its border, its title block, the default
		//properties a new conductor inherits - and nothing described how the
		//folio is drawn. So the box is opened here rather than a checkbox
		//being squeezed in beside the border sizes, where it would read as a
		//property of the border.
		//
		//It deliberately does not go in the Display menu of the editor. That
		//menu holds application keys - the grid, the terminals, the empty
		//fields, the guides - each of them one answer for every project the
		//person opens. This one is stored per folio, and an entry there would
		//tick and untick with whichever folio happened to be in front,
		//claiming a reach it does not have.
	QGroupBox *display_options = new QGroupBox(tr("Options d'affichage"), this);

	QCheckBox *dash_external_wires_cb = new QCheckBox(
			tr("Représenter en pointillés les fils qui sortent de la localisation"),
			display_options);
	dash_external_wires_cb->setToolTip(
			tr("Un fil est représenté en pointillés lorsque ses deux "
			   "extrémités sont dans des localisations différentes et "
			   "qu'aucune ne contient l'autre.\n"
			   "Ce réglage est propre à ce folio et il est enregistré "
			   "dans le projet."));
	dash_external_wires_cb->setChecked(diagram->dashExternalWires());
	dash_external_wires_cb->setEnabled(!diagram_is_read_only);

	QVBoxLayout *display_options_layout = new QVBoxLayout(display_options);
	display_options_layout->addWidget(dash_external_wires_cb);

		// Buttons
	QDialogButtonBox boutons(diagram_is_read_only ? QDialogButtonBox::Ok : QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(&boutons, &QDialogButtonBox::accepted, this, &DiagramPropertiesDialog::accept);
	connect(&boutons, &QDialogButtonBox::rejected, this, &DiagramPropertiesDialog::reject);

	QGridLayout *glayout = new QGridLayout;
	glayout->addWidget(border_infos,0,0);
	glayout->addWidget(titleblock_infos, 1, 0);
	glayout->addWidget(m_cpw, 0, 1, 0, 1);
	glayout->addWidget(display_options, 2, 0);

	QVBoxLayout vlayout(this);
	vlayout.addLayout(glayout);
	vlayout.addWidget(&boutons);

	// if dialog is accepted
	if (this -> exec() == QDialog::Accepted && !diagram_is_read_only)
	{
		TitleBlockProperties new_titleblock = titleblock_infos  -> properties();
		BorderProperties     new_border     = border_infos -> properties();
		ConductorProperties  new_conductors = m_cpw -> properties();

		// Title block have change
		if (new_titleblock != titleblock) {
			diagram -> undoStack().push(new ChangeTitleBlockCommand(diagram, titleblock, new_titleblock));
		}

		// Border have change
		if (new_border != border) {
			diagram -> undoStack().push(new ChangeBorderCommand(diagram, border, new_border));
		}

		// Conducteur have change
		if (new_conductors != conductors) {
#if TODO_LIST
#pragma message("@TODO implement an undo command to allow the user to undo/redo this action")
#endif
			/// TODO implement an undo command to allow the user to undo/redo this action
			diagram -> defaultConductorProperties = new_conductors;
		}

			// Conductor autonum name
		if (autonum_combobox->currentText() != diagram->conductorsAutonumName())
		{
			diagram->setConductorsAutonumName (autonum_combobox->currentText());
			diagram->project()->conductorAutoNumChanged();
		}

			//Dashed external wires. Applied straight, with no undo command,
			//on the local convention of the default conductor properties
			//above: one bool on one folio, and a command for it would be the
			//only one of its kind in this dialog.
			//
			//The setter is the one that marks the project modified and
			//repaints the conductors of the folio, so that a caller cannot
			//tick the box and lose the answer on closing.
		if (dash_external_wires_cb->isChecked() != diagram->dashExternalWires())
		{
#if TODO_LIST
#pragma message("@TODO implement an undo command to allow the user to undo/redo this action")
#endif
			/// TODO implement an undo command to allow the user to undo/redo this action
			diagram->setDashExternalWires(dash_external_wires_cb->isChecked());
		}
	}
}

/**
	@brief DiagramPropertiesDialog::diagramPropertiesDialog
	Static method to get a DiagramPropertiesDialog.
	@param diagram : diagram to edit properties
	@param parent : parent widget
*/
void DiagramPropertiesDialog::diagramPropertiesDialog(Diagram *diagram, QWidget *parent) {
	DiagramPropertiesDialog dialog(diagram, parent);
}

/**
	@brief DiagramPropertiesDialog::editAutonum
	Open conductor autonum editor
*/
void DiagramPropertiesDialog::editAutonum()
{
	ProjectPropertiesDialog ppd (m_diagram->project(), this);
	ppd.setCurrentPage(ProjectPropertiesDialog::Autonum);
	ppd.exec();
	m_cpw->autonumComboBox()->clear();
	m_cpw->autonumComboBox()->addItems(m_diagram->project()->conductorAutoNum().keys());
}

/**
	@brief DiagramPropertiesDialog::editAutonum
	Open folio autonum editor
*/
void DiagramPropertiesDialog::editAutoFolioNum ()
{
	ProjectPropertiesDialog ppd (m_diagram->project(), this);
	ppd.setCurrentPage(ProjectPropertiesDialog::Autonum);
	ppd.changeToFolio();
	ppd.exec();
}
