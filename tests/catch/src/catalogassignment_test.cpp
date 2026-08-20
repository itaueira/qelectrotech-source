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
#include "../../../sources/catalog/catalogassignment.h"
#include "qt_catch_tostring.h"

namespace
{
	/// A catalog in memory with one contactor part, coil plus two contacts.
	struct AssignmentFixture
	{
		AssignmentFixture()
		{
			QString error;
			REQUIRE(catalog.openInMemory(&error));

			contactor_id = catalog.classByKey(QStringLiteral("contactor")).id;
			REQUIRE(contactor_id > 0);

			part = CatalogPart(QStringLiteral("CONT-9A-24VCC"), contactor_id);
			part.setValue(QStringLiteral("designation"),
				      QStringLiteral("Contator 9 A, bobina 24 Vcc"));
			part.setValue(QStringLiteral("manufacturer"), QStringLiteral("Fornecedor A"));
			part.setValue(QStringLiteral("width"), QStringLiteral("45"));

			// The coil is one symbol, each auxiliary contact another. The
			// group is what keeps their numbers apart at assignment time.
			addPin(QStringLiteral("A1"), CatalogPinRole::Coil, QString(), coil_symbol);
			addPin(QStringLiteral("A2"), CatalogPinRole::Coil, QString(), coil_symbol);
			addPin(QStringLiteral("13"), CatalogPinRole::ContactNo,
			       QStringLiteral("13-14"), no_symbol);
			addPin(QStringLiteral("14"), CatalogPinRole::ContactNo,
			       QStringLiteral("13-14"), no_symbol);
			addPin(QStringLiteral("21"), CatalogPinRole::ContactNc,
			       QStringLiteral("21-22"), nc_symbol);
			addPin(QStringLiteral("22"), CatalogPinRole::ContactNc,
			       QStringLiteral("21-22"), nc_symbol);

			QString save_error;
			REQUIRE(catalog.savePart(part, &save_error));
			REQUIRE(save_error.isEmpty());
		}

		void addPin(const QString &label, CatalogPinRole role,
			    const QString &pair, const QString &group)
		{
			CatalogPin pin(label, role);
			pin.pair = pair;
			pin.group = group;
			part.pins.append(pin);
		}

		Catalog catalog;
		CatalogPart part;
		int contactor_id = 0;
		const QString coil_symbol = QStringLiteral("bobine_contacteur.elmt");
		const QString no_symbol   = QStringLiteral("contact_no.elmt");
		const QString nc_symbol   = QStringLiteral("contact_nc.elmt");
	};
}

TEST_CASE("CU-13.1 — attribuer une pièce apporte tout ce qu'elle sait")
{
	AssignmentFixture fixture;

	const QHash<QString, QString> values =
		CatalogAssignment::valuesForElement(fixture.catalog, fixture.part);

	// Not only the code: the designation, the manufacturer and the physical
	// width travel with it, which is why the catalog had to be a class model.
	CHECK(values.value(QStringLiteral("part_code")) == QStringLiteral("CONT-9A-24VCC"));
	CHECK(values.value(QStringLiteral("part_revision")) == QStringLiteral("1"));
	CHECK(values.value(QStringLiteral("designation"))
	      == QStringLiteral("Contator 9 A, bobina 24 Vcc"));
	CHECK(values.value(QStringLiteral("manufacturer")) == QStringLiteral("Fornecedor A"));
	CHECK(values.value(QStringLiteral("width")) == QStringLiteral("45"));

	// The real pin numbers reach each symbol, per symbol.
	const QStringList coil =
		CatalogAssignment::terminalNames(fixture.part, fixture.coil_symbol, 2);
	CHECK(coil == QStringList({ QStringLiteral("A1"), QStringLiteral("A2") }));

	const QStringList no_contact =
		CatalogAssignment::terminalNames(fixture.part, fixture.no_symbol, 2);
	CHECK(no_contact == QStringList({ QStringLiteral("13"), QStringLiteral("14") }));

	const QStringList nc_contact =
		CatalogAssignment::terminalNames(fixture.part, fixture.nc_symbol, 2);
	CHECK(nc_contact == QStringList({ QStringLiteral("21"), QStringLiteral("22") }));

	// A symbol the part knows nothing about keeps its provisional labels
	// instead of being blanked.
	const QStringList unknown =
		CatalogAssignment::terminalNames(fixture.part, QStringLiteral("autre.elmt"), 2);
	CHECK(unknown == QStringList({ QString(), QString() }));

	// And a symbol with more terminals than the part has pins keeps the rest.
	const QStringList longer =
		CatalogAssignment::terminalNames(fixture.part, fixture.coil_symbol, 4);
	REQUIRE(longer.size() == 4);
	CHECK(longer.at(0) == QStringLiteral("A1"));
	CHECK(longer.at(2).isEmpty());
}

