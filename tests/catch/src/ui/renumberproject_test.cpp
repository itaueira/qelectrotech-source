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

#include "../../../../sources/autoNum/assemblystate.h"
#include "../../../../sources/autoNum/numberingformat.h"
#include "../../../../sources/autoNum/projectrenumberer.h"
#include "../../../../sources/autoNum/renumberplan.h"
#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/assemblystatecommand.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QPointF>
#include <QUndoCommand>
#include <QUndoStack>

/*
	Renumbering a project after seeing what it would do.

	The ordering rules themselves are proved without a project open, in
	renumber_test.cpp, on RenumberInput objects built by hand. That is the
	right place for them and they are not repeated here.

	What that suite cannot see is where those inputs come from, and it is the
	whole of the queued case: the position each object is sorted by is read
	off the folio it is drawn on, the tag root off the tag it already
	carries, and the "leave this one alone" off what somebody typed. A rule
	that sorts perfectly on inputs somebody wrote by hand still renumbers the
	drawing in the wrong order if the position it is given is not the
	position on the sheet - and that is exactly the complaint the roteiro
	asks to be written down.

	So the objects here are never built by hand: every one of them is read
	from a project that was loaded from a file, through the same
	ProjectRenumberer the dialog calls.

	What stays out: the dialog. That the preview table shows these lines, in
	this order, with the skipped ones marked, is a window and stays for a
	person to look at. What is checked here is the plan the table is filled
	from.
*/

namespace {

	/// The tolerance of the reading order, in scene units, as the roteiro states it.
	const qreal ordering_tolerance = 10.0;

	struct Instance
	{
		const char *symbol;
		qreal x;
		qreal y;
		const char *label;
		bool frozen;
		int uuid;
	};

