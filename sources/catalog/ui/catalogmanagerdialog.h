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
#ifndef CATALOGMANAGERDIALOG_H
#define CATALOGMANAGERDIALOG_H

#include <QDialog>

class Catalog;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The CatalogManagerDialog class
	Where the class tree, the typed properties and the controlled lists of
	the shared catalog are edited.

	This is the dialog where the model delivers what it was designed for:
	adding a field to the catalog happens here, takes a few seconds, and needs
	neither a programmer nor a database migration.
*/
class CatalogManagerDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit CatalogManagerDialog(Catalog *catalog, QWidget *parent = nullptr);

	private slots:
		void classSelected();
		void addRootClass();
		void addSubclass();
		void removeSelectedClass();
		void applyClassChanges();
		void exportClassBranch();
		void importClassBranch();

		void addProperty();
		void editSelectedProperty();
		void removeSelectedProperty();
		void movePropertyUp();
		void movePropertyDown();

		void listSelected();
		void addList();
		void removeSelectedList();
		void saveListValues();

	private:
		void buildWidgets();
		void reloadClassTree();
		void reloadPropertyTable();
		void reloadLists();
		void updateEnabledState();
		int selectedClassId() const;
		int selectedPropertyId() const;
		void moveProperty(int offset);
		void appendClassItem(QTreeWidgetItem *parent_item, int class_id);

	private:
		Catalog *m_catalog = nullptr;

		QTreeWidget *m_class_tree = nullptr;
		QPushButton *m_add_root_class = nullptr;
		QPushButton *m_add_subclass = nullptr;
		QPushButton *m_remove_class = nullptr;
		QPushButton *m_export_class = nullptr;
		QPushButton *m_import_class = nullptr;

		QLineEdit *m_class_name = nullptr;
		QLineEdit *m_class_key = nullptr;
		QLineEdit *m_class_description = nullptr;
		QLineEdit *m_class_root = nullptr;
		QLineEdit *m_class_root_iec = nullptr;
		QCheckBox *m_class_has_symbol = nullptr;
		QComboBox *m_class_numbering = nullptr;
		QPushButton *m_apply_class = nullptr;

		QTableWidget *m_property_table = nullptr;
		QPushButton *m_add_property = nullptr;
		QPushButton *m_edit_property = nullptr;
		QPushButton *m_remove_property = nullptr;
		QPushButton *m_move_property_up = nullptr;
		QPushButton *m_move_property_down = nullptr;

		QListWidget *m_list_names = nullptr;
		QPlainTextEdit *m_list_values = nullptr;
		QPushButton *m_add_list = nullptr;
		QPushButton *m_remove_list = nullptr;
		QPushButton *m_save_list = nullptr;

		QLabel *m_status = nullptr;
};

#endif // CATALOGMANAGERDIALOG_H
