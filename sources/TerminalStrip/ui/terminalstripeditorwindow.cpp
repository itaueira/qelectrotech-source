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
#include "ui_terminalstripeditorwindow.h"

#include "../UndoCommand/addterminalstripcommand.h"
#include "freeterminaleditor.h"
#include "../../label/labelcsvwriter.h"
#include "../../label/terminallabelcollector.h"
#include "../../qetapp.h"
#include "../../qetdiagrameditor.h"
#include "../../qetproject.h"
#include "../realterminal.h"
#include "../terminalstrip.h"
#include "terminalstripcreatordialog.h"
#include "terminalstripeditor.h"
#include "terminalstripeditorwindow.h"
#include "terminalstriptreedockwidget.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>

QPointer<TerminalStripEditorWindow> TerminalStripEditorWindow::window_;

static const int EMPTY_PAGE = 0;
static const int FREE_TERMINAL_PAGE = 1;
static const int TERMINAL_STRIP_PAGE = 2;

namespace
{
	/**
		@a text with every character that a file name cannot hold on one
		system or another replaced by '_', so that the name of a strip or
		of a project can go into the file name the save dialog suggests.
	*/
	QString fileNamePart(QString text)
	{
		static const QString forbidden = QStringLiteral("\\/:*?\"<>|");
		for (QChar &character : text)
		{
			if (forbidden.contains(character)) {
				character = QLatin1Char('_');
			}
		}
		return text;
	}
}

/**
 * @brief TerminalStripEditorWindow::TerminalStripEditorWindow
 * @param project
 * @param parent
 */
void TerminalStripEditorWindow::edit(TerminalStrip *strip)
{
	if (const auto project_ = strip->project())
	{
		auto editor_  = TerminalStripEditorWindow::instance(project_, QETApp::diagramEditor(project_));
		editor_->setCurrentStrip(strip);
		editor_->show();
	}
}

TerminalStripEditorWindow::TerminalStripEditorWindow(QETProject *project, QWidget *parent) :
	QMainWindow(parent),
    ui(new Ui::TerminalStripEditorWindow),
    m_project(project)
{
	ui->setupUi(this);
    if (auto diagram_editor = QETApp::diagramEditor(project)) {
        ui->m_tool_bar->addSeparator();
        ui->m_tool_bar->addAction(diagram_editor->undo);
        ui->m_tool_bar->addAction(diagram_editor->redo);
    }
	ui->m_remove_terminal->setDisabled(true);
	addTreeDockWidget();

	m_free_terminal_editor = new FreeTerminalEditor(m_project, this);
	m_terminal_strip_editor = new TerminalStripEditor{m_project, this};

	connect(m_tree_dock, &TerminalStripTreeDockWidget::currentStripChanged, this, &TerminalStripEditorWindow::currentStripChanged);
		//A move reloads the free terminal table, which would lose its unapplied edits
	m_tree_dock->setDropCheck([this]() {
		return !m_free_terminal_editor->hasPendingEdits();
	});

		//Both pages talk through the status bar of this window, which is
		//the least intrusive place that is still read: a refused click
		//does not deserve a modal box, and a click that did something
		//has to be told apart from a click that did nothing.
	connect(m_free_terminal_editor, &FreeTerminalEditor::message,
		this, &TerminalStripEditorWindow::showMessage);
	connect(m_terminal_strip_editor, &TerminalStripEditor::message,
		this, &TerminalStripEditorWindow::showMessage);

		//Said on the button that was used in place of the other one:
		//Apply commits what the table holds, and moving a terminal from
		//one strip to another is a different button, next to the list of
		//destinations.
	if (auto apply_button = ui->m_button_box->button(QDialogButtonBox::Apply)) {
		apply_button->setToolTip(tr("Enregistrer le type, la fonction et le "
					    "voyant des bornes affichées. Pour changer "
					    "une borne de bornier, utilisez le bouton "
					    "« Déplacer »."));
	}

	ui->m_stacked_widget->insertWidget(EMPTY_PAGE, new QWidget(ui->m_stacked_widget));
	ui->m_stacked_widget->insertWidget(FREE_TERMINAL_PAGE, m_free_terminal_editor);
	ui->m_stacked_widget->insertWidget(TERMINAL_STRIP_PAGE, m_terminal_strip_editor);
}

