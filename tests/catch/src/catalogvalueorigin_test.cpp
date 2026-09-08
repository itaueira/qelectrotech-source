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
#include "../../../sources/catalog/catalogvalueorigin.h"
#include "../../../sources/catalog/physicalview.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace
{
	/// A catalogue in memory, seeded with the default class tree.
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
		@brief Give the property @a key an initial value.
		@param catalog the catalogue
		@param class_id a class the property reaches
		@param key the property
		@param value what a part with nothing filled in is worth

		Through the catalogue and not by writing a declaration of its
		own: what is under test is the road the office takes, and the
		office edits the property of the class.
	*/
	void setInitialValue(Catalog &catalog,
			     int class_id,
			     const char *key,
			     const char *value)
	{
		CatalogProperty property =
				catalog.effectiveProperty(class_id,
							  QString::fromUtf8(key));
		REQUIRE_FALSE(property.isNull());
		property.default_value = QString::fromUtf8(value);

		QString error;
		REQUIRE(catalog.updateProperty(property, &error));
		REQUIRE(error.isEmpty());
	}

	/**
		@brief Add the class the cases are written about.
		@param catalog the catalogue
		@return its identifier

		A drive under Composant, declaring one measure of its own. Two
		levels and not one, because the whole question is which of them
		gets named: the clearances belong to the declaration on
		Composant, and only the door clearance belongs to the drive.
	*/
	int addDriveClass(Catalog &catalog)
	{
		CatalogClass drive(QStringLiteral("drive"),
				   QStringLiteral("Variateur"));
		drive.parent_id = classId(catalog, "component");

		QString error;
		const int drive_id = catalog.addClass(drive, &error);
		REQUIRE(drive_id > 0);
		REQUIRE(error.isEmpty());

		CatalogProperty door(QStringLiteral("clearance_door"),
				     QStringLiteral("Dégagement de porte"),
				     CatalogPropertyType::Measure);
		door.class_id      = drive_id;
		door.unit          = QStringLiteral("mm");
		door.default_value = QStringLiteral("35");
		REQUIRE(catalog.addProperty(door, &error) > 0);
		REQUIRE(error.isEmpty());

		return drive_id;
	}

	/// @return the part @a code as the catalogue holds it
	CatalogPart saved(Catalog &catalog, CatalogPart &part)
	{
		QString error;
		REQUIRE(catalog.savePart(part, &error));
		REQUIRE(error.isEmpty());

		const CatalogPart reread = catalog.partByCode(part.code);
		REQUIRE_FALSE(reread.isNull());
		return reread;
	}
}

TEST_CASE("T20 — a number typed on the part and a number inherited from a class "
	  "are two answers, and the inherited one names its class",
	  "[catalog][valueorigin]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int drive_id = addDriveClass(catalog);

		//The office measures a drive and records that it wants 100 mm
		//of air above it. The clearance is declared on Composant, so
		//that is where the number goes.
	setInitialValue(catalog, drive_id, "clearance_top", "100");

	CatalogPart part(QStringLiteral("DRIVE-1"), drive_id);
	part.setValue(QStringLiteral("width"), QStringLiteral("150"));
	const CatalogPart stored = saved(catalog, part);

	const QHash<QString, CatalogValueOrigin> origins =
			catalog.valueOrigins(stored);

	SECTION("what somebody typed on the part is the part's")
	{
		const CatalogValueOrigin width =
				origins.value(QStringLiteral("width"));
		CHECK(width.isFromPart());
		CHECK(width.holdsValue());
		CHECK(width.class_id == 0);
		CHECK(width.className().isEmpty());
		CHECK_FALSE(width.describe().isEmpty());
	}

	SECTION("what nobody typed on the part names the class that declares it")
	{
		const CatalogValueOrigin top =
				origins.value(QStringLiteral("clearance_top"));
		CHECK(top.isFromClass());
		CHECK_FALSE(top.isFromPart());
		CHECK(top.holdsValue());

			//Composant and not Variateur, and this is the assertion
			//the whole case is written for: the initial value
			//belongs to the declaration, and the declaration is
			//where somebody would go to change it. Naming the class
			//of the part would send them to a screen with nothing
			//to edit on it.
		CHECK(top.class_id == classId(catalog, "component"));
		CHECK(top.class_key == QStringLiteral("component"));
		CHECK(top.class_name == QStringLiteral("Composant"));
		CHECK(top.className() == QStringLiteral("Composant"));
		CHECK(top.describe().contains(QStringLiteral("Composant")));

			//And the number that is being explained is the one the
			//flattened map hands over: an origin that explained
			//another number would be worse than none.
		CHECK(catalog.effectiveValues(stored).value(QStringLiteral("clearance_top"))
		      == QStringLiteral("100"));
	}

	SECTION("a class declaring its own measure is the class that gets named")
	{
		const CatalogValueOrigin door =
				origins.value(QStringLiteral("clearance_door"));
		CHECK(door.isFromClass());
		CHECK(door.class_id == drive_id);
		CHECK(door.class_key == QStringLiteral("drive"));
		CHECK(door.describe().contains(QStringLiteral("Variateur")));

			//Two inherited values on one part naming two different
			//classes: a resolving that answered the class of the
			//part would pass the case above and this one alike,
			//which is why they are asked together.
		CHECK(origins.value(QStringLiteral("clearance_top")).class_id
		      != door.class_id);
	}

	SECTION("the single key question gives the same answer as the map")
	{
		const QStringList keys = { QStringLiteral("width"),
					   QStringLiteral("clearance_top"),
					   QStringLiteral("clearance_door"),
					   QStringLiteral("clearance_bottom") };
		for (const QString &key : keys)
		{
			INFO("clé " << key.toStdString());
			const CatalogValueOrigin one = catalog.valueOrigin(stored, key);
			const CatalogValueOrigin from_map = origins.value(key);
			CHECK(one.source == from_map.source);
			CHECK(one.class_id == from_map.class_id);
			CHECK(one.class_key == from_map.class_key);
		}
	}
}

