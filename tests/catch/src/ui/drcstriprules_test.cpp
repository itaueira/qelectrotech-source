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

#include "../../../../sources/TerminalStrip/UndoCommand/addterminalstripcommand.h"
#include "../../../../sources/TerminalStrip/physicalterminal.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/cli_export.h"
#include "../../../../sources/dataBase/drcsqlengine.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/diagramposition.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/drc/drcscanengine.h"
#include "../../../../sources/drc/drcstripnumbers.h"
#include "../../../../sources/drc/drcstriprules.h"
#include "../../../../sources/drc/ui/drcpanel.h"
#include "../../../../sources/drc/ui/drcprojectactions.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"

#include <catch2/catch.hpp>

#include <QFile>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUuid>

#include <initializer_list>
#include <string>

/*
	The rules of the terminal strips: a number carried by two terminals of
	one strip (CU-24.12), a strip with no terminal (CU-24.13), and two
	strips with the same name.

	The bench draws eleven terminal symbols on two folios and mounts no
	strip: every case mounts the rails it needs, so which terminals share a
	strip is stated where it is checked. The numbers repeat across the
	symbols on purpose - three 3 on folio 1 and one on folio 2, two 5, a
	"X1:3" - and only the rails decide which of them are a repetition.

	Every case has the half the rule speaks about and the half it is silent
	about on purpose, on the same drawing: the 3 of another strip, the two
	levels of one terminal numbered alike, two terminals with no number. A
	rule that fired on every repeated text of the project would fail them.

	The rules are of the walk route, measured: the project data base has no
	table for terminal strips. What counts as the same number and the
	sentences are one floor down, in drcstripnumbers_test.cpp of
	C_unittests, and the sentences are compared here with what that file
	builds from the places read off the folios - so that this file proves
	the walk and the places, and the wording is proved once.
*/

namespace
{
	/// One terminal symbol of the bench.
	struct Symbol
	{
		int folio;
		int x;
		int y;
		const char *label;
	};

	/**
		The symbols, by index. The index is the end of the uuid, and the
		cases name the symbols by it: several carry the same label, and
		looking a terminal up by its label would be asking the very
		question the rule answers.
	*/
	const Symbol symbols[] = {
		{1, 100, 200, "1"},     //0
		{1, 180, 200, "2"},     //1
		{1, 260, 200, "3"},     //2  the first 3 of X1
		{1, 340, 200, "3"},     //3  the 3 of X4
		{1, 420, 200, "5"},     //4  the back level of the terminal of X5
		{1, 500, 200, "5"},     //5  its front level
		{1, 100, 400, "X1:3"},  //6
		{1, 180, 400, "3"},     //7
		{1, 260, 400, ""},      //8  no number
		{1, 340, 400, ""},      //9  no number
		{2, 260, 200, "3"}};    //10 the second 3 of X1, on folio 2

	QString uuidOf(int index)
	{
		return QStringLiteral("{dec0de24-0017-4000-8000-0000000000%1}")
				.arg(index, 2, 10, QLatin1Char('0'));
	}

	QString fixtureXml()
	{
		const QString definition = QStringLiteral(
			"<element name=\"terminal.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"30\" height=\"20\""
			" hotspot_x=\"15\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"terminal\">"
			"<names><name lang=\"en\">terminal.elmt</name></names>"
			"<kindInformations>"
			"<kindInformation name=\"type\">generic</kindInformation>"
			"<kindInformation name=\"function\">generic</kindInformation>"
			"</kindInformations>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;"
			"filling:none;color:black\"/>"
			"<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			"</description>"
			"</definition>"
			"</element>");

		QString folios[2];
		int index = 0;
		for (const Symbol &symbol : symbols)
		{
			folios[symbol.folio - 1] += QStringLiteral(
					"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					" freezeLabel=\"false\" orientation=\"0\""
					" type=\"embed://bench/terminal.elmt\""
					" uuid=\"%3\">"
					"<terminals>"
					"<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
					"</terminals>"
					"<inputs/>"
					"<elementInformations>"
					"<elementInformation show=\"1\" name=\"label\">%5"
					"</elementInformation>"
					"</elementInformations>"
					"<dynamic_texts/><texts_groups/>"
					"</element>")
				.arg(symbol.x)
				.arg(symbol.y)
				.arg(uuidOf(index))
				.arg(index)
				.arg(QLatin1String(symbol.label));
			++index;
		}

		const QString diagram = QStringLiteral(
			"<diagram title=\"Bench %1\" order=\"%1\" folio=\"%1\""
			" author=\"Bench\""
			" height=\"600\""
			" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements>"
			"<inputs/>"
			"<conductors/>"
			"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">%1</category>"
			       "</collection>")
			       .arg(definition)
		       + diagram.arg(QStringLiteral("1"), folios[0])
		       + diagram.arg(QStringLiteral("2"), folios[1])
		       + QStringLiteral("</project>");
	}

