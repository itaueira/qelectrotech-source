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

#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramposition.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/drc/drcscanengine.h"
#include "../../../../sources/drc/drcterminalrules.h"
#include "../../../../sources/drc/ui/drcprojectactions.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QStringList>

/*
	The rule of a component terminal nothing is wired to (T24, step 8).

	Two folios. On the first, two placings of one relay symbol - K1 and
	K2 - with three terminals each: A1, A2 and one with no name, and a
	folio report. On the second, a contact KA1 with terminals 13 and 14,
	and a terminal of a strip, X1. The case wires K1:A1 to the report and
	KA1:13 to X1:1, and leaves K2 with no wire at all.

	Only a component that has a wire is judged: a component with no wire
	is a strip drawn as blocks, its destinations written, or one not wired
	yet - and not a wire forgotten. So K2 says nothing until a wire reaches
	it, and then its other terminals speak. Two of one symbol and not one,
	because the finding has to say which of two identical terminals it
	means; a strip terminal with a wire and a free terminal, because the
	rule leaves the strip out on purpose and a rule that named every free
	terminal would pass a fixture without it.
*/
namespace
{
	QString fixtureXml()
	{
		auto symbol = [](const QString &name, const QString &link_type,
				 const QString &terminals)
		{
			return QStringLiteral(
				       "<element name=\"%1\">"
				       "<definition type=\"element\" version=\"0.80\""
				       " width=\"60\" height=\"40\""
				       " hotspot_x=\"10\" hotspot_y=\"20\""
				       " orientation=\"dnnn\" link_type=\"%2\">"
				       "<names><name lang=\"en\">%1</name></names>"
				       "<description>"
				       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
				       " antialias=\"false\""
				       " style=\"line-style:normal;line-weight:normal;"
				       "filling:none;color:black\"/>"
				       "%3"
				       "</description>"
				       "</definition>"
				       "</element>")
			       .arg(name, link_type, terminals);
		};

		const QString definitions =
				symbol(QStringLiteral("relay.elmt"), QStringLiteral("simple"),
				       QStringLiteral(
					       "<terminal x=\"0\" y=\"-10\" orientation=\"n\" name=\"A1\"/>"
					       "<terminal x=\"0\" y=\"10\" orientation=\"s\" name=\"A2\"/>"
					       "<terminal x=\"40\" y=\"0\" orientation=\"e\"/>"))
				+ symbol(QStringLiteral("contact.elmt"), QStringLiteral("slave"),
					 QStringLiteral(
						 "<terminal x=\"0\" y=\"-10\" orientation=\"n\" name=\"13\"/>"
						 "<terminal x=\"0\" y=\"10\" orientation=\"s\" name=\"14\"/>"))
				+ symbol(QStringLiteral("strip.elmt"), QStringLiteral("terminal"),
					 QStringLiteral(
						 "<terminal x=\"0\" y=\"-10\" orientation=\"n\" name=\"1\"/>"
						 "<terminal x=\"0\" y=\"10\" orientation=\"s\" name=\"2\"/>"))
				+ symbol(QStringLiteral("going.elmt"), QStringLiteral("next_report"),
					 QStringLiteral(
						 "<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"));

		auto instance = [](int index, const QString &definition, int x, int y,
				   const QString &label)
		{
			return QStringLiteral(
				       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
				       " freezeLabel=\"false\" orientation=\"0\""
				       " type=\"embed://bench/%3\""
				       " uuid=\"{7e2a1b00-0000-4000-8000-00000000000%4}\">"
				       "<terminals/><inputs/>"
				       "<elementInformations>"
				       "<elementInformation show=\"1\" name=\"label\">%5"
				       "</elementInformation>"
				       "</elementInformations>"
				       "<dynamic_texts/><texts_groups/>"
				       "</element>")
			       .arg(QString::number(x), QString::number(y), definition,
				    QString::number(index), label);
		};

		const QString first = instance(0, QStringLiteral("relay.elmt"), 145, 200, QStringLiteral("K1"))
				+ instance(1, QStringLiteral("relay.elmt"), 445, 200, QStringLiteral("K2"))
				+ instance(3, QStringLiteral("going.elmt"), 845, 200, QString());
		const QString second = instance(4, QStringLiteral("contact.elmt"), 245, 200, QStringLiteral("KA1"))
				+ instance(2, QStringLiteral("strip.elmt"), 645, 200, QStringLiteral("X1"));

		const QString sheet = QStringLiteral(
				"<diagram title=\"Bench %1\" order=\"%1\" folio=\"%1\""
				" author=\"Bench\" height=\"600\""
				" cols=\"20\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
				" displaycols=\"true\" displayrows=\"true\">"
				"<elements>%2</elements>"
				"<inputs/><conductors/>"
				"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1</category>"
			       "</collection>%2%3</project>")
		       .arg(definitions,
			    sheet.arg(QStringLiteral("1"), first),
			    sheet.arg(QStringLiteral("2"), second));
	}

	Element *tagged(QETProject *project, const QString &label)
	{
		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets) {
			const QList<Element *> elements = sheet->elements();
			for (Element *element : elements) {
				if (element->actualLabel() == label) {
					return element;
				}
			}
		}
		return nullptr;
	}

	Terminal *terminalOf(Element *element, const QString &name)
	{
		if (!element) {
			return nullptr;
		}
		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *terminal : terminals) {
			if (terminal->name() == name) {
				return terminal;
			}
		}
		return nullptr;
	}

		/// "K1 A1", one per finding of the rule, sorted; "K1 ?" for the
		/// terminal with no name
	QStringList named(const QList<DrcFinding> &findings)
	{
		QStringList list;
		for (const DrcFinding &finding : findings)
		{
			if (finding.ruleIdentifier()
			    != DrcTerminalRules::unconnectedTerminalIdentifier()) {
				continue;
			}
			Element *element = finding.element();
			const QString label = element ? element->actualLabel() : QString();
			QString name = QStringLiteral("?");
			if (element) {
				const QList<Terminal *> terminals = element->terminals();
				for (Terminal *terminal : terminals) {
					if (!terminal->name().isEmpty()
					    && finding.text().contains(
						    QStringLiteral(" %1 ").arg(terminal->name()))) {
						name = terminal->name();
					}
				}
			}
			list << label + QLatin1Char(' ') + name;
		}
		list.sort();
		return list;
	}
}

