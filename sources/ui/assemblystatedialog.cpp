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
#include "assemblystatedialog.h"

#include "assemblychangesdialog.h"
#include "../qetproject.h"
#include "../undocommand/assemblystatecommand.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QUndoStack>
#include <QVBoxLayout>

AssemblyStateDialog::AssemblyStateDialog(QETProject *project,
					 QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("État de montage du projet", "window title"));
	setMinimumWidth(560);
	setUpWidget();
	refreshPreview();
}

/**
	@brief AssemblyStateDialog::chosenStage
	@return the stage the list is on
*/
AssemblyStage AssemblyStateDialog::chosenStage() const
{
	if (!m_stage) {
		return AssemblyStage::InProject;
	}
	return AssemblyStage(m_stage->currentData().toInt());
}

/**
	@brief AssemblyStateDialog::setChosenStage
	@param stage
*/
void AssemblyStateDialog::setChosenStage(AssemblyStage stage)
{
	if (!m_stage) {
		return;
	}
	const int index = m_stage->findData(int(stage));
	if (index >= 0) {
		m_stage->setCurrentIndex(index);
	}
}

/**
	@brief AssemblyStateDialog::photographedComponents
	@return how many components confirming would freeze
*/
int AssemblyStateDialog::photographedComponents() const
{
	return m_preview.components.count();
}

/**
	@brief AssemblyStateDialog::photographedConductors
	@return how many conductors the photograph on offer records
*/
int AssemblyStateDialog::photographedConductors() const
{
	return m_preview.conductors.count();
}

/**
	@brief AssemblyStateDialog::changesReportAvailable
	@return whether the project, as it stands, has a marking to compare with
*/
bool AssemblyStateDialog::changesReportAvailable() const
{
	return m_project && m_project->assemblyState().isFrozen();
}

/**
	@brief AssemblyStateDialog::showChangesReport
	@return the list of changes now on screen, or nullptr
*/
AssemblyChangesDialog *AssemblyStateDialog::showChangesReport()
{
	if (!changesReportAvailable()) {
		return nullptr;
	}

		//Parented to whatever this window was opened from, and not to this
		//window, which is about to close: the list has to outlive it.
	AssemblyChangesDialog *report = new AssemblyChangesDialog(m_project,
								  parentWidget());
	report->setAttribute(Qt::WA_DeleteOnClose);
	report->show();

		//Out of the way, as Cancel: whatever stage the list was offering is
		//not applied. Looking at what changed is not a change of stage.
	reject();
	return report;
}