TEST_CASE("T20 — a measure nobody wrote anywhere is reported without an origin",
	  "[catalog][valueorigin]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int drive_id = addDriveClass(catalog);

	CatalogPart part(QStringLiteral("DRIVE-2"), drive_id);
	const CatalogPart stored = saved(catalog, part);
	const QHash<QString, CatalogValueOrigin> origins =
			catalog.valueOrigins(stored);

	SECTION("declared by a class, filled by nobody: nothing to name")
	{
		const CatalogValueOrigin bottom =
				origins.value(QStringLiteral("clearance_bottom"));
		CHECK(bottom.isRead());
		CHECK(bottom.source == CatalogValueSource::Unset);
		CHECK_FALSE(bottom.holdsValue());
		CHECK_FALSE(bottom.isFromClass());
		CHECK(bottom.class_id == 0);
		CHECK(bottom.className().isEmpty());

			//The initial value of that property is empty, which is
			//what the seeding writes for every measure nobody took.
			//Inheriting emptiness is inheriting nothing, and saying
			//"from class Composant" about it would turn a field the
			//office never filled into a field the office decided.
		CHECK(catalog.effectiveProperty(drive_id,
						QStringLiteral("clearance_bottom"))
		      .default_value.isEmpty());
	}

	SECTION("a key no class of the part declares is a key nothing was read about")
	{
		const QString unknown = QStringLiteral("clearance_ceiling");
		CHECK_FALSE(origins.contains(unknown));
		CHECK_FALSE(catalog.effectiveValues(stored).contains(unknown));

			//Unread and not Unset: this reading says nothing about
			//that key, which is not the same statement as the key
			//being empty. describe() stays silent for it, so a panel
			//cannot paste a sentence about a question nobody put.
		const CatalogValueOrigin nothing = origins.value(unknown);
		CHECK_FALSE(nothing.isRead());
		CHECK(nothing.source == CatalogValueSource::Unread);
		CHECK(nothing.describe().isEmpty());
	}

	SECTION("a cell that is not a number still came from where it was typed")
	{
		CatalogPart typed(QStringLiteral("DRIVE-3"), drive_id);
		typed.setValue(QStringLiteral("clearance_top"),
			       QStringLiteral("environ 10 cm"));
		const CatalogPart back = saved(catalog, typed);

		const CatalogValueOrigin top =
				catalog.valueOrigin(back,
						    QStringLiteral("clearance_top"));
		CHECK(top.isFromPart());

			//Two questions and not one: where the cell was written,
			//and whether what it holds is a length. A part carrying
			//a clearance nobody can use is a part somebody has to
			//be sent back to, and the panel can only say so if the
			//two answers stay apart.
		const CatalogMeasure measure = CatalogPhysicalView::measureIn(
				catalog.effectiveValues(back),
				declarationsOf(catalog, drive_id),
				catalog.valueOrigins(back),
				QStringLiteral("clearance_top"));
		CHECK_FALSE(measure.isDeclared());
		CHECK(measure.origin.isFromPart());
	}
}

