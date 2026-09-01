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
#include "../../../sources/location/enclosuretransfer.h"
#include "qt_catch_tostring.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <limits>

namespace
{
		/// @return an item as the layout editor will hand it over
	MountedItem mounted(const char *label,
			    qreal x, qreal y,
			    qreal item_width, qreal item_height)
	{
		MountedItem item(QString::fromUtf8(label),
				 QPointF(x, y),
				 QSizeF(item_width, item_height));
		item.uuid = QString::fromUtf8(label);
		return item;
	}

		/// @return what the plan decided about the item called @a label
	MountingFit fitFor(const EnclosureTransferPlan &plan, const char *label)
	{
		return plan.entryOf(QString::fromUtf8(label)).fit;
	}

		/// @return where the plan puts the item called @a label
	QPointF placeOf(const EnclosureTransferPlan &plan, const char *label)
	{
		return plan.entryOf(QString::fromUtf8(label)).position;
	}

		/// @return the room the item called @a label takes after the swap
	QRectF footprintOf(const EnclosureTransferPlan &plan, const char *label)
	{
		const TransferredItem entry = plan.entryOf(QString::fromUtf8(label));
		return QRectF(entry.position, entry.item.declaredSize());
	}

		/// @return how many lines of the warning name an item
	int namedLines(const QString &warning, const QString &mark)
	{
		int count = 0;
		const QStringList lines = warning.split(QChar('\n'));
		for (const QString &line : lines)
		{
			if (line.contains(mark)) {
				++count;
			}
		}
		return count;
	}

	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();
}

TEST_CASE("Swapping the enclosure keeps every item it can and names the ones it cannot",
	  "[location][enclosure]")
{
	QList<MountedItem> items;
	items << mounted("-Q1", 10.0, 20.0, 45.0, 90.0)
	      << mounted("-Q3", 500.0, 20.0, 45.0, 90.0)
	      << mounted("-Q4", 10.0, 700.0, 45.0, 90.0);

	const MountingArea old_area(600.0, 800.0);

	SECTION("An enclosure of the same size moves nothing and says nothing")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(600.0, 800.0));

		REQUIRE(plan.entries.count() == 3);
		CHECK(plan.isUsable());
		CHECK(plan.transferCount() == 3);
		CHECK_FALSE(plan.hasLoss());
		CHECK(plan.lostDesignations().isEmpty());
		CHECK_FALSE(plan.dimensionsChanged());
		CHECK_FALSE(plan.shrinks());
		CHECK_FALSE(plan.grows());
		CHECK(plan.warning().isEmpty());
	}

	SECTION("A bigger enclosure loses nothing and still reports the change")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(800.0, 1000.0));

		CHECK(plan.transferCount() == 3);
		CHECK_FALSE(plan.hasLoss());
		CHECK(plan.dimensionsChanged());
		CHECK(plan.grows());
		CHECK_FALSE(plan.shrinks());
		CHECK(plan.sizeChange().width() == 200.0);
		CHECK(plan.sizeChange().height() == 200.0);

			// nothing lost is not nothing to say: a swap nobody
			// warns about is read as a swap that costs nothing
		CHECK_FALSE(plan.warning().isEmpty());

			// and the extra room is added, not distributed
		CHECK(placeOf(plan, "-Q4").x() == 10.0);
		CHECK(placeOf(plan, "-Q4").y() == 700.0);
	}

	SECTION("A smaller enclosure names what falls out and leaves the rest alone")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(400.0, 600.0));

		CHECK(plan.shrinks());
		CHECK_FALSE(plan.grows());
		CHECK(plan.transferCount() == 1);
		CHECK(plan.lossCount() == 2);

			// which ones, and not how many
		const QStringList lost = plan.lostDesignations();
		CHECK(lost.count() == 2);
		CHECK(lost.contains(QStringLiteral("-Q3")));
		CHECK(lost.contains(QStringLiteral("-Q4")));
		CHECK_FALSE(lost.contains(QStringLiteral("-Q1")));

		CHECK(fitFor(plan, "-Q1") == MountingFit::Fits);
		CHECK(fitFor(plan, "-Q3") == MountingFit::OutsideArea);
		CHECK(fitFor(plan, "-Q4") == MountingFit::OutsideArea);

			// the survivor is where the mounting plan drilled it
		CHECK(placeOf(plan, "-Q1").x() == 10.0);
		CHECK(placeOf(plan, "-Q1").y() == 20.0);

		const QString warning = plan.warning();
		CHECK(warning.contains(QStringLiteral("-Q3")));
		CHECK(warning.contains(QStringLiteral("-Q4")));
	}

	SECTION("Taller and narrower is another enclosure and not a bigger one")
	{
		QList<MountedItem> mixed;
		mixed << mounted("-W1", 0.0, 0.0, 500.0, 60.0)
		      << mounted("-Q6", 350.0, 700.0, 45.0, 90.0)
		      << mounted("-Q7", 380.0, 100.0, 45.0, 90.0);

		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(mixed, old_area,
						MountingArea(400.0, 1200.0));

			// both at once, which is what makes this swap the
			// dangerous one: 400 more millimetre of height do not
			// pay for the 200 taken off the width
		CHECK(plan.shrinks());
		CHECK(plan.grows());

			// the duct is wider than the plate now, anywhere
		CHECK(fitFor(plan, "-W1") == MountingFit::LargerThanArea);
			// the height it gained is what keeps this one
		CHECK(fitFor(plan, "-Q6") == MountingFit::Fits);
		CHECK(placeOf(plan, "-Q6").y() == 700.0);
			// and the width it lost is what costs this one
		CHECK(fitFor(plan, "-Q7") == MountingFit::OutsideArea);

		CHECK(plan.lostDesignations().count() == 2);
	}

	SECTION("An item larger than the whole surface is told apart from a misplaced one")
	{
		QList<MountedItem> duct;
		duct << mounted("-W2", 0.0, 0.0, 500.0, 60.0);

		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(duct, old_area,
						MountingArea(400.0, 600.0));

		CHECK(fitFor(plan, "-W2") == MountingFit::LargerThanArea);

			// the sentence has to carry the size, because this is
			// the loss no amount of dragging repairs
		const QString reason = plan.entryOf(QStringLiteral("-W2")).reason();
		CHECK(reason.contains(QStringLiteral("-W2")));
		CHECK(reason.contains(QStringLiteral("500")));
	}
}

