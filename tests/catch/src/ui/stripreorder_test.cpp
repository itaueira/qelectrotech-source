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
#include "../../../../sources/label/labelentry.h"
#include "../../../../sources/label/terminallabelcollector.h"
#include "../../../../sources/TerminalStrip/physicalterminal.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/terminalstripbridge.h"
#include "../../../../sources/TerminalStrip/UndoCommand/reorderterminalstripcommand.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripeditor.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripmodel.h"

#include <catch2/catch.hpp>

#include <QAbstractItemModel>
#include <QHash>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QList>
#include <QPushButton>
#include <QSet>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QUndoStack>
#include <QVector>

#include <algorithm>

/*
	Changing the order of a terminal strip by hand: the buttons Move up,
	Move down and Move to position of the terminal strip editor.

	A strip X1 of six terminals drawn on a folio, put on the strip in the
	order 1, 2, 3, 4, DJ0, DJ1, with the bridge 2+3 - the strip of the case
	of use. Every move is made by selecting cells of the table of the editor
	the program shows and clicking its button, and every result is read
	three ways: the strip itself, the table the person looks at, and the
	label list the export writes for whoever mounts the rail.

	What each case carries:
	- one click is one undo step, and a refused click is none;
	- no label changes: the order moves, the tags stay - renumbering is a
	  decision of its own, still open;
	- a move that would put a terminal inside a bridge is refused and says
	  which bridge, and the same move with the bridged terminals selected
	  together goes through;
	- an edit typed in the table and not applied is never thrown away by a
	  move: the move is refused, and the edit is still there.

	The four parts of the case of use carry CU-33.12, one case each: (a)
	Move up twice, (b) undo and Move to position 4, (c) Move down on the 3
	alone and then on 2 and 3, (d) a pending edit. The case says "move to
	position 4" and "positions 1 to 6", counting from 1, and so do the
	Position column of the editor, the position box beside it and the label
	export (LabelEntry::strip_position says why, and terminallabel_test.cpp
	holds the two to each other). So the 4 the case writes is typed here as
	it is written, after checking it is the number the column shows for the
	terminal 4. The other cases go past the case of use and stay T33.
*/

namespace
{
	const char *const bench_labels[] = {"1", "2", "3", "4", "DJ0", "DJ1"};

	/**
		A project of one folio with six terminal elements, labelled as
		bench_labels from left to right, and nothing else: no conductor,
		because the order of a strip reads positions and bridges, not wires.
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
					     " uuid=\"{5e0dde00-0000-4000-8000-00000000000%2}\">"
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
		in the order of bench_labels.
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

	QStringList original()
	{
		return QStringList{QStringLiteral("1"), QStringLiteral("2"),
				   QStringLiteral("3"), QStringLiteral("4"),
				   QStringLiteral("DJ0"), QStringLiteral("DJ1")};
	}

	/// The order the case of use asks for.
	QStringList reordered()
	{
		return QStringList{QStringLiteral("1"), QStringLiteral("2"),
				   QStringLiteral("3"), QStringLiteral("DJ0"),
				   QStringLiteral("DJ1"), QStringLiteral("4")};
	}

	/**
		The rail of @a strip, one entry per physical terminal: the labels of
		its levels, back to front, joined by "/". The strip as it holds
		itself, before any table or export reads it.
	*/
	QStringList rail(TerminalStrip *strip)
	{
		QStringList entries;
		const auto physicals = strip->physicalTerminal();
		for (const auto &physical : physicals)
		{
			QStringList levels;
			const auto real_terminals = physical->realTerminals();
			for (const auto &real_t : real_terminals) {
				levels << real_t->label();
			}
			entries << levels.join(QLatin1Char('/'));
		}
		return entries;
	}

	/// The physical terminals holding the terminals labelled @a labels, in that order.
	ReorderTerminalStripCommand::PhysicalVector physicalsOf(const Bench &bench,
								const QStringList &labels)
	{
		ReorderTerminalStripCommand::PhysicalVector physicals;
		for (const auto &label : labels) {
			physicals << bench.terminal.value(label)->physicalTerminal();
		}
		return physicals;
	}

