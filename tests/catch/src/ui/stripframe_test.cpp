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
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/stripframeitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"

#include <catch2/catch.hpp>

#include <QUndoStack>

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
				"<dynamic_texts/><texts_groups/>"
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

	void erase(Diagram *sheet, const QList<Element *> &elements)
	{
		sheet->clearSelection();
		for (Element *e : elements) e->setSelected(true);
		DiagramContent selection(sheet);
		sheet->clearSelection();
		sheet->undoStack().push(new DeleteQGraphicsItemCommand(sheet, selection));
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
	}
		//The name at the left of a strip that lies
	CHECK(rect.left() < bench.terminals.at(0)->mapRectToScene(bench.terminals.at(0)->drawnRect()).left() - 4);
	CHECK_FALSE(rect.contains(bench.terminals.at(4)->mapRectToScene(bench.terminals.at(4)->drawnRect())));

		//One undo step takes the frame and the names away
	bench.sheet->undoStack().undo();
	CHECK(bench.frames().isEmpty());
	for (Element *e : bench.first(4)) {
		CHECK(comment(e).isEmpty());
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
