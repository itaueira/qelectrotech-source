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

#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/catalog/catalogassignment.h"
#include "../../../../sources/catalog/catalogpart.h"
#include "../../../../sources/catalog/ui/catalogprojectactions.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QUndoCommand>
#include <QUndoStack>

/*
	Assigning a catalog part on the folio, and taking it back with one Ctrl+Z.

	The arithmetic of the assignment is already proved without a project open,
	in catalogassignment_test.cpp: which values a part hands over, which keys
	it may never write, what the undo entry has to say. None of that is
	repeated here.

	What is proved here is the half that only exists once a project is open,
	and that is the half the two queued cases are about: that the values reach
	the component that is drawn, that the terminals of the symbol take the
	numbers of the part, that the count of components still to buy moves by
	exactly one, and above all that a single Ctrl+Z puts all of it back at
	once. "Half undone" is the failure the roteiro asks to be written down,
	and it is the one thing a rule tested on its own can never see.

	What stays out: the window. Composants sans pièce builds its sentence
	inside showMissingPartReport, around a modal exec() that an offscreen
	suite must not enter, so the sentence and the double click that walks the
	folio to the component are still for a person to check. The number behind
	the sentence is checked here.
*/

namespace {

	const QString contactor_symbol =
		QStringLiteral("embed://bench/contactor.elmt");
	const QString breaker_symbol =
		QStringLiteral("embed://bench/breaker.elmt");

	const QString part_code = QStringLiteral("CONT-9A-24VCC");
	const QString bare_part_code = QStringLiteral("DJ-C16-2P");

	/// The keys a part fills in, and the keys the designer owns.
	const QString manufacturer_key = QStringLiteral("manufacturer");
	const QString designation_key = QStringLiteral("designation");
	const QString comment_key = QStringLiteral("comment");
	const QString label_key = QStringLiteral("label");
	const QString location_key = QStringLiteral("location");