	/// The Label column of the table of the editor, row by row.
	QStringList labelsShown(QAbstractItemModel *model)
	{
		QStringList labels;
		for (int row = 0 ; row < model->rowCount() ; ++row) {
			labels << model->index(row, TerminalStripModel::Label).data().toString();
		}
		return labels;
	}

	/// The Position column of the table of the editor, row by row, from 1.
	QVector<int> positionsShown(QAbstractItemModel *model)
	{
		QVector<int> positions;
		for (int row = 0 ; row < model->rowCount() ; ++row) {
			positions << model->index(row, TerminalStripModel::Pos).data().toInt();
		}
		return positions;
	}

	/// The row of the table showing @a label, -1 when none.
	int rowOf(QAbstractItemModel *model, const QString &label)
	{
		for (int row = 0 ; row < model->rowCount() ; ++row) {
			if (model->index(row, TerminalStripModel::Label).data().toString() == label) {
				return row;
			}
		}
		return -1;
	}

	/// Select the Label cells of @a labels, as a click and Ctrl+clicks do.
	void selectLabels(QTableView *table, const QStringList &labels)
	{
		QAbstractItemModel *model = table->model();
		QItemSelection selection;
		for (const auto &label : labels)
		{
			const auto index = model->index(rowOf(model, label), TerminalStripModel::Label);
			selection.select(index, index);
		}
		table->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
	}

	/// The labels of the rows holding a selected cell, top to bottom.
	QStringList selectedLabels(QTableView *table)
	{
		QSet<int> row_set;
		const auto indexes = table->selectionModel()->selectedIndexes();
		for (const auto &index : indexes) {
			row_set.insert(index.row());
		}

		QList<int> rows(row_set.cbegin(), row_set.cend());
		std::sort(rows.begin(), rows.end());

		QStringList labels;
		for (const int row : std::as_const(rows)) {
			labels << table->model()->index(row, TerminalStripModel::Label).data().toString();
		}
		return labels;
	}

	/// The labels the label export writes for @a strip, in its order.
	QStringList exportedLabels(TerminalStrip *strip)
	{
		QStringList labels;
		const LabelEntryList entries = TerminalLabelCollector::collectStrip(strip);
		for (const LabelEntry &entry : entries) {
			labels << entry.primary_text;
		}
		return labels;
	}

	/// The positions the label export writes beside each label of @a strip.
	QVector<int> exportedPositions(TerminalStrip *strip)
	{
		QVector<int> positions;
		const LabelEntryList entries = TerminalLabelCollector::collectStrip(strip);
		for (const LabelEntry &entry : entries) {
			positions << entry.strip_position;
		}
		return positions;
	}

	/**
		The terminals of the bench that no longer carry the label they were
		drawn with. Empty is the answer every move must leave: the order
		changes, the tags do not.
	*/
	QStringList renamedTerminals(const Bench &bench)
	{
		QStringList renamed;
		for (const char *label : bench_labels)
		{
			const auto real_t = bench[label];
			if (real_t.isNull() || real_t->label() != QLatin1String(label)) {
				renamed << QLatin1String(label);
			}
		}
		return renamed;
	}

	/**
		Every bridge of @a strip, each written as the labels it holds joined
		by "+", in the order they are met walking the strip.
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

	QPushButton *buttonNamed(TerminalStripEditor &editor, const char *name)
	{
		return editor.findChild<QPushButton *>(QLatin1String(name));
	}

	QString bridgeRefusal(const char *bridge)
	{
		return TerminalStripEditor::tr("Not moved: bridge %1 would have a terminal in its middle.")
				.arg(QLatin1String(bridge));
	}

	QString pendingRefusal()
	{
		return TerminalStripEditor::tr("Apply the pending changes first: moving terminals reloads the table, which would discard them.");
	}

	QString movedMessage(int count)
	{
		return TerminalStripEditor::tr("%n terminal(s) moved; their labels are unchanged.", nullptr, count);
	}

	/**
		The editor of the case, its table and its buttons, found the way
		the editor names them, on the strip of @a bench.
	*/
	struct Editor
	{
		explicit Editor(UiBench::ScratchProject &project, TerminalStrip *strip) :
			editor(project.project())
		{
			editor.setCurrentStrip(strip);
			table = editor.findChild<QTableView *>(QStringLiteral("m_table_widget"));
			model = table ? table->model() : nullptr;
			up = buttonNamed(editor, "m_move_up_pb");
			down = buttonNamed(editor, "m_move_down_pb");
			to_position = buttonNamed(editor, "m_move_to_position_pb");
			position = editor.findChild<QSpinBox *>(QStringLiteral("m_position_sb"));
		}

