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
#include "drcsqlengine.h"

#include "../drc/drcruleset.h"
#include "../qetproject.h"
#include "projectdatabase.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace
{
	/// The sentences this engine says. Translated where they are made,
	/// like DrcRule does: none of these classes is a QObject.
	QString noStatementMessage()
	{
		return QCoreApplication::translate("DrcSqlEngine",
			"Aucune requête n'est enregistrée pour cette règle.");
	}

	QString noProjectMessage()
	{
		return QCoreApplication::translate("DrcSqlEngine",
			"Aucun projet à vérifier.");
	}

	QString queryFailedMessage(const QString &reason)
	{
		if (reason.isEmpty()) {
			return QCoreApplication::translate("DrcSqlEngine",
				"La requête n'a pas pu être exécutée.");
		}
		return QCoreApplication::translate("DrcSqlEngine",
			"La requête n'a pas pu être exécutée : %1").arg(reason);
	}
}

/**
	@brief DrcSqlResult::DrcSqlResult
	@param identifier : the rule this answers for
	@param severity : the severity that rule carried at the moment of the
	run, which is not necessarily the one it was written with
*/
DrcSqlResult::DrcSqlResult(const QString &identifier, DrcSeverity severity) :
	m_identifier(identifier),
	m_severity(severity)
{
}

/**
	@brief DrcSqlResult::hasFindings
	@return whether the query ran **and** brought something back. A failed
	query answers false here and true to hasFailed(), so the two are never
	the same answer.
*/
bool DrcSqlResult::hasFindings() const
{
	return m_status == DrcQueryStatus::Succeeded && !m_rows.isEmpty();
}

/**
	@brief DrcSqlResult::findingCount
	@return how many rows the query brought back, and zero for a query
	that did not run - a failure has no findings, it has a reason.
*/
int DrcSqlResult::findingCount() const
{
	return m_status == DrcQueryStatus::Succeeded ? m_rows.count() : 0;
}

/**
	@brief DrcSqlResult::columnIndex
	@param column : the name the query selected it under
	@return where it sits in each row, or -1 when the query did not select
	it. The caller tests for -1; there is no "column zero by default",
	because a rule that selects the wrong columns would then read the
	first one and look like it worked.
*/
int DrcSqlResult::columnIndex(const QString &column) const
{
	return m_columns.indexOf(column);
}

/**
	@brief DrcSqlResult::value
	@param row : which row, 0-based
	@param column : the name the query selected it under
	@return the value as text, or an empty string when the row or the
	column is not there
*/
QString DrcSqlResult::value(int row, const QString &column) const
{
	if (row < 0 || row >= m_rows.count()) {
		return QString();
	}
	const int index = columnIndex(column);
	if (index < 0 || index >= m_rows.at(row).count()) {
		return QString();
	}
	return m_rows.at(row).at(index);
}

/**
	@brief DrcSqlResult::setFailed
	The query did not run. Whatever rows were held are dropped, so that no
	caller can read half an answer and take it for the whole one.
	@param reason : what the data base said, already turned into a sentence
*/
void DrcSqlResult::setFailed(const QString &reason)
{
	m_status = DrcQueryStatus::Failed;
	m_error = reason;
	m_columns.clear();
	m_rows.clear();
}

/**
	@brief DrcSqlResult::setAnswer
	The query ran. An empty list of rows is a perfectly good answer here -
	it is the one the engine exists to be able to tell apart from a
	failure.
	@param columns : the names of the result set, in the order selected
	@param rows : one list of values per row
*/
void DrcSqlResult::setAnswer(const QStringList &columns,
			     const QList<QStringList> &rows)
{
	m_status = DrcQueryStatus::Succeeded;
	m_error.clear();
	m_columns = columns;
	m_rows = rows;
}

void DrcSqlReport::append(const DrcSqlResult &result)
{
	m_results.append(result);
}

bool DrcSqlReport::contains(const QString &identifier) const
{
	for (const DrcSqlResult &result : m_results) {
		if (result.identifier() == identifier) {
			return true;
		}
	}
	return false;
}