	/**
		The project the two cases are drawn on.

		Four symbols on one sheet: two contactors, one breaker, and one
		terminal block. The terminal block is not decoration - it is what
		makes the counts below mean something, because a terminal is drawn
		like anything else and must never be counted among the things that
		get bought.
	*/
	QString fixtureXml()
	{
		struct Instance
		{
			const char *symbol;
			int x;
			int y;
			const char *label;
			const char *comment;
			const char *location;
			int uuid;
		};

		const Instance instances[] = {
			{"contactor.elmt", 100, 100, "K1", "measured 1.31 A", "X6", 1},
			{"contactor.elmt", 200, 100, "K2", "", "", 2},
			{"breaker.elmt", 300, 100, "Q1", "", "", 3},
			{"terminalblock.elmt", 400, 100, "X1", "", "", 4}};

		QString drawn;
		for (const Instance &instance : instances)
		{
			QString information = QStringLiteral(
						      "<elementInformation show=\"1\" name=\"label\">%1"
						      "</elementInformation>")
					      .arg(QLatin1String(instance.label));
			if (*instance.comment) {
				information += QStringLiteral(
						       "<elementInformation show=\"1\" name=\"comment\">%1"
						       "</elementInformation>")
					       .arg(QLatin1String(instance.comment));
			}
			if (*instance.location) {
				information += QStringLiteral(
						       "<elementInformation show=\"1\" name=\"location\">%1"
						       "</elementInformation>")
					       .arg(QLatin1String(instance.location));
			}

			drawn += QStringLiteral(
					 "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					 " freezeLabel=\"false\" orientation=\"0\""
					 " type=\"embed://bench/%3\""
					 " uuid=\"{cafe0000-0000-4000-8000-00000000000%4}\">"
					 "<terminals/><inputs/>"
					 "<elementInformations>%5</elementInformations>"
					 "<dynamic_texts/><texts_groups/>"
					 "</element>")
				 .arg(instance.x)
				 .arg(instance.y)
				 .arg(QLatin1String(instance.symbol))
				 .arg(instance.uuid)
				 .arg(information);
		}

		// One definition per symbol, each with two connection points, so that
		// the part has something to number.
		auto definition = [](const QString &file_name, const QString &english_name,
				     const QString &link_type)
		{
			return QStringLiteral(
				       "<element name=\"%1\">"
				       "<definition type=\"element\" version=\"0.80\""
				       " width=\"20\" height=\"40\""
				       " hotspot_x=\"10\" hotspot_y=\"20\""
				       " orientation=\"dnnn\" link_type=\"%3\">"
				       "<names><name lang=\"en\">%2</name></names>"
				       "<description>"
				       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
				       " end1=\"none\" end2=\"none\" length1=\"1.5\""
				       " length2=\"1.5\" antialias=\"false\""
				       " style=\"line-style:normal;line-weight:normal;"
				       "filling:none;color:black\"/>"
				       "<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
				       "<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"
				       "</description>"
				       "</definition>"
				       "</element>")
			       .arg(file_name, english_name, link_type);
		};

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2%3</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%4</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definition(QStringLiteral("contactor.elmt"),
				       QStringLiteral("Contactor"),
				       QStringLiteral("simple")),
			    definition(QStringLiteral("breaker.elmt"),
				       QStringLiteral("Breaker"),
				       QStringLiteral("simple")),
			    definition(QStringLiteral("terminalblock.elmt"),
				       QStringLiteral("Terminal block"),
				       QStringLiteral("terminal")),
			    drawn);
	}

	/// A catalog held in memory, with one part that numbers its pins and one that does not.
	struct CatalogFixture
	{
		CatalogFixture()
		{
			QString error;
			const bool opened = catalog.openInMemory(&error);
			INFO(error.toStdString());
			REQUIRE(opened);

			const int contactor_class =
				catalog.classByKey(QStringLiteral("contactor")).id;
			REQUIRE(contactor_class > 0);

			part = CatalogPart(part_code, contactor_class);
			part.setValue(designation_key,
				      QStringLiteral("Contactor 9 A, 24 Vdc coil"));
			part.setValue(manufacturer_key, QStringLiteral("Supplier A"));

			CatalogPin a1(QStringLiteral("A1"), CatalogPinRole::Coil);
			a1.group = contactor_symbol;
			CatalogPin a2(QStringLiteral("A2"), CatalogPinRole::Coil);
			a2.group = contactor_symbol;
			part.pins << a1 << a2;

			QString save_error;
			REQUIRE(catalog.savePart(part, &save_error));
			REQUIRE(save_error.isEmpty());

			// The part of the second case: a real part, with a manufacturer
			// and a designation, and not one single pin. That is not an
			// exotic shape - it is what a catalog filled from a price list
			// looks like before anybody types a pin table.
			bare_part = CatalogPart(bare_part_code, contactor_class);
			bare_part.setValue(designation_key, QStringLiteral("Breaker 16 A, curve C"));
			bare_part.setValue(manufacturer_key, QStringLiteral("Supplier B"));
			REQUIRE(catalog.savePart(bare_part, &save_error));
			REQUIRE(save_error.isEmpty());
		}

		Catalog catalog;
		CatalogPart part;
		CatalogPart bare_part;
	};

	Element *component(Diagram *sheet, const QString &label)
	{
		if (!sheet) {
			return nullptr;
		}
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->elementInformations().value(label_key).toString() == label) {
				return element;
			}
		}
		return nullptr;
	}

	QString informationOf(Element *element, const QString &key)
	{
		return element ? element->elementInformations().value(key).toString()
			       : QString();
	}

	/// The name each terminal carries in this project, in terminal order.
	QStringList terminalNames(Element *element)
	{
		QStringList names;
		if (!element) {
			return names;
		}
		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *terminal : terminals) {
			names << terminal->instanceName();
		}
		return names;
	}

	/// The tags of the components a report would list, sorted so the order is stable.
	QStringList labelsWithoutPart(QETProject *project)
	{
		QStringList labels;
		const QList<Element *> missing =
			CatalogProjectActions::componentsWithoutPart(project);
		for (Element *element : missing) {
			labels << informationOf(element, label_key);
		}
		labels.sort();
		return labels;
	}

	/// Which terminal is not back where it was, said out loud.
	QString firstNameThatStayed(const QStringList &expected, const QStringList &found)
	{
		const int shared = qMin(expected.count(), found.count());
		for (int index = 0 ; index < shared ; ++index)
		{
			if (expected.at(index) != found.at(index)) {
				return QStringLiteral("terminal %1: expected \"%2\", found \"%3\"")
				       .arg(QString::number(index), expected.at(index),
					    found.at(index));
			}
		}
		return QString();
	}

	/// Half of an assignment: the information, and nothing else.
	class InformationOnlyCommand : public QUndoCommand
	{
		public:
			InformationOnlyCommand(Element *element, const DiagramContext &after) :
				QUndoCommand(QStringLiteral("information only")),
				m_element(element),
				m_before(element->elementInformations()),
				m_after(after)
			{}

			void undo() override {m_element->setElementInformations(m_before);}
			void redo() override {m_element->setElementInformations(m_after);}

		private:
			Element *m_element = nullptr;
			DiagramContext m_before;
			DiagramContext m_after;
	};

	/// The other half: the terminal names, and nothing else.
	class TerminalNamesOnlyCommand : public QUndoCommand
	{
		public:
			TerminalNamesOnlyCommand(Element *element, const QStringList &after) :
				QUndoCommand(QStringLiteral("terminal names only")),
				m_element(element),
				m_before(terminalNames(element)),
				m_after(after)
			{}

			void undo() override {apply(m_before);}
			void redo() override {apply(m_after);}

		private:
			void apply(const QStringList &names)
			{
				const QList<Terminal *> terminals = m_element->terminals();
				for (int index = 0 ;
				     index < terminals.count() && index < names.count() ;
				     ++index) {
					terminals.at(index)->setInstanceName(names.at(index));
				}
			}

			Element *m_element = nullptr;
			QStringList m_before;
			QStringList m_after;
	};
}

