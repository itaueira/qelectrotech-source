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
#include "../../../../sources/factory/propertieseditorfactory.h"
#include "../../../../sources/ui/stripframepropertieswidget.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/stripframeitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"

#include <catch2/catch.hpp>

#include <QListWidget>
#include <QPainterPath>
#include <QUndoStack>

#include <cmath>

/*
	The frame of a strip drawn round its terminals (T85): chosen, not drawn
	by hand, and following them.

	The bench: six terminals of a strip, each a box of 12 with an end 14
	above and 14 below its middle, four in a row at y = 300 from x = 200 to
	290, and two more at x = 500 and 530 - the same strip shown in another
	place of the folio.
*/
namespace
{
	QString fixture()
	{
		const QString definition = QStringLiteral(
			"<element name=\"strip.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\""
			" orientation=\"dnnn\" link_type=\"terminal\">"
			"<names><name lang=\"en\">strip</name></names>"
			"<description>"
			"<rect x=\"-6\" y=\"-6\" width=\"12\" height=\"12\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-14\" orientation=\"n\"/>"
			"<terminal x=\"0\" y=\"14\" orientation=\"s\"/>"
			"</description>"
			"</definition>"
			"</element>");
		const QList<int> xs {200, 230, 260, 290, 500, 530};
		QString xml;
		for (int i = 0 ; i < xs.size() ; ++i)
		{
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"300\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"0\" type=\"embed://bench/strip.elmt\""
				" uuid=\"{85000000-0000-4000-8000-0000000000%2}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%3</elementInformation>"
				"</elementInformations>"
					//A text that shows the comment, beside the terminal, as
					//the symbol of a strip of the request does
				"<dynamic_texts>"
				"<dynamic_elmt_text Halignment=\"AlignLeft\" Valignment=\"AlignTop\""
				" font=\"Sans Serif,6,-1,5,50,0,0,0,0,0\" frame=\"false\""
				" keep_visual_rotation=\"false\" rotation=\"0\" rotation_point_center=\"false\""
				" text_from=\"ElementInfo\" text_width=\"-1\""
				" uuid=\"{85000000-0000-4000-8000-0000000001%2}\" x=\"8\" y=\"-4\">"
				"<text/><info_name>comment</info_name></dynamic_elmt_text>"
				"</dynamic_texts><texts_groups/>"
				"</element>")
				.arg(xs.at(i))
				.arg(i, 2, 10, QLatin1Char('0'))
				.arg(i + 1);
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"800\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"10\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definition, xml);
	}

	struct Bench
	{
		Bench() : project(fixture()) {}
		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			terminals.clear();
			for (int i = 1 ; i <= 6 ; ++i)
			{
				for (Element *e : sheet->elements()) {
					if (e->actualLabel() == QString::number(i)) terminals << e;
				}
			}
			return terminals.size() == 6;
		}
		QList<StripFrameItem *> frames() const
		{
			QList<StripFrameItem *> list;
			for (QGraphicsItem *item : sheet->items()) {
				if (auto *frame = qgraphicsitem_cast<StripFrameItem *>(item)) list << frame;
			}
			return list;
		}
		QList<Element *> first(int n) const {return terminals.mid(0, n);}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		QList<Element *> terminals;
	};

	QString comment(const Element *e)
	{
		return e->elementInformations().value(QETInformation::ELMT_COMMENT).toString();
	}

		/// The text of the symbol that shows its comment
	DynamicElementTextItem *commentText(const Element *e)
	{
		for (DynamicElementTextItem *text : e->dynamicTextItems()) {
			if (text->infoName() == QETInformation::ELMT_COMMENT) return text;
		}
		return nullptr;
	}

	void erase(Diagram *sheet, const QList<Element *> &elements)
	{
		sheet->clearSelection();
		for (Element *e : elements) e->setSelected(true);
		DiagramContent selection(sheet);
		sheet->clearSelection();
		sheet->undoStack().push(new DeleteQGraphicsItemCommand(sheet, selection));
	}

		/// What the Delete key of the editor pushes for the selection of
		/// @a sheet that holds a frame (QETDiagramEditor::selectionGroupTriggered())
	void deleteSelection(Diagram *sheet)
	{
		DiagramContent content(sheet);
		const QList<StripFrameItem *> frames = StripFrameItem::stripFrames(sheet->selectedItems());
		sheet->clearSelection();
		QList<QUndoCommand *> others;
		if (content.hasDeletableItems()) {
			others << new DeleteQGraphicsItemCommand(sheet, content);
		}
		if (QUndoCommand *step = StripFrameItem::deleteCommand(frames, others)) {
			sheet->undoStack().push(step);
		}
	}

	bool samePoint(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
	}
}