	/// The symbol of @a project whose uuid ends in @a index.
	Element *symbolOf(QETProject *project, int index)
	{
		const QUuid wanted(uuidOf(index));
		const QList<Diagram *> diagrams = project->diagrams();
		for (Diagram *diagram : diagrams)
		{
			const QList<Element *> elements = diagram->elements();
			for (Element *element : elements)
			{
				if (element->uuid() == wanted) {
					return element;
				}
			}
		}
		return nullptr;
	}

	QSharedPointer<RealTerminal> realOf(Element *element)
	{
		if (!element) {
			return QSharedPointer<RealTerminal>();
		}
		return static_cast<TerminalElement *>(element)->realTerminal();
	}

	/**
		A new strip = + @a name, with the symbols @a indexes mounted one
		after the other, in that order. Null when one of them is not found
		or not taken.
	*/
	TerminalStrip *mountStrip(QETProject *project,
				  const QString &name,
				  std::initializer_list<int> indexes)
	{
		TerminalStrip *strip = project->newTerminalStrip(
					QStringLiteral("="), QStringLiteral("+"), name);
		if (!strip) {
			return nullptr;
		}
		for (int index : indexes)
		{
			Element *symbol = symbolOf(project, index);
			if (!symbol || !strip->addTerminal(symbol)) {
				return nullptr;
			}
		}
		return strip;
	}

	/// The findings of @a findings made by the rule @a identifier.
	QList<DrcFinding> findingsOf(const QList<DrcFinding> &findings,
				     const QString &identifier)
	{
		QList<DrcFinding> found;
		for (const DrcFinding &finding : findings)
		{
			if (finding.ruleIdentifier() == identifier) {
				found << finding;
			}
		}
		return found;
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
		What @a symbol is in a sentence of the rule: the place @a physical
		on the rail, and the folio and the cell read off its sheet - by the
		project and the folio, and not by the code under test.
	*/
	DrcStripNumbers::Mark placeOf(QETProject *project,
				      Element *symbol,
				      const QString &number,
				      int physical)
	{
		DrcStripNumbers::Mark mark;
		mark.physical = physical;
		mark.number = number;
		mark.folio = project->folioIndex(symbol->diagram()) + 1;
		mark.cell = symbol->diagram()
			    ->convertPosition(symbol->scenePos()).toString();
		return mark;
	}

	void setLabel(Element *element, const QString &label)
	{
		DiagramContext info = element->elementInformations();
		info.addValue(QStringLiteral("label"), label);
		element->setElementInformations(info);
	}

	QString stripName(const char *name)
	{
		return DrcStripNumbers::stripName(QStringLiteral("="),
						  QStringLiteral("+"),
						  QString::fromUtf8(name));
	}

	/**
		Write the project @a project holds now - its strips included - to a
		file of its own, through the function a save writes it with.
	*/
	bool writeProject(QETProject *project, const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		const QByteArray bytes = project->toXml().toString(4).toUtf8();
		return file.write(bytes) == bytes.size();
	}

	/// What one run of --check-project left behind.
	struct CheckRun
	{
		int code = -1;
		QString text;
		/// One entry per line of the report, split at the tabs.
		QList<QStringList> lines;
	};

	/**
		Run --check-project on @a project_path, the report written to
		@a report_path, and read the report back. The reader is written
		here, as in drccli_test.cpp: a line is what comes before a line
		feed, and a field what comes between two tabs.
	*/
	CheckRun checkOf(const QString &project_path, const QString &report_path)
	{
		CheckRun run;
		run.code = CLIExport::run(QStringList()
					  << QStringLiteral("--check-project")
					  << project_path << report_path);

		QFile file(report_path);
		if (file.open(QIODevice::ReadOnly)) {
			run.text = QString::fromUtf8(file.readAll());
		}

		QStringList rows = run.text.split(QLatin1Char('\n'));
		if (!rows.isEmpty() && rows.last().isEmpty()) {
			rows.removeLast();
		}
		for (const QString &row : rows) {
			run.lines << row.split(QLatin1Char('\t'));
		}
		return run;
	}

	/// The lines of @a run whose rule field is @a rule.
	QList<QStringList> linesOfRule(const CheckRun &run, const QString &rule)
	{
		QList<QStringList> lines;
		for (const QStringList &line : run.lines)
		{
			if (line.value(3) == rule) {
				lines << line;
			}
		}
		return lines;
	}

	/// The lines of @a run whose first field is @a severity.
	QList<QStringList> linesOfSeverity(const CheckRun &run,
					   const QString &severity)
	{
		QList<QStringList> lines;
		for (const QStringList &line : run.lines)
		{
			if (line.value(0) == severity) {
				lines << line;
			}
		}
		return lines;
	}
}

TEST_CASE("CU-24.12 — número de borne repetido na régua: um erro para o 3 da X1, com as posições, e o duplo clique leva ao primeiro; o 3 da X4 e os dois níveis 5 calam; X1:3 e 3 na mesma régua acusam",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcstrip.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 2);

