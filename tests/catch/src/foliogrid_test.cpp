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
#include "../../../sources/foliogrid.h"
#include "../../../sources/griddots.h"
#include "../../../sources/textgrid.h"

#include <catch2/catch.hpp>

#include <QPointF>

/*
	The folio grid with a step under 1 (T69): the settings keep it, a
	point snaps to it, and the dots of the grid are still drawn far enough
	apart.
*/
TEST_CASE("CU-69.1 — The settings keep a grid of 0.5", "[T69][grid]")
{
	CHECK(FolioGrid::step(QVariant(0.5), 10) == Approx(0.5));
	CHECK(FolioGrid::step(QVariant(0.25), 10) == Approx(0.25));
		//As the settings file of some systems holds it: text
	CHECK(FolioGrid::step(QVariant(QStringLiteral("0.5")), 10) == Approx(0.5));
		//A whole step, as before
	CHECK(FolioGrid::step(QVariant(10), 1) == Approx(10));
	CHECK(FolioGrid::step(QVariant(FolioGrid::smallest), 10) == Approx(FolioGrid::smallest));

	SECTION("what is no step falls back")
	{
		CHECK(FolioGrid::step(QVariant(0), 10) == Approx(10));
		CHECK(FolioGrid::step(QVariant(-1), 10) == Approx(10));
		CHECK(FolioGrid::step(QVariant(0.05), 10) == Approx(10));
		CHECK(FolioGrid::step(QVariant(QStringLiteral("dez")), 10) == Approx(10));
		CHECK(FolioGrid::step(QVariant(), 10) == Approx(10));
		CHECK(FolioGrid::step(QVariant(1e12), 10) == Approx(10));
	}
}

TEST_CASE("T69 — The snapping rule on a grid of 0.5", "[T69][grid]")
{
	CHECK(FolioGrid::snap(10.3, 0.5) == Approx(10.5));
	CHECK(FolioGrid::snap(7.8, 0.5) == Approx(8));
	CHECK(FolioGrid::snap(7.7, 0.5) == Approx(7.5));
		//On a whole grid, as before
	CHECK(FolioGrid::snap(224, 10) == Approx(220));
	CHECK(FolioGrid::snap(226, 10) == Approx(230));

	SECTION("a step of 0.1 leaves nothing in the last digits")
	{
			//103 times 0.1 is 10.300000000000001 in floating point
		CHECK(FolioGrid::snap(10.31, 0.1) == 10.3);
		CHECK(FolioGrid::snap(0.29, 0.1) == 0.3);
	}
	SECTION("with Ctrl, never coarser than the grid")
	{
		CHECK(FolioGrid::freeStep(10) == Approx(1));
		CHECK(FolioGrid::freeStep(1) == Approx(1));
		CHECK(FolioGrid::freeStep(0.5) == Approx(0.5));
	}
	SECTION("the grid of the texts, a fraction of a grid of 0.5")
	{
		const QPointF p = TextGrid::snap(QPointF(10.3, 7.8), 0.5, 0.5, 2);
		CHECK(p.x() == Approx(10.25));
		CHECK(p.y() == Approx(7.75));
	}
}

TEST_CASE("T69 — Which dots of a grid of 0.5 are drawn", "[T69][grid]")
{
		//At zoom 10 a step of 0.5 is 5 pixels: every dot is drawn
	CHECK(GridDots::stride(0.5, 10) == 1);
		//At zoom 1 it is half a pixel: one in ten, every 5 units, still
		//points of the grid
	CHECK(GridDots::stride(0.5, 1) == 10);
	CHECK(GridDots::stride(0.25, 4) == 5);
}
