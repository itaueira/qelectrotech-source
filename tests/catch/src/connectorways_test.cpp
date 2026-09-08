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

#include "../../../sources/autoNum/renumberplan.h"
#include "../../../sources/catalog/catalogpart.h"
#include "../../../sources/connector/connectorways.h"

/*
	The reserve of a connector: what the catalogue part has, what the
	folios draw of it, and what is left over.

	The rule is two lists of strings, so everything that can be got wrong
	about it can be got wrong here, before a table on a folio shows the
	number to somebody who is about to crimp:

	- the reserve counting a way that is in fact wired, because a label
	  the part has no way for was quietly taken as a way of it;
	- "nobody wrote down how many ways this connector has" printed as
	  "no ways left", which is the state this project has already
	  collapsed twice;
	- the same way claimed twice counted as two ways used, which takes
	  one off the reserve and hides the double claim at the same time;
	- a repeat in the pinout of the part putting a way in the reserve
	  that the product does not have;
	- a reserve of none reading as a reserve nobody measured, or the
	  other way round.

	Labelled T34 and not CU-34.1: that case is the table on the folio
	showing nine in use and seven reserve, and the table is step 6. What
	is proved here is the arithmetic behind the number, which is all
	step 2 promised.
*/

namespace
{
	/// A sixteen way connector, catalogued way by way.
	QStringList sixteenWays()
	{
		QStringList ways;
		for (int way = 1 ; way <= 16 ; ++way) {
			ways.append(QString::number(way));
		}
		return ways;
	}
}

TEST_CASE("T34 — nine ways drawn of sixteen leaves seven in reserve", "[t34][connector]")
{
	const QStringList drawn{QStringLiteral("1"), QStringLiteral("2"),
				QStringLiteral("3"), QStringLiteral("4"),
				QStringLiteral("5"), QStringLiteral("6"),
				QStringLiteral("7"), QStringLiteral("8"),
				QStringLiteral("9")};

	const ConnectorWays answer = ConnectorWays::fromPinout(sixteenWays(), drawn);

	REQUIRE(answer.isKnown());
	CHECK(answer.wayCount() == 16);
	CHECK(answer.usedCount() == 9);
	CHECK(answer.reserveCount() == 7);
	CHECK(answer.notOnPart().isEmpty());

		// The three lists do not overlap, and two of them are the whole part
	CHECK(answer.usedCount() + answer.reserveCount() == answer.wayCount());
	CHECK(answer.used() == drawn);
	CHECK(answer.reserve() == QStringList{QStringLiteral("10"), QStringLiteral("11"),
					      QStringLiteral("12"), QStringLiteral("13"),
					      QStringLiteral("14"), QStringLiteral("15"),
					      QStringLiteral("16")});

	/*
		Part order and not drawing order: the crimping guide is read
		against the product, so way 10 comes before way 11 whatever
		order the folios were drawn in.
	*/
	const QStringList backwards{QStringLiteral("9"), QStringLiteral("1")};
	const ConnectorWays shuffled = ConnectorWays::fromPinout(sixteenWays(), backwards);
	CHECK(shuffled.used() == QStringList{QStringLiteral("1"), QStringLiteral("9")});
	CHECK(shuffled.reserveCount() == 14);
}

TEST_CASE("T34 — a label the part has no way for is not a way in reserve", "[t34][connector]")
{
	/*
		Nine pins drawn, and one of them carries a label the part does
		not declare. Eight ways of the part are wired; eight are spare.
		Taking the ninth label as a way of the part - the collapse this
		case exists against - would answer seven spare, and the way it
		took would be one that is in fact crimped.
	*/
	const QStringList drawn{QStringLiteral("1"), QStringLiteral("2"),
				QStringLiteral("3"), QStringLiteral("4"),
				QStringLiteral("5"), QStringLiteral("6"),
				QStringLiteral("7"), QStringLiteral("8"),
				QStringLiteral("A1")};

	const ConnectorWays answer = ConnectorWays::fromPinout(sixteenWays(), drawn);

	REQUIRE(answer.isKnown());
	CHECK(answer.usedCount() == 8);
	CHECK(answer.reserveCount() == 8);
	CHECK(answer.notOnPart() == QStringList{QStringLiteral("A1")});
	CHECK(answer.drawnCount() == 9);

		// The mistake is outside the sum, and the sum still closes
	CHECK(answer.usedCount() + answer.reserveCount() == answer.wayCount());
	CHECK_FALSE(answer.reserve().contains(QStringLiteral("A1")));
	CHECK_FALSE(answer.used().contains(QStringLiteral("A1")));
}

