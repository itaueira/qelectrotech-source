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
#ifndef DRCSQLENGINE_H
#define DRCSQLENGINE_H

#include "../drc/drcrule.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class DrcRuleSet;
class QETProject;

/**
	@brief What became of the query behind one rule.

	Three states and not two, and the third one is the whole reason this
	enumeration exists rather than a bool.

	A checker that cannot tell a query that failed from a query that found
	nothing tells the draughtsman his project is clean when in truth
	nobody looked - which is the worst sentence a checker can say, because
	it ends the question. NotRun and Failed both come back with no row,
	exactly like Succeeded does on a project with nothing wrong, and only
	the status tells them apart.
*/
enum class DrcQueryStatus
{
	/// Nothing was asked. A default-built result, and the rule whose
	/// query the engine could not find.
	NotRun,
	/// The data base answered. The answer may well be no row, and that
	/// is the only "nothing found" worth the name.
	Succeeded,
	/// The data base refused. error() says why.
	Failed
};

/**
	@brief What one rule of the Sql route got back.

	It holds the answer and it holds the question: the statement that was
	sent is kept beside the rows, because a rule that finds something
	surprising is read by opening its query, and having to go and look it
	up somewhere else is how a wrong rule survives a review.

	The rows are kept as they came - column names, and one list of strings
	per row. No meaning is put on them here on purpose: which column holds
	the uuid of the offending element, and which one holds the sheet, is
	the business of the step that turns a row into something the panel can
	navigate to. Keeping that out means a rule may select whatever it
	needs without teaching this class about it.

	The severity is copied off the rule at the moment of the run, not
	looked up afterwards. It is the current severity - what the draughtsman
	lowered it to, if he lowered it - so a report stays true to the run
	that produced it even if the register is changed afterwards.
*/
class DrcSqlResult
{
	public:
		DrcSqlResult() {}
		DrcSqlResult(const QString &identifier, DrcSeverity severity);

		/// The rule this answers for. Machine key, never translated.
		QString identifier() const {return m_identifier;}
		/// The severity the rule carried when the run happened.
		DrcSeverity severity() const {return m_severity;}
		DrcQueryStatus status() const {return m_status;}
		/// The statement that was sent, empty when none was.
		QString statement() const {return m_statement;}
		/// Why the data base refused. Empty unless hasFailed().
		QString error() const {return m_error;}

		bool hasFailed() const {return m_status == DrcQueryStatus::Failed;}
		/// Whether the data base was asked at all.
		bool wasAsked() const {return m_status != DrcQueryStatus::NotRun;}

		/**
			Whether this rule found something wrong.

			False for a failure, on purpose: a query that could not run
			found nothing and did not find nothing. A caller that wants
			the difference asks hasFailed(), and one that forgets is at
			least not told that a broken rule is a satisfied rule.
		*/
		bool hasFindings() const;
		int findingCount() const;

		/// The column names of the result set, in the order selected.
		QStringList columns() const {return m_columns;}
		/// One list of values per row, in the order of columns().
		QList<QStringList> rows() const {return m_rows;}

		/// Where a column sits in a row, -1 when the query did not select it.
		int columnIndex(const QString &column) const;
		/// The value of one column of one row; empty when either is absent.
		QString value(int row, const QString &column) const;

		void setStatement(const QString &statement) {m_statement = statement;}
		void setFailed(const QString &reason);
		void setAnswer(const QStringList &columns,
			       const QList<QStringList> &rows);

	private:
		QString m_identifier;
		DrcSeverity m_severity = DrcSeverity::Warning;
		DrcQueryStatus m_status = DrcQueryStatus::NotRun;
		QString m_statement;
		QString m_error;
		QStringList m_columns;
		QList<QStringList> m_rows;
};

/**
	@brief What one whole run of the Sql route produced.

	One result per rule that was run - and only per rule that was run. A
	rule that was switched off has no entry here at all, which is the
	visible shape of the guard the engine holds: the report of a run with
	every rule off is empty, not a list of rules reporting nothing.

	It also carries two counters that exist to be asked by a test rather
	than by the program: how many statements were actually handed to the
	data base, and whether the data base was brought up to date. They are
	what turns "a switched-off rule builds no query" from a claim into a
	number - proving it by the absence of a result would not distinguish
	"never asked" from "asked and the answer thrown away".
*/
class DrcSqlReport
{
	public:
		DrcSqlReport() {}

		void append(const DrcSqlResult &result);

		QList<DrcSqlResult> results() const {return m_results;}
		/// How many rules were run. Zero means nothing was checked.
		int ruleCount() const {return m_results.count();}
		bool contains(const QString &identifier) const;
		/**
			@return the result of that rule, or a default-built one -
			whose wasAsked() is false - when the rule was not run.
		*/
		DrcSqlResult result(const QString &identifier) const;

		bool hasFailure() const;
		int failureCount() const;
		/// One line per failure, "identifier: reason", for a log or a message.
		QStringList failures() const;

