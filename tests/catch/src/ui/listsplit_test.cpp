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
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/projectdbmodel.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicsheaderitem.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"

#include <catch2/catch.hpp>

#include <QStringList>
#include <QUndoStack>

/*
	A list split over three folios, one part of it deleted (T16, step 10).

	Ninety components listed thirty to a folio, the way the table factory
	splits a list that does not fit: three tables, each on its own folio,
	linked one to the next and sharing one model. Deleting one part used to
	take the whole list off every folio; now the part goes alone, the parts
	before and after it are joined, and one of them takes over its rows -
	the one before it, or the one after it when the first part goes - so
	that the list still shows the ninety, in order. Undoing puts the part
	back between the same neighbours, with the same limits.

	What is read is what the folios show: for each part, the labels of the
	rows it draws, worked out the way the table works them out when it
	paints - from the rows the parts before it show, and its own count.
*/
namespace
{
	const int component_count = 90;
	const int per_folio = 30;

	const QString list_query = QStringLiteral(
		"SELECT label, designation FROM element_nomenclature_view"
		" ORDER BY label");

	QString labelOf(int index)
	{
		return QStringLiteral("K%1").arg(index + 1, 2, 10, QLatin1Char('0'));
	}

		/// One folio with the ninety components, and three empty ones for the list
	QString fixtureXml()
	{
		QString instances;
		for (int i = 0 ; i < component_count ; ++i)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/box.elmt\""
					     " uuid=\"{16140000-0000-4000-8000-0000000000%3}\">"
					     "<terminals/><inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%4"
					     "</elementInformation>"
					     "<elementInformation show=\"1\" name=\"designation\">RELAY"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(50 + (i % 15) * 50)
				     .arg(80 + (i / 15) * 80)
				     .arg(i, 2, 10, QLatin1Char('0'))
				     .arg(labelOf(i));
		}

		const QString folio = QStringLiteral(
			"<diagram title=\"%1\" order=\"%2\" height=\"600\""
			" cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%3</elements>"
			"<inputs/>"
			"<conductors/>"
			"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"30\" height=\"20\""
			       " hotspot_x=\"15\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "%1%2%3%4"
			       "</project>")
		       .arg(folio.arg(QStringLiteral("Drawing"), QStringLiteral("1"), instances),
			    folio.arg(QStringLiteral("List 1"), QStringLiteral("2"), QString()),
			    folio.arg(QStringLiteral("List 2"), QStringLiteral("3"), QString()),
			    folio.arg(QStringLiteral("List 3"), QStringLiteral("4"), QString()));
	}

	struct SplitList
	{
		ProjectDBModel *model = nullptr;
		QList<QetGraphicsTableItem *> parts;
	};

		/// The list split thirty to a folio over folios 2, 3 and 4, as the factory links it
	SplitList splitList(UiBench::ScratchProject &bench)
	{
		SplitList list;
		QETProject *project = bench.project();
		list.model = new ProjectDBModel(project, project);
		list.model->setQuery(list_query);
		QetGraphicsTableItem *previous = nullptr;
		for (int folio = 1 ; folio <= 3 ; ++folio)
		{
			auto *table = new QetGraphicsTableItem();
			if (previous) {
				table->setPreviousTable(previous);
			} else {
				table->setModel(list.model);
			}
			table->setDisplayNRow(per_folio);
			bench.diagram(folio)->addItem(table);
			list.parts << table;
			previous = table;
		}
		return list;
	}

		/// The labels @a table draws, as it works them out to paint
	QStringList shown(QetGraphicsTableItem *table)
	{
		QStringList labels;
		auto *model = table ? table->model() : nullptr;
		if (!model || !table->scene()) {
			return labels;
		}
		const int first = table->previousTable() ? table->previousTable()->displayNRowOffset() : 0;
		const int count = table->displayedRowCount();
		for (int row = first ; row < first + count ; ++row) {
			labels << model->index(row, 0).data().toString();
		}
		return labels;
	}

	QStringList everyLabel()
	{
		QStringList labels;
		for (int i = 0 ; i < component_count ; ++i) {
			labels << labelOf(i);
		}
		return labels;
	}

		/// Delete @a table the way the folio deletes its selection
	void deleteTheWayTheFolioDoes(QetGraphicsTableItem *table)
	{
		Diagram *folio = qobject_cast<Diagram *>(table->scene());
		REQUIRE(folio);
		folio->clearSelection();
		table->setSelected(true);
		DiagramContent selection(folio);
		folio->clearSelection();
		REQUIRE(selection.m_tables.size() == 1);
		folio->undoStack().push(new DeleteQGraphicsItemCommand(folio, selection));
	}
}

