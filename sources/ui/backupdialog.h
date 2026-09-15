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

#ifndef BACKUPDIALOG_H
#define BACKUPDIALOG_H

#include <QDialog>

class QCheckBox;

/**
	The question asked when a project is opened: make a backup copy of the
	file or not.

	It carries a "do not ask again" box, and that box is only half of an
	answer. What should happen in place of the question is the other half,
	and it comes from the answer being given at the same moment - which is
	why the box cannot store a preference by itself. AppPreferences holds the
	three states and the rule that puts the two halves together; this
	dialogue only says whether the box was ticked.

	@sa AppPreferences::BackupPolicy, AppPreferences::policyForAnswer
*/
class BackupDialog : public QDialog
{
	Q_OBJECT
	public:
		explicit BackupDialog(QWidget *parent = nullptr);
		~BackupDialog() override;

		bool rememberChoice() const;

	private:
		QCheckBox *m_remember_cb = nullptr;
};

#endif // BACKUPDIALOG_H