	QETProject *project = bench.project();
	const QString rule = DrcStripRules::duplicateTerminalNumberIdentifier();

		//The probe: the eleven symbols are there, and where the bench
		//says. Without it, everything below would pass over a bench that
		//measures nothing.
	Element *symbol[11] = {};
	for (int index = 0 ; index < 11 ; ++index)
	{
		symbol[index] = symbolOf(project, index);
		INFO("symbol " << index);
		REQUIRE(symbol[index] != nullptr);
		CHECK(symbol[index]->actualLabel()
		      == QString::fromUtf8(symbols[index].label));
	}
	REQUIRE(symbol[2]->diagram() == bench.diagram(0));
	REQUIRE(symbol[10]->diagram() == bench.diagram(1));

	SECTION("X1 com 1, 2, 3, 3 em duas folhas; X4 com 3; X5 com um borne de dois níveis 5 e 5; X6 com dois bornes sem número")
	{
		TerminalStrip *x1 = mountStrip(project, QStringLiteral("X1"),
					       {0, 1, 2, 10});
		TerminalStrip *x4 = mountStrip(project, QStringLiteral("X4"), {3});
		TerminalStrip *x5 = mountStrip(project, QStringLiteral("X5"),
					       {4, 5});
		TerminalStrip *x6 = mountStrip(project, QStringLiteral("X6"),
					       {8, 9});
		REQUIRE(x1);
		REQUIRE(x4);
		REQUIRE(x5);
		REQUIRE(x6);

			//The 5 of the front becomes the second level of the terminal
			//the 5 of the back is on, as grouping in the strip editor does.
		const QSharedPointer<RealTerminal> back = realOf(symbol[4]);
		const QSharedPointer<RealTerminal> front = realOf(symbol[5]);
		REQUIRE(x5->groupTerminals(back->physicalTerminal(), {front}));
		REQUIRE(x5->physicalTerminalCount() == 1);
		REQUIRE(x5->realTerminals().count() == 2);
		REQUIRE(x1->physicalTerminalCount() == 4);

		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		INFO(outcome.summary().toStdString());
		const QList<DrcFinding> findings = findingsOf(outcome.findings, rule);
		INFO(described(findings));

			//One, and not one per pair nor one per terminal: the 3 of X4,
			//the two levels 5 and the two empty numbers make none.
		REQUIRE(findings.count() == 1);

		const DrcFinding &finding = findings.first();
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.targetKind() == DrcTargetKind::Element);
		CHECK(finding.element() == symbol[2]);
		CHECK(finding.folio() == 1);
		CHECK(finding.position()
		      == placeOf(project, symbol[2], QString(), 2).cell);

			//The strip, the number, and both places: position 2 on folio
			//1 and position 3 on folio 2, where the panel's one folio
			//column would never send anybody.
		CHECK(finding.text()
		      == DrcStripNumbers::duplicateText(
			      stripName("X1"),
			      {placeOf(project, symbol[2], QStringLiteral("3"), 2),
			       placeOf(project, symbol[10], QStringLiteral("3"), 3)}));

