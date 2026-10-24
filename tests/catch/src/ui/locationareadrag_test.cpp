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

#include "../../../../sources/QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/elementsmover.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/independenttextitem.h"
#include "../../../../sources/qetgraphicsitem/locationareaitem.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/alignselectioncommand.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QUndoStack>

#include <cmath>

/*
	The location area dragged by the mouse (T85).

	A drag of its border takes what is in it - the components, the wires
	between them whole, the wire to a component left outside stretched, the
	free texts, the shapes -, in one undo step. After a double click the
	area is in edition: its handles show, and a drag moves it alone.

	The bench, one folio, y = 200: A at x = 200 and B at 300, inside the
	area drawn from (100, 100) to (400, 300); C at 600, outside. Two wires,
	A - B and B - C. Everything stands on the grid of 10, and every drag
	goes by tens, so that the grid of the machine running the suite - 1 or
	10 - moves nothing by itself.

	The events go through the scene, as a view hands them over, so that the
	scene picks the area under the mouse, selects it and lets it grab the
	mouse. The area is grabbed by its left border, at (100, 150): away from
	the handles of its corners and of the middle of its edges, which in
	edition sit over the border and would take the press.
*/
namespace
{
	struct Part
	{
		int x;
		int y;
		const char *label;
	};

	/**
		A folio with @a parts, each a symbol with a terminal east (id
		2 * index) and one west (id 2 * index + 1), and @a conductors.
	*/
	QString fixture(const QList<Part> &parts, const QString &conductors)
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		const QString definition = QStringLiteral(
			"<element name=\"part.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"30\" height=\"20\" hotspot_x=\"15\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">part</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			"<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			"</description>"
			"</definition>"
			"</element>");