/**
	@brief DrcSqlReport::result
	@param identifier : the rule asked about
	@return its result, or a default-built one when that rule was not run.
	The absent result says wasAsked() false, which is what a caller has to
	see: a rule that was switched off has no result, and reading a blank
	one as "found nothing" is the mistake this class is built to refuse.
*/
DrcSqlResult DrcSqlReport::result(const QString &identifier) const
{
	for (const DrcSqlResult &result : m_results) {
		if (result.identifier() == identifier) {
			return result;
		}
	}
	return DrcSqlResult();
}

bool DrcSqlReport::hasFailure() const
{
	for (const DrcSqlResult &result : m_results) {
		if (result.hasFailed()) {
			return true;
		}
	}
	return false;
}

int DrcSqlReport::failureCount() const
{
	int count = 0;
	for (const DrcSqlResult &result : m_results) {
		if (result.hasFailed()) {
			++count;
		}
	}
	return count;
}

/**
	@brief DrcSqlReport::failures
	@return one line per failed rule, naming the rule and the reason. A
	list and not a count, because a count sends whoever reads it back to
	find out which rule broke - and a broken rule is exactly the thing
	that has to be named to be fixed.
*/
QStringList DrcSqlReport::failures() const
{
	QStringList list;
	for (const DrcSqlResult &result : m_results) {
		if (result.hasFailed()) {
				//Plain +, not the % of QStringBuilder: % needs
				//<QStringBuilder> included, and this file does not
				//need the header for one join.
			list << result.identifier()
				+ QStringLiteral(": ") + result.error();
		}
	}
	return list;
}

int DrcSqlReport::findingCount() const
{
	int count = 0;
	for (const DrcSqlResult &result : m_results) {
		count += result.findingCount();
	}
	return count;
}

bool DrcSqlReport::hasFindingAtLeast(DrcSeverity threshold) const
{
	for (const DrcSqlResult &result : m_results) {
		if (result.hasFindings()
			&& DrcRule::isAtLeast(result.severity(), threshold)) {
			return true;
		}
	}
	return false;
}

/**
	@brief DrcSqlReport::isClean
	@return whether this run may be reported as "nothing wrong found". See
	the header for why a run in which nothing was checked does not count.
*/
bool DrcSqlReport::isClean() const
{
	return ruleCount() > 0 && !hasFailure() && findingCount() == 0;
}

/**
	@brief DrcSqlEngine::addRule
	@param set : the register the rule joins
	@param rule : the rule, which must declare DrcRoute::Sql
	@param statement : the SELECT that answers it
	@return false, having changed nothing at all, when any of the three
	is wrong
*/
bool DrcSqlEngine::addRule(DrcRuleSet &set,
			   const DrcRule &rule,
			   const QString &statement)
{
		//Everything is checked before anything is written. The register
		//and the statement table have to agree, and a rule added to one
		//of them and refused by the other is the disagreement itself.
	if (!rule.isValid() || rule.route() != DrcRoute::Sql) {
		return false;
	}
	if (!isQuestion(statement)) {
		return false;
	}
	if (set.contains(rule.identifier())
		|| m_statements.contains(rule.identifier())) {
		return false;
	}

	if (!set.add(rule)) {
		return false;
	}
	m_statements.insert(rule.identifier(), statement);
	return true;
}

bool DrcSqlEngine::setStatement(const QString &identifier,
				const QString &statement)
{
	if (identifier.isEmpty() || !isQuestion(statement)) {
		return false;
	}
	if (m_statements.contains(identifier)) {
		return false;
	}
	m_statements.insert(identifier, statement);
	return true;
}

bool DrcSqlEngine::hasStatement(const QString &identifier) const
{
	return m_statements.contains(identifier);
}

QString DrcSqlEngine::statement(const QString &identifier) const
{
	return m_statements.value(identifier);
}