TEST_CASE("CU-85.1 - The strip drawn round its terminals", "[T85][stripframe]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());

	const int steps = bench.sheet->undoStack().count();
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	CHECK(bench.sheet->undoStack().count() == steps + 1);

	const QList<StripFrameItem *> frames = bench.frames();
	REQUIRE(frames.size() == 1);
	StripFrameItem *frame = frames.first();
	CHECK(frame->stripName() == QStringLiteral("X1"));
	CHECK(frame->terminals().size() == 4);

		//Round what the four draw, and short of every end a wire comes to
	const QRectF rect = frame->frameRect();
	for (Element *e : bench.first(4))
	{
		CHECK(rect.contains(e->mapRectToScene(e->drawnRect())));
		for (Terminal *t : e->terminals()) {
			CHECK_FALSE(rect.contains(t->dockConductor()));
		}
		CHECK(comment(e) == QStringLiteral("X1"));
			//Written in the comment, and its text hidden: the frame says it
		REQUIRE(commentText(e));
		CHECK_FALSE(commentText(e)->isVisible());
	}
		//The name at the left of a strip that lies
	CHECK(rect.left() < bench.terminals.at(0)->mapRectToScene(bench.terminals.at(0)->drawnRect()).left() - 4);
	CHECK_FALSE(rect.contains(bench.terminals.at(4)->mapRectToScene(bench.terminals.at(4)->drawnRect())));

		//One undo step takes the frame and the names away
	bench.sheet->undoStack().undo();
	CHECK(bench.frames().isEmpty());
	for (Element *e : bench.first(4)) {
		CHECK(comment(e).isEmpty());
		CHECK(commentText(e)->isVisible());
	}
	bench.sheet->undoStack().redo();
	CHECK(bench.frames().size() == 1);
	CHECK(comment(bench.terminals.at(3)) == QStringLiteral("X1"));
}

TEST_CASE("CU-85.2 - The strip follows its terminal", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	REQUIRE(bench.frames().size() == 1);
	StripFrameItem *frame = bench.frames().first();
	const QRectF before = frame->frameRect();

	Element *last = bench.terminals.at(3);
	last->setPos(last->pos() + QPointF(20, 0));
	const QRectF after = frame->frameRect();
	CHECK(after.right() == Approx(before.right() + 20));
	CHECK(after.left() == Approx(before.left()));
	CHECK(after.contains(last->mapRectToScene(last->drawnRect())));
}

TEST_CASE("CU-85.3 - A terminal deleted leaves the strip", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	REQUIRE(bench.frames().size() == 1);
	StripFrameItem *frame = bench.frames().first();
	const QRectF whole = frame->frameRect();

		//The last one: the frame shrinks to the three left
	erase(bench.sheet, {bench.terminals.at(3)});
	CHECK(frame->terminals().size() == 3);
	CHECK(frame->frameRect().right() < whole.right() - 20);

		//All of them: nothing is drawn, and the undo brings them back
	erase(bench.sheet, bench.first(3));
	CHECK(frame->terminals().isEmpty());
	CHECK(frame->frameRect().isNull());
	bench.sheet->undoStack().undo();
	CHECK(frame->terminals().size() == 3);
	CHECK_FALSE(frame->frameRect().isNull());
	bench.sheet->undoStack().undo();
	CHECK(frame->terminals().size() == 4);
	CHECK(frame->frameRect().right() == Approx(whole.right()));
}

