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

#include "../../../sources/navigationhistory.h"

#include <QList>

/*
	Back and Forward retrace the way the reader went, and not the order of
	the folios in the project: that is the whole difference from PageUp and
	PageDown, and the reason this rule exists. The folios are numbers here,
	since the rule compares places and nothing else.

	Labelled T49 and not CU-49.x: the use cases are about the mouse buttons
	and the keys on a real project. What is checked here is the walk they
	move along - the project view that keeps one is in folionavigation_test
	of C_uitests.
*/

namespace
{
	QList<int> walk(std::initializer_list<int> folios)
	{
		return QList<int>(folios);
	}
}

TEST_CASE("T49 — Back retraces the way the reader went, not the order of the folios", "[t49][navigation]")
{
	NavigationHistory<int> history;

	REQUIRE_FALSE(history.canGoBack());
	REQUIRE_FALSE(history.canGoForward());
	REQUIRE(history.currentIndex() == -1);

	for (int folio : {1, 5, 3, 9}) {
		history.visit(folio);
	}
	REQUIRE(history.places() == walk({1, 5, 3, 9}));
	REQUIRE(history.current() == 9);
	REQUIRE(history.canGoBack());
	REQUIRE_FALSE(history.canGoForward());

	CHECK(history.back() == 3);
	CHECK(history.back() == 5);
	CHECK(history.back() == 1);
	CHECK_FALSE(history.canGoBack());
		//Nothing before the first folio: Back stays where it is.
	CHECK(history.back() == 1);

	CHECK(history.forward() == 5);
	CHECK(history.forward() == 3);
	CHECK(history.forward() == 9);
	CHECK_FALSE(history.canGoForward());
	CHECK(history.forward() == 9);

		//Moving inside the walk records nothing.
	CHECK(history.places() == walk({1, 5, 3, 9}));
}

TEST_CASE("T49 — Going somewhere new after going back drops what was ahead", "[t49][navigation]")
{
	NavigationHistory<int> history;
	for (int folio : {1, 5, 3, 9}) {
		history.visit(folio);
	}

	history.back();
	history.back();
	REQUIRE(history.current() == 5);
	REQUIRE(history.canGoForward());

	history.visit(2);
	CHECK(history.places() == walk({1, 5, 2}));
	CHECK_FALSE(history.canGoForward());
	CHECK(history.back() == 5);
}

TEST_CASE("T49 — Showing the folio one is already on is not a step", "[t49][navigation]")
{
	NavigationHistory<int> history;
	history.visit(4);
	history.visit(4);
	history.visit(7);
	history.visit(7);

	CHECK(history.places() == walk({4, 7}));

		//The same folio again later is a step: the reader left it between.
	history.visit(4);
	CHECK(history.places() == walk({4, 7, 4}));
}

TEST_CASE("T49 — The walk keeps the last folios seen, and drops the oldest", "[t49][navigation]")
{
	NavigationHistory<int> history(3);
	for (int folio : {1, 2, 3, 4, 5}) {
		history.visit(folio);
	}

	CHECK(history.places() == walk({3, 4, 5}));
	CHECK(history.current() == 5);
	CHECK(history.back() == 4);
	CHECK(history.back() == 3);
	CHECK_FALSE(history.canGoBack());

		//The default is the hundred the specification promises.
	NavigationHistory<int> long_walk;
	for (int folio = 0 ; folio < 150 ; ++folio) {
		long_walk.visit(folio);
	}
	CHECK(long_walk.places().size() == 100);
	CHECK(long_walk.places().first() == 50);
}

TEST_CASE("T49 — A removed folio leaves the walk", "[t49][navigation]")
{
	NavigationHistory<int> history;
	for (int folio : {1, 4, 6, 4, 8}) {
		history.visit(folio);
	}

	SECTION("behind the reader")
	{
		history.remove(4);
			//6 stood between two visits to 4, which are gone: 1, 6, 8.
		CHECK(history.places() == walk({1, 6, 8}));
		CHECK(history.current() == 8);
		CHECK(history.back() == 6);
		CHECK(history.back() == 1);
	}

	SECTION("where the reader stands")
	{
		history.back();
		history.back();
		REQUIRE(history.current() == 6);

		history.remove(6);
			//1, 4, 4, 8 - the two visits to 4 met and became one, and
			//the reader stands on the folio seen before the one removed.
		CHECK(history.places() == walk({1, 4, 8}));
		CHECK(history.current() == 4);
		CHECK(history.forward() == 8);
	}

	SECTION("the first folio, while standing on it")
	{
		while (history.canGoBack()) {
			history.back();
		}
		history.remove(1);
			//Nothing was before it: the next one becomes current.
		CHECK(history.places() == walk({4, 6, 4, 8}));
		CHECK(history.current() == 4);
		CHECK_FALSE(history.canGoBack());
	}

	SECTION("every folio")
	{
		for (int folio : {1, 4, 6, 8}) {
			history.remove(folio);
		}
		CHECK(history.places().isEmpty());
		CHECK(history.currentIndex() == -1);
		CHECK_FALSE(history.canGoBack());
		CHECK_FALSE(history.canGoForward());

			//And the walk starts again from nothing.
		history.visit(3);
		CHECK(history.places() == walk({3}));
	}

	SECTION("a folio that was never visited")
	{
		history.remove(99);
		CHECK(history.places() == walk({1, 4, 6, 4, 8}));
		CHECK(history.current() == 8);
	}
}