	QString elementXml(const Instance &instance)
	{
		QString information = QStringLiteral(
					      "<elementInformation show=\"1\" name=\"label\">%1"
					      "</elementInformation>")
				      .arg(QLatin1String(instance.label));
		if (instance.frozen)
		{
			// What the program writes when a tag is typed by hand. Read back
			// here through the very key the renumbering looks at.
			information += QStringLiteral(
					       "<elementInformation show=\"1\" name=\"auto_num_locked\">true"
					       "</elementInformation>");
		}

		return QStringLiteral(
			       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/%3\""
			       " uuid=\"{beef0000-0000-4000-8000-00000000000%4}\">"
			       "<terminals/><inputs/>"
			       "<elementInformations>%5</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
		       .arg(QString::number(instance.x), QString::number(instance.y),
			    QLatin1String(instance.symbol), QString::number(instance.uuid),
			    information);
	}

	QString definitionXml(const QString &file_name, const QString &english_name,
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
	}

	QString projectXml(const QList<Instance> &instances)
	{
		QString drawn;
		for (const Instance &instance : instances) {
			drawn += elementXml(instance);
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%3</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definitionXml(QStringLiteral("contactor.elmt"),
					  QStringLiteral("Contactor"),
					  QStringLiteral("simple")),
			    definitionXml(QStringLiteral("terminalblock.elmt"),
					  QStringLiteral("Terminal block"),
					  QStringLiteral("terminal")),
			    drawn);
	}

	/**
		The sheet of the first case.

		The tags start out of order on purpose - K3 above K1, Q9 alone in its
		root - so that "renumbered in reading order" cannot be confused with
		"left as it was", and so that a renumbering that simply kept the
		order of the file would be caught.

		Rows are 100 units apart and columns 50 or more: nothing here sits
		near the tolerance, which is the subject of the second case and not
		of this one.
	*/
	QString sheetXml()
	{
		const QList<Instance> instances = {
			{"contactor.elmt", 100, 100, "K3", false, 1},
			{"contactor.elmt", 300, 100, "K1", false, 2},
			{"contactor.elmt", 500, 100, "Q9", false, 3},
			{"contactor.elmt", 200, 200, "K2", false, 4},
			{"contactor.elmt", 150, 400, "K7", true, 5},
			{"terminalblock.elmt", 700, 100, "X1", false, 6}};
		return projectXml(instances);
	}

	/**
		Two components, the left one @a drop units lower than the right one.
		Nothing else on the sheet, so the answer depends on that number
		alone.
	*/
	QString pairXml(qreal drop)
	{
		const QList<Instance> instances = {
			{"contactor.elmt", 100, 100 + drop, "K5", false, 1},
			{"contactor.elmt", 300, 100, "K6", false, 2}};
		return projectXml(instances);
	}

	/**
		Two more components drawn on @a sheet after it was opened: the control
		circuit somebody adds to a panel that is already on the bench.

		They arrive through Diagram::fromXml, which is the path a paste takes,
		rather than being built here by hand. What the case has to be able to
		say is that the renumbering meets them exactly as it meets the ones
		that came out of the file - and an instance assembled in the test
		would be a different object from the one a draughtsman would have
		dropped, so the answer would be about the test.

		Their uuid is written down rather than generated, so that "this one is
		not in the photograph" is a fact of the fixture and not of the run.

		@param label the tag they carry when drawn, which is what decides
		their tag root
	*/
	bool addControlCircuit(Diagram *sheet, const char *label)
	{
		if (!sheet) {
			return false;
		}

		const QList<Instance> added = {
			{"contactor.elmt", 100, 300, label, false, 7},
			{"contactor.elmt", 300, 300, label, false, 8}};

		QString drawn;
		for (const Instance &instance : added) {
			drawn += elementXml(instance);
		}

		QDomDocument fragment;
		if (!fragment.setContent(QStringLiteral(
						 "<diagram><elements>%1</elements>"
						 "<inputs/><conductors/></diagram>")
					 .arg(drawn))) {
			return false;
		}

		QDomElement root = fragment.documentElement();
		// false: the border and the title block of the sheet are not to be
		// read again from a fragment that carries none.
		return sheet->fromXml(root, QPointF(), false);
	}

	Element *elementAt(Diagram *sheet, qreal x, qreal y)
	{
		if (!sheet) {
			return nullptr;
		}
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->scenePos() == QPointF(x, y)) {
				return element;
			}
		}
		return nullptr;
	}

	QString labelOf(Element *element)
	{
		return element ? element->elementInformations()
				 .value(QStringLiteral("label")).toString()
			       : QString();
	}

	/// The preview table, one line per component, in the order it is shown.
	QStringList previewTable(const RenumberPlan &plan)
	{
		QStringList lines;
		for (const RenumberEntry &entry : plan.entries)
		{
			lines << QStringLiteral("%1 -> %2%3")
				 .arg(entry.from, entry.to,
				      entry.frozen ? QStringLiteral(" (frozen)") : QString());
		}
		return lines;
	}

	/// Which line of the drawing is not back where it was, said out loud.
	QString firstLabelThatMoved(const QStringList &expected, const QStringList &found)
	{
		const int shared = qMin(expected.count(), found.count());
		for (int index = 0 ; index < shared ; ++index)
		{
			if (expected.at(index) != found.at(index)) {
				return QStringLiteral("component %1: expected \"%2\", found \"%3\"")
				       .arg(QString::number(index), expected.at(index),
					    found.at(index));
			}
		}
		if (expected.count() != found.count()) {
			return QStringLiteral("count: %1 -> %2")
			       .arg(QString::number(expected.count()),
				    QString::number(found.count()));
		}
		return QString();
	}

	/// The tags of @a elements, in the order they were handed in.
	QStringList labelsOf(const QList<Element *> &elements)
	{
		QStringList labels;
		for (Element *element : elements) {
			labels << labelOf(element);
		}
		return labels;
	}

	/// A renaming of one component, pushed on its own.
	class LabelOnlyCommand : public QUndoCommand
	{
		public:
			LabelOnlyCommand(Element *element, const QString &label) :
				QUndoCommand(QStringLiteral("label only")),
				m_element(element),
				m_before(element->elementInformations()),
				m_after(element->elementInformations())
			{
				m_after.addValue(QStringLiteral("label"), label);
			}

			void undo() override {m_element->setElementInformations(m_before);}
			void redo() override {m_element->setElementInformations(m_after);}

		private:
			Element *m_element = nullptr;
			DiagramContext m_before;
			DiagramContext m_after;
	};

	NumberingFormat sequentialFormat()
	{
		return NumberingFormat(QStringLiteral("sequential"),
				       QStringLiteral("%{root}%{n}"));
	}
}

