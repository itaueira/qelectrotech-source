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
#include "componentlabelcollector.h"

#include "../autoNum/iecstructure.h"
#include "../dataBase/projectdatabase.h"
#include "../qetproject.h"
#include "componentlabelquery.h"

#include <QSqlError>
#include <QSqlQuery>

ComponentLabelCollector::ComponentLabelCollector(QETProject *project) :
	m_project(project)
{}

LabelEntryList ComponentLabelCollector::collect()
{
	m_error.clear();
	LabelEntryList entries;

	if (m_project.isNull()) {
		m_error = QStringLiteral("no project to collect labels from");
		return entries;
	}

	projectDataBase *data_base = m_project->dataBase();
	if (!data_base) {
		m_error = QStringLiteral("the project has no data base");
		return entries;
	}

		//The data base is built lazily and is not refreshed by the mere act
		//of asking it a question, so a project edited since it was last
		//queried would answer with what it used to hold. The command line
		//export does the same before its own query, for the same reason.
	data_base->updateDB();

		//The identification structure of the project, read once for the whole
		//run: it is what decides whether the tag of a row is composed at all,
		//and it cannot change while the rows are being read.
	const IecStructureSettings settings = m_project->iecSettings();
	const QHash<int, DiagramContext> folio_information =
			settings.enabled ? folioInformation()
					 : QHash<int, DiagramContext>();

	const QStringList columns = ComponentLabelQuery::columns();
	QSqlQuery query =
			data_base->newQuery(ComponentLabelQuery::selectStatement());
	if (!query.exec()) {
		m_error = query.lastError().text();
		return entries;
	}

	while (query.next())
	{
		QHash<QString, QString> row;
		for (int i = 0 ; i < columns.size() ; ++i) {
			row.insert(columns.at(i), query.value(i).toString());
		}

			//The sheet of this row, so that the tag inherits from the folio
			//it is drawn on and not from some other one. A row whose sheet
			//is not in the map - a position that would not read as a number
			//- is composed against an empty folio, which inherits nothing
			//and invents nothing.
		const int folio_position =
				row.value(QStringLiteral("diagram_position")).toInt();

		entries << ComponentLabelQuery::entryFromRow(
				row, settings,
				folio_information.value(folio_position));
	}

		//Second query, and the reason it is one is written where the
		//statement is: the view the rows above come from carries no
		//revision, and widening it would change what the parts list and the
		//wiring list export.
	ComponentLabelQuery::applyFolioRevisions(entries, folioRevisions());

	return entries;
}

QHash<int, DiagramContext> ComponentLabelCollector::folioInformation()
{
	QHash<int, DiagramContext> information;

	if (m_project.isNull()) {
		return information;
	}
	projectDataBase *data_base = m_project->dataBase();
	if (!data_base) {
		return information;
	}

	QSqlQuery query =
			data_base->newQuery(
				ComponentLabelQuery::folioStructureStatement());
	if (!query.exec())
	{
			//Reported, and the run goes on with nothing inherited. The
			//alternative would be a run of tags composed against a folio
			//nobody read, which is worse than short tags: it would print
			//the norm with the inherited parts missing and look right.
		m_error = query.lastError().text();
		return information;
	}

	while (query.next())
	{
		information.insert(
				query.value(0).toInt(),
				ComponentLabelQuery::folioInformation(
					query.value(1).toString(),
					query.value(2).toString()));
	}
	return information;
}

QHash<int, QString> ComponentLabelCollector::folioRevisions()
{
	QHash<int, QString> revisions;

	if (m_project.isNull()) {
		return revisions;
	}
	projectDataBase *data_base = m_project->dataBase();
	if (!data_base) {
		return revisions;
	}

	QSqlQuery query =
			data_base->newQuery(ComponentLabelQuery::folioRevisionStatement());
	if (!query.exec())
	{
			//Reported, not thrown away, and it does not empty the list: the
			//labels are still right, they just cannot say at which revision
			//of the sheet they were read. Whoever is holding the roll would
			//rather have unmarked labels than none.
		m_error = query.lastError().text();
		return revisions;
	}

	while (query.next()) {
		revisions.insert(query.value(0).toInt(), query.value(1).toString());
	}
	return revisions;
}
