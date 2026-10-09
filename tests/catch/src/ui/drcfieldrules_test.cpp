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

#include "../../../../sources/catalog/catalogassignment.h"
#include "../../../../sources/catalog/ui/catalogprojectactions.h"
#include "../../../../sources/dataBase/drcsqlengine.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/drc/drcfieldrules.h"
#include "../../../../sources/drc/drcruleset.h"
#include "../../../../sources/drc/drcscanengine.h"
#include "../../../../sources/drc/ui/drcpanel.h"
#include "../../../../sources/drc/ui/drcprojectactions.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QHash>
#include <QSet>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QUuid>
#include <QVariant>

/*
	The rule of the query route that asks every component that gets bought
	for its catalogue part.

	It is read three ways, and each one can go wrong without the other two
	noticing:

	1. what the project data base holds for a field left empty - a key the
	   component never had is NULL, not '' - and why the predicate is not
	   TRIM(col) = '': that one is NULL on the component that never had a
	   code, which is the component the rule is written for;
	2. what the rule names and what it leaves alone on purpose - the part
	   assigned, the kinds that are not bought, the component ticked out of
	   the bill of materials;
	3. that it says what the missing-part report of the catalogue says on
	   the same project, and that the one difference between the two is
	   the one P109 decided.

	Every finding has to navigate: folio and cell, written by the same
	function the walk route uses, and the cell the very string the data
	base keeps in element.pos.
*/

namespace
{
	/// One component of the fixture.
	struct Item
	{
		const char *definition;
		int folio;
		int x;
		int y;
		const char *label;
			/// Extra elementInformation lines, written into the file as is.
		const char *informations;
	};

	/**
		Thirteen components over two folios.

		Each kind the rule must leave alone is there without a part: the
		terminal, the thumbnail and the folio report have no code and must
		not be named, so that a rule that forgot the kind filter fails here
		instead of passing on a fixture where everything happened to carry
		a code.

		K3, K4 and K5 carry a code in the file and lose it during the case,
		through the undo command the information panel uses: that is the
		only way a code made of spaces reaches a project, since the reader
		of the file drops a text made only of spacing. K2 is alone on the
		second folio, so that a folio number of 1 on every finding cannot
		pass for right.
	*/
	const Item items[] = {
		{"plain.elmt",   1, 100, 150, "K1",
		 "<elementInformation show=\"0\" name=\"part_code\">ABC-1"
		 "</elementInformation>"},
		{"plain.elmt",   1, 250, 150, "K3",
		 "<elementInformation show=\"0\" name=\"part_code\">TEMP-3"
		 "</elementInformation>"},
		{"plain.elmt",   1, 400, 150, "K4",
		 "<elementInformation show=\"0\" name=\"part_code\">TEMP-4"
		 "</elementInformation>"},
		{"plain.elmt",   1, 550, 150, "K5",
		 "<elementInformation show=\"0\" name=\"part_code\">TEMP-5"
		 "</elementInformation>"},
		{"plain.elmt",   1, 700, 150, "K6",
		 "<elementInformation show=\"0\" name=\"part_code\">"
		 "</elementInformation>"},
		{"coil.elmt",    1, 850, 150, "KM1", ""},
		{"contact.elmt", 1, 100, 400, "KA1", ""},
		{"plain.elmt",   1, 250, 400, "K8",
		 "<elementInformation show=\"0\" name=\"exclude_from_bom\">1"
		 "</elementInformation>"},
		{"plain.elmt",   1, 400, 400, "K9",
		 "<elementInformation show=\"0\" name=\"exclude_from_bom\">true"
		 "</elementInformation>"},
		{"strip.elmt",   1, 550, 400, "X1", ""},
		{"picture.elmt", 1, 700, 400, "T1", ""},
		{"going.elmt",   1, 850, 400, "R1", ""},
		{"plain.elmt",   2, 400, 300, "K2", ""}};

