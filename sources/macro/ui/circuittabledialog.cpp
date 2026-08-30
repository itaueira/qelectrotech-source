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
#include "circuittabledialog.h"

#include "macroparametersdialog.h"

#include "../macrofile.h"
#include "../macroparameter.h"
#include "../macroparameterset.h"

#include "../../qetapp.h"
#include "../../qeticons.h"
#include "../../qetproject.h"
#include "../../catalog/catalogpart.h"
#include "../../catalog/ui/catalogbrowserdialog.h"
#include "../../ElementsCollection/elementslocation.h"

#include <algorithm>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
	/**
		@brief columnOf
		@param index
		@return the parameter name the view column of @a index carries

		The name travels in the header, under Qt::UserRole, so nothing has to
		keep a second map from view column to parameter - and the delegate,
		which does not know the dialogue, can still ask what it is editing.
	*/
	QString columnOf(const QModelIndex &index)
	{
		if (!index.isValid() || !index.model()) {
			return QString();
		}
		return index.model()->headerData(index.column(),
						 Qt::Horizontal,
						 Qt::UserRole).toString();
	}

	/**
		@brief selectedRowsOf
		@param view
		@return the rows the selection covers, in order, the current one when
		nothing is selected
	*/
	QList<int> selectedRowsOf(const QTableWidget *view)
	{
		QList<int> rows;
		if (!view) {
			return rows;
		}

		const QList<QTableWidgetSelectionRange> ranges = view->selectedRanges();
		for (const QTableWidgetSelectionRange &range : ranges)
		{
			for (int row = range.topRow() ; row <= range.bottomRow() ; ++ row)
			{
				if (!rows.contains(row)) {
					rows << row;
				}
			}
		}

		if (rows.isEmpty() && view->currentRow() >= 0) {
			rows << view->currentRow();
		}

		std::sort(rows.begin(), rows.end());
		return rows;
	}
}

/**
	@brief CircuitCellDelegate::CircuitCellDelegate
	@param parent
*/
CircuitCellDelegate::CircuitCellDelegate(QObject *parent) :
	QStyledItemDelegate(parent)
{}

/**
	@brief CircuitCellDelegate::setTable
	@param table : not owned, and expected to outlive the delegate
*/
void CircuitCellDelegate::setTable(const CircuitTable *table)
{
	m_table = table;
}

/**
	@brief CircuitCellDelegate::createEditor
	@param parent
	@param option
	@param index
	@return the editor the declared type of this cell asks for
*/
QWidget *CircuitCellDelegate::createEditor(QWidget *parent,
					   const QStyleOptionViewItem &option,
					   const QModelIndex &index) const
{
	const QString column = columnOf(index);
	if (!m_table || column.isEmpty()) {
		return QStyledItemDelegate::createEditor(parent, option, index);
	}

	const MacroParameter parameter = m_table->parameterFor(index.row(), column);
		//An inert cell gets no editor at all. One that opened and then refused
		//on commit would say the value can be written here, and it cannot: the
		//macro of this row does not declare the variable.
	if (parameter.isNull()) {
		return nullptr;
	}

	switch (parameter.type)
	{
		case MacroParameterType::List:
		{
			QComboBox *combo = new QComboBox(parent);
				//A list holds what it declares and nothing else. The empty
				//entry is there only when the parameter may be left out.
			if (!parameter.required) {
				combo->addItem(QString(), QString());
			}
			for (const QString &choice : parameter.choices) {
				combo->addItem(choice, choice);
			}
			return combo;
		}
		case MacroParameterType::Number:
		{
			QLineEdit *line = new QLineEdit(parent);
			line->setValidator(new QRegularExpressionValidator(
				QRegularExpression(QStringLiteral("^[+-]?[0-9]{0,12}([.,][0-9]{0,6})?$")),
				line));
			return line;
		}
		case MacroParameterType::Part:
		case MacroParameterType::Text:
		default:
			break;
	}

	return new QLineEdit(parent);
}

