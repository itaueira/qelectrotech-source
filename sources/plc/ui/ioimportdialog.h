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
#ifndef IOIMPORTDIALOG_H
#define IOIMPORTDIALOG_H

#include "../iolist.h"
#include "../iosheet.h"

#include <QDialog>
#include <QList>
#include <QStringList>

class QCheckBox;
class QETProject;
class QLabel;
class QPushButton;
class QTableWidget;

/**
	@brief The IoImportDialog class
	Bring a sheet of I/O points into a project: paste it or open it, see what
	was read, say which column feeds which field, see what the import would
	do, and only then do it.

	The order is the one CatalogImportDialog established and for the same
	reason: an import that runs first and explains afterwards is an import
	nobody trusts a second time. Here it matters more, because importing is
	something this list goes through many times - the automation department
	revises it - and the second import is the dangerous one. So the summary
	shown before the button is pressed is the merge itself, run against a
	copy: five new, three changed, eighty-eight untouched, two the sheet no
	longer mentions. Nothing is written until Importer.

	Nothing is drawn either. An imported point exists in the project before
	it exists on any folio, which is exactly the state the task needed and
	the QET did not have.
*/
class IoImportDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit IoImportDialog(QETProject *project, QWidget *parent = nullptr);

		/// @return what the last import did, empty when nothing was imported
		IoList::MergeReport report() const;

	private slots:
		void pasteFromClipboard();
		void openFile();
		void headerToggled(bool checked);
		void mappingChanged();
		void runImport();

	private:
		void buildWidgets();
		void setGrid(const QList<QStringList> &grid, const QString &origin);
		void reloadPreview();
		void reloadMappingTable();
		void readMappingTable();
		void reloadSummary();
		void updateEnabledState();
		void say(const QString &message, bool problem = false);
		/// @return the mapping table rebuilt into m_mapping, points and all
		IoSheet::Report readSheet() const;
		/// @return "A", "B" … or "A — Tipo" when the sheet names its columns
		QString columnLabel(int column) const;

	private:
		QETProject *m_project = nullptr;
		QList<QStringList> m_grid;
		IoSheet::Mapping m_mapping;
		IoList::MergeReport m_report;
		bool m_loading = false;

		QPushButton *m_paste = nullptr;
		QPushButton *m_open = nullptr;
		QCheckBox *m_has_header = nullptr;
		QTableWidget *m_preview = nullptr;
		QTableWidget *m_columns = nullptr;
		QLabel *m_leftover = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_import = nullptr;
};

#endif // IOIMPORTDIALOG_H
