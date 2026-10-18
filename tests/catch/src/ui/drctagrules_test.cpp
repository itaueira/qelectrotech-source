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

#include "../../../../sources/autoNum/iecstructure.h"
#include "../../../../sources/bordertitleblock.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/drc/drcduplicatetags.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/drc/drcscanengine.h"
#include "../../../../sources/drc/drctagrules.h"
#include "../../../../sources/drc/ui/drcpanel.h"
#include "../../../../sources/drc/ui/drcprojectactions.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/titleblockproperties.h"

#include <catch2/catch.hpp>

#include <QString>
#include <QStringList>

/*
	The rule of two components carrying the same tag, on a delivered
	example and on a bench of every kind the rule must leave alone.

	The example is ShellyParts.qet: three folios, twelve simple symbols,
	twelve different tags and no formula, so the rule has nothing to say
	about it as shipped - and the first thing the case does is prove that,
	because a duplicate planted into a project that already had some would
	prove nothing about the plant. Then K3 is written on one component of
	folio 1 and one of folio 3, every folio is given the plant CT1, and the
	two components are put in cabinets A1 and A2 through the path down the
	location tree - the key an assignment from the tree writes, and the
	only one the rule reads for it.

	Every section is one change away from that state, and the changes are
	the case of use: the structure off, then on, with the cabinet from the
	tree or typed into the free field. What is compared in the sentence is
	what the pure half says about the places read off the folio here, so
	that this file proves the walk collected the right components, and the
	reading named is the one the project settings asked for.

	The project is never saved: the example is part of the source tree.
*/

namespace
{
	/// The component of @a project whose tag is @a label, if only one carries it.
	Element *componentTagged(QETProject *project, const QString &label)
	{
		Element *found = nullptr;
		const QList<Diagram *> diagrams = project->diagrams();
		for (Diagram *diagram : diagrams)
		{
			const QList<Element *> elements = diagram->elements();
			for (Element *element : elements)
			{
				if (element->actualLabel() != label) {
					continue;
				}
				if (found) {
					return nullptr;
				}
				found = element;
			}
		}
		return found;
	}

	/// Writes @a value under @a key in the information of @a element.
	void setInformation(Element *element,
			    const QString &key,
			    const QString &value)
	{
		DiagramContext info = element->elementInformations();
		info.addValue(key, value);
		element->setElementInformations(info);
	}

	/// Where @a element is, read off its folio, carrying @a tag.
	DrcDuplicateTags::Mark placeOf(QETProject *project,
				       Element *element,
				       const QString &tag)
	{
		DrcDuplicateTags::Mark mark;
		mark.tag = tag;
		mark.folio = project->folioIndex(element->diagram()) + 1;
		mark.position = element->diagram()
				->convertPosition(element->scenePos()).toString();
		return mark;
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

	IecStructureSettings structureOn(IecTagDisplay display)
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = display;
		return settings;
	}
}

