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
#include "../../../sources/location/mountingmeasure.h"
#include "qt_catch_tostring.h"

#include <QPointF>
#include <QSizeF>

#include <limits>

namespace
{
	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();

		/// @return where a movement leaves the point it is applied to
	QPointF moved(const QPointF &point, const MeasureAdjustment &answer)
	{
		return QPointF(point.x() + answer.movement.x(),
			       point.y() + answer.movement.y());
	}
}

TEST_CASE("T22 - Three readings of one pair of points agree with each other",
	  "[location][mounting][measure]")
{
		// a 3-4-5 triangle in millimetre, so the straight line is a
		// number that can be written down rather than approached
	const QPointF corner(10.0, 20.0);
	const QPointF part(13.0, 24.0);

	SECTION("Forcing a projection changes what is read, not what is read between")
	{
		CHECK(MountingMeasure::distance(corner, part,
						MeasureAxis::Direct) == Approx(5.0));
		CHECK(MountingMeasure::distance(corner, part,
						MeasureAxis::Horizontal) == 3.0);
		CHECK(MountingMeasure::distance(corner, part,
						MeasureAxis::Vertical) == 4.0);

		const qreal across = MountingMeasure::distance(corner, part,
							       MeasureAxis::Horizontal);
		const qreal down = MountingMeasure::distance(corner, part,
							     MeasureAxis::Vertical);
		const qreal straight = MountingMeasure::distance(corner, part,
								 MeasureAxis::Direct);

			// the coherence CU-22.2 asks a person to check on the
			// screen, checked here on the numbers instead
		CHECK(((across * across) + (down * down))
		      == Approx(straight * straight));
	}

	SECTION("A dimension reads the same whichever end was clicked first")
	{
		CHECK(MountingMeasure::distance(part, corner,
						MeasureAxis::Direct) == Approx(5.0));
		CHECK(MountingMeasure::distance(part, corner,
						MeasureAxis::Horizontal) == 3.0);
		CHECK(MountingMeasure::distance(part, corner,
						MeasureAxis::Vertical) == 4.0);
	}

	SECTION("A distance is never negative, whichever way the points lie")
	{
			// the part sits above and to the left of the corner
		const QPointF above(4.0, 12.0);

		CHECK(MountingMeasure::distance(corner, above,
						MeasureAxis::Horizontal) == 6.0);
		CHECK(MountingMeasure::distance(corner, above,
						MeasureAxis::Vertical) == 8.0);
		CHECK(MountingMeasure::distance(corner, above,
						MeasureAxis::Direct) == Approx(10.0));
	}

	SECTION("A coordinate nobody typed reads as no distance, not as zero")
	{
		const QPointF nowhere(not_a_number, 20.0);

		CHECK_FALSE(MountingMeasure::isLength(
				    MountingMeasure::distance(corner, nowhere)));
		CHECK_FALSE(MountingMeasure::isLength(
				    MountingMeasure::distance(corner, nowhere,
							      MeasureAxis::Vertical)));

			// zero would have been the comfortable answer, and it
			// is the one that reads as "the two are at one place"
		CHECK(MountingMeasure::isLength(0.0));
		CHECK_FALSE(MountingMeasure::isLength(-1.0));
	}
}

