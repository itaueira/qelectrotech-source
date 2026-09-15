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
#ifndef DRCPROJECTACTIONS_H
#define DRCPROJECTACTIONS_H

#include "../drcfinding.h"

#include <QList>
#include <QString>
#include <QStringList>

class DrcRuleSet;
class DrcScanEngine;
class DrcSqlEngine;
class QETProject;
class QWidget;

/**
	@brief Running the checker on a project, and showing what it found.

	What the "check now" command does, kept out of the editor window so
	that the window only has to build an action and call one function -
	the same arrangement the catalogue reports use.

	@par The two routes are timed apart, and that is the point of the
	measurement
	One number for the whole run would not say which half is expensive,
	and the two halves cost for different reasons: the query route pays
	for rebuilding the project data base from the whole project, and the
	walk route pays per folio and per component. A run whose total gets
	worse tells nobody anything; a run whose walk doubled while the
	queries stayed put names the half to look at.

	@par The check runs only when it is asked
	Nothing here is hooked to saving the project. What that would cost is
	exactly what these two numbers are being collected to find out, and
	making every save pay an unknown price before the price is known is
	the wrong order.
*/
namespace DrcProjectActions
{
	/**
		@brief What one whole check produced, both routes together.
	*/
	struct CheckOutcome
	{
		/// Everything found, the walk route first, in rule order.
		QList<DrcFinding> findings;

		/// How many rules of each route actually ran.
		int scan_rules_run = 0;
		int sql_rules_run = 0;

		/// How long each route took, in milliseconds.
		qint64 scan_msec = 0;
		qint64 sql_msec = 0;

		/// One line per query that the data base refused, "identifier: reason".
		QStringList failures;

		int ruleCount() const {return scan_rules_run + sql_rules_run;}

		/**
			Whether the project may be reported as having nothing wrong.

			Three conditions, and the third is the one that gets
			forgotten: nothing found, nothing failed, **and at least one
			rule ran**. A run with every rule switched off checked
			nothing, and saying "clean" to that is the one sentence a
			checker must never say by mistake.
		*/
		bool isClean() const;

		/// The sentence the panel puts above the table.
		QString summary() const;
	};

	/**
		Fill @a rules, @a scan and @a sql with what this delivery ships.

		The factory profile: every rule that exists, each with the
		severity it was written with. There is one profile and it carries
		no company name - what a given office considers an error is
		written into the project file, rule by rule, and not compiled in
		here.

		@return how many rules were registered
	*/
	int buildFactoryCheck(DrcRuleSet &rules,
			      DrcScanEngine &scan,
			      DrcSqlEngine &sql);

	/// Run the factory profile on @a project.
	CheckOutcome runCheck(QETProject *project);

	/**
		Run a profile of your own on @a project.

		@param project the project to check
		@param rules the register, carrying the current severity and the
		current enabled state of every rule
		@param scan the engine of the walk route
		@param sql the engine of the query route
	*/
	CheckOutcome runCheck(QETProject *project,
			      const DrcRuleSet &rules,
			      const DrcScanEngine &scan,
			      const DrcSqlEngine &sql);

	/**
		Run the factory profile and put the result on the screen.

		The window it opens is **not modal**, and that is required rather
		than chosen: the panel has to stay up while the reader walks from
		one finding to the next on the folio behind it. A window already
		open on the same project is reused and refilled instead of a
		second one being stacked on the first.

		@param project the project to check
		@param parent the window the report belongs to
	*/
	void showCheckReport(QETProject *project, QWidget *parent);
}

#endif // DRCPROJECTACTIONS_H