TEST_CASE("T34 — a part with no pinout has no reserve, which is not a reserve of none",
	  "[t34][connector]")
{
	const QStringList drawn{QStringLiteral("1"), QStringLiteral("2"),
				QStringLiteral("3")};

	const ConnectorWays nothing = ConnectorWays::fromPinout(QStringList(), drawn);

	CHECK_FALSE(nothing.isKnown());
	CHECK(nothing.wayCount() == ConnectorWays::unknownCount());
	CHECK(nothing.usedCount() == ConnectorWays::unknownCount());
	CHECK(nothing.reserveCount() == ConnectorWays::unknownCount());
	CHECK(nothing.reserveCount() != 0);

	/*
		And no finding is invented against a pinout nobody wrote: the
		three labels are not "on no way of this part", they are three
		labels waiting for a part.
	*/
	CHECK(nothing.notOnPart().isEmpty());
	CHECK(nothing.ways().isEmpty());
	CHECK(nothing.reserve().isEmpty());

		// What the folios carry is still read, and still answered for
	CHECK(nothing.drawnCount() == 3);
	CHECK(nothing.drawn() == drawn);

	/*
		The other half of the same distinction: a connector with every
		way wired has a reserve, and it is empty. Reading the two the
		same way is what makes "no ways left" and "nobody measured
		this" one sentence on a folio.
	*/
	const ConnectorWays full = ConnectorWays::fromPinout(sixteenWays(), sixteenWays());
	CHECK(full.isKnown());
	CHECK(full.reserveCount() == 0);
	CHECK(full.reserveCount() != ConnectorWays::unknownCount());
	CHECK(full.reserve().isEmpty());
	CHECK(full.usedCount() == 16);
	CHECK(full.notOnPart().isEmpty());
}

TEST_CASE("T34 — the same way claimed twice is one way used, and the claim is still reported",
	  "[t34][connector]")
{
	/*
		Ten labels drawn, one of them twice. A way is used or it is
		not, so nine ways are used and seven spare - counting the
		repeat would take one off the reserve. The repeat is a fact of
		its own all the same: two components reaching for way 3, or one
		pin drawn twice, and it is reported rather than swallowed.
	*/
	const QStringList drawn{QStringLiteral("1"), QStringLiteral("2"),
				QStringLiteral("3"), QStringLiteral("4"),
				QStringLiteral("5"), QStringLiteral("6"),
				QStringLiteral("7"), QStringLiteral("8"),
				QStringLiteral("9"), QStringLiteral("3")};

	const ConnectorWays answer = ConnectorWays::fromPinout(sixteenWays(), drawn);

	CHECK(answer.usedCount() == 9);
	CHECK(answer.reserveCount() == 7);
	CHECK(answer.drawnCount() == 9);
	CHECK(answer.drawnTwice() == QStringList{QStringLiteral("3")});
	CHECK(answer.used().count(QStringLiteral("3")) == 1);

		// Nothing repeated, nothing reported
	const ConnectorWays once = ConnectorWays::fromPinout(sixteenWays(),
							     QStringList{QStringLiteral("3")});
	CHECK(once.drawnTwice().isEmpty());
}

TEST_CASE("T34 — a label the part declares twice is one way, not a phantom in the reserve",
	  "[t34][connector]")
{
	/*
		Sixteen pins catalogued and one label typed twice: the product
		has fifteen ways that can be told apart, and a sixteenth
		reserve way would be a hole nobody can crimp into.
	*/
	QStringList pinout = sixteenWays();
	pinout[15] = QStringLiteral("3");

	const ConnectorWays answer = ConnectorWays::fromPinout(pinout,
							       QStringList{QStringLiteral("3")});

	CHECK(answer.wayCount() == 15);
	CHECK(answer.usedCount() == 1);
	CHECK(answer.reserveCount() == 14);
	CHECK(answer.declaredTwice() == QStringList{QStringLiteral("3")});
	CHECK(answer.ways().count(QStringLiteral("3")) == 1);
}