		SECTION("o duplo clique leva ao primeiro 3, e não ao segundo")
		{
			int row = -1;
			for (int i = 0 ; i < outcome.findings.count() ; ++i)
			{
				if (outcome.findings.at(i).ruleIdentifier() == rule) {
					row = i;
				}
			}
			REQUIRE(row >= 0);

			bench.diagram(0)->clearSelection();
			bench.diagram(1)->clearSelection();
			REQUIRE_FALSE(symbol[2]->isSelected());

			DrcPanel panel;
			panel.setFindings(outcome.findings);
			CHECK(panel.activateFinding(row));
			CHECK(symbol[2]->isSelected());
			CHECK_FALSE(symbol[10]->isSelected());
		}

		SECTION("e cala quando o segundo 3 passa a ser 4")
		{
				//The same rails, the same symbols: only the number changes,
				//so only the number can account for the finding going away.
			setLabel(symbol[10], QStringLiteral("4"));
			REQUIRE(symbol[10]->actualLabel() == QStringLiteral("4"));

			const DrcProjectActions::CheckOutcome after =
					DrcProjectActions::runCheck(project);
			INFO(described(after.findings));
			CHECK(findingsOf(after.findings, rule).isEmpty());
		}
	}

	SECTION("X1:3 e 3 na mesma régua: o plano desenha os dois como 3, e a regra acusa")
	{
		TerminalStrip *x1 = mountStrip(project, QStringLiteral("X1"),
					       {0, 1, 6, 7});
		REQUIRE(x1);

		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		const QList<DrcFinding> findings = findingsOf(outcome.findings, rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);

		const DrcFinding &finding = findings.first();
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.element() == symbol[6]);
		CHECK(finding.text()
		      == DrcStripNumbers::duplicateText(
			      stripName("X1"),
			      {placeOf(project, symbol[6], QStringLiteral("3"), 2),
			       placeOf(project, symbol[7], QStringLiteral("3"), 3)}));
	}
}

TEST_CASE("CU-24.13 — régua vazia: um aviso que nomeia a X9 e nenhum para a X1; apagar a X9 tira o achado; o --check-project não muda o código de saída por causa dele",
	  "[uibench][drc][cli][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcemptystrip.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	QETProject *project = bench.project();
	const QString rule = DrcStripRules::emptyStripIdentifier();

	TerminalStrip *x1 = mountStrip(project, QStringLiteral("X1"), {0, 1});
	TerminalStrip *x9 = mountStrip(project, QStringLiteral("X9"), {});
	REQUIRE(x1);
	REQUIRE(x9);
	REQUIRE(x9->physicalTerminalCount() == 0);

	SECTION("um aviso, sobre o projeto, que nomeia a X9")
	{
		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		const QList<DrcFinding> findings = findingsOf(outcome.findings, rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);

		const DrcFinding &finding = findings.first();
		CHECK(finding.severity() == DrcSeverity::Warning);
			//A strip is on no folio: the finding says so instead of
			//sending the reader to folio 1.
		CHECK(finding.targetKind() == DrcTargetKind::None);
		CHECK_FALSE(finding.hasTarget());
		CHECK_FALSE(finding.hasFolio());
		CHECK(finding.text()
		      == DrcStripNumbers::emptyStripText(stripName("X9")));
	}

	SECTION("apagar a X9 tira o achado, e desfazer o traz de volta")
	{
		project->undoStack()->push(
				new RemoveTerminalStripCommand(x9, project));
		REQUIRE_FALSE(project->terminalStrip().contains(x9));

		const DrcProjectActions::CheckOutcome removed =
				DrcProjectActions::runCheck(project);
		INFO(described(removed.findings));
		CHECK(findingsOf(removed.findings, rule).isEmpty());

			//The same project with the strip back: only the strip can
			//account for the finding.
		project->undoStack()->undo();
		REQUIRE(project->terminalStrip().contains(x9));
		const DrcProjectActions::CheckOutcome restored =
				DrcProjectActions::runCheck(project);
		CHECK(findingsOf(restored.findings, rule).count() == 1);
	}

	SECTION("--check-project: o aviso sai na lista e o código continua 0; um número repetido, sim, faz 1")
	{
			//The command line of the program runs without backups
			//(main.cpp), and so does this case.
		QETProject::setBackupEnabled(false);

		QTemporaryDir dir;
		REQUIRE(dir.isValid());

		const QString with_empty = dir.filePath(QStringLiteral("with.qet"));
		REQUIRE(writeProject(project, with_empty));
		const CheckRun with_run = checkOf(
				with_empty, dir.filePath(QStringLiteral("with.txt")));
		INFO(with_run.text.toStdString());

			//The plant is checked before the code is trusted: without the
			//line, the 0 below would say nothing about a warning.
		const QList<QStringList> empty_lines = linesOfRule(with_run, rule);
		REQUIRE(empty_lines.count() == 1);
		const QStringList &line = empty_lines.first();
		CHECK(line.value(0) == QStringLiteral("warning"));
		CHECK(line.value(1).isEmpty());
		CHECK(line.value(4).contains(QStringLiteral("X9")));
		CHECK(linesOfSeverity(with_run, QStringLiteral("error")).isEmpty());
		CHECK(with_run.code == 0);

		project->undoStack()->push(
				new RemoveTerminalStripCommand(x9, project));
		const QString without_empty =
				dir.filePath(QStringLiteral("without.qet"));
		REQUIRE(writeProject(project, without_empty));
		const CheckRun without_run = checkOf(
				without_empty, dir.filePath(QStringLiteral("without.txt")));
		INFO(without_run.text.toStdString());
		CHECK(linesOfRule(without_run, rule).isEmpty());
		CHECK(without_run.code == with_run.code);

			//And the exit code does answer to the strips: the error rule
			//beside it fails the same command. Without this half, a 0 on
			//both sides would also be what a command that never ran the
			//strip rules answers.
		TerminalStrip *x2 = mountStrip(project, QStringLiteral("X2"), {2, 3});
		REQUIRE(x2);
		const QString repeated = dir.filePath(QStringLiteral("repeated.qet"));
		REQUIRE(writeProject(project, repeated));
		const CheckRun repeated_run = checkOf(
				repeated, dir.filePath(QStringLiteral("repeated.txt")));
		INFO(repeated_run.text.toStdString());
		const QList<QStringList> repeated_lines = linesOfRule(
				repeated_run,
				DrcStripRules::duplicateTerminalNumberIdentifier());
		REQUIRE(repeated_lines.count() == 1);
		CHECK(repeated_lines.first().value(0) == QStringLiteral("error"));
		CHECK(repeated_lines.first().value(1) == QStringLiteral("1"));
		CHECK(repeated_run.code == 1);
	}
}

