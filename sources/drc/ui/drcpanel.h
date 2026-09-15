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
#ifndef DRCPANEL_H
#define DRCPANEL_H

#include "../drcfinding.h"

#include <QList>
#include <QWidget>

class QLabel;
class QTableWidget;

/**
	@brief The list of what the checker found, and the way to the thing it
	found.

	@par It is a QWidget and not a dock, on purpose
	It knows nothing about the window it sits in: today it is put inside a
	window of its own, opened by the "check now" action; tomorrow it may
	become a strip at the bottom of the editor, beside the search and
	replace one, or a dock beside the five that already exist. Any of the
	three is the same one line at the point of insertion, and none of them
	touches this class. Had it inherited from QDockWidget, the choice
	would have been written into the class and into the object name the
	window settings remember.

	@par The navigation is the one the parts report already uses
	showMe() on the sheet, clearSelection(), setSelected() and
	ensureVisible() on the object - four lines, copied rather than
	invented, so that the checker takes the reader to a component exactly
	the way the rest of the program does.

	@par But it does **not** copy the fifth line
	The parts report closes itself when a row is activated, because a
	modal window on top of the folio is in the way. This panel must stay:
	a check run produces a list to work through, and a list that vanishes
	at the first row is a list nobody finishes. Whoever puts this widget
	in a window is therefore obliged to make that window non modal - a
	modal one would leave the panel up and the folio unusable, which is
	the worst of both.

	@par Activated, not double clicked
	QTableWidget::activated fires on the double click **and** on Enter, so
	the list can be worked through from the keyboard. It is the same
	signal the parts report connects, and the same reason.

	@par A finding with nothing to point at is said, not faked
	A rule that speaks of the project as a whole has no object and no
	folio. Its row shows no folio number and activating it does nothing,
	rather than opening the first sheet - which is what somebody would
	have to write to make every row navigable, and which sends the reader
	somewhere the finding never mentioned.
*/
class DrcPanel : public QWidget
{
		Q_OBJECT

	public:
		explicit DrcPanel(QWidget *parent = nullptr);
		~DrcPanel() override;

		void setFindings(const QList<DrcFinding> &findings);
		QList<DrcFinding> findings() const {return m_findings;}
		int findingCount() const {return m_findings.count();}

		/**
			The line above the table, which says what the run did.

			Set by whoever ran the check, because only he knows what was
			run: how many rules, how long each route took, and whether the
			run may be called clean. The panel does not guess it from an
			empty table - an empty table is also what a run with every rule
			switched off produces, and the two must not read alike.
		*/
		void setSummary(const QString &summary);
		QString summary() const;

		/// The table, for a bench that wants to activate a row without a screen.
		QTableWidget *table() const {return m_table;}

		/**
			Go to what row @a row found, as far as it can be gone to.

			@return false when the row is out of range or when the finding
			has nothing to point at. Public so that a case can prove the
			navigation without synthesising a mouse.
		*/
		bool activateFinding(int row);

	Q_SIGNALS:
		/// A row was activated, whether or not there was anywhere to go.
		void findingActivated(int row);

	private:
		void fillTable();

		QLabel *m_summary = nullptr;
		QTableWidget *m_table = nullptr;
		QList<DrcFinding> m_findings;
};

#endif // DRCPANEL_H