TEST_CASE("F1 F.1 (fila) — renumerar o projeto vendo antes, e desfazer de uma vez",
	  "[uibench][renumber]")
{
	Catalog catalog;
	QString catalog_error;
	const bool catalog_open = catalog.openInMemory(&catalog_error);
	INFO(catalog_error.toStdString());
	REQUIRE(catalog_open);

	UiBench::ScratchProject scratch(sheetXml(), QStringLiteral("renumber.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 6);

	// Named by where they are drawn, because their tags are what changes.
	Element *top_left = elementAt(sheet, 100, 100);
	Element *top_middle = elementAt(sheet, 300, 100);
	Element *top_right = elementAt(sheet, 500, 100);
	Element *second_row = elementAt(sheet, 200, 200);
	Element *typed_by_hand = elementAt(sheet, 150, 400);
	REQUIRE(top_left != nullptr);
	REQUIRE(top_middle != nullptr);
	REQUIRE(top_right != nullptr);
	REQUIRE(second_row != nullptr);
	REQUIRE(typed_by_hand != nullptr);

	const QList<Element *> drawn_order = {top_left, top_middle, top_right,
					      second_row, typed_by_hand};
	const QStringList before = {QStringLiteral("K3"), QStringLiteral("K1"),
				    QStringLiteral("Q9"), QStringLiteral("K2"),
				    QStringLiteral("K7")};
	REQUIRE(labelsOf(drawn_order) == before);

	const QList<Element *> components = ProjectRenumberer::components(scratch.project());

	SECTION("o que entra na renumeração vem da folha, e o borne fica de fora")
	{
		REQUIRE(components.count() == 5);
		REQUIRE_FALSE(components.contains(elementAt(sheet, 700, 100)));
	}

	SECTION("a posição pela qual se ordena é a que a folha desenha")
	{
		const QList<RenumberInput> inputs =
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat());
		REQUIRE(inputs.count() == 5);

		// One input read back, to say that nothing in between invents a
		// coordinate: what the file drew at 100;100 arrives at 100;100.
		bool found = false;
		for (const RenumberInput &input : inputs)
		{
			if (input.uuid != top_left->uuid().toString()) {
				continue;
			}
			found = true;
			REQUIRE(input.position == QPointF(100, 100));
			REQUIRE(input.current == QStringLiteral("K3"));
			REQUIRE(input.root == QStringLiteral("K"));
			REQUIRE(input.folio_index == 0);
			REQUIRE_FALSE(input.frozen);
		}
		REQUIRE(found);
	}

	SECTION("a tabela de → para tem uma linha por componente, na ordem da leitura")
	{
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		const QStringList expected = {QStringLiteral("K3 -> K1"),
					      QStringLiteral("K1 -> K2"),
					      QStringLiteral("Q9 -> Q1"),
					      QStringLiteral("K2 -> K3"),
					      QStringLiteral("K7 -> K7 (frozen)")};
		INFO(firstLabelThatMoved(expected, previewTable(plan)).toStdString());
		REQUIRE(previewTable(plan) == expected);
		REQUIRE(plan.changeCount() == 4);
		REQUIRE(plan.frozenCount() == 1);
		REQUIRE_FALSE(plan.hasDuplicates());
	}

	SECTION("o congelado aparece na tabela, marcado como pulado")
	{
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		bool found = false;
		for (const RenumberEntry &entry : plan.entries)
		{
			if (entry.uuid != typed_by_hand->uuid().toString()) {
				REQUIRE_FALSE(entry.frozen);
				continue;
			}
			found = true;
			REQUIRE(entry.frozen);
			REQUIRE_FALSE(entry.changed);
			REQUIRE(entry.to == QStringLiteral("K7"));
		}
		REQUIRE(found);
	}

	SECTION("controle negativo — destravado, o mesmo componente entra na numeração")
	{
		// Without this, the section above would pass on a program that
		// marked everything frozen and renumbered nothing.
		DiagramContext unlocked = typed_by_hand->elementInformations();
		unlocked.addValue(QStringLiteral("auto_num_locked"), QStringLiteral("false"));
		typed_by_hand->setElementInformations(unlocked);
		REQUIRE_FALSE(ProjectRenumberer::isFrozen(typed_by_hand));

		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		REQUIRE(plan.frozenCount() == 0);
		REQUIRE(plan.changeCount() == 5);
		REQUIRE(plan.labelFor(typed_by_hand->uuid().toString())
			== QStringLiteral("K4"));
	}

	SECTION("aplicar escreve na folha o que a tabela mostrava")
	{
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		REQUIRE(ProjectRenumberer::applyPlan(components, plan) == 4);

		const QStringList after = {QStringLiteral("K1"), QStringLiteral("K2"),
					   QStringLiteral("Q1"), QStringLiteral("K3"),
					   QStringLiteral("K7")};
		INFO(firstLabelThatMoved(after, labelsOf(drawn_order)).toStdString());
		REQUIRE(labelsOf(drawn_order) == after);
	}

	SECTION("um unico Ctrl+Z devolve tudo")
	{
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		const int steps_before = sheet->undoStack().index();
		REQUIRE(ProjectRenumberer::applyPlan(components, plan) == 4);
		REQUIRE(sheet->undoStack().index() == steps_before + 1);

		const QString entry = sheet->undoStack().text(sheet->undoStack().index() - 1);
		INFO(entry.toStdString());
		REQUIRE(entry == QStringLiteral("Renuméroter 4 composant(s)"));

		sheet->undoStack().undo();

		REQUIRE(sheet->undoStack().index() == steps_before);
		INFO(firstLabelThatMoved(before, labelsOf(drawn_order)).toStdString());
		REQUIRE(labelsOf(drawn_order) == before);
	}

	SECTION("controle negativo — em dois comandos, um Ctrl+Z deixa a folha pela metade")
	{
		/*
			The check above only means something if it can tell one command
			from two. Here the renumbering arrives in two pushes, which is
			what the roteiro asks to be written down when it happens: how
			many Ctrl+Z it took, and what was left half done.
		*/
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		const int steps_before = sheet->undoStack().index();
		REQUIRE(ProjectRenumberer::applyPlan({top_left, top_middle}, plan) == 2);
		REQUIRE(ProjectRenumberer::applyPlan({top_right, second_row}, plan) == 2);
		REQUIRE(sheet->undoStack().index() == steps_before + 2);

		sheet->undoStack().undo();

		INFO(firstLabelThatMoved(before, labelsOf(drawn_order)).toStdString());
		REQUIRE_FALSE(labelsOf(drawn_order) == before);
		REQUIRE_FALSE(firstLabelThatMoved(before, labelsOf(drawn_order)).isEmpty());

		// And the second one finishes what the first left.
		sheet->undoStack().undo();
		REQUIRE(labelsOf(drawn_order) == before);
	}

	SECTION("controle negativo — uma alteração por fora e o Ctrl+Z não devolve a folha")
	{
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);
		REQUIRE(ProjectRenumberer::applyPlan(components, plan) == 4);
		sheet->undoStack().push(new LabelOnlyCommand(top_right, QStringLiteral("Q42")));

		sheet->undoStack().undo();

		const QString culprit = firstLabelThatMoved(before, labelsOf(drawn_order));
		INFO(culprit.toStdString());
		REQUIRE_FALSE(labelsOf(drawn_order) == before);
		REQUIRE(culprit == QStringLiteral("component 0: expected \"K3\", found \"K1\""));
	}

	SECTION("as duas orientações dão ordens diferentes, e as duas se leem")
	{
		const QList<RenumberInput> inputs =
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat());

		const QStringList rows_first = {QStringLiteral("K3 -> K1"),
						QStringLiteral("K1 -> K2"),
						QStringLiteral("Q9 -> Q1"),
						QStringLiteral("K2 -> K3"),
						QStringLiteral("K7 -> K7 (frozen)")};
		const QStringList columns_first = {QStringLiteral("K3 -> K1"),
						   QStringLiteral("K7 -> K7 (frozen)"),
						   QStringLiteral("K2 -> K2"),
						   QStringLiteral("K1 -> K3"),
						   QStringLiteral("Q9 -> Q1")};

		REQUIRE(previewTable(Renumberer::plan(inputs, false)) == rows_first);
		REQUIRE(previewTable(Renumberer::plan(inputs, true)) == columns_first);
		REQUIRE_FALSE(rows_first == columns_first);
	}
}