		bool isComplete() const {
			return table && model && table->selectionModel()
					&& up && down && to_position && position;
		}

		TerminalStripEditor editor;
		QTableView *table = nullptr;
		QAbstractItemModel *model = nullptr;
		QPushButton *up = nullptr;
		QPushButton *down = nullptr;
		QPushButton *to_position = nullptr;
		QSpinBox *position = nullptr;
	};
}

TEST_CASE("CU-33.12 (a) — Subir duas vezes: só a primeira move, a ponte 2+3 recusa",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(rail(strip) == original());

	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["2"], bench["3"]}));
	const auto bridge = strip->isBridged(bench["2"]);
	REQUIRE_FALSE(bridge.isNull());
	REQUIRE(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});

	Editor ui(project, strip);
	REQUIRE(ui.isComplete());
	REQUIRE(labelsShown(ui.model) == original());
	REQUIRE(ui.up->isEnabled());

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();
	QSignalSpy messages(&ui.editor, &TerminalStripEditor::message);

	selectLabels(ui.table, {QStringLiteral("DJ0"), QStringLiteral("DJ1")});
	REQUIRE(selectedLabels(ui.table) == QStringList({QStringLiteral("DJ0"), QStringLiteral("DJ1")}));

	ui.up->click();

	REQUIRE(stack->count() == before + 1);
	CHECK(UiBench::undoTopText(project.project()) == TerminalStripEditor::tr("Move terminals up"));
	CHECK(rail(strip) == reordered());
	REQUIRE(messages.count() == 1);
	CHECK(messages.at(0).at(0).toString() == movedMessage(2));

	SECTION("a tabela mostra a ordem nova, com as posições em sequência")
	{
		CHECK(labelsShown(ui.model) == reordered());
		CHECK(positionsShown(ui.model) == QVector<int>({1, 2, 3, 4, 5, 6}));
	}

	SECTION("nenhuma tag mudou")
	{
		CHECK(renamedTerminals(bench).isEmpty());
	}

	SECTION("a lista de bornes exportada sai na ordem nova")
	{
		CHECK(exportedLabels(strip) == reordered());
		CHECK(exportedPositions(strip) == QVector<int>({1, 2, 3, 4, 5, 6}));
	}

	SECTION("a seleção segue DJ0 e DJ1, e o segundo Subir é recusado pela ponte 2+3")
	{
			//The case of use presses Move up twice and expects 1, 2, 3, DJ0,
			//DJ1, 4: the first press gives it, and the second would put DJ0
			//and DJ1 between 2 and 3, inside the bridge. What the second
			//press must do is say so and leave everything as it is - one
			//undo step for the two presses.
		REQUIRE(selectedLabels(ui.table) == QStringList({QStringLiteral("DJ0"), QStringLiteral("DJ1")}));

		ui.up->click();

		CHECK(stack->count() == before + 1);
		CHECK(rail(strip) == reordered());
		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});
		CHECK(strip->isBridged(bench["3"]) == bridge);
		REQUIRE(messages.count() == 2);
		CHECK(messages.at(1).at(0).toString() == bridgeRefusal("2+3"));
	}

	SECTION("um Ctrl+Z devolve a ordem de antes, e refazer a de depois")
	{
		stack->undo();

		CHECK(stack->index() == before);
		CHECK(rail(strip) == original());
		CHECK(labelsShown(ui.model) == original());
		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});

		stack->redo();

		CHECK(rail(strip) == reordered());
		CHECK(labelsShown(ui.model) == reordered());
	}
}

