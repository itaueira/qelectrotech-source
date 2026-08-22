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
#ifndef CATALOGIMPORTDIALOG_H
#define CATALOGIMPORTDIALOG_H

#include "../catalogimport.h"

#include <QDialog>
#include <QHash>

class Catalog;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextBrowser;

/**
	@brief The CatalogImportDialog class
	Import a spreadsheet into the catalog: choose the file, see what is in it,
	say what each column means, choose what to do with what already exists,
	and read the report.

	The order matters. The file is shown **before** anything is mapped, and
	the mapping is shown **before** anything is written, because an import
	that runs first and explains afterwards is an import nobody trusts a
	second time.
*/
class CatalogImportDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit CatalogImportDialog(Catalog *catalog, QWidget *parent = nullptr);

	private slots:
		void chooseFile();
		void classChanged();
		void guessMapping();
		void saveProfile();
		void loadProfile();
		void removeProfile();
		void runImport();
		void exportCatalog();

	private:
		void buildWidgets();
		void reloadFile();
		void reloadPreview();
		void reloadMappingTable();
		/**
			Say which columns of the file nothing will read, by name and
			before anything is written. The mapping table has one row per
			property of the class, so a column the class has no field for
			has nowhere else to appear. (CU-14.12)
		*/
		void reloadLeftoverColumns();
		void reloadProfileList();
		CatalogImportProfile currentProfile() const;
		void applyProfile(const CatalogImportProfile &profile);
		void updateEnabledState();

	private:
		Catalog *m_catalog = nullptr;
		CatalogTable m_table;
		QString m_file_path;

		QLineEdit *m_file = nullptr;
		QPushButton *m_choose_file = nullptr;
		QComboBox *m_class = nullptr;
		QComboBox *m_class_column = nullptr;
		QComboBox *m_code_column = nullptr;
		QComboBox *m_policy = nullptr;
		QTableWidget *m_preview = nullptr;
		QTableWidget *m_mapping = nullptr;
		QComboBox *m_profiles = nullptr;
		QPushButton *m_save_profile = nullptr;
		QPushButton *m_load_profile = nullptr;
		QPushButton *m_remove_profile = nullptr;
		QPushButton *m_guess = nullptr;
		QPushButton *m_import = nullptr;
		QPushButton *m_export = nullptr;
		QLabel *m_leftover = nullptr;
		QTextBrowser *m_report = nullptr;
		QLabel *m_status = nullptr;
};

#endif // CATALOGIMPORTDIALOG_H