		QString instances;
		for (int index = 0 ; index < parts.size() ; ++index)
		{
			instances += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
				" freezeLabel=\"false\" orientation=\"0\""
				" type=\"embed://bench/part.elmt\""
				" uuid=\"{85a00000-0000-4000-8000-00000000000%3}\">"
				"<terminals>"
				"<terminal x=\"%4\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
				"<terminal x=\"%6\" y=\"0\" orientation=\"3\" id=\"%7\"/>"
				"</terminals>"
				"<inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%8</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(parts.at(index).x)
				.arg(parts.at(index).y)
				.arg(index)
				.arg(east_dock)
				.arg(index * 2)
				.arg(west_dock)
				.arg(index * 2 + 1)
				.arg(QLatin1String(parts.at(index).label));
		}

		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" folio=\"1\""
			" height=\"600\" cols=\"17\" colsize=\"50\" rows=\"8\""
			" rowsize=\"80\" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors>%3</conductors>"
			"</diagram>"
			"</project>")
			.arg(definition, instances, conductors);
	}

		/// A, B and C on a row, A - B and B - C wired: A east (0) to B
		/// west (3), B east (2) to C west (5)
	QString threeParts()
	{
		return fixture({{200, 200, "A"}, {300, 200, "B"}, {600, 200, "C"}},
			       QStringLiteral(
				       "<conductor terminal1=\"0\" terminal2=\"3\" num=\"W1\""
				       " displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
				       "<conductor terminal1=\"2\" terminal2=\"5\" num=\"W2\""
				       " displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"));
	}

	struct Bench
	{
		explicit Bench(const QString &xml) :
			project(xml, QStringLiteral("locationareadrag.qet"))
		{}

		bool open(int elements, int wires)
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			return sheet
				&& sheet->elements().size() == elements
				&& sheet->conductors().size() == wires;
		}

		LocationAreaItem *drawArea(const QPointF &p1, const QPointF &p2)
		{
			auto *area = new LocationAreaItem(p1, p2);
			sheet->addItem(area);
			return area;
		}

		Element *element(const QString &label) const
		{
			for (Element *e : sheet->elements()) {
				if (e->actualLabel() == label) return e;
			}
			return nullptr;
		}

			/// The wire between @a one and @a two, either way round
		Conductor *wire(const Element *one, const Element *two) const
		{
			for (Conductor *c : sheet->conductors())
			{
				const Element *e1 = c->terminal1->parentElement();
				const Element *e2 = c->terminal2->parentElement();
				if ((e1 == one && e2 == two) || (e1 == two && e2 == one)) return c;
			}
			return nullptr;
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
	};

		/// A mouse event of @a type at @a at, the left button pressed at
		/// @a from; the screen is the scene four times over, as in
		/// undomove_test.cpp
	void mouse(QGraphicsScene *scene, QEvent::Type type, const QPointF &at, const QPointF &from)
	{
		QGraphicsSceneMouseEvent event(type);
		event.setButton(Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
									    : Qt::LeftButton);
		event.setScenePos(at);
		event.setScreenPos((at * 4).toPoint());
		event.setLastScenePos(at);
		event.setLastScreenPos((at * 4).toPoint());
		event.setButtonDownScenePos(Qt::LeftButton, from);
		event.setButtonDownScreenPos(Qt::LeftButton, (from * 4).toPoint());
		QCoreApplication::sendEvent(scene, &event);
	}

		/// A click at @a at, without moving
	void click(QGraphicsScene *scene, const QPointF &at)
	{
		mouse(scene, QEvent::GraphicsSceneMousePress, at, at);
		mouse(scene, QEvent::GraphicsSceneMouseRelease, at, at);
	}

		/// A double click at @a at: a click, then the second press, which
		/// the scene hands over as a double click to the item the first one
		/// went to, then its release
	void doubleClick(QGraphicsScene *scene, const QPointF &at)
	{
		click(scene, at);
		mouse(scene, QEvent::GraphicsSceneMouseDoubleClick, at, at);
		mouse(scene, QEvent::GraphicsSceneMouseRelease, at, at);
	}

		/// A drag from @a from to @a to, in two moves, the first already
		/// past the distance a drag starts at
	void drag(QGraphicsScene *scene, const QPointF &from, const QPointF &to)
	{
		mouse(scene, QEvent::GraphicsSceneMousePress, from, from);
		mouse(scene, QEvent::GraphicsSceneMouseMove, (from + to) / 2, from);
		mouse(scene, QEvent::GraphicsSceneMouseMove, to, from);
		mouse(scene, QEvent::GraphicsSceneMouseRelease, to, from);
	}

	bool same(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
	}

	bool same(const QRectF &a, const QRectF &b)
	{
		return same(a.topLeft(), b.topLeft()) && same(a.bottomRight(), b.bottomRight());
	}

	QString text(const QPointF &p)
	{
		return QStringLiteral("(%1, %2)").arg(p.x()).arg(p.y());
	}

	QString text(const QRectF &r)
	{
		return text(r.topLeft()) + QStringLiteral(" - ") + text(r.bottomRight());
	}

		/// Whether @a wire still runs from the end of one of its terminals
		/// to the end of the other
	bool joined(Conductor *wire)
	{
		const QList<QPointF> points = wire->scenePathPoints();
		return points.size() >= 2
			&& same(points.first(), wire->terminal1->dockConductor())
			&& same(points.last(), wire->terminal2->dockConductor());
	}

		/// The handles shown on the folio
	int handles(Diagram *sheet)
	{
		int count = 0;
		const QList<QGraphicsItem *> items = sheet->items();
		for (QGraphicsItem *item : items) {
			if (item->type() == QetGraphicsHandlerItem::Type) ++count;
		}
		return count;
	}

	const QPointF border{100, 150};
	const QPointF by{60, 40};
}

