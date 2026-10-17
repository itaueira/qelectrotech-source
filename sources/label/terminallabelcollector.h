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
#ifndef TERMINALLABELCOLLECTOR_H
#define TERMINALLABELCOLLECTOR_H

#include "labelentry.h"

#include <QPointer>
#include <QString>

class QETProject;
class TerminalStrip;

/**
	@brief The labels of the terminals of a project, in the order of the rail.

	The second of the two collectors, and the one with no query path: the
	project data base holds no terminal strip at all - no table of strips,
	no position, no level. So this one walks the live objects instead, from
	QETProject::terminalStrip() down to each RealTerminal, and it only runs
	with the project open. ComponentLabelCollector is the other one, and
	reads the data base.

	What comes out is the same plain list of LabelEntry, so a writer takes
	the terminals and the components of a cabinet in one run and does not
	know which collector filled which entry - LabelEntry::source says so.

	@par The order is the order of the rail, and nothing here sorts
	Each strip is walked in the order of its vector of physical terminals,
	which is the order the strip editor shows and the order the file saves.
	Sorting a strip is something the user does, through the editor, and it
	rewrites that very vector; a collector that sorted on its own would
	print a tape that matches neither the rail already mounted nor the
	editor. So the entry carries the position the editor shows beside it,
	and a run that comes out in an unexpected order says so on the bench.

	@par The text is the tag the sheet draws
	Element::displayedLabel(), never RealTerminal::label(). The second is
	the stored field - what the strip editor lists - and with the
	identification structure of the project on, a tape printed from it names
	a terminal the folio beside it does not name. With the structure off,
	the default, the two are the same string.

	@par What this does not do yet
	The location comes out as it is stored, like the component one: writing
	it the way the standard writes it is the step that applies
	QETInformation::displayedInfoValue(). Two terminals of different strips
	reading the same tag are collected as two entries and not reported: that
	check compares the final tags and is a step of its own. And the cross
	reference of the terminal is not on the entry - the sheet it is drawn on
	is, in @c folio, and the cell on that sheet is for the drawing, not for
	the tape.
*/
class TerminalLabelCollector
{
	public:
		explicit TerminalLabelCollector(QETProject *project);

		/**
			Every terminal of every strip of the project, one entry per
			level of each terminal.

			Strip after strip, in the order the project holds them, and
			inside a strip in the order of the rail. A terminal symbol that
			belongs to no strip is not on any rail and gives no entry. The
			list is empty, and error() says why, when there is no project; a
			project with no strip gives an empty list and no error, since
			that is a true answer.
		*/
		LabelEntryList collect();

		/**
			The terminals of @a strip alone, in the order of its rail.

			Static because nothing of it needs the collector: every value
			is read from the terminal, its symbol and the sheet that symbol
			is drawn on. Empty for a null strip.
		*/
		static LabelEntryList collectStrip(TerminalStrip *strip);

		/// Empty after a collect() that went through.
		QString error() const {return m_error;}

	private:
		QPointer<QETProject> m_project;
		QString m_error;
};

#endif // TERMINALLABELCOLLECTOR_H
