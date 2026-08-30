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
#ifndef LOCATIONREPORTDIALOG_H
#define LOCATIONREPORTDIALOG_H

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QString>

class Element;
class QETProject;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The LocationReportDialog class
	The list of the components of the project that say they are nowhere,
	folio by folio, made to be walked rather than to be read.

	CU-32.3 and CU-32.8 are the same list seen from two ends. The first
	says that leaving a component unlocated is a legitimate answer - a
	pushbutton on the machine frame is in the field, and the project is not
	wrong for saying so - and therefore that this window raises no alarm
	and refuses nothing. The second says that on a project meant to be
	built, somebody eventually walks this list down to zero. A window that
	scolded on opening would be useless for the first and unbearable for
	the second.

	So it is a worklist. It groups by folio because that is the order a
	person assigns in - "everything drawn on folio 13 is in the door" is
	one gesture here, and it is the gesture that took 118 terminals in one
	go during the verification of the step before. Selecting the folio row
	takes what it contains; the filter narrows 354 rows to the handful
	being looked for; a double click goes to the component on its folio,
	which is the only way to answer "and what is this one, actually" for a
	terminal whose label says nothing.

	It does not compute its own truth. The census is the same walk the
	manager does - every folio, every item, isLocatableElement, then the
	location_path key - so the number here and the number in the manager
	footer cannot disagree, and assignment goes through the same
	AssignLocationCommand, so undo works from here exactly as it works from
	there. The window listens to the undo stack and rebuilds: assigning
	twenty components makes twenty rows leave the list, and undoing brings
	them back.

	Going to the component is a signal rather than a call. The folio has to
	be brought forward before the element can be shown, and that belongs to
	the editor that owns the project view - a report that knew how to
	switch folios would be a report that knows what an editor is.
*/
class LocationReportDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit LocationReportDialog(QETProject *project,
					      QWidget *parent = nullptr);

		/// @return the project this window is reporting on
		QETProject *project() const;

	signals:
		/**
			@brief goToElement
			@param element the component the person asked to see
			Emitted on a double click. Whoever owns the project view
			brings the folio forward and shows the element; this window
			only says which one.
		*/
		void goToElement(Element *element);

	private slots:
		void reload();
		void selectionChanged();
		void filterChanged();
		void itemActivated(QTreeWidgetItem *item, int column);
		void stackChanged();

		void assignSelection();
		void selectOnFolio();

	private:
		void buildWidgets();
		void fill();
		void say(const QString &message, bool problem = false);
		/// @return the unlocated components the person picked, folio
		/// rows counting for everything they contain
		QList<Element *> chosenElements() const;
		/// @return every unlocated component of the project, in folio order
		QList<Element *> unlocatedElements() const;

	private:
		QPointer<QETProject> m_project;
		bool m_loading = false;
			/// the components behind the row indices, guarded because the
			/// list outlives a rebuild by a moment
		QList<QPointer<Element> > m_elements;

		QLineEdit *m_filter = nullptr;
		QTreeWidget *m_tree = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_assign = nullptr;
		QPushButton *m_select = nullptr;
};

#endif // LOCATIONREPORTDIALOG_H
