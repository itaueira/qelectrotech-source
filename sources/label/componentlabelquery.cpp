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
#include "componentlabelquery.h"

namespace ComponentLabelQuery
{

QString viewName()
{
	return QStringLiteral("element_label_view");
}

QStringList columns()
{
		//label first because it is the one line that has to be legible on
		//the tape; the four that follow are what a fitter reads next, in
		//that order. The last four are not printed as such: they say which
		//sheet the row came from and whether the user kept it off the
		//purchase list.
	static const QStringList list {
		QStringLiteral("label"),
		QStringLiteral("designation"),
		QStringLiteral("manufacturer"),
		QStringLiteral("manufacturer_reference"),
		QStringLiteral("function"),
		QStringLiteral("location"),
		QStringLiteral("location_path"),
		QStringLiteral("folio"),
		QStringLiteral("diagram_position"),
		QStringLiteral("exclude_from_bom")
	};
	return list;
}

QString selectStatement()
{
		//Ordered by sheet, then by the position of the component on that
		//sheet: a run comes out in the order the folios are read, and comes
		//out the same way twice. Without an ORDER BY the answer is whatever
		//order the rows happen to sit in, which changes when a component is
		//edited, and a roll printed twice would not match itself.
	return QStringLiteral("SELECT ") + columns().join(QStringLiteral(", "))
			+ QStringLiteral(" FROM ") + viewName()
			+ QStringLiteral(" ORDER BY diagram_position, position, label");
}

QString folioRevisionStatement()
{
	return QStringLiteral(
			"SELECT pos, indexrev FROM project_summary_view ORDER BY pos");
}

LabelEntry entryFromRow(const QHash<QString, QString> &row)
{
	LabelEntry entry;
	entry.source = LabelSource::Component;
	entry.primary_text = row.value(QStringLiteral("label"));

		//Empty fields are dropped rather than printed as blank lines: tape
		//is a fixed width and a blank line is a line of it wasted. The price
		//is that the reader cannot tell a manufacturer from a function by
		//counting lines, which is why the writer, and not this, decides
		//whether to label them.
	for (const QString &key : {
			QStringLiteral("designation"),
			QStringLiteral("manufacturer"),
			QStringLiteral("manufacturer_reference"),
			QStringLiteral("function") })
	{
		const QString value = row.value(key);
		if (!value.isEmpty()) {
			entry.secondary_texts << value;
		}
	}

		//The stored path when the project has one, the plain field
		//otherwise. Which of the two it is does not change here: both are
		//kept as they were stored, and turning them into the mark the
		//standard writes is the job of the step that applies
		//QETInformation::displayedInfoValue().
	const QString path = row.value(QStringLiteral("location_path"));
	entry.location = path.isEmpty() ? row.value(QStringLiteral("location"))
					: path;

	entry.folio = row.value(QStringLiteral("folio"));

	bool ok = false;
	const int position =
			row.value(QStringLiteral("diagram_position")).toInt(&ok);
	entry.folio_position = ok ? position : -1;

	entry.excluded_from_bom =
			row.value(QStringLiteral("exclude_from_bom"))
			== QLatin1String("true");

	return entry;
}

void applyFolioRevisions(
		LabelEntryList &entries,
		const QHash<int, QString> &revision_by_folio_position)
{
	for (LabelEntry &entry : entries)
	{
			//value() with no default gives an empty QString for a sheet
			//that is not in the map, which is the answer wanted: no
			//revision, rather than the revision of some other sheet.
		entry.revision =
				revision_by_folio_position.value(entry.folio_position);
	}
}

} // namespace ComponentLabelQuery