/**
	@brief CircuitCellDelegate::setEditorData
	@param editor
	@param index
*/
void CircuitCellDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	const QString value = index.data(Qt::EditRole).toString();

	if (QComboBox *combo = qobject_cast<QComboBox *>(editor))
	{
		int at = combo->findData(value);
			//A value the list does not hold is shown rather than silently
			//turned into the first choice: it came from somewhere, and losing
			//it on a double click would be worse than showing it.
		if (at < 0 && !value.isEmpty())
		{
			combo->addItem(value, value);
			at = combo->count() - 1;
		}
		combo->setCurrentIndex(at < 0 ? 0 : at);
		return;
	}

	if (QLineEdit *line = qobject_cast<QLineEdit *>(editor))
	{
		line->setText(value);
		return;
	}

	QStyledItemDelegate::setEditorData(editor, index);
}

/**
	@brief CircuitCellDelegate::setModelData
	@param editor
	@param model
	@param index
*/
void CircuitCellDelegate::setModelData(QWidget *editor,
				       QAbstractItemModel *model,
				       const QModelIndex &index) const
{
	if (QComboBox *combo = qobject_cast<QComboBox *>(editor))
	{
		model->setData(index, combo->currentData().toString(), Qt::EditRole);
		return;
	}

	if (QLineEdit *line = qobject_cast<QLineEdit *>(editor))
	{
		model->setData(index, line->text(), Qt::EditRole);
		return;
	}

	QStyledItemDelegate::setModelData(editor, model, index);
}

/**
	@brief CircuitTableDialog::CircuitTableDialog
	@param project
	@param parent
*/
CircuitTableDialog::CircuitTableDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Générer des circuits à partir d'une table"));
	buildWidgets();
	reload();
}

/**
	@brief CircuitTableDialog::table
	@return the table as the dialogue left it
*/
CircuitTable CircuitTableDialog::table() const
{
	return m_table;
}

/**
	@brief CircuitTableDialog::setTable
	@param table : what the dialogue opens on
*/
void CircuitTableDialog::setTable(const CircuitTable &table)
{
	m_table = table;

		//A table read back from the project file remembers which macro each row
		//uses, but not what those macros declare: that lives in the .qetmak.
		//Without opening them again every column would come back inert.
	QStringList problems;
	QStringList failed;
	for (int row = 0 ; row < m_table.rowCount() ; ++ row)
	{
		const QString macro_path = m_table.macroPath(row);
		if (macro_path.isEmpty()
		    || m_table.hasParameters(macro_path)
		    || failed.contains(macro_path)) {
			continue;
		}

		QString error;
		if (!loadMacro(macro_path, &error))
		{
			failed << macro_path;
			problems << tr("Ligne %1 : %2").arg(row + 1).arg(error);
		}
	}

	if (m_table.rowCount() > 0) {
		m_last_macro = m_table.macroPath(m_table.rowCount() - 1);
	}

	reload();
	say(problems.join(QLatin1Char('\n')), !problems.isEmpty());
}

/**
	@brief CircuitTableDialog::report
	@return what the last generation did, empty when none ran
*/
CircuitGenerator::Report CircuitTableDialog::report() const
{
	return m_report;
}

