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

#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/drc/drcpotentialrules.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/drc/drcscanengine.h"
#include "../../../../sources/drc/drcwirenumbers.h"
#include "../../../../sources/drc/ui/drcpanel.h"
#include "../../../../sources/drc/ui/drcprojectactions.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

#include <utility>

/*
	The rule of a potential carrying two wire numbers, on a project of two
	folios joined by a folio report.

	Two folios and a report arrow, and not one folio, because the potential
	is the whole point of the rule: the 24 is drawn on folio 1 and the 105
	on folio 2, and nothing on either folio alone is wrong. A rule that
	looked one folio at a time would pass a fixture drawn on one.

	Every case is one fixture read before and after one change - the 105
	renumbered, or the report arrow unlinked - so that the only thing that
	can account for the finding going away is that change. And the fixture
	carries, from the start, the two potentials the rule must leave alone
	on purpose:

	- a potential numbered on one conductor and left blank, with spacing or
	  with the placeholder of a new conductor on the others: blank is
	  absence, not a second number;
	- the number 24 again, on another potential and another folio: the same
	  number on two potentials is common practice for field wiring, and the
	  inverse rule is not written.

	What counts as a number, and the exact sentence, are proved one floor
	down in drcwirenumbers_test.cpp; here the sentence is compared with what
	that function says about the marks the drawing holds, so that this file
	proves the walk collected the right ones.
*/

namespace
{
	/**
		The text of a .qet built element by element and wire by wire.

		The terminals are written at the docking point, four units inside
		the point the symbol declares, which is what the loading path
		matches an instance terminal to a symbol terminal by. Written at the
		declared point, the terminals do not match and the project opens
		with every element in place and no conductor at all - so the case
		probes the conductor count before measuring anything.
	*/
	class Drawing
	{
		public:
			/**
				Place an element of the definition @a definition.

				@return the identifiers of its top and bottom terminals. A
				report arrow has only the top one, returned twice.
			*/
			QPair<int, int> place(int folio,
					      const QString &definition,
					      int x,
					      int y)
			{
				const bool is_report =
						definition != QLatin1String("part.elmt");
				const int top = m_terminal++;
				QString terminals = QStringLiteral(
							    "<terminal x=\"0\" y=\"-6\""
							    " orientation=\"0\" id=\"%1\"/>")
						    .arg(top);
				int bottom = top;
				if (!is_report)
				{
					bottom = m_terminal++;
					terminals += QStringLiteral(
							     "<terminal x=\"0\" y=\"6\""
							     " orientation=\"2\" id=\"%1\"/>")
						     .arg(bottom);
				}

				++m_uuid;
				m_elements[folio - 1] += QStringLiteral(
						"<element x=\"%1\" y=\"%2\" z=\"10\""
						" prefix=\"\" freezeLabel=\"false\""
						" orientation=\"0\" type=\"embed://bench/%3\""
						" uuid=\"{0ddba110-0000-4000-8000-%4}\">"
						"<terminals>%5</terminals><inputs/>"
						"<elementInformations/>"
						"<dynamic_texts/><texts_groups/>"
						"</element>")
					.arg(QString::number(x),
					     QString::number(y),
					     definition,
					     QStringLiteral("%1").arg(m_uuid, 12, 10,
								      QLatin1Char('0')),
					     terminals);

				return QPair<int, int>(top, bottom);
			}

			/// A conductor between two terminals of @a folio, carrying @a num.
			void wire(int folio, int from, int to, const QString &num)
			{
				m_conductors[folio - 1] += QStringLiteral(
						"<conductor x=\"0\" y=\"0\" terminal1=\"%1\""
						" terminal2=\"%2\" num=\"%3\" type=\"multi\""
						" freezeLabel=\"false\" displaytext=\"1\""
						" numsize=\"9\" condsize=\"1\"/>")
					.arg(QString::number(from),
					     QString::number(to),
					     num.toHtmlEscaped());
			}

