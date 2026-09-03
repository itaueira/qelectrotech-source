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

#include "../../../../sources/QPropertyUndoCommand/qpropertyundocommand.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/elementsmover.h"
#include "../../../../sources/location/locatableelement.h"
#include "../../../../sources/location/locationtree.h"
#include "../../../../sources/location/projectlocation.h"
#include "../../../../sources/location/ui/locationmanagerdialog.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/locationareaitem.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/assignlocationcommand.h"
#include "../../../../sources/undocommand/movegraphicsitemcommand.h"

#include <catch2/catch.hpp>

#include <QPointF>
#include <QRectF>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoCommand>
#include <QUndoStack>

/*
	The live location rectangle, dragged, without a screen.

	Six push buttons inside a rectangle, one outside, and the count of the
	location manager going 6 - 5 - 6 as components are dragged across the
	border, with no manual assignment anywhere.

	What is driven here is the production path and not a shortcut of it:
	ElementsMover::beginMovement / continueMovement / endMovement is the very
	sequence the folio runs while the mouse is held down, and it is
	endMovement that asks the containment rule and builds the
	AssignLocationCommand. The count is read out of a real
	LocationManagerDialog, from the column a person reads, and not recomputed
	here - a rule that agrees with itself proves nothing about the window.

	One thing is deliberately left out, and it is the only one: the modal
	QInputDialog of LocationAreaItem::askForPath. A modal exec() under the
	offscreen platform does not fail, it waits for ever, so the case does what
	editProperty() does on either side of that dialog and skips the dialog
	itself. What that costs is stated where it happens, in assignArea() below.
*/

namespace {

	/// The location the six buttons end up in, as the storeroom reads it.
	const QString panel_name = QStringLiteral("Painel Remoto");
	/// The designation the same location carries on the drawing, and the path
	/// the components store: a code, not a name - see ProjectLocation.
	const QString panel_code = QStringLiteral("PR");

