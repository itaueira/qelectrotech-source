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
#ifndef TERMINALSTRIPEDITORWINDOW_H
#define TERMINALSTRIPEDITORWINDOW_H

#include <QMainWindow>
#include <QMutex>
#include <QPointer>

class QETProject;
class TerminalStripTreeDockWidget;
class TerminalStrip;
class FreeTerminalEditor;
class TerminalStripEditor;
class QAbstractButton;

namespace Ui {
	class TerminalStripEditorWindow;
}

class TerminalStripEditorWindow : public QMainWindow
{
		Q_OBJECT

	private:
			//We need to use a QPointer instead of a raw pointer because when window_
			//have got a parent widget, the parent widget can delete the window_
			//instance in her destrucor and then window_ become a dangling pointer.
		static QPointer<TerminalStripEditorWindow> window_;

    public:
		static TerminalStripEditorWindow* instance(QETProject *project, QWidget *parent = nullptr);

		static void dropInstance () {
			static QMutex mutex;
			if (window_) {
				mutex.lock();
				window_->deleteLater();
				window_.clear();
				mutex.unlock();
			}
		}

		static void edit(TerminalStrip *strip);

		/**
			@brief Write the terminal list of @a strip, or of every strip
			of @a project when @a strip is null, to the CSV file at @a path.

			What the "Export the terminal list" action of this window
			writes, kept out of the window so that it can be run - and
			proved - without one: the window reaches QETApp as soon as it
			is built.

			The same collector and the same writer as
			--export-labels <project> <file> kinds=terminal, and nothing
			else in between: TerminalLabelCollector::collect() for every
			strip, in the order the project holds them, or
			TerminalLabelCollector::collectStrip() for one, and then
			LabelCsvWriter::write(). So with @a strip null the file is that
			command's file, byte for byte, for the same project; with a
			strip it is the same header followed by the rows of that strip
			alone, in the order of its rail.

			What is written is what the strip holds - what was applied.
			An edit still pending in the table of the strip editor is not
			on the strip yet, and is not in the file.

			@param project : read only when @a strip is null.
			@param strip : the strip to write, or null for every strip.
			@param path : the file to write.
			@param written : when not null, set to the number of rows
			written - one per level of each terminal, as the command line
			counts them -, and to 0 when nothing was written.
			@return an empty string when the file was written, else the
			reason, ready to be shown. On a failure no file is left at
			@a path: the writer goes through QSaveFile.
		*/
		static QString exportTerminalList(QETProject *project,
						  TerminalStrip *strip,
						  const QString &path,
						  int *written = nullptr);

	public:
		explicit TerminalStripEditorWindow(QETProject *project, QWidget *parent = nullptr);
		~TerminalStripEditorWindow();

        void setProject(QETProject *project);
        void setCurrentStrip(TerminalStrip *strip);

	private slots:
		void on_m_add_terminal_strip_triggered();
		void on_m_remove_terminal_triggered();
		void on_m_reload_triggered();
		void on_m_export_terminal_list_triggered();
		void on_m_button_box_clicked(QAbstractButton *button);
		void on_m_stacked_widget_currentChanged(int arg1);

	private:
		void addTreeDockWidget();
		void currentStripChanged(TerminalStrip *strip);
		void updateUi();
		/**
			@brief Show @a text in the status bar of this window.

			The two editor pages have no status bar of their own,
			and a refused click does not deserve a modal box: they
			emit one sentence, and this is where it is shown.
		*/
		void showMessage(const QString &text);

	private:
		Ui::TerminalStripEditorWindow *ui{nullptr};
        QPointer <QETProject> m_project;
		TerminalStripTreeDockWidget *m_tree_dock{nullptr};
		FreeTerminalEditor *m_free_terminal_editor {nullptr};
		TerminalStripEditor *m_terminal_strip_editor {nullptr};
};

#endif // TERMINALSTRIPEDITORWINDOW_H
