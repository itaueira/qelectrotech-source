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

#include "../../../../sources/bordertitleblock.h"
#include "../../../../sources/dataBase/drcsqlengine.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/drc/drcscanengine.h"
#include "../../../../sources/drc/drctitleblock.h"
#include "../../../../sources/drc/drctitleblockrules.h"
#include "../../../../sources/drc/ui/drcpanel.h"
#include "../../../../sources/drc/ui/drcprojectactions.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/titleblockproperties.h"

#include <catch2/catch.hpp>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

/*
	The rule of a folio whose title block leaves a required field blank,
	on a project of four folios and on a shipped example.

	Four folios, one of each answer the rule can give: filled in, title
	blank, author missing altogether, and both made only of spacing. A
	fixture with only blank folios would pass a rule that reported every
	folio; one with only one kind of blank would pass a rule that named
	the wrong field.

	The project also carries, at project level, an author and a field of
	the user's own. They are there for the question the reading of the
	rule rests on - is it the field of the folio, or the value the title
	block shows once the project variables are resolved? For the title and
	the author the two are the same, because the folio's own value is
	written into the context last and with no test for emptiness: the
	project's author does not fill a folio's blank one, and the rule must
	not pretend it does. For a field of the user's own they differ, and the
	value the sheet prints - the project's - is the one that counts.

	What the pure half says - which fields are blank, and the sentence -
	is proved in drctitleblock_test.cpp of C_unittests and only checked
	for agreement here. This file proves the walk: which folios, which
	target, which severity, and that the factory profile runs it.

	The project is never saved.
*/

namespace
{
	/// The four folios, one of each answer.
	QString fixtureXml()
	{
		const QString sheet = QStringLiteral(
				"<diagram order=\"%1\"%2"
				" height=\"600\""
				" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
				" displaycols=\"true\" displayrows=\"true\">"
				"%3"
				"<elements/><inputs/><conductors/>"
				"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<properties>"
			       "<property name=\"author\" show=\"1\">Office</property>"
			       "<property name=\"client\" show=\"1\">Customer A</property>"
			       "</properties>"
			       "<collection/>"
			       "%1%2%3%4"
			       "</project>")
		       .arg(
				//Filled in, and the only one with a revision index.
			       sheet.arg(QStringLiteral("1"),
					 QStringLiteral(" title=\"Power supply\""
							" author=\"J. Doe\""
							" indexrev=\"A\""),
					 QString()),
				//The title left empty. The folio also carries the
				//field of the user's own, added to it and never set:
				//the project's value shows through it on the sheet.
			       sheet.arg(QStringLiteral("2"),
					 QStringLiteral(" title=\"\" author=\"J. Doe\""),
					 QStringLiteral(
						 "<properties>"
						 "<property name=\"client\" show=\"1\">"
						 "</property>"
						 "</properties>")),
				//No author attribute at all.
			       sheet.arg(QStringLiteral("3"),
					 QStringLiteral(" title=\"Control\""),
					 QString()),
				//Both present, and both made only of spacing.
			       sheet.arg(QStringLiteral("4"),
					 QStringLiteral(" title=\"   \" author=\" \""),
					 QString()));
	}

	/// Every finding, one per line, for a message that names them.
	std::string described(const QList<DrcFinding> &findings)
	{
		QStringList lines;
		for (const DrcFinding &finding : findings) {
			lines << finding.describe();
		}
		return lines.join(QStringLiteral("\n")).toStdString();
	}

	/**
		Write a title and an author into the title block of @a sheet the
		way the folio properties dialog does: through importTitleBlock(),
		which asks the project for the folio data again and so refreshes
		the context the title block is drawn from.
	*/
	void fillTitleBlock(Diagram *sheet,
			    const QString &title,
			    const QString &author)
	{
		TitleBlockProperties properties =
				sheet->border_and_titleblock.exportTitleBlock();
		properties.title = title;
		properties.author = author;
		sheet->border_and_titleblock.importTitleBlock(properties);
	}

	/// The value @a key has in the context the title block of @a sheet is drawn from.
	QString drawn(Diagram *sheet, const char *key)
	{
		return sheet->border_and_titleblock.titleblockInformation()
				.value(QLatin1String(key)).toString();
	}
}

