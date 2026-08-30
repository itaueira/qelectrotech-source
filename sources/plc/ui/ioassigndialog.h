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
#ifndef IOASSIGNDIALOG_H
#define IOASSIGNDIALOG_H

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QVector>

class Element;
class QComboBox;
class QETProject;
class QLabel;
class QPushButton;
class QTableWidget;

/**
	@brief The IoAssignDialog class
	Put the imported I/O points of a project into the channels of a PLC card,
	and take them back out.

	Today that job is done by typing: the card table of a PLC master is
	filled in row by row in the element properties, reading a sheet somebody
	has open on the side. The points are already in the project by the time
	this dialogue opens - the import of CU-11.1 put them there - so what is
	left is choosing a card, ticking the points, and letting the program
	write the function texts.

	Two lists side by side, because the question is always the same one: what
	is not placed yet, and what does this card still have free. The left one
	only ever holds points no card has taken; the right one is the card
	itself, row by row, saying what is in each and where it came from.

	Nothing is written before the button. The paragraph under the two tables
	is the plan, worked out against a copy of the project list and thrown
	away: which points would go into which channels, and which would stay
	out and why. The person reads it, and then presses.

	The channels already wired to a drawn slave are shown as such and never
	offered. Linking a slave to a master does not write anything into the
	card table - LinkElementCommand reads from it - so nothing in the model
	can see that link, and this dialogue, which holds the Element, is what
	has to say so.
*/
class IoAssignDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit IoAssignDialog(QETProject *project, QWidget *parent = nullptr);

		/// @return what the last button press did, empty when nothing moved
		QString report() const;

	private slots:
		void cardChanged();
		void selectionChanged();
		void assign();
		void release();

	private:
		void buildWidgets();
		void reloadCards();
		void reloadPoints();
		void reloadChannels();
		void reloadSummary();
		void updateEnabledState();
		void say(const QString &message, bool problem = false);

		/// @return the card currently chosen, nullptr when none
		Element *currentCard() const;
		/// @return the rows of @a master already wired to a drawn element
		QList<int> wiredChannels(Element *master) const;
		/// @return the ids of the points ticked on the left
		QStringList checkedPoints() const;
		/// @return the rows ticked on the right
		QList<int> checkedChannels() const;

	private:
		QETProject *m_project = nullptr;
		/// the PLC masters of the project, in the order the combo shows them
		QVector<QPointer<Element>> m_cards;
		bool m_loading = false;
		QString m_report;

		QComboBox *m_card = nullptr;
		QTableWidget *m_points = nullptr;
		QTableWidget *m_channels = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_assign = nullptr;
		QPushButton *m_release = nullptr;
};

#endif // IOASSIGNDIALOG_H
