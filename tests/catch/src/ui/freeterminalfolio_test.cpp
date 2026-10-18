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
#include "../../../../sources/TerminalStrip/terminalmove.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/ui/freeterminaleditor.h"
#include "../../../../sources/TerminalStrip/ui/freeterminalmodel.h"

#include <catch2/catch.hpp>

#include <QAbstractItemModel>
#include <QComboBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMimeData>
#include <QPoint>
#include <QPushButton>
#include <QScopedPointer>
#include <QSet>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QTest>
#include <QUndoStack>
#include <QUuid>
#include <QVector>

#include <algorithm>
#include <iterator>

/*
	The page of free terminals of the terminal strip manager, used to build
	a strip from what the folios say: the terminals of one folio, put on the
	strip in the order they are drawn in.

	Three things make that possible, and each has a defect it would have
	without the others:

	- the folio box, which keeps the terminals of one folio. The folio is
	  the one the element is drawn on, and never a piece of the
	  cross-reference text: "4-" is found in "14-D7" too, and the bench has
	  a folio 14 for that reason;
	- the sort on a column header, in natural order: the cross-references
	  of the bench cross from column 9 to column 10, where a plain text sort
	  puts "4-D10" before "4-D7";
	- the move button putting the terminals on the strip in the order the
	  table shows them, top to bottom, and not in the order the rows were
	  selected in.

	The folio 4 of the bench holds the tags 1, 2, 3, 1, 2, 3 left to right,
	and that also is on purpose: sorted by label, the table reads
	1, 1, 2, 2, 3, 3, and a move that took the order of the model instead of
	the order of the table would be told apart by the labels alone.

	Nothing here is compared to a cross-reference written by hand: the text
	depends on two settings of the program (the folio number or the folio
	position, the first column counted as 0 or as 1), and the bench reads it
	from the terminals themselves. The order it expects is the order of the
	abscissae of the elements, which is the drawing.
*/

namespace
{
	const int folio_count = 14;

		/// The folio the case of use chooses, 1 based as the folio box counts.
	const int chosen_folio = 4;

	struct BenchTerminal
	{
		int folio;		///< 1 based
		int column;		///< of the grid, 1 based as BorderTitleBlock counts
		const char *label;
	};

	const BenchTerminal bench_terminals[] = {
			//The case of use: two runs of 1, 2, 3 on one row of the grid,
			//from column 8 to column 13 - the first column counted as 0
			//or as 1, the numbers cross from one digit to two.
		{4,  8, "1"}, {4,  9, "2"}, {4, 10, "3"},
		{4, 11, "1"}, {4, 12, "2"}, {4, 13, "3"},
			//Tags folio 4 has too, on a folio whose number ends in 4.
		{14,  8, "1"}, {14,  9, "2"}, {14, 10, "3"},
		{14, 11, "4"}, {14, 12, "5"}, {14, 13, "6"}};

	const int bench_terminal_count = static_cast<int>(std::size(bench_terminals));

