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
