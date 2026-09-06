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

#include "../../../sources/ElementsCollection/collectionloadgate.h"

/*
	When the elements collection may start loading.

	The rule it holds is one sentence long - "wait for the project, but only
	when there is one" - and the second half is where it breaks. A gate that
	waits for a project the program was never given waits forever, and the
	panel stays empty: that failure is silent, it only shows on a start with
	no file, and it is the one this file exists to keep out. Hence the empty
	start being the first case here and not the last.

	Labelled T42 and not CU-42.x: what is checked below is the rule, not the
	folio appearing on the screen in less time, which is what those use cases
	promise and what only a screen can answer.
*/

TEST_CASE("T42 — a start without a project loads the collection as before", "[t42][collection]")
{
	CollectionLoadGate gate;

	REQUIRE_FALSE(gate.loadReleased());
	REQUIRE(gate.projectsLoading() == 0);

		//This is the whole no-project path: the window becomes active, and
		//the collection loads. Nothing waits for anything.
	REQUIRE(gate.windowActivated());
	REQUIRE(gate.loadReleased());
}

TEST_CASE("T42 — the collection is handed out once and only once", "[t42][collection]")
{
	CollectionLoadGate gate;

	REQUIRE(gate.windowActivated());

		//Every later event answers false: two reloads of a collection that
		//takes seconds to walk would be the cure costing more than the
		//disease.
	REQUIRE_FALSE(gate.windowActivated());
	gate.projectLoadStarted();
	REQUIRE_FALSE(gate.projectLoadFinished());
	REQUIRE(gate.loadReleased());
}

TEST_CASE("T42 — the collection waits while a project is being read", "[t42][collection]")
{
	CollectionLoadGate gate;

	gate.projectLoadStarted();
	REQUIRE(gate.projectsLoading() == 1);

		//The activation arrives in the middle of the read, because the
		//waiting dialog pumps the event loop. It must not start the load
		//there: that is the very overlap being removed.
	REQUIRE_FALSE(gate.windowActivated());
	REQUIRE_FALSE(gate.loadReleased());
	REQUIRE(gate.activationPending());

		//Postponed, never skipped: the read ends and the load starts.
	REQUIRE(gate.projectLoadFinished());
	REQUIRE(gate.loadReleased());
	REQUIRE(gate.projectsLoading() == 0);
}

TEST_CASE("T42 — the load starts even when no activation ever came", "[t42][collection]")
{
	CollectionLoadGate gate;

		//A window that never reports becoming active would leave the panel
		//empty for the whole session. The end of the read is enough.
	gate.projectLoadStarted();
	REQUIRE(gate.projectLoadFinished());
	REQUIRE(gate.loadReleased());
	REQUIRE_FALSE(gate.activationPending());
}

TEST_CASE("T42 — a project that fails to open stops holding the collection", "[t42][collection]")
{
	CollectionLoadGate gate;

	gate.projectLoadStarted();
	REQUIRE_FALSE(gate.windowActivated());

		//The file was not a project and was thrown away. It is not competing
		//for the machine any more, so there is nothing left to wait for -
		//refusing a file must not cost the user the panel.
	REQUIRE(gate.projectLoadFinished());
	REQUIRE(gate.loadReleased());
}

TEST_CASE("T42 — several projects at once release only on the last", "[t42][collection]")
{
	CollectionLoadGate gate;

		//Opening a project pumps the event loop, so a second read can begin
		//before the first is over - and two files on the command line is the
		//ordinary way to get there. Counted, not flagged: a flag would let
		//the first one to finish start the collection while the second is
		//still building folios, which is the overlap all over again.
	gate.projectLoadStarted();
	gate.projectLoadStarted();
	gate.projectLoadStarted();
	REQUIRE(gate.projectsLoading() == 3);

	REQUIRE_FALSE(gate.windowActivated());
	REQUIRE_FALSE(gate.projectLoadFinished());
	REQUIRE_FALSE(gate.projectLoadFinished());
	REQUIRE_FALSE(gate.loadReleased());

	REQUIRE(gate.projectLoadFinished());
	REQUIRE(gate.loadReleased());
}

TEST_CASE("T42 — a project opened after the collection changes nothing", "[t42][collection]")
{
	CollectionLoadGate gate;

	REQUIRE(gate.windowActivated());

		//The ordinary case, hours into the session: File > Open. The gate is
		//already spent and must stay quiet, whatever the counter does.
	gate.projectLoadStarted();
	gate.projectLoadStarted();
	REQUIRE_FALSE(gate.projectLoadFinished());
	REQUIRE_FALSE(gate.projectLoadFinished());
	REQUIRE(gate.projectsLoading() == 0);
	REQUIRE(gate.loadReleased());
}

TEST_CASE("T42 — an unpaired end of read cannot drive the counter negative", "[t42][collection]")
{
	CollectionLoadGate gate;

		//Two exits of openAndAddProject() call the end of read, and a third
		//one added later without its start would underflow the counter and
		//hold the collection back for the rest of the session. Cheap to
		//refuse, expensive to diagnose.
	REQUIRE(gate.projectLoadFinished());
	REQUIRE(gate.projectsLoading() == 0);

	CollectionLoadGate other;
	other.projectLoadStarted();
	REQUIRE(other.projectLoadFinished());
	REQUIRE_FALSE(other.projectLoadFinished());
	REQUIRE(other.projectsLoading() == 0);
}
