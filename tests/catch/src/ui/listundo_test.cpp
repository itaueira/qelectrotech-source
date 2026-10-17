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

#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/projectdbmodel.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"

#include <catch2/catch.hpp>

#include <QObject>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QUuid>

/*
	A component deleted and brought back by one undo, with the list of the
	project drawn on a folio beside it.

	The list on the folio is a ProjectDBModel that re-runs its query each
	time the data base of the project announces a change, and a table item
	that lays itself out again each time the model resets. Deleting a
	component removes its rows and announces; the undo puts the very same
	component back, with the very same uuid, and has to announce too.

	It did not, and for two reasons that fed each other. Deleting removed
	the row of the component from the table "element" and left its row of
	"element_info" behind; the undo then inserted into "element_info" under
	a primary key that was still taken, the insertion failed, and the
	announcement sat in the branch that only ran when it succeeded. The
	component came back on the drawing and not in the list, and the list
	only caught up at the next change of anything, or with the button that
	reloads it.

	Both halves are measured here, each where it can be seen:

	- the rows of the data base, read straight from the tables, so that a
	  row left behind by the deletion is seen as a row and not as a list
	  that happens to look right;
	- the list itself, read from the model the table on the folio draws -
	  the one built before anything happened, which nobody touches between
	  the undo and the reading. A model built after the undo would prove the
	  data base and say nothing about whether the list was told.

	The table is built the way QetGraphicsTableFactory::newTable() builds
	it - a model owned by the project, a table added to the folio - and not
	through AddTableDialog, which is modal and has nobody to answer it here.
*/

namespace
{
	struct Component
	{
		const char *uuid;
		int x;
		const char *label;
		const char *designation;
	};

	/// The three components of the drawing, in the order of their labels.
	const Component components[] = {
		{"{16130000-0000-4000-8000-000000000001}", 200, "K1", "CONTACTOR"},
		{"{16130000-0000-4000-8000-000000000002}", 320, "K2", "BREAKER"},
		{"{16130000-0000-4000-8000-000000000003}", 440, "K3", "PUSH BUTTON"}};

	/// The list on the folio: one row per component, in label order.
	const QString list_query = QStringLiteral(
		"SELECT label, designation FROM element_nomenclature_view"
		" ORDER BY label");

	/**
		Two folios: the drawing, with three components, and an empty one
		for the list - the list is usually not on the folio it lists.
	*/
	QString fixtureXml()
	{
			//The docking point of a terminal, which is what the instance
			//stores.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString instances;
		int index = 0;
		for (const Component &component : components)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/box.elmt\" uuid=\"%2\">"
					     "<terminals>"
					     "<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
					     "<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%7"
					     "</elementInformation>"
					     "<elementInformation show=\"1\" name=\"designation\">%8"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(component.x)
				     .arg(QLatin1String(component.uuid))
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(QString::fromLatin1(component.label),
					  QString::fromLatin1(component.designation));
			++index;
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
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "%1%2"
			       "</project>")
		       .arg(folio.arg(QStringLiteral("Drawing"), QStringLiteral("1"), instances),
			    folio.arg(QStringLiteral("List"), QStringLiteral("2"), QString()));
	}

	Element *elementWithUuid(Diagram *folio, const char *uuid)
	{
		if (!folio) {
			return nullptr;
		}
		const QList<Element *> elements = folio->elements();
		for (Element *element : elements)
		{
			if (element->uuid() == QUuid(QLatin1String(uuid))) {
				return element;
			}
		}
		return nullptr;
	}

	/// The list put on a folio: the model it draws and the table drawing it.
	struct FolioList
	{
		ProjectDBModel *model = nullptr;
		QetGraphicsTableItem *table = nullptr;
	};

	/**
		The list on @a folio, built as QetGraphicsTableFactory::newTable()
		builds it: the model is a child of the project, the table belongs
		to the folio it is added to, and both go when the project goes.
	*/
	FolioList putListOn(QETProject *project, Diagram *folio)
	{
		FolioList list;
		list.model = new ProjectDBModel(project, project);
		list.model->setQuery(list_query);
		list.table = new QetGraphicsTableItem();
		list.table->setModel(list.model);
		folio->addItem(list.table);
		return list;
	}

	/// The label column of @a model, row after row, as the list shows it.
	QStringList labelsOf(const ProjectDBModel &model)
	{
		QStringList labels;
		const int column = model.columnNames().indexOf(QStringLiteral("label"));
		if (column < 0) {
			return QStringList({QStringLiteral("<no label column>")});
		}
		for (int row = 0 ; row < model.rowCount() ; ++row) {
			labels << model.index(row, column).data().toString();
		}
		return labels;
	}

	/**
		How many rows of @a table carry @a uuid in @a column, read straight
		from the data base of the project: -1 when the query did not run.
	*/
	int rowsFor(QETProject *project,
		    const QString &table,
		    const QString &column,
		    const QUuid &uuid)
	{
		QSqlQuery query = project->dataBase()->newQuery(
					QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2 = '%3'")
					.arg(table, column, uuid.toString()));
		if (!query.isActive() || !query.next()) {
			return -1;
		}
		return query.value(0).toInt();
	}

	int elementRows(QETProject *project, const QUuid &uuid)
	{
		return rowsFor(project, QStringLiteral("element"),
			       QStringLiteral("uuid"), uuid);
	}

	int informationRows(QETProject *project, const QUuid &uuid)
	{
		return rowsFor(project, QStringLiteral("element_info"),
			       QStringLiteral("element_uuid"), uuid);
	}

	/**
		Delete @a element the way the folio deletes its selection
		(QETDiagramEditor::selectionGroupTriggered(), "delete_selection"):
		the selection is taken, cleared, and handed to one command pushed
		on the stack of the project.
	*/
	void deleteTheWayTheFolioDoes(Diagram *folio, Element *element)
	{
		folio->clearSelection();
		element->setSelected(true);
		DiagramContent selection(folio);
		folio->clearSelection();
		folio->undoStack().push(new DeleteQGraphicsItemCommand(folio, selection));
	}
}