	/// The labels folio 4 holds, left to right.
	QStringList chosenFolioLabels()
	{
		return QStringList{QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
				   QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")};
	}

	/**
		The abscissa of the middle of grid column @a column, 1 based.

		The grid starts after the margin of the scene and the header of
		the rows (20, the default of BorderProperties), and a column is
		as wide as the colsize of the fixture.
	*/
	qreal columnMiddle(int column)
	{
		const qreal rows_header_width = 20.;
		const qreal column_width = 60.;
		return Diagram::margin + rows_header_width
				+ column_width * (column - 1) + column_width / 2.;
	}

	/**
		A project of fourteen folios, terminals on folios 4 and 14 only,
		every terminal on the same row of the grid, at the middle of its
		column. The folio field of each title block is "%id", so that the
		cross-references read "4-D7" and "14-D7" in the default settings:
		the very case where a text filter on "4-" would catch folio 14.

		The terminal symbol is the one of bridgebywire_test.cpp.
	*/
	QString fixtureXml()
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QVector<QString> instances(folio_count);
		for (int index = 0 ; index < bench_terminal_count ; ++index)
		{
			const BenchTerminal &terminal = bench_terminals[index];
			instances[terminal.folio - 1] += QStringLiteral(
					     "<element x=\"%1\" y=\"300\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/terminal.elmt\""
					     " uuid=\"{f01e0000-0000-4000-8000-%2}\">"
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
					     .arg(columnMiddle(terminal.column))
					     .arg(index, 12, 10, QLatin1Char('0'))
					     .arg(east_dock)
					     .arg(index * 2)
					     .arg(west_dock)
					     .arg(index * 2 + 1)
					     .arg(QLatin1String(terminal.label));
		}

		QString diagrams;
		for (int folio = 1 ; folio <= folio_count ; ++folio)
		{
				//One pass of arg() for the two values: the instances hold
				//no placeholder, and the "%id" of the folio field is none.
			diagrams += QStringLiteral(
					    "<diagram title=\"Sheet %1\" order=\"%1\" height=\"600\""
					    " folio=\"%id\""
					    " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
					    " displaycols=\"true\" displayrows=\"true\">"
					    "<elements>%2</elements>"
					    "<inputs/><conductors/>"
					    "</diagram>")
				    .arg(QString::number(folio), instances.at(folio - 1));
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
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "%1"
			       "</project>")
		       .arg(diagrams);
	}

	/// The terminal elements drawn on @a folio, left to right: the drawing.
	QVector<TerminalElement *> terminalsLeftToRight(Diagram *folio)
	{
		QVector<TerminalElement *> terminals;
		const auto elements = folio->elements();
		for (const auto element : elements)
		{
			if (element->elementData().m_type == ElementData::Terminal) {
				terminals.append(static_cast<TerminalElement *>(element));
			}
		}

		std::sort(terminals.begin(), terminals.end(),
			  [](TerminalElement *a, TerminalElement *b) {
				return a->pos().x() < b->pos().x();
			  });
		return terminals;
	}

	/// The cross-reference of each of @a terminals, in their order.
	QStringList crossReferences(const QVector<TerminalElement *> &terminals)
	{
		QStringList xrefs;
		for (const auto terminal : terminals) {
			xrefs << terminal->realTerminal()->Xref();
		}
		return xrefs;
	}

	/// The uuid of each of @a terminals, in their order, as a drag carries it.
	QStringList elementUuids(const QVector<TerminalElement *> &terminals)
	{
		QStringList uuids;
		for (const auto terminal : terminals) {
			uuids << terminal->uuid().toString();
		}
		return uuids;
	}

	/// The text of @a column in every row of @a model, top to bottom.
	QStringList columnText(const QAbstractItemModel *model, int column)
	{
		QStringList texts;
		for (int row = 0 ; row < model->rowCount() ; ++row) {
			texts << model->index(row, column).data().toString();
		}
		return texts;
	}

	/// The first level of each terminal of @a strip, in the order of the strip.
	QVector<QSharedPointer<RealTerminal>> stripTerminals(TerminalStrip *strip)
	{
		QVector<QSharedPointer<RealTerminal>> terminals;
		const auto physicals = strip->physicalTerminal();
		for (const auto &physical : physicals)
		{
			const auto levels = physical->realTerminals();
			if (!levels.isEmpty()) {
				terminals.append(levels.first());
			}
		}
		return terminals;
	}

	QStringList stripCrossReferences(TerminalStrip *strip)
	{
		QStringList xrefs;
		for (const auto &real_t : stripTerminals(strip)) {
			xrefs << real_t->Xref();
		}
		return xrefs;
	}

	QStringList stripLabels(TerminalStrip *strip)
	{
		QStringList labels;
		for (const auto &real_t : stripTerminals(strip)) {
			labels << real_t->label();
		}
		return labels;
	}

	/// The element uuids a drag of @a indexes of @a model carries, in order.
	QStringList draggedUuids(const QAbstractItemModel *model, const QModelIndexList &indexes)
	{
		QStringList uuids;
		QScopedPointer<QMimeData> mime_data(model->mimeData(indexes));
		for (const auto &uuid : FreeTerminalModel::uuidsFromMimeData(mime_data.data())) {
			uuids << uuid.toString();
		}
		return uuids;
	}

	/**
		One click of the mouse in the middle of the header of @a section,
		as a person sorts the table. Stops the case when the click did not
		reach the header, so that a failure is not read as a wrong order.
	*/
	void clickHeader(QTableView *table, int section)
	{
		QHeaderView *header = table->horizontalHeader();
		REQUIRE(header != nullptr);
		REQUIRE(header->sectionSize(section) > 0);

		const QPoint middle(header->sectionViewportPosition(section)
				    + header->sectionSize(section) / 2,
				    header->viewport()->height() / 2);
		QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, middle);

		REQUIRE(header->sortIndicatorSection() == section);
	}

	/// The widgets of the page the cases drive, found the way the form names them.
	struct Page
	{
		explicit Page(FreeTerminalEditor &editor) :
			table(editor.findChild<QTableView *>(QStringLiteral("m_table_view"))),
			folio_box(editor.findChild<QComboBox *>(QStringLiteral("m_folio_cb"))),
			filter(editor.findChild<QLineEdit *>(QStringLiteral("m_filter_le"))),
			move_to(editor.findChild<QComboBox *>(QStringLiteral("m_move_in_cb"))),
			move(editor.findChild<QPushButton *>(QStringLiteral("m_move_pb")))
		{}