	QString fixtureXml()
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
			{"picture.elmt", "thumbnail", ""},
			{"going.elmt", "next_report", ""}};

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

		QString folios[2];
		int index = 0;
		for (const Item &item : items)
		{
			const QString uuid = QStringLiteral(
						     "{decade00-0000-4000-8000-0000000000%1}")
					     .arg(index, 2, 10, QLatin1Char('0'));

			folios[item.folio - 1] += QStringLiteral(
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
					     "%7"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(item.x)
				     .arg(item.y)
				     .arg(QLatin1String(item.definition))
				     .arg(uuid)
				     .arg(index)
				     .arg(QLatin1String(item.label))
				     .arg(QLatin1String(item.informations));
			++index;
		}

		const QString sheet = QStringLiteral(
			"<diagram title=\"Bench %1\" order=\"%1\" folio=\"%1\""
			" height=\"640\""
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
			       "</collection>"
			       "%2%3"
			       "</project>")
		       .arg(definitions,
			    sheet.arg(1).arg(folios[0]),
			    sheet.arg(2).arg(folios[1]));
	}

	/// The component of @a project whose tag is @a label.
	Element *componentOf(QETProject *project, const QString &label)
	{
		const QList<Diagram *> diagrams = project->diagrams();
		for (Diagram *diagram : diagrams)
		{
			const QList<Element *> elements = diagram->elements();
			for (Element *element : elements)
			{
				if (element->actualLabel() == label) {
					return element;
				}
			}
		}
		return nullptr;
	}

	/**
		Give @a element the part code @a code, the way the information
		panel does: through the undo stack, which is also what tells the
		project data base the row changed.
	*/
	void setPartCode(QETProject *project, Element *element, const QString &code)
	{
		const DiagramContext old_info = element->elementInformations();
		DiagramContext new_info = old_info;
		new_info.addValue(CatalogAssignment::partCodeKey(), code);
		project->undoStack()->push(
			new ChangeElementInformationCommand(element, old_info, new_info));
	}

	/// The three empty codes the rule has to see through.
	QString emptyCode()
	{
		return QStringLiteral("");
	}

	QString spacesCode()
	{
		return QStringLiteral("   ");
	}

	/**
		A tab and a non-breaking space, which QString::trimmed() removes and
		the plain SQL TRIM() does not. Built from code points so that no
		escape sequence in this file can be eaten on its way to the disk.
	*/
	QString otherBlankCode()
	{
		return QString(QChar(0x0009)) + QChar(0x00A0);
	}

	/// K3, K4 and K5 lose their code, each in one of the three shapes.
	void blankTheCodes(QETProject *project)
	{
		setPartCode(project, componentOf(project, QStringLiteral("K3")),
			    emptyCode());
		setPartCode(project, componentOf(project, QStringLiteral("K4")),
			    spacesCode());
		setPartCode(project, componentOf(project, QStringLiteral("K5")),
			    otherBlankCode());
	}

	/// The tags the part rule named, sorted.
	QStringList taggedBy(const QList<DrcFinding> &findings)
	{
		QStringList tags;
		for (const DrcFinding &finding : findings)
		{
			if (finding.ruleIdentifier()
			    != DrcFieldRules::elementWithoutPartIdentifier()) {
				continue;
			}
			Element *element = finding.element();
			tags << (element ? element->actualLabel() : QString());
		}
		tags.sort();
		return tags;
	}

	QStringList labelsOf(const QList<Element *> &elements)
	{
		QStringList labels;
			//A finding with no element is the failure being reported,
			//and the message that names it must not take the run down.
		for (Element *element : elements) {
			labels << (element ? element->actualLabel()
					   : QStringLiteral("(no element)"));
		}
		labels.sort();
		return labels;
	}

	/// The first column of the first row of @a statement, or an invalid value.
	QVariant scalar(QETProject *project, const QString &statement)
	{
		QSqlQuery query = project->dataBase()->newQuery(statement);
		if (!query.isActive() || !query.next()) {
			return QVariant();
		}
		return query.value(0);
	}

	/// How many element_info rows of @a label the condition @a where keeps.
	int countWhere(QETProject *project,
		       const QString &label,
		       const QString &where)
	{
		return scalar(project,
			      QStringLiteral("SELECT COUNT(*) FROM element_info ei"
					     " WHERE ei.label = '%1' AND %2")
			      .arg(label, where)).toInt();
	}
}