/**
	@brief CircuitTableDialog::buildWidgets
*/
void CircuitTableDialog::buildWidgets()
{
	m_view = new QTableWidget(this);
	m_view->setSelectionMode(QAbstractItemView::ContiguousSelection);
	m_view->setAlternatingRowColors(true);
	m_view->horizontalHeader()->setStretchLastSection(true);

	m_delegate = new CircuitCellDelegate(this);
	m_delegate->setTable(&m_table);
	m_view->setItemDelegate(m_delegate);

	connect(m_view, &QTableWidget::cellDoubleClicked,
		this, &CircuitTableDialog::cellDoubleClicked);
	connect(m_view, &QTableWidget::itemChanged,
		this, &CircuitTableDialog::itemChanged);
	connect(m_view, &QTableWidget::itemSelectionChanged,
		this, &CircuitTableDialog::selectionChanged);
	connect(m_view, &QTableWidget::currentCellChanged,
		this, &CircuitTableDialog::selectionChanged);

		//Ctrl+V and Ctrl+C belong to the table and not to the dialogue: while a
		//cell is being edited the line inside it keeps its own clipboard, which
		//is what a person expects when the caret is in a word.
	QShortcut *paste = new QShortcut(QKeySequence::Paste, m_view);
	paste->setContext(Qt::WidgetShortcut);
	connect(paste, &QShortcut::activated, this, &CircuitTableDialog::pasteFromClipboard);

	QShortcut *copy = new QShortcut(QKeySequence::Copy, m_view);
	copy->setContext(Qt::WidgetShortcut);
	connect(copy, &QShortcut::activated, this, &CircuitTableDialog::copyToClipboard);

	m_add = new QPushButton(QET::Icons::Add, tr("Ajouter une ligne"), this);
	connect(m_add, &QPushButton::clicked, this, &CircuitTableDialog::addRow);

	m_remove = new QPushButton(QET::Icons::Remove, tr("Supprimer"), this);
	connect(m_remove, &QPushButton::clicked, this, &CircuitTableDialog::removeRows);

	m_macro = new QPushButton(tr("Macro…"), this);
	m_macro->setToolTip(tr("Choisir le macro des lignes sélectionnées."));
	connect(m_macro, &QPushButton::clicked, this, &CircuitTableDialog::chooseMacroForSelection);

	m_fill_down = new QPushButton(tr("Recopier vers le bas"), this);
	m_fill_down->setToolTip(tr("Écrire la valeur de la première ligne "
				   "sélectionnée dans toutes les suivantes."));
	connect(m_fill_down, &QPushButton::clicked, this, &CircuitTableDialog::fillDown);

	m_fill_series = new QPushButton(tr("Incrémenter"), this);
	m_fill_series->setToolTip(tr("Continuer la suite commencée par la première "
				     "ligne sélectionnée : -QM1, -QM2, -QM3…"));
	connect(m_fill_series, &QPushButton::clicked, this, &CircuitTableDialog::fillSeries);

	m_part = new QPushButton(tr("Article…"), this);
	m_part->setToolTip(tr("Choisir dans le catalogue l'article de la cellule courante."));
	connect(m_part, &QPushButton::clicked, this, &CircuitTableDialog::choosePartForCell);

	QFrame *separator = new QFrame(this);
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Sunken);

	QVBoxLayout *side = new QVBoxLayout();
	side->addWidget(m_add);
	side->addWidget(m_remove);
	side->addWidget(m_macro);
	side->addWidget(separator);
	side->addWidget(m_fill_down);
	side->addWidget(m_fill_series);
	side->addWidget(m_part);
	side->addStretch();

	QHBoxLayout *middle = new QHBoxLayout();
	middle->addWidget(m_view, 1);
	middle->addLayout(side);

	m_per_sheet = new QSpinBox(this);
	m_per_sheet->setRange(0, 100);
	m_per_sheet->setValue(0);
	m_per_sheet->setSpecialValueText(tr("autant que le cadre en accepte"));
	m_per_sheet->setToolTip(tr("Combien de circuits sur une folio avant "
				   "d'en ouvrir une autre."));

	m_sheet_title = new QLineEdit(this);
	m_sheet_title->setPlaceholderText(tr("laisser le titre par défaut"));

	QFormLayout *options = new QFormLayout();
	options->addRow(tr("Circuits par folio :"), m_per_sheet);
	options->addRow(tr("Titre des folios créées :"), m_sheet_title);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

	m_generate = new QPushButton(tr("Générer"), this);
	connect(m_generate, &QPushButton::clicked, this, &CircuitTableDialog::generate);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	buttons->addButton(m_generate, QDialogButtonBox::ActionRole);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *root = new QVBoxLayout(this);
	root->addLayout(middle, 1);
	root->addLayout(options);
	root->addWidget(m_status);
	root->addWidget(buttons);

	resize(900, 520);
}

