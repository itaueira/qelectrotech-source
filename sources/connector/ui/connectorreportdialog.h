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
#ifndef CONNECTORREPORTDIALOG_H
#define CONNECTORREPORTDIALOG_H

#include "../connectorcheck.h"

#include <QDialog>
#include <QList>
#include <QString>

class Catalog;
class Element;
class QComboBox;
class QETProject;
class QLabel;
class QPushButton;
class QTableWidget;
class QWidget;

/**
	@brief The window over ConnectorCheck::report.

	Apart from the rule it shows, and in its own file, so that what is
	proved and what is not can be told apart by looking: everything behind
	connectorcheck.h is proved by number, and of this window only the part a
	test drives is - see ReportDialog below for which part that is.

	It is built in the mould of the two catalogue reports of the same menu -
	CatalogProjectActions::showMissingPartReport and its physical view
	sibling - and not in the mould of LocationReportDialog, which the plan
	of T34 named. The three windows of the Catalogue menu answer the same
	shape of question about the same project and have to behave the same
	way; and the one thing LocationReportDialog buys with its extra three
	hundred lines, staying open while the folio is worked on, is bought here
	by the list shortening as it is worked through and by the window
	standing aside when a row is walked to.

	One defect of that mould is not copied, and the way it is avoided is
	worth the sentence: the report next door held its rows in a list beside
	the table and shortened both, which went wrong the day one of them
	shortened alone and the double click began walking to a component nobody
	had chosen. Here nothing is shortened at all - the project is read again
	and every table is written again from that one reading - so a row and
	the element behind it cannot come apart.
*/
namespace ConnectorCheck
{
	/// Show the end of project check of the connectors of @a project
	void showReport(QETProject *project, QWidget *parent);

	/**
		@brief The window itself, apart from the exec() that shows it.

		A class and not a function holding everything in locals, for one
		reason: a test can build it, drive it and read it without ever
		calling exec(), which under the offscreen platform waits forever
		for a click nobody gives. showReport() is the only place that
		makes it modal.

		Three tables. The first two are the end of project check - the
		connectors whose ways cannot be counted, and the pins belonging to
		no connector. The third shows the ways of one connector, chosen
		above it, in reading order, and is where two ways swap their
		numbers (T34, CU-34.4): two rows selected, or one row and the
		other way picked from a question.

		@par Why the third table is in reading order and not by way number

		The rows are the pins, in the order the folios draw them, so a
		swap leaves every row where it was and changes the number written
		in two of them - the use case on the screen: "the place in the list
		does not change, the content of the rows swaps". Sorted by way
		number, the two rows would trade places after the swap and the
		column of numbers would read exactly as before, which is the one
		reading in which a swap and nothing at all look alike.

		@par What a test reaches, and what stays a roteiro for a person

		Reached: choosing the connector, selecting two ways, the swap
		button, the sentence that says why a swap was refused, and the
		Ctrl+Z and Ctrl+Y of the window. Not reached: the double click that
		walks to the folio, the two assignment buttons, and the question
		asked when one way only is selected - all three open or leave for
		something modal.

		The test finds what it drives by object name -
		connector_way_choice, connector_ways, connector_swap,
		connector_swap_status, connector_report_undo and
		connector_report_redo - so renaming one of them is a change to
		tests/catch/src/ui/connectorswap_test.cpp too.
	*/
	class ReportDialog : public QDialog
	{
		public:
			/**
				@param project the project to read
				@param catalog the catalogue to ask about parts and
				classes; null or closed reads as a catalogue that did
				not answer
				@param parent
			*/
			ReportDialog(QETProject *project, Catalog *catalog,
				     QWidget *parent = nullptr);

		private:
			void buildWidgets();
			void refill();
			void fillWays();
			void updateButtons();
			void assignPartToSelection();
			void assignConnectorToSelection();
			void swapSelectedWays();
			ConnectorSwap::Refusal swapWays(int first_row, int second_row);
			void undoLast();
			void redoNext();

			QETProject *m_project = nullptr;
			Catalog *m_catalog = nullptr;

			QLabel *m_summary = nullptr;
			QTableWidget *m_connectors = nullptr;
			QTableWidget *m_pins = nullptr;
			QComboBox *m_way_choice = nullptr;
			QTableWidget *m_ways = nullptr;
			QLabel *m_swap_status = nullptr;
			QPushButton *m_assign_part = nullptr;
			QPushButton *m_assign_connector = nullptr;
			QPushButton *m_swap = nullptr;

				//What the tables are showing, kept beside them and
				//rewritten whole on every reading. Nothing is ever
				//removed from any of them one row at a time: that is
				//what let a table and its list come apart in the
				//report next door, and it cannot happen where all of
				//them are written from the same read.
			Report m_current;
			QList<DrawnConnector> m_uncountable;
			QList<Element *> m_loose;
				/// the pins of the connector the third table shows, row by row
			QList<Element *> m_way_pins;
				/// the key of the connector the third table shows
			QString m_ways_key;
	};
}

#endif // CONNECTORREPORTDIALOG_H
