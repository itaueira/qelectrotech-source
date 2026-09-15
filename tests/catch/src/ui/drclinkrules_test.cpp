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

#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/drc/drclinkrules.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/drc/drcscanengine.h"
#include "../../../../sources/drc/ui/drcpanel.h"
#include "../../../../sources/drc/ui/drcprojectactions.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QString>
#include <QStringList>
#include <QTableWidget>

/*
	The first three rules written against the rule contract, and the walk
	that runs them.

	A rule that has never failed anything is not a proved rule; it is a
	rule whose detection nobody measured. So every case below has two
	halves and the second one is the half that matters: a drawing the rule
	speaks up about, and a drawing it says nothing about **on purpose**.
	Without the second, a rule that fired on everything would pass.

	The two halves are the same fixture, before and after the defect is
	put right, and that is stronger than two fixtures: the components, the
	folio and the labels do not change between the two readings, so the
	only thing that can account for the findings disappearing is the link
	and the strip. Two separate projects would leave a dozen other
	differences able to explain it.

	The rules are of the walk route, and the reason is measured rather
	than preferred: the project data base has no row for a slave contact
	at all, and no row for a terminal until a conductor is drawn to it, so
	any of these three written as a query would come back with nothing and
	report a clean project.
*/

namespace
{
	/**
		One folio with six components: two coils, two contacts, two
		terminals.

		Two of each and not one, because a rule that named every component
		of a kind would pass a fixture that had one. Halfway through the
		case, one coil is linked to one contact and one terminal is put
		into a strip; what has to be left is exactly the other three, by
		name.

		The labels repeat between kinds - a coil called KM1 and the
		contact of that coil also called KM1 - because that is what a
		drawing looks like, and because each rule is read on its own
		findings.
	*/
	QString fixtureXml()
	{
		struct Definition
		{
			const char *name;
			const char *link_type;
			const char *kind_informations;
		};

			//The kind of a component is a property of the symbol and not of
			//the instance, so it is written here: the two coils are two
			//placings of one definition, exactly as they would be on a real
			//folio.
		const Definition definitions_[] = {
			{"coil.elmt", "master",
			 "<kindInformations>"
			 "<kindInformation name=\"type\">coil</kindInformation>"
			 "</kindInformations>"},
			{"contact.elmt", "slave",
			 "<kindInformations>"
			 "<kindInformation name=\"type\">power</kindInformation>"
			 "<kindInformation name=\"state\">NO</kindInformation>"
			 "<kindInformation name=\"number\">1</kindInformation>"
			 "</kindInformations>"},
			{"strip.elmt", "terminal",
			 "<kindInformations>"
			 "<kindInformation name=\"type\">generic</kindInformation>"
			 "<kindInformation name=\"function\">generic</kindInformation>"
			 "</kindInformations>"}};

		struct Item
		{
			const char *definition;
			int x;
			int y;
			const char *label;
		};

		const Item items[] = {
			{"coil.elmt",    200, 200, "KM1"},
			{"coil.elmt",    200, 320, "KM2"},
			{"contact.elmt", 420, 200, "KM1"},
			{"contact.elmt", 420, 320, "KA9"},
			{"strip.elmt",   640, 200, "X1"},
			{"strip.elmt",   640, 320, "X2"}};

		QString definitions;
		for (const Definition &definition : definitions_)
		{
			definitions += QStringLiteral(
					       "<element name=\"%1\">"
					       "<definition type=\"element\" version=\"0.80\""
					       " width=\"30\" height=\"20\""
					       " hotspot_x=\"15\" hotspot_y=\"10\""
					       " orientation=\"dnnn\" link_type=\"%2\">"
					       "<names><name lang=\"en\">%1</name></names>"
					       "%3"
					       "<description>"
					       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
					       " antialias=\"false\""
					       " style=\"line-style:normal;line-weight:normal;"
					       "filling:none;color:black\"/>"
					       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
					       "</description>"
					       "</definition>"
					       "</element>")
					//One at a time, and not the three-argument form: %1
					//appears twice on purpose.
				       .arg(QLatin1String(definition.name))
				       .arg(QLatin1String(definition.link_type))
				       .arg(QLatin1String(definition.kind_informations));
		}

		QString instances;
		int index = 0;
		for (const Item &item : items)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/%3\""
					     " uuid=\"{dec0de00-0000-4000-8000-00000000000%4}\">"
					     "<terminals>"
					     "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%6"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(item.x)
				     .arg(item.y)
				     .arg(QLatin1String(item.definition))
				     .arg(index)
				     .arg(index)
				     .arg(QLatin1String(item.label));
			++index;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">%1</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" folio=\"1\""
			       " height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definitions, instances);
	}

	/// The component of @a diagram whose tag is @a label, of that kind.
	Element *componentOf(Diagram *diagram,
			     ElementData::Type kind,
			     const QString &label)
	{
		if (!diagram) {
			return nullptr;
		}

		const QList<Element *> elements = diagram->elements();
		for (Element *element : elements)
		{
			if (element->elementData().m_type == kind
			    && element->actualLabel() == label) {
				return element;
			}
		}
		return nullptr;
	}