TEST_CASE("T20 — zero typed on the part, zero inherited from a class and nothing "
	  "at all are three answers and not two",
	  "[catalog][valueorigin]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int drive_id = addDriveClass(catalog);

		//A breaker meant to be clipped against its neighbour asks for
		//no air on its right, and somebody wrote that down. The office
		//also decided that no drive needs air on its left. Nobody has
		//looked at the bottom yet.
	setInitialValue(catalog, drive_id, "clearance_right", "0");

	CatalogPart part(QStringLiteral("DRIVE-4"), drive_id);
	part.setValue(QStringLiteral("clearance_left"), QStringLiteral("0"));
	const CatalogPart stored = saved(catalog, part);

	const CatalogPhysicalView body = CatalogPhysicalView::read(
			catalog.effectiveValues(stored),
			declarationsOf(catalog, drive_id),
			catalog.valueOrigins(stored));

	SECTION("zero typed on the part is a decision of the part")
	{
		CHECK(body.clearance_left.isDeclared());
		CHECK(body.clearance_left.value == Approx(0.0));
		CHECK_FALSE(body.clearance_left.isLength());
		CHECK(body.clearance_left.origin.isFromPart());
	}

	SECTION("zero inherited is a decision of the class, and it says which")
	{
		CHECK(body.clearance_right.isDeclared());
		CHECK(body.clearance_right.value == Approx(0.0));
		CHECK_FALSE(body.clearance_right.isLength());
		CHECK(body.clearance_right.origin.isFromClass());
		CHECK(body.clearance_right.origin.class_key
		      == QStringLiteral("component"));
	}

	SECTION("not measured is neither of the two")
	{
		CHECK_FALSE(body.clearance_bottom.isDeclared());
		CHECK(body.clearance_bottom.value == Approx(0.0));
		CHECK_FALSE(body.clearance_bottom.origin.holdsValue());
		CHECK(body.clearance_bottom.origin.source
		      == CatalogValueSource::Unset);
	}

	SECTION("the three are told apart by the pair and not by the number")
	{
			//All three read zero millimetre, and the mounting check
			//asks nothing of any of them - that fold is deliberate
			//and belongs there. Here they have to stay three: a
			//panel that showed the same sentence for a zero
			//somebody measured and a zero nobody looked at would be
			//the failure this whole answer exists to prevent.
		CHECK(body.clearance_left.value == body.clearance_right.value);
		CHECK(body.clearance_left.value == body.clearance_bottom.value);

		CHECK(body.clearance_left.origin.source
		      != body.clearance_right.origin.source);
		CHECK(body.clearance_left.origin.source
		      != body.clearance_bottom.origin.source);
		CHECK(body.clearance_right.origin.source
		      != body.clearance_bottom.origin.source);

		CHECK(body.clearance_left.isDeclared()
		      != body.clearance_bottom.isDeclared());
		CHECK(body.clearance_right.isDeclared()
		      != body.clearance_bottom.isDeclared());

			//And the sentences differ too, which is what the panel
			//shows: three states that produced one sentence would
			//be three states nobody can act on.
		CHECK(body.clearance_left.origin.describe()
		      != body.clearance_right.origin.describe());
		CHECK(body.clearance_right.origin.describe()
		      != body.clearance_bottom.origin.describe());
	}
}

TEST_CASE("T20 — a value cleared on the part does not bring the initial value of "
	  "the class back",
	  "[catalog][valueorigin]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int drive_id = addDriveClass(catalog);
	setInitialValue(catalog, drive_id, "clearance_top", "100");

	CatalogPart part(QStringLiteral("DRIVE-5"), drive_id);
	part.setValue(QStringLiteral("clearance_top"), QString());
	const CatalogPart stored = saved(catalog, part);

		//The class still says 100: what changed is that this part holds
		//a cell of its own, and an own cell wins whatever it holds.
	CHECK(catalog.effectiveProperty(drive_id, QStringLiteral("clearance_top"))
	      .default_value == QStringLiteral("100"));
	REQUIRE(stored.hasValue(QStringLiteral("clearance_top")));
	CHECK(catalog.effectiveValues(stored)
	      .value(QStringLiteral("clearance_top")).isEmpty());

	const CatalogValueOrigin top =
			catalog.valueOrigin(stored, QStringLiteral("clearance_top"));

		//Unset, and above all not Class: the flattened map hands over
		//an empty cell, so an origin naming Composant would explain a
		//100 mm that nothing shows and that no check applies.
	CHECK(top.source == CatalogValueSource::Unset);
	CHECK_FALSE(top.isFromClass());
	CHECK_FALSE(top.holdsValue());
	CHECK(top.className().isEmpty());
}

