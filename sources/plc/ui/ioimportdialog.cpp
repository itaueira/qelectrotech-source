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
#include "ioimportdialog.h"

#include "../../qetproject.h"
#include "../../catalog/catalogtable.h"
#include "../../catalog/catalogtablereader.h"
#include "../../macro/circuitclipboard.h"
#include "../../undocommand/importiopointscommand.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
	/// How many rows of the sheet the preview shows.
	const int PREVIEW_ROWS = 12;
	/// Role holding the field a mapping row is about.
	const int FIELD_ROLE = Qt::UserRole + 1;

	/**
		@param column : zero based
		@return the letter a spreadsheet gives that column: A, B … Z, AA, AB

		The person mapping the columns is reading the sheet in the program
		that produced it, and that program numbers the columns this way. Row
		numbers get the same treatment further down, for the same reason.
	*/
	QString spreadsheetLetter(int column)
	{
		QString letter;
		int rest = column;
		while (rest >= 0)
		{
			letter.prepend(QChar('A' + (rest % 26)));
			rest = rest / 26 - 1;
		}
		return letter;
	}
}

/**
	@brief IoImportDialog::IoImportDialog
	@param project the project the points go into
	@param parent
*/
IoImportDialog::IoImportDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Importer une liste d'entrées / sorties"));
	buildWidgets();
	reloadPreview();
	reloadMappingTable();
	reloadSummary();
	updateEnabledState();
}

/**
	@brief IoImportDialog::report
	@return what the last import did
*/
IoList::MergeReport IoImportDialog::report() const
{
	return m_report;
}

/**
	@brief IoImportDialog::buildWidgets
*/
void IoImportDialog::buildWidgets()
{
	m_paste = new QPushButton(tr("Coller depuis le presse-papiers"), this);
	m_paste->setToolTip(tr("Sélectionner les lignes dans le tableur, les copier, "
			       "puis cliquer ici."));
	connect(m_paste, &QPushButton::clicked, this, &IoImportDialog::pasteFromClipboard);

	m_open = new QPushButton(tr("Ouvrir un fichier…"), this);
	connect(m_open, &QPushButton::clicked, this, &IoImportDialog::openFile);

	m_has_header = new QCheckBox(tr("La première ligne donne le nom des colonnes"), this);
	connect(m_has_header, &QCheckBox::toggled, this, &IoImportDialog::headerToggled);

	QHBoxLayout *source = new QHBoxLayout();
	source->addWidget(m_paste);
	source->addWidget(m_open);
	source->addWidget(m_has_header);
	source->addStretch();

	m_preview = new QTableWidget(this);
	m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_preview->setSelectionMode(QAbstractItemView::NoSelection);
	m_preview->setAlternatingRowColors(true);

	m_columns = new QTableWidget(this);
	m_columns->setColumnCount(2);
	m_columns->setHorizontalHeaderLabels(QStringList()
					     << tr("Champ") << tr("Colonne de la feuille"));
	m_columns->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_columns->setSelectionMode(QAbstractItemView::NoSelection);
	m_columns->verticalHeader()->hide();
	m_columns->horizontalHeader()->setStretchLastSection(true);

	QGroupBox *preview_box = new QGroupBox(tr("Ce qui a été lu"), this);
	QVBoxLayout *preview_layout = new QVBoxLayout(preview_box);
	preview_layout->addWidget(m_preview);

	QGroupBox *columns_box = new QGroupBox(tr("Correspondance des colonnes"), this);
	QVBoxLayout *columns_layout = new QVBoxLayout(columns_box);
	columns_layout->addWidget(m_columns);

	QHBoxLayout *middle = new QHBoxLayout();
	middle->addWidget(preview_box, 3);
	middle->addWidget(columns_box, 2);

	m_leftover = new QLabel(this);
	m_leftover->setWordWrap(true);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

	m_import = new QPushButton(tr("Importer"), this);
	connect(m_import, &QPushButton::clicked, this, &IoImportDialog::runImport);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	buttons->addButton(m_import, QDialogButtonBox::ActionRole);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *root = new QVBoxLayout(this);
	root->addLayout(source);
	root->addLayout(middle, 1);
	root->addWidget(m_leftover);
	root->addWidget(m_summary);
	root->addWidget(m_status);
	root->addWidget(buttons);

	resize(940, 560);

	say(tr("Collez la feuille ou ouvrez un fichier. Rien n'est écrit dans le projet "
	       "avant que vous cliquiez sur Importer."));
}

/**
	@brief IoImportDialog::pasteFromClipboard
*/
void IoImportDialog::pasteFromClipboard()
{
	const QString text = QApplication::clipboard()->text();
	if (text.trimmed().isEmpty())
	{
		say(tr("Le presse-papiers ne contient pas de texte."), true);
		return;
	}
	setGrid(CircuitClipboard::parse(text), tr("le presse-papiers"));
}