TEST_CASE("Transferring preserves the millimetres and not the proportions",
	  "[location][enclosure]")
{
	QList<MountedItem> row;
	row << mounted("-Q1", 0.0, 0.0, 45.0, 90.0)
	    << mounted("-Q2", 45.0, 0.0, 45.0, 90.0);

	const MountingArea old_area(600.0, 800.0);

	SECTION("Two parts mounted flush stay flush when the plate narrows")
	{
		const MountingArea new_area(300.0, 800.0);
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(row, old_area, new_area);

		CHECK(fitFor(plan, "-Q1") == MountingFit::Fits);
		CHECK(fitFor(plan, "-Q2") == MountingFit::Fits);

			// the very millimetre, not something close to it
		CHECK(placeOf(plan, "-Q1").x() == 0.0);
		CHECK(placeOf(plan, "-Q2").x() == 45.0);

		const QRectF first = footprintOf(plan, "-Q1");
		const QRectF second = footprintOf(plan, "-Q2");
		CHECK(second.left() - first.right() == 0.0);
		CHECK_FALSE(first.intersects(second));

			// what the rejected rule would have done: the plate is
			// half as wide, so a proportional transfer halves the
			// coordinate while the breaker keeps its 45 mm, and the
			// two bodies end up inside one another
		const qreal proportional_x =
			45.0 * (new_area.width / old_area.width);
		CHECK(proportional_x == 22.5);
		const QRectF proportional(QPointF(proportional_x, 0.0),
					  QSizeF(45.0, 90.0));
		CHECK(proportional.intersects(first));
	}

	SECTION("An enclosure that only grows moves nothing at all")
	{
		const MountingArea new_area(900.0, 2000.0);
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(row, old_area, new_area);

		CHECK(plan.transferCount() == 2);
		CHECK(placeOf(plan, "-Q1").x() == 0.0);
		CHECK(placeOf(plan, "-Q2").x() == 45.0);
		CHECK(placeOf(plan, "-Q2").y() == 0.0);
	}

	SECTION("The rule itself is the identity on coordinates")
	{
		const MountedItem item = mounted("-Q9", 123.5, 456.5, 45.0, 90.0);
		const QPointF place =
			EnclosureTransfer::transferredPosition(item,
							       old_area,
							       MountingArea(1200.0, 300.0));

		CHECK(place.x() == item.position.x());
		CHECK(place.y() == item.position.y());
	}
}

TEST_CASE("The edge of the mounting surface counts as inside",
	  "[location][enclosure]")
{
	const MountingArea area(600.0, 800.0);

	SECTION("A duct run right to the edge is kept")
	{
		QList<MountedItem> items;
		items << mounted("-W3", 0.0, 740.0, 600.0, 60.0);

		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, area, area);

		CHECK(fitFor(plan, "-W3") == MountingFit::Fits);
		CHECK(plan.warning().isEmpty());
	}

	SECTION("A micron past the edge is not")
	{
		QList<MountedItem> items;
		items << mounted("-W4", 0.0, 740.001, 600.0, 60.0);

		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, area, area);

		CHECK(fitFor(plan, "-W4") == MountingFit::OutsideArea);
		CHECK(plan.lostDesignations().contains(QStringLiteral("-W4")));
	}

	SECTION("A negative coordinate is outside, edge or no edge")
	{
		CHECK(area.holds(QRectF(0.0, 0.0, 600.0, 800.0)));
		CHECK_FALSE(area.holds(QRectF(-0.001, 0.0, 10.0, 10.0)));
		CHECK(area.canEverHold(QSizeF(600.0, 800.0)));
		CHECK_FALSE(area.canEverHold(QSizeF(601.0, 800.0)));
	}
}

