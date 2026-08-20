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
#ifndef CATALOGPARTDIALOG_H
#define CATALOGPARTDIALOG_H

#include "../catalogpart.h"
#include "../catalogproperty.h"

#include <QDialog>
#include <QHash>

class Catalog;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QTableWidget;
class QWidget;

/**
	@brief The CatalogPartDialog class
	Create or edit one catalog part: its class, its code, the values of the
	properties its class declares, the real numbers of its pins and the
	accessories it brings along.

	The value editors are built from the property declarations, not written by
	hand, which is what keeps the guarantee that a field added to a class shows
	up here without anybody touching this file.

	Saving offers the two ways the specification separates, and says which one
	is being taken: **in place**, which corrects a record for every project
	that uses it, and **new revision**, which leaves the delivered projects on
	the data they were delivered with.
*/
class CatalogPartDialog : public QDialog
{
	Q_OBJECT

	public:
		CatalogPartDialog(Catalog *catalog,
				  const CatalogPart &part,
				  QWidget *parent = nullptr);

		CatalogPart part() const;

	private slots:
		void classChanged();
		void addPin();
		void removeSelectedPin();
		void movePinUp();
		void movePinDown();
		void addAccessory();
		void removeSelectedAccessory();
		void pickAccessoryFromCatalog();
		void saveInPlace();
		void saveAsNewRevision();

	private:
		void buildWidgets();
		void buildValueEditors();
		void fillPinTable();
		void fillAccessoryTable();
		bool collect(CatalogPart &part);
		bool save(bool as_new_revision);
		void movePin(int offset);

		QWidget *editorFor(const CatalogProperty &property, const QString &value);
		QString valueFrom(const CatalogProperty &property, QWidget *editor) const;

	private:
		Catalog *m_catalog = nullptr;
		CatalogPart m_part;

		QComboBox *m_class = nullptr;
		QLineEdit *m_code = nullptr;
		QLabel *m_revision = nullptr;
		QFormLayout *m_value_form = nullptr;
		QWidget *m_value_container = nullptr;
		QHash<QString, QWidget *> m_editors;
		QTableWidget *m_pins = nullptr;
		QTableWidget *m_accessories = nullptr;
		QLabel *m_status = nullptr;
};

#endif // CATALOGPARTDIALOG_H