TEST_CASE("F1 F.1 (fila) — a tolerância de 10 unidades da ordem de leitura, dos dois lados",
	  "[uibench][renumber]")
{
	/*
		The number the roteiro states: a symbol dropped a few units above its
		neighbour is on the same row, and disagreeing by more than ten units
		is a defect. Ten is where that stops being true, so both sides of ten
		are written here.

		Each side is its own project, drawn from its own file, because the
		positions have to be exactly what is asked: moving an item through
		Element::setPos would snap it to the grid and quietly turn 111 into
		110 - which is also why a person cannot check this boundary with the
		mouse.
	*/
	Catalog catalog;
	QString catalog_error;
	const bool catalog_open = catalog.openInMemory(&catalog_error);
	INFO(catalog_error.toStdString());
	REQUIRE(catalog_open);

	SECTION("dez unidades abaixo ainda e a mesma linha: o da esquerda vem primeiro")
	{
		UiBench::ScratchProject scratch(pairXml(ordering_tolerance),
						QStringLiteral("inside.qet"));
		INFO(scratch.error().toStdString());
		REQUIRE(scratch.isOpen());

		Diagram *sheet = scratch.diagram(0);
		REQUIRE(sheet != nullptr);
		Element *left = elementAt(sheet, 100, 110);
		Element *right = elementAt(sheet, 300, 100);
		REQUIRE(left != nullptr);
		REQUIRE(right != nullptr);

		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(
				catalog,
				ProjectRenumberer::components(scratch.project()),
				sequentialFormat()),
			false);

		REQUIRE(plan.labelFor(left->uuid().toString()) == QStringLiteral("K1"));
		REQUIRE(plan.labelFor(right->uuid().toString()) == QStringLiteral("K2"));
	}

	SECTION("onze unidades abaixo ja e outra linha: o de cima vem primeiro")
	{
		UiBench::ScratchProject scratch(pairXml(ordering_tolerance + 1.0),
						QStringLiteral("outside.qet"));
		INFO(scratch.error().toStdString());
		REQUIRE(scratch.isOpen());

		Diagram *sheet = scratch.diagram(0);
		REQUIRE(sheet != nullptr);
		Element *left = elementAt(sheet, 100, 111);
		Element *right = elementAt(sheet, 300, 100);
		REQUIRE(left != nullptr);
		REQUIRE(right != nullptr);

		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(
				catalog,
				ProjectRenumberer::components(scratch.project()),
				sequentialFormat()),
			false);

		REQUIRE(plan.labelFor(right->uuid().toString()) == QStringLiteral("K1"));
		REQUIRE(plan.labelFor(left->uuid().toString()) == QStringLiteral("K2"));
	}
}