TEST_CASE("CU-85.4 - The strip saved and opened again", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	REQUIRE(bench.frames().size() == 1);
	const QRectF rect = bench.frames().first()->frameRect();

	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.saveAndReopen());
	REQUIRE(bench.open());
	const QList<StripFrameItem *> frames = bench.frames();
	REQUIRE(frames.size() == 1);
	CHECK(frames.first()->stripName() == QStringLiteral("X1"));
	CHECK(frames.first()->terminals().size() == 4);
	CHECK(frames.first()->frameRect().left() == Approx(rect.left()));
	CHECK(frames.first()->frameRect().right() == Approx(rect.right()));
	CHECK(comment(bench.terminals.at(0)) == QStringLiteral("X1"));
}

TEST_CASE("T85 - One strip in two places of a folio has two frames", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	StripFrameItem::drawAround(bench.sheet, bench.terminals.mid(4, 2), QStringLiteral("X1"));
	const QList<StripFrameItem *> frames = bench.frames();
	REQUIRE(frames.size() == 2);
	CHECK(frames.at(0)->terminals().size() + frames.at(1)->terminals().size() == 6);
	CHECK_FALSE(frames.at(0)->frameRect().intersects(frames.at(1)->frameRect()));

		//One of the first frame drawn again with the two of the second: it
		//leaves the first, and the frame that held the most of them grows
	StripFrameItem *second = StripFrameItem::frameOf(bench.terminals.at(4));
	REQUIRE(second);
	StripFrameItem::drawAround(bench.sheet,
				   {bench.terminals.at(3), bench.terminals.at(4), bench.terminals.at(5)},
				   QStringLiteral("X1"));
	CHECK(second->terminals().size() == 3);
	CHECK(StripFrameItem::frameOf(bench.terminals.at(0))->terminals().size() == 3);
}

TEST_CASE("T85 - A terminal taken out of its strip", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	StripFrameItem *frame = bench.frames().first();
	StripFrameItem::takeOut(bench.sheet, {bench.terminals.at(3)});
	CHECK(frame->terminals().size() == 3);
	CHECK(comment(bench.terminals.at(3)).isEmpty());
	CHECK(StripFrameItem::frameOf(bench.terminals.at(3)) == nullptr);

		//The others too: the frame goes, and the undo brings it back
	StripFrameItem::takeOut(bench.sheet, bench.first(3));
	CHECK(bench.frames().isEmpty());
	bench.sheet->undoStack().undo();
	CHECK(bench.frames().size() == 1);

		//Renamed, the terminals say the new name
	frame->rename(QStringLiteral("X9"));
	CHECK(frame->stripName() == QStringLiteral("X9"));
	CHECK(comment(bench.terminals.at(0)) == QStringLiteral("X9"));
}

TEST_CASE("T85 - The frame selected and dragged takes its terminals", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	REQUIRE(bench.frames().size() == 1);
	StripFrameItem *frame = bench.frames().first();

		//Selected by a click on its border, as asked on 23/10/2026
	CHECK(frame->flags() & QGraphicsItem::ItemIsSelectable);
	frame->setSelected(true);
	CHECK(bench.sheet->selectedItems() == QList<QGraphicsItem *>{frame});

	const QRectF before = frame->frameRect();
	const int steps = bench.sheet->undoStack().count();
	frame->moveTerminals(QPointF(40, 20));
	CHECK(bench.sheet->undoStack().count() == steps + 1);
	const QList<int> xs {200, 230, 260, 290};
	for (int i = 0 ; i < 4 ; ++i) {
		CHECK(bench.terminals.at(i)->pos() == QPointF(xs.at(i) + 40, 320));
	}
		//The two left out of the frame do not move
	CHECK(bench.terminals.at(4)->pos() == QPointF(500, 300));
	CHECK(frame->frameRect() == before.translated(40, 20));
		//And the frame is what stays selected
	CHECK(bench.sheet->selectedItems() == QList<QGraphicsItem *>{frame});

	bench.sheet->undoStack().undo();
	CHECK(bench.terminals.at(0)->pos() == QPointF(200, 300));
	CHECK(frame->frameRect() == before);
}

