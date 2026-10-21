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
#include "drcterminalrules.h"

#include "drclinkrules.h"
#include "drcruleset.h"
#include "drcscanengine.h"

#include "../diagram.h"
#include "../diagramposition.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetproject.h"

#include <QCoreApplication>

#include <algorithm>

/**
	@brief DrcTerminalRules::unconnectedTerminalIdentifier
	@return the machine key of the rule. Never translated: the project
	file writes the severity under it.
*/
QString DrcTerminalRules::unconnectedTerminalIdentifier()
{
	return QStringLiteral("unconnected_terminal");
}

/**
	@brief DrcTerminalRules::isJudged
	@param element : an element of a folio
	@return whether it is a component: a plain element, a master or a
	slave. Not a terminal of a strip, not a folio report, not a thumbnail.
*/
bool DrcTerminalRules::isJudged(const Element *element)
{
	if (!element) {
		return false;
	}
	switch (element->linkType())
	{
		case Element::Simple:
		case Element::Master:
		case Element::Slave:
			return true;
		default:
			return false;
	}
}

/**
	@brief DrcTerminalRules::unconnectedTerminals
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per terminal with no conductor, of a component
	that has a conductor on another terminal
*/
QList<DrcFinding> DrcTerminalRules::unconnectedTerminals(QETProject *project,
							 const DrcRule &rule)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

	const QList<Diagram *> sheets = project->diagrams();
	for (Diagram *sheet : sheets)
	{
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (!isJudged(element)) {
				continue;
			}

			const QList<Terminal *> terminals = element->terminals();
				//A component with no wire at all is not judged: the
				//folio draws no wire there - a strip drawn as blocks,
				//its destinations written beside it - or it is not
				//wired yet. A wire forgotten shows on a component that
				//has the others.
			const bool wired = std::any_of(terminals.cbegin(), terminals.cend(),
						       [](const Terminal *terminal) {
				return terminal && terminal->conductorsCount() > 0;
			});
			if (!wired) {
				continue;
			}

			const QString label = DrcLinkRules::labelOf(element);
			for (Terminal *terminal : terminals)
			{
				if (!terminal || terminal->conductorsCount() > 0) {
					continue;
				}

				const QString name = terminal->name();
				const QString text = name.isEmpty()
					? QCoreApplication::translate(
						  "DrcTerminalRules",
						  "A terminal of %1, with no name, is "
						  "not connected.").arg(label)
					: QCoreApplication::translate(
						  "DrcTerminalRules",
						  "Terminal %1 of %2 is not connected.")
						  .arg(name, label);

				DrcFinding finding = DrcFinding::onElement(
							rule.identifier(),
							rule.severity(),
							text,
							element);
					//The engine writes the cell of the component when
					//the position is empty; the terminal's own cell is
					//the one to look in, so it is written here first.
				finding.setPosition(
					sheet->convertPosition(terminal->dockConductor())
					.toString());
				findings.append(finding);
			}
		}
	}

	return findings;
}

/**
	@brief DrcTerminalRules::registerRules
	@param set : the register the rule joins
	@param engine : the engine that will walk it
	@return how many were taken

	Born switched on and as a warning: see the header for why neither of
	the other two choices measures anything.
*/
int DrcTerminalRules::registerRules(DrcRuleSet &set, DrcScanEngine &engine)
{
	if (engine.addRule(set,
			   DrcRule(unconnectedTerminalIdentifier(),
				   QCoreApplication::translate(
					   "DrcTerminalRules",
					   "Component terminal with nothing "
					   "wired to it"),
				   DrcSeverity::Warning,
				   DrcRoute::Scan),
			   &DrcTerminalRules::unconnectedTerminals)) {
		return 1;
	}
	return 0;
}
