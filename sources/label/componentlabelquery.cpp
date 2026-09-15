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

#include "../autoNum/iecstructure.h"
#include "../diagramcontext.h"

namespace ComponentLabelQuery
{

namespace {

/**
	@param row : one row of selectStatement()
	@return the component information the composition of the tag reads

	Three keys and not the whole row: these are the ones
	IecStructure::fromElementInformation() looks for, and they are named by
	the class that reads them rather than written out here, so a key that
	moves does not quietly stop arriving. The tag itself is not among them -
	it is passed to the composition as the label, exactly as the drawing
	passes Element::actualLabel().
*/
DiagramContext elementInformation(const QHash<QString, QString> &row)
{
	DiagramContext info;
	info.addValue(IecStructure::plantKey(),
		      row.value(IecStructure::plantKey()));
	info.addValue(IecStructure::locationKey(),
		      row.value(IecStructure::locationKey()));
	info.addValue(IecStructure::locationPathKey(),
		      row.value(IecStructure::locationPathKey()));
	return info;
}

} // namespace

QString viewName()
{
	return QStringLiteral("element_label_view");
}

QStringList columns()
{
		//label first because it is the one line that has to be legible on
		//the tape; the four that follow are what a fitter reads next, in
		//that order. Then the three the composition of the tag reads, which
		//are asked for by the name IecStructure gives them: the column of
		//the view **is** the element information key, so taking the name
		//from there is what keeps the column and its reader from drifting
		//apart. The last three are not printed as such: they say which
		//sheet the row came from and whether the user kept it off the
		//purchase list.
	static const QStringList list {
		QStringLiteral("label"),
		QStringLiteral("designation"),
		QStringLiteral("manufacturer"),
		QStringLiteral("manufacturer_reference"),
		QStringLiteral("function"),
		IecStructure::plantKey(),
		IecStructure::locationKey(),
		IecStructure::locationPathKey(),
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

QString folioStructureStatement()
{
		//The same view and the same key as the revision, and the two columns
		//the norm inherits from a sheet. Written out rather than taken from
		//IecStructure like the element ones are: those are element
		//information keys and these are diagram information keys, and the
		//folio keeps its location under a name of its own.
	return QStringLiteral(
			"SELECT pos, plant, locmach FROM project_summary_view"
			" ORDER BY pos");
}

DiagramContext folioInformation(const QString &plant, const QString &locmach)
{
	DiagramContext info;
	info.addValue(IecStructure::plantKey(), plant);
	info.addValue(IecStructure::folioLocationKey(), locmach);
	return info;
}

LabelEntry entryFromRow(const QHash<QString, QString> &row,
			const IecStructureSettings &settings,
			const DiagramContext &folio_info)
{
	LabelEntry entry;
	entry.source = LabelSource::Component;

		//Composed, never the column on its own. The `label` column holds
		//Element::actualLabel() - what somebody typed, or what a formula
		//produced - and that is the data, not the tag the sheet shows. With
		//the structure on, a tape printed from the column names a component
		//the folio beside it does not name; with it off, composedTag() gives
		//the very same string back, so this costs nothing where nothing is
		//asked of it.
	entry.primary_text = settings.composedTag(
			row.value(QStringLiteral("label")),
			elementInformation(row),
			folio_info);

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
	const QString path = row.value(IecStructure::locationPathKey());
	entry.location = path.isEmpty() ? row.value(IecStructure::locationKey())
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