TEST_CASE("T85 - The terminals of a frame put apart", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	StripFrameItem *frame = bench.frames().first();
	CHECK(frame->lies());
	CHECK(frame->spacing() == 30);

	const qreal width = frame->frameRect().width();
	const int steps = bench.sheet->undoStack().count();
	frame->setSpacing(40);
	CHECK(bench.sheet->undoStack().count() == steps + 1);
	const QList<int> xs {200, 240, 280, 320};
	for (int i = 0 ; i < 4 ; ++i) {
		CHECK(bench.terminals.at(i)->pos() == QPointF(xs.at(i), 300));
	}
	CHECK(frame->spacing() == 40);
	CHECK(frame->frameRect().width() == Approx(width + 30));
		//Moved one by one, and still not wired to each other
	for (Element *e : bench.first(4)) {
		CHECK(e->conductors().isEmpty());
	}

	bench.sheet->undoStack().undo();
	for (int i = 0 ; i < 4 ; ++i) {
		CHECK(bench.terminals.at(i)->pos() == QPointF(200 + 30 * i, 300));
	}
	bench.sheet->undoStack().redo();
	CHECK(bench.terminals.at(3)->pos() == QPointF(320, 300));
}

TEST_CASE("T85 - The frame in the properties dock", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	StripFrameItem *frame = bench.frames().first();
	frame->setSelected(true);

	PropertiesEditorWidget *editor = PropertiesEditorFactory::propertiesEditor(
			bench.sheet->selectedItems(), nullptr, nullptr);
	auto *widget = qobject_cast<StripFramePropertiesWidget *>(editor);
	REQUIRE(widget != nullptr);
	CHECK(widget->frame() == frame);
	auto *list = widget->findChild<QListWidget *>();
	REQUIRE(list != nullptr);
	CHECK(list->count() == 4);
	CHECK(list->item(2)->text() == QStringLiteral("3"));

		//A terminal picked in the list is the one selected on the folio
	widget->pick(2);
	CHECK(bench.sheet->selectedItems() == QList<QGraphicsItem *>{bench.terminals.at(2)});
	delete editor;
}

