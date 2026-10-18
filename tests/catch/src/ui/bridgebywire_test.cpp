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
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/TerminalStrip/physicalterminal.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/terminalstripbridge.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripeditor.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripmodel.h"

#include <catch2/catch.hpp>

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QPushButton>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QUndoStack>
#include <QVector>

#include <iterator>

/*
	The button of the terminal strip manager that bridges, in one command,
	every run of adjacent terminals carrying the same wire number.

	The rule behind it - which runs, and which terminals never - is proved
	without a project in terminalstripstructure_test.cpp of C_unittests,
	on structures written by hand. What that suite cannot see is the
	reading: whether the wire number the rule is given is the one drawn on
	the folio, whether the level it is given is the level the terminal sits
	on, and whether the answer, turned back into terminals, lands on the
	terminals it is about. A rule that is right on hand written input still
	bridges the wrong terminals when it is fed the wrong ones, so nothing
	here is built by hand: the wires are conductors of a folio, the strip is
	made of the terminal elements drawn on it, and the bridges are made by
	clicking the button of the editor the program shows.

	Every negative control below has a reason to be there, and it is a
	defect that would pass without it: the three fuses on one wire are
	bridged the day the type is not read, the two unnumbered terminals the
	day the placeholder text of a new conductor is read as a number, and the
	two terminals carrying the same pair of numbers the day the reader takes
	the first conductor instead of refusing to choose.

	Labelled T33 and not CU-33.2. The case of use inserts fuse terminals and
	renumbers the project, then bridges; here the numbers are written on the
	conductors by the fixture, in the state a renumbering leaves them -
	different numbers on the two sides of each fuse - and the renumbering
	itself is not run. What is proved is the half of the case the button
	owns.
*/

namespace
{
	/**
		One terminal of the bench, in the order it is put on the strip,
		which is also its label: the label is the position plus one, so
		that a failure naming a label names a position too.

		Each wire is one conductor, docked on the west side for the first
		and on the east side for the second; nullptr means nothing docked
		on that side.
	*/
	struct BenchTerminal
	{
		const char *label;
		ElementData::TerminalType type;
		const char *west_wire;
		const char *east_wire;
	};

	const BenchTerminal bench_terminals[] = {
			//Three through terminals on L1, the last with a wire on each
			//side carrying the same number: one bridge of three.
		{"1",  ElementData::TTGeneric,   "L1", nullptr},
		{"2",  ElementData::TTGeneric,   "L1", nullptr},
		{"3",  ElementData::TTGeneric,   "L1", "L1"},
			//Two terminals on L2 with one on L3 between them: a comb
			//cannot step over a terminal, so no bridge.
		{"4",  ElementData::TTGeneric,   "L2", nullptr},
		{"5",  ElementData::TTGeneric,   "L3", nullptr},
		{"6",  ElementData::TTGeneric,   "L2", nullptr},
			//Three fuses whose wires agree: only the type keeps them
			//apart, and this is the run that proves the type is read.
		{"7",  ElementData::TTFuse,      "N",  nullptr},
		{"8",  ElementData::TTFuse,      "N",  nullptr},
		{"9",  ElementData::TTFuse,      "N",  nullptr},
			//Five terminals on 24 with a sectional in the middle: two
			//bridges, one on each side, and not one bridge of four.
		{"10", ElementData::TTGeneric,   "24", nullptr},
		{"11", ElementData::TTGeneric,   "24", nullptr},
		{"12", ElementData::TTSectional, "24", nullptr},
		{"13", ElementData::TTGeneric,   "24", nullptr},
		{"14", ElementData::TTGeneric,   "24", nullptr},
			//Three fuses as a renumbered project leaves them: one feed in,
			//a different number out of each.
		{"15", ElementData::TTFuse,      "P",  "31"},
		{"16", ElementData::TTFuse,      "P",  "32"},
		{"17", ElementData::TTFuse,      "P",  "33"},
			//Two terminals whose conductors nobody numbered: the text a
			//new conductor is given is not a wire number.
		{"18", ElementData::TTGeneric,   "_",  nullptr},
		{"19", ElementData::TTGeneric,   "_",  nullptr},
			//Two through terminals each carrying the same two numbers: the
			//drawing disagrees with itself, and the reading refuses to
			//pick one.
		{"20", ElementData::TTGeneric,   "5",  "6"},
		{"21", ElementData::TTGeneric,   "5",  "6"},
			//Two diodes on one wire.
		{"22", ElementData::TTDiode,     "D",  nullptr},
		{"23", ElementData::TTDiode,     "D",  nullptr}};