/**
	@brief CircuitTableDialog::reload
	Fill the view from the table, from scratch.

	From scratch and not cell by cell because the columns are the union of
	what every macro in use declares: choosing another macro on one row can
	add a column, take one away, and move all the others.
*/
void CircuitTableDialog::reload()
{
	m_loading = true;

	const QStringList columns = m_table.columns();

	m_view->clear();
	m_view->setColumnCount(columns.count() + 1);
	m_view->setRowCount(m_table.rowCount());

	QTableWidgetItem *macro_header = new QTableWidgetItem(tr("Macro"));
	m_view->setHorizontalHeaderItem(0, macro_header);

	for (int column = 0 ; column < columns.count() ; ++ column)
	{
		QTableWidgetItem *header =
				new QTableWidgetItem(m_table.columnLabel(columns.at(column)));
		header->setData(Qt::UserRole, columns.at(column));
		header->setToolTip(columns.at(column));
		m_view->setHorizontalHeaderItem(column + 1, header);
	}

	const QBrush inert_text = palette().brush(QPalette::Disabled, QPalette::Text);

	for (int row = 0 ; row < m_table.rowCount() ; ++ row)
	{
		const QString macro_path = m_table.macroPath(row);

		QString shown = QFileInfo(macro_path).completeBaseName();
		if (shown.isEmpty()) {
			shown = tr("(aucun macro)");
		}

		QTableWidgetItem *macro_cell = new QTableWidgetItem(shown);
		macro_cell->setFlags(macro_cell->flags() & ~Qt::ItemIsEditable);
		macro_cell->setToolTip(macro_path.isEmpty()
				       ? tr("Double-cliquez pour choisir le macro de cette ligne.")
				       : macro_path);
		m_view->setItem(row, 0, macro_cell);

		for (int column = 0 ; column < columns.count() ; ++ column)
		{
			const QString name = columns.at(column);
			QTableWidgetItem *cell = new QTableWidgetItem(m_table.value(row, name));

			if (m_table.isInert(row, name))
			{
					//Shown, and greyed: the value is still there and comes
					//back the moment the row goes back to a macro that
					//declares the variable. Hiding it would look like a loss.
				cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
				cell->setForeground(inert_text);
				cell->setToolTip(tr("Le macro de cette ligne ne déclare pas "
						    "cette variable."));
			}
			else
			{
				const MacroParameter parameter = m_table.parameterFor(row, name);
				if (!parameter.description.isEmpty()) {
					cell->setToolTip(parameter.description);
				}
			}

			m_view->setItem(row, column + 1, cell);
		}
	}

	m_view->resizeColumnsToContents();

	m_loading = false;
	updateButtons();
}

/**
	@brief CircuitTableDialog::updateButtons
*/
void CircuitTableDialog::updateButtons()
{
	const bool has_rows = m_table.rowCount() > 0;
	const int row = m_view ? m_view->currentRow() : -1;
	const QString column = m_view ? columnAt(m_view->currentColumn()) : QString();

	m_remove->setEnabled(has_rows);
	m_fill_down->setEnabled(has_rows && !column.isEmpty());
	m_fill_series->setEnabled(has_rows && !column.isEmpty());

	const bool part_cell = row >= 0
			&& !column.isEmpty()
			&& m_table.parameterFor(row, column).type == MacroParameterType::Part;
	m_part->setEnabled(part_cell && QETApp::catalog() != nullptr);

	m_generate->setEnabled(has_rows
			       && m_project != nullptr
			       && !m_project->isReadOnly());
}

/**
	@brief CircuitTableDialog::say
	@param message : shown under the table, empty to clear it
	@param problem : whether it is a refusal
*/
void CircuitTableDialog::say(const QString &message, bool problem)
{
	if (!m_status) {
		return;
	}

	m_status->setText(message);

	QPalette status_palette = m_status->palette();
	status_palette.setColor(QPalette::WindowText,
				problem ? QColor(Qt::red)
					: palette().color(QPalette::WindowText));
	m_status->setPalette(status_palette);
}

