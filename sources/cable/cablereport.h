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
#ifndef CABLEREPORT_H
#define CABLEREPORT_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>

/**
	@brief What linking the wires of the cables to their conductors found.

	Counted rather than logged, for the same reason
	CatalogClassPackage::Report is: whoever opens a project has the right to
	read what happened to it, and a dialog needs numbers and names, not a
	stream of qDebug lines nobody sees.

	The field that matters is @ref orphans. A wire that names a conductor no
	conductor answers to is kept - as a marked spare, see
	CableWire::Orphan - and named here. The precedent this deliberately
	does not follow is TerminalStrip::fromXml(), which matches member uuids
	against the free terminals of the project with no else branch: a uuid
	that does not match is dropped without a word, and a physical terminal
	that loses all of its members disappears from the strip. Data that goes
	missing at load time and says nothing is the worst kind of defect,
	because the file on disk is still right and the next save writes the
	loss back.

	Deliberately free of any user interface. The report is filled while a
	project is being read, which is well before there is a window to put it
	in, and the export path has no window at all. A caller reads it whenever
	it likes:

	@code
		const CableReport report = project->cableReport();
		if (report.hasOrphan()) {
			// report.orphans is one legible line per wire that lost its
			// conductor; report.toText() is the whole of it in a paragraph
		}
	@endcode
*/
class CableReport
{
	Q_DECLARE_TR_FUNCTIONS(CableReport)

	public:
		CableReport() {}

		/// How many cables were looked at
		int cables = 0;
		/// Wires whose conductor was found
		int wires_occupied = 0;
		/// Wires that name no conductor: spares the designer wanted
		int wires_reserved = 0;
		/// Wires that name a conductor which is not in the project any more
		int wires_orphan = 0;
		/**
			How many conductors share a uuid with another one.

			Not a case the wires cause, and reported all the same: a
			duplicated uuid does not make the resolution absent, it makes
			it wrong - one of the two conductors answers for both - which
			is worse than a wire that lost its conductor and is the same
			kind of silence.
		*/
		int duplicated_conductors = 0;

		/// One line per wire that lost its conductor, naming cable and wire
		QStringList orphans;

		/// true when nothing at all was looked at
		bool isEmpty() const;
		/// true when at least one wire lost its conductor
		bool hasOrphan() const;

		void clear();

		/// The whole of it said in one paragraph, for a dialog or a log
		QString toText() const;
};

#endif // CABLEREPORT_H