TEST_CASE("T34 — a pin carrying no label at all is neither used, nor reserve, nor missing",
	  "[t34][connector]")
{
	QStringList pinout = sixteenWays();
	pinout.append(QString());
	pinout.append(QStringLiteral("  "));

	const QStringList drawn{QStringLiteral("1"), QString(), QStringLiteral("\t")};

	const ConnectorWays answer = ConnectorWays::fromPinout(pinout, drawn);

	CHECK(answer.wayCount() == 16);
	CHECK(answer.blankWayCount() == 2);
	CHECK(answer.drawnCount() == 1);
	CHECK(answer.blankDrawnCount() == 2);

	/*
		A pin nobody numbered is not a way absent from the part - that
		would report the cataloguing of the part as wrong when what is
		unfinished is the drawing - and it is not a way in reserve
		either, because nothing can be crimped into a hole with no
		number beside it.
	*/
	CHECK(answer.notOnPart().isEmpty());
	CHECK_FALSE(answer.ways().contains(QString()));
	CHECK(answer.usedCount() + answer.reserveCount() == answer.wayCount());
}

TEST_CASE("T34 — the pinout of a catalogue part is what feeds the rule", "[t34][connector]")
{
	/*
		The one place the two ends are put together: what the part
		hands out is a list of labels in pin order, and that list is
		the input of the rule. Nothing of the catalogue is linked into
		the rule itself - it is handed the list - and this case is what
		says the two shapes still fit.
	*/
	CatalogPart part;
	part.pins.append(CatalogPin(QStringLiteral("1"), CatalogPinRole::Unknown));
	part.pins.append(CatalogPin(QStringLiteral("2"), CatalogPinRole::Unknown));
	part.pins.append(CatalogPin(QStringLiteral("3"), CatalogPinRole::Unknown));

	const ConnectorWays answer = ConnectorWays::fromPinout(part.pinLabels(),
							       QStringList{QStringLiteral("2")});

	CHECK(answer.wayCount() == 3);
	CHECK(answer.reserve() == QStringList{QStringLiteral("1"), QStringLiteral("3")});

		// A part with no pin catalogued is the "no pinout" state
	const CatalogPart empty;
	CHECK_FALSE(ConnectorWays::fromPinout(empty.pinLabels(), QStringList()).isKnown());
}

TEST_CASE("T34 — a pin label is compared as it is written, a connector name is not",
	  "[t34][connector]")
{
	/*
		The asymmetry is a decision and not an oversight, so it is
		nailed here beside the reason. A connector name written two
		ways has nowhere to come out - counting the spellings apart
		restarts the numbering halfway and hands two ways the number 1,
		in silence - so Renumberer::connectorKey folds them. A pin
		label written two ways has notOnPart to come out in, by name,
		so it is left exactly as it was typed.

		The day a connector management window becomes the only thing
		that writes the field, it is connectorKey it has to agree with;
		a second normalising written for the reserve would be two parts
		of one program disagreeing about how many connectors there are.
	*/
	CHECK(Renumberer::connectorKey(QStringLiteral("CN1 "))
	      == Renumberer::connectorKey(QStringLiteral("cn1")));

	const ConnectorWays spaced = ConnectorWays::fromPinout(sixteenWays(),
							       QStringList{QStringLiteral("9 ")});
	CHECK(spaced.notOnPart() == QStringList{QStringLiteral("9 ")});
	CHECK(spaced.usedCount() == 0);
	CHECK(spaced.reserve().contains(QStringLiteral("9")));

	const ConnectorWays cased = ConnectorWays::fromPinout(
			QStringList{QStringLiteral("A1"), QStringLiteral("A2")},
			QStringList{QStringLiteral("a1")});
	CHECK(cased.notOnPart() == QStringList{QStringLiteral("a1")});
	CHECK(cased.reserveCount() == 2);
}
