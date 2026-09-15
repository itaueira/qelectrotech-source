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
#include "drcscanengine.h"

#include "drcruleset.h"

#include "../diagram.h"
#include "../diagramposition.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

/**
	@brief DrcScanReport::append
	@param finding : one thing a rule found wrong
*/
void DrcScanReport::append(const DrcFinding &finding)
{
	m_findings.append(finding);
}

/**
	@brief DrcScanReport::append
	@param findings : what one rule found, in the order it found it
*/
void DrcScanReport::append(const QList<DrcFinding> &findings)
{
	m_findings.append(findings);
}

/**
	@brief DrcScanReport::findingsOf
	@param identifier : the machine key of a rule
	@return what that rule found, in order. Empty when it found nothing -
	and empty as well when it did not run, which rulesRun() tells apart.
*/
QList<DrcFinding> DrcScanReport::findingsOf(const QString &identifier) const
{
	QList<DrcFinding> list_;
	for (const DrcFinding &finding : m_findings)
	{
		if (finding.ruleIdentifier() == identifier) {
			list_.append(finding);
		}
	}
	return list_;
}

/**
	@brief DrcScanReport::findingCountOf
	@param identifier : the machine key of a rule
	@return how many findings that rule produced
*/
int DrcScanReport::findingCountOf(const QString &identifier) const
{
	int count = 0;
	for (const DrcFinding &finding : m_findings)
	{
		if (finding.ruleIdentifier() == identifier) {
			++count;
		}
	}
	return count;
}

/**
	@brief DrcScanReport::hasFindingAtLeast
	@param threshold : the severity to test against
	@return whether anything found is at least that serious
*/
bool DrcScanReport::hasFindingAtLeast(DrcSeverity threshold) const
{
	return DrcFinding::hasAtLeast(m_findings, threshold);
}

/**
	@brief DrcScanReport::isClean
	@return whether this run may be reported as a project with nothing
	wrong: no finding, and at least one rule actually walked.
*/
bool DrcScanReport::isClean() const
{
	return m_findings.isEmpty() && m_rules_run > 0;
}

/**
	@brief DrcScanEngine::addRule
	@param set : the register the rule joins
	@param rule : the rule, whose route must be DrcRoute::Scan
	@param check : what it walks
	@return true when both registers took it
*/
bool DrcScanEngine::addRule(DrcRuleSet &set,
			    const DrcRule &rule,
			    DrcScanCheck check)
{
		//Everything is tested before anything is written: a refusal must
		//leave the register and the walks exactly as it found them, or a
		//rule ends up registered with no walk and reports nothing for ever.
	if (!rule.isValid() || rule.route() != DrcRoute::Scan) {
		return false;
	}
	if (!check) {
		return false;
	}
	if (m_checks.contains(rule.identifier())) {
		return false;
	}
	if (set.contains(rule.identifier())) {
		return false;
	}

	if (!set.add(rule)) {
		return false;
	}
	m_checks.insert(rule.identifier(), check);
	return true;
}

/**
	@brief DrcScanEngine::setCheck
	@param identifier : the machine key of a rule registered elsewhere
	@param check : what it walks
	@return false when the identifier is empty, the check empty, or that
	identifier already has a walk
*/
bool DrcScanEngine::setCheck(const QString &identifier, DrcScanCheck check)
{
	if (identifier.isEmpty() || !check) {
		return false;
	}
	if (m_checks.contains(identifier)) {
		return false;
	}

	m_checks.insert(identifier, check);
	return true;
}

/**
	@brief DrcScanEngine::hasCheck
	@param identifier : the machine key of a rule
	@return whether this engine knows what to walk for it
*/
bool DrcScanEngine::hasCheck(const QString &identifier) const
{
	return m_checks.contains(identifier);
}

namespace
{
	/**
		The sheet a finding sits on, as far as the finding itself knows.

		A rule hands back the object; which sheet it is drawn on is
		something the engine reads once, here, so that no rule has to
		remember to. A finding that already carries a sheet - one made by
		onDiagram() - keeps it.
	*/
	Diagram *sheetOf(const DrcFinding &finding)
	{
		if (finding.diagram()) {
			return finding.diagram();
		}
		if (finding.element()) {
			return finding.element()->diagram();
		}
		if (finding.conductor()) {
			return finding.conductor()->diagram();
		}
		return nullptr;
	}
}

/**
	@brief DrcScanEngine::run
	@param project : the project to walk
	@param rules : the register the rules and their current state live in
	@return what this route found. Empty, with rulesRun() at zero, when
	there was nothing to do - and that zero is what stops an empty report
	from being read as a clean project.
*/
DrcScanReport DrcScanEngine::run(QETProject *project,
				 const DrcRuleSet &rules) const
{
	DrcScanReport report;
	if (!project) {
		return report;
	}

		//Before anything is walked, and not after the findings are
		//filtered: a route whose rules are all switched off must not cost
		//one folio of walking.
	if (!rules.hasEnabledRule(DrcRoute::Scan)) {
		return report;
	}

	const QList<DrcRule> enabled = rules.enabledRules(DrcRoute::Scan);
	for (const DrcRule &rule : enabled)
	{
		const DrcScanCheck check = m_checks.value(rule.identifier());
		if (!check) {
				//Registered as a Scan rule and given no walk. It did not
				//run, so it is not counted as having run - which is the
				//difference isClean() rests on.
			continue;
		}

		report.countRuleRun();

		QList<DrcFinding> found = check(project, rule);
		for (DrcFinding &finding : found)
		{
			if (!finding.isValid()) {
				continue;
			}

				//The address of a finding is written here and nowhere
				//else. A rule that numbered its own folio would be a rule
				//that could number it differently from the next one, and
				//the panel would address the same sheet two ways.
			Diagram *sheet = sheetOf(finding);
			if (sheet)
			{
				finding.setDiagram(sheet);
				finding.setFolio(project->folioIndex(sheet) + 1);

				if (finding.element() && finding.position().isEmpty())
				{
					finding.setPosition(
						sheet->convertPosition(
							finding.element()->scenePos())
						.toString());
				}
			}

			report.append(finding);
		}
	}

	return report;
}
