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
#include "../../../sources/conductortextfit.h"

#include <catch2/catch.hpp>

#include <QList>

#include <limits>

/*
	The size the name of a conductor is drawn at so that it fits its run
	(T70). The width of the text is a function given here, 10 per point,
	so that every answer can be worked out by hand: at 9 the text takes
	90, at 5 it takes 50.

	The case on a folio, with a real font and a conductor drawn by mouse,
	is CU-70.7 in src/ui/wirenamefit_test.cpp.
*/
namespace
{
		/// The text takes 10 per point, and every size asked is kept
	struct Ruler
	{
		QList<int> asked;
		qreal operator()(int size)
		{
			asked << size;
			return 10.0 * size;
		}
	};
}

TEST_CASE("T70 — The largest size that fits", "[T70][wirename]")
{
	Ruler ruler;
	auto measure = [&ruler](int size) { return ruler(size); };

		//90 and 80 are wider than 75; 70 is not
	CHECK(ConductorTextFit::fittingSize(9, 75, measure) == 7);
		//From the size the file holds downwards, one at a time
	CHECK(ruler.asked == QList<int>({9, 8, 7}));

	SECTION("exactly as long as the room fits")
	{
		CHECK(ConductorTextFit::fittingSize(9, 60, measure) == 6);
		CHECK(ConductorTextFit::fittingSize(9, 59.9, measure) == 5);
	}
}

TEST_CASE("T70 — A name that fits keeps its size", "[T70][wirename]")
{
	Ruler ruler;
	auto measure = [&ruler](int size) { return ruler(size); };

	CHECK(ConductorTextFit::fittingSize(9, 90, measure) == 9);
	CHECK(ConductorTextFit::fittingSize(9, 1000, measure) == 9);
		//Measured at its own size, and that is all
	CHECK(ruler.asked == QList<int>({9, 9}));
		//Never enlarged, however much room there is
	CHECK(ConductorTextFit::fittingSize(7, 1000, measure) == 7);
}

TEST_CASE("T70 — Never below 5, even when it does not fit", "[T70][wirename]")
{
	Ruler ruler;
	auto measure = [&ruler](int size) { return ruler(size); };

	CHECK(ConductorTextFit::smallest == 5);
		//50 is still wider than 10: drawn at 5 all the same, past the run
	CHECK(ConductorTextFit::fittingSize(9, 10, measure) == 5);
	CHECK(ConductorTextFit::fittingSize(9, 0, measure) == 5);
	CHECK(ConductorTextFit::fittingSize(12, 0, measure) == 5);
		//No size outside the range is ever asked for
	REQUIRE_FALSE(ruler.asked.isEmpty());
	for (int size : ruler.asked)
	{
		CHECK(size > ConductorTextFit::smallest);
		CHECK(size <= 12);
	}
}

TEST_CASE("T70 — A size already at 5 or less stays", "[T70][wirename]")
{
	Ruler ruler;
	auto measure = [&ruler](int size) { return ruler(size); };

	CHECK(ConductorTextFit::fittingSize(5, 0, measure) == 5);
	CHECK(ConductorTextFit::fittingSize(4, 0, measure) == 4);
	CHECK(ConductorTextFit::fittingSize(1, 1000, measure) == 1);
		//What a damaged file can hold is not the rule's to mend
	CHECK(ConductorTextFit::fittingSize(0, 1000, measure) == 0);
	CHECK(ConductorTextFit::fittingSize(-3, 1000, measure) == -3);
		//None of them was measured
	CHECK(ruler.asked.isEmpty());
}

TEST_CASE("T70 — A width that is not a number never fits", "[T70][wirename]")
{
	const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
	CHECK(ConductorTextFit::fittingSize(9, 1000, [nan](int) { return nan; }) == 5);
	CHECK(ConductorTextFit::fittingSize(9, nan, [](int size) { return 10.0 * size; }) == 5);
}

TEST_CASE("T70 — The room of a run", "[T70][wirename]")
{
	const qreal clearance = ConductorTextFit::end_clearance;
	REQUIRE(clearance > 0);

		//The run less the clearance at both ends
	CHECK(ConductorTextFit::room(40) == Approx(40 - 2 * clearance));
	CHECK(ConductorTextFit::room(100) == Approx(100 - 2 * clearance));
		//A segment running backwards has a negative length
	CHECK(ConductorTextFit::room(-40) == Approx(40 - 2 * clearance));
		//Nothing left
	CHECK(ConductorTextFit::room(2 * clearance) == 0);
	CHECK(ConductorTextFit::room(clearance) == 0);
	CHECK(ConductorTextFit::room(0) == 0);
	CHECK(ConductorTextFit::room(std::numeric_limits<qreal>::quiet_NaN()) == 0);
}
