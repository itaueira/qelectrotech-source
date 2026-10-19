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

#include "../../../sources/wirelanes.h"

#include <catch2/catch.hpp>

#include <algorithm>
#include <cmath>

/*
	Where the bridge of each conductor goes (T67). The numbers of the main
	case are those of a folio of a real project: six conductors from two
	sensors, inside a frame drawn up to x = 215 with a pen 1 wide, to six
	terminals whose ends are at x = 270, the terminal strip being in a
	frame from 282. The automatic path puts the six bridges at 225, half
	way between the terminal ends, three of them on top of each other.
*/
namespace
{
	using WireLanes::Frame;
	using WireLanes::Request;
	using WireLanes::Wire;

	const Frame sensors_frame{74.5, 215.5, 119.5, 615.5};
	const Frame strip_frame{281.5, 292.5, 121.5, 706.5};

	Wire wire(double from, double to, double low = 180, double high = 270)
	{
		Wire w;
		w.low = low;
		w.high = high;
		w.from = from;
		w.to = to;
		return w;
	}

	Request folio()
	{
		Request r;
		r.frames << sensors_frame << strip_frame;
		r.wires << wire(154.99, 155)	// SP1 +, straight
			<< wire(167.99, 178)	// SP1 -
			<< wire(180.98, 201)	// SP1 signal
			<< wire(204.99, 224)	// SF1 +
			<< wire(217.99, 247)	// SF1 -
			<< wire(230.98, 270);	// SF1 signal
		return r;
	}
}

TEST_CASE("T67 — The bridge goes between the two frames", "[T67][wirelanes]")
{
	Request r;
	r.frames << sensors_frame << strip_frame;
	r.wires << wire(167.99, 178);

		//Two pieces: 180 to 215.5, inside the sensors' frame, and 215.5 to
		//270, between the frames
	const auto pieces = WireLanes::pieces(180, 270, 167.99, 178, {}, r.frames);
	REQUIRE(pieces.size() == 2);
	CHECK(pieces.at(0).frames == 1);
	CHECK(pieces.at(1).frames == 0);
	CHECK(pieces.at(1).low == Approx(215.5));
	CHECK(pieces.at(1).high == Approx(270));

	const WireLanes::Result result = WireLanes::arrange(r);
	CHECK(result.at.at(0) == Approx(243));	// 242.75 on the unit

	SECTION("on the grid of 10, when it fits")
	{
		r.grid = 10;
		CHECK(WireLanes::arrange(r).at.at(0) == Approx(240));
	}

	SECTION("the wider piece loses when it is inside a frame")
	{
			//The frame ends at 240 now: 180 to 240 inside it is wider than
			//240 to 270 outside, and outside wins
		Request wide;
		wide.frames << Frame{74.5, 240, 119.5, 615.5};
		wide.wires << wire(167.99, 178);
		CHECK(WireLanes::arrange(wide).at.at(0) == Approx(255));
	}
}

TEST_CASE("CU-67.2 — Five bridges that touch, in a staircase", "[T67][wirelanes]")
{
	const WireLanes::Result result = WireLanes::arrange(folio());

		//SP1 + has no bridge to move
	CHECK(std::isnan(result.at.at(0)));
	CHECK(result.group.at(0) == -1);

		//The five others, one group, the upper one on the right: 54.5 of
		//room for five lanes is 54.5 / 6 between two of them, and as much
		//to the frame and to the terminal ends
	for (int i = 2 ; i <= 5 ; ++i) {
		CHECK(result.group.at(i) == result.group.at(1));
	}
	CHECK(result.at.at(1) == Approx(261));
	CHECK(result.at.at(2) == Approx(252));
	CHECK(result.at.at(3) == Approx(243));
	CHECK(result.at.at(4) == Approx(234));
	CHECK(result.at.at(5) == Approx(225));
}

TEST_CASE("T67 — The grid gives way when it would push a lane against the frame", "[T67][wirelanes]")
{
		//The five lanes of the folio on a grid of 10 would be 220 to 260,
		//the first one 4.5 from the frame where it should have 9: on whole
		//units instead
	const QVector<double> at = WireLanes::positions(5, 215.5, 270, 10, 10);
	REQUIRE(at.size() == 5);
	CHECK(at.at(0) == Approx(225));
	CHECK(at.at(4) == Approx(261));

		//On the folio itself the terminals, with their margin, start at 267:
		//on the grid the first lane would be 4.5 from the frame where it
		//should have 8.6, which half of it would still have let through
	const QVector<double> folio = WireLanes::positions(5, 215.5, 267, 10, 10);
	REQUIRE(folio.size() == 5);
	CHECK(folio.at(0) == Approx(224));
	CHECK(folio.at(2) == Approx(241));
	CHECK(folio.at(4) == Approx(258));

		//Two lanes have room enough: on the grid
	const QVector<double> two = WireLanes::positions(2, 215.5, 270, 10, 10);
	REQUIRE(two.size() == 2);
	CHECK(two.at(0) == Approx(240));
	CHECK(two.at(1) == Approx(250));
}