TEST_CASE("T22 - On the mounting surface y grows downwards and never upwards",
	  "[location][mounting][measure]")
{
		// the surface as the transfer rule declares it: 600 by 800,
		// origin at its own top left corner
	const MountingArea plate(600.0, 800.0);
		// where that corner falls in the frame the caller draws in
	const QPointF plate_origin(1000.0, 2000.0);

	SECTION("A hole below the top edge is at a positive y")
	{
			// 250 mm to the right of the corner, 30 mm below it
		const QPointF hole(1250.0, 2030.0);
		const QPointF coordinate =
			MountingMeasure::coordinateOf(hole, plate_origin);

		CHECK(coordinate.x() == 250.0);
		CHECK(coordinate.y() == 30.0);

			// the two wrong answers, named so that neither can be
			// arrived at by accident: y turned over at the origin,
			// and y measured up from the bottom edge
		CHECK_FALSE(coordinate.y() == -30.0);
		CHECK_FALSE(coordinate.y() == plate.height - 30.0);
	}

	SECTION("The way back is the way out, exactly")
	{
		const QPointF hole(1013.5, 2456.5);
		const QPointF coordinate =
			MountingMeasure::coordinateOf(hole, plate_origin);

		CHECK(coordinate.x() == 13.5);
		CHECK(coordinate.y() == 456.5);

		const QPointF back =
			MountingMeasure::pointOf(coordinate, plate_origin);
		CHECK(back.x() == hole.x());
		CHECK(back.y() == hole.y());
	}

	SECTION("The frame is the one the mounted item already uses")
	{
			// an item as the layout editor hands it over: top left
			// corner at (10, 20), 45 by 90
		const MountedItem breaker(QStringLiteral("-Q1"),
					  QPointF(10.0, 20.0),
					  QSizeF(45.0, 90.0));

			// its far corner is further right AND further down,
			// which is what makes the footprint a rectangle with a
			// positive width and height in this frame
		CHECK(breaker.footprint().right() == 55.0);
		CHECK(breaker.footprint().bottom() == 110.0);

		CHECK(MountingMeasure::fitOfCoordinate(QPointF(55.0, 110.0), plate)
		      == MountingFit::Fits);
		CHECK(MountingMeasure::distance(breaker.position,
						QPointF(55.0, 110.0),
						MeasureAxis::Vertical) == 90.0);
	}

	SECTION("A coordinate off the surface says so, and one with no surface says something else")
	{
		CHECK(MountingMeasure::fitOfCoordinate(QPointF(0.0, 0.0), plate)
		      == MountingFit::Fits);
			// flush against the far edges still counts as on it
		CHECK(MountingMeasure::fitOfCoordinate(QPointF(600.0, 800.0), plate)
		      == MountingFit::Fits);

			// above the top edge, which is where a turned over y
			// would have put a perfectly good hole
		CHECK(MountingMeasure::fitOfCoordinate(QPointF(250.0, -30.0), plate)
		      == MountingFit::OutsideArea);
		CHECK(MountingMeasure::fitOfCoordinate(QPointF(250.0, 800.5), plate)
		      == MountingFit::OutsideArea);

		CHECK(MountingMeasure::fitOfCoordinate(QPointF(250.0, 30.0),
						       MountingArea())
		      == MountingFit::NoArea);
		CHECK(MountingMeasure::fitOfCoordinate(QPointF(250.0, 30.0),
						       MountingArea(600.0, 0.0))
		      == MountingFit::NoArea);

			// a coordinate has no size, so the fourth answer cannot
			// come out of here - a hole has one, and a hole is
			// asked with its diameter, which is another question
		CHECK_FALSE(MountingMeasure::fitOfCoordinate(QPointF(-1.0, -1.0), plate)
			    == MountingFit::LargerThanArea);
	}
}

