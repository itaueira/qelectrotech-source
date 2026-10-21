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
#include "assemblychangesdialog.h"
#include "assemblywalk.h"

#include "../autoNum/projectrenumberer.h"
#include "../conductorproperties.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../diagramposition.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "../wirecolorname.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <utility>

namespace
{
	/// The columns of the list.
	enum Column
	{
		LabelColumn = 0,
		FolioColumn,
		PositionColumn,
		DescriptionColumn,
		PartColumn,
		ColumnCount
	};

	/// What a row of the list holds: its line in AssemblyChanges::all(), or
	/// -1 for the heading of a group.
	const int LINE_ROLE = Qt::UserRole + 1;

	/**
		The words of a group of lines, in the language of the reader. The
		keys of the file are fixed and English (AssemblyChanges::actionKey);
		these are not keys, they are what a person reads.
	*/
	QString groupTitle(AssemblyItemKind kind, AssemblyAction action, int count)
	{
		const bool component = kind == AssemblyItemKind::Component;
		switch (action)
		{
			case AssemblyAction::Add:
				return component
					? AssemblyChangesDialog::tr("Components to add (%1)")
					  .arg(count)
					: AssemblyChangesDialog::tr("Wires to add (%1)")
					  .arg(count);
			case AssemblyAction::Remove:
				return component
					? AssemblyChangesDialog::tr("Components to remove (%1)")
					  .arg(count)
					: AssemblyChangesDialog::tr("Wires to remove (%1)")
					  .arg(count);
			case AssemblyAction::Relabel:
				return component
					? AssemblyChangesDialog::tr("Components to relabel (%1)")
					  .arg(count)
					: AssemblyChangesDialog::tr("Wires to renumber (%1)")
					  .arg(count);
		}
		return QString();
	}

	/// What the label column of a line says.
	QString labelText(const AssemblyChange &change)
	{
		switch (change.action)
		{
			case AssemblyAction::Add:
				return change.new_label;
			case AssemblyAction::Remove:
				return change.old_label;
			case AssemblyAction::Relabel:
				return QStringLiteral("%1 → %2")
					.arg(change.old_label, change.new_label);
		}
		return QString();
	}
}

/**
	@brief AssemblyChangesDialog::AssemblyChangesDialog
	@param project : the project whose changes are listed
	@param parent
*/
AssemblyChangesDialog::AssemblyChangesDialog(QETProject *project,
					     QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Changes since assembly", "window title"));
	setMinimumSize(720, 440);
	setUpWidget();
	refresh();
}

/**
	@brief AssemblyChangesDialog::compute
	@param project
	@return what changed on @a project since it was marked
*/
AssemblyChanges AssemblyChangesDialog::compute(QETProject *project)
{
	if (!project) {
		return AssemblyChanges();
	}
	const AssemblyWalk walked = AssemblyWalk::of(project);
	return AssemblyChanges::compare(project->assemblyState(),
					walked.components, walked.wires);
}

/**
	@brief AssemblyChangesDialog::goTo
	@param index : a line of changes().all()
	@return whether it went anywhere

	The four lines DrcPanel uses, and not the fifth: the window stays where
	it is, because whoever activated a line wants to see the item **and**
	to carry on down the list.
*/
bool AssemblyChangesDialog::goTo(int index)
{
	const QList<AssemblyChange> lines = m_changes.all();
	if (index < 0 || index >= lines.size()) {
		return false;
	}

	const AssemblyChange &change = lines.at(index);
	if (change.source < 0) {
			//A removal: drawn on no folio any more, so there is nowhere to
			//go, and opening some folio would send the reader to a place
			//the line never mentioned.
		return false;
	}

	if (change.kind == AssemblyItemKind::Component)
	{
		Element *element = m_components.value(change.source).data();
		Diagram *sheet = element ? element->diagram() : nullptr;
		if (!sheet) {
				//Deleted since the list was made: Refresh says so.
			return false;
		}
		sheet->showMe();
		sheet->clearSelection();
		element->setSelected(true);
		element->ensureVisible();
		return true;
	}

	Conductor *wire = m_wires.value(change.source).data();
	Diagram *sheet = wire ? wire->diagram() : nullptr;
	if (!sheet) {
		return false;
	}
	sheet->showMe();
	sheet->clearSelection();
	wire->setSelected(true);
	wire->ensureVisible();
	return true;
}