	/**
		The tags the rule @a identifier named, sorted.

		Sorted because the order the components come back in is the order
		the folio holds them, and a case that asserted that order would be
		measuring the scene rather than the rule.
	*/
	QStringList taggedBy(const QList<DrcFinding> &findings,
			     const QString &identifier)
	{
		QStringList tags;
		for (const DrcFinding &finding : findings)
		{
			if (finding.ruleIdentifier() != identifier) {
				continue;
			}
			Element *element = finding.element();
			tags << (element ? element->actualLabel() : QString());
		}
		tags.sort();
		return tags;
	}
}

TEST_CASE("T24 — as três regras de vínculo acusam o que está solto, e só isso",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drclink.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

		//A sonda: sem os seis componentes na folha, tudo abaixo passaria
		//sobre uma bancada que não mede nada.
	Element *coil_linked = componentOf(sheet, ElementData::Master,
					   QStringLiteral("KM1"));
	Element *coil_loose = componentOf(sheet, ElementData::Master,
					  QStringLiteral("KM2"));
	Element *contact_linked = componentOf(sheet, ElementData::Slave,
					      QStringLiteral("KM1"));
	Element *contact_loose = componentOf(sheet, ElementData::Slave,
					     QStringLiteral("KA9"));
	Element *terminal_in_strip = componentOf(sheet, ElementData::Terminal,
						 QStringLiteral("X1"));
	Element *terminal_loose = componentOf(sheet, ElementData::Terminal,
					      QStringLiteral("X2"));

	REQUIRE(coil_linked);
	REQUIRE(coil_loose);
	REQUIRE(contact_linked);
	REQUIRE(contact_loose);
	REQUIRE(terminal_in_strip);
	REQUIRE(terminal_loose);

	const QString master_rule = DrcLinkRules::masterWithoutSlaveIdentifier();
	const QString slave_rule = DrcLinkRules::slaveWithoutMasterIdentifier();
	const QString terminal_rule =
			DrcLinkRules::terminalOutsideStripIdentifier();

	SECTION("num desenho em que nada está ligado, as três falam")
	{
		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(bench.project());

		CHECK(outcome.scan_rules_run == 3);
		CHECK_FALSE(outcome.isClean());

		CHECK(taggedBy(outcome.findings, master_rule)
		      == QStringList({QStringLiteral("KM1"),
				      QStringLiteral("KM2")}));
		CHECK(taggedBy(outcome.findings, slave_rule)
		      == QStringList({QStringLiteral("KA9"),
				      QStringLiteral("KM1")}));
		CHECK(taggedBy(outcome.findings, terminal_rule)
		      == QStringList({QStringLiteral("X1"),
				      QStringLiteral("X2")}));

			//O endereço é escrito pelo motor, e uma vez só: o achado
			//que não souber dizer a folha manda o leitor a lugar nenhum.
		for (const DrcFinding &finding : outcome.findings)
		{
			INFO(finding.describe().toStdString());
			CHECK(finding.hasTarget());
			CHECK(finding.folio() == 1);
			CHECK_FALSE(finding.position().isEmpty());
		}
	}

	SECTION("e calam sobre a bobina com contato e a borne com régua")
	{
			//A metade que prova que a regra não acusa tudo. Nada muda na
			//folha além do vínculo e da régua.
		coil_linked->linkToElement(contact_linked);
		REQUIRE_FALSE(coil_linked->isFree());
		REQUIRE_FALSE(contact_linked->isFree());

		TerminalStrip *strip = bench->newTerminalStrip(
					QStringLiteral("="),
					QStringLiteral("+"),
					QStringLiteral("X1"));
		REQUIRE(strip);
		REQUIRE(strip->addTerminal(terminal_in_strip));
		REQUIRE(static_cast<TerminalElement *>(terminal_in_strip)
			->parentTerminalStrip() != nullptr);

		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(bench.project());

		CHECK(outcome.scan_rules_run == 3);
		CHECK(taggedBy(outcome.findings, master_rule)
		      == QStringList({QStringLiteral("KM2")}));
		CHECK(taggedBy(outcome.findings, slave_rule)
		      == QStringList({QStringLiteral("KA9")}));
		CHECK(taggedBy(outcome.findings, terminal_rule)
		      == QStringList({QStringLiteral("X2")}));
	}

	SECTION("e um desenho inteiro correto não produz achado nenhum")
	{
		coil_linked->linkToElement(contact_linked);
		coil_loose->linkToElement(contact_loose);

		TerminalStrip *strip = bench->newTerminalStrip(
					QStringLiteral("="),
					QStringLiteral("+"),
					QStringLiteral("X1"));
		REQUIRE(strip);
		REQUIRE(strip->addTerminal(terminal_in_strip));
		REQUIRE(strip->addTerminal(terminal_loose));

		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(bench.project());

		INFO(outcome.summary().toStdString());
		CHECK(outcome.findings.isEmpty());

			//E "limpo" exige que alguma regra tenha rodado: lista vazia
			//sozinha é o que uma rodada sem regra nenhuma também devolve.
		CHECK(outcome.scan_rules_run == 3);
		CHECK(outcome.isClean());
	}
}