/**
	@brief IoImportDialog::openFile
*/
void IoImportDialog::openFile()
{
	const QString path = QFileDialog::getOpenFileName(
		this, tr("Ouvrir une liste d'entrées / sorties"), QString(),
		tr("Feuilles de calcul (*.csv *.txt *.xlsx);;Tous les fichiers (*)"));
	if (path.isEmpty()) {
		return;
	}

	QString error;
	const CatalogTable table = CatalogTableReader::read(path, QChar(), &error);
	if (!error.isEmpty())
	{
		QMessageBox::warning(this, tr("Fichier non lu"), error);
		say(error, true);
		return;
	}

		//CatalogTableReader splits the first row off as the header, which is
		//a decision this import has not made yet: a two column sheet has no
		//header at all, and taking its first line for one would lose a point
		//without saying so. Putting the row back is what leaves the decision
		//where it belongs, in IoSheet::mappingFor.
	QList<QStringList> grid;
	if (!table.headers.isEmpty()) {
		grid.append(table.headers);
	}
	grid.append(table.rows);

	setGrid(grid, QFileInfo(path).fileName());
}

/**
	@brief IoImportDialog::setGrid
	@param grid what was read
	@param origin where it came from, for the message

	The mapping is guessed here and only here. Afterwards it belongs to the
	person: rebuilding the table on every keystroke would undo, silently, a
	column somebody corrected by hand.
*/
void IoImportDialog::setGrid(const QList<QStringList> &grid, const QString &origin)
{
	m_grid = grid;
	m_mapping = IoSheet::mappingFor(m_grid);

	m_loading = true;
	m_has_header->setChecked(m_mapping.has_header);
	m_loading = false;

	reloadPreview();
	reloadMappingTable();
	reloadSummary();
	updateEnabledState();

	if (m_grid.isEmpty())
	{
		say(tr("Rien de lisible dans %1.").arg(origin), true);
		return;
	}

	say(tr("%n ligne(s) lue(s) depuis %1.", "", int(m_grid.size())).arg(origin));
}

/**
	@brief IoImportDialog::headerToggled
	@param checked

	Only whether the first row is data changes. Which column feeds which
	field is left alone, because a person who unticks the box to look at the
	first row does not expect to lose the mapping while doing it.
*/
void IoImportDialog::headerToggled(bool checked)
{
	if (m_loading) {
		return;
	}
	m_mapping.has_header = checked;
	reloadPreview();
	reloadMappingTable();
	reloadSummary();
	updateEnabledState();
}

/**
	@brief IoImportDialog::mappingChanged
*/
void IoImportDialog::mappingChanged()
{
	if (m_loading) {
		return;
	}
	readMappingTable();
	reloadMappingTable();
	reloadSummary();
	updateEnabledState();
}

/**
	@brief IoImportDialog::reloadPreview

	The vertical header carries the sheet's own row numbers, header row
	included. That is what makes "row 25 was blank" mean the same thing here
	and in the spreadsheet the person is going to go and look at.
*/
void IoImportDialog::reloadPreview()
{
	m_preview->clear();

	int columns = 0;
	for (int row = 0 ; row < m_grid.size() ; ++row) {
		columns = qMax(columns, int(m_grid.at(row).size()));
	}
	m_preview->setColumnCount(columns);

	QStringList headers;
	for (int column = 0 ; column < columns ; ++column) {
		headers << columnLabel(column);
	}
	m_preview->setHorizontalHeaderLabels(headers);

	const int shown = qMin(PREVIEW_ROWS, int(m_grid.size()));
	m_preview->setRowCount(shown);

	QStringList row_names;
	for (int row = 0 ; row < shown ; ++row)
	{
		row_names << QString::number(row + 1);
		const QStringList &cells = m_grid.at(row);
		for (int column = 0 ; column < columns ; ++column)
		{
			const QString cell = column < cells.size() ? cells.at(column) : QString();
			QTableWidgetItem *item = new QTableWidgetItem(cell);
			if (row == 0 && m_mapping.has_header)
			{
				QFont bold = item->font();
				bold.setBold(true);
				item->setFont(bold);
			}
			m_preview->setItem(row, column, item);
		}
	}
	m_preview->setVerticalHeaderLabels(row_names);
	m_preview->resizeColumnsToContents();
}