TEST_CASE("T24 — régua cujos símbolos foram apagados da folha é régua vazia, e desfazer a enche de novo",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcdeleted.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	QETProject *project = bench.project();
	const QString empty_rule = DrcStripRules::emptyStripIdentifier();
	const QString number_rule =
			DrcStripRules::duplicateTerminalNumberIdentifier();

		//X8 holds the 3 of folio 1 and the 3 of folio 2: a repetition, as
		//long as both are drawn.
	TerminalStrip *x8 = mountStrip(project, QStringLiteral("X8"), {2, 10});
	REQUIRE(x8);
	Element *first = symbolOf(project, 2);
	Element *second = symbolOf(project, 10);
	REQUIRE(first);
	REQUIRE(second);

	{
		const DrcProjectActions::CheckOutcome drawn =
				DrcProjectActions::runCheck(project);
		INFO(described(drawn.findings));
		REQUIRE(findingsOf(drawn.findings, number_rule).count() == 1);
		REQUIRE(findingsOf(drawn.findings, empty_rule).isEmpty());
	}

	SECTION("apagado o segundo 3, a repetição some: ele não está em folha nenhuma")
	{
		Diagram *sheet = second->diagram();
		REQUIRE(sheet);
		sheet->clearSelection();
		second->setSelected(true);
		DiagramContent doomed(sheet);
		sheet->clearSelection();
		sheet->undoStack().push(new DeleteQGraphicsItemCommand(sheet, doomed));
		REQUIRE(second->diagram() == nullptr);

			//The precondition that makes the case about what is drawn:
			//the strip still holds the deleted terminal.
		CHECK(x8->physicalTerminalCount() == 2);

		const DrcProjectActions::CheckOutcome after =
				DrcProjectActions::runCheck(project);
		INFO(described(after.findings));
		CHECK(findingsOf(after.findings, number_rule).isEmpty());
		CHECK(findingsOf(after.findings, empty_rule).isEmpty());
	}

	SECTION("apagados os dois, a régua é vazia; desfazer a enche de novo")
	{
		for (Element *symbol : {first, second})
		{
			Diagram *sheet = symbol->diagram();
			REQUIRE(sheet);
			sheet->clearSelection();
			symbol->setSelected(true);
			DiagramContent doomed(sheet);
			sheet->clearSelection();
			sheet->undoStack().push(
					new DeleteQGraphicsItemCommand(sheet, doomed));
			REQUIRE(symbol->diagram() == nullptr);
		}

		const DrcProjectActions::CheckOutcome deleted =
				DrcProjectActions::runCheck(project);
		INFO(described(deleted.findings));
		const QList<DrcFinding> empty = findingsOf(deleted.findings,
							   empty_rule);
		REQUIRE(empty.count() == 1);
		CHECK(empty.first().text()
		      == DrcStripNumbers::emptyStripText(stripName("X8")));
		CHECK(findingsOf(deleted.findings, number_rule).isEmpty());

			//One stack for the project: the two deletions come back in the
			//reverse order they were made.
		project->undoStack()->undo();
		project->undoStack()->undo();
		REQUIRE(first->diagram() == bench.diagram(0));
		REQUIRE(second->diagram() == bench.diagram(1));

		const DrcProjectActions::CheckOutcome restored =
				DrcProjectActions::runCheck(project);
		INFO(described(restored.findings));
		CHECK(findingsOf(restored.findings, empty_rule).isEmpty());
		CHECK(findingsOf(restored.findings, number_rule).count() == 1);
	}
}