TEST_CASE("CU-24.4 — K3 em dois armários: desligada acusa; ligada, com o armário vindo da árvore, cala; com o armário digitado no campo livre, acusa — e a frase diz a leitura",
	  "[uibench][drc][t24]")
{
	UiBench::Project bench(QStringLiteral("ShellyParts.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 3);

	QETProject *project = bench.project();
	REQUIRE_FALSE(project->iecSettings().enabled);

	Element *first = componentTagged(project, QStringLiteral("SHSW1"));
	Element *middle = componentTagged(project, QStringLiteral("SHPLUS1"));
	Element *second = componentTagged(project, QStringLiteral("SHPRO1"));
	REQUIRE(first != nullptr);
	REQUIRE(middle != nullptr);
	REQUIRE(second != nullptr);
	REQUIRE(first->diagram() == bench.diagram(0));
	REQUIRE(middle->diagram() == bench.diagram(1));
	REQUIRE(second->diagram() == bench.diagram(2));

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcTagRules::registerRules(rules, scan) == 1);
	const QString rule = DrcTagRules::duplicateTagIdentifier();

		//As shipped: twelve tags, all different. Silence here is what
		//makes the finding below the plant's and nobody else's - and the
		//rule did run, or the silence would mean nothing.
	{
		const DrcScanReport shipped = scan.run(project, rules);
		INFO(described(shipped.findings()));
		REQUIRE(shipped.rulesRun() == 1);
		REQUIRE(shipped.findingCountOf(rule) == 0);
	}

		//The plant: the same tag on folio 1 and folio 3, the plant CT1 on
		//every folio, and the two cabinets from the location tree.
	setInformation(first, QStringLiteral("label"), QStringLiteral("K3"));
	setInformation(second, QStringLiteral("label"), QStringLiteral("K3"));
	REQUIRE(first->actualLabel() == QStringLiteral("K3"));
	REQUIRE(second->actualLabel() == QStringLiteral("K3"));

	const QList<Diagram *> sheets = bench.diagrams();
	for (Diagram *sheet : sheets)
	{
		TitleBlockProperties folio =
				sheet->border_and_titleblock.exportTitleBlock();
		folio.plant = QStringLiteral("CT1");
		sheet->border_and_titleblock.importTitleBlock(folio);
	}

	setInformation(first, IecStructure::locationPathKey(), QStringLiteral("A1"));
	setInformation(second, IecStructure::locationPathKey(), QStringLiteral("A2"));

	using Reading = DrcDuplicateTags::Reading;

	SECTION("desligada: um achado, de erro, no K3 da folha 1, com os dois lugares e a leitura como digitada")
	{
		const DrcScanReport report = scan.run(project, rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);

		const DrcFinding &finding = findings.first();
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.element() == first);
		CHECK(finding.folio() == 1);
		CHECK(finding.position()
		      == placeOf(project, first, QString()).position);

			//The cabinet does not separate them: the structure is off,
			//and the tag is compared as the components carry it.
		CHECK(finding.text()
		      == DrcDuplicateTags::duplicateText(
			      {placeOf(project, first, QStringLiteral("K3")),
			       placeOf(project, second, QStringLiteral("K3"))},
			      Reading::AsTyped));

		SECTION("e cala quando um dos dois volta à sua tag")
		{
			setInformation(second, QStringLiteral("label"),
				       QStringLiteral("SHPRO1"));
			const DrcScanReport after = scan.run(project, rules);
			INFO(described(after.findings()));
			CHECK(after.rulesRun() == 1);
			CHECK(after.findingCountOf(rule) == 0);
		}
	}

	SECTION("ligada, na forma curta: a folha desenha -K3 nos dois, e a regra cala, porque a designação é =CT1+A1-K3 e =CT1+A2-K3")
	{
		project->setIecSettings(structureOn(IecTagDisplay::Short));

			//The drawn form is the same twice. Comparing it would report
			//two components the norm tells apart - the decision this
			//section exists to hold.
		CHECK(first->displayedLabel() == QStringLiteral("-K3"));
		CHECK(second->displayedLabel() == QStringLiteral("-K3"));

		const DrcScanReport report = scan.run(project, rules);
		INFO(described(report.findings()));
		CHECK(report.rulesRun() == 1);
		CHECK(report.findingCountOf(rule) == 0);
	}

	SECTION("ligada, na forma completa: a folha desenha as duas designações, e a regra cala")
	{
		project->setIecSettings(structureOn(IecTagDisplay::Full));

		CHECK(first->displayedLabel() == QStringLiteral("=CT1+A1-K3"));
		CHECK(second->displayedLabel() == QStringLiteral("=CT1+A2-K3"));

		const DrcScanReport report = scan.run(project, rules);
		INFO(described(report.findings()));
		CHECK(report.rulesRun() == 1);
		CHECK(report.findingCountOf(rule) == 0);
	}

	SECTION("ligada, os dois no mesmo armário: acusa, nomeando a designação composta que ficou igual")
	{
		project->setIecSettings(structureOn(IecTagDisplay::Short));
		setInformation(second, IecStructure::locationPathKey(),
			       QStringLiteral("A1"));

		const DrcScanReport report = scan.run(project, rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);

		const DrcFinding &finding = findings.first();
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.element() == first);
		CHECK(finding.text()
		      == DrcDuplicateTags::duplicateText(
			      {placeOf(project, first, QStringLiteral("=CT1+A1-K3")),
			       placeOf(project, second, QStringLiteral("=CT1+A1-K3"))},
			      Reading::ComposedFromTree));
	}

	SECTION("ligada, com o armário digitado no campo livre e não na árvore: acusa com razão, e a frase diz que o campo não foi lido")
	{
		project->setIecSettings(structureOn(IecTagDisplay::Short));
		REQUIRE_FALSE(project->iecSettings().location_from_element);

		setInformation(first, IecStructure::locationPathKey(), QString());
		setInformation(second, IecStructure::locationPathKey(), QString());
		setInformation(first, IecStructure::locationKey(), QStringLiteral("A1"));
		setInformation(second, IecStructure::locationKey(), QStringLiteral("A2"));

		const DrcScanReport report = scan.run(project, rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);

		const DrcFinding &finding = findings.first();
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.element() == first);
		CHECK(finding.text()
		      == DrcDuplicateTags::duplicateText(
			      {placeOf(project, first, QStringLiteral("=CT1-K3")),
			       placeOf(project, second, QStringLiteral("=CT1-K3"))},
			      Reading::ComposedFromTree));

		SECTION("e, dito que o campo é o + da norma, cala")
		{
			IecStructureSettings settings =
					structureOn(IecTagDisplay::Short);
			settings.location_from_element = true;
			project->setIecSettings(settings);

			const DrcScanReport after = scan.run(project, rules);
			INFO(described(after.findings()));
			CHECK(after.rulesRun() == 1);
			CHECK(after.findingCountOf(rule) == 0);
		}
	}

	SECTION("três componentes com a mesma tag são um achado só, com os três lugares")
	{
		setInformation(middle, QStringLiteral("label"), QStringLiteral("K3"));

		const DrcScanReport report = scan.run(project, rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);
		CHECK(findings.first().element() == first);
		CHECK(findings.first().text()
		      == DrcDuplicateTags::duplicateText(
			      {placeOf(project, first, QStringLiteral("K3")),
			       placeOf(project, middle, QStringLiteral("K3")),
			       placeOf(project, second, QStringLiteral("K3"))},
			      Reading::AsTyped));
	}

	SECTION("pelo perfil de fábrica: a regra está lá, como erro, e o painel leva ao componente")
	{
		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(project);
		INFO(outcome.summary().toStdString());

		int row = -1;
		int count = 0;
		for (int i = 0 ; i < outcome.findings.count() ; ++i)
		{
			if (outcome.findings.at(i).ruleIdentifier() == rule)
			{
				row = i;
				++count;
			}
		}
		REQUIRE(count == 1);

		const DrcFinding &finding = outcome.findings.at(row);
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.element() == first);
		CHECK(finding.folio() == 1);

		DrcPanel panel;
		panel.setFindings(outcome.findings);
		first->diagram()->clearSelection();
		REQUIRE_FALSE(first->isSelected());

		CHECK(panel.activateFinding(row));
		CHECK(first->isSelected());
	}
}

