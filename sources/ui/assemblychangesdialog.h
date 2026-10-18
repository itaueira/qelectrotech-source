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
#ifndef ASSEMBLYCHANGESDIALOG_H
#define ASSEMBLYCHANGESDIALOG_H

#include <QDialog>
#include <QList>
#include <QPointer>

#include "../autoNum/assemblychanges.h"

class Conductor;
class Element;
class QETProject;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The order of service of a modification to a panel already wired:
	what to mount, what to take out, what to label again.

	@par Where the lines come from
	AssemblyChanges compares; this window walks the project, hands the
	comparison what is drawn now, and shows the answer. The walk reads the
	very lists and the very fields the photograph was taken from -
	ProjectRenumberer::components() and the label information of each
	component, the conductors of each folio and their text - because any
	difference between the two readings would show up as a change that
	nobody made: a field read another way is every item of the panel
	"relabelled", a list walked another way is a terminal "added".

	@par The way to the item is the one the checker already uses
	showMe() on the folio, clearSelection(), setSelected() and
	ensureVisible() on the object - the four lines of DrcPanel, and for the
	same reason: they need nothing but the folio, and so the window does not
	have to know the editor it sits in front of. And like DrcPanel, and
	unlike the reports that close themselves on the first double click, it
	stays: an order of service is a list to work through, and a list that
	vanishes at the first row is a list nobody finishes. Whoever opens it
	therefore opens it non modal.

	A removed item has nowhere to go: it is drawn on no folio any more. Its
	row says so rather than opening some folio of its own choosing.

	@par What a removed line can say
	The photograph keeps a uuid and a label per item and nothing else, so a
	removed component is named by the tag printed on it - which is how the
	shop finds it on the rail - and a removed wire by its number, without
	its two ends. The window says it once, under the list, rather than
	leaving the shop to wonder why that column is empty.

	@par The file
	AssemblyChanges::writeCsv(): fixed keys in the first row, the rows in the
	order of the window. exportTo() is the whole of the export but the
	question of where, so that a bench can write and read back the file
	without a file dialog to answer.
*/
class AssemblyChangesDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit AssemblyChangesDialog(QETProject *project,
					       QWidget *parent = nullptr);

			/// The lines the window shows, as last computed.
		const AssemblyChanges &changes() const {return m_changes;}

		/**
			@param project
			@return what changed on @a project since it was marked, read
			the way this window reads it.
		*/
		static AssemblyChanges compute(QETProject *project);

		/**
			@param index a line of changes().all()
			@return true when the folio of that line was brought forward
			and its item selected; false for a removal, which is drawn
			nowhere, and for an item deleted since the list was made.
		*/
		bool goTo(int index);

		/**
			Write the lines the window shows to @a path.
			@return an empty string when the file was written, else the
			reason.
		*/
		QString exportTo(const QString &path) const;

	public slots:
			/// Walk the project again and show what it says now.
		void refresh();

	private slots:
		void exportCsv();
		void rowActivated(QTreeWidgetItem *item, int column);

	private:
		void setUpWidget();
		void fill();

		QPointer<QETProject> m_project;
		AssemblyChanges m_changes;
			/// The components walked, in the order of the items compared:
			/// AssemblyChange::source points into this.
		QList<QPointer<Element>> m_components;
			/// Same, for the wires.
		QList<QPointer<Conductor>> m_wires;

		QLabel *m_summary = nullptr;
		QTreeWidget *m_tree = nullptr;
		QLabel *m_note = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_export = nullptr;
};

#endif // ASSEMBLYCHANGESDIALOG_H