			QString xml() const
			{
				auto symbol = [](const QString &name,
						 const QString &link_type,
						 bool two_terminals)
				{
					QString terminals = QStringLiteral(
								    "<terminal x=\"0\" y=\"-10\""
								    " orientation=\"n\"/>");
					if (two_terminals) {
						terminals += QStringLiteral(
								     "<terminal x=\"0\" y=\"10\""
								     " orientation=\"s\"/>");
					}

					return QStringLiteral(
						       "<element name=\"%1\">"
						       "<definition type=\"element\" version=\"0.80\""
						       " width=\"20\" height=\"40\""
						       " hotspot_x=\"10\" hotspot_y=\"20\""
						       " orientation=\"dnnn\" link_type=\"%2\">"
						       "<names><name lang=\"en\">%1</name></names>"
						       "<description>"
						       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
						       " end1=\"none\" end2=\"none\" length1=\"1.5\""
						       " length2=\"1.5\" antialias=\"false\""
						       " style=\"line-style:normal;line-weight:normal;"
						       "filling:none;color:black\"/>"
						       "%3"
						       "</description>"
						       "</definition>"
						       "</element>")
					       .arg(name, link_type, terminals);
				};

					//The report arrows carry one terminal, like the folio
					//reference symbols of the shipped collection do.
				const QString definitions =
						symbol(QStringLiteral("part.elmt"),
						       QStringLiteral("simple"), true)
						+ symbol(QStringLiteral("going.elmt"),
							 QStringLiteral("next_report"), false)
						+ symbol(QStringLiteral("coming.elmt"),
							 QStringLiteral("previous_report"), false);

				const QString sheet = QStringLiteral(
						"<diagram title=\"Bench %1\" order=\"%1\""
						" height=\"600\""
						" cols=\"40\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
						" displaycols=\"true\" displayrows=\"true\">"
						"<elements>%2</elements>"
						"<inputs/><conductors>%3</conductors>"
						"</diagram>");

				return QStringLiteral(
					       "<project title=\"bench\" version=\"0.80\">"
					       "<collection><category name=\"bench\">%1</category>"
					       "</collection>"
					       "%2%3"
					       "</project>")
				       .arg(definitions,
					    sheet.arg(QStringLiteral("1"),
						      m_elements[0], m_conductors[0]),
					    sheet.arg(QStringLiteral("2"),
						      m_elements[1], m_conductors[1]));
			}

		private:
			QString m_elements[2];
			QString m_conductors[2];
			int m_terminal = 0;
			int m_uuid = 0;
	};

	/**
		Two folios, five potentials, nine conductors.

		Folio 1 holds seven conductors: the 24 going off to folio 2 through
		a report arrow; a potential of two conductors numbered "7" and "7 ",
		which print the same; and a potential numbered 40 on one conductor
		and left empty, as the placeholder and as spaces on three others.

		Folio 2 holds two: the 105 arriving from folio 1, and another
		potential numbered 24.

		The report arrows are linked by the case and not in the file, the
		way the cases of the link rules link a coil and its contact: the
		other half of the case unlinks them again.
	*/
	QString fixtureXml()
	{
		Drawing drawing;
		const QString part = QStringLiteral("part.elmt");

			//Potential A, first half: the 24, leaving folio 1.
		const QPair<int, int> p1 = drawing.place(1, part, 100, 100);
		const QPair<int, int> going =
				drawing.place(1, QStringLiteral("going.elmt"), 100, 300);
		drawing.wire(1, p1.second, going.first, QStringLiteral("24"));

			//Potential C: one number, written twice, once with a space.
		const QPair<int, int> p3 = drawing.place(1, part, 200, 100);
		const QPair<int, int> p4 = drawing.place(1, part, 200, 300);
		const QPair<int, int> p5 = drawing.place(1, part, 260, 100);
		drawing.wire(1, p3.second, p4.first, QStringLiteral("7"));
		drawing.wire(1, p5.second, p4.first, QStringLiteral("7 "));

			//Potential E: numbered on one conductor out of four.
		const QPair<int, int> p6 = drawing.place(1, part, 400, 100);
		const QPair<int, int> p7 = drawing.place(1, part, 400, 300);
		const QPair<int, int> p8 = drawing.place(1, part, 460, 100);
		const QPair<int, int> p9 = drawing.place(1, part, 520, 100);
		const QPair<int, int> p10 = drawing.place(1, part, 580, 100);
		drawing.wire(1, p6.second, p7.first, QStringLiteral("40"));
		drawing.wire(1, p8.second, p7.first, QString());
		drawing.wire(1, p9.second, p7.first, QStringLiteral("_"));
		drawing.wire(1, p10.second, p7.first, QStringLiteral("   "));

			//Potential A, second half: the 105, arriving on folio 2.
		const QPair<int, int> coming =
				drawing.place(2, QStringLiteral("coming.elmt"), 100, 100);
		const QPair<int, int> p2 = drawing.place(2, part, 100, 300);
		drawing.wire(2, coming.first, p2.first, QStringLiteral("105"));

			//Potential F: the 24 again, on another potential and folio.
		const QPair<int, int> p11 = drawing.place(2, part, 300, 100);
		const QPair<int, int> p12 = drawing.place(2, part, 300, 300);
		drawing.wire(2, p11.second, p12.first, QStringLiteral("24"));

		return drawing.xml();
	}

