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
#include "../../../sources/catalog/catalog.h"
#include "../../../sources/location/mountingpartview.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace
{
	/// A catalog in memory, seeded with the default class tree.
	class OpenCatalog
	{
		public:
			OpenCatalog()
			{
				QString error;
				const bool opened = catalog.openInMemory(&error);
				REQUIRE(error.isEmpty());
				REQUIRE(opened);
			}

			Catalog catalog;
	};

	/// @return the identifier of the class @a key, which has to exist
	int classId(const Catalog &catalog, const char *key)
	{
		const CatalogClass found =
				catalog.classByKey(QString::fromUtf8(key));
		REQUIRE_FALSE(found.isNull());
		return found.id;
	}

	/**
		@brief savePart
		Save one part of the contactor class - a subclass of the class
		the physical view keys are declared on, so that every read here
		goes through the inheritance rather than around it.
	*/
	void savePart(Catalog &catalog,
		      const char *code,
		      const QHash<QString, QString> &values)
	{
		CatalogPart part(QString::fromUtf8(code),
				 classId(catalog, "contactor"));
		for (auto value = values.constBegin();
		     value != values.constEnd();
		     ++value) {
			part.setValue(value.key(), value.value());
		}

		QString error;
		const bool saved = catalog.savePart(part, &error);
		REQUIRE(error.isEmpty());
		REQUIRE(saved);
	}

	/// Write one value into a part that is already in the catalog.
	void setPartValue(Catalog &catalog,
			  const char *code,
			  const QString &key,
			  const QString &value)
	{
		CatalogPart part = catalog.partByCode(QString::fromUtf8(code));
		REQUIRE_FALSE(part.isNull());

		if (value.isNull()) {
			part.clearValue(key);
		} else {
			part.setValue(key, value);
		}

		QString error;
		const bool saved = catalog.savePart(part, &error);
		REQUIRE(error.isEmpty());
		REQUIRE(saved);
	}

	/// @return one part screwed to a face, millimetre
	MountedItem mounted(const char *uuid,
			    const char *code,
			    qreal x, qreal y,
			    const QSizeF &size = QSizeF())
	{
		MountedItem item;
		item.uuid      = QString::fromUtf8(uuid);
		item.part_code = QString::fromUtf8(code);
		item.position  = QPointF(x, y);
		item.size      = size;
		return item;
	}

	/// @return the mounting plate of a location, measured in millimetre
	MountingSurface plate(qreal width, qreal height)
	{
		MountingSurface surface(QStringLiteral("QCM1"),
					QStringLiteral("plate"),
					MountingArea(width, height));
		surface.uuid = QStringLiteral("plate-1");
		return surface;
	}
}

TEST_CASE("T19 — the ten keys the layout reads are the ten the catalogue seeds",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int component = classId(catalog, "component");

		//The one failure this catches is silent by construction: a key
		//renamed on the seeding end alone would make every part read as
		//unmeasured, which the whole family tolerates on purpose, so
		//nothing would break loudly and every plate would go quiet.
	const QStringList keys = MountingPartReader::physicalViewKeys();
	CHECK(keys.count() == 10);
	for (const QString &key : keys)
	{
		INFO("clé " << key.toStdString());
		CHECK_FALSE(catalog.effectiveProperty(component, key).isNull());
	}
}