TEST_CASE("An enclosure with no usable surface loses everything and admits it",
	  "[location][enclosure]")
{
	QList<MountedItem> items;
	items << mounted("-Q1", 10.0, 20.0, 45.0, 90.0)
	      << mounted("-Q2", 60.0, 20.0, 45.0, 90.0);

	const MountingArea old_area(600.0, 800.0);

	SECTION("A surface of zero millimetre")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(0.0, 0.0));

		CHECK_FALSE(plan.isUsable());
		CHECK(plan.transferCount() == 0);
		CHECK(plan.lossCount() == 2);
		CHECK(fitFor(plan, "-Q1") == MountingFit::NoArea);
		CHECK(fitFor(plan, "-Q2") == MountingFit::NoArea);

			// everything is at stake, so everything is named
		CHECK(plan.lostDesignations().count() == 2);
		CHECK_FALSE(plan.warning().isEmpty());
	}

	SECTION("A dimension typed with a minus sign")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(-600.0, 800.0));

		CHECK_FALSE(plan.isUsable());
		CHECK(fitFor(plan, "-Q1") == MountingFit::NoArea);
		CHECK(plan.lostDesignations().count() == 2);
	}

	SECTION("A dimension that is not a number")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(not_a_number, 800.0));

		CHECK_FALSE(plan.isUsable());
		CHECK(fitFor(plan, "-Q2") == MountingFit::NoArea);
		CHECK_FALSE(plan.warning().isEmpty());

			// unmeasurable is not unchanged
		CHECK(plan.dimensionsChanged());
	}

	SECTION("A default built plan keeps nothing")
	{
		const EnclosureTransferPlan plan;

		CHECK_FALSE(plan.isUsable());
		CHECK(plan.entries.isEmpty());
		CHECK_FALSE(plan.hasLoss());

			// and a default built entry has been told nothing,
			// which must not read as permission to keep the item
		const TransferredItem entry;
		CHECK_FALSE(entry.isTransferred());
	}
}

TEST_CASE("An empty enclosure is still worth a word about its dimensions",
	  "[location][enclosure]")
{
	const QList<MountedItem> nothing_mounted;

	SECTION("Dimensions that change are reported with nothing mounted")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(nothing_mounted,
						MountingArea(600.0, 800.0),
						MountingArea(400.0, 600.0));

		CHECK(plan.entries.isEmpty());
		CHECK(plan.transferCount() == 0);
		CHECK(plan.lossCount() == 0);
		CHECK_FALSE(plan.hasLoss());
		CHECK(plan.lostDesignations().isEmpty());
		CHECK(plan.dimensionsChanged());
		CHECK_FALSE(plan.warning().isEmpty());
	}

	SECTION("Dimensions that do not change are reported by silence")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(nothing_mounted,
						MountingArea(600.0, 800.0),
						MountingArea(600.0, 800.0));

		CHECK_FALSE(plan.dimensionsChanged());
		CHECK(plan.warning().isEmpty());
	}
}

TEST_CASE("What was already off the plate is not charged to the new enclosure",
	  "[location][enclosure]")
{
	QList<MountedItem> items;
	items << mounted("-Q9", 700.0, 20.0, 45.0, 90.0)
	      << mounted("-Q10", 300.0, 20.0, 45.0, 90.0);

	const MountingArea old_area(600.0, 800.0);

	SECTION("The item is named, and the sentence says who lost it")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(400.0, 600.0));

		const TransferredItem hanging =
			plan.entryOf(QStringLiteral("-Q9"));
		CHECK(hanging.outside_before);
		CHECK(hanging.fit == MountingFit::OutsideArea);
		CHECK(hanging.reason().contains(QStringLiteral("-Q9")));

		const TransferredItem kept =
			plan.entryOf(QStringLiteral("-Q10"));
		CHECK_FALSE(kept.outside_before);
		CHECK(kept.isTransferred());
		CHECK(kept.reason().isEmpty());
	}

	SECTION("A bigger enclosure can rescue it, and it is still flagged")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(900.0, 1200.0));

		const TransferredItem hanging =
			plan.entryOf(QStringLiteral("-Q9"));
		CHECK(hanging.isTransferred());
		CHECK(hanging.outside_before);
		CHECK(plan.lostDesignations().isEmpty());
	}

	SECTION("With no old surface to compare, nothing is claimed about before")
	{
		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, MountingArea(),
						MountingArea(900.0, 1200.0));

		CHECK_FALSE(plan.entryOf(QStringLiteral("-Q9")).outside_before);
		CHECK(plan.transferCount() == 2);
			// the old surface is unknown, which is itself worth
			// saying rather than passing off as unchanged
		CHECK(plan.dimensionsChanged());
		CHECK_FALSE(plan.warning().isEmpty());
	}
}

