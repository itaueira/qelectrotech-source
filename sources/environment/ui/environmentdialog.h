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
#ifndef ENVIRONMENTDIALOG_H
#define ENVIRONMENTDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;

/**
	@brief The EnvironmentDialog class
	See and change the environment folder, and make a backup copy of it.

	The path is a visible, editable property because that is the whole point:
	sharing has to be pointing at a folder. Changing it needs a restart, and
	the dialog says so instead of leaving half the program looking at the old
	place.

	The backup is here and not only in a document because a procedure nobody
	can run from the program is a procedure that does not get run.
*/
class EnvironmentDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit EnvironmentDialog(QWidget *parent = nullptr);

		/// true when the user changed the path and the program has to restart
		bool pathChanged() const;

	private slots:
		void browse();
		void apply();
		void backup();
		void openInFileManager();

	private:
		void buildWidgets();
		void refreshContents();

	private:
		QLineEdit *m_path = nullptr;
		QPushButton *m_browse = nullptr;
		QPushButton *m_apply = nullptr;
		QPushButton *m_backup = nullptr;
		QPushButton *m_open = nullptr;
		QTextBrowser *m_contents = nullptr;
		QLabel *m_status = nullptr;
		bool m_path_changed = false;
};

#endif // ENVIRONMENTDIALOG_H
