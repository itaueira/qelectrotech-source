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
#include "../../../sources/location/mountingcheck.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>

#include <limits>

namespace
{
	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();

	/**
		@return one part as the layout editor will hand it over: an
		identity, a product code, a corner and a size, all in
		millimetre
	*/
	MountedItem part(const char *uuid,
			 const char *code,
			 qreal x, qreal y,
			 qreal width, qreal height)
	{
		MountedItem item(QString::fromUtf8(uuid),
				 QPointF(x, y),
				 QSizeF(width, height));
		item.uuid = QString::fromUtf8(uuid);
		item.part_code = QString::fromUtf8(code);
		return item;
	}

	/// @return a part whose dimensions nobody has typed, in either spelling
	MountedItem unmeasuredPart(const char *uuid,
				   qreal x, qreal y,
				   const QSizeF &size)
	{
		MountedItem item(QString::fromUtf8(uuid), QPointF(x, y), size);
		item.uuid = QString::fromUtf8(uuid);
		return item;
	}

	/// @return the mounting plate of a location, measured in millimetre
	MountingSurface plate(const char *location, qreal width, qreal height)
	{
		MountingSurface surface(QString::fromUtf8(location),
					QStringLiteral("plate"),
					MountingArea(width, height));
		surface.uuid = QStringLiteral("plate-1");
		return surface;
	}

	/// @return a table of clearances holding one product code
	QHash<QString, MountingClearance> asks(const char *code,
					       const MountingClearance &clearance)
	{
		QHash<QString, MountingClearance> table;
		table.insert(QString::fromUtf8(code), clearance);
		return table;
	}
}

TEST_CASE("T19 — two parts wanting the same room are named, and neighbours are not",
	  "[location][mounting][check]")
{
	SECTION("breakers clipped shoulder to shoulder are not a complaint")
	{
		QList<MountedItem> items;
		items << part("Q1", "BREAKER", 0, 0, 22.5, 80)
		      << part("Q2", "BREAKER", 22.5, 0, 22.5, 80)
		      << part("Q3", "BREAKER", 45, 0, 22.5, 80);

		CHECK(MountingCheck::overlaps(items).isEmpty());
	}

	SECTION("one pair, once, with the room they share and the way out")
	{
		QList<MountedItem> items;
		items << part("Q1", "BREAKER", 0, 0, 22.5, 80)
		      << part("Q2", "BREAKER", 22.5, 0, 22.5, 80)
		      << part("Q3", "BREAKER", 40, 0, 22.5, 80);

		const QList<MountingOverlap> found = MountingCheck::overlaps(items);
		REQUIRE(found.count() == 1);
		CHECK(found.first().first_uuid == QStringLiteral("Q2"));
		CHECK(found.first().second_uuid == QStringLiteral("Q3"));
		CHECK(found.first().shared.left() == Approx(40.0));
		CHECK(found.first().shared.width() == Approx(5.0));
		CHECK(found.first().shared.height() == Approx(80.0));
			//5 mm along x, and not the 80 mm of shared height:
			//the shorter way out is the one a person drags
		CHECK(found.first().way_out == Approx(5.0));
	}

	SECTION("a part nobody measured collides with nothing, and neither does a part nowhere")
	{
		QList<MountedItem> items;
		items << part("Q1", "BREAKER", 0, 0, 22.5, 80)
		      << unmeasuredPart("Q2", 10, 10, QSizeF())
		      << unmeasuredPart("Q3", 10, 10, QSizeF(0, 0))
		      << part("Q4", "BREAKER", not_a_number, 0, 22.5, 80);

		CHECK(MountingCheck::overlaps(items).isEmpty());
	}

	SECTION("a part right on top of another one is one complaint, not two")
	{
		QList<MountedItem> items;
		items << part("Q1", "BREAKER", 100, 100, 45, 80)
		      << part("Q2", "BREAKER", 100, 100, 45, 80);

		const QList<MountingOverlap> found = MountingCheck::overlaps(items);
		REQUIRE(found.count() == 1);
		CHECK(found.first().shared.width() == Approx(45.0));
		CHECK(found.first().way_out == Approx(45.0));
	}
}

