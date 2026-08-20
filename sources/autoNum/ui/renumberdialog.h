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
#ifndef RENUMBERDIALOG_H
#define RENUMBERDIALOG_H

#include "../numberingformat.h"
#include "../renumberplan.h"

#include <QDialog>
#include <QList>

class Catalog;
class Element;
class QETProject;
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QTableWidget;

/**
	@brief The RenumberDialog class
	Show what a renumbering would do, and only then do it.

	Renumbering blind is worse than not renumbering: a project comes back from
	the workshop with the tags of the drawing, and a silent renumbering means
	the drawing and the cabinet stop agreeing. So the "from → to" table is not
	an option to tick, it is the dialog.
*/
class RenumberDialog : public QDialog
{
	Q_OBJECT

	public:
		RenumberDialog(QETProject *project,
			       Catalog *catalog,
			       const QList<Element *> &selection,
			       QWidget *parent = nullptr);

		/// How many components were renumbered, 0 when nothing was applied
		int appliedCount() const;

	private slots:
		void recompute();
		void apply();

	private:
		void buildWidgets();
		QList<Element *> currentScope() const;

	private:
		QETProject *m_project = nullptr;
		Catalog *m_catalog = nullptr;
		QList<Element *> m_selection;
		QList<Element *> m_scope;
		RenumberPlan m_plan;
		int m_applied = 0;

		QRadioButton *m_whole_project = nullptr;
		QRadioButton *m_only_selection = nullptr;
		QComboBox *m_format = nullptr;
		QComboBox *m_orientation = nullptr;
		QCheckBox *m_only_changes = nullptr;
		QTableWidget *m_preview = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_warning = nullptr;
};

#endif // RENUMBERDIALOG_H
