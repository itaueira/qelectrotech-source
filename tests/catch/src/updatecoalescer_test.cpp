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

#include "../../../sources/dataBase/updatecoalescer.h"

/*
	One notice per gesture, not one per row.

	The rule is short enough to read in one sitting and has four ways of
	being wrong, all of them silent: announcing per row (the storm), never
	announcing (the folio that stops following the project), announcing an
	operation in which nothing happened (the folio redrawn for nothing), and
	announcing twice when an operation calls another one.

	Labelled T17 and not CU-17.19: that use case asks for a renumbering of a
	six-folio project to refresh the list on the folio once, with a
	stopwatch on it. What is checked here is the rule the count rests on -
	the counting of real announcements over a real data base is in
	dbnotify_test.cpp of C_uitests, and the folio redrawing is a screen.
*/

TEST_CASE("T17 — a change outside an operation is announced on the spot", "[t17][database]")
{
	UpdateCoalescer coalescer;

	REQUIRE_FALSE(coalescer.isOperationOpen());
	REQUIRE(coalescer.depth() == 0);

		//A single edit is the common case, and it must not wait for anybody
		//to close a bracket that was never opened.
	REQUIRE(coalescer.notify());
	REQUIRE(coalescer.notify());
	REQUIRE_FALSE(coalescer.isPending());
}

TEST_CASE("T17 — an operation announces once, whatever it touched", "[t17][database]")
{
	UpdateCoalescer coalescer;

	coalescer.beginOperation();
	REQUIRE(coalescer.isOperationOpen());

		//Twelve conductors of the same potential renamed by one gesture.
	for (int row = 0 ; row < 12 ; ++row) {
		REQUIRE_FALSE(coalescer.notify());
	}
	REQUIRE(coalescer.isPending());

	REQUIRE(coalescer.endOperation());
	REQUIRE_FALSE(coalescer.isOperationOpen());
		//And the notice is consumed: closing again announces nothing.
	REQUIRE_FALSE(coalescer.isPending());
}

TEST_CASE("T17 — an operation that changed nothing announces nothing", "[t17][database]")
{
	UpdateCoalescer coalescer;

	coalescer.beginOperation();
	REQUIRE_FALSE(coalescer.endOperation());

		//A renumbering where every wire already carried the number it was
		//being given changes no row. Redrawing every list of the project for
		//that would be the cure costing more than the disease.
	REQUIRE_FALSE(coalescer.isPending());
}

TEST_CASE("T17 — nested operations announce once, at the outermost close", "[t17][database]")
{
	UpdateCoalescer coalescer;

	coalescer.beginOperation();
	coalescer.beginOperation();
	REQUIRE(coalescer.depth() == 2);

	REQUIRE_FALSE(coalescer.notify());

		//The inner close is not the end of the gesture: announcing here
		//would be the storm again, one level deeper.
	REQUIRE_FALSE(coalescer.endOperation());
	REQUIRE(coalescer.depth() == 1);
	REQUIRE(coalescer.isPending());

	REQUIRE(coalescer.endOperation());
	REQUIRE(coalescer.depth() == 0);
}

TEST_CASE("T17 — an operation left open holds the notice back", "[t17][database]")
{
	UpdateCoalescer coalescer;

	coalescer.beginOperation();
	REQUIRE_FALSE(coalescer.notify());

		//Said out loud because it is the price of the mechanism: whoever
		//opens an operation and does not close it stops the folio from ever
		//following the project again. That is what the guard object in
		//projectDataBase is for.
	REQUIRE(coalescer.isPending());
	REQUIRE(coalescer.isOperationOpen());
}

TEST_CASE("T17 — a close with no operation open is not an announcement", "[t17][database]")
{
	UpdateCoalescer coalescer;

	REQUIRE_FALSE(coalescer.endOperation());
	REQUIRE(coalescer.depth() == 0);

		//And the depth did not go below zero: were it -1, the next real
		//operation would close one level short and never announce - a wrong
		//close in one place silencing a right one somewhere else.
	coalescer.beginOperation();
	REQUIRE_FALSE(coalescer.notify());
	REQUIRE(coalescer.endOperation());
}
