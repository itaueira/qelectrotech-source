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
#ifndef DRILLINGTABLEDIALOG_H
#define DRILLINGTABLEDIALOG_H

#include "../drillingtable.h"
#include "../mountinglayout.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

class QComboBox;
class QETProject;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The DrillingTableDialog class
	The holes of one mounting surface, as the worklist the bench drills
	from, with the frame they are measured in written at the head of it.

	It is a worklist and not a grid, which is a decision and not a
	shortcut: a coordinate is not edited here. A hole is where the part
	that needs it stands, so moving a hole means moving the part, and a
	table that let somebody type 137 into a cell would be a second place
	where a panel is described - the one failure this whole module is
	shaped against. What is done here is looking, narrowing, pointing at
	the part a row belongs to, and handing the list over.

	@par The frame is at the head of the window for the same reason it is
	on the first line of the file

	Since the corner became a choice there is nothing else on the sheet
	that says which of the four was used, and the same pair of numbers
	admits four readings. The sentence is built by
	DrillingTable::referenceFrameText out of the very frame the coordinates
	went through, so the header and the rows under it cannot describe
	different origins. The exported file prints the same sentence from the
	same call.

	@par The columns are DrillingTable's and not this window's

	setHeaderLabels is fed DrillingTable::header(), the same list the
	exported file is headed with. Two spellings of the same column would be
	two columns as far as anybody reading the two documents side by side is
	concerned, and the day a column is added the window follows without
	being touched.

	There is no mounting surface column, and the absence is deliberate:
	which face a hole is in is the selector at the top, not a cell repeated
	on every row. One list is one plate, because one plate is what somebody
	takes to the drill.

	@par What it opens with today

	Nothing produces a hole yet - the drilling view that turns a part into
	a cut-out is a later step - so over a real project this window opens
	empty, and says so rather than looking broken. setHoles is the one way
	in, and what it proves today is the form, the header, the grouping and
	the export. When holes start being produced, the window is already
	here.

	@par The export is the whole plate and never the filter

	The filter narrows what is looked at; the file is what is drilled.
	Handing over a filtered list would hand over a plate with holes
	missing, and a missing hole is not visible in the document that lacks
	it. The status line says so at the moment of exporting rather than
	leaving it to be discovered.

	@par Pointing back at the part is a signal and not a call

	The row carries the identity of the component the hole belongs to, and
	this window says which one. What is done with it - selecting it on the
	plate, bringing a folio forward - belongs to whoever owns the drawing,
	exactly as LocationReportDialog::goToElement does for a folio.
*/
class DrillingTableDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit DrillingTableDialog(QETProject *project,
					     QWidget *parent = nullptr);

			/// @return the project this table is read from
		QETProject *project() const;
			/// @return the face being listed, empty when none
		QString shownSurface() const;

		/**
			@brief List the holes of @a surface_uuid.
			@param surface_uuid which face, empty for none
			@return true when there was such a face in the project
		*/
		bool showSurface(const QString &surface_uuid);

		/**
			@brief Hand the holes of one face over.
			@param surface_uuid which face they belong to
			@param surface_holes the holes, in the order they are
			to be drilled

			Kept per face rather than as one list, so that looking
			at the door and coming back to the plate does not lose
			what was handed over. The order is the caller's: this
			window sorts nothing, because the order holes are
			drilled in is a decision of whoever produced them.
		*/
		void setHoles(const QString &surface_uuid,
			      const QList<DrillingHole> &surface_holes);
			/// @brief The same, for the face being listed
		void setHoles(const QList<DrillingHole> &surface_holes);

			/// @return the holes of the face being listed
		QList<DrillingHole> holes() const;
			/// @return how many holes that face has
		int holeCount() const;
			/// @return how many rows the filter leaves visible
		int visibleRowCount() const;

		/**
			@brief Narrow what is looked at.
			@param needle what to match, empty to show everything

			Public because what it must not do is worth a case of
			its own: the filter narrows the list on the screen and
			never the list that is handed over, and a rule about
			what something does not do can only be checked by doing
			it.
		*/
		void setFilter(const QString &needle);
			/// @return what is being filtered on
		QString filter() const;
			/// @return the column names, in order
		QStringList columnNames() const;

		/**
			@return the frame sentence shown at the head, the one
			the exported file is headed with.
		*/
		QString referenceFrameText() const;

		/**
			@brief The list as it is handed over.
			@param separator the delimiter between cells
			@return the frame line, the header line and one line
			per hole of the face being listed
		*/
		QString asText(const QString &separator
			       = QStringLiteral(";")) const;

			/// @return how many holes each tool makes, ordered by key
		QList<DrillingToolTotal> toolTotals() const;

		/**
			@return how many holes are not in the metal: off the
			edge of the plate, or bigger than the whole of it.

			Counted rather than refused, and counted apart from the
			plate nobody has measured: a hole on an unmeasured plate
			is not misplaced, it is unjudged, and reporting the two
			as one number would send somebody looking for a mistake
			that is not there.
		*/
		int offSurfaceCount() const;

			/// @return the sentence under the table, as it is shown
		QString summaryText() const;

		/**
			@brief Do what a double click on a row does.
			@param row the hole, in the order they were handed over
			@return true when a component was pointed at

			The one door the mouse and the keyboard both go
			through, and public for that reason: a wire that can
			only be pulled by clicking is a wire nobody checks
			twice. False for a hole that belongs to no component,
			which is a legitimate hole and not a failure.
		*/
		bool pointAtRow(int row);

	signals:
		/**
			@brief goToComponent
			@param component_uuid the component the row belongs to

			Emitted on a double click, and empty for a hole that
			belongs to no component - a cable gland, a fixing hole
			for a duct cut on the bench. Whoever owns the drawing
			decides what to do with it.
		*/
		void goToComponent(const QString &component_uuid);

	private slots:
		void surfaceChosen(int index);
		void filterChanged();
		void rowActivated(QTreeWidgetItem *item, int column);
		void copyToClipboard();
		void exportCsv();
		void layoutChanged();

	private:
		void buildWidgets();
		void refreshSurfaceList();
		void fill();
		void updateSummary();
		void say(const QString &message, bool problem = false);

			/// @return the face being listed, a default one when none
		MountingSurface shownSurfaceData() const;
			/// @return the origin bound to that face's dimensions
		DrillingFrame frame() const;

		QPointer<QETProject> m_project;

			/// the holes handed over, face by face
		QHash<QString, QList<DrillingHole> > m_holes;
			/// which face is listed
		QString m_shown;
			/// true while the list of faces is being filled
		bool m_filling = false;

		QComboBox *m_surface_box = nullptr;
		QLabel *m_frame_label = nullptr;
		QLineEdit *m_filter = nullptr;
		QTreeWidget *m_tree = nullptr;
		QTreeWidget *m_tools = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_copy = nullptr;
		QPushButton *m_export = nullptr;
};

#endif // DRILLINGTABLEDIALOG_H