TEST_CASE("CU-13.1 — une pièce dessinée en un seul symbole n'a pas besoin de groupe")
{
	AssignmentFixture fixture;

	// A part registered from a single symbol records no group at all; asking
	// for any group must still give its pins, or assigning it would silently
	// do nothing.
	CatalogPart single(QStringLiteral("SINALEIRO-24V"), fixture.contactor_id);
	single.pins.append(CatalogPin(QStringLiteral("X1"), CatalogPinRole::Terminal));
	single.pins.append(CatalogPin(QStringLiteral("X2"), CatalogPinRole::Terminal));

	const QStringList names =
		CatalogAssignment::terminalNames(single, QStringLiteral("voyant.elmt"), 2);
	CHECK(names == QStringList({ QStringLiteral("X1"), QStringLiteral("X2") }));

	const QStringList without_group =
		CatalogAssignment::terminalNames(single, QString(), 2);
	CHECK(without_group == QStringList({ QStringLiteral("X1"), QStringLiteral("X2") }));
}

TEST_CASE("CU-13.1 — le repère et la numérotation ne sont jamais écrasés")
{
	AssignmentFixture fixture;

	// A catalog property that happens to be named after something the
	// component owns must not reach the drawing. The tag is the case that
	// hurts: a part assignment that renumbers the folio is a disaster that
	// looks like a feature.
	QString error;
	const int component_id = fixture.catalog.classByKey(QStringLiteral("component")).id;
	for (const char *name : { "label", "formula", "plant", "location" })
	{
		CatalogProperty property(QString::fromLatin1(name),
					 QString::fromLatin1(name),
					 CatalogPropertyType::Text);
		property.class_id = component_id;
		property.default_value = QStringLiteral("NAO DEVE APARECER");
		REQUIRE(fixture.catalog.addProperty(property, &error) > 0);
	}

	const CatalogPart reread = fixture.catalog.partByCode(QStringLiteral("CONT-9A-24VCC"));
	const QHash<QString, QString> values =
		CatalogAssignment::valuesForElement(fixture.catalog, reread);

	for (const QString &key : CatalogAssignment::protectedElementKeys()) {
		CHECK_FALSE(values.contains(key));
	}
	CHECK_FALSE(values.contains(QStringLiteral("label")));
	CHECK_FALSE(values.contains(QStringLiteral("formula")));

	// What is not protected still travels.
	CHECK(values.contains(QStringLiteral("designation")));
}

TEST_CASE("CU-13.7 — remplacer une pièce efface ce que l'ancienne avait mis")
{
	AssignmentFixture fixture;
	QString error;

	// A discontinued contactor replaced by another: the manufacturer of the
	// old one must not survive on the component.
	CatalogPart replacement(QStringLiteral("CONT-9A-220VCA"), fixture.contactor_id);
	replacement.setValue(QStringLiteral("designation"),
			     QStringLiteral("Contator 9 A, bobina 220 Vca"));
	REQUIRE(fixture.catalog.savePart(replacement, &error));

	const QHash<QString, QString> values =
		CatalogAssignment::valuesForElement(fixture.catalog, replacement);

	CHECK(values.value(QStringLiteral("part_code")) == QStringLiteral("CONT-9A-220VCA"));
	// Present and empty, not absent: writing the empty value is what clears
	// the manufacturer the previous part had left behind.
	CHECK(values.contains(QStringLiteral("manufacturer")));
	CHECK(values.value(QStringLiteral("manufacturer")).isEmpty());
	CHECK(values.contains(QStringLiteral("width")));
	CHECK(values.value(QStringLiteral("width")).isEmpty());
}