TEST_CASE("T19 — the air a part asks for belongs to it, and is never counted twice",
	  "[location][mounting][check]")
{
	SECTION("a duct 30 mm from a drive that asked for 100 is 70 mm short")
	{
		QList<MountedItem> items;
		items << part("K1", "DRIVE", 0, 0, 100, 200)
		      << part("W1", "DUCT", 130, 0, 60, 200);

		const QList<MountingEncroachment> found =
			MountingCheck::encroachments(items,
						     asks("DRIVE",
							  MountingClearance::uniform(100)));
		REQUIRE(found.count() == 1);
			//the claimant is the one that asked, the intruder is
			//the one standing in it - not the other way round
		CHECK(found.first().claimant_uuid == QStringLiteral("K1"));
		CHECK(found.first().intruder_uuid == QStringLiteral("W1"));
		CHECK(found.first().missing == Approx(70.0));
			//the shared room is the whole body of the duct, which
			//is why the missing air cannot be read off it
		CHECK(found.first().shared.width() == Approx(60.0));
	}

	SECTION("two parts that each asked for 100 and stand 150 apart are both satisfied")
	{
		QList<MountedItem> items;
		items << part("K1", "DRIVE", 0, 0, 100, 200)
		      << part("K2", "DRIVE", 250, 0, 100, 200);

			//growing both bodies and intersecting them would
			//report a violation here: the two grown rectangles
			//overlap by 50 mm while each part has its 100 mm of
			//body-free room
		CHECK(MountingCheck::encroachments(items,
						   asks("DRIVE",
							MountingClearance::uniform(100)))
		      .isEmpty());
	}

	SECTION("when both asked and both are starved, both are told")
	{
		QList<MountedItem> items;
		items << part("K1", "DRIVE", 0, 0, 100, 200)
		      << part("K2", "DRIVE", 130, 0, 100, 200);

		const QList<MountingEncroachment> found =
			MountingCheck::encroachments(items,
						     asks("DRIVE",
							  MountingClearance::uniform(100)));
		REQUIRE(found.count() == 2);
		CHECK(found.at(0).claimant_uuid == QStringLiteral("K1"));
		CHECK(found.at(0).intruder_uuid == QStringLiteral("K2"));
		CHECK(found.at(0).missing == Approx(70.0));
		CHECK(found.at(1).claimant_uuid == QStringLiteral("K2"));
		CHECK(found.at(1).intruder_uuid == QStringLiteral("K1"));
		CHECK(found.at(1).missing == Approx(70.0));
	}

	SECTION("metal on metal is the other complaint, and is not repeated here")
	{
		QList<MountedItem> items;
		items << part("K1", "DRIVE", 0, 0, 100, 200)
		      << part("K2", "DRIVE", 50, 0, 100, 200);

		CHECK(MountingCheck::overlaps(items).count() == 1);
		CHECK(MountingCheck::encroachments(items,
						   asks("DRIVE",
							MountingClearance::uniform(100)))
		      .isEmpty());
	}

	SECTION("the air is asked for on named sides, and only there")
	{
		QList<MountedItem> items;
		items << part("K1", "DRIVE", 0, 200, 100, 100)
		      << part("W1", "DUCT", 0, 150, 100, 20)
		      << part("W2", "DUCT", 130, 200, 50, 100);

		const QList<MountingEncroachment> found =
			MountingCheck::encroachments(items,
						     asks("DRIVE",
							  MountingClearance(100, 0, 0, 0)));
		REQUIRE(found.count() == 1);
			//the duct above it is 30 mm away where 100 was asked
		CHECK(found.first().intruder_uuid == QStringLiteral("W1"));
		CHECK(found.first().missing == Approx(70.0));
	}
}