TEST_CASE("T19 — the box of a part is born from its product code, and follows it",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	QHash<QString, QString> narrow;
	narrow.insert(QStringLiteral("width"), QStringLiteral("45"));
	narrow.insert(QStringLiteral("height"), QStringLiteral("85"));
	narrow.insert(QStringLiteral("depth"), QStringLiteral("70"));
	savePart(catalog, "CONT-45", narrow);

	QHash<QString, QString> wide;
	wide.insert(QStringLiteral("width"), QStringLiteral("70"));
	wide.insert(QStringLiteral("height"), QStringLiteral("85"));
	savePart(catalog, "CONT-70", wide);

	SECTION("the catalogue is what says how big it is")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("CONT-45"));
		CHECK(view.isKnownPart());
		CHECK(view.hasSize());
		CHECK(view.hasDepth());
		CHECK(view.width == Approx(45.0));
		CHECK(view.height == Approx(85.0));
		CHECK(view.depth == Approx(70.0));
		CHECK(view.size().width() == Approx(45.0));
		CHECK(view.footprintAt(QPointF(100, 200)).right() == Approx(145.0));
	}

	SECTION("an item enters the plate with the size the catalogue holds")
	{
		const MountedItem item =
				MountingPartReader::mountedItemFor(catalog,
								   QStringLiteral("CONT-45"),
								   QPointF(100, 200),
								   QStringLiteral("-KM1"));
		CHECK(item.part_code == QStringLiteral("CONT-45"));
		CHECK(item.label == QStringLiteral("-KM1"));
		CHECK(item.position.x() == Approx(100.0));
		CHECK(item.hasDeclaredSize());
		CHECK(item.size.width() == Approx(45.0));
		CHECK(item.size.height() == Approx(85.0));
			//Identity is the layout's to give, not the catalogue's.
		CHECK(item.uuid.isEmpty());
	}

	SECTION("changing the product code changes the box, with nobody typing a number")
	{
		MountedItem item =
				MountingPartReader::mountedItemFor(catalog,
								   QStringLiteral("CONT-45"),
								   QPointF(0, 0));
		REQUIRE(item.size.width() == Approx(45.0));

		item.part_code = QStringLiteral("CONT-70");
		CHECK(MountingPartReader::applyTo(catalog, item));
		CHECK(item.size.width() == Approx(70.0));

			//And reading it again changes nothing, which is what
			//lets a refresh run on every open without dirtying the
			//project.
		CHECK_FALSE(MountingPartReader::applyTo(catalog, item));
	}

	SECTION("correcting the product record reaches the plate that carries it")
	{
		MountedItem item =
				MountingPartReader::mountedItemFor(catalog,
								   QStringLiteral("CONT-45"),
								   QPointF(0, 0));
		setPartValue(catalog, "CONT-45",
			     QStringLiteral("width"), QStringLiteral("47.5"));

		CHECK(MountingPartReader::applyTo(catalog, item));
		CHECK(item.size.width() == Approx(47.5));
	}
}

TEST_CASE("T19 — a part nobody measured is named, never given a size",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	savePart(catalog, "SANS-MESURE", QHash<QString, QString>());

	QHash<QString, QString> half;
	half.insert(QStringLiteral("width"), QStringLiteral("45"));
	savePart(catalog, "DEMI-MESURE", half);

	QHash<QString, QString> written_zero;
	written_zero.insert(QStringLiteral("width"), QStringLiteral("0"));
	written_zero.insert(QStringLiteral("height"), QStringLiteral("0"));
	savePart(catalog, "ZERO", written_zero);

	SECTION("nothing filled in is nothing invented")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("SANS-MESURE"));
		CHECK(view.isKnownPart());
		CHECK_FALSE(view.hasWidth());
		CHECK_FALSE(view.hasHeight());
		CHECK_FALSE(view.hasSize());
		CHECK_FALSE(view.hasDepth());
			//Zero, and not a box of twenty units: an item checked as
			//a point is reported, an item given an invented box
			//passes the check and fails on the bench.
		CHECK(view.size().width() == Approx(0.0));
		CHECK(view.size().height() == Approx(0.0));
	}

	SECTION("half a measurement is not half a box")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("DEMI-MESURE"));
		CHECK(view.hasWidth());
		CHECK_FALSE(view.hasHeight());
		CHECK_FALSE(view.hasSize());
		CHECK(view.width == Approx(45.0));
	}

	SECTION("a dimension written as zero is still nothing: no part is 0 mm wide")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("ZERO"));
		CHECK(view.isKnownPart());
		CHECK_FALSE(view.hasSize());
	}

	SECTION("the unmeasured part stays on the plate and is counted, never dropped")
	{
		MountingSurface surface = plate(600, 600);
		surface.items << mounted("Q1", "SANS-MESURE", 10, 10)
			      << mounted("Q2", "DEMI-MESURE", 10, 200);

		const MountedItem item =
				MountingPartReader::mountedItemFor(catalog,
								   QStringLiteral("SANS-MESURE"),
								   QPointF(10, 10));
		CHECK_FALSE(item.hasDeclaredSize());
			//Never empty, so the warning that names it names
			//something.
		CHECK_FALSE(item.designation().isEmpty());

		const MountingSurfaceReport report =
				MountingPartReader::checkSurface(catalog, surface);
		CHECK(report.itemCount() == 2);
		CHECK(report.unknown_size_count == 2);
		CHECK_FALSE(report.isConclusive());
	}
}