		/// How many rows every rule found, failures counting for nothing.
		int findingCount() const;
		/**
			Whether anything found is at least that serious.

			The comparison goes through DrcRule::isAtLeast and not through
			the numbers, because that is the one place the order of the
			severities is written - the command line exit code of the step
			that follows asks this same question.
		*/
		bool hasFindingAtLeast(DrcSeverity threshold) const;

		/// How many statements were handed to the data base in this run.
		int queriesSent() const {return m_queries_sent;}
		void countQuerySent() {++m_queries_sent;}
		/// Whether updateDB() was called. False when there was nothing to ask.
		bool databaseUpdated() const {return m_database_updated;}
		void setDatabaseUpdated(bool updated) {m_database_updated = updated;}

		/**
			Whether this run may be reported as "nothing wrong found".

			Three things have to hold, and the third one is the one that
			gets forgotten: no rule failed, no rule found a row, **and at
			least one rule ran**. A run with every rule switched off
			checked nothing, and answering "clean" to it would be the same
			lie as answering "clean" to a run whose queries all failed -
			only quieter, because there is no error to notice.
		*/
		bool isClean() const;

	private:
		QList<DrcSqlResult> m_results;
		int m_queries_sent = 0;
		bool m_database_updated = false;
};

/**
	@brief Runs the rules whose answer lives in the project data base.

	The engine of the Sql route, and one half of the two routes the rule
	contract declares. It knows one thing the rest of the checker does
	not: that this kind of rule is a SELECT over the tables
	projectDataBase derives from the project XML - diagram_info and
	element_info, one column per information key, and the views built on
	them. What a rule of the other route walks has nothing in common with
	that, which is why there is no check() shared between the two.

	The statements live here and not in DrcRule, and that is a deliberate
	split: the register says which rules exist, what they are called, how
	serious they are and whether they are on; this engine says, for the
	rules of its own route, what question to ask. A rule of the Scan route
	therefore cannot be given a statement at all - addRule() refuses it -
	and that refusal is the point of the route being declared.

	@par A switched-off rule costs nothing, and "nothing" means nothing
	The filtering happens before a single query is built, not after the
	rows come back: run() asks the register whether it has any enabled
	rule of this route, and returns an empty report without so much as
	updating the data base when it has not. Then it iterates
	enabledRules(DrcRoute::Sql) and never the whole register, so a rule
	that is off is never looked up, never turned into SQL and never sent.
	The report counts what was sent, so the difference is measurable
	instead of merely intended.

	That matters more than it looks: updateDB() repopulates five tables
	from the whole project, so a checker that updated the data base before
	noticing it had nothing to ask would make "switch the rules off" cost
	the most expensive part of the run.

	@par A failure is never reported as a clean project
	newQuery() executes as it builds - QSqlQuery(query, db) runs the
	statement in its constructor - so what comes back is already a result
	set and the engine reads isActive() rather than calling exec() again.
	Asking it to execute a second time does more than double the work: on
	a statement that failed to prepare it also throws the reason away, and
	the error then reads "No query Unable to fetch row" instead of the "no
	such column" the data base actually answered. Measured next door, in
	the list model that had this same bug.
*/
class DrcSqlEngine
{
	public:
		DrcSqlEngine() {}

		/**
			Declare a rule of this route: register it and give it its query.

			The one call somebody writing a new rule has to make, and the
			only one that keeps the two registers in step. Everything is
			checked before anything is written, so a refusal leaves both
			of them exactly as they were.

			@param set the register the rule joins
			@param rule the rule, whose route must be DrcRoute::Sql
			@param statement the SELECT that answers it
			@return false, having changed nothing, when the rule is of the
			other route, when the statement is not a question (see
			isQuestion()), or when the register already holds that
			identifier.
		*/
		bool addRule(DrcRuleSet &set,
			     const DrcRule &rule,
			     const QString &statement);

		/**
			Give a query to a rule that is registered elsewhere.

			@return false when the identifier is empty, when the statement
			is not a question, or when that identifier already has a
			query. Refusing to overwrite is the same rule the register
			holds for identifiers, and for the same reason: a second
			statement quietly replacing the first would run something
			nobody read.
		*/
		bool setStatement(const QString &identifier,
				  const QString &statement);

		bool hasStatement(const QString &identifier) const;
		QString statement(const QString &identifier) const;
		int statementCount() const {return m_statements.count();}

		/**
			Whether a statement is a question rather than an order.

			A rule asks; it does not change anything. The data base it
			would change is the one every list drawn on a folio reads
			from, so a rule written with a DELETE where a SELECT was meant
			would empty a table under the windows that are showing it, and
			the next updateDB() would fill it back in - a defect that
			comes and goes and belongs to no window.

			The test is the leading keyword, SELECT or WITH. It is a
			coarse sieve on purpose: it must never refuse a legitimate
			question, and anything it lets through that is not one is
			refused by the data base itself, where it becomes an ordinary
			failure with a reason. Several statements in one string are
			not a way round it either - the driver refuses those on its
			own.
		*/
		static bool isQuestion(const QString &statement);

		DrcSqlReport run(QETProject *project, const DrcRuleSet &rules) const;

	private:
		/// identifier -> the SELECT that answers it
		QHash<QString, QString> m_statements;
};

#endif // DRCSQLENGINE_H