/**
	@brief CircuitTableDialog::columnAt
	@param view_column
	@return the parameter name that column carries, empty for the macro column
*/
QString CircuitTableDialog::columnAt(int view_column) const
{
	if (!m_view || view_column <= 0) {
		return QString();
	}

	QTableWidgetItem *header = m_view->horizontalHeaderItem(view_column);
	return header ? header->data(Qt::UserRole).toString() : QString();
}

/**
	@brief CircuitTableDialog::macroForNewRows
	@return the macro a new row, or a paste that adds rows, should use

	The one under the cursor, then the one of the last row, then the last one
	chosen. A person who chose a macro and pasted twenty lines into an empty
	table means those twenty lines to be that macro.
*/
QString CircuitTableDialog::macroForNewRows() const
{
	const int current = m_view ? m_view->currentRow() : -1;
	if (current >= 0 && current < m_table.rowCount())
	{
		const QString path = m_table.macroPath(current);
		if (!path.isEmpty()) {
			return path;
		}
	}

	if (m_table.rowCount() > 0)
	{
		const QString path = m_table.macroPath(m_table.rowCount() - 1);
		if (!path.isEmpty()) {
			return path;
		}
	}

	return m_last_macro;
}

/**
	@brief CircuitTableDialog::chooseMacroPath
	@return what the file chooser gave, empty when nothing was chosen
*/
QString CircuitTableDialog::chooseMacroPath()
{
	QString start = QETApp::userMacrosDir();
	if (!m_last_macro.isEmpty())
	{
		const QString real = QETApp::realPath(m_last_macro);
		const QString held = real.isEmpty() ? m_last_macro : real;
		const QString directory = QFileInfo(held).absolutePath();
		if (!directory.isEmpty()) {
			start = directory;
		}
	}

	const QString chosen = QFileDialog::getOpenFileName(
				this,
				tr("Choisir un macro"),
				start,
				tr("Macro QElectroTech (*.qetmak)"));
	if (chosen.isEmpty()) {
		return QString();
	}

		//Kept symbolic when the macro sits in one of the collections the
		//program knows, so the table still finds it on the next machine. One
		//kept anywhere else keeps its real path, which is all there is to keep.
	const QString symbolic = QETApp::symbolicPath(chosen);
	return symbolic.isEmpty() ? chosen : symbolic;
}

/**
	@brief CircuitTableDialog::loadMacro
	@param macro_path
	@param error : filled with the reason when the answer is false
	@return whether the table now knows what @a macro_path declares
*/
bool CircuitTableDialog::loadMacro(const QString &macro_path, QString *error)
{
	if (macro_path.isEmpty())
	{
		if (error) {
			*error = tr("Aucun macro choisi.");
		}
		return false;
	}

	if (m_table.hasParameters(macro_path)) {
		return true;
	}

		//Read exactly as the generator and as the drop on a folio read it: a
		//macro that answered differently here would be a macro nobody could
		//check by dropping it once.
	MacroFile file;
	const ElementsLocation location(macro_path);
	const QString file_path = location.fileSystemPath();
	if (file_path.isEmpty()) {
		file.load(macro_path);
	} else {
		file.load(file_path);
	}

	if (file.isNull())
	{
		if (error) {
			*error = file.errorText();
		}
		return false;
	}

	m_table.setParameters(macro_path, file.parameters());
	return true;
}