		bool isComplete() const {
			return table && table->model() && folio_box && filter && move_to && move;
		}

		QAbstractItemModel *rows() const {return table->model();}

		QTableView *table;
		QComboBox *folio_box;
		QLineEdit *filter;
		QComboBox *move_to;
		QPushButton *move;
	};
}

TEST_CASE("CU-33.13 — os bornes de uma folha, na ordem do desenho",
	  "[terminalstrip][freeterminal]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("freeterminalfolio.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == folio_count);

	Diagram *folio_4 = bench.diagram(chosen_folio - 1);
	REQUIRE(folio_4 != nullptr);
	const auto drawn = terminalsLeftToRight(folio_4);
	REQUIRE(drawn.size() == 6);
	const QStringList drawing_order = crossReferences(drawn);
	INFO("cross-references of folio 4, left to right: "
	     << drawing_order.join(QStringLiteral(", ")).toStdString());

		//What the bench rests on: six places of the grid, all distinct,
		//which a plain text sort puts in another order than the drawing.
		//Without the second, a sort that is not natural would pass.
	REQUIRE(QSet<QString>(drawing_order.cbegin(), drawing_order.cend()).size() == 6);
	QStringList text_order = drawing_order;
	std::sort(text_order.begin(), text_order.end());
	REQUIRE(text_order != drawing_order);

	TerminalStrip *x1 = bench->newTerminalStrip(QString(), QString(), QStringLiteral("X1"));
	REQUIRE(x1 != nullptr);

	FreeTerminalEditor editor(bench.project());
	editor.reload();
	editor.resize(900, 500);
	editor.show();
	REQUIRE(QTest::qWaitForWindowExposed(&editor));

	Page page(editor);
	REQUIRE(page.isComplete());
	QAbstractItemModel *rows = page.rows();
	REQUIRE(rows->rowCount() == bench_terminal_count);

		//Every folio of the project, after the entry for all of them,
		//and the folio named as its title block numbers it.
	REQUIRE(page.folio_box->count() == folio_count + 1);
	CHECK(page.folio_box->itemText(chosen_folio) == QStringLiteral("4 - Sheet 4"));

		//Folio: 4
	page.folio_box->setCurrentIndex(page.folio_box->findData(folio_4->uuid()));
	REQUIRE(page.folio_box->currentIndex() == chosen_folio);

	SECTION("a caixa Folha: 4 mostra só os seis bornes da folha 4")
	{
		const QStringList shown = columnText(rows, FreeTerminalModel::XRef);
		CHECK(shown.size() == 6);
		CHECK(QSet<QString>(shown.cbegin(), shown.cend())
		      == QSet<QString>(drawing_order.cbegin(), drawing_order.cend()));

			//Before any column is clicked, the table is in natural order
			//of label, as it always was.
		CHECK(columnText(rows, FreeTerminalModel::Label)
		      == QStringList({QStringLiteral("1"), QStringLiteral("1"),
				      QStringLiteral("2"), QStringLiteral("2"),
				      QStringLiteral("3"), QStringLiteral("3")}));

		page.folio_box->setCurrentIndex(0);
		CHECK(rows->rowCount() == bench_terminal_count);
	}

	SECTION("ordenar por referência cruzada, selecionar tudo e Mover põe na X1 os seis na ordem da coluna, e um Ctrl+Z os devolve a livres")
	{
		clickHeader(page.table, FreeTerminalModel::XRef);
		REQUIRE(page.table->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder);

		const QStringList column = columnText(rows, FreeTerminalModel::XRef);
		CHECK(column == drawing_order);
		CHECK(columnText(rows, FreeTerminalModel::Label) == chosenFolioLabels());

		page.table->selectAll();
		const int x1_entry = page.move_to->findData(x1->uuid());
		REQUIRE(x1_entry >= 0);
		page.move_to->setCurrentIndex(x1_entry);
		REQUIRE(page.move->isEnabled());

		QUndoStack *stack = bench->undoStack();
		REQUIRE(stack != nullptr);
		const int before = stack->count();
		const QString destination = page.move_to->currentText();
		QSignalSpy messages(&editor, &FreeTerminalEditor::message);

		page.move->click();

		REQUIRE(x1->physicalTerminalCount() == 6);
		CHECK(stripCrossReferences(x1) == column);
		CHECK(stripLabels(x1) == chosenFolioLabels());

		REQUIRE(stack->count() == before + 1);
		CHECK(UiBench::undoTopText(bench.project())
		      == QObject::tr("Add %n terminals to terminal group %1", "", 6)
				 .arg(QStringLiteral("X1")));
		REQUIRE(messages.count() == 1);
		CHECK(messages.at(0).at(0).toString()
		      == TerminalMove::Decision::describeMove(6, destination));

			//The folio chosen stays chosen after the move, with nothing
			//left on it, and the six of folio 14 were not taken along.
		CHECK(page.folio_box->currentIndex() == chosen_folio);
		CHECK(rows->rowCount() == 0);
		page.folio_box->setCurrentIndex(0);
		CHECK(rows->rowCount() == bench_terminal_count - 6);
		page.folio_box->setCurrentIndex(chosen_folio);

		stack->undo();

		CHECK(stack->index() == before);
		CHECK(x1->physicalTerminalCount() == 0);
		for (const auto terminal : drawn) {
			CHECK(terminal->realTerminal()->parentStrip() == nullptr);
		}

			//Read anew, as the window does after an undo: the six are free
			//again, and the table keeps both the folio and the column it was
			//sorted on.
		editor.reload();
		CHECK(columnText(rows, FreeTerminalModel::XRef) == drawing_order);
	}

	SECTION("o filtro de texto continua casando a tag")
	{
		page.filter->setText(QStringLiteral("3"));
		CHECK(columnText(rows, FreeTerminalModel::Label)
		      == QStringList({QStringLiteral("3"), QStringLiteral("3")}));

			//The two of folio 4 and the one of folio 14
		page.folio_box->setCurrentIndex(0);
		CHECK(rows->rowCount() == 3);

			//Every cross-reference holds a dash, and no tag of the bench:
			//the text filter does not read the cross-reference.
		page.filter->setText(QStringLiteral("-"));
		CHECK(rows->rowCount() == 0);
	}
}