/**
 * @brief TerminalStripEditorWindow::showMessage
 * Show @a text in the status bar of this window.
 * @param text
 */
void TerminalStripEditorWindow::showMessage(const QString &text)
{
	if (!text.isEmpty()) {
		statusBar()->showMessage(text, 8000);
	}
}

/**
 * @brief TerminalStripEditorWindow::~TerminalStripEditorWindow
 */
TerminalStripEditorWindow::~TerminalStripEditorWindow()
{
	delete ui;
}

/**
 * @brief TerminalStripEditorWindow::setProject
 * @param project
 */
void TerminalStripEditorWindow::setProject(QETProject *project)
{
    m_project = project;
    m_tree_dock->setProject(project);
    m_free_terminal_editor->setProject(project);
    m_terminal_strip_editor->setProject(project);
}

void TerminalStripEditorWindow::setCurrentStrip(TerminalStrip *strip) {
	m_tree_dock->setSelectedStrip(strip);
}

/**
 * @brief TerminalStripEditorWindow::addTreeDockWidget
 */
void TerminalStripEditorWindow::addTreeDockWidget()
{
	m_tree_dock = new TerminalStripTreeDockWidget(m_project, this);
	m_tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	addDockWidget(Qt::LeftDockWidgetArea, m_tree_dock);
}

/**
 * @brief TerminalStripEditorWindow::currentStripChanged
 * @param strip
 */
void TerminalStripEditorWindow::currentStripChanged(TerminalStrip *strip)
{
	Q_UNUSED(strip)
	updateUi();

}

void TerminalStripEditorWindow::updateUi()
{
	ui->m_remove_terminal->setEnabled(m_tree_dock->currentIsStrip());

	ui->m_stacked_widget->setCurrentIndex(EMPTY_PAGE);

	if (auto real_terminal = m_tree_dock->currentRealTerminal())
	{
		if (!real_terminal->parentStrip())
		{
			ui->m_stacked_widget->setCurrentIndex(FREE_TERMINAL_PAGE);
			m_free_terminal_editor->reload();
		}
	} else if (auto strip_ = m_tree_dock->currentStrip()) {
		ui->m_stacked_widget->setCurrentIndex(TERMINAL_STRIP_PAGE);
		m_terminal_strip_editor->setCurrentStrip(strip_);
	}
}

/**
 * @brief TerminalStripEditorWindow::on_m_add_terminal_strip_triggered
 * Action when user click on add terminal strip button
 */
void TerminalStripEditorWindow::on_m_add_terminal_strip_triggered()
{
	QScopedPointer<TerminalStripCreatorDialog> dialog(new TerminalStripCreatorDialog(m_project, this));

	dialog->setLocation(m_tree_dock->currentLocation());
	dialog->setInstallation(m_tree_dock->currentInstallation());

	if (dialog->exec() == QDialog::Accepted)
	{
		auto ts = dialog->generatedTerminalStrip();
		m_project->undoStack()->push(new AddTerminalStripCommand(ts, m_project));

		m_tree_dock->reload();
		m_tree_dock->setSelectedStrip(ts);
	}
}

/**
 * @brief TerminalStripEditorWindow::on_m_remove_terminal_triggered
 */
void TerminalStripEditorWindow::on_m_remove_terminal_triggered()
{
	if (m_tree_dock->currentIsStrip())
	{
		if (auto strip_ = m_tree_dock->currentStrip())
		{
			m_project->undoStack()->push(new RemoveTerminalStripCommand(strip_, m_project));
			m_tree_dock->reload();
		}

	}
}

/**
 * @brief TerminalStripEditorWindow::on_m_reload_triggered
 */
void TerminalStripEditorWindow::on_m_reload_triggered() {
	m_tree_dock->reload();
	m_terminal_strip_editor->reload();
	m_free_terminal_editor->reload();
}