TEST_CASE("T24 — duas réguas com o mesmo nome completo: um aviso; o mesmo nome noutra localização cala",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcstripname.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	QETProject *project = bench.project();
	const QString rule = DrcStripRules::duplicateStripNameIdentifier();

	REQUIRE(mountStrip(project, QStringLiteral("X1"), {0}));
	REQUIRE(mountStrip(project, QStringLiteral("X2"), {1}));

	SECTION("e cala: nomes diferentes")
	{
		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		INFO(described(outcome.findings));
		CHECK(findingsOf(outcome.findings, rule).isEmpty());
	}

	SECTION("x1, escrita em minúscula, é a mesma X1: um aviso, sobre o projeto")
	{
		REQUIRE(mountStrip(project, QStringLiteral("x1"), {3}));

		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		const QList<DrcFinding> findings = findingsOf(outcome.findings, rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);
		CHECK(findings.first().severity() == DrcSeverity::Warning);
		CHECK_FALSE(findings.first().hasTarget());
		CHECK(findings.first().text()
		      == DrcStripNumbers::sameNameText(stripName("X1"), 2));
	}

	SECTION("e cala: X1 noutra localização é outra régua")
	{
		TerminalStrip *other = project->newTerminalStrip(
					QStringLiteral("="), QStringLiteral("+A2"),
					QStringLiteral("X1"));
		REQUIRE(other);
		REQUIRE(other->addTerminal(symbolOf(project, 3)));

		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		INFO(described(outcome.findings));
		CHECK(findingsOf(outcome.findings, rule).isEmpty());
	}
}

TEST_CASE("T24 — as regras da régua estão no perfil de fábrica, ligadas: o número repetido como erro, as outras duas como aviso",
	  "[uibench][drc][t24]")
{
	DrcRuleSet rules;
	DrcScanEngine scan;
	DrcSqlEngine sql;
	DrcProjectActions::buildFactoryCheck(rules, scan, sql);

	const QList<QPair<QString, DrcSeverity>> declared {
		{DrcStripRules::duplicateTerminalNumberIdentifier(),
		 DrcSeverity::Error},
		{DrcStripRules::emptyStripIdentifier(), DrcSeverity::Warning},
		{DrcStripRules::duplicateStripNameIdentifier(),
		 DrcSeverity::Warning}};

	for (const auto &entry : declared)
	{
		INFO(entry.first.toStdString());
		REQUIRE(rules.contains(entry.first));
		const DrcRule rule = rules.rule(entry.first);
		CHECK(rule.route() == DrcRoute::Scan);
		CHECK(rule.isEnabled());
		CHECK(rule.defaultEnabled());
		CHECK(rule.severity() == entry.second);
		CHECK(rule.defaultSeverity() == entry.second);
		CHECK(scan.hasCheck(entry.first));
	}

	SECTION("o identificador repetido é recusado, não substituído")
	{
		const int before = rules.count();
		CHECK(DrcStripRules::registerRules(rules, scan) == 0);
		CHECK(rules.count() == before);
	}
}