TEST_CASE("T19 — the clearance table the check asks for is filled from the catalogue",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

		//A drive that wants 100 mm of air all round, and a duct that
		//wants none.
	QHash<QString, QString> drive;
	drive.insert(QStringLiteral("width"), QStringLiteral("200"));
	drive.insert(QStringLiteral("height"), QStringLiteral("300"));
	drive.insert(QStringLiteral("clearance_top"), QStringLiteral("100"));
	drive.insert(QStringLiteral("clearance_bottom"), QStringLiteral("100"));
	drive.insert(QStringLiteral("clearance_left"), QStringLiteral("100"));
	drive.insert(QStringLiteral("clearance_right"), QStringLiteral("100"));
	savePart(catalog, "VARIATEUR", drive);

	QHash<QString, QString> duct;
	duct.insert(QStringLiteral("width"), QStringLiteral("600"));
	duct.insert(QStringLiteral("height"), QStringLiteral("60"));
	savePart(catalog, "GOULOTTE", duct);

		//The duct runs 30 mm above the drive: bottom edge at 70, drive
		//top edge at 100.
	MountingSurface surface = plate(600, 600);
	surface.items << mounted("U1", "VARIATEUR", 100, 100, QSizeF(200, 300))
		      << mounted("W1", "GOULOTTE", 0, 10, QSizeF(600, 60));

	SECTION("the table holds what was measured, and only that")
	{
		const QHash<QString, MountingClearance> table =
				MountingPartReader::clearancesFor(catalog, surface);
		REQUIRE(table.count() == 1);
		REQUIRE(table.contains(QStringLiteral("VARIATEUR")));
		CHECK(table.value(QStringLiteral("VARIATEUR")).top == Approx(100.0));
		CHECK(table.value(QStringLiteral("VARIATEUR")).left == Approx(100.0));
			//The duct asks for nothing, so it is absent rather than
			//present asking for zero: the two are one answer to the
			//check, and the smaller table is the honest one.
		CHECK_FALSE(table.contains(QStringLiteral("GOULOTTE")));
	}

	SECTION("30 mm from a part that wanted 100 is 70 mm short, not 60")
	{
		const MountingSurfaceReport report =
				MountingPartReader::checkSurface(catalog, surface);

		REQUIRE(report.encroachments.count() == 1);
		const MountingEncroachment starved = report.encroachments.first();
		CHECK(starved.claimant_uuid == QStringLiteral("U1"));
		CHECK(starved.intruder_uuid == QStringLiteral("W1"));
			//The whole body of the duct is inside the claimed room
			//along y, so the shared rectangle is 60 mm tall - and 60
			//is the answer that would leave the drive short after
			//the duct had been dragged exactly as far as the panel
			//said.
		CHECK(starved.missing == Approx(70.0));
		CHECK(starved.shared.height() == Approx(60.0));

		CHECK_FALSE(report.isClean());
		CHECK(report.overlaps.isEmpty());
		CHECK(report.complainedAbout().contains(QStringLiteral("U1")));
	}

	SECTION("the same plate, checked without the catalogue, comes back clean")
	{
			//This is the wiring, stated as a number. The rule is the
			//same rule; what changes is whether anybody handed it the
			//clearances. Left to itself it finds no requirement,
			//reports no violation, and says so through isConclusive -
			//which is exactly the answer a panel must not show alone.
		const MountingSurfaceReport unwired =
				MountingCheck::surfaceReport(surface);
		CHECK(unwired.encroachments.isEmpty());
		CHECK(unwired.isClean());
		CHECK_FALSE(unwired.isConclusive());
		CHECK(unwired.unknown_clearance_count == 2);
	}
}

