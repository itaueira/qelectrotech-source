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
#ifndef MOUNTINGCHECKDIALOG_H
#define MOUNTINGCHECKDIALOG_H

#include "../mountingcheck.h"
#include "../mountinglayout.h"

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

class Catalog;
class QComboBox;
class QETProject;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The MountingCheckDialog class
	What is wrong with one plate, said before a hole is drilled in it.

	The arithmetic is not here and never was: MountingCheck answers the
	three questions - do two parts want the same room, does every part have
	the air it asked for, is everything on the plate at all - and has
	answered them since long before anything showed them. What was missing
	was the half a person can read, and a rule whose answer nobody ever
	sees is a rule that might as well not run. This is that half.

	@par Why the four answers are four sentences

	MountingFit has four values and not two, because "it does not fit" is
	two different jobs for whoever has to fix it: a part that would fit
	somewhere else on the plate needs dragging, and a part wider than the
	whole plate needs another enclosure or another product. Telling a
	person the first when it is the second wastes an afternoon, and that is
	the entire reason this window exists as a step of its own rather than
	as a red outline on the drawing.

	The switch that maps them covers all four, Fits included, so that a
	fifth answer added to the enum cannot slip through as a silent blank
	row.

	@par Clean and conclusive are shown together, always

	A plate whose parts nobody has measured comes back clean, because a
	part with no dimensions cannot be shown to collide with anything - and
	that clean answer is worth exactly what the measurements behind it are
	worth. MountingSurfaceReport says so through isConclusive, and showing
	one without the other would promise a fit that was never verified. So
	the summary is two sentences and not one, even when the first of them
	is "nothing found".

	@par Clearance is checked against the catalogue and against nothing else

	The four numbers a part asks for live on the catalogue part, per
	component, with nothing seeded by class - the decision taken when the
	question was asked. A part nobody measured therefore asks for nothing,
	produces no violation, and is counted in the sentence that says how
	many parts were checked against nothing.

	The catalogue is handed in rather than fetched from the application,
	and that is worth a sentence: a window that reached for the one
	catalogue of the program would open it as a side effect of being
	built - creating the file if it was not there - and a report is not
	the thing that should be deciding that. Handed no catalogue at all, it
	falls back to the same state honestly: every part checked against no
	requirement, said out loud through isConclusive rather than presented
	as a clean plate.

	@par Pointing at the part is a signal and not a call

	Selecting a row says which part is complained about; what is done with
	it - selecting it on the drawing, bringing it into view - belongs to
	whoever owns the scene, exactly as LocationReportDialog::goToElement
	does for a folio. A report that knew how to centre a view would be a
	report that knows what a view is.
*/
class MountingCheckDialog : public QDialog
{
	Q_OBJECT

	public:
		/**
			@brief MountingCheckDialog
			@param project the project the faces are read from
			@param catalog where the clearance of a product code is
			looked up; none means none was asked for, which is a
			state this window says out loud
			@param parent parent widget

			The catalogue is not owned and has to outlive this
			window - the one the program holds does, being the
			application's.
		*/
		explicit MountingCheckDialog(QETProject *project,
					     Catalog *catalog = nullptr,
					     QWidget *parent = nullptr);

			/// @return the project this check reads from
		QETProject *project() const;
			/// @return the face being checked, empty when none
		QString shownSurface() const;

		/**
			@brief Check the face of @a surface_uuid.
			@param surface_uuid which face, empty for none
			@return true when there was such a face in the project
		*/
		bool showSurface(const QString &surface_uuid);

			/// @return the whole report of the face being checked
		MountingSurfaceReport report() const;

		/**
			@return how many complaint rows are listed.

			Counted from the widget and not from the report, on
			purpose: it is the number a person sees, and it is what
			catches a complaint the report holds and this window
			forgot to draw.
		*/
		int issueRowCount() const;

			/// @return the identity of every part named in a complaint
		QStringList complainedAbout() const;
			/// @return the sentence under the list, as it is shown
		QString summaryText() const;

		/**
			@brief Do what a double click on a complaint does.
			@param index the complaint, in the order they are
			listed, groups walked in turn
			@return true when a part was pointed at

			The one door the mouse and the keyboard both go
			through, and public for that reason: a wire that can
			only be pulled by clicking is a wire nobody checks
			twice.
		*/
		bool pointAtRow(int index);

		/**
			@brief What to call one of the four answers.
			@param fit the answer
			@return the sentence for the person who has to fix it
		*/
		static QString fitMessage(MountingFit fit);

	signals:
		/**
			@brief goToItem
			@param item_uuid the part the row complains about

			Emitted on a double click and on a change of selection.
			Whoever owns the drawing selects it and brings it into
			view; this window only says which one.
		*/
		void goToItem(const QString &item_uuid);

	private slots:
		void surfaceChosen(int index);
		void rowActivated(QTreeWidgetItem *item, int column);
		void selectionChanged();
		void layoutChanged();
		void recheck();

	private:
		void buildWidgets();
		void refreshSurfaceList();
		void fill();
		void say(const QString &message, bool problem = false);

			/// @return the face being checked, a default one when none
		MountingSurface shownSurfaceData() const;
			/// @return how a part is called on this face
		QString designationOf(const QString &item_uuid) const;

		QPointer<QETProject> m_project;
			/// where a clearance is looked up, none when none
		QPointer<Catalog> m_catalog;

			/// which face is checked
		QString m_shown;
			/// the answer the list was drawn from
		MountingSurfaceReport m_report;
			/// true while the list of faces is being filled
		bool m_filling = false;

		QComboBox *m_surface_box = nullptr;
		QTreeWidget *m_tree = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_trust = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_recheck = nullptr;
};

#endif // MOUNTINGCHECKDIALOG_H
