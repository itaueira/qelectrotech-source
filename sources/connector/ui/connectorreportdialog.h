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

class QETProject;
class QWidget;

/**
	@brief The window over ConnectorCheck::report.

	Apart from the rule it shows, and in its own file, so that what is
	proved and what is not can be told apart by looking: neither test suite
	of this fork opens a QDialog, so everything in the .cpp beside this
	declaration is a roteiro for a person, and everything behind
	connectorcheck.h is proved by number.

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
	and both tables are written again from that one reading - so a row and
	the element behind it cannot come apart.
*/
namespace ConnectorCheck
{
	/// Show the end of project check of the connectors of @a project
	void showReport(QETProject *project, QWidget *parent);
}

#endif // CONNECTORREPORTDIALOG_H