TEST_CASE("T19 — a clearance nobody typed asks for nothing, and one typed as zero says so",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	QHash<QString, QString> silent;
	silent.insert(QStringLiteral("width"), QStringLiteral("22.5"));
	silent.insert(QStringLiteral("height"), QStringLiteral("80"));
	savePart(catalog, "MUET", silent);

	QHash<QString, QString> flush;
	flush.insert(QStringLiteral("width"), QStringLiteral("22.5"));
	flush.insert(QStringLiteral("height"), QStringLiteral("80"));
	flush.insert(QStringLiteral("clearance_left"), QStringLiteral("0"));
	flush.insert(QStringLiteral("clearance_right"), QStringLiteral("0"));
	savePart(catalog, "ACCOLE", flush);

	QHash<QString, QString> negative;
	negative.insert(QStringLiteral("width"), QStringLiteral("22.5"));
	negative.insert(QStringLiteral("height"), QStringLiteral("80"));
	negative.insert(QStringLiteral("clearance_top"), QStringLiteral("-10"));
	savePart(catalog, "NEGATIF", negative);

	SECTION("nobody typed anything, so nothing is asked and nothing is claimed")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("MUET"));
		CHECK_FALSE(view.hasClearance());
		CHECK(view.clearance.isEmpty());
	}

	SECTION("somebody typed zero, which is a statement about the part")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("ACCOLE"));
			//The declaration survives the boundary...
		CHECK(view.hasClearance());
			//...and the numbers cannot hold it, which is a limit and
			//not an oversight: MountingClearance folds zero and
			//unmeasured into one state on purpose, so the check
			//counts this part among those verified against nothing.
			//Whoever writes the sentence the panel shows has the
			//flag above to tell the two apart.
		CHECK(view.clearance.isEmpty());

		MountingSurface surface = plate(600, 600);
		surface.items << mounted("Q1", "ACCOLE", 0, 0, QSizeF(22.5, 80));

		const MountingSurfaceReport report =
				MountingPartReader::checkSurface(catalog, surface);
		CHECK(report.unknown_clearance_count == 1);
		CHECK_FALSE(report.isConclusive());
	}

	SECTION("a negative clearance is folded away, never used to shrink a body")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("NEGATIF"));
		CHECK(view.hasClearance());
		CHECK(view.clearance.top == Approx(0.0));
		CHECK(view.clearance.isEmpty());
	}
}

TEST_CASE("T19 — a plate whose parts are all measured comes back conclusive",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

		//Until something read the catalogue, the second question was
		//false on every plate there could ever be, because no clearance
		//had a reader. This is the case that makes it able to be true.
	QHash<QString, QString> breaker;
	breaker.insert(QStringLiteral("width"), QStringLiteral("22.5"));
	breaker.insert(QStringLiteral("height"), QStringLiteral("80"));
	breaker.insert(QStringLiteral("clearance_top"), QStringLiteral("5"));
	savePart(catalog, "DISJ-22", breaker);

	MountingSurface surface = plate(600, 600);
	surface.items << mounted("Q1", "DISJ-22", 0, 100, QSizeF(22.5, 80))
		      << mounted("Q2", "DISJ-22", 22.5, 100, QSizeF(22.5, 80));

	const MountingSurfaceReport report =
			MountingPartReader::checkSurface(catalog, surface);

	CHECK(report.itemCount() == 2);
	CHECK(report.overlaps.isEmpty());
	CHECK(report.encroachments.isEmpty());
	CHECK(report.unknown_size_count == 0);
	CHECK(report.unknown_clearance_count == 0);
	CHECK(report.isClean());
	CHECK(report.isConclusive());
}

TEST_CASE("T19 — the axis of a part is a pair, and half of it is not an answer",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	QHash<QString, QString> full;
	full.insert(QStringLiteral("width"), QStringLiteral("45"));
	full.insert(QStringLiteral("height"), QStringLiteral("85"));
	full.insert(QStringLiteral("insertion_x"), QStringLiteral("22.5"));
	full.insert(QStringLiteral("insertion_y"), QStringLiteral("30"));
	savePart(catalog, "AXE-COMPLET", full);

	QHash<QString, QString> half;
	half.insert(QStringLiteral("insertion_x"), QStringLiteral("22.5"));
	savePart(catalog, "AXE-DEMI", half);

	QHash<QString, QString> corner;
	corner.insert(QStringLiteral("insertion_x"), QStringLiteral("0"));
	corner.insert(QStringLiteral("insertion_y"), QStringLiteral("0"));
	savePart(catalog, "AXE-COIN", corner);

	SECTION("both offsets: the axis is where the part says, y running down")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("AXE-COMPLET"));
		CHECK(view.hasInsertion());
		CHECK_FALSE(view.hasHalfInsertion());
		CHECK(view.insertionOffset().x() == Approx(22.5));
		CHECK(view.insertionOffset().y() == Approx(30.0));
		CHECK(view.insertionAt(QPointF(100, 200)).y() == Approx(230.0));
	}

	SECTION("one offset alone is a record somebody did not finish")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("AXE-DEMI"));
		CHECK_FALSE(view.hasInsertion());
		CHECK(view.hasHalfInsertion());
			//Reported, and not completed: the missing half would
			//have to be invented, and an invented axis puts a whole
			//row of a rail out of line.
		CHECK(view.insertionOffset().x() == Approx(0.0));
	}

	SECTION("an offset of zero is a declaration, not an absence")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("AXE-COIN"));
			//This is the one place where zero and empty must not be
			//folded together: the top left corner of the body is a
			//place somebody may have chosen on purpose.
		CHECK(view.hasInsertion());
		CHECK_FALSE(view.hasHalfInsertion());
		CHECK(view.insertionOffset().x() == Approx(0.0));
		CHECK(view.insertionOffset().y() == Approx(0.0));
	}
}