namespace
{
	/**
		One folio holding one of each kind of symbol, every kind twice
		with one tag, and one pair that must be reported.

		- a coil KM1 and two contacts KM1: the contacts are the same
		  device as the coil, drawn again, and are left out by kind;
		- two terminals X1, two folio report arrows R1, two thumbnails T1
		  and two terminals drawn from their strip, X2: none of them is
		  compared;
		- two simple symbols with no tag: no tag is not a shared tag;
		- a simple symbol KA2 and a coil KA2: the pair that is reported,
		  so that the silence about everything else is not the silence of
		  a rule that reports nothing.

		Each kind the rule leaves alone carries a repeated tag on purpose:
		a fixture where they carried different tags would pass a rule that
		forgot the filter.
	*/
	QString scopeFixtureXml()
	{
		struct Definition
		{
			const char *name;
			const char *link_type;
			const char *kind_informations;
		};

		const Definition definitions_[] = {
			{"plain.elmt", "simple", ""},
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
			 "</kindInformations>"},
			{"striphead.elmt", "master",
			 "<kindInformations>"
			 "<kindInformation name=\"type\">strip_terminal</kindInformation>"
			 "</kindInformations>"},
			{"picture.elmt", "thumbnail", ""},
			{"going.elmt", "next_report", ""}};

		struct Item
		{
			const char *definition;
			int x;
			int y;
			const char *label;
		};

		const Item items[] = {
			{"coil.elmt",      100, 150, "KM1"},
			{"contact.elmt",   250, 150, "KM1"},
			{"contact.elmt",   400, 150, "KM1"},
			{"strip.elmt",     550, 150, "X1"},
			{"strip.elmt",     700, 150, "X1"},
			{"going.elmt",     850, 150, "R1"},
			{"going.elmt",     100, 400, "R1"},
			{"picture.elmt",   250, 400, "T1"},
			{"picture.elmt",   400, 400, "T1"},
			{"striphead.elmt", 550, 400, "X2"},
			{"striphead.elmt", 700, 400, "X2"},
			{"plain.elmt",     850, 400, ""},
			{"plain.elmt",     100, 550, ""},
			{"plain.elmt",     250, 550, "KA2"},
			{"coil.elmt",      400, 550, "KA2"}};

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
					//One at a time: %1 appears twice on purpose.
				       .arg(QLatin1String(definition.name))
				       .arg(QLatin1String(definition.link_type))
				       .arg(QLatin1String(definition.kind_informations));
		}

		QString instances;
		int index = 0;
		for (const Item &item : items)
		{
			const QString uuid = QStringLiteral(
						     "{7a9d0000-0000-4000-8000-0000000000%1}")
					     .arg(index, 2, 10, QLatin1Char('0'));

			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/%3\" uuid=\"%4\">"
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
				     .arg(uuid)
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
			       " height=\"640\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definitions, instances);
	}

	/// The first component of @a diagram of that kind and that tag.
	Element *componentOf(Diagram *diagram,
			     ElementData::Type kind,
			     const QString &label)
	{
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
}