	/// The first report arrow of @a diagram.
	Element *reportOn(Diagram *diagram)
	{
		const QList<Element *> elements = diagram->elements();
		for (Element *element : elements)
		{
			if (element->linkType() & Element::AllReport) {
				return element;
			}
		}
		return nullptr;
	}

	/// The conductor of @a diagram carrying exactly @a text, if only one does.
	Conductor *numbered(Diagram *diagram, const QString &text)
	{
		Conductor *found = nullptr;
		const QList<Conductor *> conductors = diagram->conductors();
		for (Conductor *conductor : conductors)
		{
			if (conductor->properties().text != text) {
				continue;
			}
			if (found) {
				return nullptr;
			}
			found = conductor;
		}
		return found;
	}

	/// The whole potential of @a conductor, itself included.
	QSet<Conductor *> potentialOf(Conductor *conductor)
	{
		QSet<Conductor *> potential =
				conductor->relatedPotentialConductors();
		potential.insert(conductor);
		return potential;
	}

	DrcWireNumbers::Mark mark(const char *text, int folio)
	{
		DrcWireNumbers::Mark built;
		built.text = QString::fromUtf8(text);
		built.folio = folio;
		return built;
	}
}

TEST_CASE("CU-24.7 — o potencial com 24 numa folha e 105 na outra é acusado uma vez, e o 24 repetido em outro potencial não",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcpotential.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 2);

	Diagram *first = bench.diagram(0);
	Diagram *second = bench.diagram(1);
	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);

		//The probe: a terminal that failed to match its symbol drops its
		//conductors without a word, and everything below would then be
		//measured on a drawing with no wire in it.
	REQUIRE(first->conductors().count() == 7);
	REQUIRE(second->conductors().count() == 2);

	Element *going = reportOn(first);
	Element *coming = reportOn(second);
	REQUIRE(going != nullptr);
	REQUIRE(coming != nullptr);
	going->linkToElement(coming);
	REQUIRE_FALSE(going->isFree());
	REQUIRE_FALSE(coming->isFree());

	Conductor *wire_24 = numbered(first, QStringLiteral("24"));
	Conductor *wire_105 = numbered(second, QStringLiteral("105"));
	Conductor *field_24 = numbered(second, QStringLiteral("24"));
	Conductor *wire_7 = numbered(first, QStringLiteral("7"));
	Conductor *wire_40 = numbered(first, QStringLiteral("40"));
	REQUIRE(wire_24 != nullptr);
	REQUIRE(wire_105 != nullptr);
	REQUIRE(field_24 != nullptr);
	REQUIRE(wire_7 != nullptr);
	REQUIRE(wire_40 != nullptr);

		//And the potentials are the ones the fixture says: the report arrow
		//is what joins the 24 and the 105, and the other 24 is apart.
	REQUIRE(potentialOf(wire_24).contains(wire_105));
	REQUIRE_FALSE(potentialOf(wire_24).contains(field_24));
	REQUIRE(potentialOf(wire_7).count() == 2);
	REQUIRE(potentialOf(wire_40).count() == 4);

	DrcRuleSet rules;
	DrcScanEngine scan;
	REQUIRE(DrcPotentialRules::registerRules(rules, scan) == 1);
	const QString rule = DrcPotentialRules::twoWireNumbersIdentifier();

	SECTION("um achado, de erro, no 24 da folha 1, nomeando os dois números e a folha de cada um")
	{
		const DrcScanReport report = scan.run(bench.project(), rules);
		CHECK(report.rulesRun() == 1);

			//One finding for the potential, and not one per conductor; and
			//none for the 24 of the other potential, nor for the potential
			//numbered by halves, nor for the "7" written twice.
		REQUIRE(report.findingCountOf(rule) == 1);

		const DrcFinding finding = report.findingsOf(rule).first();
		INFO(finding.describe().toStdString());
		CHECK(finding.severity() == DrcSeverity::Error);
		CHECK(finding.targetKind() == DrcTargetKind::Conductor);
		CHECK(finding.hasTarget());

			//The first numbered conductor of the potential, on its lowest
			//folio - and the address the engine wrote for it.
		CHECK(finding.conductor() == wire_24);
		CHECK(finding.diagram() == first);
		CHECK(finding.folio() == 1);

			//The sentence is the one the pure half writes for exactly these
			//two marks: 24 on folio 1 and 105 on folio 2. So the walk took
			//both numbers and the folio of each, and took nothing else.
		CHECK(finding.text()
		      == DrcWireNumbers::severalNumbersText({mark("24", 1),
							     mark("105", 2)}));
		CHECK(finding.text().contains(QStringLiteral("24")));
		CHECK(finding.text().contains(QStringLiteral("105")));
	}

	SECTION("e cala quando o 105 vira 24: o mesmo 24 em dois potenciais, em folhas diferentes, não é erro")
	{
		ConductorProperties properties = wire_105->properties();
		properties.text = QStringLiteral("24");
		wire_105->setProperties(properties);
		REQUIRE(wire_105->properties().text == QStringLiteral("24"));

		const DrcScanReport report = scan.run(bench.project(), rules);

			//"Clean" needs a rule to have run: an empty list is also what a
			//run with nothing switched on gives back.
		CHECK(report.rulesRun() == 1);
		CHECK(report.findingCountOf(rule) == 0);
		CHECK(report.isClean());

			//And what is left in the drawing is exactly the inverse case:
			//the same 24 on two potentials, on two folios.
		CHECK(field_24->properties().text == wire_24->properties().text);
		CHECK_FALSE(potentialOf(wire_24).contains(field_24));
		CHECK(field_24->diagram() != wire_24->diagram());
	}

	SECTION("e cala quando o relatório de folha é desfeito: o achado vinha do potencial, e não de uma folha")
	{
		going->unlinkAllElements();
		REQUIRE(going->isFree());
		REQUIRE(coming->isFree());
		REQUIRE_FALSE(potentialOf(wire_24).contains(wire_105));

		const DrcScanReport report = scan.run(bench.project(), rules);
		CHECK(report.rulesRun() == 1);
		CHECK(report.findingCountOf(rule) == 0);
	}

	SECTION("texto vazio é ausência: o potencial numerado pela metade, e o 7 escrito com espaço, não são acusados (P50)")
	{
			//What the potential numbered by halves holds, read back from the
			//drawing: the 40, the placeholder, and two texts with nothing
			//in them - empty and spaces, or empty twice if the reader of the
			//file drops the spaces. Either way, two blanks.
		QStringList texts;
		for (Conductor *conductor : potentialOf(wire_40)) {
			texts << conductor->properties().text;
		}
		INFO(texts.join(QStringLiteral("|")).toStdString());
		REQUIRE(texts.count() == 4);
		CHECK(texts.contains(QStringLiteral("40")));
		CHECK(texts.contains(DrcWireNumbers::placeholder()));
		int blanks = 0;
		for (const QString &text : std::as_const(texts))
		{
			if (text.trimmed().isEmpty()) {
				++blanks;
			}
		}
		CHECK(blanks == 2);

		const DrcScanReport report = scan.run(bench.project(), rules);
		const QList<DrcFinding> findings = report.findingsOf(rule);
		for (const DrcFinding &finding : findings)
		{
			INFO(finding.describe().toStdString());
			CHECK_FALSE(potentialOf(wire_40).contains(finding.conductor()));
			CHECK_FALSE(potentialOf(wire_7).contains(finding.conductor()));
		}
			//Only the potential of the 24 and the 105.
		CHECK(findings.count() == 1);
	}

	SECTION("a regra está no perfil de fábrica, como erro, e o painel leva ao condutor")
	{
		const DrcProjectActions::CheckOutcome outcome =
				DrcProjectActions::runCheck(bench.project());
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
		CHECK(finding.conductor() == wire_24);
		CHECK(finding.folio() == 1);

		DrcPanel panel;
		panel.setFindings(outcome.findings);
		first->clearSelection();
		REQUIRE_FALSE(wire_24->isSelected());

		CHECK(panel.activateFinding(row));
		CHECK(wire_24->isSelected());
	}
}