TEST_CASE("CU-29.1 — circuito novo em quadro montado: só o que veio depois é numerado",
	  "[uibench][renumber][assembly]")
{
	/*
		The case of T29 that costs the most when it is missing: a panel is on
		the bench, its labels printed and stuck on, and a control circuit has
		to be added. Renumbering today would remake the tags of the four
		hundred parts already screwed down, so nobody runs it - and the change
		goes back to being done by hand.

		What is proved here is the whole of the case as the roteiro states it,
		with one half left out on purpose: that the project is a real one, and
		that the shop finds the drawing it expects, is for a person to look at.
		The rest is here, on components read off a sheet loaded from a file
		and drawn on it afterwards through the very path a paste takes.

		The new circuit is tagged KA, a root of its own. The collision that
		arises when it shares the root of a part already on the rail is a
		different subject, measured in the case below, and mixing the two
		would make this one fail for a reason that is not the freezing.
	*/
	Catalog catalog;
	QString catalog_error;
	const bool catalog_open = catalog.openInMemory(&catalog_error);
	INFO(catalog_error.toStdString());
	REQUIRE(catalog_open);

	UiBench::ScratchProject scratch(sheetXml(), QStringLiteral("assembled.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 6);

	Element *top_left = elementAt(sheet, 100, 100);
	Element *top_middle = elementAt(sheet, 300, 100);
	Element *top_right = elementAt(sheet, 500, 100);
	Element *second_row = elementAt(sheet, 200, 200);
	Element *typed_by_hand = elementAt(sheet, 150, 400);
	REQUIRE(top_left != nullptr);
	REQUIRE(top_middle != nullptr);
	REQUIRE(top_right != nullptr);
	REQUIRE(second_row != nullptr);
	REQUIRE(typed_by_hand != nullptr);

	// What is screwed to the rail, in the order it is drawn. The tags of
	// these five are the whole subject: not one of them may move.
	const QList<Element *> on_the_rail = {top_left, top_middle, top_right,
					      second_row, typed_by_hand};
	const QStringList before = {QStringLiteral("K3"), QStringLiteral("K1"),
				    QStringLiteral("Q9"), QStringLiteral("K2"),
				    QStringLiteral("K7")};
	REQUIRE(labelsOf(on_the_rail) == before);

	SECTION("controle negativo — sem a marcação, a renumeração mexe em tudo")
	{
		/*
			Without this the case below would pass on a program that had
			simply stopped renumbering, and "nothing existing changed" would
			be the report of a broken automation rather than of a working
			freeze.
		*/
		REQUIRE(addControlCircuit(sheet, "KA"));
		const QList<Element *> components =
			ProjectRenumberer::components(scratch.project());
		REQUIRE(components.count() == 7);

		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		// Seven lines: six changed and the one tag somebody typed by hand.
		REQUIRE(plan.entries.count() == 7);
		REQUIRE(plan.changeCount() == 6);
		REQUIRE(plan.frozenCount() == 1);

		REQUIRE(ProjectRenumberer::applyPlan(components, plan) == 6);
		INFO(firstLabelThatMoved(before, labelsOf(on_the_rail)).toStdString());
		REQUIRE_FALSE(labelsOf(on_the_rail) == before);
	}

	SECTION("marcado como montado, só os dois componentes novos são numerados")
	{
		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));

		// The photograph, counted both ways: five components in it, and five
		// items in it altogether. The sheet draws no wire, so the two
		// numbers agree here - and saying only one of them would hide the
		// day they stop agreeing. The terminal block is not a component the
		// renumbering tags, and is in neither count.
		const AssemblyState state = scratch->assemblyState();
		REQUIRE(state.components.size() == 5);
		REQUIRE(state.frozenCount() == 5);

		REQUIRE(addControlCircuit(sheet, "KA"));
		const QList<Element *> components =
			ProjectRenumberer::components(scratch.project());
		REQUIRE(components.count() == 7);

		Element *new_left = elementAt(sheet, 100, 300);
		Element *new_right = elementAt(sheet, 300, 300);
		REQUIRE(new_left != nullptr);
		REQUIRE(new_right != nullptr);
		REQUIRE_FALSE(state.holdsComponent(new_left->uuid().toString()));

		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		// Groups and total, both said: five left alone, two renumbered, and
		// seven lines in the table. The sum is asserted as well, so that an
		// entry which is neither - passed over without being reported - has
		// nowhere to hide inside a plausible pair of numbers.
		REQUIRE(plan.frozenCount() == 5);
		REQUIRE(plan.changeCount() == 2);
		REQUIRE(plan.skippedCount() == 0);
		REQUIRE(plan.entries.count() == 7);
		REQUIRE(plan.entries.count() == plan.frozenCount() + plan.changeCount());
		REQUIRE_FALSE(plan.hasDuplicates());

		REQUIRE(ProjectRenumberer::applyPlan(components, plan) == 2);

		// The whole point of the task, in one line.
		INFO(firstLabelThatMoved(before, labelsOf(on_the_rail)).toStdString());
		REQUIRE(labelsOf(on_the_rail) == before);

		REQUIRE(labelOf(new_left) == QStringLiteral("KA1"));
		REQUIRE(labelOf(new_right) == QStringLiteral("KA2"));
	}

	SECTION("o travamento é derivado da fotografia, e não gravado no componente")
	{
		/*
			Decision P85, seen from the renumbering side rather than from the
			marking side. A component of the panel answers "frozen" while
			carrying no lock of its own, and that is what lets the marking be
			taken back later without freeing the one tag the draughtsman
			locked by hand.
		*/
		const QStringList locked_before =
			UiBench::information(sheet, QStringLiteral("auto_num_locked"));
		REQUIRE(locked_before.count(QStringLiteral("true")) == 1);
		REQUIRE_FALSE(ProjectRenumberer::isFrozen(top_left));

		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));

		REQUIRE(ProjectRenumberer::isFrozen(top_left));
		REQUIRE(top_left->elementInformations()
			.value(QStringLiteral("auto_num_locked")).toString().isEmpty());
		REQUIRE(UiBench::information(sheet, QStringLiteral("auto_num_locked"))
			== locked_before);
	}
}