TEST_CASE("T19 — a clearance nobody measured asks for nothing, and a negative one shrinks nothing",
	  "[location][mounting][check]")
{
	QList<MountedItem> items;
	items << part("K1", "DRIVE", 0, 0, 100, 200)
	      << part("W1", "DUCT", 101, 0, 60, 200);

	SECTION("no table at all is no requirement, not a refusal to answer")
	{
		CHECK(MountingCheck::encroachments(items,
						   QHash<QString, MountingClearance>())
		      .isEmpty());
	}

	SECTION("a part with no product code asks for nothing, whatever the table says")
	{
		QList<MountedItem> nameless;
		nameless << part("K1", "", 0, 0, 100, 200)
			 << part("W1", "DUCT", 101, 0, 60, 200);

		CHECK(MountingCheck::encroachments(nameless,
						   asks("DRIVE",
							MountingClearance::uniform(100)))
		      .isEmpty());
	}

	SECTION("zero, absent and negative are one state, and it is not a violation")
	{
		CHECK(MountingClearance().isEmpty());
		CHECK(MountingClearance(0, 0, 0, 0).isEmpty());
		CHECK(MountingClearance(-5, -5, -5, -5).isEmpty());
		CHECK(MountingClearance(not_a_number, 0, 0, 0).isEmpty());
		CHECK(MountingClearance(-5, -5, -5, -5) == MountingClearance());

		CHECK(MountingCheck::encroachments(items,
						   asks("DRIVE",
							MountingClearance(-5, -5, -5, -5)))
		      .isEmpty());
	}

	SECTION("a negative clearance never shrinks the body it belongs to")
	{
		const QRectF body(0, 0, 100, 200);
		CHECK(MountingClearance(-5, -5, -5, -5).grown(body) == body);
		CHECK(MountingClearance().grown(body) == body);
		CHECK(MountingClearance::uniform(10).grown(body)
		      == QRectF(-10, -10, 120, 220));
	}

	SECTION("a claimant nobody measured claims no air around a body it has not got")
	{
		QList<MountedItem> unmeasured;
		MountedItem drive = unmeasuredPart("K1", 0, 0, QSizeF());
		drive.part_code = QStringLiteral("DRIVE");
		unmeasured << drive
			   << part("W1", "DUCT", 10, 0, 60, 200);

		CHECK(MountingCheck::encroachments(unmeasured,
						   asks("DRIVE",
							MountingClearance::uniform(100)))
		      .isEmpty());
	}
}

TEST_CASE("T19 — a rail says what is clipped on it, what is left, and what the answer rests on",
	  "[location][mounting][check]")
{
	QList<MountedItem> breakers;
	breakers << part("Q1", "BREAKER", 0, 0, 22.5, 80)
		 << part("Q2", "BREAKER", 22.5, 0, 22.5, 80)
		 << part("Q3", "BREAKER", 45, 0, 22.5, 80);

	SECTION("a rail cut to 300 mm with three breakers on it has 232.5 mm left")
	{
		const MountingRailFill fill = MountingCheck::railFill(breakers, 300);
		CHECK(fill.isMeasured());
		CHECK(fill.item_count == 3);
		CHECK(fill.used == Approx(67.5));
		CHECK(fill.free() == Approx(232.5));
		CHECK(fill.ratio() == Approx(0.225));
		CHECK_FALSE(fill.isOverfilled());
		CHECK(fill.overrun() == Approx(0.0));
		CHECK(fill.isConclusive());
	}

	SECTION("more clipped on than the rail is long is said out loud")
	{
		const MountingRailFill fill = MountingCheck::railFill(breakers, 60);
		CHECK(fill.isOverfilled());
		CHECK(fill.overrun() == Approx(7.5));
		CHECK(fill.free() == Approx(-7.5));
	}

	SECTION("a part with no width counts as nothing and is counted as such")
	{
		QList<MountedItem> mixed = breakers;
		mixed << unmeasuredPart("Q4", 67.5, 0, QSizeF())
		      << unmeasuredPart("Q5", 90, 0, QSizeF(0, 0));

		const MountingRailFill fill = MountingCheck::railFill(mixed, 300);
		CHECK(fill.item_count == 5);
		CHECK(fill.unknown_width_count == 2);
		CHECK(fill.used == Approx(67.5));
			//room to spare, and the answer is worth what the two
			//missing measurements are worth
		CHECK(fill.free() == Approx(232.5));
		CHECK_FALSE(fill.isConclusive());
	}

	SECTION("a rail nobody has cut yet is not a rail that is too short")
	{
		const MountingRailFill fill = MountingCheck::railFill(breakers, 0);
		CHECK_FALSE(fill.isMeasured());
		CHECK_FALSE(fill.isOverfilled());
		CHECK(fill.free() == Approx(0.0));
		CHECK(fill.overrun() == Approx(0.0));
		CHECK(fill.ratio() == Approx(0.0));
		CHECK(fill.used == Approx(67.5));
	}
}