/**
	@brief CircuitTableDialog::selectedBlock
	@param top : receives the first row of the selection, -1 when there is none
	@param bottom : receives the last one
	@param columns : receives the parameter names it covers, in view order
*/
void CircuitTableDialog::selectedBlock(int *top, int *bottom, QStringList *columns) const
{
	int first = -1;
	int last = -1;
	QStringList names;

	if (m_view)
	{
		const QList<QTableWidgetSelectionRange> ranges = m_view->selectedRanges();
		for (const QTableWidgetSelectionRange &range : ranges)
		{
			if (first < 0 || range.topRow() < first) {
				first = range.topRow();
			}
			if (range.bottomRow() > last) {
				last = range.bottomRow();
			}

			for (int column = range.leftColumn() ; column <= range.rightColumn() ; ++ column)
			{
				const QString name = columnAt(column);
				if (!name.isEmpty() && !names.contains(name)) {
					names << name;
				}
			}
		}
	}

	if (top) {
		*top = first;
	}
	if (bottom) {
		*bottom = last;
	}
	if (columns) {
		*columns = names;
	}
}

/**
	@brief CircuitTableDialog::addRow
*/
void CircuitTableDialog::addRow()
{
	QString macro_path = macroForNewRows();
	if (macro_path.isEmpty())
	{
		macro_path = chooseMacroPath();
		if (macro_path.isEmpty()) {
			return;
		}
	}

	QString error;
	if (!loadMacro(macro_path, &error))
	{
		say(error, true);
		return;
	}

	m_last_macro = macro_path;

	const int index = m_table.appendRow(macro_path);
	reload();

	if (index >= 0) {
		m_view->setCurrentCell(index, m_view->columnCount() > 1 ? 1 : 0);
	}
	say(QString());
}

/**
	@brief CircuitTableDialog::removeRows
*/
void CircuitTableDialog::removeRows()
{
	const QList<int> rows = selectedRowsOf(m_view);
	if (rows.isEmpty())
	{
		say(tr("Sélectionnez d'abord la ou les lignes à supprimer."), true);
		return;
	}

		//From the bottom up: taking the third row away first would move the
		//fourth into its place, and the next removal would take the fifth.
	for (int i = rows.count() - 1 ; i >= 0 ; -- i) {
		m_table.removeRow(rows.at(i));
	}

	reload();
	say(tr("%1 ligne(s) supprimée(s).").arg(rows.count()));
}

/**
	@brief CircuitTableDialog::chooseMacroForSelection
*/
void CircuitTableDialog::chooseMacroForSelection()
{
	const QString macro_path = chooseMacroPath();
	if (macro_path.isEmpty()) {
		return;
	}

	QString error;
	if (!loadMacro(macro_path, &error))
	{
		say(error, true);
		return;
	}

	m_last_macro = macro_path;

	const QList<int> rows = selectedRowsOf(m_view);
	if (rows.isEmpty())
	{
			//No row to give it to. The macro is remembered anyway: the next row
			//added, and the next paste that adds rows, start from it.
		reload();
		say(tr("Macro retenu pour les lignes à venir."));
		return;
	}

	for (const int row : rows) {
		m_table.setMacroPath(row, macro_path);
	}

	reload();
	say(tr("%1 ligne(s) sur le macro choisi.").arg(rows.count()));
}

/**
	@brief CircuitTableDialog::choosePartForCell
*/
void CircuitTableDialog::choosePartForCell()
{
	const int row = m_view->currentRow();
	const int view_column = m_view->currentColumn();
	const QString column = columnAt(view_column);

	if (row < 0 || column.isEmpty())
	{
		say(tr("Placez-vous d'abord sur la cellule à remplir."), true);
		return;
	}
	if (!QETApp::catalog())
	{
		say(tr("Le catalogue n'est pas ouvert."), true);
		return;
	}

	const CatalogPart part = CatalogBrowserDialog::choosePart(QETApp::catalog(), this);
	if (part.isNull()) {
		return;
	}

	QString error;
	if (!m_table.setValue(row, column, part.code, &error))
	{
		say(error, true);
		return;
	}

	m_loading = true;
	if (QTableWidgetItem *item = m_view->item(row, view_column)) {
		item->setText(part.code);
	}
	m_loading = false;

	say(QString());
}

