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
#ifndef CATALOGREPOSITORYDIALOG_H
#define CATALOGREPOSITORYDIALOG_H

#include "../catalogpart.h"
#include "../catalogrepository.h"

#include <QDialog>

class Catalog;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextBrowser;

/**
	@brief The CatalogRepositoryDialog class
	Search the shared repository, look at what a package holds before taking
	it, and bring it into the local catalog.

	It is also the way out of a dead end: the moment somebody discovers a part
	is missing is the moment they are assigning one, so the assignment dialog
	can open this, and importedPart() carries the result back so the
	assignment continues instead of starting over.
*/
class CatalogRepositoryDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit CatalogRepositoryDialog(Catalog *catalog, QWidget *parent = nullptr);

		/// The part that was brought into the catalog, null when none was
		CatalogPart importedPart() const;

		/**
			Open the repository, and return the part the user downloaded and
			imported - a null part when they took nothing.
		*/
		static CatalogPart findAndImport(Catalog *catalog, QWidget *parent = nullptr);

	private slots:
		void reload();
		void search();
		void clearFilters();
		void selectionChanged();
		void importSelected();
		void contribute();
		void chooseFolder();

	private:
		void buildWidgets();
		void fillFilters();
		void showPreview(const CatalogRepositoryEntry &entry);

	private:
		Catalog *m_catalog = nullptr;
		QList<CatalogRepositoryEntry> m_entries;
		QList<CatalogRepositoryEntry> m_results;
		CatalogRepositoryEntry m_selected;
		CatalogPart m_imported;

		QLineEdit *m_folder = nullptr;
		QPushButton *m_choose_folder = nullptr;
		QLineEdit *m_text = nullptr;
		QComboBox *m_class_filter = nullptr;
		QComboBox *m_manufacturer_filter = nullptr;
		QPushButton *m_clear_filters = nullptr;
		QTableWidget *m_results_table = nullptr;
		QTextBrowser *m_preview = nullptr;
		QLabel *m_image = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_import = nullptr;
		QPushButton *m_contribute = nullptr;
};

#endif // CATALOGREPOSITORYDIALOG_H
