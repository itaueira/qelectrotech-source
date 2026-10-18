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
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/TerminalStrip/physicalterminal.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/terminalstripbridge.h"
#include "../../../../sources/TerminalStrip/UndoCommand/bridgeterminalscommand.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripeditor.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripmodel.h"

#include <catch2/catch.hpp>

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QUndoStack>
#include <QVector>

/*
	Undoing the bridge button of the terminal strip editor when the
	selection extends a bridge that is already there.

	The button hands BridgeTerminalsCommand every terminal of the selection,
	the bridged ones included, and the strip extends the bridge it finds
	among them. Undo used to unbridge every terminal it had been handed:
	with 1 and 2 bridged and 1, 2 and 3 selected, one Ctrl+Z took the bridge
	1-2 apart together with the extension, and the strip ended with less
	than it had before the click. Undo now gives back only the terminals
	the command bridged, and the bridge that was there stays the same
	object, with the same color.

	Each case carries the defect it exists for. The first is the one above.
	The second extends a bridge on both sides, because the strip only lets
	go of terminals at one end of a bridge: an undo that took the two added
	terminals back in one call would be refused and do nothing. The third
	is a new bridge, which undo takes away whole, as it always did - the
	part of the old behaviour that was right. The fourth fills the gap of
	a bridge a reordering left with a hole, which the strip cannot give
	back as it was; what is proved there is that undo does not go silent.
	The last goes through the button itself, with cells selected in the
	table the way somebody selects them.

	Labelled T33 and not with a case of use: what is proved is the undo of
	a button, not a case of use of the specification.
*/

namespace
{
	const char *const bench_labels[] = {"1", "2", "3", "4", "5"};

	/**
		A project of one folio with five terminal elements, labelled 1 to
		5 from left to right, and nothing else: no conductor, because
		bridging reads positions and levels, not wires.
	*/
	QString fixtureXml()
	{
		QString instances;
		int index = 0;
		for (const char *label : bench_labels)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"150\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/terminal.elmt\""
					     " uuid=\"{b71d9e00-0000-4000-8000-00000000000%2}\">"
					     "<terminals>"
					     "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%3\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%4"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(120 + index * 120)
				     .arg(index)
				     .arg(index)
				     .arg(QLatin1String(label));
			++index;
		}

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
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " folio=\"1\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(instances);
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

	/**
		The terminals of the bench, by label, and the strip X1 made of them
		in the order of their labels, so that the position of a terminal
		is its label minus one.
	*/
	struct Bench
	{
		TerminalStrip *strip = nullptr;
		QHash<QString, QSharedPointer<RealTerminal>> terminal;

		QSharedPointer<RealTerminal> operator[](const char *label) const {
			return terminal.value(QLatin1String(label));
		}
	};

	/// Null strip when a terminal of the bench is missing from the folio.
	Bench benchStrip(UiBench::ScratchProject &project)
	{
		Bench bench;
		Diagram *sheet = project.diagram(0);
		TerminalStrip *strip = project->newTerminalStrip(QString(),
								 QString(),
								 QStringLiteral("X1"));
		if (!sheet || !strip) {
			return bench;
		}

		for (const char *label : bench_labels)
		{
			TerminalElement *element = terminalLabelled(sheet, QLatin1String(label));
			if (!element || !strip->addTerminal(element)) {
				return bench;
			}
			bench.terminal.insert(QLatin1String(label), element->realTerminal());
		}

		bench.strip = strip;
		return bench;
	}