TEST_CASE("T24 — folha com título ou autor em branco: um achado por folha, na folha, de aviso, nomeando os campos; preenchidos, nada",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drctitleblock.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 4);

	QETProject *project = bench.project();
	Diagram *filled = bench.diagram(0);
	Diagram *untitled = bench.diagram(1);
	Diagram *unsigned_sheet = bench.diagram(2);
	Diagram *blank = bench.diagram(3);
	REQUIRE(filled != nullptr);
	REQUIRE(untitled != nullptr);
	REQUIRE(unsigned_sheet != nullptr);
	REQUIRE(blank != nullptr);

		//The probe: the fixture reads as it was written. Without it a
		//folio that failed to load its title block would be blank for a
		//reason of its own, and the findings below would be measured on
		//that.
	REQUIRE(drawn(filled, "title") == QStringLiteral("Power supply"));
	REQUIRE(drawn(filled, "author") == QStringLiteral("J. Doe"));
	REQUIRE(drawn(untitled, "title").isEmpty());
	REQUIRE(drawn(untitled, "author") == QStringLiteral("J. Doe"));
	REQUIRE(drawn(unsigned_sheet, "title") == QStringLiteral("Control"));
		//Spacing, and not nothing: the case of the fourth folio is the
		//blank that is not empty, and a parser that dropped the spaces
		//would leave it proving the second folio again.
	REQUIRE(drawn(blank, "title") == QStringLiteral("   "));
	REQUIRE(project->projectProperties().value(QStringLiteral("author"))
		.toString() == QStringLiteral("Office"));

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcTitleBlockRules::registerRules(rules, scan) == 1);
	const QString rule = DrcTitleBlockRules::emptyFieldIdentifier();

	SECTION("três achados, um por folha incompleta, na ordem das folhas, e nenhum na folha preenchida")
	{
		const DrcScanReport report = scan.run(project, rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		REQUIRE(report.rulesRun() == 1);
		REQUIRE(findings.count() == 3);

		struct Expected
		{
			Diagram *sheet;
			int folio;
			QStringList empty;
		};
		const Expected expected[] = {
			{untitled, 2, {QStringLiteral("title")}},
			{unsigned_sheet, 3, {QStringLiteral("author")}},
			{blank, 4, {QStringLiteral("title"),
				    QStringLiteral("author")}}};

		for (int i = 0 ; i < 3 ; ++i)
		{
			const DrcFinding &finding = findings.at(i);
			INFO(finding.describe().toStdString());

				//On the folio, and on nothing drawn on it: the title
				//block is part of the sheet, and there is no component
				//to select.
			CHECK(finding.targetKind() == DrcTargetKind::Diagram);
			CHECK(finding.hasTarget());
			CHECK(finding.diagram() == expected[i].sheet);
			CHECK(finding.element() == nullptr);
			CHECK(finding.conductor() == nullptr);
			CHECK(finding.folio() == expected[i].folio);
			CHECK(finding.folio()
			      == project->folioIndex(expected[i].sheet) + 1);
			CHECK(finding.position().isEmpty());

			CHECK(finding.severity() == DrcSeverity::Warning);
			CHECK(finding.text()
			      == DrcTitleBlock::emptyFieldsText(expected[i].empty));
		}

			//And the three sentences written out once, so that a pure
			//half that went quiet - an empty sentence for everything -
			//could not pass the comparison above by agreeing with itself.
		CHECK(findings.at(0).text()
		      == QStringLiteral("Empty title block field: Title"));
		CHECK(findings.at(1).text()
		      == QStringLiteral("Empty title block field: Author"));
		CHECK(findings.at(2).text()
		      == QStringLiteral("Empty title block fields: Title, Author"));
	}

	SECTION("o autor do projeto não preenche o da folha: o carimbo desenha o campo da folha, e é ele que a regra lê")
	{
			//The project says Office, and the context folio 3 is drawn
			//from says nothing - the historical fields are written last.
			//A rule that read the project's author would stay quiet about
			//a sheet that prints a blank cell.
		const DiagramContext context =
				unsigned_sheet->border_and_titleblock.titleblockInformation();
		CHECK(context.contains(QStringLiteral("author")));
		CHECK(context.value(QStringLiteral("author")).toString().isEmpty());

			//While a field only the project fills does reach the same
			//context: the reading is not "the folio and nothing else".
		CHECK(drawn(unsigned_sheet, "client") == QStringLiteral("Customer A"));

		const DrcScanReport report = scan.run(project, rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		bool found = false;
		for (const DrcFinding &finding : findings)
		{
			if (finding.diagram() == unsigned_sheet)
			{
				found = true;
				CHECK(finding.text()
				      == DrcTitleBlock::emptyFieldsText(
					      {QStringLiteral("author")}));
			}
		}
		CHECK(found);
	}

	SECTION("preenchidos os campos, a regra cala — e ela rodou")
	{
		fillTitleBlock(untitled, QStringLiteral("Motor control"),
			       QStringLiteral("J. Doe"));
		fillTitleBlock(unsigned_sheet, QStringLiteral("Control"),
			       QStringLiteral("J. Doe"));
		fillTitleBlock(blank, QStringLiteral("Terminals"),
			       QStringLiteral("J. Doe"));

			//The context was drawn again: without this, the silence
			//below could be a context that never saw the change.
		REQUIRE(drawn(untitled, "title") == QStringLiteral("Motor control"));
		REQUIRE(drawn(unsigned_sheet, "author") == QStringLiteral("J. Doe"));
		REQUIRE(drawn(blank, "title") == QStringLiteral("Terminals"));

		const DrcScanReport report = scan.run(project, rules);
		INFO(described(report.findings()));
		CHECK(report.rulesRun() == 1);
		CHECK(report.findingCountOf(rule) == 0);
		CHECK(report.isClean());

		SECTION("e volta a acusar quando um campo é apagado — aquele campo, naquela folha")
		{
			fillTitleBlock(unsigned_sheet, QStringLiteral("Control"),
				       QStringLiteral("  "));

			const DrcScanReport after = scan.run(project, rules);
			const QList<DrcFinding> findings = after.findingsOf(rule);
			INFO(described(findings));
			REQUIRE(findings.count() == 1);
			CHECK(findings.first().diagram() == unsigned_sheet);
			CHECK(findings.first().folio() == 3);
			CHECK(findings.first().text()
			      == QStringLiteral("Empty title block field: Author"));
		}
	}

	SECTION("a lista é configuração: pedir também a revisão e um campo do usuário acusa a revisão, e não o campo que o projeto preenche")
	{
		DrcRuleSet custom_rules;
		DrcScanEngine custom_scan;
		REQUIRE(DrcTitleBlockRules::registerRules(
				custom_rules, custom_scan,
				{QStringLiteral("title"), QStringLiteral("author"),
				 QStringLiteral("indexrev"), QStringLiteral("client")})
			== 1);

			//Folio 2 carries its own client field, blank; the project's
			//value shows through it, and that is what the sheet prints.
		REQUIRE(drawn(untitled, "client") == QStringLiteral("Customer A"));

		const DrcScanReport report = custom_scan.run(project, custom_rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 3);

		CHECK(findings.at(0).diagram() == untitled);
		CHECK(findings.at(0).text()
		      == DrcTitleBlock::emptyFieldsText(
			      {QStringLiteral("title"), QStringLiteral("indexrev")}));
		CHECK(findings.at(1).diagram() == unsigned_sheet);
		CHECK(findings.at(1).text()
		      == DrcTitleBlock::emptyFieldsText(
			      {QStringLiteral("author"), QStringLiteral("indexrev")}));
		CHECK(findings.at(2).diagram() == blank);
		CHECK(findings.at(2).text()
		      == DrcTitleBlock::emptyFieldsText(
			      {QStringLiteral("title"), QStringLiteral("author"),
			       QStringLiteral("indexrev")}));

			//Folio 1 has a revision index and the project's client: the
			//longer list finds nothing more on it than the factory one.
		for (const DrcFinding &finding : findings) {
			CHECK(finding.diagram() != filled);
		}
	}

	SECTION("uma lista sem campo nenhum não registra regra nenhuma")
	{
			//A rule asking for nothing would run, find nothing, and let a
			//check whose only rule it was call the project clean.
		DrcRuleSet none;
		DrcScanEngine none_scan;
		CHECK(DrcTitleBlockRules::registerRules(none, none_scan,
							QStringList()) == 0);
		CHECK(DrcTitleBlockRules::registerRules(none, none_scan,
							{QString()}) == 0);
		CHECK(none.count() == 0);
		CHECK(none_scan.checkCount() == 0);
	}
}

TEST_CASE("T24 — a folha cujo carimbo não é desenhado não é acusada (P139)",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drctitleblockhidden.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 4);
	QETProject *project = bench.project();
	Diagram *untitled = bench.diagram(1);
	REQUIRE(untitled != nullptr);
	REQUIRE(drawn(untitled, "title").isEmpty());

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcTitleBlockRules::registerRules(rules, scan) == 1);
	const QString rule = DrcTitleBlockRules::emptyFieldIdentifier();

		//Shown, the folio without a title is one of the three
	QList<DrcFinding> findings = scan.run(project, rules).findingsOf(rule);
	INFO(described(findings));
	REQUIRE(findings.count() == 3);
	CHECK(findings.first().diagram() == untitled);

		//Its title block not drawn, it prints no blank cell, and is left out
	untitled->border_and_titleblock.displayTitleBlock(false);
	findings = scan.run(project, rules).findingsOf(rule);
	INFO(described(findings));
	CHECK(findings.count() == 2);
	for (const DrcFinding &finding : std::as_const(findings)) {
		CHECK(finding.diagram() != untitled);
	}
}

