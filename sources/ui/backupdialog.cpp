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

#include "backupdialog.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

/**
	@brief BackupDialog::BackupDialog
	@param parent parent widget
*/
BackupDialog::BackupDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Créer une copie de sauvegarde ?", "window title"));
		//Not a fixed height any more: the check box below has to fit, and a
		//fixed height clips whatever the translation of it happens to need.
	setMinimumWidth(450);

	auto main_layout = new QVBoxLayout(this);

	auto label = new QLabel(
		tr("Souhaitez-vous créer une copie de sauvegarde ?",
		   "dialog message"));
	label->setWordWrap(true);
	main_layout->addWidget(label);

		//The box does not answer the question, it says the answer is to be
		//kept - and the answer clicked right after is what says whether the
		//copy is made from now on or never is. Left unticked, nothing is
		//stored and the question comes back next time, which is the state a
		//fresh profile is in.
	m_remember_cb = new QCheckBox(
		tr("Do not ask again, and keep this answer",
		   "check box, backup copy question"));
	m_remember_cb->setToolTip(
		tr("The choice is kept for this workstation and can be changed in "
		   "Settings > General > Projects. It is never written into the "
		   "project file.",
		   "check box tool tip, backup copy question"));
	main_layout->addWidget(m_remember_cb);

	main_layout->addStretch();

	auto button_layout = new QHBoxLayout();
	button_layout->addStretch();

	auto yes_button = new QPushButton(tr("Oui", "yes button"));
	auto no_button = new QPushButton(tr("Non", "no button"));

	button_layout->addWidget(yes_button);
	button_layout->addWidget(no_button);
	main_layout->addLayout(button_layout);

	connect(yes_button, &QPushButton::clicked, this, &QDialog::accept);
	connect(no_button, &QPushButton::clicked, this, &QDialog::reject);
}

/**
	@brief BackupDialog::rememberChoice
	@return true when the user asked for the answer he is giving to be kept.
	What that answer then means for the stored preference is
	AppPreferences::policyForAnswer(), and not this class: the dialogue
	reports, it does not decide.
*/
bool BackupDialog::rememberChoice() const
{
	return m_remember_cb && m_remember_cb->isChecked();
}

/**
	@brief BackupDialog::~BackupDialog
*/
BackupDialog::~BackupDialog() = default;
