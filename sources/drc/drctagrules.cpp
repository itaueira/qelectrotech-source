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
#include "drctagrules.h"

#include "drcduplicatetags.h"
#include "drcruleset.h"
#include "drcscanengine.h"

#include "../autoNum/iecstructure.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../properties/elementdata.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QCoreApplication>
#include <QPointF>

#include <algorithm>
#include <utility>

/**
	@brief DrcTagRules::duplicateTagIdentifier
	@return the machine key of the rule. Never translated: the project file
	writes it.
*/
QString DrcTagRules::duplicateTagIdentifier()
{
	return QStringLiteral("duplicate_tag");
}

/**
	@brief DrcTagRules::isCompared
	@param element : a component of the folio
	@return whether its tag takes part in the comparison
*/
bool DrcTagRules::isCompared(const Element *element)
{
	if (!element) {
		return false;
	}

	const ElementData data = element->elementData();
	if (data.m_type == ElementData::Simple) {
		return true;
	}
		//The master of a terminal drawn on a strip sheet is that terminal
		//seen from its strip, and terminals are not compared here.
	return data.m_type == ElementData::Master
	       && !data.isStripTerminalMaster();
}

/**
	@brief DrcTagRules::duplicateTags
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per tag carried by more than one component

	The settings of the structure are read once for the run, and not per
	component: the reading the sentence names has to be the one every tag
	of the run was compared by.
*/
QList<DrcFinding> DrcTagRules::duplicateTags(QETProject *project,
					     const DrcRule &rule)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

	const IecStructureSettings settings = project->iecSettings();
	const DrcDuplicateTags::Reading reading =
			DrcDuplicateTags::readingOf(settings);

	QList<Element *> compared;
	QList<DrcDuplicateTags::Mark> marks;

		//diagrams() builds every folio, so that a tag repeated on a folio
		//nobody has opened yet is compared like the others.
	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
			//The scene hands its items out in stacking order, which moves
			//when something is brought to the front. Sorted by place, the
			//first member of a group - the one the finding points at - is
			//the same on every run.
		QList<Element *> elements = diagram->elements();
		std::sort(elements.begin(), elements.end(),
			  [](Element *a, Element *b) {
			const QPointF a_pos = a->scenePos();
			const QPointF b_pos = b->scenePos();
			if (a_pos.y() != b_pos.y()) {
				return a_pos.y() < b_pos.y();
			}
			if (a_pos.x() != b_pos.x()) {
				return a_pos.x() < b_pos.x();
			}
			return a->uuid() < b->uuid();
		});

			//What Element::composedLabel() hands the composition: the
			//title block of the sheet the component is drawn on.
		const DiagramContext folio_info =
				diagram->border_and_titleblock.titleblockInformation();

		for (Element *element : std::as_const(elements))
		{
			if (!isCompared(element)) {
				continue;
			}

			DrcDuplicateTags::Mark mark;
			mark.tag = DrcDuplicateTags::comparedTag(
					   settings,
					   element->actualLabel(),
					   element->elementInformations(),
					   folio_info);
			if (DrcDuplicateTags::key(mark.tag).isEmpty()) {
				continue;
			}

			compared << element;
			marks << mark;
		}
	}

	const QList<QList<int>> groups = DrcDuplicateTags::groups(marks);
	for (const QList<int> &group : groups)
	{
		QList<DrcDuplicateTags::Mark> members;
		for (int index : group)
		{
				//The folio and the cell are written by the same function
				//that addresses every finding of the checker, and only for
				//the components that end up in a sentence: the sentence
				//and the columns of the panel cannot disagree, and a clean
				//project pays nothing for it.
			DrcDuplicateTags::Mark member = marks.at(index);
			DrcFinding probe = DrcFinding::onElement(rule.identifier(),
								 rule.severity(),
								 QString(),
								 compared.at(index));
			DrcScanEngine::locate(project, probe);
			member.folio = probe.folio();
			member.position = probe.position();
			members << member;
		}

		const QString sentence =
				DrcDuplicateTags::duplicateText(members, reading);
		if (sentence.isEmpty()) {
			continue;
		}

		findings.append(DrcFinding::onElement(rule.identifier(),
						      rule.severity(),
						      sentence,
						      compared.at(group.first())));
	}

	return findings;
}

/**
	@brief DrcTagRules::registerRules
	@param set : the register the rules join
	@param engine : the engine that will walk them
	@return how many were taken

	Born an error and switched on: see the header for why this one is not
	a warning.
*/
int DrcTagRules::registerRules(DrcRuleSet &set, DrcScanEngine &engine)
{
	int taken = 0;

	if (engine.addRule(set,
			   DrcRule(duplicateTagIdentifier(),
				   QCoreApplication::translate(
					   "DrcTagRules",
					   "Two components with the same tag"),
				   DrcSeverity::Error,
				   DrcRoute::Scan),
			   &DrcTagRules::duplicateTags)) {
		++taken;
	}

	return taken;
}
