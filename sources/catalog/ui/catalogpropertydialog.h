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
#ifndef CATALOGPROPERTYDIALOG_H
#define CATALOGPROPERTYDIALOG_H

#include "../catalogproperty.h"

#include <QDialog>

class Catalog;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

/**
	@brief The CatalogPropertyDialog class
	Create or edit one typed property of a catalog class.

	The key is derived from the name while the user types, and stops being
	derived as soon as it is saved once: renaming a property must not orphan
	the values the parts already carry under the old key.
*/
class CatalogPropertyDialog : public QDialog
{
	Q_OBJECT

	public:
		CatalogPropertyDialog(Catalog *catalog,
				      const CatalogProperty &property,
				      QWidget *parent = nullptr);

		CatalogProperty property() const;
		/// true when the user asked for the initial value to reach the existing parts
		bool applyToExistingParts() const;

	private slots:
		void nameEdited(const QString &name);
		void listBehaviourChanged();
		void validateAndAccept();

	private:
		void buildWidgets();
		void fillFromProperty();

	private:
		Catalog *m_catalog = nullptr;
		CatalogProperty m_property;
		bool m_key_is_frozen = false;

		QLineEdit *m_name = nullptr;
		QLineEdit *m_key = nullptr;
		QComboBox *m_type = nullptr;
		QComboBox *m_list_behaviour = nullptr;
		QComboBox *m_list_name = nullptr;
		QPlainTextEdit *m_inline_options = nullptr;
		QLabel *m_inline_options_label = nullptr;
		QLineEdit *m_default_value = nullptr;
		QLineEdit *m_unit = nullptr;
		QLineEdit *m_description = nullptr;
		QCheckBox *m_apply_to_existing = nullptr;
};

#endif // CATALOGPROPERTYDIALOG_H
