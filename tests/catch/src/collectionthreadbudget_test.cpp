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

#include "../../../sources/ElementsCollection/collectionthreadbudget.h"

/*
	How many cores the elements collection scan may take.

	The rule is one subtraction, and everything below is about the ends of
	it: a machine so small that the subtraction would leave nothing, a core
	count the operating system refused to answer, and an override typed by
	hand. The middle of the range needs no test - it is the subtraction.

	Labelled T42 and not CU-42.x: what is checked here is the arithmetic,
	not the window staying responsive while the collection loads, which is
	CU-42.4 and which only someone in front of the screen can answer.

	And what is *not* checked here, said out loud because the count of these
	tests is easily read as more than it is: none of them touches a thread
	pool. The call that carries this number to the pool lives in
	ElementsCollectionModel, out of reach of this suite - delete it and
	every assertion below still passes.
*/

TEST_CASE("T42 — the scan leaves cores to the rest of the machine", "[t42][collection]")
{
		//The machine this was measured on. Eight cores in, six workers out:
		//the two that are left are what the window and everything else the
		//user has open run on while the scan takes seconds.
	REQUIRE(CollectionThreadBudget::threadCount(8) == 6);
	REQUIRE(CollectionThreadBudget::threadCount(16) == 14);

		//The reserve is a promise about what is left, not about what is
		//taken, so it is stated that way here too.
	REQUIRE(CollectionThreadBudget::threadCount(12)
		== 12 - CollectionThreadBudget::kCoresLeftFree);
}

TEST_CASE("T42 — a small machine still scans its collection", "[t42][collection]")
{
		//This is the case that silently breaks everything: reserve two of
		//two cores and the budget is zero, a pool set to zero threads is a
		//pool that never runs the map, and the symbol panel stays empty
		//forever with no error anywhere. Never below one.
	REQUIRE(CollectionThreadBudget::threadCount(1) == 1);
	REQUIRE(CollectionThreadBudget::threadCount(2) == 1);
	REQUIRE(CollectionThreadBudget::threadCount(3) == 1);
	REQUIRE(CollectionThreadBudget::threadCount(4) == 2);
}

TEST_CASE("T42 — a machine that will not say how many cores it has", "[t42][collection]")
{
		//QThread::idealThreadCount() answers -1 when it cannot tell. The
		//point is not that -1 is handled, it is that it is not handled by
		//arithmetic: -1 - 2 = -3, and a negative maximum thread count is
		//not a smaller scan, it is undefined behaviour handed to Qt.
	REQUIRE(CollectionThreadBudget::threadCount(-1) == 1);
	REQUIRE(CollectionThreadBudget::threadCount(0) == 1);
	REQUIRE(CollectionThreadBudget::threadCount(-1000) == 1);
}

TEST_CASE("T42 — the environment can override the budget", "[t42][collection]")
{
		//The override exists so the cap can be re-measured on a machine
		//with another core count without rebuilding, including asking for
		//more threads than there are cores, which is how the behaviour
		//from before this rule existed is reproduced for comparison.
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("8")) == 8);
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("1")) == 1);
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("32")) == 32);
}

TEST_CASE("T42 — an override that says nothing usable is ignored", "[t42][collection]")
{
		//An unset variable reads as an empty string, and the whole point is
		//that this is the ordinary case: it must land on the rule, not on
		//zero threads.
	REQUIRE(CollectionThreadBudget::threadCount(8, QString()) == 6);
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("")) == 6);

		//A value that is not a number, or one that would stop the scan, is
		//refused rather than obeyed: "0" and "-4" are the two ways a typo
		//turns into a symbol panel that never fills.
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("all")) == 6);
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("0")) == 6);
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("-4")) == 6);
	REQUIRE(CollectionThreadBudget::threadCount(8, QStringLiteral("2.5")) == 6);
}
