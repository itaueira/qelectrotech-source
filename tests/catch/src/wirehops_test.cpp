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
#include "../../../sources/wirehops.h"

#include <catch2/catch.hpp>

#include <QPainterPath>

/*
	The hops of a wire crossing others (23/10/2026): two wires close together
	are two hops, smaller, and not one hop over both; and wires crossing at
	one point - an arm and the branches drawn along it - are one hop.
*/
namespace
{
	QVector<QPointF> line(qreal x1, qreal y1, qreal x2, qreal y2)
	{
		return {QPointF(x1, y1), QPointF(x2, y2)};
	}
}

TEST_CASE("Two wires close together get a hop each", "[wirehops]")
{
	const QVector<QPointF> wire = line(0, 0, 100, 0);
	const QList<QVector<QPointF>> others {line(50, -20, 50, 20), line(56, -20, 56, 20)};

	const QList<QPointF> hops = WireHops::crossings(wire, others, WireHops::Mode::Horizontal);
	REQUIRE(hops.size() == 2);
	CHECK(hops.at(0) == QPointF(50, 0));
	CHECK(hops.at(1) == QPointF(56, 0));

		//Each smaller than a lone hop, and apart: no arc reaches the other
	const QPainterPath path = WireHops::path(wire, hops, WireHops::Mode::Horizontal);
	CHECK(path.boundingRect().top() >= -2.5 - 0.01);
	CHECK(path.boundingRect().top() < -WireHops::min_radius + 0.01);
	CHECK_FALSE(path.contains(QPointF(53, -0.5)));
}

TEST_CASE("Wires crossing at one point get one hop", "[wirehops]")
{
	const QVector<QPointF> wire = line(0, 0, 100, 0);
		//The arm, and two branches whose path runs along it
	const QList<QVector<QPointF>> others {line(50, -40, 50, 40), line(50, -40, 50, 10), line(50, -40, 50, 30)};
	const QList<QPointF> hops = WireHops::crossings(wire, others, WireHops::Mode::Horizontal);
	REQUIRE(hops.size() == 1);
	CHECK(hops.first() == QPointF(50, 0));

		//A lone hop keeps its full size
	const QPainterPath path = WireHops::path(wire, hops, WireHops::Mode::Horizontal);
	CHECK(path.boundingRect().top() == Approx(-WireHops::radius));
}