TEST_CASE("T24 — pelo perfil de fábrica: a regra do carimbo está lá, ligada e de aviso, e o painel abre a folha",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drctitleblockprofile.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 4);

	const QString rule = DrcTitleBlockRules::emptyFieldIdentifier();

	SECTION("declarada como o perfil a escreve")
	{
		DrcRuleSet rules;
		DrcScanEngine scan;
		DrcSqlEngine sql;
		DrcProjectActions::buildFactoryCheck(rules, scan, sql);

		REQUIRE(rules.contains(rule));
		const DrcRule declared = rules.rule(rule);
		CHECK(declared.route() == DrcRoute::Scan);
		CHECK(declared.isEnabled());
		CHECK(declared.defaultEnabled());
		CHECK(declared.severity() == DrcSeverity::Warning);
		CHECK(declared.defaultSeverity() == DrcSeverity::Warning);
		CHECK(scan.hasCheck(rule));
	}

	SECTION("as três folhas incompletas aparecem, e ativar a linha abre a folha dela")
	{
		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(bench.project());
		INFO(outcome.summary().toStdString());

		QList<int> rows;
		for (int i = 0 ; i < outcome.findings.count() ; ++i)
		{
			if (outcome.findings.at(i).ruleIdentifier() == rule) {
				rows << i;
			}
		}
		REQUIRE(rows.count() == 3);

		const int row = rows.at(1);
		Diagram *sheet = bench.diagram(2);
		const DrcFinding &finding = outcome.findings.at(row);
		CHECK(finding.severity() == DrcSeverity::Warning);
		CHECK(finding.diagram() == sheet);
		CHECK(finding.folio() == 3);

		DrcPanel panel;
		panel.setFindings(outcome.findings);
		REQUIRE(panel.table() != nullptr);

			//The folio column says where, and the cell column says
			//nothing: a title block is not a cell of the grid.
		CHECK(panel.table()->item(row, 1)->text() == QStringLiteral("3"));
		CHECK(panel.table()->item(row, 2)->text().isEmpty());

			//Opening a folio is a signal the editor answers; the panel
			//has no view of its own. The receiver is a local object, so
			//the connection goes away with it and not with the folio.
		Diagram *shown = nullptr;
		QObject guard;
		QObject::connect(sheet, &Diagram::showDiagram, &guard,
				 [&shown](Diagram *diagram) {shown = diagram;});

		CHECK(panel.activateFinding(row));
		CHECK(shown == sheet);
	}
}

