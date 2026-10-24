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

#include "../../../sources/wirepath.h"

#include <catch2/catch.hpp>

/*
	The path of a conductor made clean after a segment was dragged (T68):
	a terminal pointing down at (0, 0), its segment down to (0, 40), and the
	conductor going right at the height it was dragged to.
*/
TEST_CASE("CU-68.5 — The path made clean", "[T68][wirepath]")
{
	SECTION("a segment running back over the one before it goes")
	{
			//Dragged up to 25: the inserted segment runs back from 40
		const QList<QPointF> tail{{0, 0}, {0, 40}, {0, 25}, {100, 25}, {100, 60}, {120, 60}};
		const QList<QPointF> clean{{0, 0}, {0, 25}, {100, 25}, {100, 60}, {120, 60}};
		CHECK(WirePath::simplified(tail) == clean);
	}
	SECTION("two segments on one line, in the same direction, stay two")
	{
			//The segment of the terminal and the next one, as every
			//automatic path has them: the second one keeps its handle
		const QList<QPointF> path{{0, 0}, {0, 10}, {0, 40}, {100, 40}, {100, 60}};
		CHECK(WirePath::simplified(path) == path);
	}
	SECTION("turning back past the start of the segment before")
	{
			//Dragged up to 5: back past the end of the terminal's segment
			//and past the point before it
		const QList<QPointF> tail{{0, 0}, {0, 10}, {0, 40}, {0, 5}, {100, 5}};
		const QList<QPointF> clean{{0, 0}, {0, 5}, {100, 5}};
		CHECK(WirePath::simplified(tail) == clean);
	}
	SECTION("a point repeated goes")
	{
		const QList<QPointF> path{{0, 0}, {0, 40}, {0, 40}, {100, 40}, {100, 60}};
		const QList<QPointF> clean{{0, 0}, {0, 40}, {100, 40}, {100, 60}};
		CHECK(WirePath::simplified(path) == clean);
	}
	SECTION("the ends stay, and a conductor keeps three points")
	{
		const QList<QPointF> straight{{0, 0}, {0, 30}, {0, 10}, {0, 80}};
		const QList<QPointF> clean{{0, 0}, {0, 10}, {0, 80}};
		CHECK(WirePath::simplified(straight) == clean);
		const QList<QPointF> three{{0, 0}, {0, 40}, {0, 80}};
		CHECK(WirePath::simplified(QList<QPointF>{{0, 0}, {0, 80}, {0, 80}}) == three);
	}
	SECTION("a clean path is left as it is")
	{
		const QList<QPointF> path{{0, 0}, {0, 25}, {100, 25}, {100, 60}, {120, 60}};
		CHECK(WirePath::simplified(path) == path);
	}
}

TEST_CASE("CU-68.7 — Dragging a segment, without a sheet", "[T68][wirepath]")
{
		//A terminal pointing down at (0, 0), its segment long, to 88, and
		//the conductor going right at 88, down to 120 and to a terminal at
		//(220, 120)
	const QList<QPointF> long_arm{{0, 0}, {0, 88}, {200, 88}, {200, 120}, {220, 120}};

	SECTION("towards the terminal, the segment of the terminal gets shorter")
	{
		QPointF middle;
		const QList<QPointF> bent{{0, 0}, {0, 62}, {200, 62}, {200, 120}, {220, 120}};
		CHECK(WirePath::dragged(long_arm, 1, QPointF(0, -26), &middle) == bent);
		CHECK(middle == QPointF(100, 62));
	}
	SECTION("only the part of the drag across the segment counts")
	{
		const QList<QPointF> bent{{0, 0}, {0, 62}, {200, 62}, {200, 120}, {220, 120}};
		CHECK(WirePath::dragged(long_arm, 1, QPointF(37, -26)) == bent);
	}
	SECTION("past the terminal, the drag stops one unit from it")
	{
		QPointF middle;
		const QList<QPointF> stopped{{0, 0}, {0, 1}, {200, 1}, {200, 120}, {220, 120}};
		CHECK(WirePath::dragged(long_arm, 1, QPointF(0, -100), &middle) == stopped);
		CHECK(middle == QPointF(100, 1));
	}
	SECTION("the segment after the terminal's, on its line, turns back and goes")
	{
			//The automatic path: the terminal's segment to 10, the next one
			//on to 40, then right
		const QList<QPointF> automatic{{0, 0}, {0, 10}, {0, 40}, {100, 40}, {100, 60}, {120, 60}};
		const QList<QPointF> bent{{0, 0}, {0, 2}, {100, 2}, {100, 60}, {120, 60}};
		CHECK(WirePath::dragged(automatic, 2, QPointF(0, -38)) == bent);
		const QList<QPointF> stopped{{0, 0}, {0, 1}, {100, 1}, {100, 60}, {120, 60}};
		CHECK(WirePath::dragged(automatic, 2, QPointF(0, -45)) == stopped);
	}
	SECTION("a neighbour on the same line gets a segment joining the two")
	{
		const QList<QPointF> path{{0, 0}, {10, 0}, {50, 0}, {50, 30}, {90, 30}, {100, 30}};
		const QList<QPointF> jog{{0, 0}, {10, 0}, {10, 10}, {50, 10}, {50, 30}, {90, 30}, {100, 30}};
		CHECK(WirePath::dragged(path, 1, QPointF(0, 10)) == jog);
	}
	SECTION("the segments of the terminals do not drag")
	{
		CHECK(WirePath::dragged(long_arm, 0, QPointF(30, 0)) == long_arm);
		CHECK(WirePath::dragged(long_arm, 3, QPointF(0, 30)) == long_arm);
	}
	SECTION("the other terminal holds too")
	{
			//The upright segment dragged right past the terminal at 220
		const QList<QPointF> stopped{{0, 0}, {0, 88}, {219, 88}, {219, 120}, {220, 120}};
		CHECK(WirePath::dragged(long_arm, 2, QPointF(50, 0)) == stopped);
	}
}

/*
	A run that leaves its terminal to the side (Conductor::handlerSegments()):
	a terminal pointing down at (0, 0), the run going right at its end to
	(100, 0), then down. The path is handed over with the segment of the
	terminal of no length - the first point twice -, and the way the
	terminal points.
*/
TEST_CASE("T67 - A run that leaves its terminal to the side drags", "[T67][wirepath]")
{
	const QList<QPointF> path{{0, 0}, {0, 0}, {100, 0}, {100, 50}, {110, 50}};
	const QPointF down(0, 1);
	SECTION("away from the terminal, its segment grows")
	{
		QPointF middle;
		const QList<QPointF> moved{{0, 0}, {0, 20}, {100, 20}, {100, 50}, {110, 50}};
		CHECK(WirePath::dragged(path, 1, QPointF(0, 20), &middle, 1.0, down) == moved);
		CHECK(middle == QPointF(50, 20));
	}
	SECTION("into the terminal, it stays at its end")
	{
		const QList<QPointF> kept{{0, 0}, {100, 0}, {100, 50}, {110, 50}};
		CHECK(WirePath::dragged(path, 1, QPointF(0, -20), nullptr, 1.0, down) == kept);
	}
	SECTION("the way unknown, any way, as before")
	{
		const QList<QPointF> moved{{0, 0}, {0, -20}, {100, -20}, {100, 50}, {110, 50}};
		CHECK(WirePath::dragged(path, 1, QPointF(0, -20)) == moved);
	}
}