	const int bench_terminal_count = static_cast<int>(std::size(bench_terminals));

	/// The bridges the bench expects, by the labels they hold.
	QStringList expectedBridges()
	{
		return QStringList{QStringLiteral("1+2+3"),
				   QStringLiteral("10+11"),
				   QStringLiteral("13+14")};
	}

	/// The conductors the bench draws, counted from the table.
	int benchConductorCount()
	{
		int count = 0;
		for (const BenchTerminal &terminal : bench_terminals)
		{
			if (terminal.west_wire) {
				++count;
			}
			if (terminal.east_wire) {
				++count;
			}
		}
		return count;
	}

	/**
		A project of one folio: the terminals of the table, and one box
		with one docking point per conductor, so that each conductor has
		an end of its own and the number it carries is the one the table
		gives it.

		The docking coordinates of an instance are not those of the
		definition: Terminal offsets them by Terminal::terminalSize towards
		the middle of the element, as terminalstripcable_test.cpp explains,
		and the arithmetic is copied from there rather than the numbers.
	*/
	QString fixtureXml()
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString instances;
		QString conductors;
		QString box_terminals;
		QString box_definition_terminals;
		int box_point = 0;

		const auto wire = [&](const char *number, int terminal_id)
		{
			const int y = -150 + box_point * 10;
			const int box_id = 1000 + box_point;
			++box_point;

			box_terminals += QStringLiteral(
						 "<terminal x=\"%1\" y=\"%2\""
						 " orientation=\"1\" id=\"%3\"/>")
					 .arg(east_dock)
					 .arg(y)
					 .arg(box_id);

			box_definition_terminals += QStringLiteral(
							    "<terminal x=\"10\" y=\"%1\""
							    " orientation=\"e\" name=\"%2\"/>")
						    .arg(y)
						    .arg(box_point);

			conductors += QStringLiteral(
					      "<conductor terminal1=\"%1\" terminal2=\"%2\""
					      " num=\"%3\" displaytext=\"1\" type=\"multi\""
					      " condsize=\"1\"/>")
				      .arg(box_id)
				      .arg(terminal_id)
				      .arg(QLatin1String(number));
		};

			//Terminal element i docks its east side on id 2i and its west
			//side on id 2i+1.
		for (int index = 0 ; index < bench_terminal_count ; ++index)
		{
			const BenchTerminal &terminal = bench_terminals[index];

			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"300\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/terminal.elmt\""
					     " uuid=\"{b81d6e00-0000-4000-8000-%2}\">"
					     "<terminals>"
					     "<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
					     "<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%7"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(200 + index * 40)
				     .arg(index, 12, 10, QLatin1Char('0'))
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(QLatin1String(terminal.label));

			if (terminal.west_wire) {
				wire(terminal.west_wire, index * 2 + 1);
			}
			if (terminal.east_wire) {
				wire(terminal.east_wire, index * 2);
			}
		}