	/**
		The project the case works on, as text.

		Built here rather than taken from examples/ because no example ships
		six push buttons inside a rectangle, and because a fixture written by
		the case is a fixture whose expected numbers are not a guess about
		somebody elses drawing.

		The symbol has no terminal at all, on purpose. A terminal would let
		endMovement() reach the automatic conductor path, and a conductor
		appearing halfway through would change the undo captions this case
		measures without saying anything about location.
	*/
	QString fixtureXml()
	{
		struct Button
		{
			int x;
			int y;
			const char *label;
		};

		// Six inside the rectangle drawn below, one well outside it.
		const Button buttons[] = {
			{100, 100, "S1"},
			{160, 100, "S2"},
			{220, 100, "S3"},
			{100, 180, "S4"},
			{160, 180, "S5"},
			{220, 180, "S6"},
			{500, 300, "S7"}};

		QString instances;
		int index = 0;
		for (const Button &button : buttons)
		{
			++index;
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/pushbutton.elmt\""
					     " uuid=\"{c0ffee00-0000-4000-8000-00000000000%3}\">"
					     "<terminals/><inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%4"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(button.x)
				     .arg(button.y)
				     .arg(index)
				     .arg(QLatin1String(button.label));
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"pushbutton.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Push button</name></names>"
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
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(instances);
	}

	/// The push button carrying @a label, or nullptr when the sheet has none.
	Element *button(Diagram *sheet, const QString &label)
	{
		if (!sheet) {
			return nullptr;
		}

		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->elementInformations()
			    .value(QETInformation::ELMT_LABEL).toString() == label) {
				return element;
			}
		}
		return nullptr;
	}

	/**
		The components standing on @a path, by name, sorted.

		A list and not a count, so that a failure says which button is missing
		instead of only how many are. The counting rule is the one of the
		location manager: what is not a locatable element - a folio report, a
		thumbnail - is not a thing screwed down anywhere.
	*/
	QStringList componentsAt(QETProject *project, const QString &path)
	{
		QStringList names;
		if (!project) {
			return names;
		}

		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets)
		{
			const QList<Element *> elements = sheet->elements();
			for (Element *element : elements)
			{
				if (!isLocatableElement(element)) {
					continue;
				}
				const DiagramContext information = element->elementInformations();
				if (information.value(QETInformation::ELMT_LOCATION_PATH).toString()
				    != path) {
					continue;
				}
				names << information.value(QETInformation::ELMT_LABEL).toString();
			}
		}

		names.sort();
		return names;
	}

	/// The path a single component carries, empty when it carries none.
	QString pathOf(Element *element)
	{
		return element
		       ? element->elementInformations()
			 .value(QETInformation::ELMT_LOCATION_PATH).toString()
		       : QString();
	}

	/// The column of @a tree whose header reads @a title, -1 when there is none.
	int columnTitled(QTreeWidget *tree, const QString &title)
	{
		if (!tree || !tree->headerItem()) {
			return -1;
		}
		for (int column = 0 ; column < tree->columnCount() ; ++column)
		{
			if (tree->headerItem()->text(column) == title) {
				return column;
			}
		}
		return -1;
	}

	/**
		The number the location manager shows for the row named @a name.

		Read off the widget rather than recomputed, because the number the
		screen-test queue asks about is the one in that window. The two columns
		are found by their header text and not by index, so that a column added
		to the window does not silently move the answer.

		@return the count, -1 when no row is named @a name, -2 when the window
		has no such columns at all.
	*/
	int managerCount(LocationManagerDialog &dialog, const QString &name)
	{
		QTreeWidget *tree = dialog.findChild<QTreeWidget *>();
		if (!tree) {
			return -2;
		}

		const int name_column = columnTitled(tree, LocationManagerDialog::tr("Nom"));
		const int count_column = columnTitled(
					tree, LocationManagerDialog::tr("Composants"));
		if (name_column < 0 || count_column < 0) {
			return -2;
		}

		for (int row = 0 ; row < tree->topLevelItemCount() ; ++row)
		{
			QTreeWidgetItem *item = tree->topLevelItem(row);
			if (item->text(name_column) == name) {
				return item->text(count_column).toInt();
			}
		}
		return -1;
	}

	/**
		What LocationAreaItem::editProperty() does once the person has answered
		the dialog: write the path onto the area, ask the rule what follows
		from that, and push the two as one command.

		The dialog itself - askForPath, a modal QInputDialog - is the one thing
		this bench cannot run: a modal exec() under the offscreen platform
		waits for a click nobody is there to give, and the suite hangs instead
		of failing. So what is proved from here on is everything after the
		person pressed OK, and not that the dialog offers the right paths. That
		half stays with the human queue.
	*/
	void assignArea(LocationAreaItem *area, const QString &path)
	{
		REQUIRE(area != nullptr);
		Diagram *sheet = area->diagram();
		REQUIRE(sheet != nullptr);

		const QString old_path = area->locationPath();
		area->setLocationPath(path);

		auto *undo = new QUndoCommand(
				QStringLiteral("Modifier la localisation d'une zone"));
		new QPropertyUndoCommand(area, "locationPath", old_path, path, undo);

		const QList<LocationAssignment> assignments{
			LocationAreaItem::pendingAssignments(
				sheet, LocationAreaItem::areasOf(sheet))};
		if (!assignments.isEmpty())
		{
			new AssignLocationCommand(
				assignments,
				QStringLiteral("Affecter les composants d'une zone"),
				undo);
		}

		sheet->undoStack().push(undo);
	}

	/**
		One drag, from where the component is to @a target, through the very
		code the folio runs while the mouse is held down.

		The component is the only thing selected, because beginMovement() takes
		the selection and nothing else: whatever else were selected would
		travel with it.
	*/
	void dragTo(Diagram *sheet, Element *element, const QPointF &target)
	{
		REQUIRE(sheet != nullptr);
		REQUIRE(element != nullptr);

		sheet->clearSelection();
		element->setSelected(true);

		ElementsMover mover;
		REQUIRE(mover.beginMovement(sheet) == 1);
		mover.continueMovement(target - element->pos());
		mover.endMovement();

		sheet->clearSelection();
	}

	/// The one location rectangle of the sheet, nullptr when there is none.
	LocationAreaItem *areaOf(Diagram *sheet)
	{
		if (!sheet) {
			return nullptr;
		}
		const QList<QGraphicsItem *> items = sheet->items();
		for (QGraphicsItem *item : items)
		{
			if (auto *area = qgraphicsitem_cast<LocationAreaItem *>(item)) {
				return area;
			}
		}
		return nullptr;
	}

	// Where the rectangle is drawn, in scene coordinates. It holds the centres
	// of the first six buttons and none of the seventh - containment is judged
	// on the centre of a component, and a symbol of twenty units with its
	// hotspot in the middle has its centre exactly at its position.
	const QPointF area_corner_1{60., 60.};
	const QPointF area_corner_2{280., 220.};

	// Where the drags end. Outside is well past the right edge of the
	// rectangle; inside is a free spot between the two rows of buttons.
	const QPointF outside_target{500., 100.};
	const QPointF inside_target{250., 150.};
}

