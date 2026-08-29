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
#include "pinoutusagescanner.h"

#include "../catalog/catalogassignment.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetinformation.h"
#include "../qetproject.h"

#include <QUuid>
#include <QVariant>

PinoutUsage PinoutUsageScanner::scan(QETProject *project,
				     QList<PinoutUsageConflict> *conflicts)
{
	PinoutUsage usage;
	if (!project) {
		return usage;
	}
	for (Diagram *diagram : project->diagrams())
	{
		if (!diagram) {
			continue;
		}
		for (const Element *element : diagram->elements()) {
			read(usage, element, conflicts);
		}
	}
	return usage;
}

PinoutUsage PinoutUsageScanner::scan(Diagram *diagram,
				     QList<PinoutUsageConflict> *conflicts)
{
	PinoutUsage usage;
	if (!diagram) {
		return usage;
	}
	for (const Element *element : diagram->elements()) {
		read(usage, element, conflicts);
	}
	return usage;
}

PinoutUsage PinoutUsageScanner::scanExcluding(QETProject *project,
		const Element *excluded, QList<PinoutUsageConflict> *conflicts)
{
	PinoutUsage usage;
	if (!project) {
		return usage;
	}
	for (Diagram *diagram : project->diagrams())
	{
		if (!diagram) {
			continue;
		}
		for (const Element *element : diagram->elements())
		{
			if (element == excluded) {
				continue;
			}
			read(usage, element, conflicts);
		}
	}
	return usage;
}

PinoutUsageEntry PinoutUsageScanner::placeOf(const Element *element)
{
	PinoutUsageEntry entry;
	if (!element) {
		return entry;
	}

		//The label, and not the part code: it is the label that says two
		//drawings are one component, and it is the label the cross
		//reference already goes by.
	const DiagramContext info = element->elementInformations();
	entry.component = info.value(QETInformation::ELMT_LABEL)
			.toString().trimmed();
	entry.part_code = info.value(CatalogAssignment::partCodeKey())
			.toString().trimmed();
	entry.element_uuid = element->uuid().toString();

	if (const Diagram *diagram = element->diagram())
	{
		entry.sheet = diagram->title();
			//Counted from one, the way the folio tab shows it, and
			//not from zero the way the list holds it.
		entry.sheet_number = diagram->folioIndex() + 1;
	}
	return entry;
}

QStringList PinoutUsageScanner::terminalsOf(const Element *element)
{
	QStringList names;
	if (!element) {
		return names;
	}
	for (const Terminal *terminal : element->terminals())
	{
		if (!terminal) {
			continue;
		}
			//The name it carries on this sheet, which is the one the
			//terminal list will print: an instance name when it has
			//been given one, and the name of the symbol definition
			//when it has not.
		const QString name = terminal->name().trimmed();
		if (!name.isEmpty() && !names.contains(name)) {
			names << name;
		}
	}
	return names;
}

QString PinoutUsageScanner::refusal(QETProject *project,
				    const QString &component,
				    const SymbolDefinition &symbol)
{
	return scan(project).refusal(component, symbol);
}

QString PinoutUsageScanner::refusal(QETProject *project,
				    const Element *element)
{
	if (!element) {
		return QString();
	}
	const PinoutUsageEntry place = placeOf(element);
	if (place.component.isEmpty()) {
		return QString();
	}

		//Without itself, or it would be found conflicting with the copy
		//of itself the scan just registered.
	const PinoutUsage usage = scanExcluding(project, element);
	return PinoutUsage::messageFor(usage.conflicts(place.component,
			terminalsOf(element)));
}

void PinoutUsageScanner::read(PinoutUsage &usage, const Element *element,
			      QList<PinoutUsageConflict> *conflicts)
{
	const PinoutUsageEntry place = placeOf(element);
		//An element with no label is not attributed to any component, so
		//there is nothing to hold a terminal against. Most elements are
		//in that state for a while, and refusing them would refuse a
		//drawing halfway through being made.
	if (place.component.isEmpty()) {
		return;
	}

	for (const QString &name : terminalsOf(element))
	{
		PinoutUsageEntry entry = place;
		entry.terminal = name;

		PinoutUsageConflict conflict;
		if (!usage.add(entry, &conflict)
				&& conflicts && !conflict.isNull()) {
			*conflicts << conflict;
		}
	}
}