TEST_CASE("T24 — campo ausente chega ao banco como NULL, e o predicado da regra pega os três vazios",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcblank.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	QETProject *project = bench.project();
	REQUIRE(componentOf(project, QStringLiteral("K2")));
	REQUIRE(componentOf(project, QStringLiteral("K3")));
	REQUIRE(componentOf(project, QStringLiteral("K4")));
	REQUIRE(componentOf(project, QStringLiteral("K5")));

		//The probe of the probe: the empty code handed to the command is an
		//empty text and not a null one, or the second shape below would be
		//the first one again under another name.
	REQUIRE_FALSE(emptyCode().isNull());
	REQUIRE(emptyCode().isEmpty());

	blankTheCodes(project);
	project->dataBase()->updateDB();

	const QString part = QStringLiteral("ei.")
			     + CatalogAssignment::partCodeKey();

	SECTION("o que o banco guarda em cada forma de vazio")
	{
			//K2 never had the key: the binder hands a null string, and it
			//lands as NULL. This is the measurement the predicate rests on.
		CHECK(countWhere(project, QStringLiteral("K2"),
				 part + QStringLiteral(" IS NULL")) == 1);
			//K3 holds an empty text, and it lands as one.
		CHECK(countWhere(project, QStringLiteral("K3"),
				 part + QStringLiteral(" IS NULL")) == 0);
		CHECK(countWhere(project, QStringLiteral("K3"),
				 part + QStringLiteral(" = ''")) == 1);
			//K4 holds its three spaces as they were typed.
		CHECK(countWhere(project, QStringLiteral("K4"),
				 part + QStringLiteral(" = '   '")) == 1);

			//K6 is written empty in the file. Which of the two shapes the
			//reader gives it is the reader's business, and the rule must
			//not care - so it is said here, not asserted.
		INFO("K6 IS NULL: "
		     << countWhere(project, QStringLiteral("K6"),
				   part + QStringLiteral(" IS NULL")));
		CHECK(countWhere(project, QStringLiteral("K6"),
				 DrcFieldRules::blankCondition(part)) == 1);
	}

	SECTION("TRIM(col) = '' cala sobre o que nunca teve o campo; o predicado da regra, não")
	{
		const QString naive = QStringLiteral("TRIM(%1) = ''").arg(part);
		const QString rule = DrcFieldRules::blankCondition(part);

			//The plan's predicate: right on the empty text and the spaces,
			//and silent on the component that never had a code at all.
		CHECK(countWhere(project, QStringLiteral("K2"), naive) == 0);
		CHECK(countWhere(project, QStringLiteral("K3"), naive) == 1);
		CHECK(countWhere(project, QStringLiteral("K4"), naive) == 1);
			//And blind to the tab and the non-breaking space.
		CHECK(countWhere(project, QStringLiteral("K5"), naive) == 0);

		CHECK(countWhere(project, QStringLiteral("K2"), rule) == 1);
		CHECK(countWhere(project, QStringLiteral("K3"), rule) == 1);
		CHECK(countWhere(project, QStringLiteral("K4"), rule) == 1);
		CHECK(countWhere(project, QStringLiteral("K5"), rule) == 1);

			//And it is no blanket "true": a real code stays a code, with or
			//without blanks around it.
		CHECK(countWhere(project, QStringLiteral("K1"), rule) == 0);
	}

	SECTION("o predicado diz o que CatalogAssignment::isWithoutPart() diz, valor a valor")
	{
		const QString codes[] = {QString(), emptyCode(), spacesCode(),
					 otherBlankCode(),
					 QStringLiteral("ABC-1"),
					 QStringLiteral("  ABC-1 ")};
		const QString rule = DrcFieldRules::blankCondition(QStringLiteral("v"));

		for (const QString &code : codes)
		{
			QHash<QString, QString> values;
			if (!code.isNull()) {
				values.insert(CatalogAssignment::partCodeKey(), code);
			}

				//The value is spelled by its code points, so that what is
				//compared is the value and not a quoting of it: NULL for
				//the key that is not there, '' for the empty text.
			QString literal = QStringLiteral("NULL");
			if (!code.isNull())
			{
				QStringList points;
				for (const QChar character : code) {
					points << QString::number(character.unicode());
				}
				literal = points.isEmpty()
					  ? QStringLiteral("''")
					  : QStringLiteral("char(%1)")
					    .arg(points.join(QLatin1Char(',')));
			}

			const QVariant answer = scalar(
						project,
						QStringLiteral("SELECT CASE WHEN %1 THEN 1 ELSE 0 END"
							       " FROM (SELECT %2 AS v)")
						.arg(rule, literal));

			INFO("code length " << code.length()
			     << (code.isNull() ? " (null)" : ""));
			REQUIRE(answer.isValid());
			CHECK((answer.toInt() == 1)
			      == CatalogAssignment::isWithoutPart(values));
		}
	}
}