TEST_CASE("T29 — desmarcar devolve a folha à automação, e o contador não reserva o que congelou",
	  "[uibench][renumber][assembly]")
{
	/*
		Two things measured on the same fixture, because both of them are
		about what the freezing does *not* do.

		The first is the half of taking the marking back that this step can
		answer: the freezing is derived, so unmarking frees what the machine
		froze and leaves alone what somebody locked by hand. The window that
		offers the unmarking is another step, and the case number belongs to
		it.

		The second is a gap, written down here rather than left to be found on
		a bench. A frozen line of the plan does not consume a number of its
		counter, so a component drawn after the marking, under the root of a
		part that is already screwed down, is handed a number that part is
		wearing. The plan reports it as a duplicate instead of writing it -
		which is the difference between an awkward answer and a wrong one -
		but reserving the numbers of the photograph is not part of this step,
		and the day it is, this case is what says what changed.
	*/
	Catalog catalog;
	QString catalog_error;
	const bool catalog_open = catalog.openInMemory(&catalog_error);
	INFO(catalog_error.toStdString());
	REQUIRE(catalog_open);

	UiBench::ScratchProject scratch(sheetXml(), QStringLiteral("assembled.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);

	Element *top_left = elementAt(sheet, 100, 100);
	Element *typed_by_hand = elementAt(sheet, 150, 400);
	REQUIRE(top_left != nullptr);
	REQUIRE(typed_by_hand != nullptr);

	SECTION("desmarcar libera o que a fotografia congelou e não o que foi travado à mão")
	{
		const int steps_before = scratch->undoStack()->index();
		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));
		REQUIRE(addControlCircuit(sheet, "KA"));
		REQUIRE(ProjectRenumberer::isFrozen(top_left));

		// The circuit was drawn without going through the stack, so the one
		// step this takes back is the marking and nothing else.
		REQUIRE(scratch->undoStack()->index() == steps_before + 1);
		scratch->undoStack()->undo();
		REQUIRE(scratch->undoStack()->index() == steps_before);

		REQUIRE_FALSE(scratch->assemblyState().isFrozen());
		REQUIRE_FALSE(ProjectRenumberer::isFrozen(top_left));
		REQUIRE(ProjectRenumberer::isFrozen(typed_by_hand));

		const QList<Element *> components =
			ProjectRenumberer::components(scratch.project());
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		REQUIRE(plan.entries.count() == 7);
		REQUIRE(plan.frozenCount() == 1);
		REQUIRE(plan.changeCount() == 6);
	}

	SECTION("componente novo sob a raiz de um congelado recebe número repetido, e o plano acusa")
	{
		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));
		REQUIRE(addControlCircuit(sheet, "K"));

		const QList<Element *> components =
			ProjectRenumberer::components(scratch.project());
		const RenumberPlan plan = Renumberer::plan(
			ProjectRenumberer::inputsFor(catalog, components, sequentialFormat()),
			false);

		REQUIRE(plan.frozenCount() == 5);
		REQUIRE(plan.changeCount() == 2);
		REQUIRE(plan.entries.count() == 7);

		// Measured, not supposed: the counter of the K bucket starts at one
		// because the five frozen lines never touched it, so the two new
		// contactors are offered K1 and K2 - tags that are printed and stuck
		// on two parts of the panel.
		const QStringList colliding = {QStringLiteral("K1"), QStringLiteral("K2")};
		REQUIRE(plan.duplicates() == colliding);
		REQUIRE(plan.hasDuplicates());
	}
}
