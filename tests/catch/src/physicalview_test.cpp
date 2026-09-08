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
#include "../../../sources/catalog/physicalview.h"
#include "../../../sources/location/enclosuretransfer.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QList>
#include <QPointF>
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

	/// @return the properties a class declares, by key
	QHash<QString, CatalogProperty> declarationsOf(const Catalog &catalog,
						       int class_id)
	{
		QHash<QString, CatalogProperty> declarations;
		const QList<CatalogProperty> declared =
				catalog.effectiveProperties(class_id);
		for (const CatalogProperty &property : declared) {
			declarations.insert(property.key, property);
		}
		return declarations;
	}

	/**
		@brief bodyOf
		@param catalog a catalog holding the class tree
		@param values what somebody typed into the part
		@return the physical view those values describe

		Read through the declarations of the contactor class - a
		subclass of the one the keys are declared on, so that every read
		here goes through the inheritance rather than around it - and
		not through a table typed in this file: the conversion under
		test is the catalogue's own, and a fake declaration would prove
		the fake.
	*/
	CatalogPhysicalView bodyOf(const Catalog &catalog,
				   const QHash<QString, QString> &values)
	{
		return CatalogPhysicalView::read(
				values,
				declarationsOf(catalog, classId(catalog, "contactor")));
	}

	/// @return a map holding one cell
	QHash<QString, QString> cell(const char *key, const char *value)
	{
		QHash<QString, QString> values;
		values.insert(QString::fromUtf8(key), QString::fromUtf8(value));
		return values;
	}
}

TEST_CASE("T20 — the ten keys the physical view reads are keys the catalogue declares",
	  "[catalog][physicalview]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int component = classId(catalog, "component");

		//The failure this catches is silent by construction: a key
		//renamed on the seeding end alone would make every part read
		//as unmeasured, which every caller tolerates on purpose, so
		//nothing would break loudly and every plate would go quiet.
	const QStringList keys = CatalogPhysicalView::keys();
	CHECK(keys.count() == 10);
	for (const QString &key : keys)
	{
		INFO("clé " << key.toStdString());
		CHECK_FALSE(catalog.effectiveProperty(component, key).isNull());
		CHECK(Catalog::seededComponentPropertyKeys().contains(key));
	}
}

TEST_CASE("T20 — the two files that fold a length agree on where a length stops",
	  "[catalog][physicalview]")
{
		//Two symbols and one number, because the catalogue core is
		//linked without the layout and cannot include its header. That
		//they agree is the thing this case exists to keep true: a
		//catalogue calling a part measured and a plate calling the same
		//part unmeasured is the kind of disagreement nobody would think
		//to look for.
	CHECK(CatalogPhysicalView::tolerance() == MountingArea::tolerance());
}

TEST_CASE("T20 — a part has a physical view when its width and its height are lengths",
	  "[catalog][physicalview]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	SECTION("both measured: the part can be drawn as a rectangle")
	{
		QHash<QString, QString> values;
		values.insert(QStringLiteral("width"), QStringLiteral("45"));
		values.insert(QStringLiteral("height"), QStringLiteral("85"));

		const CatalogPhysicalView body = bodyOf(catalog, values);
		CHECK(body.hasWidth());
		CHECK(body.hasHeight());
		CHECK(body.hasPhysicalView());
		CHECK(body.width.value == Approx(45.0));
		CHECK(body.height.value == Approx(85.0));
	}

	SECTION("nothing filled in is nothing invented")
	{
		const CatalogPhysicalView body =
				bodyOf(catalog, QHash<QString, QString>());
		CHECK_FALSE(body.hasWidth());
		CHECK_FALSE(body.hasHeight());
		CHECK_FALSE(body.hasPhysicalView());
		CHECK_FALSE(body.width.isDeclared());
	}

	SECTION("half a measurement is not half a view")
	{
		const CatalogPhysicalView body = bodyOf(catalog, cell("width", "45"));
		CHECK(body.hasWidth());
		CHECK_FALSE(body.hasHeight());
		CHECK_FALSE(body.hasPhysicalView());
	}

	SECTION("a length is refused for being negative, not for being absent")
	{
		QHash<QString, QString> values;
		values.insert(QStringLiteral("width"), QStringLiteral("-45"));
		values.insert(QStringLiteral("height"), QStringLiteral("85"));

		const CatalogPhysicalView body = bodyOf(catalog, values);
		CHECK_FALSE(body.hasPhysicalView());
			//Declared and not a length: the record exists and is
			//wrong, which is a different sentence from "nobody
			//measured this" and has to stay sayable.
		CHECK(body.width.isDeclared());
		CHECK_FALSE(body.width.isLength());
		CHECK(body.width.value == Approx(-45.0));
	}
}