TEST_CASE("CU-13.6 — un accessoire enregistré avec la pièce revient avec elle")
{
	AssignmentFixture fixture;
	QString error;

	// The fuse inside the fuse holder: the case that repeats in every cabinet.
	CatalogPart fuse(QStringLiteral("FUSIVEL-2A"), fixture.contactor_id);
	fuse.setValue(QStringLiteral("designation"), QStringLiteral("Fusível 2 A"));
	REQUIRE(fixture.catalog.savePart(fuse, &error));

	CatalogPart holder(QStringLiteral("PORTA-FUSIVEL-1"), fixture.contactor_id);
	holder.accessories.append(CatalogAccessory(QStringLiteral("FUSIVEL-2A"), 1));
	REQUIRE(fixture.catalog.savePart(holder, &error));

	const CatalogPart reread = fixture.catalog.partByCode(QStringLiteral("PORTA-FUSIVEL-1"));
	REQUIRE(reread.accessories.size() == 1);
	CHECK(reread.accessories.first().code == QStringLiteral("FUSIVEL-2A"));

	// Removing the accessory and saving again is how it stops coming back.
	CatalogPart without = reread;
	without.accessories.clear();
	REQUIRE(fixture.catalog.savePart(without, &error));
	CHECK(fixture.catalog.partByCode(QStringLiteral("PORTA-FUSIVEL-1")).accessories.isEmpty());
}

TEST_CASE("CU-13.8 — le rapport de fin de projet compte une seule chose")
{
	QHash<QString, QString> with_part;
	with_part.insert(QStringLiteral("label"), QStringLiteral("K1"));
	with_part.insert(CatalogAssignment::partCodeKey(), QStringLiteral("CONT-9A-24VCC"));
	CHECK_FALSE(CatalogAssignment::isWithoutPart(with_part));

	QHash<QString, QString> without_part;
	without_part.insert(QStringLiteral("label"), QStringLiteral("K2"));
	without_part.insert(QStringLiteral("designation"), QStringLiteral("Digitado à mão"));
	CHECK(CatalogAssignment::isWithoutPart(without_part));

	// A component whose code is blank counts as missing, not as filled: a
	// report that trusts the presence of the key would return an empty list
	// on a project where somebody cleared the field.
	QHash<QString, QString> blank_code;
	blank_code.insert(CatalogAssignment::partCodeKey(), QStringLiteral("   "));
	CHECK(CatalogAssignment::isWithoutPart(blank_code));
}

TEST_CASE("CU-13.2 — la pièce par défaut d'un symbole est une pièce comme les autres")
{
	AssignmentFixture fixture;

	// The default part of a symbol is read by code, the same way an explicit
	// assignment is: nothing in the assignment path needs to know where the
	// code came from.
	const CatalogPart by_code = fixture.catalog.partByCode(QStringLiteral("CONT-9A-24VCC"));
	REQUIRE_FALSE(by_code.isNull());

	const QHash<QString, QString> values =
		CatalogAssignment::valuesForElement(fixture.catalog, by_code);
	CHECK(values.value(QStringLiteral("part_code")) == QStringLiteral("CONT-9A-24VCC"));

	// A code that is not in the catalog gives nothing, and gives it quietly:
	// a symbol carrying a default code the catalog lost must still be
	// insertable.
	const CatalogPart missing = fixture.catalog.partByCode(QStringLiteral("NAO-EXISTE"));
	CHECK(missing.isNull());
	CHECK(CatalogAssignment::valuesForElement(fixture.catalog, missing).isEmpty());
}