TEST_CASE("T19 — whether a part is on the plate is the answer the enclosure swap already gives",
	  "[location][mounting][check]")
{
	const MountingArea area(600, 800);
	QList<MountedItem> items;
	items << part("Q1", "BREAKER", 10, 10, 22.5, 80)
	      << part("Q2", "BREAKER", 590, 10, 50, 80)
	      << part("K1", "DRIVE", 10, 10, 700, 200)
	      << unmeasuredPart("Q3", 10, 400, QSizeF());

	const QList<MountingItemFit> fits = MountingCheck::fits(items, area);
	REQUIRE(fits.count() == 4);

	CHECK(fits.at(0).uuid == QStringLiteral("Q1"));
	CHECK(fits.at(0).fit == MountingFit::Fits);
	CHECK_FALSE(fits.at(0).size_unknown);
	CHECK(fits.at(0).isFitting());

		//it would fit the plate somewhere else: this one is dragged
	CHECK(fits.at(1).fit == MountingFit::OutsideArea);
		//this one is not dragged anywhere, it is 700 mm wide
	CHECK(fits.at(2).fit == MountingFit::LargerThanArea);
		//a part nobody measured is checked as a point, and said so
	CHECK(fits.at(3).fit == MountingFit::Fits);
	CHECK(fits.at(3).size_unknown);

		//the same four answers the enclosure swap gives, because it is
		//the same rule and not a second copy of it
	for (int index = 0 ; index < items.count() ; ++index)
	{
		CHECK(fits.at(index).fit
		      == EnclosureTransfer::fitOf(items.at(index), area));
	}

	SECTION("a face nobody has measured answers NoArea about everything")
	{
		const QList<MountingItemFit> unmeasured =
			MountingCheck::fits(items, MountingArea());
		REQUIRE(unmeasured.count() == 4);
		for (const MountingItemFit &entry : unmeasured) {
			CHECK(entry.fit == MountingFit::NoArea);
		}
	}

	SECTION("a part flush with the edge of the plate is on the plate")
	{
		QList<MountedItem> flush;
		flush << part("W1", "DUCT", 0, 0, 600, 60);
		CHECK(MountingCheck::fits(flush, area).first().fit
		      == MountingFit::Fits);
	}
}

TEST_CASE("T19 — the report reads the face's own area and its own items",
	  "[location][mounting][check]")
{
	MountingSurface surface = plate("QCM1", 300, 400);
	surface.items << part("Q1", "BREAKER", 0, 0, 22.5, 80)
		      << part("Q2", "BREAKER", 10, 0, 22.5, 80)
		      << part("K1", "DRIVE", 0, 200, 100, 100)
		      << part("W1", "DUCT", 0, 150, 100, 20)
		      << part("Q9", "BREAKER", 290, 300, 22.5, 80);

	MountingLayout layout;
	REQUIRE_FALSE(layout.appendSurface(surface).isEmpty());

	const MountingSurfaceReport report =
		MountingCheck::surfaceReport(layout.surface(QStringLiteral("plate-1")),
					     asks("DRIVE",
						  MountingClearance(100, 0, 0, 0)));

	CHECK(report.surface_uuid == QStringLiteral("plate-1"));
		//the numbers of the stored face, and not a plate this rule
		//invented for itself
	CHECK(report.area.width == Approx(300.0));
	CHECK(report.area.height == Approx(400.0));
	CHECK(report.itemCount() == 5);

	REQUIRE(report.overlaps.count() == 1);
	CHECK(report.overlaps.first().first_uuid == QStringLiteral("Q1"));
	CHECK(report.overlaps.first().second_uuid == QStringLiteral("Q2"));

	REQUIRE(report.encroachments.count() == 1);
	CHECK(report.encroachments.first().claimant_uuid == QStringLiteral("K1"));
	CHECK(report.encroachments.first().intruder_uuid == QStringLiteral("W1"));
	CHECK(report.encroachments.first().missing == Approx(70.0));

		//the breaker at 290 mm hangs off a plate 300 mm wide
	REQUIRE(report.misfitCount() == 1);
	CHECK(report.misfits().first().uuid == QStringLiteral("Q9"));
	CHECK(report.misfits().first().fit == MountingFit::OutsideArea);

	CHECK(report.issueCount() == 3);
	CHECK_FALSE(report.isClean());
	CHECK(report.unknown_size_count == 0);
		//four of the five parts were checked against no clearance
	CHECK(report.unknown_clearance_count == 4);
	CHECK_FALSE(report.isConclusive());

		//each part named once, in the order the complaints were built
	const QStringList named = report.complainedAbout();
	CHECK(named == QStringList({QStringLiteral("Q1"),
				    QStringLiteral("Q2"),
				    QStringLiteral("K1"),
				    QStringLiteral("W1"),
				    QStringLiteral("Q9")}));
}

