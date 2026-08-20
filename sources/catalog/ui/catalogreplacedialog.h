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
#ifndef CATALOGREPLACEDIALOG_H
#define CATALOGREPLACEDIALOG_H

#include <QDialog>
#include <QHash>
#include <QList>
#include <QString>

#include "../catalogpart.h"

class Catalog;
class Element;
class QETProject;
class QLabel;
class QPushButton;
class QTableWidget;

/**
	@brief Swap one catalog part for another across a whole project.

	The move this exists for: the contactor chosen three weeks ago is out of
	stock, or the customer asked for another brand, and it is in nineteen
	places over six folios. Doing that by hand means opening nineteen
	components and getting one of them wrong.

	Which parts the project uses, and how many components use each, is the
	first thing on screen — because the question "what is in this project"
	comes before "swap it". Choosing the replacement says how many components
	move, and on which folios, before anything happens.

	One undo command for the whole swap. Nineteen Ctrl+Z is not undoing.
*/
class CatalogReplaceDialog : public QDialog
{
	Q_OBJECT

	public:
		CatalogReplaceDialog(QETProject *project,
				     Catalog *catalog,
				     QWidget *parent = nullptr);

		/// how many components the swap touched, 0 when nothing was done
		int replacedCount() const;

	private slots:
		void selectionChanged();
		void chooseReplacement();
		void replace();

	private:
		void setUpWidget();
		void reload();
		/// the components of the project that carry @a code
		QList<Element *> componentsUsing(const QString &code) const;
		/// the folios @a elements sit on, by their displayed title
		QStringList foliosOf(const QList<Element *> &elements) const;

		QETProject *m_project = nullptr;
		Catalog *m_catalog = nullptr;
		CatalogPart m_replacement;
		int m_replaced = 0;

		/// part code → the components using it, built once per reload
		QHash<QString, QList<Element *>> m_usage;

		QTableWidget *m_table = nullptr;
		QLabel *m_replacement_label = nullptr;
		QPushButton *m_choose = nullptr;
		QLabel *m_preview = nullptr;
		QPushButton *m_replace = nullptr;
};

#endif // CATALOGREPLACEDIALOG_H