TEST_CASE("T24 — a component terminal with nothing wired to it",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcterminal.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();

	Element *k1 = tagged(project, QStringLiteral("K1"));
	Element *k2 = tagged(project, QStringLiteral("K2"));
	Element *ka1 = tagged(project, QStringLiteral("KA1"));
	Element *x1 = tagged(project, QStringLiteral("X1"));
	REQUIRE(k1);
	REQUIRE(k2);
	REQUIRE(ka1);
	REQUIRE(x1);
	REQUIRE(k1->terminals().size() == 3);
	REQUIRE(x1->terminals().size() == 2);

	Element *report = nullptr;
	for (Element *element : k1->diagram()->elements()) {
		if (element->linkType() & Element::AllReport) {
			report = element;
		}
	}
	REQUIRE(report);
	REQUIRE(report->terminals().size() == 1);

	auto wire = [](Terminal *from, Terminal *to) {
		REQUIRE(from);
		REQUIRE(to);
		auto *conductor = new Conductor(from, to);
		from->diagram()->addItem(conductor);
		REQUIRE(from->conductorsCount() == 1);
	};
	wire(terminalOf(k1, QStringLiteral("A1")), report->terminals().first());
	wire(terminalOf(ka1, QStringLiteral("13")), terminalOf(x1, QStringLiteral("1")));

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcTerminalRules::registerRules(rules, scan) == 1);
	const DrcRule rule = rules.rule(DrcTerminalRules::unconnectedTerminalIdentifier());

	SECTION("born switched on, as a warning, and only once")
	{
		CHECK(rule.isEnabled());
		CHECK(rule.severity() == DrcSeverity::Warning);
		CHECK(rule.route() == DrcRoute::Scan);
		CHECK(DrcTerminalRules::registerRules(rules, scan) == 0);
		CHECK(rules.count() == 1);
	}

	SECTION("the free terminals of a wired component, and of nothing else")
	{
			//K2 has no wire, and the free terminal of X1 is a strip's
		const QList<DrcFinding> findings = scan.run(project, rules).findings();
		CHECK(named(findings) == QStringList({
			QStringLiteral("K1 ?"), QStringLiteral("K1 A2"),
			QStringLiteral("KA1 14")}));
		CHECK(DrcTerminalRules::isJudged(k1));
		CHECK(DrcTerminalRules::isJudged(ka1));
		CHECK_FALSE(DrcTerminalRules::isJudged(x1));
		CHECK_FALSE(DrcTerminalRules::isJudged(report));

		for (const DrcFinding &finding : findings)
		{
			INFO(finding.describe().toStdString());
			CHECK(finding.severity() == DrcSeverity::Warning);
			CHECK(finding.hasTarget());
			CHECK_FALSE(finding.text().contains(QLatin1Char('%')));
			CHECK(finding.folio() == (finding.element() == ka1 ? 2 : 1));
		}
	}

	SECTION("a wire clears its ends, and wakes the component it reaches")
	{
		wire(terminalOf(k1, QStringLiteral("A2")), terminalOf(k2, QStringLiteral("A1")));

		const QList<DrcFinding> findings = scan.run(project, rules).findings();
		CHECK(named(findings) == QStringList({
			QStringLiteral("K1 ?"),
			QStringLiteral("K2 ?"), QStringLiteral("K2 A2"),
			QStringLiteral("KA1 14")}));
	}

	SECTION("the finding points at the symbol it is on, and at the terminal's cell")
	{
		wire(terminalOf(k1, QStringLiteral("A2")), terminalOf(k2, QStringLiteral("A1")));

			//The terminal with no name sits 40 to the right of the
			//insertion point, in the next column of the border: a rule
			//that left the position to the engine would write the cell of
			//the component, and the case would not tell the two apart
			//unless they differ - so that is required first.
		Terminal *far = nullptr;
		const QList<Terminal *> terminals = k2->terminals();
		for (Terminal *terminal : terminals) {
			if (terminal->name().isEmpty()) {
				far = terminal;
			}
		}
		REQUIRE(far);
		Diagram *sheet = k2->diagram();
		const QString terminal_cell = sheet->convertPosition(far->dockConductor()).toString();
		const QString component_cell = sheet->convertPosition(k2->scenePos()).toString();
		REQUIRE(terminal_cell != component_cell);

		const QList<DrcFinding> findings = scan.run(project, rules).findings();
		int on_k2 = 0;
		bool far_found = false;
		for (const DrcFinding &finding : findings)
		{
			if (finding.element() != k2) {
				continue;
			}
			++on_k2;
			if (!finding.text().contains(QStringLiteral(" A1 "))
			    && !finding.text().contains(QStringLiteral(" A2 "))) {
				far_found = true;
				CHECK(finding.position() == terminal_cell);
			}
		}
		CHECK(on_k2 == 2);
		CHECK(far_found);
	}

	SECTION("the factory profile runs it")
	{
		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		CHECK(outcome.scan_rules_run == 10);
		CHECK(named(outcome.findings).size() == 3);
	}
}