TEST_CASE("CU-32.2 (fila T-1) — o retangulo vivo: a contagem vai 6, 5, 6 sem atribuicao manual",
	  "[uibench][location]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("location-drag.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagramCount() == 1);

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 7);

	// The location exists in the project before anything is drawn, which is
	// the precondition the queue item states: a location registered in the
	// manager before the rectangle is put around the buttons.
	LocationTree tree;
	QString tree_error;
	const QString panel_uuid = tree.append(ProjectLocation(panel_code, panel_name),
					       &tree_error);
	INFO(tree_error.toStdString());
	REQUIRE_FALSE(panel_uuid.isEmpty());
	scratch.project()->setLocationTree(tree);

	auto *area = new LocationAreaItem(area_corner_1, area_corner_2);
	sheet->addItem(area);
	assignArea(area, panel_code);

	SECTION("a contagem comeca em 6, e a setima botoeira nao esta nela")
	{
		const QStringList inside = componentsAt(scratch.project(), panel_code);
		INFO(inside.join(QLatin1String(", ")).toStdString());
		REQUIRE(inside.count() == 6);
		REQUIRE(inside == QStringList({QStringLiteral("S1"), QStringLiteral("S2"),
					       QStringLiteral("S3"), QStringLiteral("S4"),
					       QStringLiteral("S5"), QStringLiteral("S6")}));
		REQUIRE(pathOf(button(sheet, QStringLiteral("S7"))).isEmpty());
	}

	SECTION("a janela do gerenciador mostra o mesmo 6 que a folha tem")
	{
		LocationManagerDialog manager(scratch.project());
		REQUIRE(managerCount(manager, panel_name) == 6);
	}

	SECTION("controle negativo — uma atribuicao a mais move a contagem da janela")
	{
		// The number in the window has to be able to be something else, or
		// the six above proves nothing. The seventh button is assigned by
		// hand, away from any rectangle, which is the one thing the
		// containment rule never undoes on its own.
		LocationManagerDialog manager(scratch.project());
		Element *stray = button(sheet, QStringLiteral("S7"));
		REQUIRE(stray != nullptr);
		sheet->undoStack().push(new AssignLocationCommand(
						QList<Element *>({stray}), panel_code));

		const QStringList inside = componentsAt(scratch.project(), panel_code);
		INFO(inside.join(QLatin1String(", ")).toStdString());
		REQUIRE(inside.count() == 7);
		REQUIRE(inside.contains(QStringLiteral("S7")));
		REQUIRE(managerCount(manager, panel_name) == 7);

		sheet->undoStack().undo();
		REQUIRE(componentsAt(scratch.project(), panel_code).count() == 6);
		REQUIRE(managerCount(manager, panel_name) == 6);
	}

	SECTION("arrastar uma botoeira para fora leva a contagem a 5, e limpa o campo dela")
	{
		LocationManagerDialog manager(scratch.project());
		dragTo(sheet, button(sheet, QStringLiteral("S1")), outside_target);

		const QStringList inside = componentsAt(scratch.project(), panel_code);
		INFO(inside.join(QLatin1String(", ")).toStdString());
		REQUIRE(inside.count() == 5);
		REQUIRE_FALSE(inside.contains(QStringLiteral("S1")));
		REQUIRE(managerCount(manager, panel_name) == 5);

		// The queue item asks for these two apart: a component that left with
		// an empty field and a component that kept the old path are different
		// defects, and only the first is right.
		REQUIRE(pathOf(button(sheet, QStringLiteral("S1"))).isEmpty());
	}

	SECTION("arrastar a de fora para dentro devolve a contagem a 6, agora com a setima")
	{
		LocationManagerDialog manager(scratch.project());
		dragTo(sheet, button(sheet, QStringLiteral("S1")), outside_target);
		dragTo(sheet, button(sheet, QStringLiteral("S7")), inside_target);

		const QStringList inside = componentsAt(scratch.project(), panel_code);
		INFO(inside.join(QLatin1String(", ")).toStdString());
		REQUIRE(inside.count() == 6);
		REQUIRE(inside.contains(QStringLiteral("S7")));
		REQUIRE_FALSE(inside.contains(QStringLiteral("S1")));
		REQUIRE(managerCount(manager, panel_name) == 6);
	}

	SECTION("controle negativo — a borda tem dois lados: parar rente a ela por dentro nao tira nada")
	{
		// A drag that stops inside the rectangle must not change the count,
		// otherwise "6 becomes 5" would only be saying that any drag at all
		// empties the field. The centre of the component is what is judged, so
		// a stop two units short of the border is still inside.
		Element *moved = button(sheet, QStringLiteral("S1"));
		dragTo(sheet, moved,
		       QPointF(area_corner_2.x() - 2., area_corner_2.y() - 2.));

		REQUIRE(componentsAt(scratch.project(), panel_code).count() == 6);
		REQUIRE(pathOf(moved) == panel_code);

		// And two units the other side of the same border does take it out.
		dragTo(sheet, moved,
		       QPointF(area_corner_2.x() + 2., area_corner_2.y() + 2.));
		REQUIRE(componentsAt(scratch.project(), panel_code).count() == 5);
		REQUIRE(pathOf(moved).isEmpty());
	}

	SECTION("cada arrasto e um passo so de desfazer, e devolve posicao e atribuicao juntas")
	{
		LocationManagerDialog manager(scratch.project());
		Element *out = button(sheet, QStringLiteral("S1"));
		Element *in = button(sheet, QStringLiteral("S7"));
		const QPointF out_was = out->pos();
		const QPointF in_was = in->pos();

		// One command for the rectangle, then one for each drag.
		REQUIRE(sheet->undoStack().index() == 1);
		dragTo(sheet, out, outside_target);
		REQUIRE(sheet->undoStack().index() == 2);
		dragTo(sheet, in, inside_target);
		REQUIRE(sheet->undoStack().index() == 3);

		sheet->undoStack().undo();
		REQUIRE(sheet->undoStack().index() == 2);
		REQUIRE(in->pos() == in_was);
		REQUIRE(pathOf(in).isEmpty());
		REQUIRE(managerCount(manager, panel_name) == 5);

		sheet->undoStack().undo();
		REQUIRE(sheet->undoStack().index() == 1);
		REQUIRE(out->pos() == out_was);
		REQUIRE(pathOf(out) == panel_code);
		REQUIRE(managerCount(manager, panel_name) == 6);

		// The third undo takes back the rectangle itself, and with it the six
		// assignments it caused.
		sheet->undoStack().undo();
		REQUIRE(sheet->undoStack().index() == 0);
		REQUIRE(areaOf(sheet)->locationPath().isEmpty());
		REQUIRE(componentsAt(scratch.project(), panel_code).isEmpty());
		REQUIRE(managerCount(manager, panel_name) == 0);
	}

	SECTION("controle negativo — em dois comandos, um desfazer devolve a atribuicao e nao a posicao")
	{
		/*
			What the check above is worth depends on it being able to tell one
			command from two, so here the drag is taken apart into the two
			halves it is made of and pushed separately. That is exactly the
			failure the queue item asks to be recorded apart - the undo gave
			back the position but not the assignment, or the other way round -
			and this is the second of the two.
		*/
		Element *moved = button(sheet, QStringLiteral("S1"));
		const QPointF was = moved->pos();

		sheet->clearSelection();
		moved->setSelected(true);
		const DiagramContent content(sheet);
		const QPointF movement = outside_target - was;

		/*
			The item is put where it is going first, and only then does the
			command of the movement get pushed. That is not a detour: the
			first redo of MoveGraphicsItemCommand applies -movement to arm
			the animation and moves nothing, because in production the mouse
			has already dragged the item and the command exists to be able
			to take that back. Pushing it on an item that has not moved
			leaves the item where it was.
		*/
		moved->setPos(outside_target);

		sheet->undoStack().push(
			new MoveGraphicsItemCommand(sheet, content, movement));
		sheet->undoStack().push(new AssignLocationCommand(
						QList<Element *>({moved}), QString()));
		sheet->clearSelection();

		REQUIRE(sheet->undoStack().index() == 3);
		REQUIRE(componentsAt(scratch.project(), panel_code).count() == 5);

		sheet->undoStack().undo();
		// The assignment came back, the position did not: the count says 6
		// while the button is still sitting outside the rectangle, and the
		// component that is lying about where it stands is named here.
		REQUIRE(componentsAt(scratch.project(), panel_code).count() == 6);
		INFO(moved->elementInformations()
		     .value(QETInformation::ELMT_LABEL).toString().toStdString());
		REQUIRE(moved->pos() == outside_target);
		REQUIRE_FALSE(moved->pos() == was);

		sheet->undoStack().undo();
		REQUIRE(moved->pos() == was);
	}

	SECTION("o retangulo e as atribuicoes sobrevivem a salvar e reabrir")
	{
		dragTo(sheet, button(sheet, QStringLiteral("S1")), outside_target);
		dragTo(sheet, button(sheet, QStringLiteral("S7")), inside_target);

		REQUIRE(scratch.saveAndReopen());

		// Every pointer taken before the save is gone; the sheet, the area and
		// the buttons are taken again from the reopened project.
		Diagram *reopened = scratch.diagram(0);
		REQUIRE(reopened != nullptr);
		REQUIRE(reopened->elements().count() == 7);

		LocationAreaItem *saved_area = areaOf(reopened);
		REQUIRE(saved_area != nullptr);
		REQUIRE(saved_area->locationPath() == panel_code);
		REQUIRE(saved_area->sceneRect() == QRectF(area_corner_1, area_corner_2));

		const QStringList inside = componentsAt(scratch.project(), panel_code);
		INFO(inside.join(QLatin1String(", ")).toStdString());
		REQUIRE(inside.count() == 6);
		REQUIRE(inside.contains(QStringLiteral("S7")));
		REQUIRE_FALSE(inside.contains(QStringLiteral("S1")));

		// And the manager of the reopened project says the same thing: the
		// location itself survived the round trip, not only the components
		// pointing at it.
		LocationManagerDialog reopened_manager(scratch.project());
		REQUIRE(managerCount(reopened_manager, panel_name) == 6);
	}

	SECTION("controle negativo — sem retangulo salvo, o arquivo reabre sem ele")
	{
		// The survival check above has to be able to fail, and the way it
		// would fail is a file that carries the components but not the
		// rectangle. Taking the area out before saving produces exactly that
		// file.
		delete areaOf(sheet);
		REQUIRE(areaOf(sheet) == nullptr);
		REQUIRE(scratch.saveAndReopen());

		Diagram *reopened = scratch.diagram(0);
		REQUIRE(reopened != nullptr);
		REQUIRE(areaOf(reopened) == nullptr);

		// The components keep the path they were given, because a path is a
		// field of the component and not a property of the rectangle: what is
		// lost with the rectangle is the rule that keeps that field honest.
		REQUIRE(componentsAt(scratch.project(), panel_code).count() == 6);
	}
}