TEST_CASE("T85 - The area dragged takes what is in it", "[T85][zone]")
{
	Bench bench(threeParts());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open(3, 2));
	Diagram *sheet = bench.sheet;

	LocationAreaItem *area = bench.drawArea(QPointF(100, 100), QPointF(400, 300));
	auto *shape_in = new QetShapeItem(QPointF(150, 260), QPointF(230, 260), QetShapeItem::Line);
	auto *shape_out = new QetShapeItem(QPointF(500, 400), QPointF(560, 400), QetShapeItem::Line);
	auto *text_in = new IndependentTextItem(QStringLiteral("note"));
	sheet->addItem(shape_in);
	sheet->addItem(shape_out);
	sheet->addItem(text_in);
	text_in->setPos(250, 230);

	Element *a = bench.element(QStringLiteral("A"));
	Element *b = bench.element(QStringLiteral("B"));
	Element *c = bench.element(QStringLiteral("C"));
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	REQUIRE(c != nullptr);
	Conductor *ab = bench.wire(a, b);
	Conductor *bc = bench.wire(b, c);
	REQUIRE(ab != nullptr);
	REQUIRE(bc != nullptr);

		//The fixture is what it says: the area holds A, B, the line and the
		//text, and neither C nor the other line
	const QList<QGraphicsItem *> carried = area->carriedItems();
	const auto holds = [&carried](QGraphicsItem *item) { return carried.contains(item); };
	CHECK(holds(a));
	CHECK(holds(b));
	CHECK(holds(shape_in));
	CHECK(holds(text_in));
	CHECK_FALSE(holds(c));
	CHECK_FALSE(holds(shape_out));
	CHECK_FALSE(holds(area));
	REQUIRE(joined(ab));
	REQUIRE(joined(bc));

	const QRectF area_was = area->sceneRect();
	const QPointF a_was = a->pos();
	const QPointF b_was = b->pos();
	const QPointF c_was = c->pos();
	const QPointF shape_in_was = shape_in->pos();
	const QPointF shape_out_was = shape_out->pos();
	const QPointF text_was = text_in->pos();
	const int steps = sheet->undoStack().count();

	sheet->clearSelection();
	drag(sheet, border, border + by);

	INFO("area " << text(area->sceneRect()).toStdString()
	     << ", A " << text(a->pos()).toStdString()
	     << ", B " << text(b->pos()).toStdString()
	     << ", C " << text(c->pos()).toStdString()
	     << ", line " << text(shape_in->pos()).toStdString()
	     << ", text " << text(text_in->pos()).toStdString());
		//The area, and what is in it, by the same
	CHECK(same(area->sceneRect(), area_was.translated(by)));
	CHECK(same(a->pos(), a_was + by));
	CHECK(same(b->pos(), b_was + by));
	CHECK(same(shape_in->pos(), shape_in_was + by));
	CHECK(same(text_in->pos(), text_was + by));
		//What is outside stays
	CHECK(same(c->pos(), c_was));
	CHECK(same(shape_out->pos(), shape_out_was));
		//The wire between two moved goes whole, the one to C stretches:
		//both still run from terminal to terminal, and no wire was added
	CHECK(joined(ab));
	CHECK(joined(bc));
	CHECK(sheet->conductors().size() == 2);
		//One step, the area left selected alone and out of edition
	CHECK(sheet->undoStack().count() == steps + 1);
	CHECK(sheet->selectedItems() == QList<QGraphicsItem *>{area});
	CHECK_FALSE(area->isEditing());

		//One undo takes all of it back, the area included
	sheet->undoStack().undo();
	QCoreApplication::processEvents();
	CHECK(same(area->sceneRect(), area_was));
	CHECK(same(a->pos(), a_was));
	CHECK(same(b->pos(), b_was));
	CHECK(same(shape_in->pos(), shape_in_was));
	CHECK(same(text_in->pos(), text_was));
	CHECK(same(c->pos(), c_was));
	CHECK(joined(ab));
	CHECK(joined(bc));

		//And the redo all of it again
	sheet->undoStack().redo();
	QCoreApplication::processEvents();
	CHECK(same(area->sceneRect(), area_was.translated(by)));
	CHECK(same(a->pos(), a_was + by));
	CHECK(same(b->pos(), b_was + by));
	CHECK(joined(bc));
}

