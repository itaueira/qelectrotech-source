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
#ifndef CREATESYMBOLDIALOG_H
#define CREATESYMBOLDIALOG_H

#include <QDialog>

#include "../symbolbuilder.h"

class Catalog;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;
class SymbolPreview;

/**
	@brief Turns what was drawn on the sheet into a symbol of the library.

	Everything the symbol needs that the drawing cannot say: the name, the
	class it belongs to, the folder it goes in, the provisional label of each
	connection point and what each pair of them is — a NO contact, a NC
	contact, a power pole.

	The table of connection points is the centre of the dialog rather than a
	tab hidden behind the name, because it is the part that decides whether
	the symbol works. It arrives filled with what the drawing suggested, and
	says out loud when a point had to be pushed onto the grid to make it.

	Saving over a symbol that already exists asks the question of T12 in the
	same words: change it for every project that uses it, or leave those
	alone and file a new revision.
*/
class CreateSymbolDialog : public QDialog
{
	Q_OBJECT

	public:
		/**
			@param symbol what was read off the sheet, already carrying the
			connection points the drawing suggested
			@param catalog the classes to choose from, may be nullptr
		*/
		CreateSymbolDialog(const SymbolDefinition &symbol,
				   Catalog *catalog,
				   QWidget *parent = nullptr);

		/// the symbol as the dialog left it
		SymbolDefinition symbol() const;
		/// the file the symbol was written to, empty when nothing was written
		QString savedPath() const;

	private slots:
		void chooseDefaultPart();
		void clearDefaultPart();
		void addTerminal();
		void removeTerminal();
		void pairSelected();
		void unpairSelected();
		void chooseFolder();
		void terminalsChanged();
		void terminalRowChanged();
		void terminalPickedInPreview(int index);
		void save();

	private:
		void setUpWidget();
		void fillTerminals();
		void readTerminals();
		void refreshProblems();
		/// the folder the symbol goes in, created if it does not exist yet
		QString targetFolder() const;

		SymbolDefinition m_symbol;
		Catalog *m_catalog = nullptr;
		SymbolGrid m_grid;
		QString m_saved_path;

		QLineEdit *m_name = nullptr;
		QComboBox *m_class = nullptr;
		QComboBox *m_link_type = nullptr;
		QLabel *m_default_part = nullptr;
		QPushButton *m_choose_part = nullptr;
		QPushButton *m_clear_part = nullptr;
		QLineEdit *m_folder = nullptr;
		QPushButton *m_folder_button = nullptr;
		QTableWidget *m_terminals = nullptr;
		SymbolPreview *m_preview = nullptr;
		QPushButton *m_add_terminal = nullptr;
		QPushButton *m_remove_terminal = nullptr;
		QPushButton *m_pair = nullptr;
		QPushButton *m_unpair = nullptr;
		QListWidget *m_properties = nullptr;
		QLabel *m_snap_note = nullptr;
		QLabel *m_problems = nullptr;
		QLabel *m_summary = nullptr;
		QPushButton *m_save = nullptr;
};

#endif // CREATESYMBOLDIALOG_H