TEST_CASE("T19 — the outline flag arrives, and a part nobody knows still draws a box",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	savePart(catalog, "CONTOUR", QHash<QString, QString>());

	SECTION("the initial value of the flag is what a part with no picture needs")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("CONTOUR"));
		CHECK(view.draw_outline);
	}

	SECTION("turned off, it arrives turned off")
	{
		setPartValue(catalog, "CONTOUR",
			     QStringLiteral("draw_outline"), QStringLiteral("0"));

		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("CONTOUR"));
		CHECK_FALSE(view.draw_outline);
	}

	SECTION("a code the catalogue does not hold is not a part that vanishes")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("JAMAIS-VU"));
		CHECK_FALSE(view.isKnownPart());
		CHECK_FALSE(view.isNull());
			//It names itself, so the sentence saying it was not
			//found can name it.
		CHECK(view.part_code == QStringLiteral("JAMAIS-VU"));
		CHECK_FALSE(view.hasSize());
		CHECK(view.draw_outline);
	}

	SECTION("no code at all is no question asked")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog, QString());
		CHECK(view.isNull());
		CHECK_FALSE(view.isKnownPart());
	}
}

TEST_CASE("T19 — a catalogue that does not answer never erases what the project knows",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	QHash<QString, QString> known;
	known.insert(QStringLiteral("width"), QStringLiteral("45"));
	known.insert(QStringLiteral("height"), QStringLiteral("85"));
	savePart(catalog, "CONNU", known);

	SECTION("a catalogue that was never opened changes nothing at all")
	{
			//The share the office keeps the catalogue on can be
			//down, and a layout opened away from the office must not
			//come back with every box blanked.
		const Catalog closed;
		CHECK_FALSE(closed.isOpen());

		MountedItem item = mounted("Q1", "CONNU", 0, 0, QSizeF(45, 85));
		CHECK_FALSE(MountingPartReader::applyTo(closed, item));
		CHECK(item.size.width() == Approx(45.0));
		CHECK_FALSE(MountingPartReader::viewOf(closed,
						       QStringLiteral("CONNU"))
			    .isKnownPart());
	}

	SECTION("a code the catalogue does not hold leaves the stored size alone")
	{
		MountedItem item = mounted("Q1", "INCONNU", 0, 0, QSizeF(45, 85));
		CHECK_FALSE(MountingPartReader::applyTo(catalog, item));
		CHECK(item.size.width() == Approx(45.0));
	}

	SECTION("an item with no product code is not a catalogue part")
	{
			//A rail is cut to length on the bench and carries no
			//code. Its length is the person's, and a refresh that
			//resized every rail to nothing would destroy the
			//drawing.
		MountedItem rail = mounted("R1", "", 0, 300, QSizeF(560, 35));
		CHECK_FALSE(MountingPartReader::applyTo(catalog, rail));
		CHECK(rail.size.width() == Approx(560.0));
	}

	SECTION("a part the catalogue holds and says nothing about does blank the box")
	{
			//This is not the share being down: it is the office
			//having removed a width it no longer trusts, and the
			//plate has to say "not measured" rather than go on
			//showing a number nobody stands behind.
		MountedItem item = mounted("Q1", "CONNU", 0, 0, QSizeF(45, 85));
		setPartValue(catalog, "CONNU", QStringLiteral("width"), QString());

		CHECK(MountingPartReader::applyTo(catalog, item));
		CHECK_FALSE(item.hasDeclaredSize());
	}
}