TEST_CASE("T24 — componente comprável sem peça: acusa os três vazios, e cala sobre o que tem peça, o que não se compra e o que saiu da lista",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcpart.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	QETProject *project = bench.project();
	REQUIRE(bench.diagramCount() == 2);
	blankTheCodes(project);

	const DrcProjectActions::CheckOutcome outcome =
			DrcProjectActions::runCheck(project);
	INFO(outcome.summary().toStdString());
	INFO(outcome.failures.join(QStringLiteral("\n")).toStdString());

	REQUIRE(outcome.failures.isEmpty());
	CHECK(outcome.sql_rules_run == 1);

	SECTION("nomeia exatamente os compráveis sem peça")
	{
			//K2 absent, K3 empty, K4 spaces, K5 tab and no-break space, K6
			//empty in the file; a coil and a contact, which are bought; and
			//K8, whose flag is "1" - kept by the parts list, so asked here.
		CHECK(taggedBy(outcome.findings)
		      == QStringList({QStringLiteral("K2"), QStringLiteral("K3"),
				      QStringLiteral("K4"), QStringLiteral("K5"),
				      QStringLiteral("K6"), QStringLiteral("K8"),
				      QStringLiteral("KA1"), QStringLiteral("KM1")}));

			//And not: K1 has its part, K9 is ticked out of the bill of
			//materials, and the terminal, the thumbnail and the folio report
			//are not bought. Said one by one so that a failure names which.
		const QStringList named = taggedBy(outcome.findings);
		for (const char *quiet : {"K1", "K9", "X1", "T1", "R1"}) {
			INFO(quiet);
			CHECK_FALSE(named.contains(QLatin1String(quiet)));
		}
	}

	SECTION("cada achado leva ao componente: folha e casa da grade")
	{
		int found = 0;
		for (const DrcFinding &finding : outcome.findings)
		{
			if (finding.ruleIdentifier()
			    != DrcFieldRules::elementWithoutPartIdentifier()) {
				continue;
			}
			++found;

			INFO(finding.describe().toStdString());
			REQUIRE(finding.hasTarget());
			Element *element = finding.element();
			REQUIRE(element);
			REQUIRE(element->diagram());

			CHECK(finding.diagram() == element->diagram());
			CHECK(finding.folio()
			      == project->folioIndex(element->diagram()) + 1);
			CHECK(finding.position()
			      == element->diagram()->convertPosition(
					 element->scenePos()).toString());
			CHECK_FALSE(finding.position().isEmpty());

				//The sentence names the component.
			CHECK(finding.text().contains(element->actualLabel()));
		}
		CHECK(found == 8);

			//K2 is the one on the second folio, and its finding says so.
		for (const DrcFinding &finding : outcome.findings)
		{
			if (finding.element()
			    && finding.element()->actualLabel() == QLatin1String("K2")) {
				CHECK(finding.folio() == 2);
			}
		}
	}

	SECTION("a linha do banco e o achado dão o mesmo endereço")
	{
			//element.pos is the cell of the grid and diagram.pos the order
			//of the sheet. The rule selects both, aliased apart, and the
			//finding built from the scene has to say the same two things.
		DrcRuleSet rules;
		DrcSqlEngine sql;
		REQUIRE(DrcFieldRules::registerRules(rules, sql) == 1);

		const DrcSqlReport report = sql.run(project, rules);
		const DrcSqlResult result = report.result(
						    DrcFieldRules::elementWithoutPartIdentifier());
		INFO(result.error().toStdString());
		REQUIRE(result.status() == DrcQueryStatus::Succeeded);
		REQUIRE(result.findingCount() == 8);

		for (int row = 0 ; row < result.findingCount() ; ++row)
		{
			const QUuid uuid(result.value(row,
						      DrcFieldRules::elementUuidColumn()));
			Element *element = nullptr;
			for (const DrcFinding &finding : outcome.findings)
			{
				if (finding.element() && finding.element()->uuid() == uuid)
				{
					element = finding.element();
					INFO(finding.describe().toStdString());
					CHECK(result.value(row, QStringLiteral("position"))
					      == finding.position());
					CHECK(result.value(row, QStringLiteral("folio")).toInt()
					      == finding.folio());
				}
			}
			INFO(result.value(row, QStringLiteral("label")).toStdString());
			CHECK(element);
		}
	}

	SECTION("o painel leva da linha ao componente")
	{
		DrcPanel panel;
		panel.setFindings(outcome.findings);

		for (int row = 0 ; row < outcome.findings.count() ; ++row)
		{
			const DrcFinding &finding = outcome.findings.at(row);
			if (finding.ruleIdentifier()
			    != DrcFieldRules::elementWithoutPartIdentifier()) {
				continue;
			}
			Element *element = finding.element();
			REQUIRE(element);
			element->diagram()->clearSelection();
			REQUIRE_FALSE(element->isSelected());

			CHECK(panel.activateFinding(row));
			CHECK(element->isSelected());
		}
	}

	SECTION("e cala quando cada um ganha a sua peça")
	{
			//The same drawing, the same components, the same tags: only the
			//codes change, so only the codes can account for the findings
			//going away.
		for (const char *tag : {"K2", "K3", "K4", "K5", "K6", "K8",
					"KA1", "KM1"})
		{
			Element *element = componentOf(project, QLatin1String(tag));
			REQUIRE(element);
			setPartCode(project, element,
				    QStringLiteral("P-%1").arg(QLatin1String(tag)));
		}

		const DrcProjectActions::CheckOutcome after =
				DrcProjectActions::runCheck(project);
		INFO(after.summary().toStdString());
		CHECK(after.failures.isEmpty());
		CHECK(after.sql_rules_run == 1);
		CHECK(taggedBy(after.findings).isEmpty());
	}
}