	/**
		Every bridge of @a strip, each written as the labels it holds joined
		by "+", in the order they are met walking the strip.

		A list and not a count, so that a failure says which terminals are
		bridged; and grouped by bridge, so that a bridge given back as two
		reads differently from one.
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

	QString commandText()
	{
		return QObject::tr("Bridging terminals together");
	}
}

TEST_CASE("T33 — estender uma ponte e desfazer devolve a ponte de antes, o mesmo objeto e a mesma cor",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("manualbridgeundo.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(bridgedRuns(strip).isEmpty());

		//The bridge made before, and a colour on it that only this bridge
		//object carries.
	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["1"], bench["2"]}));
	const auto before_bridge = strip->isBridged(bench["1"]);
	REQUIRE_FALSE(before_bridge.isNull());
	before_bridge->setColor(Qt::blue);
	REQUIRE(bridgedRuns(strip) == QStringList{QStringLiteral("1+2")});

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();

		//Out of order on purpose: the editor hands the terminals out of a
		//set, in no order of the strip.
	stack->push(new BridgeTerminalsCommand(strip,
					       QVector<QSharedPointer<RealTerminal>>{bench["3"], bench["1"], bench["2"]}));

	REQUIRE(stack->count() == before + 1);
	CHECK(UiBench::undoTopText(project.project()) == commandText());
	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3")});
		//Extended, and not replaced.
	CHECK(strip->isBridged(bench["3"]) == before_bridge);

	SECTION("um Ctrl+Z tira só o borne acrescentado")
	{
		stack->undo();

		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2")});
		CHECK(strip->isBridged(bench["1"]) == before_bridge);
		CHECK(strip->isBridged(bench["2"]) == before_bridge);
		CHECK(strip->isBridged(bench["3"]).isNull());
		CHECK(before_bridge->color() == QColor(Qt::blue));
		CHECK(stack->index() == before);
	}

	SECTION("refazer estende a mesma ponte outra vez")
	{
		stack->undo();
		stack->redo();

		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3")});
		CHECK(strip->isBridged(bench["3"]) == before_bridge);
		CHECK(before_bridge->color() == QColor(Qt::blue));
	}

	SECTION("desfazer e refazer de novo continua devolvendo o mesmo")
	{
			//The second undo runs on what a later redo left, and not on
			//the state the command was first pushed on.
		stack->undo();
		stack->redo();
		stack->undo();

		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2")});
		CHECK(strip->isBridged(bench["1"]) == before_bridge);
	}
}

TEST_CASE("T33 — ponte estendida dos dois lados: um Ctrl+Z tira os dois blocos e devolve a do meio",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("manualbridgeundo.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);

	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["2"], bench["3"]}));
	const auto before_bridge = strip->isBridged(bench["2"]);
	REQUIRE_FALSE(before_bridge.isNull());
	before_bridge->setColor(Qt::blue);

	QUndoStack *stack = project->undoStack();
	stack->push(new BridgeTerminalsCommand(strip,
					       QVector<QSharedPointer<RealTerminal>>{bench["4"], bench["2"], bench["1"], bench["3"]}));

	REQUIRE(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3+4")});
	REQUIRE(strip->isBridged(bench["1"]) == before_bridge);

		//1 and 4 are not adjacent, and TerminalStrip::unBridge refuses
		//them in one call: only block by block do they come off.
	stack->undo();

	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});
	CHECK(strip->isBridged(bench["2"]) == before_bridge);
	CHECK(strip->isBridged(bench["1"]).isNull());
	CHECK(strip->isBridged(bench["4"]).isNull());
	CHECK(before_bridge->color() == QColor(Qt::blue));

	stack->redo();

	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3+4")});
	CHECK(strip->isBridged(bench["4"]) == before_bridge);
}

TEST_CASE("T33 — ponte nova: um Ctrl+Z a tira inteira, e refazer devolve a mesma",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("manualbridgeundo.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(bridgedRuns(strip).isEmpty());

	QUndoStack *stack = project->undoStack();
	const int before = stack->count();
	stack->push(new BridgeTerminalsCommand(strip,
					       QVector<QSharedPointer<RealTerminal>>{bench["2"], bench["3"], bench["1"]}));

	REQUIRE(stack->count() == before + 1);
	REQUIRE(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3")});
	const auto made = strip->isBridged(bench["1"]);
	REQUIRE_FALSE(made.isNull());
	made->setColor(Qt::red);

	stack->undo();

	CHECK(bridgedRuns(strip).isEmpty());

	stack->redo();

	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3")});
	CHECK(strip->isBridged(bench["1"]) == made);
	CHECK(made->color() == QColor(Qt::red));
}

TEST_CASE("T33 — ponte com vão, preenchida: o Ctrl+Z tira o borne acrescentado e refazer a devolve inteira",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("manualbridgeundo.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);

		//1 and 2 bridged, then 3 put between them: the bridge is left
		//with a hole, which is what reordering a strip does to a bridge.
	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["1"], bench["2"]}));
	const auto before_bridge = strip->isBridged(bench["1"]);
	REQUIRE_FALSE(before_bridge.isNull());
	REQUIRE(strip->setOrderTo(QVector<QSharedPointer<PhysicalTerminal>>{
					  bench["1"]->physicalTerminal(),
					  bench["3"]->physicalTerminal(),
					  bench["2"]->physicalTerminal(),
					  bench["4"]->physicalTerminal(),
					  bench["5"]->physicalTerminal()}));
	REQUIRE(bridgedRuns(strip) == QStringList{QStringLiteral("1+2")});
	REQUIRE(strip->isBridged(bench["3"]).isNull());

	const QVector<QSharedPointer<RealTerminal>> filled{bench["1"], bench["3"], bench["2"]};
	REQUIRE(strip->isBridgeable(filled));

	QUndoStack *stack = project->undoStack();
	stack->push(new BridgeTerminalsCommand(strip, filled));
	REQUIRE(bridgedRuns(strip) == QStringList{QStringLiteral("1+3+2")});

		//The strip cannot unbridge 3 from the middle of the bridge, nor
		//make the bridge 1-2 again with 3 between them. What has to hold
		//whatever undo can do here: the terminal the command added does
		//not stay bridged, so that the Ctrl+Z is not a silent one. Today
		//1 and 2 lose their bridge with it, as every undo of this command
		//did before.
	stack->undo();

	CHECK(strip->isBridged(bench["3"]).isNull());

	stack->redo();

	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+3+2")});
	CHECK(strip->isBridged(bench["3"]) == before_bridge);
}

TEST_CASE("T33 — o botão de pontear, estendendo uma ponte feita à mão: um Ctrl+Z a devolve inteira",
	  "[terminalstrip][bridge]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("manualbridgeundo.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);

	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["1"], bench["2"]}));
	const auto by_hand = strip->isBridged(bench["1"]);
	REQUIRE_FALSE(by_hand.isNull());
	by_hand->setColor(Qt::blue);

	TerminalStripEditor editor(project.project());
	editor.setCurrentStrip(strip);

	QTableView *table = editor.findChild<QTableView *>(QStringLiteral("m_table_widget"));
	REQUIRE(table != nullptr);
	QAbstractItemModel *model = table->model();
	REQUIRE(model != nullptr);
	REQUIRE(table->selectionModel() != nullptr);

		//The rows the selection below takes, checked before it is made:
		//one row per terminal, in the order of the strip.
	for (int row = 0 ; row < 3 ; ++row) {
		REQUIRE(model->index(row, TerminalStripModel::Label).data().toString()
			== QString::number(row + 1));
	}

	QPushButton *button = editor.findChild<QPushButton *>(QStringLiteral("m_bridge_terminals_pb"));
	REQUIRE(button != nullptr);

		//Terminals 1 to 3 in the column of level 0, as a drag down that
		//column selects them.
	table->selectionModel()->select(QItemSelection(model->index(0, TerminalStripModel::Level0),
						       model->index(2, TerminalStripModel::Level0)),
					QItemSelectionModel::ClearAndSelect);
	REQUIRE(button->isEnabled());

	QUndoStack *stack = project->undoStack();
	const int before = stack->count();

	button->click();

	REQUIRE(stack->count() == before + 1);
	CHECK(UiBench::undoTopText(project.project()) == commandText());
	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3")});
	CHECK(strip->isBridged(bench["3"]) == by_hand);

	SECTION("um Ctrl+Z devolve a ponte feita à mão, e não a desfaz")
	{
		stack->undo();

		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2")});
		CHECK(strip->isBridged(bench["1"]) == by_hand);
		CHECK(strip->isBridged(bench["2"]) == by_hand);
		CHECK(strip->isBridged(bench["3"]).isNull());
		CHECK(by_hand->color() == QColor(Qt::blue));
	}

	SECTION("refazer estende a mesma ponte outra vez")
	{
		stack->undo();
		stack->redo();

		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("1+2+3")});
		CHECK(strip->isBridged(bench["3"]) == by_hand);
	}
}