TEST_CASE("T20 — zero is a measure somebody wrote, and absence is not zero",
	  "[catalog][physicalview]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

		//The whole point of reading through CatalogProperty::toVariant
		//rather than through a bare toDouble: an empty cell answers
		//with no number at all, and a cell holding "0" answers with a
		//number that happens to be zero. Both faces are checked, and
		//the two fields that read them differently are checked side by
		//side, because it is the difference that is the rule.
	SECTION("an empty cell is not a number")
	{
		const CatalogPhysicalView body = bodyOf(catalog, cell("width", ""));
		CHECK_FALSE(body.width.isDeclared());
		CHECK(body.width.value == Approx(0.0));
	}

	SECTION("a cell holding zero is a number, and the view says so")
	{
		const CatalogPhysicalView body = bodyOf(catalog, cell("width", "0"));
		CHECK(body.width.isDeclared());
			//And still not a width: no part is 0 mm wide, so the
			//drawing folds the two together even though the reading
			//keeps them apart.
		CHECK_FALSE(body.width.isLength());
		CHECK_FALSE(body.hasPhysicalView());
	}

	SECTION("a clearance of zero is a statement, a clearance nobody typed is not")
	{
		const CatalogPhysicalView measured =
				bodyOf(catalog, cell("clearance_top", "0"));
		CHECK(measured.hasClearance());
		CHECK(measured.clearance_top.isDeclared());

		const CatalogPhysicalView unmeasured =
				bodyOf(catalog, QHash<QString, QString>());
		CHECK_FALSE(unmeasured.hasClearance());
	}

	SECTION("a stray space is still a number, a comma is not")
	{
		const CatalogPhysicalView spaced =
				bodyOf(catalog, cell("width", " 45 "));
		CHECK(spaced.hasWidth());
		CHECK(spaced.width.value == Approx(45.0));

			//1,250 is a thousand two hundred and fifty to one
			//office and one and a quarter to another, so it is
			//reported as unmeasured rather than guessed at.
		const CatalogPhysicalView comma =
				bodyOf(catalog, cell("width", "1,250"));
		CHECK_FALSE(comma.width.isDeclared());
	}
}

TEST_CASE("T20 — the depth is asked apart, and answered apart",
	  "[catalog][physicalview]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	SECTION("a part drawn on a plate is not thereby a part drawn on a side view")
	{
		QHash<QString, QString> flat;
		flat.insert(QStringLiteral("width"), QStringLiteral("45"));
		flat.insert(QStringLiteral("height"), QStringLiteral("85"));

		const CatalogPhysicalView body = bodyOf(catalog, flat);
		CHECK(body.hasPhysicalView());
			//The measure a manufacturer omits most often, which is
			//why a side view drawn from what is there looks
			//complete and is not.
		CHECK_FALSE(body.hasDepth());
	}

	SECTION("a depth on its own is a depth, and still no physical view")
	{
		const CatalogPhysicalView body = bodyOf(catalog, cell("depth", "70"));
		CHECK(body.hasDepth());
		CHECK(body.depth.value == Approx(70.0));
		CHECK_FALSE(body.hasPhysicalView());
	}

	SECTION("all three measured")
	{
		QHash<QString, QString> values;
		values.insert(QStringLiteral("width"), QStringLiteral("45"));
		values.insert(QStringLiteral("height"), QStringLiteral("85"));
		values.insert(QStringLiteral("depth"), QStringLiteral("70"));

		const CatalogPhysicalView body = bodyOf(catalog, values);
		CHECK(body.hasPhysicalView());
		CHECK(body.hasDepth());
	}
}