TEST_CASE("CU-33.12 (b) — desfazer e Mover para a posição 4 dá a mesma ordem",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["2"], bench["3"]}));

	Editor ui(project, strip);
	REQUIRE(ui.isComplete());

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();

		//(a) and its undo, as the case of use orders them.
	selectLabels(ui.table, {QStringLiteral("DJ0"), QStringLiteral("DJ1")});
	ui.up->click();
	REQUIRE(rail(strip) == reordered());
	stack->undo();
	REQUIRE(rail(strip) == original());
	REQUIRE(stack->index() == before);

		//The 4 the case of use writes is the number the Position column
		//shows for the terminal 4, read from the table: the box, the column
		//and the case count alike, from 1, up to the six terminals there are.
	const int position_of_4 = ui.model->index(rowOf(ui.model, QStringLiteral("4")),
						  TerminalStripModel::Pos).data().toInt();
	REQUIRE(position_of_4 == 4);
	REQUIRE(ui.position->minimum() == 1);
	REQUIRE(ui.position->maximum() == 6);

	QSignalSpy messages(&ui.editor, &TerminalStripEditor::message);
	selectLabels(ui.table, {QStringLiteral("DJ0"), QStringLiteral("DJ1")});
	ui.position->setValue(4);
	ui.to_position->click();

	REQUIRE(stack->count() == before + 1);
	CHECK(stack->index() == before + 1);
	CHECK(UiBench::undoTopText(project.project())
	      == TerminalStripEditor::tr("Move terminals to position %1").arg(4));
	CHECK(rail(strip) == reordered());
	CHECK(labelsShown(ui.model) == reordered());
	CHECK(positionsShown(ui.model) == QVector<int>({1, 2, 3, 4, 5, 6}));
	CHECK(renamedTerminals(bench).isEmpty());
	CHECK(exportedLabels(strip) == reordered());
	CHECK(exportedPositions(strip) == QVector<int>({1, 2, 3, 4, 5, 6}));
	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});
	REQUIRE(messages.count() == 1);
	CHECK(messages.at(0).at(0).toString() == movedMessage(2));

	SECTION("um Ctrl+Z desfaz o clique inteiro")
	{
		stack->undo();

		CHECK(stack->index() == before);
		CHECK(rail(strip) == original());
	}

	SECTION("bornes separados vão juntos, na ordem do trilho, e além do fim o bloco fica no fim")
	{
		stack->undo();
		REQUIRE(rail(strip) == original());

			//1 and 4 are not neighbours, and 6 is past the last place a
			//block of two fits in: the block lands at 5, and the command
			//and the message say 5, not the 6 typed.
		selectLabels(ui.table, {QStringLiteral("4"), QStringLiteral("1")});
		ui.position->setValue(6);
		ui.to_position->click();

		CHECK(rail(strip) == QStringList({QStringLiteral("2"), QStringLiteral("3"),
						  QStringLiteral("DJ0"), QStringLiteral("DJ1"),
						  QStringLiteral("1"), QStringLiteral("4")}));
		CHECK(UiBench::undoTopText(project.project())
		      == TerminalStripEditor::tr("Move terminals to position %1").arg(5));
		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});
	}
}