/**
	@brief AssemblyChangesDialog::exportTo
	@param path
	@return an empty string when the file was written, else the reason
*/
QString AssemblyChangesDialog::exportTo(const QString &path) const
{
	if (!m_changes.isMarked()) {
			//No photograph, no "since": a file with a header and nothing
			//under it would read as "nothing to do", and that is not what
			//an unmarked project means.
		return tr("This project is not marked as assembled.");
	}
	return m_changes.writeCsv(path);
}

/**
	@brief AssemblyChangesDialog::refresh
	Walk the project again: the drawing may have changed under the window,
	and a list that does not follow it is an order of service for a panel
	that no longer exists.
*/
void AssemblyChangesDialog::refresh()
{
	const AssemblyWalk walked = AssemblyWalk::of(m_project.data());
	m_components = walked.component_objects;
	m_wires = walked.wire_objects;
	m_changes = m_project
		    ? AssemblyChanges::compare(m_project->assemblyState(),
					       walked.components, walked.wires)
		    : AssemblyChanges();
	if (m_status) {
		m_status->clear();
	}
	fill();
}

/**
	@brief AssemblyChangesDialog::setUpWidget
*/
void AssemblyChangesDialog::setUpWidget()
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(ColumnCount);
	m_tree->setHeaderLabels(QStringList()
				<< tr("Label")
				<< tr("Folio")
				<< tr("Position")
				<< tr("Description")
				<< tr("Part code"));
	m_tree->setRootIsDecorated(true);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setToolTip(tr("Double-click a line to show the item on its "
			      "folio."));
	layout->addWidget(m_tree);

	m_note = new QLabel(this);
	m_note->setWordWrap(true);
	layout->addWidget(m_note);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	layout->addWidget(m_status);

	QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close,
						      this);
	QPushButton *refresh_button = box->addButton(tr("Refresh"),
						     QDialogButtonBox::ActionRole);
	refresh_button->setAutoDefault(false);
	m_export = box->addButton(tr("Export CSV…"),
				  QDialogButtonBox::ActionRole);
	m_export->setAutoDefault(false);
	m_export->setToolTip(tr("Write this list to a file for the workshop."));
	layout->addWidget(box);

		//activated and not doubleClicked: it fires on Enter as well, so the
		//list can be worked through from the keyboard - the same reason
		//DrcPanel gives.
	connect(m_tree, &QTreeWidget::itemActivated,
		this, &AssemblyChangesDialog::rowActivated);
	connect(refresh_button, &QPushButton::clicked,
		this, &AssemblyChangesDialog::refresh);
	connect(m_export, &QPushButton::clicked,
		this, &AssemblyChangesDialog::exportCsv);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	if (m_project) {
			//The window outlives nothing it points at: a project closed
			//under it takes the window with it.
		connect(m_project.data(), &QObject::destroyed,
			this, &QDialog::reject);
	}
}

