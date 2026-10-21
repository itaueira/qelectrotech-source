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
#include "assemblywalk.h"

#include "../autoNum/projectrenumberer.h"
#include "../conductorproperties.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../diagramposition.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "../wirecolorname.h"

namespace
{
	/**
		What a component is, for the shop: its designation when somebody
		wrote one, the name of its symbol otherwise - never nothing, because
		a line that says "mount K12" and not what K12 is sends the shop back
		to the drawing.
	*/
	QString whatIs(Element *element, const DiagramContext &information)
	{
		const QString designation = information
				.value(QETInformation::ELMT_DESIGNATION).toString();
		return designation.isEmpty() ? element->name() : designation;
	}

	/// One end of a wire, the way a wiring list names it: tag:terminal.
	QString endOf(Terminal *terminal)
	{
		if (!terminal) {
			return QString();
		}

		Element *owner = terminal->parentElement();
		QString tag;
		if (owner)
		{
			tag = owner->elementInformations()
			      .value(QETInformation::ELMT_LABEL).toString();
			if (tag.isEmpty()) {
				tag = owner->name();
			}
		}

		const QString name = terminal->name();
		if (name.isEmpty()) {
			return tag;
		}
		if (tag.isEmpty()) {
			return name;
		}
		return tag + QLatin1Char(':') + name;
	}
}

/**
	Read @a project the way the photograph was read.

	The same list of components - ProjectRenumberer::components(), which
	is what AssemblyStateCommand::photograph() walks - and the same
	fields: the "label" information of a component and the text of a
	conductor. A difference between the two readings is a change nobody
	made, listed on an order of service: keep them the same, or move both.

	project->diagrams() builds every folio still waiting to be built, and
	that matters here more than anywhere: a folio not built yet draws
	nothing, and every item on it would be listed as one to remove.
*/
AssemblyWalk AssemblyWalk::of(QETProject *project)
{
	AssemblyWalk walked;
	if (!project) {
		return walked;
	}

	const QList<Element *> elements = ProjectRenumberer::components(project);
	for (Element *element : elements)
	{
		if (!element) {
			continue;
		}

		const DiagramContext information = element->elementInformations();
		AssemblyItem item;
		item.uuid = element->uuid().toString();
		item.label = information.value(QStringLiteral("label")).toString();
		if (Diagram *sheet = element->diagram())
		{
			item.folio = sheet->folioIndex() + 1;
			item.position = sheet->convertPosition(element->scenePos())
					.toString();
		}
		item.description = whatIs(element, information);
		item.part_code = information
				 .value(QETInformation::ELMT_PART_CODE).toString();

		walked.components << item;
		walked.component_objects << QPointer<Element>(element);
	}

	const QList<Diagram *> sheets = project->diagrams();
	for (Diagram *sheet : sheets)
	{
		if (!sheet) {
			continue;
		}
		const QList<Conductor *> conductors = sheet->conductors();
		for (Conductor *conductor : conductors)
		{
			if (!conductor) {
				continue;
			}

			AssemblyItem item;
			item.uuid = conductor->uuid().toString();
				//Without the marker of the square of the wire's
				//colour, as AssemblyStateCommand::photograph() keeps
				//it: the tag printed is the text, not the square
			item.label = WireColorName::withoutMarker(conductor->properties().text);
			item.folio = sheet->folioIndex() + 1;
				//Where the wire starts: the middle of its bounding box
				//falls in empty space for any wire that turns a corner.
			if (conductor->terminal1)
			{
				item.position = sheet->convertPosition(
							conductor->terminal1->dockConductor())
						.toString();
			}
			item.description = QStringLiteral("%1 - %2")
					   .arg(endOf(conductor->terminal1),
						endOf(conductor->terminal2));

			walked.wires << item;
			walked.wire_objects << QPointer<Conductor>(conductor);
		}
	}

	return walked;
}
