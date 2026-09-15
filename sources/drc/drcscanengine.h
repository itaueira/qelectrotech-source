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
#ifndef DRCSCANENGINE_H
#define DRCSCANENGINE_H

#include "drcfinding.h"
#include "drcrule.h"

#include <QHash>
#include <QList>
#include <QString>

#include <functional>

class DrcRuleSet;
class QETProject;

/**
	@brief What one rule of the Scan route does.

	It is handed the project and the rule as it stands at the moment of
	the run - so that it may copy the current severity into every finding
	it makes - and it hands back what it found. Nothing else: no printing,
	no folio numbering, no sheet lookup. The engine fills the folio and
	the sheet of every finding afterwards, in one place, because a rule
	that did it itself would be a rule that could get it wrong on its own.
*/
using DrcScanCheck = std::function<QList<DrcFinding>(QETProject *,
						     const DrcRule &)>;

/**
	@brief What one whole run of the Scan route produced.

	The same shape as DrcSqlReport, and deliberately so: the two routes
	answer the same question and whoever reads a report should not have to
	learn two vocabularies. The findings are flat - a walk produces
	findings, not result sets - and the two counters are there for the
	same reason the Sql ones are: to turn "a switched-off rule costs
	nothing" into a number, since proving it by an empty list would not
	tell "never walked" from "walked and found nothing".
*/
class DrcScanReport
{
	public:
		DrcScanReport() {}

		void append(const DrcFinding &finding);
		void append(const QList<DrcFinding> &findings);

		QList<DrcFinding> findings() const {return m_findings;}
		int findingCount() const {return m_findings.count();}
		/// The findings of one rule, in the order they were found.
		QList<DrcFinding> findingsOf(const QString &identifier) const;
		int findingCountOf(const QString &identifier) const;

		/// How many rules were actually walked. Zero means nothing was checked.
		int rulesRun() const {return m_rules_run;}
		void countRuleRun() {++m_rules_run;}

		bool hasFindingAtLeast(DrcSeverity threshold) const;

		/**
			Whether this run may be reported as "nothing wrong found".

			The same three conditions the Sql report holds, minus the one
			that cannot happen here: no finding, **and at least one rule
			walked**. A run in which every rule was switched off checked
			nothing, and calling that a clean project is the one sentence
			a checker must never say by mistake.
		*/
		bool isClean() const;

	private:
		QList<DrcFinding> m_findings;
		int m_rules_run = 0;
};

/**
	@brief Runs the rules whose answer the project data base does not hold.

	The engine of the Scan route, and the other half of the two routes the
	rule contract declares. What it walks are the objects the program has
	in memory - the folios, the components on them, the links between a
	coil and its contacts, the terminals that belong to a strip and the
	ones that belong to none.

	@par Why this is not SQL, measured and not preferred
	projectDataBase fills its terminal table along the conductor path only
	(insertTerminal() is called from the two places that record a
	conductor), so a terminal with no wire never reaches it; and
	populateElementTable() filters the components by type, so a slave
	contact has no row in any table at all. A rule about either one,
	written as a SELECT, would run, come back with nothing, and report a
	clean project. That is the failure the route declaration exists to
	prevent, and this engine is the place the rules that avoid it live.

	@par The check is a function, where the Sql engine holds a statement
	The split is the same one: the register says which rules exist, what
	they are called, how serious they are and whether they are on; the
	engine of each route says what to do for the rules of that route. A
	rule of the Sql route therefore cannot be given a walk at all -
	addRule() refuses it - exactly as the other engine refuses to give a
	statement to a rule of this one.

	@par A switched-off rule costs nothing
	run() asks the register whether it holds any enabled rule of this
	route and returns an empty report without walking a single folio when
	it does not. Then it iterates enabledRules(DrcRoute::Scan) and never
	the whole register, so a rule that is off is never looked up and its
	walk never runs. The report counts the rules that were walked, so the
	difference is measured rather than intended.

	@par The folio and the position are filled here, once
	Every finding that comes back with a sheet gets its folio number from
	QETProject::folioIndex() + 1, and every finding on a component gets
	its position from the grid of the border. Doing it here rather than in
	each rule is what makes a finding of one rule addressed the same way
	as a finding of another - and the same way the parts report and the
	location report address a component, which is the whole point of the
	panel and those reports speaking one language.
*/
class DrcScanEngine
{
	public:
		DrcScanEngine() {}

		/**
			Declare a rule of this route: register it and give it its walk.

			The one call somebody writing a new rule of this route has to
			make. Everything is checked before anything is written, so a
			refusal leaves both registers exactly as they were.

			@param set the register the rule joins
			@param rule the rule, whose route must be DrcRoute::Scan
			@param check what it walks
			@return false, having changed nothing, when the rule is of the
			other route, when the check is empty, or when the register
			already holds that identifier.
		*/
		bool addRule(DrcRuleSet &set,
			     const DrcRule &rule,
			     DrcScanCheck check);

		/**
			Give a walk to a rule that is registered elsewhere.

			@return false when the identifier is empty, when the check is
			empty, or when that identifier already has one. Refusing to
			overwrite is the rule the register holds for identifiers, for
			the same reason: a second walk quietly replacing the first
			would run something nobody read.
		*/
		bool setCheck(const QString &identifier, DrcScanCheck check);

		bool hasCheck(const QString &identifier) const;
		int checkCount() const {return m_checks.count();}

		DrcScanReport run(QETProject *project,
				  const DrcRuleSet &rules) const;

	private:
		/// identifier -> the walk that answers it
		QHash<QString, DrcScanCheck> m_checks;
};

#endif // DRCSCANENGINE_H
