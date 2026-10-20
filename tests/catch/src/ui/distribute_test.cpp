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
#include "../../../../sources/foliogrid.h"
#include "../../../../sources/itemgroups.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/alignselectioncommand.h"

#include <catch2/catch.hpp>

#include <QSettings>
#include <QUndoStack>
#include <QUuid>

/*
	Distributing the selected symbols of a folio (T65): the top one (or
	the left one) stays, and the others follow it at one pitch, centre to
	centre, on the grid. The pitch proposed is the smallest at which no
	symbol, texts included, touches the next one. A group moves as one,
	a locked symbol stays, and one undo puts everything back.

	The bench: five symbols of 20 by 40, centred on their origin, each with
	a text of its own to its right - a column at x = 200 with the distances
	all different, or the same five on a row.
*/
namespace
{
	QString definition()
	{
		return QStringLiteral(
			"<element name=\"block.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">block</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-18\" width=\"16\" height=\"36\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-20\" orientation=\"n\"/>"
			"<terminal x=\"0\" y=\"20\" orientation=\"s\"/>"
			"</description>"
			"</definition>"
			"</element>");
	}

	/// A project of one folio with a block at each of @a positions
	QString fixtureXml(const QList<QPoint> &positions)
	{
		QString instances;
		for (int i = 0 ; i < positions.size() ; ++i)
		{
			instances += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"0\" type=\"embed://bench/block.elmt\""
				" uuid=\"{d15b0000-0000-4000-8000-00000000000%3}\">"
				"<terminals/><inputs/><elementInformations/>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(positions.at(i).x()).arg(positions.at(i).y()).arg(i);
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
			.arg(definition(), instances);
	}

	/// The blocks of @a sheet, in the order of their uuid: the order of the fixture
	QList<Element *> blocks(Diagram *sheet)
	{
		QList<Element *> list = sheet->elements();
		std::sort(list.begin(), list.end(), [](Element *a, Element *b) {
			return a->uuid().toString() < b->uuid().toString();
		});
		return list;
	}

	/// Give @a element a text of its own, @a text, to its right
	DynamicElementTextItem *addText(Element *element, const QString &text)
	{
		auto *item = new DynamicElementTextItem(element);
		item->setTextFrom(DynamicElementTextItem::UserText);
		item->setText(text);
		item->setPos(14, -8);
		element->addDynamicTextItem(item);
		return item;
	}

	qreal gridStep(bool vertical)
	{
		QSettings settings;
		return vertical ? FolioGrid::step(settings.value(FolioGrid::y_key), Diagram::yGrid)
						: FolioGrid::step(settings.value(FolioGrid::x_key), Diagram::xGrid);
	}

	/// Select @a elements and nothing else
	void select(Diagram *sheet, const QList<Element *> &elements)
	{
		sheet->clearSelection();
		for (Element *element : elements)
			element->setSelected(true);
	}

	/// The box of @a element with the texts it shows, as the command measures it
	QRectF box(Element *element)
	{
		QRectF rect = element->sceneBoundingRect();
		for (DynamicElementTextItem *text : element->dynamicTextItems())
			if (!text->toPlainText().trimmed().isEmpty())
				rect = rect.united(text->sceneBoundingRect());
		return rect;
	}

	const QList<QPoint> column{{200, 100}, {200, 130}, {200, 220}, {200, 260}, {200, 400}};
}

TEST_CASE("T65 — A column distributed downwards", "[T65][distribute]")
{
	UiBench::ScratchProject bench(fixtureXml(column));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	const QList<Element *> list = blocks(sheet);
	REQUIRE(list.size() == 5);
	for (int i = 0 ; i < list.size() ; ++i)
		addText(list.at(i), QStringLiteral("X1:%1").arg(i + 1));
	select(sheet, list);

	const qreal grid = gridStep(true);
	const int steps = AlignSelectionCommand::automaticPitch(sheet, AlignSelectionCommand::DistributeVertical);
	CAPTURE(grid, steps);
	REQUIRE(steps > 0);

	auto *command = new AlignSelectionCommand(sheet, AlignSelectionCommand::DistributeVertical, steps);
	REQUIRE(command->isValid());
	sheet->undoStack().push(command);

		//The top one stays, the others follow it at the pitch, on the grid,
		//and nobody moves sideways
	for (int i = 0 ; i < list.size() ; ++i)
	{
		CAPTURE(i);
		CHECK(list.at(i)->pos() == QPointF(200, 100 + i * steps * grid));
	}
		//And no box runs into the next one: tight, they may touch
	for (int i = 0 ; i + 1 < list.size() ; ++i)
	{
		CAPTURE(i);
		CHECK(box(list.at(i)).bottom() <= box(list.at(i + 1)).top() + 1e-6);
	}
		//One pitch less, and two of them would overlap: it is the smallest
	bool overlap = false;
	for (int i = 0 ; i + 1 < list.size() ; ++i)
		overlap |= box(list.at(i + 1)).top() - grid < box(list.at(i)).bottom() - 1e-6;
	CHECK(overlap);

		//One undo puts all of them back
	sheet->undoStack().undo();
	for (int i = 0 ; i < list.size() ; ++i)
	{
		CAPTURE(i);
		CHECK(list.at(i)->pos() == QPointF(column.at(i)));
	}

	SECTION("a chosen pitch is the one used")
	{
		select(sheet, list);
		sheet->undoStack().push(new AlignSelectionCommand(sheet, AlignSelectionCommand::DistributeVertical,
														  steps + 2));
		CHECK(list.at(4)->pos() == QPointF(200, 100 + 4 * (steps + 2) * grid));
	}
}

TEST_CASE("T65 — A longer text asks for a longer pitch", "[T65][distribute]")
{
	UiBench::ScratchProject bench(fixtureXml(column));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	const QList<Element *> list = blocks(sheet);
	REQUIRE(list.size() == 5);
	QList<DynamicElementTextItem *> texts;
	for (int i = 0 ; i < list.size() ; ++i)
		texts << addText(list.at(i), QStringLiteral("X1:%1").arg(i + 1));
	select(sheet, list);
	const int before = AlignSelectionCommand::automaticPitch(sheet, AlignSelectionCommand::DistributeVertical);

		//A text standing on end, longer than the symbol: it is what sets the pitch
	texts.at(2)->setText(QStringLiteral("X1:5.1.12, a terminal of three levels"));
	texts.at(2)->setRotation(90);
	const int after = AlignSelectionCommand::automaticPitch(sheet, AlignSelectionCommand::DistributeVertical);
	CAPTURE(before, after);
	CHECK(after > before);
}

/*
	A text that shows nothing - an information the symbol does not have,
	a text left blank - is still a box on the folio, often away from the
	symbol. Counted, it made the pitch proposed for a column of terminals
	far longer than the tags needed. The same text, written, counts.
*/
TEST_CASE("T65 — An empty text does not ask for room", "[T65][distribute]")
{
	UiBench::ScratchProject bench(fixtureXml(column));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	const QList<Element *> list = blocks(sheet);
	REQUIRE(list.size() == 5);
	for (int i = 0 ; i < list.size() ; ++i)
		addText(list.at(i), QStringLiteral("X1:%1").arg(i + 1));
	select(sheet, list);
	const int before = AlignSelectionCommand::automaticPitch(sheet, AlignSelectionCommand::DistributeVertical);

		//An empty text well under the second symbol
	DynamicElementTextItem *empty = addText(list.at(1), QString());
	empty->setPos(0, 90);
	const int with_empty = AlignSelectionCommand::automaticPitch(sheet, AlignSelectionCommand::DistributeVertical);

		//The same text, written: now it counts
	empty->setText(QStringLiteral("K1"));
	const int written = AlignSelectionCommand::automaticPitch(sheet, AlignSelectionCommand::DistributeVertical);
	CAPTURE(before, with_empty, written);
	CHECK(with_empty == before);
	CHECK(written > before);
}

TEST_CASE("T65 — A group moves as one, and a locked symbol stays", "[T65][distribute]")
{
	UiBench::ScratchProject bench(fixtureXml(column));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	const QList<Element *> list = blocks(sheet);
	REQUIRE(list.size() == 5);

	const QUuid group = QUuid::createUuid();
	ItemGroups::setGroup(list.at(1), group);
	ItemGroups::setGroup(list.at(2), group);
	list.at(4)->setMovable(false);
	select(sheet, list);

	auto *command = new AlignSelectionCommand(sheet, AlignSelectionCommand::DistributeVertical, 0);
	CHECK(command->lockedCount() == 1);
	CHECK(command->itemCount() == 3);	//The top one, the group, and the fourth
	REQUIRE(command->isValid());
	sheet->undoStack().push(command);

		//The group keeps its shape
	CHECK(list.at(2)->pos() - list.at(1)->pos() == QPointF(column.at(2) - column.at(1)));
		//And the locked one did not move
	CHECK(list.at(4)->pos() == QPointF(column.at(4)));
		//The first stays, and the fourth follows the group
	CHECK(list.at(0)->pos() == QPointF(column.at(0)));
	CHECK(list.at(3)->pos().y() > list.at(2)->pos().y());
}

TEST_CASE("T65 — A row distributed to the right", "[T65][distribute]")
{
	const QList<QPoint> row{{300, 200}, {100, 200}, {160, 200}, {420, 200}};
	UiBench::ScratchProject bench(fixtureXml(row));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	const QList<Element *> list = blocks(sheet);
	REQUIRE(list.size() == 4);
	for (int i = 0 ; i < list.size() ; ++i)
		addText(list.at(i), QStringLiteral("K%1").arg(i + 1));
	select(sheet, list);

	const qreal grid = gridStep(false);
	const int steps = AlignSelectionCommand::automaticPitch(sheet, AlignSelectionCommand::DistributeHorizontal);
	REQUIRE(steps > 0);
	sheet->undoStack().push(new AlignSelectionCommand(sheet, AlignSelectionCommand::DistributeHorizontal, steps));

		//The order of the folio: 100, 160, 300, 420 - the second of the
		//fixture is the left one, and stays
	CHECK(list.at(1)->pos() == QPointF(100, 200));
	CHECK(list.at(2)->pos() == QPointF(100 + steps * grid, 200));
	CHECK(list.at(0)->pos() == QPointF(100 + 2 * steps * grid, 200));
	CHECK(list.at(3)->pos() == QPointF(100 + 3 * steps * grid, 200));
}
