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

#include "../../../sources/ElementsCollection/terminalmatch.h"

/*
	Which terminal of a replacing element each terminal of the replaced one
	becomes. The wires follow the answer, so a wrong answer is a wire on the
	wrong terminal that nobody sees: each pass is checked against the pass
	after it, where the two could disagree.
*/

namespace
{
	using TerminalMatch::Spot;

	Spot spot(const QString &uuid, const QString &name,
		  qreal x, qreal y, int orientation = 0)
	{
		Spot s;
		s.uuid = uuid.isEmpty() ? QUuid() : QUuid(uuid);
		s.name = name;
		s.point = QPointF(x, y);
		s.orientation = orientation;
		return s;
	}

	const QString uuid_a = QStringLiteral("{11111111-1111-4111-8111-111111111111}");
	const QString uuid_b = QStringLiteral("{22222222-2222-4222-8222-222222222222}");
}

TEST_CASE("CU-54.5 — The same uuid wins over the same point",
	  "[t54][terminalmatch]")
{
		//The new symbol was made from the old one: terminal A moved, and
		//terminal B now sits where A was
	const QVector<Spot> old_spots {spot(uuid_a, {}, 0, 0),
				       spot(uuid_b, {}, 0, 20)};
	const QVector<Spot> new_spots {spot(uuid_b, {}, 0, 0),
				       spot(uuid_a, {}, 15, 0)};

	const QVector<int> result = TerminalMatch::match(old_spots, new_spots);
	CHECK(result == QVector<int>{1, 0});
}

TEST_CASE("CU-54.5 — The same name wins over the same point, when it is told once on each side",
	  "[t54][terminalmatch]")
{
	SECTION("a name carried once on each side")
	{
		const QVector<Spot> old_spots {spot({}, QStringLiteral("A1"), 0, 0),
					       spot({}, QStringLiteral("A2"), 0, 40)};
		const QVector<Spot> new_spots {spot({}, QStringLiteral("A2"), 0, 0),
					       spot({}, QStringLiteral("A1"), 0, 40)};
		CHECK(TerminalMatch::match(old_spots, new_spots) == QVector<int>{1, 0});
	}

	SECTION("a name repeated on one side tells nothing, and the point decides")
	{
		const QVector<Spot> old_spots {spot({}, QStringLiteral("1"), 0, 0),
					       spot({}, QStringLiteral("2"), 0, 40)};
		const QVector<Spot> new_spots {spot({}, QStringLiteral("2"), 0, 0),
					       spot({}, QStringLiteral("1"), 0, 40),
					       spot({}, QStringLiteral("1"), 20, 40)};
			//"2" is told once on each side; "1" twice on the new one
		const QVector<int> result = TerminalMatch::match(old_spots, new_spots);
		CHECK(result.at(1) == 0);
		CHECK(result.at(0) == -1);
	}

	SECTION("a terminal without a name is matched by its point")
	{
		const QVector<Spot> old_spots {spot({}, {}, 0, 0)};
		const QVector<Spot> new_spots {spot({}, {}, 10, 0),
					       spot({}, {}, 0, 0)};
		CHECK(TerminalMatch::match(old_spots, new_spots) == QVector<int>{1});
	}
}

TEST_CASE("CU-54.5 — At the same point, the same orientation wins",
	  "[t54][terminalmatch]")
{
		//Two terminals at one point, facing two ways: each old one finds
		//the one that faces its own way, whichever comes first in the list
	const QVector<Spot> old_spots {spot({}, {}, 0, 0, 2),
				       spot({}, {}, 0, 0, 0)};
	const QVector<Spot> new_spots {spot({}, {}, 0, 0, 0),
				       spot({}, {}, 0, 0, 2)};
	CHECK(TerminalMatch::match(old_spots, new_spots) == QVector<int>{1, 0});

		//And a terminal facing another way at the same point is still the
		//same point: the wire stays docked there
	const QVector<Spot> turned {spot({}, {}, 0, 0, 1)};
	CHECK(TerminalMatch::match({spot({}, {}, 0, 0, 3)}, turned) == QVector<int>{0});
}

