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

#include "../../../sources/TerminalStrip/wireendformat.h"

#include <catch2/catch.hpp>

/*
	The destination of a wire, as the plan of a terminal strip writes it,
	built from a pattern the project chooses: %tag, %point, %folio,
	%position, %wire and %description. An empty variable leaves nothing
	behind, so a point with no name does not leave "K1:" and a folio-less
	pattern does not leave "()".
*/

namespace
{
	WireEnd contactor()
	{
		WireEnd end;
		end.tag = QStringLiteral("K1");
		end.point = QStringLiteral("24");
		end.folio = QStringLiteral("3");
		end.position = QStringLiteral("B4");
		end.wire = QStringLiteral("105");
		end.description = QStringLiteral("Contator: motor 1");
		return end;
	}
}

TEST_CASE("T64 — The destination of a wire is written with the pattern of the project",
		  "[wireendformat]")
{
	const WireEnd end = contactor();

	SECTION("the default pattern: tag, point and folio")
	{
		CHECK(WireEndFormat::format(QStringLiteral("%tag:%point (Folio %folio)"), end)
			  == QStringLiteral("K1:24 (Folio 3)"));
	}

	SECTION("every variable")
	{
		CHECK(WireEndFormat::format(QStringLiteral("%tag:%point %position %wire %description"), end)
			  == QStringLiteral("K1:24 B4 105 Contator: motor 1"));
	}

	SECTION("%position is not read as %point")
	{
		CHECK(WireEndFormat::format(QStringLiteral("%position"), end) == QStringLiteral("B4"));
	}

	SECTION("a point with no name takes its separator with it")
	{
		WireEnd motor = end;
		motor.tag = QStringLiteral("M1");
		motor.point.clear();
		CHECK(WireEndFormat::format(QStringLiteral("%tag:%point (Folio %folio)"), motor)
			  == QStringLiteral("M1 (Folio 3)"));
		CHECK(WireEndFormat::format(QStringLiteral("%tag:%point %position %wire"), motor)
			  == QStringLiteral("M1 B4 105"));
	}

	SECTION("brackets left empty go too")
	{
		WireEnd nowhere = end;
		nowhere.wire.clear();
		CHECK(WireEndFormat::format(QStringLiteral("%tag:%point (%wire)"), nowhere)
			  == QStringLiteral("K1:24"));
	}

	SECTION("the colon inside a description stays")
	{
		CHECK(WireEndFormat::format(QStringLiteral("%description"), end)
			  == QStringLiteral("Contator: motor 1"));
	}

	SECTION("text that is no variable stays as written")
	{
		CHECK(WireEndFormat::format(QStringLiteral("%tag -> %point %unknown"), end)
			  == QStringLiteral("K1 -> 24 %unknown"));
	}
}