/**
	@brief AssemblyChangesDialog::fill
	Six groups, always: an empty group says "nothing of this kind to do",
	which is part of the order of service, and a group that vanished would
	leave the reader asking whether it was ever looked at.
*/
void AssemblyChangesDialog::fill()
{
	m_tree->clear();

	if (!m_project)
	{
		m_summary->clear();
		m_note->clear();
		m_export->setEnabled(false);
		return;
	}

	if (!m_changes.isMarked())
	{
		m_summary->setText(
			tr("This project is not marked as assembled: there is no "
			   "photograph of the panel to compare the drawing with."));
		m_note->clear();
		m_export->setEnabled(false);
		return;
	}

	const QString stage = AssemblyState::translatedStage(m_changes.stage);
	if (m_changes.isEmpty()) {
		m_summary->setText(tr("Nothing has changed since the project was "
				      "marked as %1.").arg(stage));
	} else {
		m_summary->setText(tr("%n change(s) since the project was marked "
				      "as %1.", "", m_changes.count())
				   .arg(stage));
	}
	m_export->setEnabled(true);

	int line = 0;
	const QList<std::pair<AssemblyItemKind, const AssemblyChangeLists *>> kinds {
		{AssemblyItemKind::Component, &m_changes.components},
		{AssemblyItemKind::Wire, &m_changes.wires}};

	for (const auto &kind : kinds)
	{
		const QList<std::pair<AssemblyAction, const QList<AssemblyChange> *>> groups {
			{AssemblyAction::Add, &kind.second->to_add},
			{AssemblyAction::Remove, &kind.second->to_remove},
			{AssemblyAction::Relabel, &kind.second->to_relabel}};

		for (const auto &group : groups)
		{
			const QList<AssemblyChange> &changes = *group.second;

			QTreeWidgetItem *heading = new QTreeWidgetItem(m_tree);
			heading->setText(LabelColumn, groupTitle(kind.first,
								 group.first,
								 int(changes.size())));
			heading->setData(LabelColumn, LINE_ROLE, -1);
			heading->setFirstColumnSpanned(true);

				//The rows are made in the order of all(), so the running
				//count is the line each of them stands for.
			for (const AssemblyChange &change : changes)
			{
				QTreeWidgetItem *row = new QTreeWidgetItem(heading);
				row->setText(LabelColumn, labelText(change));
				row->setText(FolioColumn, change.folio > 0
							  ? QString::number(change.folio)
							  : QString());
				row->setText(PositionColumn, change.position);
				row->setText(DescriptionColumn, change.description);
				row->setText(PartColumn, change.part_code);
				row->setData(LabelColumn, LINE_ROLE, line);
				if (change.source < 0) {
					row->setToolTip(LabelColumn,
						tr("Drawn on no folio any more: take it "
						   "out of the panel."));
				}
				++line;
			}
		}
	}

	m_tree->expandAll();
	for (int column = 0 ; column < ColumnCount ; ++column) {
		m_tree->resizeColumnToContents(column);
	}

	if (!m_changes.components.to_remove.isEmpty()
	    || !m_changes.wires.to_remove.isEmpty())
	{
		m_note->setText(
			tr("An item to remove is named by the label it carried: the "
			   "photograph taken when the project was marked records "
			   "nothing else about it - neither its folio nor, for a "
			   "wire, its two ends."));
	}
	else {
		m_note->clear();
	}
}

/**
	@brief AssemblyChangesDialog::rowActivated
	@param item
	@param column
*/
void AssemblyChangesDialog::rowActivated(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column)
	if (!item) {
		return;
	}
	bool ok = false;
	const int line = item->data(LabelColumn, LINE_ROLE).toInt(&ok);
	if (ok && line >= 0) {
		goTo(line);
	}
}

/**
	@brief AssemblyChangesDialog::exportCsv
	Ask where, then write. The whole list, always: an order of service
	handed over with lines missing produces a panel with changes missing,
	and nothing in the document that lacks them says they were ever there.
*/
void AssemblyChangesDialog::exportCsv()
{
	QString suggested;
	if (m_project)
	{
		QString base = QFileInfo(m_project->filePath()).completeBaseName();
		if (base.isEmpty()) {
			base = m_project->title();
		}
		if (base.isEmpty()) {
			base = QStringLiteral("project");
		}
		suggested = QDir(m_project->currentDir())
			    .filePath(tr("%1 - changes since assembly.csv")
				      .arg(base));
	}

	const QString path = QFileDialog::getSaveFileName(
				     this,
				     tr("Export the changes since assembly"),
				     suggested,
				     tr("CSV file (*.csv)"));
	if (path.isEmpty()) {
		return;
	}

	const QString reason = exportTo(path);
	if (!reason.isEmpty())
	{
		QMessageBox::warning(this, tr("Changes since assembly"),
				     tr("The file \"%1\" could not be written: %2")
				     .arg(QDir::toNativeSeparators(path), reason));
		return;
	}
	m_status->setText(tr("%n line(s) written to \"%1\".", "",
			     m_changes.count())
			  .arg(QDir::toNativeSeparators(path)));
}
