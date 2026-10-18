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
#include "terminallabelcollector.h"

#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../autoNum/iecstructure.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "componentlabelquery.h"

#include <QSharedPointer>
#include <QVector>

namespace {

/**
	@param strip_name : the name of the strip the terminal is on
	@param strip_position : the position of its physical terminal on the rail
	@param real_terminal : the terminal, one level of that physical terminal
	@return the label of that one level
*/
LabelEntry terminalEntry(const QString &strip_name,
			 int strip_position,
			 const QSharedPointer<RealTerminal> &real_terminal)
{
	LabelEntry entry;
	entry.source = LabelSource::Terminal;
	entry.strip = strip_name;
	entry.strip_position = strip_position;
	entry.level = real_terminal->level();

		//A terminal of the rail with no symbol behind it - one whose symbol
		//went away while the strip still holds it - is still a place on the
		//rail. It keeps its entry, with nothing to print: dropping it would
		//shift every label after it by one against the terminals they are
		//meant for, which is the one failure a tape cannot recover from.
	Element *element = real_terminal->element();
	if (!element) {
		return entry;
	}

		//The tag the sheet draws, and not RealTerminal::label(). That one is
		//Element::actualLabel(), the stored field, and with the structure of
		//the project on it is not what the folio says. displayedLabel() goes
		//through IecStructureSettings::composedTag(), the composition the
		//component labels use too, and gives the stored field back untouched
		//when the structure is off.
	entry.primary_text = element->displayedLabel();

		//The three the strip editor shows next to the label, in the order of
		//its columns: the wires docked, then the cable and the wire of the
		//cable. Empty ones are dropped, as the component labels drop theirs:
		//tape is a fixed width, and a terminal with nothing docked is the
		//common case, not the exception.
		//
		//The wires come as the editor writes them, one cell for all of them
		//and a mark for a wire with no number: a terminal with an unnumbered
		//wire must not read like a terminal with none.
	for (const QString &value : {real_terminal->conductor(),
				     real_terminal->cable(),
				     real_terminal->cableWire()})
	{
		if (!value.isEmpty()) {
			entry.secondary_texts << value;
		}
	}

		//The information the project keeps for the symbol, and not the copy
		//the symbol carries: for a placed symbol the store is the one that
		//answers, and it is the one the data base - and so the component
		//labels - read.
	const DiagramContext information = element->elementInformations();

		//The sheet the symbol is drawn on, and the project of that sheet:
		//the settings are asked of the same project Element::displayedLabel()
		//composes the tag above with, so the location and the tag of one
		//terminal cannot be read against two different structures. A symbol
		//on no sheet has no project to say anything, and reads as one whose
		//structure is off.
	Diagram *diagram = element->diagram();
	QETProject *project = diagram ? diagram->project() : nullptr;
	const IecStructureSettings settings =
			project ? project->iecSettings() : IecStructureSettings();

		//The mark of the norm and not the stored path, through the very
		//rule the component labels write theirs with - not a copy of it -
		//so that a terminal and a component in the same place read the same
		//mark, and a terminal symbol, which the data base also publishes as
		//a component, reads the same in both of its rows.
	entry.location = ComponentLabelQuery::writtenLocation(information,
							      settings);

		//Compared the way the parts list view compares it, so that an entry
		//says it is excluded exactly when the parts list leaves it out.
		//Carried, never obeyed: the box keeps a terminal off the purchase
		//list, and the terminal is on the rail all the same.
	entry.excluded_from_bom =
			information.value(QStringLiteral("exclude_from_bom")).toString()
			== QLatin1String("true");

		//The sheet the symbol is drawn on, read from its own title block and
		//not from the strip: a strip crosses sheets, and each terminal of it
		//carries the folio and the revision of the sheet it came from. The
		//information is the very context the title block draws and the data
		//base stores for that sheet, so a terminal and a component of the
		//same sheet read the same folio and the same revision.
	if (!diagram) {
		return entry;
	}

	const DiagramContext folio =
			diagram->border_and_titleblock.titleblockInformation();
	entry.folio = folio.value(QStringLiteral("folio")).toString();
	entry.revision = folio.value(QStringLiteral("indexrev")).toString();

		//Counted from 1, like the diagram_position of the data base the
		//component labels read, so that one sheet has one number on both
		//kinds of entry.
	if (project)
	{
		const int index = project->folioIndex(diagram);
		entry.folio_position = index < 0 ? -1 : index + 1;
	}

	return entry;
}

} // namespace

TerminalLabelCollector::TerminalLabelCollector(QETProject *project) :
	m_project(project)
{}

LabelEntryList TerminalLabelCollector::collect()
{
	m_error.clear();
	LabelEntryList entries;

	if (m_project.isNull()) {
		m_error = QStringLiteral("no project to collect labels from");
		return entries;
	}

		//The strips in the order the project holds them, which is the order
		//the file saves them in. Not sorted by name either: the same rule as
		//inside a strip, applied one level up - what comes out is what the
		//project says, in the order it says it.
	const QVector<TerminalStrip *> strips = m_project->terminalStrip();
	for (TerminalStrip *strip : strips) {
		entries += collectStrip(strip);
	}

	return entries;
}

LabelEntryList TerminalLabelCollector::collectStrip(TerminalStrip *strip)
{
	LabelEntryList entries;

	if (!strip) {
		return entries;
	}

	const QString strip_name = strip->name();

		//The vector of physical terminals as it stands, and that is the
		//whole of the ordering: it is the order of the rail, the order the
		//strip editor lists, and the order the file saves. Nothing is sorted
		//here, by label or otherwise - see the class documentation.
	const QVector<QSharedPointer<PhysicalTerminal>> physical_terminals =
			strip->physicalTerminal();
	for (const QSharedPointer<PhysicalTerminal> &physical : physical_terminals)
	{
		if (physical.isNull()) {
			continue;
		}

			//PhysicalTerminal::pos() and not a counter of this loop, though
			//the two agree: it is the number the strip editor writes in its
			//Position column, and the point of carrying it is that the two
			//can be held side by side.
		const int strip_position = physical->pos();

			//Back to front, the order of the levels: realTerminals() is
			//indexed by level.
		const QVector<QSharedPointer<RealTerminal>> levels =
				physical->realTerminals();
		for (const QSharedPointer<RealTerminal> &real_terminal : levels)
		{
				//Skipped as the strip editor skips it: a null level has
				//nothing to read and is not a place on the rail of its own.
			if (real_terminal.isNull()) {
				continue;
			}

			entries << terminalEntry(strip_name, strip_position,
						 real_terminal);
		}
	}

	return entries;
}