/**
	@brief CircuitTableDialog::cellDoubleClicked
	@param row
	@param column
*/
void CircuitTableDialog::cellDoubleClicked(int row, int column)
{
	if (column != 0 || row < 0 || row >= m_table.rowCount()) {
		return;
	}

	const QString macro_path = chooseMacroPath();
	if (macro_path.isEmpty()) {
		return;
	}

	QString error;
	if (!loadMacro(macro_path, &error))
	{
		say(error, true);
		return;
	}

	m_last_macro = macro_path;
	m_table.setMacroPath(row, macro_path);

	reload();
	m_view->setCurrentCell(row, 0);
	say(QString());
}

/**
	@brief CircuitTableDialog::itemChanged
	@param item : the cell that was just written

	Where the view goes back into the table. A refusal puts the old value back
	and says why, instead of leaving on the screen a value the table does not
	hold - which is how a person generates a circuit they never asked for.
*/
void CircuitTableDialog::itemChanged(QTableWidgetItem *item)
{
	if (m_loading || !item || item->column() == 0) {
		return;
	}

	const QString column = columnAt(item->column());
	if (column.isEmpty()) {
		return;
	}

	const int row = item->row();
	const QString written = item->text();

		//The table refuses the list value it does not hold and the variable the
		//macro does not declare, but it takes any text for a Number: it has no
		//opinion on what a number looks like. The dialogue does, and it is the
		//same opinion the T06 dialogue holds, so 7,5 and 7.5 both pass.
	const MacroParameter parameter = m_table.parameterFor(row, column);
	if (parameter.type == MacroParameterType::Number
	    && !written.isEmpty()
	    && !MacroParametersDialog::looksLikeNumber(written))
	{
		say(tr("Ligne %1 : %2 attend un nombre.")
		    .arg(QString::number(row + 1), m_table.columnLabel(column)), true);
		m_loading = true;
		item->setText(m_table.value(row, column));
		m_loading = false;
		return;
	}

	QString error;
	if (!m_table.setValue(row, column, written, &error))
	{
		say(error, true);
		m_loading = true;
		item->setText(m_table.value(row, column));
		m_loading = false;
		return;
	}

	say(QString());
}

/**
	@brief CircuitTableDialog::selectionChanged
*/
void CircuitTableDialog::selectionChanged()
{
	updateButtons();
}

/**
	@brief CircuitTableDialog::pasteFromClipboard
*/
void CircuitTableDialog::pasteFromClipboard()
{
	const QString text = QApplication::clipboard()->text();
	if (text.isEmpty())
	{
		say(tr("Le presse-papiers ne contient pas de texte."), true);
		return;
	}

	const QString macro_for_new_rows = macroForNewRows();
	if (m_table.isEmpty() && macro_for_new_rows.isEmpty())
	{
		say(tr("Choisissez d'abord le macro des circuits à générer : "
		       "sans lui, la table ne sait pas dans quelles colonnes "
		       "le collage tombe."), true);
		return;
	}

	if (!macro_for_new_rows.isEmpty())
	{
		QString error;
		if (!loadMacro(macro_for_new_rows, &error))
		{
			say(error, true);
			return;
		}
	}

	const CircuitTable::PasteReport pasted =
			m_table.pasteTsv(text,
					 m_view->currentRow(),
					 columnAt(m_view->currentColumn()),
					 macro_for_new_rows);

	reload();
	say(pasted.text(), !pasted.ok || !pasted.refused.isEmpty());
}

/**
	@brief CircuitTableDialog::copyToClipboard
*/
void CircuitTableDialog::copyToClipboard()
{
	int top = -1;
	int bottom = -1;
	QStringList columns;
	selectedBlock(&top, &bottom, &columns);

	const QString text = m_table.copyTsv(top, bottom, columns, true);
	if (text.isEmpty())
	{
		say(tr("Il n'y a rien à copier."), true);
		return;
	}

	QApplication::clipboard()->setText(text);

	const int count = (top < 0 || bottom < 0)
			? m_table.rowCount()
			: bottom - top + 1;
	say(tr("%1 ligne(s) copiée(s) dans le presse-papiers.").arg(count));
}