TEST_CASE("T19 — reading the catalogue is tolerant, and what it cannot read it reports",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	QHash<QString, QString> spaced;
	spaced.insert(QStringLiteral("width"), QStringLiteral(" 45 "));
	spaced.insert(QStringLiteral("height"), QStringLiteral("85"));
	savePart(catalog, "ESPACE", spaced);

	QHash<QString, QString> comma;
	comma.insert(QStringLiteral("width"), QStringLiteral("45,5"));
	comma.insert(QStringLiteral("height"), QStringLiteral("85"));
	savePart(catalog, "VIRGULE", comma);

	QHash<QString, QString> words;
	words.insert(QStringLiteral("width"), QStringLiteral("à mesurer"));
	savePart(catalog, "MOTS", words);

	SECTION("a stray space around a number is still a number")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("ESPACE"));
		CHECK(view.hasSize());
		CHECK(view.width == Approx(45.0));
	}

	SECTION("a comma is not read as a decimal point, and the part reads as unmeasured")
	{
			//1,250 is a thousand two hundred and fifty to one office
			//and one and a quarter to another. Guessing here would
			//put a part off by a factor of a thousand on a plate,
			//so it is reported instead - which is the state the
			//whole family is built to carry.
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("VIRGULE"));
		CHECK(view.isKnownPart());
		CHECK_FALSE(view.hasWidth());
		CHECK(view.hasHeight());
		CHECK_FALSE(view.hasSize());
	}

	SECTION("a sentence where a length was expected says nothing about the length")
	{
		const MountingPartView view =
				MountingPartReader::viewOf(catalog,
							   QStringLiteral("MOTS"));
		CHECK_FALSE(view.hasWidth());
	}
}

TEST_CASE("T19 — the whole layout is refreshed face by face, and says what changed",
	  "[location][mounting][catalog]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	QHash<QString, QString> breaker;
	breaker.insert(QStringLiteral("width"), QStringLiteral("22.5"));
	breaker.insert(QStringLiteral("height"), QStringLiteral("80"));
	savePart(catalog, "DISJ", breaker);

	QHash<QString, QString> lamp;
	lamp.insert(QStringLiteral("width"), QStringLiteral("30"));
	lamp.insert(QStringLiteral("height"), QStringLiteral("30"));
	savePart(catalog, "VOYANT", lamp);

	MountingLayout layout;

	MountingSurface board(QStringLiteral("QCM1"),
			      QStringLiteral("plate"),
			      MountingArea(600, 600));
	board.uuid = QStringLiteral("plate-1");
	board.items << mounted("Q1", "DISJ", 0, 0)
		    << mounted("R1", "", 0, 300, QSizeF(560, 35));
	REQUIRE_FALSE(layout.appendSurface(board).isEmpty());

	MountingSurface door(QStringLiteral("QCM1"),
			     QStringLiteral("door"),
			     MountingArea(600, 600));
	door.uuid = QStringLiteral("door-1");
	door.items << mounted("H1", "VOYANT", 50, 50)
		   << mounted("H2", "PAS-AU-CATALOGUE", 100, 50, QSizeF(25, 25));
	REQUIRE_FALSE(layout.appendSurface(door).isEmpty());

	SECTION("the codes of the whole project are collected once each")
	{
		const QStringList codes = MountingPartReader::partCodesOf(layout);
		CHECK(codes.count() == 3);
		CHECK(codes.contains(QStringLiteral("DISJ")));
		CHECK(codes.contains(QStringLiteral("VOYANT")));
		CHECK(codes.contains(QStringLiteral("PAS-AU-CATALOGUE")));
			//The rail has no code, so it is not a code.
		CHECK_FALSE(codes.contains(QString()));
	}

	SECTION("what the catalogue does not hold is asked as its own question")
	{
		const QStringList unknown =
				MountingPartReader::unknownCodes(catalog,
								 layout.surface(QStringLiteral("door-1")));
		CHECK(unknown == QStringList{ QStringLiteral("PAS-AU-CATALOGUE") });
	}

	SECTION("both faces are read, and only what changed is named")
	{
		const QStringList changed =
				MountingPartReader::applyTo(catalog, layout);
		CHECK(changed.count() == 2);
		CHECK(changed.contains(QStringLiteral("Q1")));
		CHECK(changed.contains(QStringLiteral("H1")));
			//The rail keeps the length somebody cut it to, and the
			//unknown code keeps the size the project stored.
		CHECK_FALSE(changed.contains(QStringLiteral("R1")));
		CHECK_FALSE(changed.contains(QStringLiteral("H2")));

		CHECK(layout.item(QStringLiteral("Q1")).size.width() == Approx(22.5));
		CHECK(layout.item(QStringLiteral("H1")).size.height() == Approx(30.0));
		CHECK(layout.item(QStringLiteral("R1")).size.width() == Approx(560.0));
		CHECK(layout.item(QStringLiteral("H2")).size.width() == Approx(25.0));

			//Run again, nothing changes: a refresh on every open
			//must not make the project ask to be saved.
		CHECK(MountingPartReader::applyTo(catalog, layout).isEmpty());
	}
}