		instances += QStringLiteral(
				     "<element x=\"100\" y=\"300\" z=\"10\" prefix=\"\""
				     " freezeLabel=\"false\" orientation=\"0\""
				     " type=\"embed://bench/box.elmt\""
				     " uuid=\"{b0c50000-0000-4000-8000-000000000000}\">"
				     "<terminals>%1</terminals>"
				     "<inputs/>"
				     "<elementInformations>"
				     "<elementInformation show=\"1\" name=\"label\">K1"
				     "</elementInformation>"
				     "</elementInformations>"
				     "<dynamic_texts/><texts_groups/>"
				     "</element>")
			     .arg(box_terminals);

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"terminal.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"terminal\">"
			       "<names><name lang=\"en\">Terminal</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"40\" height=\"320\""
			       " hotspot_x=\"20\" hotspot_y=\"160\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-155\" width=\"16\" height=\"310\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "%1"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " folio=\"1\""
			       " cols=\"25\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors>%3</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(box_definition_terminals, instances, conductors);
	}

	/// The terminal element of @a diagram labelled @a label, nullptr when none.
	TerminalElement *terminalLabelled(Diagram *diagram, const QString &label)
	{
		if (!diagram) {
			return nullptr;
		}

		const auto elements = diagram->elements();
		for (const auto element : elements)
		{
			if (element->elementData().m_type != ElementData::Terminal) {
				continue;
			}

			const auto terminal_element = static_cast<TerminalElement *>(element);
			const auto real_t = terminal_element->realTerminal();
			if (!real_t.isNull() && real_t->label() == label) {
				return terminal_element;
			}
		}

		return nullptr;
	}

	/// The real terminal labelled @a label, null when none.
	QSharedPointer<RealTerminal> realLabelled(Diagram *diagram, const QString &label)
	{
		const auto terminal_element = terminalLabelled(diagram, label);
		return terminal_element ? terminal_element->realTerminal()
					: QSharedPointer<RealTerminal>();
	}

	/**
		The strip of the bench: every terminal of the table, in the order of
		the table, each with its type. Null when a terminal of the table is
		missing from the folio, so that the case stops on the fixture and
		not on a bridge.
	*/
	TerminalStrip *benchStrip(UiBench::ScratchProject &bench)
	{
		Diagram *sheet = bench.diagram(0);
		TerminalStrip *strip = bench->newTerminalStrip(QString(),
							       QString(),
							       QStringLiteral("X1"));
		if (!sheet || !strip) {
			return nullptr;
		}

		for (const BenchTerminal &terminal : bench_terminals)
		{
			TerminalElement *element = terminalLabelled(sheet, QLatin1String(terminal.label));
			if (!element || !strip->addTerminal(element)) {
				return nullptr;
			}

			if (terminal.type != ElementData::TTGeneric)
			{
					//What the Apply button of the editor writes for a
					//type chosen in the table.
				ElementData data = element->elementData();
				data.setTerminalType(terminal.type);
				element->setElementData(data);
			}
		}

		return strip;
	}

	/**
		Every bridge of @a strip, each written as the labels it holds joined
		by "+", in the order they are met walking the strip - positions
		first, levels inside each.

		A list and not a count, so that a failure says which terminals were
		combed; and grouped by bridge, so that two runs wrongly joined into
		one bridge read differently from two bridges.
	*/
	QStringList bridgedRuns(TerminalStrip *strip)
	{
		QVector<QSharedPointer<TerminalStripBridge>> order;
		QHash<TerminalStripBridge *, QStringList> labels;

		const auto physicals = strip->physicalTerminal();
		for (const auto &physical : physicals)
		{
			const auto levels = physical->realTerminals();
			for (const auto &real_t : levels)
			{
				const auto bridge_ = strip->isBridged(real_t);
				if (bridge_.isNull()) {
					continue;
				}

				if (!labels.contains(bridge_.data())) {
					order.append(bridge_);
				}
				labels[bridge_.data()] << real_t->label();
			}
		}

		QStringList runs;
		for (const auto &bridge_ : order) {
			runs << labels.value(bridge_.data()).join(QLatin1Char('+'));
		}
		return runs;
	}

	bool isBridged(TerminalStrip *strip, Diagram *sheet, const char *label)
	{
		const auto real_t = realLabelled(sheet, QLatin1String(label));
		return !real_t.isNull() && !strip->isBridged(real_t).isNull();
	}

	/// The button under test, found the way the editor names it.
	QPushButton *bridgeByWireButton(TerminalStripEditor &editor)
	{
		return editor.findChild<QPushButton *>(QStringLiteral("m_bridge_same_wire_pb"));
	}

	QString commandText()
	{
		return TerminalStripEditor::tr("Bridge terminals with the same wire number");
	}
}

