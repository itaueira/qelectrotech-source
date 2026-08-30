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
#ifndef CIRCUITTABLEDIALOG_H
#define CIRCUITTABLEDIALOG_H

#include "../circuitgenerator.h"
#include "../circuittable.h"

#include <QDialog>
#include <QStringList>
#include <QStyledItemDelegate>

class QETProject;

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;

/**
	@brief Edits one cell of the circuit table with the widget its type asks
	for.

	The map of type to widget is the one MacroParametersDialog already uses
	for the insertion of a single macro: a list is a list, a number refuses
	letters while they are typed, a part code has the catalogue behind it.
	Twenty circuits filled in a table and one circuit filled in a dialogue
	have to answer the same way, otherwise the table becomes the place where
	values nobody would have accepted one at a time get in.

	A cell whose row uses a macro that does not declare the variable is
	inert, and inert means no editor at all rather than an editor that
	refuses on commit.
*/
class CircuitCellDelegate : public QStyledItemDelegate
{
	Q_OBJECT

	public:
		explicit CircuitCellDelegate(QObject *parent = nullptr);

		/// @param table the table the cells belong to; not owned
		void setTable(const CircuitTable *table);

		QWidget *createEditor(QWidget *parent,
				      const QStyleOptionViewItem &option,
				      const QModelIndex &index) const override;
		void setEditorData(QWidget *editor,
				   const QModelIndex &index) const override;
		void setModelData(QWidget *editor,
				  QAbstractItemModel *model,
				  const QModelIndex &index) const override;

	private:
		const CircuitTable *m_table = nullptr;
};

/**
	@brief The table of circuits, and the button that draws it.

	This is the door of T08. Twenty feeders that differ by a mark, a rating
	and a cable section are twenty lines of a table, not twenty trips through
	the insertion dialogue, and the table is where a person can see all
	twenty at once - which is the only way to notice that the eleventh says
	16 A where every other one of its family says 10.

	Three things it deliberately does the way a spreadsheet does, because
	that is where the values come from and what the hands already know:
	Ctrl+V pastes a block of cells, Ctrl+C copies one back, and a column can
	be recopied downwards or continued as a series. What a paste could not
	write is said cell by cell and the rest of the paste stands - refusing
	twenty rows because one holds a typo is how a person loses the twenty
	minutes of typing this exists to save.

	Nothing is drawn until the button is pressed, and what the button does is
	one undoable command. The report is shown afterwards and names every row
	that was skipped, because a generation that quietly drew nineteen of
	twenty is worse than one that drew none.
*/
class CircuitTableDialog : public QDialog
{
	Q_OBJECT

	public:
		/**
			@param project where the circuits will be drawn; may be nullptr,
			and then the table can be filled but not generated
			@param parent
		*/
		explicit CircuitTableDialog(QETProject *project,
					    QWidget *parent = nullptr);

		/// @return the table as the dialogue left it
		CircuitTable table() const;
		/// @param table what the dialogue opens on
		void setTable(const CircuitTable &table);
		/// @return what the last generation did, empty when none ran
		CircuitGenerator::Report report() const;

	private slots:
		void addRow();
		void removeRows();
		void chooseMacroForSelection();
		void choosePartForCell();
		void cellDoubleClicked(int row, int column);
		void itemChanged(QTableWidgetItem *item);
		void selectionChanged();
		void pasteFromClipboard();
		void copyToClipboard();
		void fillDown();
		void fillSeries();
		void generate();

	private:
		void buildWidgets();
		void reload();
		void updateButtons();
		void say(const QString &message, bool problem = false);

			/// @return the parameter name the view column carries
		QString columnAt(int view_column) const;
			/// @return the macro new rows and pastes should use
		QString macroForNewRows() const;
			/// @return what the file chooser gave, symbolic when it can be
		QString chooseMacroPath();
			/// @return true when the parameters of @a macro_path are known
		bool loadMacro(const QString &macro_path, QString *error = nullptr);
			/// @return the selected block, columns in view order
		void selectedBlock(int *top, int *bottom, QStringList *columns) const;

		QETProject *m_project = nullptr;
		CircuitTable m_table;
		CircuitGenerator::Report m_report;

		QTableWidget *m_view = nullptr;
		CircuitCellDelegate *m_delegate = nullptr;
		QPushButton *m_add = nullptr;
		QPushButton *m_remove = nullptr;
		QPushButton *m_macro = nullptr;
		QPushButton *m_part = nullptr;
		QPushButton *m_fill_down = nullptr;
		QPushButton *m_fill_series = nullptr;
		QPushButton *m_generate = nullptr;
		QSpinBox *m_per_sheet = nullptr;
		QLineEdit *m_sheet_title = nullptr;
		QLabel *m_status = nullptr;

			/// the last macro chosen, so the next row starts from it
		QString m_last_macro;
			/// true while the view is being filled from the table
		bool m_loading = false;
};

#endif // CIRCUITTABLEDIALOG_H