TEST_CASE("F1 B.2 (fila) — atribuir a peça enche os campos e os bornes, e um Ctrl+Z desfaz os dois",
	  "[uibench][catalog]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(), QStringLiteral("assign.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 4);

	Element *k1 = component(sheet, QStringLiteral("K1"));
	REQUIRE(k1 != nullptr);
	REQUIRE(k1->terminals().count() == 2);

	SECTION("o simbolo com que o componente foi desenhado e a chave dos pinos")
	{
		// The join between a part and a drawing is this string, and nothing
		// else. Written down because a test that reads it from the element
		// and hands it straight back to the part would pass whatever it was.
		REQUIRE(k1->location().path() == contactor_symbol);

		Element *q1 = component(sheet, QStringLiteral("Q1"));
		REQUIRE(q1 != nullptr);
		REQUIRE(q1->location().path() == breaker_symbol);
	}

	SECTION("atribuir enche os campos com os valores da peça")
	{
		REQUIRE(informationOf(k1, manufacturer_key).isEmpty());

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, fixture.part) == 1);

		REQUIRE(informationOf(k1, manufacturer_key) == QStringLiteral("Supplier A"));
		REQUIRE(informationOf(k1, designation_key)
			== QStringLiteral("Contactor 9 A, 24 Vdc coil"));
		REQUIRE(informationOf(k1, CatalogAssignment::partCodeKey()) == part_code);
	}

	SECTION("atribuir poe nos bornes os numeros da peça")
	{
		REQUIRE(terminalNames(k1) == QStringList({QString(), QString()}));

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, fixture.part) == 1);

		REQUIRE(terminalNames(k1)
			== QStringList({QStringLiteral("A1"), QStringLiteral("A2")}));
	}

	SECTION("a peça so numera o simbolo que ela conhece")
	{
		// The other side of the same border: the pins of this part belong to
		// the contactor symbol, so the breaker gets the values and keeps its
		// own connection points untouched.
		Element *q1 = component(sheet, QStringLiteral("Q1"));
		REQUIRE(q1 != nullptr);

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({q1}), fixture.catalog, fixture.part) == 1);

		REQUIRE(informationOf(q1, manufacturer_key) == QStringLiteral("Supplier A"));
		REQUIRE(terminalNames(q1) == QStringList({QString(), QString()}));
	}

	SECTION("um unico Ctrl+Z desfaz os campos e os bornes juntos")
	{
		const int steps_before = sheet->undoStack().index();
		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, fixture.part) == 1);
		REQUIRE(sheet->undoStack().index() == steps_before + 1);

		sheet->undoStack().undo();

		REQUIRE(sheet->undoStack().index() == steps_before);
		REQUIRE(informationOf(k1, manufacturer_key).isEmpty());
		REQUIRE(informationOf(k1, CatalogAssignment::partCodeKey()).isEmpty());
		const QStringList empty({QString(), QString()});
		INFO(firstNameThatStayed(empty, terminalNames(k1)).toStdString());
		REQUIRE(terminalNames(k1) == empty);
	}

	SECTION("controle negativo — em dois comandos, um Ctrl+Z deixa a folha pela metade")
	{
		/*
			The check above is worth exactly as much as its ability to tell
			one command from two, so the assignment is taken apart into the
			two halves it is made of and pushed separately - which is the
			failure the roteiro asks to be recorded apart: the undo gave back
			the fields and left the terminals with the numbers of a part the
			component no longer has.
		*/
		DiagramContext after = k1->elementInformations();
		after.addValue(manufacturer_key, QStringLiteral("Supplier A"));

		const int steps_before = sheet->undoStack().index();
		sheet->undoStack().push(new TerminalNamesOnlyCommand(
						k1, QStringList({QStringLiteral("A1"),
								 QStringLiteral("A2")})));
		sheet->undoStack().push(new InformationOnlyCommand(k1, after));
		REQUIRE(sheet->undoStack().index() == steps_before + 2);

		sheet->undoStack().undo();

		// The fields came back; the drawing did not.
		REQUIRE(informationOf(k1, manufacturer_key).isEmpty());
		const QStringList empty({QString(), QString()});
		INFO(firstNameThatStayed(empty, terminalNames(k1)).toStdString());
		REQUIRE_FALSE(terminalNames(k1) == empty);
		REQUIRE_FALSE(firstNameThatStayed(empty, terminalNames(k1)).isEmpty());

		// And the second undo finishes what the first one left.
		sheet->undoStack().undo();
		REQUIRE(terminalNames(k1) == empty);
	}

	SECTION("o que o projetista digitou continua igual")
	{
		REQUIRE(informationOf(k1, comment_key) == QStringLiteral("measured 1.31 A"));
		REQUIRE(informationOf(k1, location_key) == QStringLiteral("X6"));

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, fixture.part) == 1);

		REQUIRE(informationOf(k1, label_key) == QStringLiteral("K1"));
		REQUIRE(informationOf(k1, comment_key) == QStringLiteral("measured 1.31 A"));
		REQUIRE(informationOf(k1, location_key) == QStringLiteral("X6"));
	}

	SECTION("controle negativo — campo que a peça declara sobrescreve, e isso e de proposito")
	{
		// The other side of the rule above, and the reason changing the part
		// of a component is worth anything: an empty field of the part says
		// nothing, a filled one says the product is another one. The tag and
		// the location stay protected either way.
		CatalogPart talkative = fixture.part;
		talkative.setValue(comment_key, QStringLiteral("from the part sheet"));
		talkative.setValue(location_key, QStringLiteral("Z9"));

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, talkative) == 1);

		REQUIRE(informationOf(k1, comment_key) == QStringLiteral("from the part sheet"));
		REQUIRE(informationOf(k1, location_key) == QStringLiteral("X6"));
		REQUIRE(informationOf(k1, label_key) == QStringLiteral("K1"));
	}

	SECTION("a legenda do desfazer nomeia a tag, e nao o simbolo")
	{
		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, fixture.part) == 1);

		const QString entry = sheet->undoStack().text(sheet->undoStack().index() - 1);
		INFO(entry.toStdString());
		REQUIRE(entry == QStringLiteral("Attribuer la pièce CONT-9A-24VCC à K1"));
		REQUIRE_FALSE(entry.contains(QStringLiteral("Contactor")));
	}

	SECTION("controle negativo — sem tag, a legenda nao pode inventar uma")
	{
		Element *k2 = component(sheet, QStringLiteral("K2"));
		REQUIRE(k2 != nullptr);
		DiagramContext without_tag = k2->elementInformations();
		without_tag.addValue(label_key, QString());
		k2->setElementInformations(without_tag);

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k2}), fixture.catalog, fixture.part) == 1);

		const QString entry = sheet->undoStack().text(sheet->undoStack().index() - 1);
		INFO(entry.toStdString());
		REQUIRE(entry.contains(part_code));
		REQUIRE_FALSE(entry.contains(QStringLiteral("K1")));
		REQUIRE_FALSE(entry.endsWith(QStringLiteral(" à ")));
	}
}