TEST_CASE("T33 — pontear iguais ponteia cada fila de bornes vizinhos de mesmo número de fio, num só comando",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("bridgebywire.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->conductors().size() == benchConductorCount());

	TerminalStrip *strip = benchStrip(bench);
	REQUIRE(strip != nullptr);
	REQUIRE(strip->physicalTerminalCount() == bench_terminal_count);
	REQUIRE(bridgedRuns(strip).isEmpty());

		//The reading the bench rests on, checked before the button: if the
		//folio did not give these, every case below would be measuring the
		//fixture.
	REQUIRE(realLabelled(sheet, QStringLiteral("3"))->conductors()
		== QStringList({QStringLiteral("L1"), QStringLiteral("L1")}));
	REQUIRE(realLabelled(sheet, QStringLiteral("7"))->type() == ElementData::TTFuse);
	REQUIRE(realLabelled(sheet, QStringLiteral("12"))->type() == ElementData::TTSectional);
	REQUIRE(realLabelled(sheet, QStringLiteral("18"))->conductors()
		== QStringList{QStringLiteral("_")});

	TerminalStripEditor editor(bench.project());
	editor.setCurrentStrip(strip);

	QPushButton *button = bridgeByWireButton(editor);
	REQUIRE(button != nullptr);
	REQUIRE(button->isEnabled());

	QUndoStack *stack = bench->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();

	QSignalSpy messages(&editor, &TerminalStripEditor::message);
	button->click();

	SECTION("as pontes são exatamente as três filas de bornes passantes")
	{
		CHECK(bridgedRuns(strip) == expectedBridges());
	}

	SECTION("fusível, seccionável, diodo e o par separado ficam sem ponte")
	{
		for (const char *label : {"4", "6", "7", "8", "9", "12",
					  "15", "16", "17", "22", "23"})
		{
			INFO("label " << label);
			CHECK_FALSE(isBridged(strip, sheet, label));
		}
	}

	SECTION("borne sem número e borne com dois números ficam sem ponte")
	{
		for (const char *label : {"18", "19", "20", "21"})
		{
			INFO("label " << label);
			CHECK_FALSE(isBridged(strip, sheet, label));
		}
	}

	SECTION("o clique empilha um comando só, e um Ctrl+Z desfaz todas as pontes")
	{
		REQUIRE(stack->count() == before + 1);
		CHECK(UiBench::undoTopText(bench.project()) == commandText());

		stack->undo();

		CHECK(bridgedRuns(strip).isEmpty());
		CHECK(stack->index() == before);
	}

	SECTION("refazer devolve as mesmas três pontes")
	{
		stack->undo();
		REQUIRE(bridgedRuns(strip).isEmpty());

		stack->redo();

		CHECK(bridgedRuns(strip) == expectedBridges());
	}

	SECTION("a janela diz quantas pontes fez")
	{
		REQUIRE(messages.count() == 1);
		CHECK(messages.at(0).at(0).toString()
		      == TerminalStripEditor::tr("%n bridge(s) made or extended by wire number.",
						 nullptr, 3));
	}
}

TEST_CASE("T33 — apertar pontear iguais de novo não ponteia nada nem empilha comando",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("bridgebywire.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	TerminalStrip *strip = benchStrip(bench);
	REQUIRE(strip != nullptr);

	TerminalStripEditor editor(bench.project());
	editor.setCurrentStrip(strip);
	QPushButton *button = bridgeByWireButton(editor);
	REQUIRE(button != nullptr);

	button->click();
	REQUIRE(bridgedRuns(strip) == expectedBridges());
	const int after_first = bench->undoStack()->count();

	QSignalSpy messages(&editor, &TerminalStripEditor::message);
	button->click();

		//A run a bridge already holds whole is not proposed again, and a
		//command that changes nothing would be a Ctrl+Z that does nothing.
	CHECK(bench->undoStack()->count() == after_first);
	CHECK(bridgedRuns(strip) == expectedBridges());
	REQUIRE(messages.count() == 1);
	CHECK(messages.at(0).at(0).toString()
	      == TerminalStripEditor::tr("No adjacent terminals left to bridge by wire number."));
}

TEST_CASE("T33 — uma ponte feita à mão é estendida, e o Ctrl+Z a devolve inteira",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("bridgebywire.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	TerminalStrip *strip = benchStrip(bench);
	REQUIRE(strip != nullptr);

	const auto t1 = realLabelled(sheet, QStringLiteral("1"));
	const auto t2 = realLabelled(sheet, QStringLiteral("2"));
	const auto t3 = realLabelled(sheet, QStringLiteral("3"));
	REQUIRE_FALSE(t1.isNull());
	REQUIRE_FALSE(t2.isNull());
	REQUIRE_FALSE(t3.isNull());

		//The bridge somebody made by hand before pressing the button, and
		//a colour on it that only this bridge object carries.
	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{t1, t2}));
	const auto by_hand = strip->isBridged(t1);
	REQUIRE_FALSE(by_hand.isNull());
	by_hand->setColor(Qt::blue);
	REQUIRE(bridgedRuns(strip) == QStringList{QStringLiteral("1+2")});

	TerminalStripEditor editor(bench.project());
	editor.setCurrentStrip(strip);
	QPushButton *button = bridgeByWireButton(editor);
	REQUIRE(button != nullptr);

	QUndoStack *stack = bench->undoStack();
	const int before = stack->count();

	button->click();

	REQUIRE(stack->count() == before + 1);
	CHECK(bridgedRuns(strip) == expectedBridges());

		//Extended, and not replaced: the third terminal joined the very
		//bridge that was there, colour and all.
	CHECK(strip->isBridged(t3) == by_hand);
	CHECK(by_hand->color() == QColor(Qt::blue));

	SECTION("um Ctrl+Z devolve a ponte feita à mão, e não a desfaz")
	{
			//The defect this case exists for: undoing the extension by
			//unbridging the whole run took the hand made bridge apart too,
			//and the strip ended with less than it had before the click.
		stack->undo();

		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2")});
		CHECK(strip->isBridged(t1) == by_hand);
		CHECK(strip->isBridged(t2) == by_hand);
		CHECK(strip->isBridged(t3).isNull());
	}

	SECTION("refazer estende a mesma ponte outra vez")
	{
		stack->undo();
		stack->redo();

		CHECK(bridgedRuns(strip) == expectedBridges());
		CHECK(strip->isBridged(t3) == by_hand);
	}
}

TEST_CASE("T33 — com mudança pendente na tabela, pontear iguais recusa e diz por quê",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("bridgebywire.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	TerminalStrip *strip = benchStrip(bench);
	REQUIRE(strip != nullptr);

	TerminalStripEditor editor(bench.project());
	editor.setCurrentStrip(strip);
	QPushButton *button = bridgeByWireButton(editor);
	REQUIRE(button != nullptr);

	QTableView *table = editor.findChild<QTableView *>(QStringLiteral("m_table_widget"));
	REQUIRE(table != nullptr);
	QAbstractItemModel *model = table->model();
	REQUIRE(model != nullptr);

		//Terminal 1 marked as a fuse in the table, and not applied: the
		//strip still holds it as a through terminal on L1, next to two
		//more on L1.
	REQUIRE(model->index(0, TerminalStripModel::Label).data().toString()
		== QStringLiteral("1"));
	model->setData(model->index(0, TerminalStripModel::Type), ElementData::TTFuse);

	const int before = bench->undoStack()->count();
	QSignalSpy messages(&editor, &TerminalStripEditor::message);
	button->click();

	CHECK(bench->undoStack()->count() == before);
	CHECK(bridgedRuns(strip).isEmpty());
	REQUIRE(messages.count() == 1);
	CHECK(messages.at(0).at(0).toString()
	      == TerminalStripEditor::tr("Apply the pending changes first: bridging by wire number reads the saved terminal types."));
}

TEST_CASE("T33 — num borne de dois níveis, cada nível é ponteado pelos próprios fios",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("bridgebywire.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

		//Two physical terminals of two levels each. Level 0 carries L1 and
		//L2, which are two wires; level 1 carries 24 and 24, which is one.
	TerminalStrip *strip = bench->newTerminalStrip(QString(), QString(),
						       QStringLiteral("X2"));
	REQUIRE(strip != nullptr);
	for (const char *label : {"1", "4", "10", "11"})
	{
		TerminalElement *element = terminalLabelled(sheet, QLatin1String(label));
		REQUIRE(element != nullptr);
		REQUIRE(strip->addTerminal(element));
	}

	const auto t1  = realLabelled(sheet, QStringLiteral("1"));
	const auto t4  = realLabelled(sheet, QStringLiteral("4"));
	const auto t10 = realLabelled(sheet, QStringLiteral("10"));
	const auto t11 = realLabelled(sheet, QStringLiteral("11"));
	REQUIRE(strip->groupTerminals(t1->physicalTerminal(),
				      QVector<QSharedPointer<RealTerminal>>{t10}));
	REQUIRE(strip->groupTerminals(t4->physicalTerminal(),
				      QVector<QSharedPointer<RealTerminal>>{t11}));
	REQUIRE(strip->physicalTerminalCount() == 2);
	REQUIRE(t10->level() == 1);
	REQUIRE(t11->level() == 1);

	TerminalStripEditor editor(bench.project());
	editor.setCurrentStrip(strip);
	QPushButton *button = bridgeByWireButton(editor);
	REQUIRE(button != nullptr);

	button->click();

		//A reading that lost the level - every terminal read as level 0,
		//or only the first of each physical terminal read - would give no
		//bridge here, or a bridge on the wrong row.
	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("10+11")});
	CHECK(strip->isBridged(t1).isNull());
	CHECK(strip->isBridged(t4).isNull());
}