TEST_CASE("T23 — regra desligada não anda folha nenhuma",
	  "[uibench][drc][t23]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcoff.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcLinkRules::registerRules(rules, scan) == 3);
	REQUIRE(rules.count() == 3);

	SECTION("com todas desligadas, nenhuma regra roda e o projeto não é limpo")
	{
		const QList<DrcRule> all = rules.rules();
		for (const DrcRule &rule : all) {
			REQUIRE(rules.setEnabled(rule.identifier(), false));
		}
		REQUIRE_FALSE(rules.hasEnabledRule(DrcRoute::Scan));

		const DrcScanReport report = scan.run(bench.project(), rules);

			//O que se mede não é a ausência de achado -- ela também
			//viria de um projeto correto. É que nenhuma regra rodou.
		CHECK(report.rulesRun() == 0);
		CHECK(report.findingCount() == 0);
		CHECK_FALSE(report.isClean());
	}

	SECTION("desligar uma tira os achados dela e deixa os das outras")
	{
		REQUIRE(rules.setEnabled(
				DrcLinkRules::terminalOutsideStripIdentifier(),
				false));

		const DrcScanReport report = scan.run(bench.project(), rules);

		CHECK(report.rulesRun() == 2);
		CHECK(report.findingCountOf(
			      DrcLinkRules::terminalOutsideStripIdentifier()) == 0);
		CHECK(report.findingCountOf(
			      DrcLinkRules::masterWithoutSlaveIdentifier()) == 2);
		CHECK(report.findingCountOf(
			      DrcLinkRules::slaveWithoutMasterIdentifier()) == 2);
	}

	SECTION("o identificador repetido é recusado, não substituído")
	{
			//A segunda tentativa não pode entrar: a chave é o que o
			//arquivo de projeto grava a gravidade sob, e duas regras com
			//a mesma chave fariam a escolha feita sobre uma valer para a
			//outra sem ninguém ver.
		CHECK(DrcLinkRules::registerRules(rules, scan) == 0);
		CHECK(rules.count() == 3);
	}
}

TEST_CASE("T23 — o painel leva da linha ao componente na folha",
	  "[uibench][drc][t23]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcpanel.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	const DrcProjectActions::CheckOutcome outcome =
			DrcProjectActions::runCheck(bench.project());
	REQUIRE(outcome.findings.count() == 6);

	DrcPanel panel;
	panel.setFindings(outcome.findings);
	panel.setSummary(outcome.summary());

	REQUIRE(panel.table() != nullptr);
	REQUIRE(panel.table()->rowCount() == 6);
	CHECK(panel.table()->columnCount() == 4);

	SECTION("ativar a linha seleciona o componente que ela nomeia")
	{
		sheet->clearSelection();

		const int row = 0;
		Element *target = outcome.findings.at(row).element();
		REQUIRE(target);
		REQUIRE_FALSE(target->isSelected());

		CHECK(panel.activateFinding(row));
		CHECK(target->isSelected());

			//E a lista continua de pé: é a linha que o relatório do
			//catálogo copia e esta **não** copia -- fechar a janela ao
			//navegar faria a lista sumir no primeiro achado.
		CHECK(panel.table()->rowCount() == 6);

		SECTION("e ativar a linha seguinte troca a seleção")
		{
			Element *other = nullptr;
			for (int i = 1 ; i < outcome.findings.count() ; ++i)
			{
				if (outcome.findings.at(i).element() != target) {
					other = outcome.findings.at(i).element();
					CHECK(panel.activateFinding(i));
					break;
				}
			}
			REQUIRE(other);
			CHECK(other->isSelected());
			CHECK_FALSE(target->isSelected());
		}
	}

	SECTION("a linha sem alvo não manda ninguém para a folha 1")
	{
		QList<DrcFinding> findings = outcome.findings;
		findings << DrcFinding::onProject(
				QStringLiteral("project_wide"),
				DrcSeverity::Warning,
				QStringLiteral("Le projet n'a pas de cartouche."));

		panel.setFindings(findings);
		REQUIRE(panel.table()->rowCount() == 7);

			//A célula de folha fica vazia, e ativar não vai a lugar
			//nenhum. Um 1 ali levaria o leitor a uma folha que o
			//achado nunca citou.
		CHECK(panel.table()->item(6, 1)->text().isEmpty());
		CHECK_FALSE(panel.activateFinding(6));
	}

	SECTION("linha fora da tabela não faz nada")
	{
		CHECK_FALSE(panel.activateFinding(-1));
		CHECK_FALSE(panel.activateFinding(99));
	}
}