/**
	@brief DrcSqlEngine::isQuestion
	@param statement : the text of a rule's query
	@return whether it opens with SELECT or WITH. See the header for why
	the sieve is coarse on purpose.
*/
bool DrcSqlEngine::isQuestion(const QString &statement)
{
	const QString trimmed = statement.trimmed();
	if (trimmed.isEmpty()) {
		return false;
	}
	return trimmed.startsWith(QLatin1String("select"), Qt::CaseInsensitive)
		|| trimmed.startsWith(QLatin1String("with"), Qt::CaseInsensitive);
}

/**
	@brief DrcSqlEngine::run
	Run every enabled rule of the Sql route against the data base of
	@p project.
	@param project : the project to check; a null one is a failed run and
	not a clean project
	@param rules : the register the enabled rules are read from
	@return one result per rule that ran, and nothing at all for the rules
	that did not
*/
DrcSqlReport DrcSqlEngine::run(QETProject *project,
			       const DrcRuleSet &rules) const
{
	DrcSqlReport report;

		//The guard, and it comes first for a reason: updateDB()
		//repopulates five tables out of the whole project, so asking it
		//before finding out there is nothing to ask would make a run with
		//every rule switched off cost the most expensive part of a full
		//one. Nothing below this line happens when no rule of this route
		//is on - not a query built, not a table read, not even the
		//project dereferenced.
	if (!rules.hasEnabledRule(DrcRoute::Sql)) {
		return report;
	}

	const QList<DrcRule> enabled = rules.enabledRules(DrcRoute::Sql);

		//No project: every rule of the route reports a failure, one each,
		//rather than the run coming back empty. An empty report would be
		//indistinguishable from the guard above having fired, and the two
		//mean opposite things - "nothing to check" against "there was
		//something to check and it could not be done".
	projectDataBase *data_base = project ? project->dataBase() : nullptr;
	if (!data_base) {
		for (const DrcRule &rule : enabled) {
			DrcSqlResult result(rule.identifier(), rule.severity());
			result.setFailed(noProjectMessage());
			report.append(result);
		}
		return report;
	}

		//The data base is built as the project is read and then follows
		//it, but a rule has to see the project as it is now: a sheet
		//added or a label changed between two runs has to be in the
		//tables before the first question is asked.
	data_base->updateDB();
	report.setDatabaseUpdated(true);

	for (const DrcRule &rule : enabled)
	{
		DrcSqlResult result(rule.identifier(), rule.severity());

		const QString statement = m_statements.value(rule.identifier());
		if (statement.isEmpty())
		{
				//A rule that says it is answered by a query, and has
				//none. It is a mistake in whoever wrote the rule, and
				//it is loud on purpose: silently skipping it would
				//leave a rule that is switched on, listed, and asking
				//nothing.
			result.setFailed(noStatementMessage());
			report.append(result);
			continue;
		}
		result.setStatement(statement);

			//newQuery() executes as it builds - QSqlQuery(query, db)
			//runs the statement in its constructor - so what comes back
			//is already a result set. exec() is deliberately not called
			//on it: on a statement that failed to prepare, a second
			//execution throws away the reason the data base gave and
			//leaves "Unable to fetch row" in its place.
		QSqlQuery query = data_base->newQuery(statement);
		report.countQuerySent();

		if (!query.isActive())
		{
			result.setFailed(
				queryFailedMessage(
					query.lastError().text().trimmed()));
			report.append(result);
			continue;
		}

			//Read once, outside the row loop: the result set is fixed
			//for the whole pass, only the row moves.
		const QSqlRecord record = query.record();
		const int column_count = record.count();

		QStringList columns;
		columns.reserve(column_count);
		for (int i = 0 ; i < column_count ; ++i) {
			columns << record.fieldName(i);
		}

		QList<QStringList> rows;
		while (query.next())
		{
			QStringList values;
			values.reserve(column_count);
			for (int i = 0 ; i < column_count ; ++i) {
				values << query.value(i).toString();
			}
			rows.append(values);
		}

			//An empty list of rows lands here, as a success, and that is
			//the whole point: this is the only place in the run where
			//"nothing found" is said, and it is said by a query that ran.
		result.setAnswer(columns, rows);
		report.append(result);
	}

	return report;
}