TEST_CASE("CU-16.14 — apagar uma divisão do meio e desfazer",
	  "[uibench][t16][listsplit]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("listsplit.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 4);

	const SplitList list = splitList(bench);
	REQUIRE(list.model->rowCount() == component_count);
	QetGraphicsTableItem *first = list.parts.at(0);
	QetGraphicsTableItem *middle = list.parts.at(1);
	QetGraphicsTableItem *last = list.parts.at(2);

		//Before: thirty to a folio, the ninety in order
	const QStringList all = everyLabel();
	const QStringList first_rows = shown(first);
	const QStringList middle_rows = shown(middle);
	const QStringList last_rows = shown(last);
	REQUIRE(first_rows == all.mid(0, 30));
	REQUIRE(middle_rows == all.mid(30, 30));
	REQUIRE(last_rows == all.mid(60, 30));

	Diagram *middle_folio = bench.diagram(2);
	QUndoStack &stack = middle_folio->undoStack();
	deleteTheWayTheFolioDoes(middle);

	SECTION("the two neighbours are joined, and the list is whole")
	{
		CHECK(middle->scene() == nullptr);
		CHECK(first->nextTable() == last);
		CHECK(last->previousTable() == first);
		CHECK(shown(first) + shown(last) == all);
		CHECK(shown(first).size() == 60);
		CHECK(shown(last) == last_rows);
			//A header on each folio that still has a part
		CHECK(first->headerItem()->scene() == bench.diagram(1));
		CHECK(last->headerItem()->scene() == bench.diagram(3));
	}

	SECTION("undone, the three parts are back, with the same limits")
	{
		stack.undo();
		CHECK(middle->scene() == middle_folio);
		CHECK(first->nextTable() == middle);
		CHECK(middle->previousTable() == first);
		CHECK(middle->nextTable() == last);
		CHECK(last->previousTable() == middle);
		CHECK(shown(first) == first_rows);
		CHECK(shown(middle) == middle_rows);
		CHECK(shown(last) == last_rows);
		CHECK(first->displayNRow() == per_folio);
		CHECK(middle->displayNRow() == per_folio);

		SECTION("and done and undone again, nothing is left over")
		{
			stack.redo();
			CHECK(shown(first) + shown(last) == all);
			stack.undo();
			CHECK(shown(first) == first_rows);
			CHECK(shown(middle) == middle_rows);
			CHECK(shown(last) == last_rows);
		}
	}
}

TEST_CASE("T16 — apagar a primeira ou a última divisão também junta a lista",
	  "[uibench][t16][listsplit]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("listends.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	const SplitList list = splitList(bench);
	QetGraphicsTableItem *first = list.parts.at(0);
	QetGraphicsTableItem *middle = list.parts.at(1);
	QetGraphicsTableItem *last = list.parts.at(2);
	const QStringList all = everyLabel();

	SECTION("the first: the next one takes over from the first row")
	{
		deleteTheWayTheFolioDoes(first);
		CHECK(middle->previousTable() == nullptr);
		CHECK(middle->model() == list.model);
		CHECK(shown(middle) + shown(last) == all);
		CHECK(shown(middle).size() == 60);

		bench.diagram(1)->undoStack().undo();
		CHECK(shown(first) == all.mid(0, 30));
		CHECK(shown(middle) == all.mid(30, 30));
		CHECK(shown(last) == all.mid(60, 30));
	}

	SECTION("the last: the one before it takes over its rows")
	{
		deleteTheWayTheFolioDoes(last);
		CHECK(middle->nextTable() == nullptr);
		CHECK(shown(first) + shown(middle) == all);
		CHECK(shown(middle).size() == 60);

		bench.diagram(3)->undoStack().undo();
		CHECK(shown(middle) == all.mid(30, 30));
		CHECK(shown(last) == all.mid(60, 30));
	}
}

TEST_CASE("T16 — a lista sem uma divisão continua inteira ao reabrir",
	  "[uibench][t16][listsplit]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("listsave.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	const SplitList list = splitList(bench);
	deleteTheWayTheFolioDoes(list.parts.at(1));
		//Whole before it is saved: a list that does not fit its parts
		//raises a modal warning when the project opens, and with no
		//screen nobody answers it - the case would hang instead of failing
	REQUIRE(shown(list.parts.at(0)) + shown(list.parts.at(2)) == everyLabel());
	REQUIRE(bench.saveAndReopen());

	QList<QetGraphicsTableItem *> parts;
	for (int folio = 1 ; folio <= 3 ; ++folio) {
		for (QGraphicsItem *item : bench.diagram(folio)->items()) {
			if (auto *table = qgraphicsitem_cast<QetGraphicsTableItem *>(item)) {
				parts << table;
			}
		}
	}
	REQUIRE(parts.size() == 2);
	QetGraphicsTableItem *head = parts.at(0)->previousTable() ? parts.at(1) : parts.at(0);
	QetGraphicsTableItem *tail = head->nextTable();
	REQUIRE(tail);
	CHECK(shown(head) + shown(tail) == everyLabel());
}
