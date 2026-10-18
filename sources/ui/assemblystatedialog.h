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
#ifndef ASSEMBLYSTATEDIALOG_H
#define ASSEMBLYSTATEDIALOG_H

#include <QDialog>

#include "../autoNum/assemblystate.h"

class AssemblyChangesDialog;
class QComboBox;
class QETProject;
class QLabel;
class QPushButton;

/**
	@brief Says that this project is wired, and takes it back.

	@par What the window is for

	The whole engine of the assembled project was already there and nothing
	called it: the state, the photograph, the undo command, and a renumbering
	that reads the photograph before it touches a tag. This is the one control
	that starts it, and the point of it being one control is that freezing
	four hundred components one at a time is work nobody does - if it is not a
	click, the automation stays unused after the panel is built, which is the
	problem the task exists for.

	@par It counts before it acts

	The photograph is taken while the window is open and the numbers are shown
	before the button is pressed, because "what does this do to my drawing" is
	the question and the answer is a count. The very state the numbers were
	read from is what the command applies, so the figure on screen and the
	figure that is frozen cannot disagree.

	@par What it says, and why it may

	Marking freezes the tags of the components and the numbers of the
	conductors, and the window says both because both halves of the
	photograph are read: ProjectRenumberer::isFrozen() before a tag is
	touched, and ConductorAutoNumerotation::isFrozen() before a wire number
	is written - when a wire is drawn, and when two folio reports are
	linked. Until the wire half was delivered, the window said only that the
	conductors were recorded: a window that promises more than the drawing
	does is a lie the drawing tells back the next day.

	@par The undo, and where it goes

	QETProject::undoStack(), through AssemblyStateCommand: taking the marking
	back has to be one Ctrl+Z, which is exactly what IecStructureDialog does
	not do - it writes its settings into the project directly. The difference
	is deliberate and is the one thing this window does not copy from it.

	@par The unsaved project, and why it is a note rather than a refusal

	The decision on record said to refuse marking a project that has not been
	saved by this version, because a conductor with no uuid in the file is
	given a fresh one while the project is being opened, and a photograph
	taken before the first save would point at conductors that no longer
	exist. Measured since: Conductor::toXml writes the uuid unconditionally,
	so the photograph and the identities it names are written to the file by
	the same save and cannot come apart. What is left is the ordinary truth
	about any change - it is in the file once the project is saved - and that
	is a note, not a refusal.

	@par The way to the order of service
	The photograph keeps the label of each item so that the program can say
	what changed since, and this window is where the photograph is made, so
	the list of those changes opens from here. The button is enabled only
	while the project is marked: an unmarked project has no "since", and a
	button that opened an empty list would read as "nothing to do".

	The list opens non modal, in front of whatever this window was opened
	from, and this window closes - without applying the stage on offer, as
	Cancel would. The list leads from each line to the item on its folio,
	and a modal window left above the folio would block exactly that.
*/
class AssemblyStateDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit AssemblyStateDialog(QETProject *project,
					     QWidget *parent = nullptr);

		/// The stage the window is currently offering to put the project in.
		AssemblyStage chosenStage() const;
		/// Pick @a stage, exactly as choosing it in the list would.
		void setChosenStage(AssemblyStage stage);

		/// How many components the photograph on offer holds.
		int photographedComponents() const;
		/// How many conductors it holds.
		int photographedConductors() const;

		/// Whether the list of changes since the marking can be opened:
		/// the project is marked as it stands, not as the list offers.
		bool changesReportAvailable() const;

		/**
			Open the list of changes since the marking, non modal, and
			close this window without applying anything.
			@return the list, deleted when it is closed; nullptr when the
			project is not marked.

			A plain member and not a slot: the button reaches it through a
			pointer-to-member connection, which needs no slot, and keeping
			it out of the meta-object keeps the return type out of it too.
		*/
		AssemblyChangesDialog *showChangesReport();

	private slots:
		void refreshPreview();
		void apply();

	private:
		void setUpWidget();

		QETProject *m_project = nullptr;
		/// What confirming would apply - counted, shown, and then pushed.
		AssemblyState m_preview;

		QComboBox *m_stage = nullptr;
		QLabel *m_current = nullptr;
		QLabel *m_effect = nullptr;
		QLabel *m_save_note = nullptr;
		QPushButton *m_changes = nullptr;
};

#endif // ASSEMBLYSTATEDIALOG_H