TEST_CASE("An item nobody finished describing is transferred, never dropped",
	  "[location][enclosure]")
{
	const MountingArea old_area(600.0, 800.0);

	SECTION("No dimensions typed: checked as a point and flagged")
	{
		QList<MountedItem> items;
		items << mounted("-X1", 10.0, 10.0, 0.0, 0.0)
		      << mounted("-X2", 10.0, 10.0, not_a_number, 90.0);

		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(400.0, 600.0));

		CHECK(plan.entries.count() == 2);
		CHECK(plan.transferCount() == 2);
		CHECK_FALSE(plan.hasLoss());
		CHECK(plan.withUnknownSize().count() == 2);
		CHECK(plan.entryOf(QStringLiteral("-X1")).size_unknown);
		CHECK(plan.entryOf(QStringLiteral("-X2")).size_unknown);

			// kept, and the warning does not promise a fit it never
			// verified
		const QString warning = plan.warning();
		CHECK_FALSE(warning.isEmpty());
		CHECK(plan.entryOf(QStringLiteral("-X1"))
		      .reason().contains(QStringLiteral("-X1")));
	}

	SECTION("No dimensions and out of bounds: lost, and named all the same")
	{
		QList<MountedItem> items;
		items << mounted("-X3", 900.0, 10.0, 0.0, 0.0);

		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(400.0, 600.0));

		CHECK(plan.lossCount() == 1);
		CHECK(plan.lostDesignations().contains(QStringLiteral("-X3")));
			// it was never transferred, so it is not counted among
			// what was kept without checking
		CHECK(plan.withUnknownSize().isEmpty());
	}

	SECTION("No mark at all: the warning still has something to call it")
	{
		MountedItem nameless;
		nameless.position = QPointF(900.0, 900.0);
		nameless.size = QSizeF(45.0, 90.0);

		MountedItem by_part;
		by_part.part_code = QStringLiteral("XYZ-1234");
		by_part.position = QPointF(900.0, 900.0);
		by_part.size = QSizeF(45.0, 90.0);

		QList<MountedItem> items;
		items << nameless << by_part;

		const EnclosureTransferPlan plan =
			EnclosureTransfer::plan(items, old_area,
						MountingArea(400.0, 600.0));

		REQUIRE(plan.entries.count() == 2);
		CHECK(plan.lossCount() == 2);

		const QStringList lost = plan.lostDesignations();
		REQUIRE(lost.count() == 2);
			// an empty name in the list is the same as no list
		CHECK_FALSE(lost.at(0).isEmpty());
		CHECK(lost.at(1) == QStringLiteral("XYZ-1234"));
		CHECK(plan.entries.at(0).item.isNull());
		CHECK_FALSE(plan.entries.at(0).reason().isEmpty());
	}
}

TEST_CASE("The warning caps its enumeration and the plan never does",
	  "[location][enclosure]")
{
	QList<MountedItem> items;
	for (int i = 1; i <= 12; ++i)
	{
		const QString label = QStringLiteral("-Q%1")
				      .arg(i, 2, 10, QChar('0'));
		MountedItem item(label,
				 QPointF(200.0 + i, 200.0),
				 QSizeF(45.0, 90.0));
		item.uuid = label;
		items << item;
	}

	const EnclosureTransferPlan plan =
		EnclosureTransfer::plan(items,
					MountingArea(600.0, 800.0),
					MountingArea(100.0, 100.0));

	CHECK(plan.lossCount() == 12);

		// the list is the acceptance criterion, and it is complete
	const QStringList lost = plan.lostDesignations();
	CHECK(lost.count() == 12);
	CHECK(lost.first() == QStringLiteral("-Q01"));
	CHECK(lost.last() == QStringLiteral("-Q12"));

		// the sentence is a message box, and it is not
	const QString warning = plan.warning();
	CHECK(namedLines(warning, QStringLiteral("-Q"))
	      == EnclosureTransferPlan::warningItemLimit());
	CHECK(warning.contains(QStringLiteral("-Q01")));
	CHECK_FALSE(warning.contains(QStringLiteral("-Q12")));
		// and it says there is more, so nobody reads six as all
	CHECK(warning.count(QChar('\n'))
	      > EnclosureTransferPlan::warningItemLimit());
}