TEST_CASE("T85 - A double click edits the area alone", "[T85][zone]")
{
	Bench bench(threeParts());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open(3, 2));
	Diagram *sheet = bench.sheet;

	LocationAreaItem *area = bench.drawArea(QPointF(100, 100), QPointF(400, 300));
	Element *a = bench.element(QStringLiteral("A"));
	Element *b = bench.element(QStringLiteral("B"));
	Element *c = bench.element(QStringLiteral("C"));
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	REQUIRE(c != nullptr);
	Conductor *bc = bench.wire(b, c);
	REQUIRE(bc != nullptr);

		//A click selects it, out of edition: no handle
	sheet->clearSelection();
	click(sheet, border);
	REQUIRE(sheet->selectedItems() == QList<QGraphicsItem *>{area});
	CHECK_FALSE(area->isEditing());
	CHECK(handles(sheet) == 0);

		//The double click: the eight handles of a rectangle
	doubleClick(sheet, border);
	REQUIRE(area->isEditing());
	CHECK(handles(sheet) == 8);

	const QRectF area_was = area->sceneRect();
	const QPointF a_was = a->pos();
	const QPointF b_was = b->pos();
	const int steps = sheet->undoStack().count();

	drag(sheet, border, border + by);

	INFO("area " << text(area->sceneRect()).toStdString()
	     << ", A " << text(a->pos()).toStdString()
	     << ", B " << text(b->pos()).toStdString());
		//The area alone: what it held stays where it was
	CHECK(same(area->sceneRect(), area_was.translated(by)));
	CHECK(same(a->pos(), a_was));
	CHECK(same(b->pos(), b_was));
	CHECK(joined(bc));
	CHECK(sheet->undoStack().count() == steps + 1);
		//Still in edition, its handles following it
	CHECK(area->isEditing());
	CHECK(handles(sheet) == 8);

		//The undo puts the rectangle back too, and not only what the drag
		//changed around it
	sheet->undoStack().undo();
	QCoreApplication::processEvents();
	CHECK(same(area->sceneRect(), area_was));
	CHECK(same(a->pos(), a_was));

		//Deselected, out of edition, no handle left
	sheet->clearSelection();
	CHECK_FALSE(area->isEditing());
	CHECK(handles(sheet) == 0);

		//Selected again, a drag takes the content once more
	drag(sheet, border, border + by);
	CHECK(same(area->sceneRect(), area_was.translated(by)));
	CHECK(same(a->pos(), a_was + by));
}

TEST_CASE("T85 - A lone component in a dragged area gets no new wire", "[T85][zone]")
{
		//D in the area, E outside, on the same line: D east faces E west,
		//both free
	Bench bench(fixture({{200, 400, "D"}, {500, 400, "E"}}, QString()));
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open(2, 0));
	Diagram *sheet = bench.sheet;
	bench.project->setAutoConductor(true);

	LocationAreaItem *area = bench.drawArea(QPointF(100, 300), QPointF(300, 500));
	Element *d = bench.element(QStringLiteral("D"));
	REQUIRE(d != nullptr);
	const QPointF d_was = d->pos();

		//The control: D dragged alone, along its line, is wired to E - the
		//automatic wire of a component placed in line with a free terminal
	sheet->clearSelection();
	d->setSelected(true);
	ElementsMover mover;
	REQUIRE(mover.beginMovement(sheet) == 1);
	mover.continueMovement(QPointF(20, 0));
	mover.endMovement();
	sheet->clearSelection();
	REQUIRE(sheet->conductors().size() == 1);
	sheet->undoStack().undo();
	QCoreApplication::processEvents();
	REQUIRE(sheet->conductors().isEmpty());
	REQUIRE(same(d->pos(), d_was));

		//The area dragged along the same line, D in it: no wire
	const QPointF grip{100, 350};
	drag(sheet, grip, grip + QPointF(40, 0));
	INFO("D " << text(d->pos()).toStdString());
	CHECK(same(d->pos(), d_was + QPointF(40, 0)));
	CHECK(sheet->conductors().isEmpty());
		//And the project keeps its automatic wires for the next drag
	CHECK(bench.project->autoConductor());
	CHECK(same(area->sceneRect().topLeft(), QPointF(140, 300)));
}

namespace
{
	void key(QGraphicsScene *scene, Qt::Key k)
	{
		QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier);
		QCoreApplication::sendEvent(scene, &press);
		QKeyEvent release(QEvent::KeyRelease, k, Qt::NoModifier);
		QCoreApplication::sendEvent(scene, &release);
	}
}