TEST_CASE("T19 — a plate nobody measured comes back clean, and says what that is worth",
	  "[location][mounting][check]")
{
	MountingSurface surface = plate("QCM1", 600, 800);
	surface.items << unmeasuredPart("Q1", 100, 100, QSizeF())
		      << unmeasuredPart("Q2", 100, 100, QSizeF(0, 0));

	const MountingSurfaceReport report = MountingCheck::surfaceReport(surface);

		//two parts at the very same coordinate, and no complaint: a
		//part with no dimensions cannot be shown to collide
	CHECK(report.isClean());
	CHECK(report.issueCount() == 0);
		//which is exactly why the second question has to be asked
	CHECK(report.unknown_size_count == 2);
	CHECK_FALSE(report.isConclusive());
	CHECK(report.complainedAbout().isEmpty());

	SECTION("a code asking for zero is still a code nobody measured")
	{
		MountingSurface measured = plate("QCM1", 600, 800);
		measured.items << part("Q1", "BREAKER", 0, 0, 22.5, 80)
			       << part("Q2", "BREAKER", 22.5, 0, 22.5, 80);

		const MountingSurfaceReport clean =
			MountingCheck::surfaceReport(measured,
						     asks("BREAKER",
							  MountingClearance::uniform(0)));
		CHECK(clean.isClean());
		CHECK(clean.unknown_size_count == 0);
			//zero and absent are one state, so the answer still
			//rests on a measurement nobody took
		CHECK(clean.unknown_clearance_count == 2);
		CHECK_FALSE(clean.isConclusive());
	}

	SECTION("with a real clearance on every code the answer stands on itself, rail neighbours included")
	{
		MountingSurface measured = plate("QCM1", 600, 800);
		measured.items << part("Q1", "BREAKER", 0, 0, 22.5, 80)
			       << part("Q2", "BREAKER", 22.5, 0, 22.5, 80);

		const MountingSurfaceReport report =
			MountingCheck::surfaceReport(measured,
						     asks("BREAKER",
							  MountingClearance::uniform(5)));
		CHECK(report.unknown_size_count == 0);
		CHECK(report.unknown_clearance_count == 0);
		CHECK(report.isConclusive());
			//and the limit written down in MountingCheck: nothing
			//in the stored layout says these two are clipped onto
			//the same rail, so 5 mm recorded against a breaker is
			//asked for against the breaker beside it
		CHECK(report.encroachments.count() == 2);
		CHECK(report.overlaps.isEmpty());
	}

	SECTION("a face with no usable area is never a conclusive answer")
	{
		MountingSurface unmeasured_face =
			MountingSurface(QStringLiteral("QCM1"),
					QStringLiteral("plate"));
		unmeasured_face.items << part("Q1", "BREAKER", 0, 0, 22.5, 80);

		const MountingSurfaceReport report_of_nothing =
			MountingCheck::surfaceReport(unmeasured_face);
		CHECK_FALSE(report_of_nothing.isConclusive());
		CHECK(report_of_nothing.misfitCount() == 1);
		CHECK(report_of_nothing.fits.first().fit == MountingFit::NoArea);
	}
}