TEST_CASE("CU-33.12 (c) — Descer só o 3 é recusado; 2 e 3 juntos descem",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["2"], bench["3"]}));
	const auto bridge = strip->isBridged(bench["2"]);
	REQUIRE_FALSE(bridge.isNull());

		//The order (a) and (b) leave, set on the strip: how it is reached
		//is what the two cases above prove.
	REQUIRE(strip->setOrderTo(physicalsOf(bench, reordered())));
	REQUIRE(rail(strip) == reordered());

	Editor ui(project, strip);
	REQUIRE(ui.isComplete());
	REQUIRE(labelsShown(ui.model) == reordered());

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();
	QSignalSpy messages(&ui.editor, &TerminalStripEditor::message);

	selectLabels(ui.table, {QStringLiteral("3")});
	ui.down->click();

	CHECK(stack->count() == before);
	CHECK(rail(strip) == reordered());
	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});
	REQUIRE(messages.count() == 1);
	CHECK(messages.at(0).at(0).toString() == bridgeRefusal("2+3"));

	selectLabels(ui.table, {QStringLiteral("2"), QStringLiteral("3")});
	ui.down->click();

	REQUIRE(stack->count() == before + 1);
	CHECK(UiBench::undoTopText(project.project()) == TerminalStripEditor::tr("Move terminals down"));
	CHECK(rail(strip) == QStringList({QStringLiteral("1"), QStringLiteral("DJ0"),
					  QStringLiteral("2"), QStringLiteral("3"),
					  QStringLiteral("DJ1"), QStringLiteral("4")}));
		//The same bridge, moved with its terminals, and not a new one.
	CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});
	CHECK(strip->isBridged(bench["2"]) == bridge);
	CHECK(strip->isBridged(bench["3"]) == bridge);
	CHECK(renamedTerminals(bench).isEmpty());
	REQUIRE(messages.count() == 2);
	CHECK(messages.at(1).at(0).toString() == movedMessage(2));
}

TEST_CASE("CU-33.12 (d) — com edição pendente, mover é recusado e a edição fica",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);

	Editor ui(project, strip);
	REQUIRE(ui.isComplete());

		//The tag of terminal 1 typed in the table and not applied.
	const int row_of_1 = rowOf(ui.model, QStringLiteral("1"));
	REQUIRE(row_of_1 >= 0);
	ui.model->setData(ui.model->index(row_of_1, TerminalStripModel::Label),
			  QStringLiteral("1A"));
	REQUIRE(ui.model->index(row_of_1, TerminalStripModel::Label).data().toString()
		== QStringLiteral("1A"));

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();
	QSignalSpy messages(&ui.editor, &TerminalStripEditor::message);

	selectLabels(ui.table, {QStringLiteral("DJ0")});
	ui.position->setValue(1);

	for (QPushButton *button : {ui.up, ui.down, ui.to_position})
	{
		INFO("button " << button->objectName().toStdString());
		const int said = messages.count();

		button->click();

		CHECK(stack->count() == before);
		CHECK(rail(strip) == original());
		REQUIRE(messages.count() == said + 1);
		CHECK(messages.last().at(0).toString() == pendingRefusal());
	}

		//The edit is where it was typed, still pending, and the terminal
		//still carries the tag it had: refusing did not apply it either.
	CHECK(ui.model->index(row_of_1, TerminalStripModel::Label).data().toString()
	      == QStringLiteral("1A"));
	CHECK(bench["1"]->label() == QStringLiteral("1"));
}

TEST_CASE("T33 — mudar a ordem à mão: sem seleção, no topo, no fim e na mesma posição, nada é empilhado e a mensagem diz por quê",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);

	Editor ui(project, strip);
	REQUIRE(ui.isComplete());

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();
	QSignalSpy messages(&ui.editor, &TerminalStripEditor::message);

	SECTION("sem seleção")
	{
		ui.table->selectionModel()->clearSelection();
		ui.up->click();
		REQUIRE(messages.count() == 1);
		CHECK(messages.at(0).at(0).toString()
		      == TerminalStripEditor::tr("Select the terminals to move first."));
	}

	SECTION("Subir o primeiro")
	{
		selectLabels(ui.table, {QStringLiteral("1")});
		ui.up->click();
		REQUIRE(messages.count() == 1);
		CHECK(messages.at(0).at(0).toString()
		      == TerminalStripEditor::tr("The selected terminals are already at the top."));
	}

	SECTION("Descer o último")
	{
		selectLabels(ui.table, {QStringLiteral("DJ1")});
		ui.down->click();
		REQUIRE(messages.count() == 1);
		CHECK(messages.at(0).at(0).toString()
		      == TerminalStripEditor::tr("The selected terminals are already at the bottom."));
	}

	SECTION("Mover para a posição em que já está")
	{
			//3 and 4 are at the positions 3 and 4 the column shows.
		selectLabels(ui.table, {QStringLiteral("3"), QStringLiteral("4")});
		ui.position->setValue(3);
		ui.to_position->click();
		REQUIRE(messages.count() == 1);
		CHECK(messages.at(0).at(0).toString()
		      == TerminalStripEditor::tr("The selected terminals are already at position %1.").arg(3));
	}

	CHECK(stack->count() == before);
	CHECK(rail(strip) == original());
}

