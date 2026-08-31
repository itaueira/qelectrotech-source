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
#include "../../../sources/catalog/catalogclass.h"
#include "../../../sources/catalog/catalogclasspackage.h"
#include "../../../sources/catalog/catalogimport.h"
#include "../../../sources/catalog/catalogtablereader.h"
#include "qt_catch_tostring.h"

#include <QFile>
#include <QTemporaryDir>

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

TEST_CASE("CU-13.9 — atribuir peça não apaga o que o projetista escreveu", "[catalog]")
{
	AssignmentFixture fixture;
	QString error;

		//The real case of the project: a phase monitor drawn with a function
		//and a comment somebody typed while looking at the panel, and a
		//catalog part that knows the manufacturer but says nothing about
		//either field.
	CatalogPart monitor(QStringLiteral("RPW-PTCE05"), fixture.contactor_id);
	monitor.setValue(QStringLiteral("manufacturer"), QStringLiteral("Fornecedor A"));
	monitor.setValue(QStringLiteral("designation"), QStringLiteral("RPW-PTCE05"));
	REQUIRE(fixture.catalog.savePart(monitor, &error));

	QHash<QString, QString> drawn;
	drawn.insert(QStringLiteral("label"), QStringLiteral("RL2"));
	drawn.insert(QStringLiteral("function"), QStringLiteral("Monitor Fase Entrada"));
	drawn.insert(QStringLiteral("comment"), QStringLiteral("220V"));

	SECTION("a primeira atribuição preenche e não apaga")
	{
		const QHash<QString, QString> values =
			CatalogAssignment::valuesForElement(fixture.catalog, monitor, drawn);

			//What the part knows arrives.
		CHECK(values.value(QStringLiteral("part_code")) == QStringLiteral("RPW-PTCE05"));
		CHECK(values.value(QStringLiteral("manufacturer")) == QStringLiteral("Fornecedor A"));

			//What a person typed is not touched: the key is absent, so the
			//undo command has nothing to write over it.
		CHECK_FALSE(values.contains(QStringLiteral("function")));
		CHECK_FALSE(values.contains(QStringLiteral("comment")));

			//And a field nobody filled is still cleared, because there is
			//nothing to lose there.
		CHECK(values.contains(QStringLiteral("supplier")));
		CHECK(values.value(QStringLiteral("supplier")).isEmpty());
	}

	SECTION("trocar de peça apaga o que a peça anterior tinha posto")
	{
			//The component already carries the monitor: manufacturer and
			//designation on it came from the part, not from a person.
		QHash<QString, QString> current = drawn;
		const QHash<QString, QString> first =
			CatalogAssignment::valuesForElement(fixture.catalog, monitor, drawn);
		const QStringList first_keys = first.keys();
		for (const QString &key : first_keys) {
			current.insert(key, first.value(key));
		}
		CHECK(current.value(QStringLiteral("manufacturer")) == QStringLiteral("Fornecedor A"));
		CHECK(current.value(QStringLiteral("comment")) == QStringLiteral("220V"));

			//The replacement says nothing about the manufacturer.
		CatalogPart replacement(QStringLiteral("OUTRO-MONITOR"), fixture.contactor_id);
		replacement.setValue(QStringLiteral("designation"), QStringLiteral("Outro monitor"));
		REQUIRE(fixture.catalog.savePart(replacement, &error));

		const QHash<QString, QString> values =
			CatalogAssignment::valuesForElement(fixture.catalog, replacement, current);

			//It is cleared anyway: the component is no longer that product.
			//This is CU-13.7, and it has to keep working now that empties are
			//no longer written blindly.
		CHECK(values.contains(QStringLiteral("manufacturer")));
		CHECK(values.value(QStringLiteral("manufacturer")).isEmpty());
		CHECK(values.value(QStringLiteral("part_code")) == QStringLiteral("OUTRO-MONITOR"));

			//The comment the person typed survives the swap too.
		CHECK_FALSE(values.contains(QStringLiteral("comment")));
	}

	SECTION("uma peça anterior que o catálogo perdeu não autoriza apagar nada")
	{
			//A project made on another machine can carry a code this catalog
			//does not have. Without the previous part there is no way to tell
			//product data from typed text, and the safe answer is to keep
			//what is there.
		QHash<QString, QString> current = drawn;
		current.insert(CatalogAssignment::partCodeKey(), QStringLiteral("NAO-EXISTE"));
		current.insert(CatalogAssignment::partRevisionKey(), QStringLiteral("1"));
		current.insert(QStringLiteral("manufacturer"), QStringLiteral("Fornecedor Desconhecido"));

		const QHash<QString, QString> values =
			CatalogAssignment::valuesForElement(fixture.catalog, monitor, current);

			//The new part has a manufacturer, so it overwrites - that is not
			//erasing, that is assigning.
		CHECK(values.value(QStringLiteral("manufacturer")) == QStringLiteral("Fornecedor A"));
		CHECK_FALSE(values.contains(QStringLiteral("comment")));
		CHECK_FALSE(values.contains(QStringLiteral("function")));
	}

	SECTION("componente sem nada escrito se comporta como antes")
	{
			//A blank component: every empty is written, exactly as the
			//two-argument overload does. The new rule must not change the
			//case it was not made for.
		const QHash<QString, QString> plain =
			CatalogAssignment::valuesForElement(fixture.catalog, monitor);
		const QHash<QString, QString> guarded =
			CatalogAssignment::valuesForElement(fixture.catalog, monitor,
							    QHash<QString, QString>());
		CHECK(plain == guarded);
	}
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

TEST_CASE("CU-13.10 — registrar peça a partir do componente corrige a peça, não a rebaixa",
	  "[catalog]")
{
	Catalog catalog;
	QString error;
	REQUIRE(catalog.openInMemory(&error));

	const int component_id = catalog.classByKey(QStringLiteral("component")).id;
	REQUIRE(component_id > 0);

	// A class of the office, with a field the generic class does not have.
	CatalogClass monitoring(QStringLiteral("monitoring_relay"),
				QStringLiteral("Relé de monitoramento"));
	monitoring.parent_id = component_id;
	const int monitoring_id = catalog.addClass(monitoring, &error);
	REQUIRE(monitoring_id > 0);

	CatalogProperty supply(QString(), QStringLiteral("Tensão de alimentação"),
			       CatalogPropertyType::Text);
	supply.class_id = monitoring_id;
	REQUIRE(catalog.addProperty(supply, &error) > 0);
	const QString supply_key = QStringLiteral("tensao_alimentacao");

	// The part as the office typed it once: the class, the typed field, and an
	// origin that says it came from a package rather than from a drawing.
	CatalogPart stored(QStringLiteral("RPW-PTCE05"), monitoring_id);
	stored.setValue(QStringLiteral("designation"), QStringLiteral("Relé PTC"));
	stored.setValue(supply_key, QStringLiteral("24 Vcc"));
	stored.origin = QStringLiteral("package");
	REQUIRE(catalog.savePart(stored, &error));
	REQUIRE(stored.id > 0);

	// What the component carries on the folio: the code of that part, plus a
	// description the designer has just corrected while drawing.
	QHash<QString, QString> information;
	information.insert(CatalogAssignment::partCodeKey(), QStringLiteral("RPW-PTCE05"));
	information.insert(QStringLiteral("designation"),
			   QStringLiteral("Relé PTC, rearme manual"));

	const CatalogPart proposed = CatalogAssignment::partFromValues(catalog, information);

	// It is the part that exists, in its own class: the dialog opens on Relé de
	// monitoramento with the typed field filled, not on Componente empty.
	CHECK(proposed.id == stored.id);
	CHECK(proposed.class_id == monitoring_id);
	CHECK(proposed.value(supply_key) == QStringLiteral("24 Vcc"));
	CHECK(proposed.origin == QStringLiteral("package"));

	// And the correction from the drawing did travel.
	CHECK(proposed.value(QStringLiteral("designation"))
	      == QStringLiteral("Relé PTC, rearme manual"));

	// Saving it back is the moment the old behaviour lost the data: a bare part
	// with the same code took the id, so the class became Componente and the
	// rewrite of the value rows deleted the typed field.
	CatalogPart to_save = proposed;
	REQUIRE(catalog.savePart(to_save, &error));

	const CatalogPart reread = catalog.partByCode(QStringLiteral("RPW-PTCE05"));
	CHECK(reread.class_id == monitoring_id);
	CHECK(reread.value(supply_key) == QStringLiteral("24 Vcc"));
	CHECK(reread.value(QStringLiteral("designation"))
	      == QStringLiteral("Relé PTC, rearme manual"));
	CHECK(reread.origin == QStringLiteral("package"));
}

TEST_CASE("CU-13.10 — um componente que não aponta peça nenhuma nasce em Componente",
	  "[catalog]")
{
	Catalog catalog;
	QString error;
	REQUIRE(catalog.openInMemory(&error));

	const int component_id = catalog.classByKey(QStringLiteral("component")).id;
	REQUIRE(component_id > 0);

	// The other half of the use case, and the reason the generic class still
	// exists: a component drawn with a manufacturer typed on it and no code.
	QHash<QString, QString> information;
	information.insert(QStringLiteral("manufacturer"), QStringLiteral("Fornecedor A"));
	information.insert(QStringLiteral("nowhere"), QStringLiteral("nada"));

	const CatalogPart part = CatalogAssignment::partFromValues(catalog, information);

	CHECK(part.id == 0);
	CHECK(part.class_id == component_id);
	CHECK(part.code.isEmpty());
	CHECK(part.origin == QStringLiteral("project"));
	CHECK(part.value(QStringLiteral("manufacturer")) == QStringLiteral("Fornecedor A"));

	// A key no class declares is not stored: writing the value rows does not
	// filter by class, so a value nobody can see would be a value nobody can
	// correct.
	CHECK(part.value(QStringLiteral("nowhere")).isEmpty());

	// A code that names no part is kept, because it is what the designer
	// wants to register under.
	information.insert(CatalogAssignment::partCodeKey(), QStringLiteral("SEM-CADASTRO-1"));
	const CatalogPart named = CatalogAssignment::partFromValues(catalog, information);
	CHECK(named.id == 0);
	CHECK(named.class_id == component_id);
	CHECK(named.code == QStringLiteral("SEM-CADASTRO-1"));
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

TEST_CASE("CU-13.5 — o acessório diz de quem ele é", "[catalog]")
{
	SECTION("a chave guarda o uuid do dono, não a tag dele")
	{
			//Tag é renumerada. Um vínculo que quebra quando o projeto é
			//renumerado é pior que nenhum vínculo — por isso a chave existe
			//separada e o teste diz o que ela guarda.
		CHECK(CatalogAssignment::accessoryOwnerKey() ==
		      QStringLiteral("accessory_of"));

			//E não é uma chave que a atribuição de peça escreve ou apaga: o
			//acessório continua rattaché depois de trocar a peça do dono.
		CHECK_FALSE(CatalogAssignment::protectedElementKeys()
			    .contains(CatalogAssignment::accessoryOwnerKey()));
	}
}

TEST_CASE("CU-13.11 — o rótulo de desfazer nomeia a tag, nunca o símbolo", "[catalog]")
{
	CatalogPart part(QStringLiteral("PSS24-W/5"), 1);

	SECTION("com tag, é a tag que aparece")
	{
			//Achado dirigindo o programa em 28/08: a lista de desfazer dizia
			//"Atribuir a peça PSS24-W/5 a Fonte chaveada AC/DC 68V" — o nome do
			//símbolo. Num projeto de 236 componentes, vários compartilham o mesmo
			//símbolo, e a entrada não dizia qual deles tinha sido tocado.
		const QString label = CatalogAssignment::commandLabel(
			part,
			QStringLiteral("PS4 - Switch"),
			QStringLiteral("Fonte chaveada AC/DC 68V"));

		CHECK(label.contains(QStringLiteral("PS4 - Switch")));
		CHECK_FALSE(label.contains(QStringLiteral("Fonte chaveada AC/DC 68V")));
		CHECK(label.contains(QStringLiteral("PSS24-W/5")));
	}

	SECTION("sem tag, cai no nome do símbolo em vez de ficar em branco")
	{
			//Um componente recém posto na folha ainda não tem tag. Melhor um
			//rótulo impreciso que um rótulo vazio.
		const QString label = CatalogAssignment::commandLabel(
			part, QString(), QStringLiteral("Fonte chaveada AC/DC 68V"));

		CHECK(label.contains(QStringLiteral("Fonte chaveada AC/DC 68V")));
	}

	SECTION("sem tag e sem nome de símbolo, o código da peça ainda aparece")
	{
		const QString label =
			CatalogAssignment::commandLabel(part, QString(), QString());

		CHECK(label.contains(QStringLiteral("PSS24-W/5")));
	}
}

TEST_CASE("a ancestralidade de classe responde por descendência, não por igualdade", "[catalog]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	Catalog catalog;
	QString error;
	REQUIRE(catalog.open(dir.path() + QStringLiteral("/c.sqlite"), &error));

	const CatalogClass accessory =
			catalog.classByKey(QStringLiteral("accessory"));
	REQUIRE(accessory.id > 0);

	SECTION("a própria classe conta")
	{
		CHECK(catalog.isDescendantOf(accessory.id,
					     QStringLiteral("accessory")));
	}

	SECTION("uma classe que a casa criou debaixo dela também conta")
	{
			//"Acessório de porta" debaixo de "Acessório" continua sendo
			//acessório — é o motivo de a pergunta ser por descendência.
		CatalogClass door(QStringLiteral("door_accessory"),
				  QStringLiteral("Acessório de porta"));
		door.parent_id = accessory.id;
		const int id = catalog.addClass(door, &error);
		REQUIRE(id > 0);
		CHECK(catalog.isDescendantOf(id, QStringLiteral("accessory")));
	}

	SECTION("um contator não é acessório")
	{
		const CatalogClass contactor =
				catalog.classByKey(QStringLiteral("contactor"));
		REQUIRE(contactor.id > 0);
		CHECK_FALSE(catalog.isDescendantOf(contactor.id,
						   QStringLiteral("accessory")));
	}

	SECTION("chave vazia e classe inexistente não são descendentes de nada")
	{
		CHECK_FALSE(catalog.isDescendantOf(accessory.id, QString()));
		CHECK_FALSE(catalog.isDescendantOf(999999,
						   QStringLiteral("accessory")));
	}
}