TEST_CASE("T20 — the insertion point is a pair, and either half alone is refused",
	  "[catalog][physicalview]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	SECTION("both offsets: the axis is where the part says, y running down")
	{
		QHash<QString, QString> values;
		values.insert(QStringLiteral("insertion_x"), QStringLiteral("22.5"));
		values.insert(QStringLiteral("insertion_y"), QStringLiteral("30"));

		const CatalogPhysicalView body = bodyOf(catalog, values);
		CHECK(body.hasInsertionPoint());
		CHECK_FALSE(body.hasHalfInsertionPoint());
		CHECK(body.insertionOffset().x() == Approx(22.5));
		CHECK(body.insertionOffset().y() == Approx(30.0));
	}

	SECTION("neither offset: no axis was declared, and none is invented")
	{
		const CatalogPhysicalView body =
				bodyOf(catalog, QHash<QString, QString>());
		CHECK_FALSE(body.hasInsertionPoint());
		CHECK_FALSE(body.hasHalfInsertionPoint());
		CHECK(body.insertionOffset() == QPointF(0.0, 0.0));
	}

		//Each half on its own, and not one of the two: the pair is
		//symmetric and a rule written for one of them would pass this
		//suite while letting the other through.
	SECTION("x alone is a record somebody did not finish")
	{
		const CatalogPhysicalView body =
				bodyOf(catalog, cell("insertion_x", "22.5"));
		CHECK_FALSE(body.hasInsertionPoint());
		CHECK(body.hasHalfInsertionPoint());
			//Reported and not completed: the missing half would
			//have to be invented, and an invented axis clips a
			//whole row of a rail at the wrong height.
		CHECK(body.insertionOffset() == QPointF(0.0, 0.0));
	}

	SECTION("y alone is refused the same way")
	{
		const CatalogPhysicalView body =
				bodyOf(catalog, cell("insertion_y", "30"));
		CHECK_FALSE(body.hasInsertionPoint());
		CHECK(body.hasHalfInsertionPoint());
		CHECK(body.insertionOffset() == QPointF(0.0, 0.0));
	}

	SECTION("an offset of zero is a declaration, not an absence")
	{
		QHash<QString, QString> corner;
		corner.insert(QStringLiteral("insertion_x"), QStringLiteral("0"));
		corner.insert(QStringLiteral("insertion_y"), QStringLiteral("0"));

			//This is the one field where zero and empty must not be
			//folded together: the top left corner of the body is a
			//place somebody may have chosen on purpose.
		const CatalogPhysicalView body = bodyOf(catalog, corner);
		CHECK(body.hasInsertionPoint());
		CHECK_FALSE(body.hasHalfInsertionPoint());
		CHECK(body.insertionOffset() == QPointF(0.0, 0.0));
	}

	SECTION("a negative offset is an axis above the outline, and is kept")
	{
		QHash<QString, QString> above;
		above.insert(QStringLiteral("insertion_x"), QStringLiteral("22.5"));
		above.insert(QStringLiteral("insertion_y"), QStringLiteral("-12"));

		const CatalogPhysicalView body = bodyOf(catalog, above);
		CHECK(body.hasInsertionPoint());
		CHECK(body.insertionOffset().y() == Approx(-12.0));
	}

	SECTION("half a pair is refused even when the part is fully measured")
	{
		QHash<QString, QString> values;
		values.insert(QStringLiteral("width"), QStringLiteral("45"));
		values.insert(QStringLiteral("height"), QStringLiteral("85"));
		values.insert(QStringLiteral("insertion_x"), QStringLiteral("22.5"));

		const CatalogPhysicalView body = bodyOf(catalog, values);
		CHECK(body.hasPhysicalView());
		CHECK(body.hasHalfInsertionPoint());
		CHECK_FALSE(body.hasInsertionPoint());
	}
}

TEST_CASE("T20 — the outline is drawn unless the catalogue says otherwise",
	  "[catalog][physicalview]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	SECTION("a cell that says nothing usable leaves the outline on")
	{
		CHECK(bodyOf(catalog, QHash<QString, QString>()).draw_outline);
		CHECK(bodyOf(catalog, cell("draw_outline", "")).draw_outline);
		CHECK(bodyOf(catalog, cell("draw_outline", "peut-être")).draw_outline);
	}

	SECTION("the three spellings the catalogue writes")
	{
		CHECK(bodyOf(catalog, cell("draw_outline", "1")).draw_outline);
		CHECK(bodyOf(catalog, cell("draw_outline", "true")).draw_outline);
		CHECK_FALSE(bodyOf(catalog, cell("draw_outline", "0")).draw_outline);
		CHECK_FALSE(bodyOf(catalog, cell("draw_outline", "false")).draw_outline);
		CHECK_FALSE(bodyOf(catalog, cell("draw_outline", "FALSE")).draw_outline);
	}
}

TEST_CASE("T20 — a part read out of the catalogue reads the same as its own values",
	  "[catalog][physicalview]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	CatalogPart part(QStringLiteral("CONT-45"), classId(catalog, "contactor"));
	part.setValue(QStringLiteral("width"), QStringLiteral("45"));
	part.setValue(QStringLiteral("height"), QStringLiteral("85"));
	part.setValue(QStringLiteral("insertion_x"), QStringLiteral("22.5"));
	part.setValue(QStringLiteral("insertion_y"), QStringLiteral("42.5"));

	QString error;
	REQUIRE(catalog.savePart(part, &error));
	REQUIRE(error.isEmpty());

	const CatalogPart saved = catalog.partByCode(QStringLiteral("CONT-45"));
	REQUIRE_FALSE(saved.isNull());

		//The road the dialogue and the report both take: the map of
		//effective values, which is the initial value of every property
		//the class declares with what the part actually holds written
		//over it.
	const CatalogPhysicalView body =
			CatalogPhysicalView::read(catalog.effectiveValues(saved),
						  declarationsOf(catalog, saved.class_id));
	CHECK(body.hasPhysicalView());
	CHECK(body.hasInsertionPoint());
	CHECK(body.insertionOffset().x() == Approx(22.5));

		//And the same answer from nothing but the map, which is what a
		//caller holding a package or a file has. The overload exists so
		//that such a caller does not write a second reading of its own;
		//it has to give the same answer or it would be that second
		//reading.
	const CatalogPhysicalView bare =
			CatalogPhysicalView::read(catalog.effectiveValues(saved));
	CHECK(bare.hasPhysicalView());
	CHECK(bare.hasInsertionPoint());
	CHECK(bare.width.value == Approx(body.width.value));
	CHECK(bare.insertionOffset() == body.insertionOffset());
}
