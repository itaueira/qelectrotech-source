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
#ifndef COMPONENTLABELCOLLECTOR_H
#define COMPONENTLABELCOLLECTOR_H

#include "labelentry.h"

#include "../diagramcontext.h"

#include <QHash>
#include <QPointer>
#include <QString>

class QETProject;

/**
	@brief The labels of the components of a project.

	One of the two collectors, and the one whose data already has a query
	path: a component is a row of the project data base, so this walks the
	data base. The terminals of a strip cannot be collected this way and are
	not collected here - the data base holds no strip at all, so their
	labels have to be read from the live objects, by a collector of their
	own.

	What comes out is a plain list of LabelEntry: no format, no file, no
	page. A writer turns it into whatever is being printed, and adding a
	second output format does not touch this class - which is the point of
	collecting before formatting rather than the other way round.

	@par What this does not do yet
	The location comes out as it is stored: writing it the way the standard
	writes it is the step that applies QETInformation::displayedInfoValue(),
	and it is not applied here.

	The tag no longer comes out that way. It is composed through
	IecStructureSettings::composedTag(), the function the sheet itself draws
	with, so a project with the identification structure on prints the
	structured tag rather than the stored field. With the structure off - the
	default - the composition gives that field straight back, so nothing
	changes for a project that never turned it on.
*/
class ComponentLabelCollector
{
	public:
		explicit ComponentLabelCollector(QETProject *project);

		/**
			Every component of the project, one entry each, ordered by
			sheet and then by position on the sheet.

			Items the user kept out of the bill of materials are included,
			and say so on the entry. The list is empty, and error() says
			why, when the project cannot be read.
		*/
		LabelEntryList collect();

		/// Empty after a collect() that went through.
		QString error() const {return m_error;}

	private:
		QHash<int, QString> folioRevisions();

		/**
			What each sheet hands down to the tags read from it, by sheet
			position: its function and its location.

			Asked only when the project has the structure on. A project
			with it off inherits nothing, so the question has no answer to
			give and is not put.
		*/
		QHash<int, DiagramContext> folioInformation();

		QPointer<QETProject> m_project;
		QString m_error;
};

#endif // COMPONENTLABELCOLLECTOR_H