TEST_CASE("T24 — num exemplo do programa, a folha sem autor é a única acusada",
	  "[uibench][drc][t24]")
{
		//ArduinoLCD.qet as shipped: three folios, every one titled, two
		//signed and the third, GEIGER, with its author left empty. A
		//project nobody wrote for this rule, so that the count is a fact
		//about real data and not about a fixture built to match.
	UiBench::Project bench(QStringLiteral("ArduinoLCD.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 3);

	QETProject *project = bench.project();
	Diagram *geiger = nullptr;
	const QList<Diagram *> sheets = bench.diagrams();
	for (Diagram *sheet : sheets)
	{
		if (sheet->border_and_titleblock.title() == QLatin1String("GEIGER")) {
			geiger = sheet;
		}
	}
	REQUIRE(geiger != nullptr);

		//The probe: the other two are signed, or the silence about them
		//would mean nothing.
	for (Diagram *sheet : sheets)
	{
		if (sheet == geiger) {
			continue;
		}
		CHECK_FALSE(drawn(sheet, "author").trimmed().isEmpty());
		CHECK_FALSE(drawn(sheet, "title").trimmed().isEmpty());
	}

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcTitleBlockRules::registerRules(rules, scan) == 1);

	const DrcScanReport report = scan.run(project, rules);
	const QList<DrcFinding> findings =
			report.findingsOf(DrcTitleBlockRules::emptyFieldIdentifier());
	INFO(described(findings));
	REQUIRE(findings.count() == 1);
	CHECK(findings.first().diagram() == geiger);
	CHECK(findings.first().folio() == project->folioIndex(geiger) + 1);
	CHECK(findings.first().severity() == DrcSeverity::Warning);
	CHECK(findings.first().text()
	      == QStringLiteral("Empty title block field: Author"));
}