TEST_CASE("T33 — mudar a ordem à mão: uma ponte que já tinha buraco não trava a régua",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(strip->setBridge(QVector<QSharedPointer<RealTerminal>>{bench["2"], bench["3"]}));

		//The bridge 2+3 with DJ0 in its middle: a strip sorted after it
		//was bridged. The move did not open it, so it does not stop the
		//move - and the one that closes it is the move somebody makes to
		//repair the strip.
	const QStringList gapped{QStringLiteral("1"), QStringLiteral("2"),
				 QStringLiteral("DJ0"), QStringLiteral("3"),
				 QStringLiteral("4"), QStringLiteral("DJ1")};
	REQUIRE(strip->setOrderTo(physicalsOf(bench, gapped)));

	Editor ui(project, strip);
	REQUIRE(ui.isComplete());

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();

	SECTION("mover outro borne passa")
	{
		selectLabels(ui.table, {QStringLiteral("DJ1")});
		ui.up->click();

		CHECK(stack->count() == before + 1);
		CHECK(rail(strip) == QStringList({QStringLiteral("1"), QStringLiteral("2"),
						  QStringLiteral("DJ0"), QStringLiteral("3"),
						  QStringLiteral("DJ1"), QStringLiteral("4")}));
	}

	SECTION("tirar o borne do meio fecha a ponte")
	{
		selectLabels(ui.table, {QStringLiteral("DJ0")});
		ui.down->click();

		CHECK(stack->count() == before + 1);
		CHECK(rail(strip) == QStringList({QStringLiteral("1"), QStringLiteral("2"),
						  QStringLiteral("3"), QStringLiteral("DJ0"),
						  QStringLiteral("4"), QStringLiteral("DJ1")}));
		CHECK(bridgedRuns(strip) == QStringList{QStringLiteral("2+3")});
	}
}

TEST_CASE("T33 — mudar a ordem à mão: num borne de dois níveis, o borne físico inteiro se move e a célula escolhida continua escolhida",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);

		//3 and 4 on one physical terminal of two levels: five places on
		//the rail, six rows in the table.
	REQUIRE(strip->groupTerminals(bench["3"]->physicalTerminal(),
				      QVector<QSharedPointer<RealTerminal>>{bench["4"]}));
	REQUIRE(rail(strip) == QStringList({QStringLiteral("1"), QStringLiteral("2"),
					    QStringLiteral("3/4"), QStringLiteral("DJ0"),
					    QStringLiteral("DJ1")}));

	Editor ui(project, strip);
	REQUIRE(ui.isComplete());
	REQUIRE(ui.model->rowCount() == 6);
	CHECK(ui.position->maximum() == 5);

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();

		//Only the cell of the level 4, not the row of 3.
	selectLabels(ui.table, {QStringLiteral("4")});
	ui.up->click();

	REQUIRE(stack->count() == before + 1);
	CHECK(rail(strip) == QStringList({QStringLiteral("1"), QStringLiteral("3/4"),
					  QStringLiteral("2"), QStringLiteral("DJ0"),
					  QStringLiteral("DJ1")}));
		//The selection follows the cell, and does not grow to the whole
		//physical terminal: a second click moves it one place further.
	REQUIRE(selectedLabels(ui.table) == QStringList{QStringLiteral("4")});

	ui.up->click();

	REQUIRE(stack->count() == before + 2);
	CHECK(rail(strip) == QStringList({QStringLiteral("3/4"), QStringLiteral("1"),
					  QStringLiteral("2"), QStringLiteral("DJ0"),
					  QStringLiteral("DJ1")}));
	CHECK(positionsShown(ui.model) == QVector<int>({1, 1, 2, 3, 4, 5}));
	CHECK(renamedTerminals(bench).isEmpty());
}

