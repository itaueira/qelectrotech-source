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
#include "../qetinformation.h"

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

QString writtenLocation(const DiagramContext &element_info,
			const IecStructureSettings &settings)
{
		//The `+` part the composition of the tag reads, asked of the very
		//function that reads it: the path down the tree first and whatever
		//the project says, the free text field only when the project says
		//it is a place of the norm. The switch counts only with the
		//structure on, as composedTag() - which returns before reading it -
		//and the dialogue that greys it out both have it. No tag is passed:
		//what is wanted is the place alone, and not what a tag typed with
		//its own `+` would add to it.
	const IecStructure place = IecStructure::fromElementInformation(
			QString(), element_info,
			settings.enabled && settings.location_from_element);

	if (!place.location.isEmpty())
	{
			//Written by toFullTag() of a structure holding the place and
			//nothing else, so the `+` comes from where the full tag on the
			//sheet takes it. For a path this is LocationTree::iecTag() of
			//that path, character for character - the same string
			//QETInformation::displayedInfoValue() gives for it.
		return IecStructure(QString(), place.location, QString())
				.toFullTag();
	}

		//Not a place of the norm: the field as it was typed, through the
		//function every reader of a data base row writes a value with. An
		//empty field stays empty - there is no `+` to write for nothing.
	return QETInformation::displayedInfoValue(
			IecStructure::locationKey(),
			element_info.value(IecStructure::locationKey()));
}

LabelEntry entryFromRow(const QHash<QString, QString> &row,
			const IecStructureSettings &settings,
			const DiagramContext &folio_info)
{
	LabelEntry entry;
	entry.source = LabelSource::Component;

	const DiagramContext element_info = elementInformation(row);

		//Composed, never the column on its own. The `label` column holds
		//Element::actualLabel() - what somebody typed, or what a formula
		//produced - and that is the data, not the tag the sheet shows. With
		//the structure on, a tape printed from the column names a component
		//the folio beside it does not name; with it off, composedTag() gives
		//the very same string back, so this costs nothing where nothing is
		//asked of it.
	entry.primary_text = settings.composedTag(
			row.value(QStringLiteral("label")),
			element_info,
			folio_info);

		//Empty fields are dropped rather than printed as blank lines: tape
		//is a fixed width and a blank line is a line of it wasted. The price
		//is that the reader cannot tell a manufacturer from a function by
		//counting lines, which is why the writer, and not this, decides
		//whether to label them.
		//
		//Each value is written the way the parts list and the table drawn
		//on a folio write the same column - a date in the locale of the
		//machine, the rest as it stands - so that one component does not
		//read two ways depending on which document it was read from.
	for (const QString &key : {
			QStringLiteral("designation"),
			QStringLiteral("manufacturer"),
			QStringLiteral("manufacturer_reference"),
			QStringLiteral("function") })
	{
		const QString value =
				QETInformation::displayedInfoValue(key, row.value(key));
		if (!value.isEmpty()) {
			entry.secondary_texts << value;
		}
	}

		//The mark of the norm, and never the stored path: QCM1/PLACA is
		//how the tree keeps a place, +QCM1+PLACA is how the norm writes it,
		//and so do the location dialogues and the parts list. The free text
		//field keeps its typing unless the project said it is a place. Read
		//from the very information the tag above was composed from.
	entry.location = writtenLocation(element_info, settings);

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