TEST_CASE("T24 — o painel e o relatório de componentes sem peça dizem a mesma coisa",
	  "[uibench][drc][t24]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("drcreport.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	QETProject *project = bench.project();
	blankTheCodes(project);

	const DrcProjectActions::CheckOutcome outcome =
			DrcProjectActions::runCheck(project);
	REQUIRE(outcome.failures.isEmpty());

	QSet<Element *> panel;
	for (const DrcFinding &finding : outcome.findings)
	{
		if (finding.ruleIdentifier()
		    == DrcFieldRules::elementWithoutPartIdentifier()) {
			panel.insert(finding.element());
		}
	}

	const QList<Element *> report =
			CatalogProjectActions::componentsWithoutPart(project);

		//The report does not excuse the component ticked out of the bill of
		//materials, and the panel does: that is the decision, and it is
		//the only difference allowed between the two. The test of "ticked"
		//is the literal the parts list drops the row on.
	QList<Element *> report_minus_excluded;
	QList<Element *> excluded;
	for (Element *element : report)
	{
		if (element->elementInformations()
		    .value(QStringLiteral("exclude_from_bom")).toString()
		    == QLatin1String("true")) {
			excluded << element;
		} else {
			report_minus_excluded << element;
		}
	}

	INFO("report: " << labelsOf(report).join(QStringLiteral(" ")).toStdString());
	INFO("panel: " << labelsOf(panel.values()).join(QStringLiteral(" ")).toStdString());

		//Not empty, or the comparison below would hold between two lists
		//that both found nothing.
	REQUIRE(panel.count() == 8);

	CHECK(panel == QSet<Element *>(report_minus_excluded.begin(),
				       report_minus_excluded.end()));
	CHECK(labelsOf(excluded) == QStringList({QStringLiteral("K9")}));
}