TEST_CASE("T22 - Typing a value into a dimension gives the movement that satisfies it",
	  "[location][mounting][measure]")
{
	SECTION("A vertical dimension that grows pushes the part further down")
	{
			// two ducts, one 50 mm below the other
		const QPointF upper(100.0, 100.0);
		const QPointF lower(100.0, 150.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(upper, lower, 120.0,
						    MeasureAxis::Vertical);

		REQUIRE(answer.is_possible);
		CHECK(answer.measured == 50.0);
		CHECK(answer.wanted == 120.0);
		CHECK_FALSE(answer.isAlreadyThere());

			// down is positive: the lower duct goes 70 mm further
			// down, and never 70 mm up, which is where the same
			// arithmetic in a y-up frame would have sent it
		CHECK(answer.movement.x() == 0.0);
		CHECK(answer.movement.y() == 70.0);
		CHECK(moved(lower, answer).y() == 220.0);
		CHECK(MountingMeasure::distance(upper, moved(lower, answer),
						MeasureAxis::Vertical) == 120.0);
	}

	SECTION("A vertical dimension keeps the side the part is already on")
	{
			// the same request with the part above instead of below
		const QPointF reference(100.0, 150.0);
		const QPointF part(100.0, 100.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(reference, part, 120.0,
						    MeasureAxis::Vertical);

		REQUIRE(answer.is_possible);
		CHECK(answer.movement.y() == -70.0);
		CHECK(moved(part, answer).y() == 30.0);
		CHECK(MountingMeasure::distance(reference, moved(part, answer),
						MeasureAxis::Vertical) == 120.0);
	}

	SECTION("A horizontal dimension moves across and not down")
	{
		const QPointF left(100.0, 100.0);
		const QPointF right(180.0, 143.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(left, right, 120.0,
						    MeasureAxis::Horizontal);

		REQUIRE(answer.is_possible);
		CHECK(answer.measured == 80.0);
		CHECK(answer.movement.x() == 40.0);
		CHECK(answer.movement.y() == 0.0);
		CHECK(moved(right, answer).y() == 143.0);
		CHECK(MountingMeasure::distance(left, moved(right, answer),
						MeasureAxis::Horizontal) == 120.0);
	}

	SECTION("A straight dimension moves along the line it already lies on")
	{
		const QPointF corner(10.0, 20.0);
		const QPointF part(13.0, 24.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(corner, part, 10.0,
						    MeasureAxis::Direct);

		REQUIRE(answer.is_possible);
		CHECK(answer.measured == Approx(5.0));
			// twice as far along the same 3-4-5 direction
		CHECK(answer.movement.x() == Approx(3.0));
		CHECK(answer.movement.y() == Approx(4.0));
		CHECK(MountingMeasure::distance(corner, moved(part, answer),
						MeasureAxis::Direct) == Approx(10.0));
	}

	SECTION("A dimension that shrinks pulls the part back the same way")
	{
		const QPointF upper(100.0, 100.0);
		const QPointF lower(100.0, 150.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(upper, lower, 20.0,
						    MeasureAxis::Vertical);

		REQUIRE(answer.is_possible);
		CHECK(answer.movement.y() == -30.0);
		CHECK(moved(lower, answer).y() == 120.0);
	}

	SECTION("It is the second point that moves, and the first that stays")
	{
		const QPointF first(100.0, 100.0);
		const QPointF second(100.0, 150.0);

		const MeasureAdjustment forwards =
			MountingMeasure::adjustment(first, second, 120.0,
						    MeasureAxis::Vertical);
		const MeasureAdjustment backwards =
			MountingMeasure::adjustment(second, first, 120.0,
						    MeasureAxis::Vertical);

		REQUIRE(forwards.is_possible);
		REQUIRE(backwards.is_possible);

			// the same distance was read both ways, and the other
			// end of it is what moved
		CHECK(forwards.measured == backwards.measured);
		CHECK(forwards.movement.y() == 70.0);
		CHECK(backwards.movement.y() == -70.0);
		CHECK(moved(first, backwards).y() == 30.0);
	}

	SECTION("Asking for what is already drawn is answered, and moves nothing")
	{
		const QPointF upper(100.0, 100.0);
		const QPointF lower(100.0, 220.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(upper, lower, 120.0,
						    MeasureAxis::Vertical);

		REQUIRE(answer.is_possible);
		CHECK(answer.isAlreadyThere());
		CHECK(answer.movement.x() == 0.0);
		CHECK(answer.movement.y() == 0.0);
	}
}

TEST_CASE("T22 - A dimension refuses what it cannot satisfy instead of picking a side",
	  "[location][mounting][measure]")
{
	SECTION("Two points on top of one another name no direction to push along")
	{
		const QPointF spot(100.0, 100.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(spot, spot, 120.0,
						    MeasureAxis::Direct);

		CHECK_FALSE(answer.is_possible);
		CHECK_FALSE(answer.isAlreadyThere());
			// the movement is nothing here AND in the section
			// above, which is why is_possible has to exist: the two
			// answers are told apart by it and by nothing else
		CHECK(answer.movement.x() == 0.0);
		CHECK(answer.movement.y() == 0.0);
	}

	SECTION("Two points on top of one another can still be asked to stay there")
	{
		const QPointF spot(100.0, 100.0);

		const MeasureAdjustment answer =
			MountingMeasure::adjustment(spot, spot, 0.0,
						    MeasureAxis::Direct);

		CHECK(answer.is_possible);
		CHECK(answer.isAlreadyThere());
		CHECK(answer.movement.y() == 0.0);
	}

	SECTION("Two parts at the same height have no vertical distance to set")
	{
		const QPointF left(100.0, 140.0);
		const QPointF right(300.0, 140.0);

		const MeasureAdjustment vertical =
			MountingMeasure::adjustment(left, right, 120.0,
						    MeasureAxis::Vertical);
		CHECK_FALSE(vertical.is_possible);
		CHECK(vertical.measured == 0.0);

			// and the same pair is perfectly answerable across
		const MeasureAdjustment horizontal =
			MountingMeasure::adjustment(left, right, 120.0,
						    MeasureAxis::Horizontal);
		CHECK(horizontal.is_possible);
		CHECK(horizontal.movement.x() == -80.0);
	}

	SECTION("A distance below zero is not a distance")
	{
		const MeasureAdjustment answer =
			MountingMeasure::adjustment(QPointF(100.0, 100.0),
						    QPointF(100.0, 150.0),
						    -120.0,
						    MeasureAxis::Vertical);

		CHECK_FALSE(answer.is_possible);
		CHECK(answer.movement.y() == 0.0);
	}

	SECTION("A value or a point that is not a number is refused, not folded to zero")
	{
		const MeasureAdjustment no_value =
			MountingMeasure::adjustment(QPointF(100.0, 100.0),
						    QPointF(100.0, 150.0),
						    not_a_number,
						    MeasureAxis::Vertical);
		CHECK_FALSE(no_value.is_possible);
		CHECK_FALSE(no_value.isAlreadyThere());

		const MeasureAdjustment no_point =
			MountingMeasure::adjustment(QPointF(100.0, not_a_number),
						    QPointF(100.0, 150.0),
						    120.0,
						    MeasureAxis::Vertical);
		CHECK_FALSE(no_point.is_possible);
		CHECK_FALSE(no_point.isAlreadyThere());
	}

	SECTION("Equality of lengths is the one the mounting rules already use")
	{
		const qreal slack = MountingArea::tolerance();

		CHECK(MountingMeasure::isSameLength(120.0, 120.0));
		CHECK(MountingMeasure::isSameLength(120.0, 120.0 + (slack / 2.0)));
		CHECK_FALSE(MountingMeasure::isSameLength(120.0, 120.1));
		CHECK_FALSE(MountingMeasure::isSameLength(120.0, not_a_number));
	}
}