TEST_CASE("T85 - The frame selected turns its strip as one piece", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
		//A long name: it makes the frame far wider at its side
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1 - Control terminals"));
	REQUIRE(bench.frames().size() == 1);
	StripFrameItem *frame = bench.frames().first();
		//The middle of what the four draw
	auto middle = [&bench]() {
		QRectF drawn;
		for (Element *e : bench.first(4)) {
			drawn |= e->mapRectToScene(e->drawnRect());
		}
		return drawn.center();
	};
		//A wire from the first of the strip to a terminal out of it
	Terminal *own = bench.terminals.at(0)->terminals().last();
	auto *wire = new Conductor(own, bench.terminals.at(4)->terminals().last());
	bench.sheet->addItem(wire);

		//Terminals selected without their frame are not its business
	bench.terminals.at(0)->setSelected(true);
	const int untouched = bench.sheet->undoStack().count();
	CHECK_FALSE(StripFrameItem::rotateSelection(bench.sheet));
	CHECK(bench.sheet->undoStack().count() == untouched);
	bench.sheet->clearSelection();

	frame->setSelected(true);
	REQUIRE(bench.sheet->selectedItems() == QList<QGraphicsItem *>{frame});
	REQUIRE(frame->lies());
	const QRectF before = frame->frameRect();
	const QRectF caption = frame->captionRect();
	REQUIRE(caption.width() > 40);
	const QRectF first_body = bench.terminals.at(0)->mapRectToScene(bench.terminals.at(0)->drawnRect());
		//The name at the left of the strip lying
	CHECK(caption.right() <= first_body.left());
	const QPointF was = middle();
	const QPointF pivot = Diagram::snapToGrid(was);
	QList<QPointF> from;
	for (Element *e : bench.first(4)) {
		from << e->pos();
	}

	const int steps = bench.sheet->undoStack().count();
	CHECK(StripFrameItem::rotateSelection(bench.sheet));
	CHECK(bench.sheet->undoStack().count() == steps + 1);

		//A quarter turn round the middle of the strip, all together: each
		//turned in place, and the distances between them turned with them
	for (int i = 0 ; i < 4 ; ++i)
	{
		CAPTURE(i);
		Element *e = bench.terminals.at(i);
		const QPointF d = from.at(i) - pivot;
		CHECK(e->rotation() == Approx(90));
		CHECK(e->pos() == pivot + QPointF(-d.y(), d.x()));
		const QPointF apart = from.at(i) - from.at(0);
		CHECK(e->pos() - bench.terminals.at(0)->pos() == QPointF(-apart.y(), apart.x()));
	}
		//Round the middle of the strip, and not of the frame, which the
		//name makes wider at the left: the strip stays where it was, to a
		//grid step, and is not carried half the width of its name away
	const QPointF moved = middle() - was;
	CHECK(std::abs(moved.x()) <= 10);
	CHECK(std::abs(moved.y()) <= 10);
		//The ones out of the frame do not move
	CHECK(bench.terminals.at(4)->pos() == QPointF(500, 300));
	CHECK(bench.terminals.at(4)->rotation() == Approx(0));
		//The wire stays on its terminal
	const QPainterPath path = wire->mapToScene(wire->path());
	REQUIRE(path.elementCount() >= 2);
	const QPointF dock = own->dockConductor();
	CHECK((samePoint(path.elementAt(0), dock)
	       || samePoint(path.elementAt(path.elementCount() - 1), dock)));

		//The frame stands round them, and its name stays upright, the same
		//box, now at the top
	const QRectF after = frame->frameRect();
	CHECK_FALSE(frame->lies());
	CHECK(after.height() > after.width());
	for (Element *e : bench.first(4)) {
		CHECK(after.contains(e->mapRectToScene(e->drawnRect())));
	}
	CHECK(frame->sceneTransform().isIdentity());
	const QRectF name = frame->captionRect();
	CHECK(name.width() == Approx(caption.width()));
	CHECK(name.height() == Approx(caption.height()));
	CHECK(name.bottom() <= bench.terminals.at(0)->mapRectToScene(bench.terminals.at(0)->drawnRect()).top());
	CHECK(name.center().x() == Approx(after.center().x()));
		//And the frame is what stays selected
	CHECK(bench.sheet->selectedItems() == QList<QGraphicsItem *>{frame});

		//One undo puts the strip back as it was
	bench.sheet->undoStack().undo();
	for (int i = 0 ; i < 4 ; ++i)
	{
		CAPTURE(i);
		CHECK(bench.terminals.at(i)->pos() == from.at(i));
		CHECK(bench.terminals.at(i)->rotation() == Approx(0));
	}
	CHECK(frame->lies());
	CHECK(frame->frameRect() == before);
	CHECK(frame->captionRect() == caption);

		//Four turns bring it back where it was, to the pixel: the middle
		//of four terminals three steps apart is half a step off the grid,
		//and each turn goes round the point the one before went round
	for (int turn = 0 ; turn < 4 ; ++turn) {
		CHECK(StripFrameItem::rotateSelection(bench.sheet));
	}
	for (int i = 0 ; i < 4 ; ++i)
	{
		CAPTURE(i);
		CHECK(bench.terminals.at(i)->pos() == from.at(i));
		CHECK(bench.terminals.at(i)->rotation() == Approx(360));
	}
	CHECK(frame->frameRect() == before);
}