TEST_CASE("T20 — the origins answer for the same keys the values answer for",
	  "[catalog][valueorigin]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int drive_id = addDriveClass(catalog);
	setInitialValue(catalog, drive_id, "clearance_top", "100");

	CatalogPart part(QStringLiteral("DRIVE-6"), drive_id);
	part.setValue(QStringLiteral("width"), QStringLiteral("150"));
	part.setValue(QStringLiteral("height"), QStringLiteral("330"));
	part.setValue(QStringLiteral("clearance_left"), QStringLiteral("0"));
	part.setValue(QStringLiteral("clearance_bottom"), QString());
		//The one own cell that is empty over an initial value that is
		//not. Measured, and it is why it is here: with only the empty
		//cell above - whose class says nothing either - a resolving
		//that let an empty cell fall through to the class default kept
		//this whole case green.
	part.setValue(QStringLiteral("clearance_top"), QString());
	const CatalogPart stored = saved(catalog, part);

	const QHash<QString, QString> values = catalog.effectiveValues(stored);
	const QHash<QString, CatalogValueOrigin> origins =
			catalog.valueOrigins(stored);

	QStringList value_keys = values.keys();
	value_keys.sort();
	QStringList origin_keys = origins.keys();
	origin_keys.sort();

		//One question asked twice over the same map. A key answered by
		//one and not by the other is a number a panel would show
		//without provenance, or a provenance about a number that is not
		//there.
	CHECK(value_keys == origin_keys);
	CHECK_FALSE(value_keys.isEmpty());

	for (const QString &key : value_keys)
	{
		INFO("clé " << key.toStdString());
		const CatalogValueOrigin origin = origins.value(key);
		CHECK(origin.isRead());

			//There is a record to name exactly when there is
			//something to show.
		CHECK(origin.holdsValue() == !values.value(key).trimmed().isEmpty());

		if (origin.isFromClass())
		{
				//And when the record is a class, the value
				//handed over is that declaration's initial
				//value, on the class the origin names.
			const CatalogProperty declared =
					catalog.effectiveProperty(stored.class_id, key);
			CHECK(declared.class_id == origin.class_id);
			CHECK(values.value(key) == declared.default_value);
		}
	}
}

TEST_CASE("T20 — a body read without the origins says nothing about them rather "
	  "than saying nothing was filled in",
	  "[catalog][valueorigin]")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	const int drive_id = addDriveClass(catalog);
	setInitialValue(catalog, drive_id, "clearance_top", "100");

	CatalogPart part(QStringLiteral("DRIVE-7"), drive_id);
	part.setValue(QStringLiteral("width"), QStringLiteral("150"));
	const CatalogPart stored = saved(catalog, part);

	const QHash<QString, QString> values = catalog.effectiveValues(stored);
	const QHash<QString, CatalogProperty> declarations =
			declarationsOf(catalog, drive_id);

	const CatalogPhysicalView bare =
			CatalogPhysicalView::read(values, declarations);
	const CatalogPhysicalView explained =
			CatalogPhysicalView::read(values, declarations,
						  catalog.valueOrigins(stored));

	SECTION("the numbers are the same numbers")
	{
			//The overload with the origins reads the cells once,
			//through the overload without them: a second reading
			//would let what is drawn and what is explained drift
			//apart.
		CHECK(bare.width.value == Approx(explained.width.value));
		CHECK(bare.hasPhysicalView() == explained.hasPhysicalView());
		CHECK(bare.clearance_top.isDeclared()
		      == explained.clearance_top.isDeclared());
		CHECK(bare.draw_outline == explained.draw_outline);
	}

	SECTION("the reading without origins claims none")
	{
		CHECK_FALSE(bare.width.origin.isRead());
		CHECK_FALSE(bare.clearance_top.origin.isRead());
		CHECK(bare.width.origin.describe().isEmpty());

			//And it is not the same thing as the cell being empty:
			//the width was typed on the part, and this reading
			//simply did not ask.
		CHECK(bare.width.isDeclared());
		CHECK(bare.width.origin.source != CatalogValueSource::Unset);
	}

	SECTION("the reading with origins carries them")
	{
		CHECK(explained.width.origin.isFromPart());
		CHECK(explained.clearance_top.origin.isFromClass());
		CHECK(explained.clearance_top.origin.class_key
		      == QStringLiteral("component"));
		CHECK(explained.clearance_bottom.origin.source
		      == CatalogValueSource::Unset);
	}
}
