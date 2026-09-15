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
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/dataBase/drcsqlengine.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/drc/drcrule.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QSignalSpy>
#include <QString>
#include <QStringList>

/*
	The engine of the Sql route: what it asks, what it refuses to ask, and
	how it tells a query that broke from a query that found nothing.

	It needs a project open, so it lives here and not beside the rule
	contract, which is proved without one in drcrule_test.cpp of
	C_unittests. The split is the same as everywhere else in this suite: the
	register, the severity ordering and the refusal of a duplicate
	identifier are arithmetic over a list and are settled a floor below;
	what is here is the half that a list cannot have - a data base with rows
	in it, and a question sent to it.

	Three things are measured, and each one can be cut without the pure
	suite noticing:

	1. a rule written from scratch over diagram_info finds what it was
	   written to find (CU-23.1);
	2. a rule that is switched off produces no SQL at all - not a filtered
	   result, not a query built and thrown away, nothing sent;
	3. a query that failed is never reported as a project with nothing
	   wrong, which is the one sentence a checker must never say by
	   mistake.
*/

namespace
{
	/**
		Three sheets, each one carrying the title it was given.

		No component and no wire: the rules below read diagram_info and
		element_info, and what the second one has to show here is that an
		empty table is an answer and not a failure. A fixture that drew
		something would still prove the first rule and would stop proving
		the second.
	*/
	QString projectXml(const QString &first_title,
			   const QString &second_title,
			   const QString &third_title)
	{
		const QString sheet = QStringLiteral(
			"<diagram title=\"%1\" order=\"%2\" height=\"600\""
			" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements/><inputs/><conductors/>"
			"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "%1%2%3"
			       "</project>")
		       .arg(sheet.arg(first_title).arg(1),
			    sheet.arg(second_title).arg(2),
			    sheet.arg(third_title).arg(3));
	}

	/**
		The rule of CU-23.1, written out as somebody writing a new rule
		would write it: one SELECT over the sheets of the project.

		It reads project_summary_view rather than diagram_info because of
		one column: pos, the rank of the sheet in the project, which is
		what a finding has to name for anybody to go and look. Everything
		else it selects is a title block information key, and the view
		publishes all nine of them.

		trim() is there on purpose. A title block filled with three spaces
		is a title block nobody filled in, and a rule that only tested for
		NULL and '' would report that sheet as done.
	*/
	const char *const empty_title_sql =
		"SELECT pos, title FROM project_summary_view"
		" WHERE title IS NULL OR trim(title) = ''"
		" ORDER BY pos";

	/// A question the data base cannot answer: there is no such column.
	const char *const broken_sql =
		"SELECT colonne_absente FROM diagram_info";

	/// A question whose answer is legitimately nothing on this fixture.
	const char *const unlabelled_element_sql =
		"SELECT element_uuid FROM element_info"
		" WHERE label IS NULL OR trim(label) = ''";

	DrcRule sqlRule(const char *identifier,
			DrcSeverity severity = DrcSeverity::Warning)
	{
		return DrcRule(QLatin1String(identifier),
			       QStringLiteral("Règle de vérification."),
			       severity,
			       DrcRoute::Sql);
	}
}