/**
	@brief CircuitTableDialog::fillDown
*/
void CircuitTableDialog::fillDown()
{
	int top = -1;
	int bottom = -1;
	QStringList columns;
	selectedBlock(&top, &bottom, &columns);

	if (top < 0 || bottom <= top || columns.isEmpty())
	{
		say(tr("Sélectionnez la cellule à recopier et les lignes qui la suivent."), true);
		return;
	}

	int written = 0;
	QStringList refused;
	for (const QString &column : columns)
	{
		QString error;
		if (!m_table.canFillDown(top, bottom, column, &error))
		{
			refused << error;
			continue;
		}
		written += m_table.fillDown(top, bottom, column);
	}

	reload();
	m_view->setRangeSelected(QTableWidgetSelectionRange(top, 1, bottom,
							    m_view->columnCount() - 1), true);

	QStringList lines;
	lines << tr("%1 cellule(s) recopiée(s).").arg(written);
	lines << refused;
	say(lines.join(QLatin1Char('\n')), !refused.isEmpty());
}

/**
	@brief CircuitTableDialog::fillSeries
*/
void CircuitTableDialog::fillSeries()
{
	int top = -1;
	int bottom = -1;
	QStringList columns;
	selectedBlock(&top, &bottom, &columns);

	if (top < 0 || bottom <= top || columns.isEmpty())
	{
		say(tr("Sélectionnez la cellule qui commence la suite et les lignes "
		       "qui la suivent."), true);
		return;
	}

	int written = 0;
	QStringList refused;
	for (const QString &column : columns)
	{
		QString error;
		if (!m_table.canFillSeries(top, bottom, column, &error))
		{
			refused << error;
			continue;
		}
		written += m_table.fillSeries(top, bottom, column);
	}

	reload();
	m_view->setRangeSelected(QTableWidgetSelectionRange(top, 1, bottom,
							    m_view->columnCount() - 1), true);

	QStringList lines;
	lines << tr("%1 cellule(s) écrite(s).").arg(written);
	lines << refused;
	say(lines.join(QLatin1Char('\n')), !refused.isEmpty());
}

/**
	@brief CircuitTableDialog::generate
*/
void CircuitTableDialog::generate()
{
	if (!m_project)
	{
		say(tr("Aucun projet ouvert."), true);
		return;
	}
	if (m_table.isEmpty())
	{
		say(tr("La table ne contient aucun circuit."), true);
		return;
	}

		//Said before anything is drawn, and only about the rows that cannot be:
		//nineteen good circuits are not held hostage by the twentieth, but the
		//person is told which one is missing before the folios exist.
	const QList<CircuitTable::Problem> problems = m_table.problems();
	if (problems.count() == m_table.rowCount())
	{
		QStringList lines;
		for (const CircuitTable::Problem &problem : problems) {
			lines << problem.text();
		}
		say(lines.join(QLatin1Char('\n')), true);
		return;
	}

	if (!problems.isEmpty())
	{
		QStringList lines;
		for (const CircuitTable::Problem &problem : problems) {
			lines << problem.text();
		}

		const QMessageBox::StandardButton answer = QMessageBox::question(
					this,
					tr("Générer les circuits"),
					tr("%1 ligne(s) ne seront pas générées :\n\n%2\n\n"
					   "Générer les autres ?")
					.arg(QString::number(problems.count()),
					     lines.join(QLatin1Char('\n'))),
					QMessageBox::Yes | QMessageBox::No,
					QMessageBox::Yes);
		if (answer != QMessageBox::Yes) {
			return;
		}
	}

	CircuitGenerator::Options options;
	options.circuits_per_sheet = m_per_sheet->value();
	options.sheet_title = m_sheet_title->text().trimmed();

	CircuitGenerator generator(m_project);
	m_report = generator.generate(m_table, options);

	if (m_report.generated == 0)
	{
		QMessageBox::warning(this, tr("Générer les circuits"), m_report.text());
		say(m_report.text(), true);
		return;
	}

	QMessageBox::information(this, tr("Générer les circuits"), m_report.text());
	accept();
}