TEST_CASE("T67 — Two that go down: the lower one on the left", "[T67][wirelanes]")
{
		//SF1 + and SF1 -: overlapping from 218 to 224. The other way round,
		//each would cross the other, and the run of SF1 + to its terminal
		//would go along the one of SF1 - from its sensor, six apart.
	const Wire upper = wire(204.99, 224);
	const Wire lower = wire(217.99, 247);
	REQUIRE(WireLanes::inConflict(upper, lower, 10));
	CHECK(WireLanes::orderCost(lower, upper, 10) == 0);
	CHECK(WireLanes::orderCost(upper, lower, 10) == 2 + 2 + 1);

	const QVector<int> lanes = WireLanes::lanes({upper, lower}, 10);
	CHECK(lanes.at(1) < lanes.at(0));

	SECTION("and two that go up, the upper one")
	{
		const Wire up_high = wire(224, 204.99);
		const Wire up_low = wire(247, 217.99);
		const QVector<int> up = WireLanes::lanes({up_high, up_low}, 10);
		CHECK(up.at(0) < up.at(1));
	}
	SECTION("when only the ends are close, side by side decides")
	{
			//SP1 - ends at 178 and SP1 signal starts at 181: no crossing
			//either way, but the other way round their runs would go side
			//by side, three apart
		const Wire first = wire(167.99, 178);
		const Wire second = wire(180.98, 201);
		REQUIRE(WireLanes::inConflict(first, second, 10));
		CHECK(WireLanes::orderCost(second, first, 10) == 0);
		CHECK(WireLanes::orderCost(first, second, 10) == 1);
	}
}

TEST_CASE("T67 — Bridges far apart share a lane", "[T67][wirelanes]")
{
	Request r;
	r.wires << wire(100, 120, 0, 100) << wire(200, 220, 0, 100);
	REQUIRE_FALSE(WireLanes::inConflict(r.wires.at(0), r.wires.at(1), 10));
	const WireLanes::Result result = WireLanes::arrange(r);
	CHECK(result.at.at(0) == Approx(50));
	CHECK(result.at.at(1) == Approx(50));
	CHECK(result.group.at(0) != result.group.at(1));
}

TEST_CASE("T67 — Few lanes: 10 apart, centered", "[T67][wirelanes]")
{
	Request r;
	r.wires << wire(100, 130, 0, 100) << wire(120, 150, 0, 100);
	const WireLanes::Result result = WireLanes::arrange(r);
	CHECK(result.at.at(1) == Approx(45));
	CHECK(result.at.at(0) == Approx(55));
}

TEST_CASE("CU-67.6 — A narrow room: the lanes spread with the same room at each end", "[T67][wirelanes]")
{
		//Six bridges in a chain, in a room 48 wide: 48 / 7 apart, less
		//than 10
	const QVector<double> at = WireLanes::positions(6, 0, 48, 10, 0.5);
	REQUIRE(at.size() == 6);
	const double pitch = 48.0 / 7;
	for (int k = 0 ; k < 6 ; ++k) {
		CHECK(at.at(k) == Approx(pitch * (k + 1)).margin(0.5));
	}

	Request r;
	for (int k = 0 ; k < 6 ; ++k) {
		r.wires << wire(100 + 20 * k, 125 + 20 * k, 0, 48);
	}
	const WireLanes::Result result = WireLanes::arrange(r);
	QVector<double> sorted = result.at;
	std::sort(sorted.begin(), sorted.end());
	CHECK(sorted.first() == Approx(pitch).margin(0.5));
	CHECK(48 - sorted.last() == Approx(pitch).margin(0.5));
	for (int k = 1 ; k < 6 ; ++k) {
		CHECK(sorted.at(k) - sorted.at(k - 1) == Approx(pitch).margin(1));
	}
}

TEST_CASE("T67 — A symbol in the room is not crossed", "[T67][wirelanes]")
{
	Request r;
	r.blocks << WireLanes::Block{55, 85, 90, 140};
	r.wires << wire(100, 130, 0, 200);
	const WireLanes::Result result = WireLanes::arrange(r);
		//The pieces are 0 to 55 and 85 to 200: the wider one
	CHECK(result.at.at(0) == Approx(142.5).margin(0.5));

	SECTION("a symbol that covers the whole room is a frame")
	{
		Request around;
		around.blocks << WireLanes::Block{-50, 250, 0, 300};
		around.wires << wire(100, 130, 0, 200);
		CHECK(WireLanes::arrange(around).at.at(0) == Approx(100));
	}
	SECTION("a symbol beside the bridge, not across it, is not in the way")
	{
		Request beside;
		beside.blocks << WireLanes::Block{80, 120, 300, 340};
		beside.wires << wire(100, 130, 0, 200);
		CHECK(WireLanes::arrange(beside).at.at(0) == Approx(100));
	}
}

TEST_CASE("T67 — Too narrow outside the frames: inside, where there is room", "[T67][wirelanes]")
{
		//Between the frame and the terminal ends, 8.5: one lane fits (4.25
		//from each side), two do not (2.8)
	Request r;
	r.frames << Frame{68.5, 221.5, 113.5, 621.5};
	r.wires << wire(167.99, 178, 180, 230);
	CHECK(WireLanes::arrange(r).at.at(0) == Approx(226));

	r.wires << wire(180.98, 201, 180, 230);
	const WireLanes::Result two = WireLanes::arrange(r);
		//Inside the frame, 180 to 221.5: 10 apart, centered on 200.75
	CHECK(two.at.at(1) == Approx(196));
	CHECK(two.at.at(0) == Approx(206));
}

TEST_CASE("T67 — A chain with no room in common: each wire on its own", "[T67][wirelanes]")
{
	Request r;
	r.wires << wire(100, 130, 0, 50) << wire(120, 150, 40, 100) << wire(140, 170, 90, 140);
	const WireLanes::Result result = WireLanes::arrange(r);
	CHECK(result.at.at(0) == Approx(25));
	CHECK(result.at.at(1) == Approx(70));
	CHECK(result.at.at(2) == Approx(115));
}