TEST_CASE("T85 - Turned with its frame, a terminal turns once", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	REQUIRE(bench.frames().size() == 1);
	StripFrameItem *frame = bench.frames().first();

		//The frame, one of its terminals, and one out of it, as a rubber
		//band takes them
	frame->setSelected(true);
	bench.terminals.at(1)->setSelected(true);
	bench.terminals.at(4)->setSelected(true);
	const QList<QGraphicsItem *> selection = bench.sheet->selectedItems();
	const int steps = bench.sheet->undoStack().count();
	CHECK(StripFrameItem::rotateSelection(bench.sheet));
	CHECK(bench.sheet->undoStack().count() == steps + 1);

		//The terminal of the frame turns with the frame, and not once more
		//in place; the one out of it turns in place, as the folio turns it
	for (Element *e : bench.first(4)) {
		CHECK(e->rotation() == Approx(90));
	}
	CHECK(bench.terminals.at(4)->rotation() == Approx(90));
	CHECK(bench.terminals.at(4)->pos() == QPointF(500, 300));
	CHECK_FALSE(frame->lies());
		//The selection is what it was
	CHECK(bench.sheet->selectedItems().size() == selection.size());
	for (QGraphicsItem *item : selection) {
		CHECK(item->isSelected());
	}

	bench.sheet->undoStack().undo();
	for (int i = 0 ; i < 5 ; ++i)
	{
		CAPTURE(i);
		CHECK(bench.terminals.at(i)->rotation() == Approx(0));
	}
	CHECK(bench.terminals.at(0)->pos() == QPointF(200, 300));
	CHECK(frame->lies());
}

TEST_CASE("T85 - Delete takes the frame off and leaves its terminals", "[T85][stripframe]")
{
	Bench bench;
	REQUIRE(bench.open());
	StripFrameItem::drawAround(bench.sheet, bench.first(4), QStringLiteral("X1"));
	REQUIRE(bench.frames().size() == 1);
	StripFrameItem *frame = bench.frames().first();
	const QRectF rect = frame->frameRect();

		//Only the frame selected
	frame->setSelected(true);
	int steps = bench.sheet->undoStack().count();
	deleteSelection(bench.sheet);
	CHECK(bench.sheet->undoStack().count() == steps + 1);
	CHECK(bench.frames().isEmpty());
	for (Element *e : bench.first(4))
	{
		CHECK(e->scene() == bench.sheet);
		CHECK(StripFrameItem::frameOf(e) == nullptr);
			//Still of that strip, and the text that says so shows again
		CHECK(comment(e) == QStringLiteral("X1"));
		CHECK(commentText(e)->isVisible());
	}
	bench.sheet->undoStack().undo();
	REQUIRE(bench.frames().size() == 1);
	CHECK(frame->terminals().size() == 4);
	CHECK(StripFrameItem::frameOf(bench.terminals.at(0)) == frame);
	CHECK_FALSE(commentText(bench.terminals.at(0))->isVisible());
	CHECK(frame->frameRect() == rect);

		//With a component out of it: both go in one step, and come back
	frame->setSelected(true);
	bench.terminals.at(4)->setSelected(true);
		//The place in the stack, and not the count: the step undone above
		//is dropped when this one is pushed
	steps = bench.sheet->undoStack().index();
	deleteSelection(bench.sheet);
	CHECK(bench.sheet->undoStack().index() == steps + 1);
	CHECK(bench.frames().isEmpty());
	CHECK(bench.terminals.at(4)->scene() == nullptr);
	CHECK(bench.terminals.at(0)->scene() == bench.sheet);
	bench.sheet->undoStack().undo();
	CHECK(bench.frames().size() == 1);
	CHECK(bench.terminals.at(4)->scene() == bench.sheet);
	CHECK(frame->terminals().size() == 4);

		//With its own terminals: all of it goes, and comes back whole
	frame->setSelected(true);
	for (Element *e : bench.first(4)) {
		e->setSelected(true);
	}
	deleteSelection(bench.sheet);
	CHECK(bench.frames().isEmpty());
	CHECK(bench.terminals.at(0)->scene() == nullptr);
	bench.sheet->undoStack().undo();
	REQUIRE(bench.frames().size() == 1);
	CHECK(frame->terminals().size() == 4);
	CHECK(frame->frameRect() == rect);
}