TEST_CASE("T85 - The area moved by the arrows takes what is in it", "[T85][zone]")
{
	Bench bench(threeParts());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open(3, 2));
	Diagram *sheet = bench.sheet;
	LocationAreaItem *area = bench.drawArea(QPointF(100, 100), QPointF(400, 300));
	auto *text_in = new IndependentTextItem(QStringLiteral("note"));
	sheet->addItem(text_in);
	text_in->setPos(250, 230);
	Element *a = bench.element(QStringLiteral("A"));
	Element *b = bench.element(QStringLiteral("B"));
	Element *c = bench.element(QStringLiteral("C"));
	REQUIRE(a != nullptr);
	REQUIRE(c != nullptr);
	Conductor *bc = bench.wire(b, c);
	REQUIRE(bc != nullptr);
	const QRectF area_was = area->sceneRect();
	const QPointF a_was = a->pos();
	const QPointF b_was = b->pos();
	const QPointF c_was = c->pos();
	const QPointF text_was = text_in->pos();

		//Only the area selected, out of edition, as a click leaves it
	sheet->clearSelection();
	area->setSelected(true);
	const int index = sheet->undoStack().index();
	key(sheet, Qt::Key_Right);

		//By one step of the arrows, whatever the grid: the area, and what is
		//in it by the same
	const QPointF moved = area->sceneRect().topLeft() - area_was.topLeft();
	INFO("moved " << text(moved).toStdString());
	CHECK(moved.x() > 0);
	CHECK(moved.y() == Approx(0));
	CHECK(same(a->pos(), a_was + moved));
	CHECK(same(b->pos(), b_was + moved));
	CHECK(same(text_in->pos(), text_was + moved));
	CHECK(same(c->pos(), c_was));
	CHECK(joined(bc));
	CHECK(sheet->conductors().size() == 2);
		//One step, and the area selected alone again
	CHECK(sheet->undoStack().index() == index + 1);
	CHECK(sheet->selectedItems() == QList<QGraphicsItem *>{area});

	sheet->undoStack().undo();
	QCoreApplication::processEvents();
	CHECK(same(area->sceneRect(), area_was));
	CHECK(same(a->pos(), a_was));
	CHECK(same(text_in->pos(), text_was));
	CHECK(joined(bc));

		//In edition, the arrows move the area alone
	area->setEditing(true);
	REQUIRE(area->isEditing());
	key(sheet, Qt::Key_Down);
	CHECK(area->sceneRect().top() > area_was.top());
	CHECK(same(a->pos(), a_was));
	sheet->undoStack().undo();
	QCoreApplication::processEvents();
	CHECK(same(area->sceneRect(), area_was));
}

TEST_CASE("T85 - The area lines up with a component, and takes what is in it", "[T85][zone]")
{
	Bench bench(threeParts());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open(3, 2));
	Diagram *sheet = bench.sheet;
	LocationAreaItem *area = bench.drawArea(QPointF(100, 100), QPointF(400, 300));
	Element *a = bench.element(QStringLiteral("A"));
	Element *c = bench.element(QStringLiteral("C"));
	REQUIRE(a != nullptr);
	REQUIRE(c != nullptr);
	const QRectF area_was = area->sceneRect();
	const QPointF a_was = a->pos();
	const QPointF c_was = c->pos();

		//The area and C, out of it, selected; A inside, selected too, is
		//the area's to move, and not lined up once more
	sheet->clearSelection();
	area->setSelected(true);
	c->setSelected(true);
	a->setSelected(true);
	CHECK(AlignSelectionCommand::unitCount(DiagramContent(sheet)) == 2);
	auto *align = new AlignSelectionCommand(sheet, AlignSelectionCommand::AlignRight);
	REQUIRE(align->isValid());
	sheet->undoStack().push(align);

		//The right of the area on the right of C, within a step of the grid;
		//A moved with it, C where it was
	const QPointF moved = area->sceneRect().topLeft() - area_was.topLeft();
	INFO("moved " << text(moved).toStdString());
	CHECK(moved.x() > 100);
	CHECK(std::abs(area->sceneRect().right() - c->sceneBoundingRect().right()) <= 10);
	CHECK(same(a->pos(), a_was + moved));
	CHECK(same(c->pos(), c_was));

	sheet->undoStack().undo();
	CHECK(same(area->sceneRect(), area_was));
	CHECK(same(a->pos(), a_was));
}
