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
#include <catch2/catch.hpp>

#include "../../../sources/griddots.h"

#include <QList>

/*
	Which dots of the folio grid are drawn. The zoom is pixels per folio
	unit, as Diagram::drawBackground reads it from the painter.
*/

namespace
{
		/// The strides the grid may use, in order
	const QList<int> ladder{1, 2, 5, 10, 20, 50, 100};
}

TEST_CASE("CU-60.4 — A grid of 1 seen from afar draws fewer dots, still on the grid",
		  "[T60][grid]")
{
	SECTION("the steps of a grid of 1 with a dot of one pixel")
	{
		CHECK(GridDots::stride(1, 1.0) == 5);
		CHECK(GridDots::stride(1, 0.6) == 10);
		CHECK(GridDots::stride(1, 3.0) == 2);
		CHECK(GridDots::stride(1, 4.5) == 2);
		CHECK(GridDots::stride(1, 5.0) == 1);
		CHECK(GridDots::stride(1, 12.0) == 1);
	}

	SECTION("at every zoom the dots drawn are 5 pixels apart or more, and "
			"the stride before it in the ladder would put them closer")
	{
		for (int grid = 1 ; grid <= 30 ; ++grid) {
			for (qreal zoom = 0.51 ; zoom <= 10.0 ; zoom += 0.07) {
				CAPTURE(grid, zoom);
				const int stride = GridDots::stride(grid, zoom);
				REQUIRE(ladder.contains(stride));
				CHECK(grid * stride * zoom >= GridDots::min_gap_px);
				const int index = ladder.indexOf(stride);
				if (index > 0) {
					CHECK(grid * ladder.at(index - 1) * zoom < GridDots::min_gap_px);
				}
			}
		}
	}

	SECTION("a wider dot needs a wider gap: more than twice its width")
	{
			//3 pixels wide: 7 pixels apart at least
		CHECK(GridDots::stride(1, 3.0, 3) == 5);
		CHECK(GridDots::stride(1, 7.0, 3) == 1);
		CHECK(GridDots::stride(1, 6.9, 3) == 2);
	}

	SECTION("nothing sensible to compute: every dot")
	{
		CHECK(GridDots::stride(0, 1.0) == 1);
		CHECK(GridDots::stride(10, 0.0) == 1);
		CHECK(GridDots::stride(-5, 1.0) == 1);
	}
}

TEST_CASE("CU-60.5 — The default grid of 10 draws every dot, as it always did",
		  "[T60][grid]")
{
		//Drawn only above zoom 0.5 (Diagram::drawBackground)
	for (qreal zoom = 0.501 ; zoom <= 20.0 ; zoom += 0.013) {
		CAPTURE(zoom);
		CHECK(GridDots::stride(10, zoom) == 1);
	}
}