TEST_CASE("CU-54.5 — A new terminal is given to one old terminal at most",
	  "[t54][terminalmatch]")
{
	SECTION("two old terminals at the point of a single new one")
	{
		const QVector<Spot> old_spots {spot({}, {}, 0, 0, 0),
					       spot({}, {}, 0, 0, 0)};
		const QVector<Spot> new_spots {spot({}, {}, 0, 0, 0)};
		CHECK(TerminalMatch::match(old_spots, new_spots) == QVector<int>{0, -1});
	}

	SECTION("a new terminal taken by its uuid is not given away by its point")
	{
		const QVector<Spot> old_spots {spot(uuid_a, {}, 50, 50),
					       spot({}, {}, 0, 0)};
		const QVector<Spot> new_spots {spot(uuid_a, {}, 0, 0)};
		CHECK(TerminalMatch::match(old_spots, new_spots) == QVector<int>{0, -1});
	}
}

TEST_CASE("CU-54.5 — A terminal with nowhere to go has no match",
	  "[t54][terminalmatch]")
{
	const QVector<Spot> old_spots {spot({}, {}, 0, 0),
				       spot({}, {}, 0, 20)};
	const QVector<Spot> new_spots {spot({}, {}, 0, 0)};
	CHECK(TerminalMatch::match(old_spots, new_spots) == QVector<int>{0, -1});

		//A point a rounding error away is the same point; a point one unit
		//away is not
	CHECK(TerminalMatch::match({spot({}, {}, 10.2, 0)}, {spot({}, {}, 10, 0)})
	      == QVector<int>{0});
	CHECK(TerminalMatch::match({spot({}, {}, 11, 0)}, {spot({}, {}, 10, 0)})
	      == QVector<int>{-1});
	CHECK(TerminalMatch::match({}, new_spots).isEmpty());
}

/*
	Where the wires of a place go when another element comes to it, the
	wires staying where they are (T66): the terminal that lies where a wire
	ends, then the one of the same name. The uuid says nothing: the two are
	different components.
*/
TEST_CASE("T66 — By place: the same point first, the uuid aside",
	  "[t66][terminalmatch]")
{
		//The terminal with the same uuid is elsewhere; the one at the point
		//of the wire takes it
	const QVector<Spot> old_spots {spot(uuid_a, {}, 0, 0),
				       spot(uuid_b, {}, 0, 20)};
	const QVector<Spot> new_spots {spot(uuid_a, {}, 40, 0),
				       spot({}, {}, 0, 0),
				       spot({}, {}, 0, 20)};
	CHECK(TerminalMatch::matchByPlace(old_spots, new_spots) == QVector<int>{1, 2});
}

TEST_CASE("T66 — By place: the same name for what the point left",
	  "[t66][terminalmatch]")
{
		//A1 is not where it was on the other symbol, and nothing lies there:
		//the name takes it; 14 has no match at all
	const QVector<Spot> old_spots {spot({}, QStringLiteral("A1"), 0, 0),
				       spot({}, QStringLiteral("14"), 0, 40)};
	const QVector<Spot> new_spots {spot({}, QStringLiteral("A1"), 0, 10),
				       spot({}, QStringLiteral("A2"), 0, 30)};
	CHECK(TerminalMatch::matchByPlace(old_spots, new_spots) == QVector<int>{0, -1});

		//And a terminal is taken once: two old ones on the same point
	const QVector<Spot> twice {spot({}, {}, 0, 0), spot({}, {}, 0, 0)};
	const QVector<Spot> one {spot({}, {}, 0, 0)};
	CHECK(TerminalMatch::matchByPlace(twice, one) == QVector<int>{0, -1});
}
