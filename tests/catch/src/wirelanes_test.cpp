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

TEST_CASE("CU-67.16 — The lanes take the whole corridor", "[T67][wirelanes]")
{
		//The five lanes of the folio: 54.5 / 6 is 9.08, 9 apart on whole
		//units, and 9.5 and 9 left at the two ends
	const QVector<double> at = WireLanes::positions(5, 215.5, 270);
	REQUIRE(at.size() == 5);
	CHECK(at.at(0) == Approx(225));
	CHECK(at.at(4) == Approx(261));

		//On the folio itself the terminals, with their margin, start at 267:
		//51.5 / 6 is 8.58. 8 apart would leave 9.5 and 10 at the ends, 9
		//apart 7.5 and 8, nearer to the step: 9
	const QVector<double> folio = WireLanes::positions(5, 215.5, 267);
	REQUIRE(folio.size() == 5);
	CHECK(folio.at(0) == Approx(223));
	CHECK(folio.at(2) == Approx(241));
	CHECK(folio.at(4) == Approx(259));

		//Two lanes in the same corridor are not kept 10 apart in its
		//middle, with 20 empty on each side: 54.5 / 3 is 18.17, 18 apart
	const QVector<double> two = WireLanes::positions(2, 215.5, 270);
	REQUIRE(two.size() == 2);
	CHECK(two.at(0) == Approx(234));
	CHECK(two.at(1) == Approx(252));
	CHECK(two.at(0) - 215.5 == Approx(18.5));
	CHECK(270 - two.at(1) == Approx(18));
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

TEST_CASE("T67 — Few lanes: spread over the room, centered", "[T67][wirelanes]")
{
		//100 / 3 is 33.3: 33 apart, 34 and 33 left at the ends
	Request r;
	r.wires << wire(100, 130, 0, 100) << wire(120, 150, 0, 100);
	const WireLanes::Result result = WireLanes::arrange(r);
	CHECK(result.at.at(1) == Approx(34));
	CHECK(result.at.at(0) == Approx(67));
}

TEST_CASE("CU-67.6 — A narrow room: the lanes spread with the same room at each end", "[T67][wirelanes]")
{
		//Six bridges in a chain, in a room 48 wide: 48 / 7 is 6.86. 6
		//apart would leave 9 at each end, 7 apart 7 and 6, nearer to the
		//step: 7
	const QVector<double> at = WireLanes::positions(6, 0, 48);
	REQUIRE(at.size() == 6);
	for (int k = 0 ; k < 6 ; ++k) {
		CHECK(at.at(k) == Approx(7 + 7 * k));
	}

	Request r;
	for (int k = 0 ; k < 6 ; ++k) {
		r.wires << wire(100 + 20 * k, 125 + 20 * k, 0, 48);
	}
	const WireLanes::Result result = WireLanes::arrange(r);
	QVector<double> sorted = result.at;
	std::sort(sorted.begin(), sorted.end());
	CHECK(std::abs(sorted.first() - (48 - sorted.last())) <= 1);
	for (int k = 1 ; k < 6 ; ++k) {
		CHECK(sorted.at(k) - sorted.at(k - 1) == Approx(7));
	}
}

TEST_CASE("CU-67.15 — The gaps between neighbours are all the same", "[T67][wirelanes]")
{
		//The 22 bridges of the folio with its terminals moved away: 134.5
		//of room is 5.85 apart, which rounded lane by lane would give gaps
		//of 5 and 6 side by side. 5 apart would leave 15 at each end, 6
		//apart 4.5 and 4: 6
	const QVector<double> at = WireLanes::positions(22, 215.5, 350);
	REQUIRE(at.size() == 22);
	for (int k = 1 ; k < 22 ; ++k) {
		CAPTURE(k);
		CHECK(at.at(k) - at.at(k - 1) == Approx(6));
	}
	CHECK(at.first() == Approx(220));
	CHECK(at.last() == Approx(346));
		//Centered: the room left at the two ends differs by less than a step
	CHECK(std::abs((at.first() - 215.5) - (350 - at.last())) < 1);
}

TEST_CASE("T67 — An end never gets less than half the step", "[T67][wirelanes]")
{
		//20 lanes in 116: 5.52 apart. 6 apart would leave 1 at each end,
		//nearer to the step than the 11 and 10 that 5 apart leaves, and
		//would put the first lane against the frame
	const QVector<double> at = WireLanes::positions(20, 0, 116);
	REQUIRE(at.size() == 20);
	for (int k = 1 ; k < 20 ; ++k) {
		CAPTURE(k);
		CHECK(at.at(k) - at.at(k - 1) == Approx(5));
	}
	CHECK(at.first() == Approx(11));
	CHECK(at.last() == Approx(106));
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
		//Inside the frame, 180 to 221.5: 41.5 / 3 is 13.8, 14 apart, with
		//14 and 13.5 left at the ends
	CHECK(two.at.at(1) == Approx(194));
	CHECK(two.at.at(0) == Approx(208));
}

TEST_CASE("T69 — The lanes on a grid of 0.5", "[T69][wirelanes]")
{
		//Ten lanes in 60: 5.45 apart. On whole units, 5, with 8 and 7 left
		//at the ends; on a grid of 0.5, 5.5, with 5.5 and 5
	const QVector<double> whole = WireLanes::positions(10, 0, 60);
	REQUIRE(whole.size() == 10);
	CHECK(whole.first() == Approx(8));
	CHECK(whole.at(1) - whole.at(0) == Approx(5));

	const QVector<double> half = WireLanes::positions(10, 0, 60, 0.5);
	REQUIRE(half.size() == 10);
	for (int k = 0 ; k < 10 ; ++k) {
		CAPTURE(k);
		CHECK(half.at(k) == Approx(5.5 + 5.5 * k));
	}

	SECTION("and through the arrangement")
	{
		Request r;
		r.unit = 0.5;
		for (int k = 0 ; k < 10 ; ++k) {
			r.wires << wire(100 + 20 * k, 125 + 20 * k, 0, 60);
		}
		QVector<double> at = WireLanes::arrange(r).at;
		std::sort(at.begin(), at.end());
		CHECK(at.first() == Approx(5.5));
		CHECK(at.at(1) - at.at(0) == Approx(5.5));
	}
	SECTION("the run that leaves a terminal, by halves")
	{
			//A line at 609.5 and the terminal at 608: 3 from it is 612.5 on
			//halves, 613 on whole units
		WireLanes::Clearance c;
		c.parallel << WireLanes::Line{609.5, 180, 350};
		CHECK(WireLanes::runAt(608, 1, 167, 350, c, 20) == Approx(613));
		CHECK(WireLanes::runAt(608, 1, 167, 350, c, 20, 0.5) == Approx(612.5));
	}
}

TEST_CASE("T67 — Where the run that leaves a terminal goes", "[T67][wirelanes]")
{
	using WireLanes::Clearance;
	using WireLanes::Line;

		//Nothing in the way: at the terminal itself, to the side
	Clearance free;
	CHECK(WireLanes::runAt(498, 1, 166, 350, free, 20) == Approx(498));

	SECTION("on the line of a terminal of the strip, out by the least")
	{
			//The lower terminal of the middle sensor of the folio, at 608,
			//and the line of the 15, at 608: 3 down
		Clearance c;
		c.parallel << Line{608, 180, 350};
		CHECK(WireLanes::runAt(608, 1, 167, 350, c, 20) == Approx(611));
			//The upper one of the first, at 438, and the line of the 6 at
			//437: 4 up
		Clearance d;
		d.parallel << Line{437, 180, 350};
		CHECK(WireLanes::runAt(438, -1, 166, 350, d, 20) == Approx(434));
			//Exactly 3 from a line is clear
		Clearance e;
		e.parallel << Line{551, 180, 350};
		CHECK(WireLanes::runAt(548, -1, 167, 350, e, 20) == Approx(548));
	}
	SECTION("a line that ends before the run starts is not in the way")
	{
		Clearance c;
		c.parallel << Line{608, 400, 500};
		CHECK(WireLanes::runAt(608, 1, 167, 350, c, 20) == Approx(608));
	}
	SECTION("not past the terminal of another component")
	{
		Clearance c;
		c.spots << WireLanes::Spot{200, 500};
		CHECK(WireLanes::runAt(500, 1, 166, 350, c, 20) == Approx(505));
	}
	SECTION("not across a wire that stays, nor close to its end")
	{
		Clearance c;
		c.crossing << Line{200, 480, 520};
		CHECK(WireLanes::runAt(500, 1, 166, 350, c, 30) == Approx(523));
	}
	SECTION("not through another component")
	{
		Clearance c;
		c.bodies << WireLanes::Block{190, 210, 495, 515};
		CHECK(WireLanes::runAt(500, 1, 166, 350, c, 20) == Approx(515));
	}
	SECTION("nothing clear within reach")
	{
		Clearance c;
		c.bodies << WireLanes::Block{190, 210, 400, 600};
		CHECK(std::isnan(WireLanes::runAt(500, 1, 166, 350, c, 20)));
	}
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
