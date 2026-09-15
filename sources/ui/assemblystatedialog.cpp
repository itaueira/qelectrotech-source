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

#include "../qetproject.h"
#include "../undocommand/assemblystatecommand.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
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
	layout->addWidget(box);

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
			//Two sentences and not one, because the two halves are not
			//equally true today: the renumbering of the components really
			//does read this photograph, and nothing reads the conductor
			//half of it yet.
		const QString components = tr("%n composant(s)", "",
					      photographedComponents());
		const QString conductors = tr("%n conducteur(s)", "",
					      photographedConductors());

		m_effect->setText(
			tr("Confirmer fige le repère de %1 : la renumérotation "
			   "automatique les saute et n'offre plus leur repère à un "
			   "composant dessiné après.\n\n"
			   "Le texte de %2 est également photographié, pour la "
			   "numérotation des potentiels — qui ne sait pas encore lire "
			   "cette photographie et continue donc de numéroter comme "
			   "aujourd'hui.")
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
