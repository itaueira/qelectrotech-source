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
#ifndef SYMBOLGROUPDIALOG_H
#define SYMBOLGROUPDIALOG_H

#include <QDialog>

#include "../symbolgroup.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;

/**
	@brief Files a piece of schematic in the library, and takes one back out.

	Two jobs in one dialog because they are two ends of the same idea and
	share the whole of the display: the list of what is filed, and what each
	one carries. Splitting them would mean writing the same list twice.

	What each grouping carries is shown before anything happens — how many
	components, how many conductors, and which catalog parts come with it.
	That last line is the reason to use a grouping at all, so it is not
	hidden behind a hover.
*/
class SymbolGroupDialog : public QDialog
{
	Q_OBJECT

	public:
		enum Mode
		{
			Save,   ///< file the selection
			Insert  ///< take one back out
		};

		SymbolGroupDialog(Mode mode,
				  const QDomDocument &fragment,
				  QWidget *parent = nullptr);

		/// the grouping the user picked, null when none
		SymbolGroup chosenGroup() const;
		/// the file written, empty when nothing was written
		QString savedPath() const;

		/**
			@brief File @a fragment as a grouping.
			@return the path written, empty when cancelled
		*/
		static QString saveSelection(const QDomDocument &fragment,
					     QWidget *parent = nullptr);
		/**
			@brief Pick a grouping to insert.
			@return the grouping, null when cancelled
		*/
		static SymbolGroup chooseGroup(QWidget *parent = nullptr);

	private slots:
		void selectionChanged();
		void nameChanged();
		void removeSelected();
		void act();

	private:
		void setUpWidget();
		void reload();
		/// the folder groupings live in, created when missing
		static QString folder();

		Mode m_mode = Save;
		QDomDocument m_fragment;
		SymbolGroup m_chosen;
		QString m_saved_path;
		QList<SymbolGroup> m_groups;

		QLineEdit *m_name = nullptr;
		QTextEdit *m_description = nullptr;
		QListWidget *m_list = nullptr;
		QLabel *m_content = nullptr;
		QPushButton *m_remove = nullptr;
		QPushButton *m_act = nullptr;
};

#endif // SYMBOLGROUPDIALOG_H
