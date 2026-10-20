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
#include "drcpotentialrules.h"

#include "drcruleset.h"
#include "drcscanengine.h"
#include "drcwirenumbers.h"

#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetproject.h"
#include "../wirecolorname.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <utility>

/**
	@brief DrcPotentialRules::twoWireNumbersIdentifier
	@return the machine key of the rule. Never translated: the project file
	writes it.
*/
QString DrcPotentialRules::twoWireNumbersIdentifier()
{
	return QStringLiteral("potential_with_two_wire_numbers");
}

/**
	@brief DrcPotentialRules::potentialsOf
	@param project : the project to walk
	@return one list of conductors per potential, in the order the walk
	meets them
*/
QList<QList<Conductor *>> DrcPotentialRules::potentialsOf(QETProject *project)
{
	QList<QList<Conductor *>> potentials;
	if (!project) {
		return potentials;
	}

		//The order of the walk, written down once: it is what the
		//conductors of each potential are sorted by, so that the first one
		//is the one on the lowest folio and not whichever a QSet hands out
		//first. diagrams() builds every folio, so that a potential crossing
		//a report arrow is followed onto a folio nobody has opened yet.
	QList<Conductor *> walk;
	QHash<Conductor *, int> order;
	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
		const QList<Conductor *> conductors = diagram->conductors();
		for (Conductor *conductor : conductors)
		{
			if (order.contains(conductor)) {
				continue;
			}
			order.insert(conductor, int(walk.count()));
			walk << conductor;
		}
	}

		//The same walk exportNets() does for the nets of the command line:
		//every conductor not yet seen opens a potential, and the whole of
		//that potential is then marked as seen.
	QSet<Conductor *> visited;
	for (Conductor *conductor : walk)
	{
		if (visited.contains(conductor)) {
			continue;
		}

		QSet<Conductor *> group = conductor->relatedPotentialConductors(true);
		group.insert(conductor);

		QList<Conductor *> potential;
		for (Conductor *member : std::as_const(group))
		{
			visited.insert(member);
				//A conductor the folios do not hold is not one this walk
				//can address; leaving it out keeps every member of the list
				//something a finding may point at.
			if (order.contains(member)) {
				potential << member;
			}
		}

		std::sort(potential.begin(), potential.end(),
			  [&order](Conductor *a, Conductor *b) {
			return order.value(a) < order.value(b);
		});
		potentials << potential;
	}

	return potentials;
}

/**
	@brief DrcPotentialRules::potentialWithTwoNumbers
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per potential carrying more than one wire number

	The folio of each conductor is read here, and not only by the engine,
	because it is part of what the sentence says: when the numbers sit on
	different folios, the finding names where each of them is. It is the
	same QETProject::folioIndex() + 1 the engine writes the address of the
	finding with, so the sentence and the folio column cannot disagree.
*/
QList<DrcFinding> DrcPotentialRules::potentialWithTwoNumbers(
		QETProject *project,
		const DrcRule &rule)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

	const QList<QList<Conductor *>> potentials = potentialsOf(project);
	QHash<Diagram *, int> folio_of;

	for (const QList<Conductor *> &potential : potentials)
	{
		QList<DrcWireNumbers::Mark> marks;
		Conductor *target = nullptr;

		for (Conductor *conductor : potential)
		{
			DrcWireNumbers::Mark mark;
				//Without the marker of the square of the wire's colour:
				//the sentence names the number, and a text that is only
				//the square carries none
			mark.text = WireColorName::withoutMarker(conductor->properties().text);

			Diagram *sheet = conductor->diagram();
			if (sheet)
			{
				if (!folio_of.contains(sheet)) {
					folio_of.insert(sheet, project->folioIndex(sheet) + 1);
				}
				mark.folio = folio_of.value(sheet);
			}
			marks << mark;

				//The first conductor that carries a number, on the lowest
				//folio the potential is numbered on: one of the two the
				//reader has to change, and the one the walk meets first.
			if (!target && !DrcWireNumbers::isAbsent(mark.text)) {
				target = conductor;
			}
		}

		const QString sentence = DrcWireNumbers::severalNumbersText(marks);
		if (sentence.isEmpty() || !target) {
			continue;
		}

		findings.append(DrcFinding::onConductor(rule.identifier(),
							rule.severity(),
							sentence,
							target));
	}

	return findings;
}

/**
	@brief DrcPotentialRules::registerRules
	@param set : the register the rules join
	@param engine : the engine that will walk them
	@return how many were taken

	Born an error and switched on: see the header for why this one, of all
	the rules of the checker, is not a warning.
*/
int DrcPotentialRules::registerRules(DrcRuleSet &set, DrcScanEngine &engine)
{
	int taken = 0;

	if (engine.addRule(set,
			   DrcRule(twoWireNumbersIdentifier(),
				   QCoreApplication::translate(
					   "DrcPotentialRules",
					   "Potential with two wire numbers"),
				   DrcSeverity::Error,
				   DrcRoute::Scan),
			   &DrcPotentialRules::potentialWithTwoNumbers)) {
		++taken;
	}

	return taken;
}