/**
	@brief AssemblyStateDialog::setUpWidget
*/
void AssemblyStateDialog::setUpWidget()
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	QLabel *explanation = new QLabel(
		tr("Une fois le châssis câblé, les repères sont imprimés et collés "
		   "sur les appareils : les renuméroter ne coûte pas un clic, cela "
		   "coûte une visite avec une étiqueteuse. Marquer le projet comme "
		   "monté prend une photographie de ce qui existe à cet instant, et "
		   "l'automatisation ne numérote plus que ce qui sera dessiné "
		   "après."), this);
	explanation->setWordWrap(true);
	layout->addWidget(explanation);

	m_current = new QLabel(this);
	m_current->setWordWrap(true);
	layout->addWidget(m_current);

	QFormLayout *form = new QFormLayout();
	m_stage = new QComboBox(this);
	for (AssemblyStage stage : {AssemblyStage::InProject,
				    AssemblyStage::Assembled,
				    AssemblyStage::InField}) {
		m_stage->addItem(AssemblyState::translatedStage(stage), int(stage));
	}
	form->addRow(tr("État du projet :"), m_stage);
	layout->addLayout(form);

	if (m_project) {
		setChosenStage(m_project->assemblyState().stage);
	}

	m_effect = new QLabel(this);
	m_effect->setWordWrap(true);
	m_effect->setTextFormat(Qt::PlainText);
	m_effect->setStyleSheet(QStringLiteral(
		"QLabel { background-color : palette(base); "
		"border : 1px solid palette(mid); padding : 8px; }"));
	layout->addWidget(m_effect);

	m_save_note = new QLabel(this);
	m_save_note->setWordWrap(true);
	layout->addWidget(m_save_note);

	QDialogButtonBox *box = new QDialogButtonBox(
				QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
		//In the same box, as an action: one box in the window, so that Ok
		//and Cancel are where they always are and are found the way they
		//always were. Never the default button - Enter confirms the stage,
		//it does not open a list.
	m_changes = box->addButton(tr("Changes since assembly…"),
				   QDialogButtonBox::ActionRole);
	m_changes->setAutoDefault(false);
	m_changes->setObjectName(QStringLiteral("changes_since_assembly"));
	m_changes->setToolTip(tr("What to mount, remove and relabel on the "
				 "panel since the project was marked."));
	layout->addWidget(box);

	connect(m_changes, &QPushButton::clicked,
		this, &AssemblyStateDialog::showChangesReport);
	connect(m_stage, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &AssemblyStateDialog::refreshPreview);
	connect(box, &QDialogButtonBox::accepted,
		this, &AssemblyStateDialog::apply);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief AssemblyStateDialog::refreshPreview
	Take the photograph the chosen stage would produce, and say what it holds.
*/
void AssemblyStateDialog::refreshPreview()
{
		//Taken here rather than at confirmation time, so that the number on
		//screen is the number that gets applied: two walks of the project
		//could count two different things if something moved in between, and
		//the reader would have no way of telling which one was true.
	m_preview = AssemblyStateCommand::photograph(m_project, chosenStage());

		//The project as it stands, not the stage on offer: the list
		//compares the drawing with the photograph already taken.
	if (m_changes) {
		m_changes->setEnabled(changesReportAvailable());
	}

	if (!m_project)
	{
		m_current->clear();
		m_effect->clear();
		m_save_note->clear();
		return;
	}

	const AssemblyState now = m_project->assemblyState();
	if (now.isFrozen()) {
		m_current->setText(
			tr("État actuel : %1, %n élément(s) figé(s).", "",
			   now.frozenCount())
			.arg(AssemblyState::translatedStage(now.stage)));
	} else {
		m_current->setText(tr("État actuel : %1, rien n'est figé.")
				   .arg(AssemblyState::translatedStage(now.stage)));
	}

	if (m_preview.isFrozen())
	{
			//One sentence for both halves, because both are read: the
			//renumbering of the components through
			//ProjectRenumberer::isFrozen(), and the wire numbering through
			//ConductorAutoNumerotation::isFrozen() - when a wire is drawn and
			//when two folio reports are linked.
		const QString components = tr("%n component(s)", "",
					      photographedComponents());
		const QString conductors = tr("%n conductor(s)", "",
					      photographedConductors());

		m_effect->setText(
			tr("Confirming freezes the tags of %1 and the numbers of %2: "
			   "the automatic numbering leaves them as they are, and "
			   "numbers only what is drawn afterwards.")
			.arg(components, conductors));
	}
	else
	{
		m_effect->setText(
			tr("Le projet revient en étude : la photographie est effacée et "
			   "l'automatisation retrouve l'ensemble du dessin.\n\n"
			   "Les verrous posés à la main sur un composant ne sont pas "
			   "touchés — ils n'ont jamais fait partie de la "
			   "photographie, et survivent donc au retour en étude."));
	}

	if (m_project->projectWasModified()) {
		m_save_note->setText(
			tr("Ce projet a des modifications non enregistrées : comme tout "
			   "le reste, la marque ne sera dans le fichier qu'après "
			   "l'enregistrement."));
	} else {
		m_save_note->clear();
	}
}

/**
	@brief AssemblyStateDialog::apply
	Push the change on the undo stack of the project, and close.
*/
void AssemblyStateDialog::apply()
{
	if (!m_project) {
		reject();
		return;
	}

	AssemblyStateCommand *command =
			new AssemblyStateCommand(m_project, m_preview);

	if (!command->changesAnything())
	{
			//Confirming without having changed anything is not an error, but
			//it has no business filling the undo menu with a step that undoes
			//nothing.
		delete command;
		accept();
		return;
	}

	m_project->undoStack()->push(command);
	accept();
}