TEST_CASE("CU-23.1 — uma regra de consulta escrita do zero acha as folhas sem carimbo preenchido",
	  "[uibench][drc][database]")
{
	DrcRuleSet rules;
	DrcSqlEngine engine;

		//The whole of what writing a rule takes: name it, say how serious
		//it is, say where its data lives, and write the question. If this
		//ever needs more than these four, the contract of the step before
		//has failed and the dozens of rules that are to come will not be
		//written.
	REQUIRE(engine.addRule(rules,
			       sqlRule("folio.title.empty"),
			       QLatin1String(empty_title_sql)));

	SECTION("as folhas sem título são apontadas, e a que tem título não é")
	{
		UiBench::ScratchProject bench(
				projectXml(QStringLiteral("Commande"),
					   QString(),
					   QStringLiteral("   ")),
				QStringLiteral("drcsql.qet"));
		INFO(bench.error().toStdString());
		REQUIRE(bench.isOpen());
		REQUIRE(bench.diagramCount() == 3);

		const DrcSqlReport report = engine.run(bench.project(), rules);

		INFO("failures: "
		     << report.failures().join(QStringLiteral(" | ")).toStdString());
		REQUIRE_FALSE(report.hasFailure());
		REQUIRE(report.ruleCount() == 1);
		CHECK(report.queriesSent() == 1);
		CHECK(report.databaseUpdated());

		const DrcSqlResult result =
				report.result(QStringLiteral("folio.title.empty"));
		REQUIRE(result.wasAsked());
		REQUIRE(result.status() == DrcQueryStatus::Succeeded);

			//Two, and which two: a count alone would pass if the rule
			//pointed at the wrong pair of sheets.
		REQUIRE(result.findingCount() == 2);
		CHECK(result.value(0, QStringLiteral("pos")) == QStringLiteral("2"));
		CHECK(result.value(1, QStringLiteral("pos")) == QStringLiteral("3"));

			//The sheet of three spaces is in the list, and that is the
			//half of the rule trim() buys.
		CHECK(result.value(1, QStringLiteral("title")).trimmed().isEmpty());

			//The columns come back named, which is what the step that
			//turns a row into something the panel can navigate to will
			//read the sheet number off.
		CHECK(result.columns() == QStringList({QStringLiteral("pos"),
						       QStringLiteral("title")}));

		CHECK(report.findingCount() == 2);
		CHECK(report.hasFindingAtLeast(DrcSeverity::Warning));
		CHECK_FALSE(report.hasFindingAtLeast(DrcSeverity::Error));
		CHECK_FALSE(report.isClean());
	}

	SECTION("um projeto em que toda folha tem título é limpo, e foi perguntado")
	{
		UiBench::ScratchProject bench(
				projectXml(QStringLiteral("Commande"),
					   QStringLiteral("Puissance"),
					   QStringLiteral("Bornier")),
				QStringLiteral("drcsql.qet"));
		INFO(bench.error().toStdString());
		REQUIRE(bench.isOpen());

		const DrcSqlReport report = engine.run(bench.project(), rules);

		INFO("failures: "
		     << report.failures().join(QStringLiteral(" | ")).toStdString());
		REQUIRE(report.ruleCount() == 1);
		REQUIRE_FALSE(report.hasFailure());

		const DrcSqlResult result =
				report.result(QStringLiteral("folio.title.empty"));
		CHECK(result.status() == DrcQueryStatus::Succeeded);
		CHECK(result.findingCount() == 0);
		CHECK_FALSE(result.hasFindings());

			//The one state in which the project may be called clean: a
			//rule ran, it did not break, and it found nothing.
		CHECK(report.isClean());
	}

	SECTION("a mesma pergunta duas vezes dá a mesma resposta")
	{
			//Nothing is kept between runs - there is no cache to go stale
			//- and this is what says so. A checker whose answer moves
			//without the project moving is a checker nobody trusts twice.
		UiBench::ScratchProject bench(
				projectXml(QStringLiteral("Commande"),
					   QString(),
					   QStringLiteral("   ")),
				QStringLiteral("drcsql.qet"));
		REQUIRE(bench.isOpen());

		const DrcSqlReport first = engine.run(bench.project(), rules);
		const DrcSqlReport second = engine.run(bench.project(), rules);

		REQUIRE(first.ruleCount() == second.ruleCount());
		CHECK(first.findingCount() == second.findingCount());

		const DrcSqlResult first_result =
				first.result(QStringLiteral("folio.title.empty"));
		const DrcSqlResult second_result =
				second.result(QStringLiteral("folio.title.empty"));
		REQUIRE(first_result.findingCount() == 2);
		CHECK(first_result.value(0, QStringLiteral("pos"))
		      == second_result.value(0, QStringLiteral("pos")));
		CHECK(first_result.value(1, QStringLiteral("pos"))
		      == second_result.value(1, QStringLiteral("pos")));
	}
}

