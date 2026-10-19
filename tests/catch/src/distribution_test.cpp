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
#include "qt_catch_tostring.h"

#include "../../../sources/alignment.h"

#include <catch2/catch.hpp>

/*
	Distributing a selection on the folio: the pieces go in the order they
	have on the folio, the first one stays, and each other one goes one
	pitch after the one before it, centre to centre. The automatic pitch
	is the smallest whole number of grid steps at which no box touches the
	next one - for a symbol, its outline and its texts together.
*/
namespace
{
		/// An item centred on (@a x, @a y), 20 wide, its box reaching
		/// @a up above the centre and @a down under it
	Alignment::Item item(qreal x, qreal y, qreal up, qreal down)
	{
		return Alignment::Item{QRectF(x - 10, y - up, 20, up + down), QPointF(x, y)};
	}
}

TEST_CASE("T65 — Distributing keeps the first one and the order of the folio",
		  "[T65][distribution]")
{
	using namespace Alignment;

		//Listed out of order: the one at 50 is the top one
	const QList<Item> items{item(0, 150, 10, 10), item(0, 50, 10, 10), item(5, 90, 10, 10)};
	CHECK(distributionOrder(items, Vertical) == QList<int>{1, 2, 0});

	const QList<QPointF> offsets = distributeOffsets(items, Vertical, 40);
	CHECK(offsets.at(1) == QPointF());			//The top one stays
	CHECK(offsets.at(2) == QPointF());			//Already at 50 + 40
	CHECK(offsets.at(0) == QPointF(0, -20));	//To 50 + 2 x 40

		//On the same line, the one further left comes first
	const QList<Item> tie{item(30, 0, 10, 10), item(10, 0, 10, 10)};
	CHECK(distributionOrder(tie, Vertical) == QList<int>{1, 0});
}

TEST_CASE("T65 — The automatic pitch is the one the largest box needs",
		  "[T65][distribution]")
{
	using namespace Alignment;

		//10 above and 10 under each centre, and 2 between: 22, so 3 steps of 10
	QList<Item> items{item(0, 0, 10, 10), item(0, 100, 10, 10), item(0, 200, 10, 10)};
	CHECK(distributionPitch(items, Vertical, 10, 2) == 3);

		//A text 35 under the second one: 35 + 10 + 2 = 47, so 5 steps
	items[1] = item(0, 100, 10, 35);
	CHECK(distributionPitch(items, Vertical, 10, 2) == 5);

		//A whole number of steps is not rounded up once more: 9 + 9 + 2
	const QList<Item> exact{item(0, 0, 9, 9), item(0, 50, 9, 9)};
	CHECK(distributionPitch(exact, Vertical, 10, 2) == 2);

		//Nothing to space: one step
	CHECK(distributionPitch({item(0, 0, 10, 10)}, Vertical, 10, 2) == 1);
}

TEST_CASE("T65 — Distributing to the right moves nothing up or down",
		  "[T65][distribution]")
{
	using namespace Alignment;

	const QList<Item> items{item(100, 0, 10, 10), item(0, 30, 10, 10), item(230, 7, 10, 10)};
	CHECK(distributionOrder(items, Horizontal) == QList<int>{1, 0, 2});
		//20 wide: 10 + 10 + 2, so 3 steps of 10
	CHECK(distributionPitch(items, Horizontal, 10, 2) == 3);

	const QList<QPointF> offsets = distributeOffsets(items, Horizontal, 30);
	CHECK(offsets.at(1) == QPointF());
	CHECK(offsets.at(0) == QPointF(-70, 0));
	CHECK(offsets.at(2) == QPointF(-170, 0));
}