TEST_CASE("CV D.3 (fila) — a conta de quem esta sem peça cai por um, e o Ctrl+Z a traz de volta",
	  "[uibench][catalog]")
{
	CatalogFixture fixture;

	UiBench::ScratchProject scratch(fixtureXml(), QStringLiteral("missing.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	Element *k1 = component(sheet, QStringLiteral("K1"));
	REQUIRE(k1 != nullptr);

	SECTION("a conta de partida separa o que se compra do que se desenha")
	{
		// Four symbols are drawn and three of them get bought: a terminal
		// block is not an item of a bill of material, and counting it would
		// make the report useless on its first run.
		REQUIRE(sheet->elements().count() == 4);
		REQUIRE(CatalogProjectActions::components(scratch.project()).count() == 3);
		REQUIRE(CatalogProjectActions::componentsWithoutPart(scratch.project()).count() == 3);
		REQUIRE(labelsWithoutPart(scratch.project())
			== QStringList({QStringLiteral("K1"), QStringLiteral("K2"),
					QStringLiteral("Q1")}));
	}

	SECTION("uma atribuicao tira exatamente uma linha da lista")
	{
		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, fixture.part) == 1);

		REQUIRE(CatalogProjectActions::componentsWithoutPart(scratch.project()).count() == 2);
		REQUIRE(labelsWithoutPart(scratch.project())
			== QStringList({QStringLiteral("K2"), QStringLiteral("Q1")}));
	}

	SECTION("o Ctrl+Z devolve a linha")
	{
		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1}), fixture.catalog, fixture.part) == 1);
		REQUIRE(CatalogProjectActions::componentsWithoutPart(scratch.project()).count() == 2);

		sheet->undoStack().undo();

		REQUIRE(CatalogProjectActions::componentsWithoutPart(scratch.project()).count() == 3);
		REQUIRE(labelsWithoutPart(scratch.project())
			== QStringList({QStringLiteral("K1"), QStringLiteral("K2"),
					QStringLiteral("Q1")}));
	}

	SECTION("uma peça sem pino nenhum enche os campos e nao mexe em borne nenhum")
	{
		/*
			The case the roteiro warns about before it is run: a catalog
			filled from a price list has parts and no pin table at all, and
			assigning one of them must not blank a name somebody typed on a
			terminal. Here the terminals carry no typed name, so what is
			checked is the other half of the same promise - the assignment
			leaves them exactly as the symbol drew them.
		*/
		Element *q1 = component(sheet, QStringLiteral("Q1"));
		REQUIRE(q1 != nullptr);
		const QStringList before = terminalNames(q1);
		REQUIRE(before == QStringList({QString(), QString()}));

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({q1}), fixture.catalog, fixture.bare_part) == 1);

		REQUIRE(informationOf(q1, manufacturer_key) == QStringLiteral("Supplier B"));
		REQUIRE(informationOf(q1, CatalogAssignment::partCodeKey()) == bare_part_code);
		INFO(firstNameThatStayed(before, terminalNames(q1)).toStdString());
		REQUIRE(terminalNames(q1) == before);
		REQUIRE(CatalogProjectActions::componentsWithoutPart(scratch.project()).count() == 2);
	}

	SECTION("controle negativo — uma peça com pino move o nome do borne, e diz qual")
	{
		// Without this the section above would pass on a program that never
		// writes a terminal name at all.
		Element *k2 = component(sheet, QStringLiteral("K2"));
		REQUIRE(k2 != nullptr);
		const QStringList before = terminalNames(k2);

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k2}), fixture.catalog, fixture.part) == 1);

		INFO(firstNameThatStayed(before, terminalNames(k2)).toStdString());
		REQUIRE_FALSE(terminalNames(k2) == before);
		REQUIRE_FALSE(firstNameThatStayed(before, terminalNames(k2)).isEmpty());
	}

	SECTION("controle negativo — atribuir a dois componentes tira duas linhas")
	{
		// A counter that always answered "one less" would pass every section
		// above.
		Element *k2 = component(sheet, QStringLiteral("K2"));
		REQUIRE(k2 != nullptr);

		REQUIRE(CatalogProjectActions::assignPart(
				QList<Element *>({k1, k2}), fixture.catalog, fixture.part) == 2);

		REQUIRE(CatalogProjectActions::componentsWithoutPart(scratch.project()).count() == 1);
		REQUIRE(labelsWithoutPart(scratch.project())
			== QStringList({QStringLiteral("Q1")}));

		sheet->undoStack().undo();
		REQUIRE(CatalogProjectActions::componentsWithoutPart(scratch.project()).count() == 3);
	}

	SECTION("controle negativo — o que nao se compra nunca entra na conta, atribuido ou nao")
	{
		Element *x1 = component(sheet, QStringLiteral("X1"));
		REQUIRE(x1 != nullptr);
		REQUIRE(informationOf(x1, CatalogAssignment::partCodeKey()).isEmpty());

		const QList<Element *> missing =
			CatalogProjectActions::componentsWithoutPart(scratch.project());
		REQUIRE_FALSE(missing.contains(x1));
		REQUIRE_FALSE(labelsWithoutPart(scratch.project())
			      .contains(QStringLiteral("X1")));
	}
}