TEST_CASE("T23 — regra desligada não gera consulta nenhuma",
	  "[uibench][drc][database]")
{
	UiBench::ScratchProject bench(
			projectXml(QStringLiteral("Commande"),
				   QString(),
				   QStringLiteral("   ")),
			QStringLiteral("drcsql.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.project()->dataBase() != nullptr);

	DrcRuleSet rules;
	DrcSqlEngine engine;

	SECTION("a regra desligada é barrada antes do SQL, e o SQL dela quebraria")
	{
		/*
			The proof that the filtering happens before the query is built
			and not after the rows come back, and it is the point of this
			whole case. The switched-off rule carries a statement the data
			base cannot answer: had it been sent, the report would carry a
			failure. It carries none.

			Written this way on purpose. Asserting that the report has no
			entry for the rule would not distinguish "never asked" from
			"asked, and the answer dropped on the way out" - which is
			exactly the mistake this is guarding against.
		*/
		REQUIRE(engine.addRule(rules,
				       sqlRule("folio.title.empty"),
				       QLatin1String(empty_title_sql)));
		REQUIRE(engine.addRule(rules,
				       sqlRule("rule.that.would.break"),
				       QLatin1String(broken_sql)));
		REQUIRE(rules.setEnabled(QStringLiteral("rule.that.would.break"),
					 false));

		const DrcSqlReport off = engine.run(bench.project(), rules);
		INFO("failures: "
		     << off.failures().join(QStringLiteral(" | ")).toStdString());
		CHECK_FALSE(off.hasFailure());
		CHECK(off.ruleCount() == 1);
		CHECK(off.queriesSent() == 1);
		CHECK_FALSE(off.contains(QStringLiteral("rule.that.would.break")));

			//The result of a rule that did not run is not an empty
			//result: it says nobody asked.
		CHECK_FALSE(off.result(QStringLiteral("rule.that.would.break"))
			    .wasAsked());

			//And switched back on, the very same statement fails - which
			//is what makes the silence above mean something.
		REQUIRE(rules.setEnabled(QStringLiteral("rule.that.would.break"),
					 true));
		const DrcSqlReport on = engine.run(bench.project(), rules);
		CHECK(on.ruleCount() == 2);
		CHECK(on.queriesSent() == 2);
		CHECK(on.hasFailure());
	}

	SECTION("com todas as regras desligadas o banco nem é atualizado")
	{
		REQUIRE(engine.addRule(rules,
				       sqlRule("folio.title.empty"),
				       QLatin1String(empty_title_sql)));
		REQUIRE(engine.addRule(rules,
				       sqlRule("element.label.empty"),
				       QLatin1String(unlabelled_element_sql)));
		REQUIRE(rules.setEnabled(QStringLiteral("folio.title.empty"), false));
		REQUIRE(rules.setEnabled(QStringLiteral("element.label.empty"),
					 false));
		REQUIRE_FALSE(rules.hasEnabledRule(DrcRoute::Sql));

			//updateDB() repopulates five tables out of the whole project
			//and announces itself once when it is done. Counting that
			//announcement is how "costs nothing" is measured from
			//outside the engine, instead of being taken from what the
			//engine says about itself.
		QSignalSpy spy(bench.project()->dataBase(),
			       &projectDataBase::dataBaseUpdated);
		REQUIRE(spy.isValid());

		const DrcSqlReport report = engine.run(bench.project(), rules);

		CHECK(spy.count() == 0);
		CHECK_FALSE(report.databaseUpdated());
		CHECK(report.queriesSent() == 0);
		CHECK(report.ruleCount() == 0);

			//Nothing was checked, so nothing may be called clean. This is
			//the third state, and the reason isClean() is not simply
			//"no finding and no failure".
		CHECK_FALSE(report.isClean());
	}

	SECTION("uma regra ligada faz o banco ser atualizado uma vez")
	{
		REQUIRE(engine.addRule(rules,
				       sqlRule("folio.title.empty"),
				       QLatin1String(empty_title_sql)));

		QSignalSpy spy(bench.project()->dataBase(),
			       &projectDataBase::dataBaseUpdated);
		REQUIRE(spy.isValid());

		const DrcSqlReport report = engine.run(bench.project(), rules);

		CHECK(spy.count() == 1);
		CHECK(report.databaseUpdated());
	}

	SECTION("regra da outra rota não é deste motor")
	{
			//A rule of the Scan route, switched on, and this engine does
			//nothing at all: it does not run it, and it does not update
			//the data base for it either. Which engine runs which rule is
			//settled by the route the rule declares, and not by which
			//engine happens to be called first.
		REQUIRE(rules.add(DrcRule(QStringLiteral("terminal.unconnected"),
					  QStringLiteral("Borne sans conducteur."),
					  DrcSeverity::Error,
					  DrcRoute::Scan)));
		REQUIRE(rules.hasEnabledRule(DrcRoute::Scan));

		QSignalSpy spy(bench.project()->dataBase(),
			       &projectDataBase::dataBaseUpdated);
		REQUIRE(spy.isValid());

		const DrcSqlReport report = engine.run(bench.project(), rules);

		CHECK(report.ruleCount() == 0);
		CHECK(report.queriesSent() == 0);
		CHECK(spy.count() == 0);

			//And a rule of that route cannot be given a statement at all.
			//The route is declared so that the question of where the data
			//lives is answered once; a Scan rule holding a SELECT would be
			//the guess the declaration exists to prevent.
		CHECK_FALSE(engine.addRule(
				rules,
				DrcRule(QStringLiteral("terminal.other"),
					QStringLiteral("Borne sans conducteur."),
					DrcSeverity::Error,
					DrcRoute::Scan),
				QLatin1String(empty_title_sql)));
		CHECK_FALSE(rules.contains(QStringLiteral("terminal.other")));
	}

	SECTION("o motor recusa o que não é pergunta, e não registra nada")
	{
			//A rule asks; it does not change anything. The data base it
			//would change is the one every list drawn on a folio reads
			//from.
		CHECK_FALSE(DrcSqlEngine::isQuestion(
				QStringLiteral("DELETE FROM element_info")));
		CHECK_FALSE(DrcSqlEngine::isQuestion(QString()));
		CHECK(DrcSqlEngine::isQuestion(QLatin1String(empty_title_sql)));
		CHECK(DrcSqlEngine::isQuestion(
			QStringLiteral("  with x as (select 1) select * from x")));

		CHECK_FALSE(engine.addRule(
				rules,
				sqlRule("rule.that.writes"),
				QStringLiteral("DELETE FROM element_info")));

			//Refused leaves both registers exactly as they were: a rule
			//in one of them and not in the other is the disagreement the
			//single entry point exists to avoid.
		CHECK(rules.count() == 0);
		CHECK(engine.statementCount() == 0);
	}
}

TEST_CASE("T23 — consulta quebrada não é projeto limpo",
	  "[uibench][drc][database]")
{
	/*
		The three answers a rule can come back with, side by side in one
		run, because it is only side by side that they are worth anything:
		two of them come back with no row, and a checker that cannot tell
		them apart says "nothing wrong found" about a project nobody
		looked at.

		The same rule was fixed one floor over, in the list drawn on a
		folio: a query that could not run used to draw a table with no
		column and no row, which on the sheet looked exactly like a list
		with nothing in it. A checker saying it has the same duty and a
		harder one - a wrong list is read as wrong sooner or later, a
		wrong "no problem" is never read at all.
	*/
	UiBench::ScratchProject bench(
			projectXml(QStringLiteral("Commande"),
				   QString(),
				   QStringLiteral("   ")),
			QStringLiteral("drcsql.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	DrcRuleSet rules;
	DrcSqlEngine engine;

	REQUIRE(engine.addRule(rules,
			       sqlRule("folio.title.empty", DrcSeverity::Error),
			       QLatin1String(empty_title_sql)));
	REQUIRE(engine.addRule(rules,
			       sqlRule("element.label.empty"),
			       QLatin1String(unlabelled_element_sql)));
	REQUIRE(engine.addRule(rules,
			       sqlRule("rule.that.breaks"),
			       QLatin1String(broken_sql)));

	const DrcSqlReport report = engine.run(bench.project(), rules);
	REQUIRE(report.ruleCount() == 3);
	REQUIRE(report.queriesSent() == 3);

	SECTION("a que achou, achou")
	{
		const DrcSqlResult found =
				report.result(QStringLiteral("folio.title.empty"));
		CHECK(found.status() == DrcQueryStatus::Succeeded);
		CHECK(found.hasFindings());
		CHECK(found.findingCount() == 2);
		CHECK_FALSE(found.hasFailed());
	}

	SECTION("a que não achou nada foi perguntada, e não falhou")
	{
			//element_info is empty on this fixture, so the honest answer
			//is no row. This is the only "nothing found" the engine may
			//produce, and it says so with a status and not with silence.
		const DrcSqlResult empty =
				report.result(QStringLiteral("element.label.empty"));
		CHECK(empty.wasAsked());
		CHECK(empty.status() == DrcQueryStatus::Succeeded);
		CHECK_FALSE(empty.hasFailed());
		CHECK_FALSE(empty.hasFindings());
		CHECK(empty.findingCount() == 0);
	}

	SECTION("a que quebrou diz por que, e não conta como nada encontrado")
	{
		const DrcSqlResult broken =
				report.result(QStringLiteral("rule.that.breaks"));
		CHECK(broken.status() == DrcQueryStatus::Failed);
		CHECK(broken.hasFailed());

			//And it is not a finding either. A failure counted as a
			//finding would be the opposite mistake: a rule that breaks
			//on every project would report a defect on every project.
		CHECK_FALSE(broken.hasFindings());
		CHECK(broken.findingCount() == 0);

			//The reason is kept, not swallowed. The text comes from the
			//data base, so the assertion is on there being one and on the
			//name of the column it refused, not on the whole sentence.
		INFO("error: " << broken.error().toStdString());
		CHECK_FALSE(broken.error().isEmpty());
		CHECK(broken.error().contains(QStringLiteral("colonne_absente"),
					      Qt::CaseInsensitive));

			//The question it asked is kept beside the answer, so that a
			//rule that behaves strangely is read without going to look
			//for it somewhere else.
		CHECK(broken.statement() == QLatin1String(broken_sql));
	}

	SECTION("o relatório inteiro não pode ser lido como limpo")
	{
		CHECK(report.hasFailure());
		CHECK(report.failureCount() == 1);
		CHECK_FALSE(report.isClean());

			//Named, not counted: a count sends whoever reads it back to
			//find out which rule broke.
		const QStringList failures = report.failures();
		REQUIRE(failures.count() == 1);
		CHECK(failures.first().startsWith(QStringLiteral("rule.that.breaks:")));
	}

	SECTION("uma regra sem consulta registrada falha, e não custa uma consulta")
	{
			//A rule that declares it is answered by a query and has none.
			//It can only be built by adding it to the register without
			//going through the engine, which is what this does - and the
			//point is that it is loud instead of quietly skipped.
		DrcRuleSet orphan_rules;
		DrcSqlEngine orphan_engine;
		REQUIRE(orphan_rules.add(sqlRule("rule.without.query")));

		const DrcSqlReport orphan_report =
				orphan_engine.run(bench.project(), orphan_rules);

		REQUIRE(orphan_report.ruleCount() == 1);
		CHECK(orphan_report.hasFailure());
		CHECK(orphan_report.queriesSent() == 0);
		CHECK_FALSE(orphan_report.isClean());

		const DrcSqlResult result =
				orphan_report.result(QStringLiteral("rule.without.query"));
		CHECK(result.hasFailed());
		CHECK(result.statement().isEmpty());
		CHECK_FALSE(result.error().isEmpty());
	}

	SECTION("sem projeto, toda regra falha - e nenhuma diz que está tudo bem")
	{
		const DrcSqlReport no_project = engine.run(nullptr, rules);

		REQUIRE(no_project.ruleCount() == 3);
		CHECK(no_project.failureCount() == 3);
		CHECK(no_project.queriesSent() == 0);
		CHECK_FALSE(no_project.databaseUpdated());
		CHECK(no_project.findingCount() == 0);

			//The contrast that makes the count above mean something: a
			//run with nothing to ask comes back empty, a run that could
			//not ask comes back full of failures. An engine that returned
			//an empty report for both would make "no project" read as
			//"no rule to run".
		CHECK_FALSE(no_project.isClean());
	}
}
