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
#ifndef CATALOGBROWSERDIALOG_H
#define CATALOGBROWSERDIALOG_H

#include "../catalogpart.h"

#include <QDialog>

class Catalog;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextBrowser;

/**
	@brief The CatalogBrowserDialog class
	Search the catalog and pick a part.

	Free text on the code and on every value, a filter by class that includes
	the subclasses, a filter by manufacturer, and one button that clears all
	of it - refining a search you cannot undo is worse than no filter at all.

	The dialog is also the way in to creating and editing a part, because the
	moment somebody notices a part is missing is the moment they are looking
	for it.
*/
class CatalogBrowserDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit CatalogBrowserDialog(Catalog *catalog, QWidget *parent = nullptr);

		/// @return the part the user picked, a null part when none
		CatalogPart selectedPart() const;

		/**
			Open the browser and return the part the user picked, a null part
			when the dialog was cancelled.
		*/
		static CatalogPart choosePart(Catalog *catalog, QWidget *parent = nullptr);

	private slots:
		void search();
		void clearFilters();
		void selectionChanged();
		void createPart();
		void editSelectedPart();
		void duplicateSelectedPart();
		void removeSelectedPart();
		void searchRepository();
		void acceptSelection();

	private:
		void buildWidgets();
		void fillClassFilter();
		void fillManufacturerFilter();
		void showPreview(const CatalogPart &part);

	private:
		Catalog *m_catalog = nullptr;
		QList<CatalogPart> m_results;
		CatalogPart m_selected;

		QLineEdit *m_text = nullptr;
		QComboBox *m_class_filter = nullptr;
		QComboBox *m_manufacturer_filter = nullptr;
		QPushButton *m_clear_filters = nullptr;
		QTableWidget *m_results_table = nullptr;
		QTextBrowser *m_preview = nullptr;
		QLabel *m_image = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_new_part = nullptr;
		QPushButton *m_edit_part = nullptr;
		QPushButton *m_duplicate_part = nullptr;
		QPushButton *m_remove_part = nullptr;
		QPushButton *m_repository = nullptr;
		QPushButton *m_choose = nullptr;
};

#endif // CATALOGBROWSERDIALOG_H