TEST_CASE("CU-16.13 — desfazer a exclusão de um componente o devolve ao desenho e à lista na folha, no mesmo passo",
	  "[uibench][listengine][t16]")
{
	UiBench::ScratchProject scratch(fixtureXml(), QStringLiteral("listundo.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagramCount() == 2);

	QETProject *project = scratch.project();
	Diagram *drawing = scratch.diagram(0);
	Diagram *list_folio = scratch.diagram(1);
	REQUIRE(drawing->elements().count() == 3);

	Element *k2 = elementWithUuid(drawing, components[1].uuid);
	REQUIRE(k2 != nullptr);
	const QUuid k2_uuid = k2->uuid();

	const FolioList list = putListOn(project, list_folio);
	INFO("query error: " << list.model->lastError().toStdString());
	REQUIRE(list.model->lastError().isEmpty());
	const int row_height = list.table->minimumRowHeight();
	REQUIRE(row_height > 0);

		//A second list, to be cut off from the data base once the deletion
		//has reached it: the list the defect left behind. Built here, before
		//anything is deleted, because building a model runs updateDB(), and a
		//full rebuild between the deletion and the undo would wipe exactly
		//the row the deletion may have left behind.
	ProjectDBModel deaf(project);
	deaf.setQuery(list_query);

		//What the list says before anything happens, written down here and
		//not read back from the program.
	REQUIRE(labelsOf(*list.model)
		== QStringList({QStringLiteral("K1"), QStringLiteral("K2"), QStringLiteral("K3")}));
	REQUIRE(list.table->displayedRowCount() == 3);
	REQUIRE(elementRows(project, k2_uuid) == 1);
	REQUIRE(informationRows(project, k2_uuid) == 1);

	QUndoStack &stack = drawing->undoStack();
	const int steps_before = stack.count();

	deleteTheWayTheFolioDoes(drawing, k2);

	SECTION("apagar tira o componente do desenho, da lista e do bloco na folha, e não deixa linha para trás")
	{
		CHECK_FALSE(drawing->elements().contains(k2));
		CHECK(stack.count() == steps_before + 1);
			//The step undone below is the deletion of one component, and
			//not of something else that happened to be selected.
		CHECK(UiBench::undoTopText(project) == QStringLiteral("delete 1 elements"));

		CHECK(labelsOf(*list.model)
		      == QStringList({QStringLiteral("K1"), QStringLiteral("K3")}));
		CHECK(list.table->displayedRowCount() == 2);
		CHECK(list.table->size().height() == 2 * row_height);

			//Both rows of the component go, and not only the one of the
			//table "element": the row of "element_info" left behind is the
			//one the undo used to collide with.
		CHECK(elementRows(project, k2_uuid) == 0);
		CHECK(informationRows(project, k2_uuid) == 0);
	}

	SECTION("um desfazer devolve o componente ao desenho e à lista, sem recarregar nada")
	{
		REQUIRE(labelsOf(*list.model)
			== QStringList({QStringLiteral("K1"), QStringLiteral("K3")}));
		REQUIRE(labelsOf(deaf)
			== QStringList({QStringLiteral("K1"), QStringLiteral("K3")}));
		QObject::disconnect(project->dataBase(), nullptr, &deaf, nullptr);

		stack.undo();

		CHECK(drawing->elements().contains(k2));
		CHECK(stack.count() == steps_before + 1);

			//The list on the folio, the very model built before the
			//deletion, read with nothing done in between: no other change
			//to the project, no reload, no new query.
		CHECK(labelsOf(*list.model)
		      == QStringList({QStringLiteral("K1"), QStringLiteral("K2"), QStringLiteral("K3")}));
		CHECK(list.table->displayedRowCount() == 3);
		CHECK(list.table->size().height() == 3 * row_height);

		CHECK(elementRows(project, k2_uuid) == 1);
		CHECK(informationRows(project, k2_uuid) == 1);

			//Negative control: a list that was not told about the undo is
			//still where the deletion left it. This is what tells the line
			//above apart from a model that would read the data base on its
			//own: the two were built the same way and differ only in
			//having heard the undo.
		CHECK(labelsOf(deaf)
		      == QStringList({QStringLiteral("K1"), QStringLiteral("K3")}));

			//And the project agrees with the list: a model built now,
			//after every reading above, says the same three.
		ProjectDBModel witness(project);
		witness.setQuery(list_query);
		CHECK(labelsOf(witness)
		      == QStringList({QStringLiteral("K1"), QStringLiteral("K2"), QStringLiteral("K3")}));
	}

	SECTION("apagar de novo pelo refazer e desfazer outra vez não deixa resíduo")
	{
		stack.undo();
		REQUIRE(labelsOf(*list.model)
			== QStringList({QStringLiteral("K1"), QStringLiteral("K2"), QStringLiteral("K3")}));

		stack.redo();
		CHECK_FALSE(drawing->elements().contains(k2));
		CHECK(labelsOf(*list.model)
		      == QStringList({QStringLiteral("K1"), QStringLiteral("K3")}));
		CHECK(elementRows(project, k2_uuid) == 0);
		CHECK(informationRows(project, k2_uuid) == 0);

		stack.undo();
		CHECK(drawing->elements().contains(k2));
		CHECK(labelsOf(*list.model)
		      == QStringList({QStringLiteral("K1"), QStringLiteral("K2"), QStringLiteral("K3")}));
		CHECK(list.table->displayedRowCount() == 3);
		CHECK(list.table->size().height() == 3 * row_height);
		CHECK(elementRows(project, k2_uuid) == 1);
		CHECK(informationRows(project, k2_uuid) == 1);
	}
}

TEST_CASE("T16 — apagar e desfazer um componente custa um aviso do banco e uma consulta da lista, em cada sentido",
	  "[uibench][listengine][t16]")
{
	UiBench::ScratchProject scratch(fixtureXml(), QStringLiteral("listundo.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	QETProject *project = scratch.project();
	Diagram *drawing = scratch.diagram(0);
	Diagram *list_folio = scratch.diagram(1);
	REQUIRE(drawing != nullptr);
	REQUIRE(list_folio != nullptr);

	Element *k2 = elementWithUuid(drawing, components[1].uuid);
	REQUIRE(k2 != nullptr);

	const FolioList list = putListOn(project, list_folio);
	REQUIRE(list.model->lastError().isEmpty());
	REQUIRE(list.model->rowCount() == 3);

		//Each run of the query of the model ends in one of these two
		//signals and never in both: a reset when the number of rows
		//changed, dataChanged when it did not. Counting both counts the
		//runs. The guard is declared after the counter so that it dies
		//first and takes the connections with it: the project closing at
		//the end of the case notifies the model once more.
	int runs = 0;
	QObject guard;
	QObject::connect(list.model, &QAbstractItemModel::modelReset, &guard, [&runs]() {++runs;});
	QObject::connect(list.model, &QAbstractItemModel::dataChanged, &guard, [&runs]() {++runs;});

	QSignalSpy notices(project->dataBase(), &projectDataBase::dataBaseUpdated);
	REQUIRE(notices.isValid());

	SECTION("apagar: um aviso, uma consulta")
	{
		deleteTheWayTheFolioDoes(drawing, k2);
		REQUIRE(list.model->rowCount() == 2);
		CHECK(notices.count() == 1);
		CHECK(runs == 1);
	}

	SECTION("desfazer: um aviso, uma consulta")
	{
		deleteTheWayTheFolioDoes(drawing, k2);
		REQUIRE(list.model->rowCount() == 2);
		notices.clear();
		runs = 0;

		drawing->undoStack().undo();
		REQUIRE(list.model->rowCount() == 3);
		CHECK(notices.count() == 1);
		CHECK(runs == 1);
	}
}