/**
 * @brief TerminalStripEditorWindow::exportTerminalList
 * See the declaration. Nothing here formats a cell: the collector reads
 * the strips, the writer writes the file, and this only chooses between
 * one strip and every strip - which is why the file of every strip is the
 * file of the command line.
 */
QString TerminalStripEditorWindow::exportTerminalList(QETProject *project,
						      TerminalStrip *strip,
						      const QString &path,
						      int *written)
{
	if (written) {
		*written = 0;
	}

	LabelEntryList entries;
	if (strip) {
		entries = TerminalLabelCollector::collectStrip(strip);
	}
	else
	{
			//Every strip, in the order the project holds them - not in the
			//order the tree of this window lists them, which is sorted by
			//name: the command line follows the project, and the two files
			//have to be the same file.
		TerminalLabelCollector collector(project);
		entries = collector.collect();
		if (!collector.error().isEmpty()) {
			return collector.error();
		}
	}

	const QString failure = LabelCsvWriter::write(entries, path);
	if (!failure.isEmpty()) {
		return failure;
	}

	if (written) {
		*written = int(entries.size());
	}
	return QString();
}

/**
 * @brief TerminalStripEditorWindow::on_m_export_terminal_list_triggered
 * Write the terminal list of the strip selected in the tree, or of every
 * strip, to a CSV file the user chooses.
 *
 * How many rows were written, and where, is said in the status bar, where
 * the two pages already talk. A file that could not be written is said in
 * a box instead: the user asked for that file and will go looking for it,
 * and a line that fades out of the status bar is too easily missed.
 */
void TerminalStripEditorWindow::on_m_export_terminal_list_triggered()
{
	if (!m_project) {
		return;
	}

		//The strip of the selected node: the strip itself, or the strip of a
		//selected terminal - the strip the editor beside the tree is about.
		//Any other node - the project, an installation, a location, the
		//independent terminals - is not one strip, and gives every strip.
	TerminalStrip *strip = m_tree_dock->currentStrip();

	QString dir = m_project->currentDir();
	if (dir.isEmpty()) {
		dir = QETApp::documentDir();
	}
	const QString suggested = dir + QLatin1Char('/')
			+ tr("terminal_list_")
			+ fileNamePart(strip ? strip->name() : m_project->title())
			+ QStringLiteral(".csv");

	const QString path = QFileDialog::getSaveFileName(
				 this, tr("Export the terminal list"), suggested,
				 tr("CSV files (*.csv)"));
	if (path.isEmpty()) {
		return;
	}

	int written = 0;
	const QString failure = exportTerminalList(m_project, strip, path,
						   &written);
	const QString shown_path = QDir::toNativeSeparators(path);
	if (!failure.isEmpty())
	{
		QMessageBox::warning(this, tr("Export the terminal list"),
				     tr("The terminal list could not be written to %1.\n%2")
				     .arg(shown_path, failure));
		return;
	}

	if (strip) {
		showMessage(tr("%n terminal(s) of %1 exported to %2", nullptr, written)
			    .arg(strip->name(), shown_path));
	} else {
		showMessage(tr("%n terminal(s) of every strip exported to %1", nullptr, written)
			    .arg(shown_path));
	}
}

/**
 * @brief TerminalStripEditorWindow::on_m_button_box_clicked
 * Action when user click on the apply/reset button
 * @param button
 */
void TerminalStripEditorWindow::on_m_button_box_clicked(QAbstractButton *button)
{
	auto role_{ui->m_button_box->buttonRole(button)};

	if (role_ == QDialogButtonBox::ApplyRole)
	{
		switch (ui->m_stacked_widget->currentIndex()) {
			case FREE_TERMINAL_PAGE:
				m_free_terminal_editor->apply();
				break;
			case TERMINAL_STRIP_PAGE:
				m_terminal_strip_editor->apply();
				break;
			default:
				break;
		}
	}
	else if (role_ == QDialogButtonBox::ResetRole)
	{
		m_terminal_strip_editor->reload();
		m_free_terminal_editor->reload();
	}
}


void TerminalStripEditorWindow::on_m_stacked_widget_currentChanged(int arg1) {
	ui->m_button_box->setHidden(arg1 == EMPTY_PAGE);
}