TEST_CASE("T33 — mudar a ordem à mão: Subir, Descer e Mover para a posição, sobre seleções separadas",
	  "[terminalstrip][reorder]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripreorder.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);

	const auto order = strip->physicalTerminal();
	const auto labels_of = [](const ReorderTerminalStripCommand::PhysicalVector &physicals)
	{
		QStringList labels;
		for (const auto &physical : physicals) {
			labels << physical->realTerminals().constFirst()->label();
		}
		return labels;
	};

	SECTION("Subir: a fila já no topo fica, as outras sobem uma")
	{
		const auto selected = physicalsOf(bench, {QStringLiteral("1"), QStringLiteral("2"),
							  QStringLiteral("4"), QStringLiteral("DJ1")});
		CHECK(labels_of(ReorderTerminalStripCommand::movedUp(order, selected))
		      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
				      QStringLiteral("4"), QStringLiteral("3"),
				      QStringLiteral("DJ1"), QStringLiteral("DJ0")}));
	}

	SECTION("Descer: a fila já no fim fica, as outras descem uma")
	{
		const auto selected = physicalsOf(bench, {QStringLiteral("1"), QStringLiteral("3"),
							  QStringLiteral("DJ0"), QStringLiteral("DJ1")});
		CHECK(labels_of(ReorderTerminalStripCommand::movedDown(order, selected))
		      == QStringList({QStringLiteral("2"), QStringLiteral("1"),
				      QStringLiteral("4"), QStringLiteral("3"),
				      QStringLiteral("DJ0"), QStringLiteral("DJ1")}));
	}

	SECTION("Mover para a posição: o bloco na ordem do trilho, qualquer que seja a ordem da seleção")
	{
		const auto selected = physicalsOf(bench, {QStringLiteral("DJ1"), QStringLiteral("2")});
			//The command counts from 0, like PhysicalTerminal::pos(): 0 is
			//the first place, the 1 of the column and of the box.
		CHECK(labels_of(ReorderTerminalStripCommand::movedTo(order, selected, 0))
		      == QStringList({QStringLiteral("2"), QStringLiteral("DJ1"),
				      QStringLiteral("1"), QStringLiteral("3"),
				      QStringLiteral("4"), QStringLiteral("DJ0")}));
		CHECK(labels_of(ReorderTerminalStripCommand::movedTo(order, selected, 99))
		      == QStringList({QStringLiteral("1"), QStringLiteral("3"),
				      QStringLiteral("4"), QStringLiteral("DJ0"),
				      QStringLiteral("2"), QStringLiteral("DJ1")}));
	}
}

namespace
{
	/**
		The editor of @a strip with its two grouping buttons, as the
		person finds them on the page of the strip.
	*/
	struct GroupEditor : Editor
	{
		GroupEditor(UiBench::ScratchProject &project, TerminalStrip *strip) :
			Editor(project, strip),
			group(buttonNamed(editor, "m_group_terminals_pb")),
			ungroup(buttonNamed(editor, "m_ungroup_pb"))
		{}

		QPushButton *group = nullptr;
		QPushButton *ungroup = nullptr;
	};
}