/**
	@brief IoImportDialog::reloadMappingTable
	One row per field a column may feed, so what can be mapped is what the
	point actually carries.
*/
void IoImportDialog::reloadMappingTable()
{
	m_loading = true;

	const QList<IoField> fields = IoSheet::mappableFields();
	m_columns->setRowCount(int(fields.size()));

	int columns = 0;
	for (int row = 0 ; row < m_grid.size() ; ++row) {
		columns = qMax(columns, int(m_grid.at(row).size()));
	}

	for (int row = 0 ; row < fields.size() ; ++row)
	{
		const IoField field = fields.at(row);

		QTableWidgetItem *name = new QTableWidgetItem(IoSheet::fieldName(field));
		name->setData(FIELD_ROLE, int(field));
		m_columns->setItem(row, 0, name);

		QComboBox *choice = new QComboBox(m_columns);
		choice->addItem(tr("(ne pas importer)"), -1);
		for (int column = 0 ; column < columns ; ++column) {
			choice->addItem(columnLabel(column), column);
		}

		const int mapped = choice->findData(m_mapping.columnOf(field));
		choice->setCurrentIndex(mapped >= 0 ? mapped : 0);
		connect(choice, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &IoImportDialog::mappingChanged);
		m_columns->setCellWidget(row, 1, choice);
	}

	m_columns->resizeColumnsToContents();
	m_loading = false;

		//Which columns nothing reads, said by name and before anything is
		//written: a column the sheet carries and no field claims has nowhere
		//else to appear, and it is usually the sign of a wrong mapping.
	QSet<int> used;
	const QList<IoField> mapped_fields = m_mapping.mappedFields();
	for (int index = 0 ; index < mapped_fields.size() ; ++index) {
		used.insert(m_mapping.columnOf(mapped_fields.at(index)));
	}

	QStringList leftover;
	for (int column = 0 ; column < columns ; ++column)
	{
		if (!used.contains(column)) {
			leftover << columnLabel(column);
		}
	}

	m_leftover->setText(leftover.isEmpty()
			    ? QString()
			    : tr("Colonnes qu'aucun champ ne lit : %1")
			      .arg(leftover.join(QStringLiteral(", "))));
}

/**
	@brief IoImportDialog::readMappingTable
	Take back into m_mapping what the combo boxes say.
*/
void IoImportDialog::readMappingTable()
{
	IoSheet::Mapping mapping;
	mapping.has_header = m_mapping.has_header;

	for (int row = 0 ; row < m_columns->rowCount() ; ++row)
	{
		const QTableWidgetItem *name = m_columns->item(row, 0);
		QComboBox *choice = qobject_cast<QComboBox *>(m_columns->cellWidget(row, 1));
		if (!name || !choice) {
			continue;
		}

		const int column = choice->currentData().toInt();
		if (column < 0) {
			continue;
		}
		mapping.setColumn(IoField(name->data(FIELD_ROLE).toInt()), column);
	}

	m_mapping = mapping;
}

/**
	@brief IoImportDialog::readSheet
	@return the points, and everything about the sheet that was not plain
	sailing
*/
IoSheet::Report IoImportDialog::readSheet() const
{
	return IoSheet::read(m_grid, m_mapping);
}

/**
	@brief IoImportDialog::reloadSummary
	Run the merge against a copy and show what it would do.

	This is the whole point of the dialogue. The second import is the
	dangerous one - the list is revised many times during a project, and by
	then points have been assigned and drawn - so what the merge would do has
	to be readable before it is done, not explained afterwards.
*/
void IoImportDialog::reloadSummary()
{
	if (m_grid.isEmpty() || m_mapping.isEmpty())
	{
		m_summary->setText(QString());
		return;
	}

	const IoSheet::Report read = readSheet();

	IoList preview = m_project ? m_project->ioList() : IoList();
	const IoList::MergeReport merge = preview.merge(read.points, m_mapping.fields());

	QStringList lines;
	lines << read.text();
	if (!merge.isEmpty()) {
		lines << merge.text();
	}
	m_summary->setText(lines.join(QStringLiteral("\n")));
}

/**
	@brief IoImportDialog::updateEnabledState
*/
void IoImportDialog::updateEnabledState()
{
	const bool readable = !m_grid.isEmpty() && !m_mapping.isEmpty();
	m_import->setEnabled(readable && m_project && !m_project->isReadOnly());
}

/**
	@brief IoImportDialog::runImport
*/
void IoImportDialog::runImport()
{
	if (!m_project || m_project->isReadOnly()) {
		return;
	}

	const IoSheet::Report read = readSheet();
	if (read.points.isEmpty())
	{
		say(tr("Aucun point n'a pu être lu : vérifiez la correspondance des colonnes."),
		    true);
		return;
	}

	IoList list = m_project->ioList();
	m_report = list.merge(read.points, m_mapping.fields());

	const int touched = int(m_report.added.size() + m_report.updated.size());
	if (touched == 0)
	{
		say(tr("La feuille ne change rien : les %n point(s) lu(s) sont déjà dans le "
		       "projet, à l'identique.", "", int(read.points.size())));
		return;
	}

	if (QUndoStack *stack = m_project->undoStack()) {
		stack->push(new ImportIoPointsCommand(m_project, list, touched));
	} else {
		m_project->setIoList(list);
	}

	say(m_report.text());
	accept();
}

/**
	@brief IoImportDialog::say
	@param message
	@param problem : true to write it in red
*/
void IoImportDialog::say(const QString &message, bool problem)
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
	@brief IoImportDialog::columnLabel
	@param column : zero based
	@return the letter, followed by the header when the sheet gives one
*/
QString IoImportDialog::columnLabel(int column) const
{
	const QString letter = spreadsheetLetter(column);
	if (!m_mapping.has_header || m_grid.isEmpty()) {
		return letter;
	}

	const QStringList &header = m_grid.first();
	if (column >= header.size() || header.at(column).trimmed().isEmpty()) {
		return letter;
	}

	return tr("%1 — %2", "column letter, then its name in the sheet")
	       .arg(letter, header.at(column).trimmed());
}