TEST_CASE("T24 — a tag repetida compara o símbolo simples e o mestre, e deixa de fora o escravo, o borne, a seta de folha, a miniatura e o borne da régua",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(scopeFixtureXml(),
				      QStringLiteral("drctagscope.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 15);

	Element *coil = componentOf(sheet, ElementData::Master,
				    QStringLiteral("KM1"));
	Element *contact = componentOf(sheet, ElementData::Slave,
				       QStringLiteral("KM1"));
	Element *terminal = componentOf(sheet, ElementData::Terminal,
					QStringLiteral("X1"));
	Element *arrow = componentOf(sheet, ElementData::NextReport,
				     QStringLiteral("R1"));
	Element *thumbnail = componentOf(sheet, ElementData::Thumbnail,
					 QStringLiteral("T1"));
	Element *strip_head = componentOf(sheet, ElementData::Master,
					  QStringLiteral("X2"));
	Element *plain_ka2 = componentOf(sheet, ElementData::Simple,
					 QStringLiteral("KA2"));
	Element *coil_ka2 = componentOf(sheet, ElementData::Master,
					QStringLiteral("KA2"));
	REQUIRE(coil != nullptr);
	REQUIRE(contact != nullptr);
	REQUIRE(terminal != nullptr);
	REQUIRE(arrow != nullptr);
	REQUIRE(thumbnail != nullptr);
	REQUIRE(strip_head != nullptr);
	REQUIRE(plain_ka2 != nullptr);
	REQUIRE(coil_ka2 != nullptr);

		//The fixture built what it says: without this, the strip head
		//below could be an ordinary coil, and its exclusion would be
		//proved on nothing.
	REQUIRE(strip_head->elementData().isStripTerminalMaster());
	REQUIRE_FALSE(coil->elementData().isStripTerminalMaster());

	SECTION("quem entra e quem fica de fora, componente a componente")
	{
		CHECK(DrcTagRules::isCompared(plain_ka2));
		CHECK(DrcTagRules::isCompared(coil));
		CHECK(DrcTagRules::isCompared(coil_ka2));
		CHECK_FALSE(DrcTagRules::isCompared(contact));
		CHECK_FALSE(DrcTagRules::isCompared(terminal));
		CHECK_FALSE(DrcTagRules::isCompared(arrow));
		CHECK_FALSE(DrcTagRules::isCompared(thumbnail));
		CHECK_FALSE(DrcTagRules::isCompared(strip_head));
		CHECK_FALSE(DrcTagRules::isCompared(nullptr));
	}

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcTagRules::registerRules(rules, scan) == 1);
	const QString rule = DrcTagRules::duplicateTagIdentifier();

	SECTION("acusa só o par KA2, o símbolo simples com o mestre, e aponta o da esquerda")
	{
		const DrcScanReport report = scan.run(bench.project(), rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		INFO(described(findings));
		REQUIRE(findings.count() == 1);

		const DrcFinding &finding = findings.first();
		CHECK(finding.element() == plain_ka2);
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.text()
		      == DrcDuplicateTags::duplicateText(
			      {placeOf(bench.project(), plain_ka2,
				       QStringLiteral("KA2")),
			       placeOf(bench.project(), coil_ka2,
				       QStringLiteral("KA2"))},
			      DrcDuplicateTags::Reading::AsTyped));
	}

	SECTION("e cala quando o mestre muda de tag: nada do resto era achado")
	{
		setInformation(coil_ka2, QStringLiteral("label"),
			       QStringLiteral("KA3"));

		const DrcScanReport report = scan.run(bench.project(), rules);
		INFO(described(report.findings()));
		CHECK(report.rulesRun() == 1);
		CHECK(report.findingCountOf(rule) == 0);
	}
}