/*
	Grouping and ungrouping: a terminal of several levels is made of one
	symbol per level, joined with the Group button.

	Which terminal received the others - and so stayed level 0 - came from
	a QSet of rows: hash order, a different one from run to run. The same
	selection grouped as 5.2/5.1 on the screen, with 5.1 clicked first.
	The rule now is the table's: the topmost row is level 0 and the others
	follow it, whatever order they were selected in. The selections below
	are made bottom-up on purpose.

	Undo and Ungroup used to give every level a position of its own at the
	end of the strip: undoing a group moved the terminal from position 5
	to 7, and a level taken out of a group landed far from it. Undo now
	gives back the positions of before, and Ungroup leaves the level right
	after its group.
*/
TEST_CASE("T33 — Agrupar: o borne mais acima na tabela fica no nível 0",
	  "[terminalstrip][group]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripgroup.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(rail(strip) == original());

	GroupEditor ui(project, strip);
	REQUIRE(ui.isComplete());
	REQUIRE(ui.group != nullptr);

	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);
	const int before = stack->count();

	selectLabels(ui.table, {QStringLiteral("3"), QStringLiteral("1"), QStringLiteral("2")});
	const auto selected = ui.table->selectionModel()->selectedIndexes();
	REQUIRE(selected.size() == 3);
	REQUIRE(ui.model->index(selected.first().row(), TerminalStripModel::Label).data().toString()
		== QStringLiteral("3"));
	REQUIRE(ui.group->isEnabled());

	ui.group->click();

	REQUIRE(stack->count() == before + 1);
	CHECK(rail(strip) == QStringList({QStringLiteral("1/2/3"), QStringLiteral("4"),
					  QStringLiteral("DJ0"), QStringLiteral("DJ1")}));
	CHECK(bench["1"]->level() == 0);
	CHECK(bench["2"]->level() == 1);
	CHECK(bench["3"]->level() == 2);
	CHECK(renamedTerminals(bench).isEmpty());

	SECTION("a caixa Estágio mostra o estágio da linha escolhida, sem gravá-lo")
	{
		QSpinBox *level = ui.editor.findChild<QSpinBox *>(QStringLiteral("m_level_sb"));
		REQUIRE(level != nullptr);

		selectLabels(ui.table, {QStringLiteral("3")});
		REQUIRE(level->isEnabled());
		CHECK(level->value() == 2);

		selectLabels(ui.table, {QStringLiteral("2")});
		CHECK(level->value() == 1);

		selectLabels(ui.table, {QStringLiteral("1")});
		CHECK(level->value() == 0);

			//Showing is not choosing: nothing is pending, nothing moved
		const auto model = qobject_cast<TerminalStripModel *>(ui.model);
		REQUIRE(model != nullptr);
		CHECK(model->modifiedmodelRealTerminalData().isEmpty());
		CHECK(rail(strip) == QStringList({QStringLiteral("1/2/3"), QStringLiteral("4"),
						  QStringLiteral("DJ0"), QStringLiteral("DJ1")}));
		CHECK(stack->count() == before + 1);
	}

	SECTION("desfazer devolve os três às posições 1, 2 e 3")
	{
		stack->undo();
		CHECK(rail(strip) == original());
		CHECK(labelsShown(ui.model) == original());
	}

	SECTION("desagrupar o 3 deixa-o logo depois do grupo, e não no fim da régua")
	{
		REQUIRE(ui.ungroup != nullptr);
		selectLabels(ui.table, {QStringLiteral("3")});
		REQUIRE(ui.ungroup->isEnabled());

		ui.ungroup->click();

		REQUIRE(stack->count() == before + 2);
		CHECK(rail(strip) == QStringList({QStringLiteral("1/2"), QStringLiteral("3"),
						  QStringLiteral("4"), QStringLiteral("DJ0"),
						  QStringLiteral("DJ1")}));
	}
}

TEST_CASE("T33 — Desfazer o agrupamento de bornes que não são vizinhos devolve cada um ao seu lugar",
	  "[terminalstrip][group]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("stripgroup.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const Bench bench = benchStrip(project);
	TerminalStrip *strip = bench.strip;
	REQUIRE(strip != nullptr);
	REQUIRE(rail(strip) == original());

	GroupEditor ui(project, strip);
	REQUIRE(ui.isComplete());
	REQUIRE(ui.group != nullptr);
	QUndoStack *stack = project->undoStack();
	REQUIRE(stack != nullptr);

	selectLabels(ui.table, {QStringLiteral("DJ0"), QStringLiteral("2")});
	REQUIRE(ui.group->isEnabled());

	ui.group->click();

	REQUIRE(rail(strip) == QStringList({QStringLiteral("1"), QStringLiteral("2/DJ0"),
					    QStringLiteral("3"), QStringLiteral("4"),
					    QStringLiteral("DJ1")}));

	stack->undo();

	CHECK(rail(strip) == original());
	CHECK(labelsShown(ui.model) == original());

	stack->redo();

	CHECK(rail(strip) == QStringList({QStringLiteral("1"), QStringLiteral("2/DJ0"),
					  QStringLiteral("3"), QStringLiteral("4"),
					  QStringLiteral("DJ1")}));
}
