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
#include "../../../sources/location/locationboundary.h"
#include "qt_catch_tostring.h"

#include <QString>

/*
	The rule behind the dashed external wire: whether a conductor between two
	locations leaves the panel.

	The whole point of this rule being a free function over two strings is
	that it can be checked here, so it is checked here exhaustively rather
	than by drawing conductors on a folio and looking at them.
*/

TEST_CASE("a place and a place inside it are one place",
	  "[locationboundary]")
{
	SECTION("the door of an enclosure does not leave the enclosure")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("QCM1"),
						    QStringLiteral("QCM1/PORTE")));
	}

	SECTION("the same place is the degenerate case of containment")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("QCM1"),
						    QStringLiteral("QCM1")));
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("A/B/C"),
						    QStringLiteral("A/B/C")));
	}

	SECTION("containment holds at any depth, and in either order")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("A"),
						    QStringLiteral("A/B/C/D")));
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("A/B/C/D"),
						    QStringLiteral("A")));
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("A/B/C"),
						    QStringLiteral("A/B")));
	}
}

TEST_CASE("two places neither of which holds the other are a boundary",
	  "[locationboundary]")
{
	SECTION("two enclosures in one room are still two enclosures")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("PLANTA/QCM1"),
					      QStringLiteral("PLANTA/QCM2")));
	}

	SECTION("leaving the enclosure for the field")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("QCM1"),
					      QStringLiteral("CAMPO")));
	}

	SECTION("a divergence deep in the path is still a divergence")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("A/B/C"),
					      QStringLiteral("A/X/C")));
	}

		/*
			The consequence decision L wrote down because it surprises
			people: two mounting surfaces of one cabinet, modelled as
			siblings, are two places. It is true - the wire needs slack, a
			gland, and it probably crosses the hinge - and it is not what
			the wording of the use case leads a reader to expect.
		*/
	SECTION("two sibling mounting surfaces of one cabinet are a boundary")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("QCM1/PLACA"),
					      QStringLiteral("QCM1/PORTE")));
	}
}

/*
	The trap that made segment comparison the rule rather than a string
	prefix test. A prefix test answers these wrong, and only on the day
	somebody numbers an enclosure past nine.
*/
TEST_CASE("comparison is by whole segment, never by prefix of text",
	  "[locationboundary]")
{
	SECTION("QCM1 is not an ancestor of QCM10")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("QCM1"),
					      QStringLiteral("QCM10")));
	}

	SECTION("the trap survives being buried in the middle of a path")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("QCM1/A"),
					      QStringLiteral("QCM10/A")));
	}

	SECTION("a real ancestor still answers no")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("QCM10"),
						    QStringLiteral("QCM10/A")));
	}
}

/*
	Case is ignored because LocationTree ignores it: two sibling codes
	differing only in case are one location as far as the tree is concerned,
	so a case sensitive rule here would have the folio contradict the
	location manager.
*/
TEST_CASE("case does not make a boundary",
	  "[locationboundary]")
{
	SECTION("one segment")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("qcm1"),
						    QStringLiteral("QCM1")));
	}

	SECTION("a segment inside a path")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("QCM1/porte"),
						    QStringLiteral("QCM1/PORTE")));
	}

	SECTION("case does not rescue a real boundary either")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("qcm1"),
					      QStringLiteral("QCM2")));
	}
}

/*
	Decision F says an empty path means "not known yet" and not "nowhere".
	Dashing against an unknown would paint half of any existing project as
	external the day the option is first switched on.
*/
TEST_CASE("an end that asserts nothing never makes a boundary",
	  "[locationboundary]")
{
	SECTION("one end empty")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("QCM1"),
						    QString()));
		CHECK_FALSE(crossesLocationBoundary(QString(),
						    QStringLiteral("QCM1")));
	}

	SECTION("both ends empty")
	{
		CHECK_FALSE(crossesLocationBoundary(QString(), QString()));
	}

		//A path of nothing but separators carries no code, so it asserts
		//nothing either - splitPath is what makes the two cases one.
	SECTION("a path of separators asserts nothing")
	{
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("//"),
						    QStringLiteral("QCM1")));
		CHECK_FALSE(crossesLocationBoundary(QStringLiteral("/"),
						    QStringLiteral("CAMPO")));
	}
}

/*
	Two properties that hold for every pair, and that would each let a whole
	class of mistakes through if they stopped holding.
*/
TEST_CASE("the rule is symmetric and indifferent to typing",
	  "[locationboundary]")
{
	SECTION("swapping the two ends never changes the answer")
	{
		CHECK(crossesLocationBoundary(QStringLiteral("QCM1"),
					      QStringLiteral("CAMPO"))
		      == crossesLocationBoundary(QStringLiteral("CAMPO"),
						 QStringLiteral("QCM1")));

		CHECK(crossesLocationBoundary(QStringLiteral("QCM1"),
					      QStringLiteral("QCM1/PORTE"))
		      == crossesLocationBoundary(QStringLiteral("QCM1/PORTE"),
						 QStringLiteral("QCM1")));
	}

	SECTION("stray spaces around a code are not a boundary")
	{
		CHECK_FALSE(crossesLocationBoundary(
					QStringLiteral(" QCM1 / PORTE "),
					QStringLiteral("QCM1/PORTE")));
	}
}