/*
	Labelled T33 and not CU-33.13: the case of use selects every row, and
	this case is the half of the rule a selection of every row cannot see.
	A selection lists its cells in the order it was made, so a selection of
	every row comes top to bottom and the order of the table and the order
	of the selection agree - the move button that followed the selection
	passed the case of use. Here the rows are picked from the bottom up.
*/
TEST_CASE("T33 — Mover e arrastar levam os bornes na ordem da tabela, e não na ordem em que as linhas foram selecionadas",
	  "[terminalstrip][freeterminal]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("freeterminalfolio.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *folio_4 = bench.diagram(chosen_folio - 1);
	REQUIRE(folio_4 != nullptr);
	const auto drawn = terminalsLeftToRight(folio_4);
	REQUIRE(drawn.size() == 6);

	TerminalStrip *x1 = bench->newTerminalStrip(QString(), QString(), QStringLiteral("X1"));
	REQUIRE(x1 != nullptr);

	FreeTerminalEditor editor(bench.project());
	editor.reload();
	Page page(editor);
	REQUIRE(page.isComplete());
	QAbstractItemModel *rows = page.rows();

	page.folio_box->setCurrentIndex(page.folio_box->findData(folio_4->uuid()));
	page.table->sortByColumn(FreeTerminalModel::XRef, Qt::AscendingOrder);
	REQUIRE(columnText(rows, FreeTerminalModel::XRef) == crossReferences(drawn));

	QItemSelectionModel *selection = page.table->selectionModel();
	REQUIRE(selection != nullptr);
		//Nothing left from before, so that the first range of the
		//selection is the first row picked below.
	selection->clearSelection();
	for (int row = rows->rowCount() - 1 ; row >= 0 ; --row) {
		selection->select(rows->index(row, 0),
				  QItemSelectionModel::Select | QItemSelectionModel::Rows);
	}

		//The bench rests on this: the selection lists the last row first.
		//Were it top to bottom, both orders would agree and nothing below
		//would tell them apart.
	const QModelIndexList picked = selection->selectedIndexes();
	REQUIRE(picked.size() == 6 * rows->columnCount());
	REQUIRE(picked.first().row() == rows->rowCount() - 1);

	SECTION("Mover põe os bornes na régua de cima para baixo da tabela")
	{
		const int x1_entry = page.move_to->findData(x1->uuid());
		REQUIRE(x1_entry >= 0);
		page.move_to->setCurrentIndex(x1_entry);
		REQUIRE(page.move->isEnabled());

		page.move->click();

		CHECK(stripCrossReferences(x1) == crossReferences(drawn));
		CHECK(stripLabels(x1) == chosenFolioLabels());
	}

	SECTION("o arrasto leva os bornes de cima para baixo da tabela")
	{
		CHECK(draggedUuids(rows, picked) == elementUuids(drawn));
	}
}
