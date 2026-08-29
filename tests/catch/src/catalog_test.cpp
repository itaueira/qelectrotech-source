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
#include "../../../sources/catalog/catalogpackage.h"
#include "../../../sources/catalog/catalogschema.h"
#include "qt_catch_tostring.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace
{
	/// A catalog in memory, seeded with the default class tree.
	class OpenCatalog
	{
		public:
			OpenCatalog()
			{
				QString error;
				opened = catalog.openInMemory(&error);
				REQUIRE(error.isEmpty());
				REQUIRE(opened);
			}

			Catalog catalog;
			bool opened = false;
	};

	int classId(const Catalog &catalog, const char *key)
	{
		const CatalogClass found = catalog.classByKey(QString::fromLatin1(key));
		REQUIRE_FALSE(found.isNull());
		return found.id;
	}

	bool hasProperty(const QList<CatalogProperty> &properties, const QString &key)
	{
		for (const CatalogProperty &property : properties)
		{
			if (property.key == key) {
				return true;
			}
		}
		return false;
	}
}

TEST_CASE("Catalog - le catalogue neuf porte l'arbre de classes et ses propriétés")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	CHECK(catalog.isOpen());
	CHECK(catalog.schemaVersion() == CatalogSchema::currentVersion());
	CHECK(catalog.isWritable());
	CHECK(catalog.unitSystem() == QStringLiteral("mm"));

	// The tree the specification asks for, and the component subclasses.
	CHECK_FALSE(catalog.classByKey(QStringLiteral("project_object")).isNull());
	CHECK_FALSE(catalog.classByKey(QStringLiteral("component")).isNull());
	CHECK_FALSE(catalog.classByKey(QStringLiteral("contactor")).isNull());
	CHECK_FALSE(catalog.classByKey(QStringLiteral("terminal_strip")).isNull());

	// The fixed fields of today are properties of the Component class now,
	// under the very keys QElectroTech already uses for an element.
	const QList<CatalogProperty> component =
		catalog.effectiveProperties(classId(catalog, "component"));
	const QStringList seeded = Catalog::seededComponentPropertyKeys();
	for (const QString &key : seeded) {
		CHECK(hasProperty(component, key));
	}
}

TEST_CASE("CU-12.1 — champ nouveau sans programmeur")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	const int contactor_id = classId(catalog, "contactor");
	const int component_id = classId(catalog, "component");

	// A part that already exists *before* the field is created. This is the
	// half of the use case that a naive implementation gets wrong.
	CatalogPart existing(QStringLiteral("PECA-EXISTENTE-1"), contactor_id);
	existing.setValue(QStringLiteral("designation"), QStringLiteral("Contacteur 9 A"));
	QString error;
	REQUIRE(catalog.savePart(existing, &error));
	REQUIRE(error.isEmpty());

	// Add the field on the mother class, with no code change and no migration.
	CatalogProperty internal_code(QString(),
				      QStringLiteral("Código interno ACME"),
				      CatalogPropertyType::Text);
	internal_code.class_id = component_id;
	const int property_id = catalog.addProperty(internal_code, &error);
	REQUIRE(property_id > 0);
	REQUIRE(error.isEmpty());

	// The key is derived from the name, accents folded.
	CHECK(CatalogProperty::keyFromName(QStringLiteral("Código interno ACME"))
	      == QStringLiteral("codigo_interno_acme"));
	const QString key = QStringLiteral("codigo_interno_acme");

	// It shows up on every subclass...
	CHECK(hasProperty(catalog.effectiveProperties(component_id), key));
	CHECK(hasProperty(catalog.effectiveProperties(contactor_id), key));
	CHECK(hasProperty(catalog.effectiveProperties(classId(catalog, "motor")), key));
	CHECK(hasProperty(catalog.effectiveProperties(classId(catalog, "plc")), key));

	// ... and on the part that already existed, which is what makes the
	// property a column the part lists and the bill of material can offer.
	const CatalogPart reread = catalog.partByCode(QStringLiteral("PECA-EXISTENTE-1"));
	REQUIRE_FALSE(reread.isNull());
	const QHash<QString, QString> values = catalog.effectiveValues(reread);
	CHECK(values.contains(key));

	// Nothing outside the Component branch gained the field.
	CHECK_FALSE(hasProperty(catalog.effectiveProperties(classId(catalog, "wire_cable")), key));
}

TEST_CASE("CU-12.2 — héritage : seule la sous-classe gagne le champ")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	const int contactor_id = classId(catalog, "contactor");

	CatalogClass reverser(QStringLiteral("contactor_reverser"),
			      QStringLiteral("Contator reversor"));
	reverser.parent_id = contactor_id;
	QString error;
	const int reverser_id = catalog.addClass(reverser, &error);
	REQUIRE(reverser_id > 0);
	REQUIRE(error.isEmpty());

	CatalogProperty interlock(QString(),
				  QStringLiteral("Intertravamento mecânico"),
				  CatalogPropertyType::Boolean);
	interlock.class_id = reverser_id;
	REQUIRE(catalog.addProperty(interlock, &error) > 0);

	const QString key = QStringLiteral("intertravamento_mecanico");
	CHECK(hasProperty(catalog.effectiveProperties(reverser_id), key));
	CHECK_FALSE(hasProperty(catalog.effectiveProperties(contactor_id), key));

	// The subclass still inherits everything the Component class declares.
	CHECK(hasProperty(catalog.effectiveProperties(reverser_id),
			  QStringLiteral("manufacturer")));

	// Declaring on a subclass a key a mother class already declares is
	// refused: two properties with one key is how a catalog starts lying.
	CatalogProperty clash(QStringLiteral("manufacturer"),
			      QStringLiteral("Fabricante"),
			      CatalogPropertyType::Text);
	clash.class_id = reverser_id;
	error.clear();
	CHECK(catalog.addProperty(clash, &error) == 0);
	CHECK_FALSE(error.isEmpty());
}

TEST_CASE("CU-12.3 — liste suggérée : on peut taper autre chose, et ça se voit")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	const QStringList approved = { QStringLiteral("Fornecedor A"),
				       QStringLiteral("Fornecedor B"),
				       QStringLiteral("Fornecedor C") };

	// The list the seeded Fabricant property is bound to.
	const QStringList lists = catalog.listNames();
	QString manufacturers_list;
	for (const QString &name : lists)
	{
		if (name.startsWith(QStringLiteral("Fabric"))) {
			manufacturers_list = name;
		}
	}
	REQUIRE_FALSE(manufacturers_list.isEmpty());

	QString error;
	REQUIRE(catalog.setListValues(manufacturers_list, approved, &error));
	REQUIRE(error.isEmpty());

	const CatalogProperty manufacturer =
		catalog.effectiveProperty(classId(catalog, "contactor"),
					  QStringLiteral("manufacturer"));
	REQUIRE_FALSE(manufacturer.isNull());
	CHECK(manufacturer.list_behaviour == CatalogListBehaviour::Suggested);
	CHECK(manufacturer.options == approved);

	// Suggested: a value outside the list goes through, and is flagged.
	CHECK(manufacturer.acceptsValue(QStringLiteral("Fornecedor novo")));
	CHECK(manufacturer.isOutsideList(QStringLiteral("Fornecedor novo")));
	CHECK_FALSE(manufacturer.isOutsideList(QStringLiteral("Fornecedor A")));

	// Mandatory: the same value is refused, and saving a part with it fails.
	CatalogProperty degree(QString(),
			       QStringLiteral("Grau de proteção"),
			       CatalogPropertyType::Text);
	degree.class_id = classId(catalog, "component");
	degree.list_behaviour = CatalogListBehaviour::Mandatory;
	// QStringList{...} spelled out: assigning a bare braced list to a
	// QStringList is ambiguous under Qt5, where QStringList is a QList.
	degree.options = QStringList{ QStringLiteral("IP20"),
				      QStringLiteral("IP54"),
				      QStringLiteral("IP65") };
	REQUIRE(catalog.addProperty(degree, &error) > 0);

	const CatalogProperty reread =
		catalog.effectiveProperty(classId(catalog, "contactor"),
					  QStringLiteral("grau_de_protecao"));
	REQUIRE_FALSE(reread.isNull());
	CHECK(reread.acceptsValue(QStringLiteral("IP54")));
	CHECK_FALSE(reread.acceptsValue(QStringLiteral("IP99")));

	CatalogPart part(QStringLiteral("PECA-IP"), classId(catalog, "contactor"));
	part.setValue(QStringLiteral("grau_de_protecao"), QStringLiteral("IP99"));
	error.clear();
	CHECK_FALSE(catalog.savePart(part, &error));
	CHECK_FALSE(error.isEmpty());

	part.setValue(QStringLiteral("grau_de_protecao"), QStringLiteral("IP54"));
	CHECK(catalog.savePart(part, &error));
}

TEST_CASE("CU-12.4 — changer de standard de repère, c'est lire l'autre racine")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	const int motor_id = classId(catalog, "motor");

	// The plan records that the house writes MTR where IEC 81346 writes M.
	CHECK(catalog.tagRoot(motor_id, false) == QStringLiteral("MTR"));
	CHECK(catalog.tagRoot(motor_id, true) == QStringLiteral("M"));

	// A subclass that declares no root of its own inherits the closest one,
	// so a new kind of motor is numbered right without being told twice.
	CatalogClass brake_motor(QStringLiteral("brake_motor"), QStringLiteral("Motofreio"));
	brake_motor.parent_id = motor_id;
	QString error;
	const int brake_motor_id = catalog.addClass(brake_motor, &error);
	REQUIRE(brake_motor_id > 0);
	CHECK(catalog.tagRoot(brake_motor_id, false) == QStringLiteral("MTR"));
	CHECK(catalog.tagRoot(brake_motor_id, true) == QStringLiteral("M"));

	// Changing the root of a class is one edit, and it reaches every object
	// of that class - past, present and future - because nobody stores a copy.
	CatalogClass motor = catalog.classById(motor_id);
	motor.root = QStringLiteral("MOT");
	REQUIRE(catalog.updateClass(motor, &error));
	CHECK(catalog.tagRoot(motor_id, false) == QStringLiteral("MOT"));
	CHECK(catalog.tagRoot(brake_motor_id, false) == QStringLiteral("MOT"));
	CHECK(catalog.tagRoot(brake_motor_id, true) == QStringLiteral("M"));
}

TEST_CASE("CU-12.5 — pièce sans symbole")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	// A terminal strip end stop and a button label plate: real parts, in the
	// bill of material and in the cabinet layout, with no symbol on any folio.
	const CatalogClass strip = catalog.classByKey(QStringLiteral("terminal_strip"));
	REQUIRE_FALSE(strip.isNull());
	CHECK_FALSE(strip.has_symbol);

	QString error;
	CatalogClass plate(QStringLiteral("button_plate"), QStringLiteral("Plaqueta de botão"));
	plate.parent_id = classId(catalog, "accessory");
	plate.has_symbol = false;
	const int plate_id = catalog.addClass(plate, &error);
	REQUIRE(plate_id > 0);

	CatalogPart end_stop(QStringLiteral("BATENTE-REGUA-1"), strip.id);
	end_stop.setValue(QStringLiteral("designation"), QStringLiteral("Batente de régua"));
	REQUIRE(catalog.savePart(end_stop, &error));

	CatalogPart label_plate(QStringLiteral("PLAQUETA-1"), plate_id);
	REQUIRE(catalog.savePart(label_plate, &error));

	// Both are enumerable as project objects without any symbol involved,
	// which is what a list or a layout reads.
	CHECK(catalog.parts(strip.id).size() == 1);
	CHECK(catalog.parts(plate_id).size() == 1);
	CHECK_FALSE(catalog.classById(plate_id).has_symbol);
	CHECK(catalog.partCount() == 2);
}

TEST_CASE("CU-12.6 — révision : le projet livré garde l'ancienne largeur")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;

	const int breaker_id = classId(catalog, "breaker");
	QString error;

	CatalogPart part(QStringLiteral("DISJ-3P-25A"), breaker_id);
	part.setValue(QStringLiteral("width"), QStringLiteral("54"));
	REQUIRE(catalog.savePart(part, &error));
	CHECK(part.revision == 1);

	// The manufacturer changed the product: new revision, old projects keep
	// what they had.
	CatalogPart wider = part;
	wider.setValue(QStringLiteral("width"), QStringLiteral("72"));
	REQUIRE(catalog.savePartAsNewRevision(wider, &error));
	CHECK(wider.revision == 2);

	CHECK(catalog.partRevisions(QStringLiteral("DISJ-3P-25A")) == QList<int>({1, 2}));

	// A project that pinned revision 1 still reads 54.
	const CatalogPart pinned = catalog.partByCode(QStringLiteral("DISJ-3P-25A"), 1);
	REQUIRE_FALSE(pinned.isNull());
	CHECK(pinned.value(QStringLiteral("width")) == QStringLiteral("54"));
	CHECK_FALSE(pinned.is_current);

	// A new project reads the current revision, which is 72.
	const CatalogPart current = catalog.partByCode(QStringLiteral("DISJ-3P-25A"));
	REQUIRE_FALSE(current.isNull());
	CHECK(current.revision == 2);
	CHECK(current.value(QStringLiteral("width")) == QStringLiteral("72"));

	// Correcting a record, by contrast, changes it in place for everybody.
	CatalogPart fix = catalog.partByCode(QStringLiteral("DISJ-3P-25A"));
	fix.setValue(QStringLiteral("designation"), QStringLiteral("Disjuntor tripolar 25 A"));
	REQUIRE(catalog.savePart(fix, &error));
	CHECK(catalog.partRevisions(QStringLiteral("DISJ-3P-25A")).size() == 2);
	CHECK(catalog.partByCode(QStringLiteral("DISJ-3P-25A")).value(QStringLiteral("designation"))
	      == QStringLiteral("Disjuntor tripolar 25 A"));
}

TEST_CASE("CU-12.7 — deux postes sur le même catalogue")
{
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	const QString file = directory.filePath(QStringLiteral("catalogo.sqlite"));

	Catalog first_station;
	Catalog second_station;
	QString error;
	REQUIRE(first_station.open(file, &error));
	REQUIRE(error.isEmpty());
	REQUIRE(second_station.open(file, &error));
	REQUIRE(error.isEmpty());

	// The second station opened a catalog that already had a tree: it must
	// not seed a second one on top.
	CHECK(second_station.classes().size() == first_station.classes().size());

	const int contactor_id = classId(first_station, "contactor");
	CatalogPart part(QStringLiteral("CONT-REDE-1"), contactor_id);
	part.setValue(QStringLiteral("designation"), QStringLiteral("Cadastrado no posto 1"));
	REQUIRE(first_station.savePart(part, &error));

	// The other station sees it without reinstalling or restarting anything.
	const CatalogPart seen = second_station.partByCode(QStringLiteral("CONT-REDE-1"));
	REQUIRE_FALSE(seen.isNull());
	CHECK(seen.value(QStringLiteral("designation")) == QStringLiteral("Cadastrado no posto 1"));

	// Not every draughtsman creates parts.
	second_station.setWritable(false);
	CatalogPart refused(QStringLiteral("CONT-REDE-2"), contactor_id);
	error.clear();
	CHECK_FALSE(second_station.savePart(refused, &error));
	CHECK_FALSE(error.isEmpty());
	CHECK(second_station.partByCode(QStringLiteral("CONT-REDE-2")).isNull());
}

TEST_CASE("CU-12.8 — sans catalogue, le lien reste la référence")
{
	// Opening a catalog that is not reachable fails with a message, and
	// leaves nothing half open behind: the project still has to open, using
	// what was written in the .qet itself.
	Catalog unreachable;
	QString error;
	const QString impossible =
		QStringLiteral("Z:/nao-existe-") + QUuid::createUuid().toString(QUuid::WithoutBraces)
		+ QStringLiteral("/catalogo.sqlite");
	CHECK_FALSE(unreachable.open(impossible, &error));
	CHECK_FALSE(error.isEmpty());
	CHECK_FALSE(unreachable.isOpen());
	CHECK(unreachable.parts().isEmpty());
	CHECK(unreachable.partCount() == 0);
	CHECK(unreachable.classes().isEmpty());

	// And a closed catalog refuses to be written to instead of crashing.
	CatalogPart part(QStringLiteral("QUALQUER"), 1);
	error.clear();
	CHECK_FALSE(unreachable.savePart(part, &error));
	CHECK_FALSE(error.isEmpty());
}

TEST_CASE("CU-12.9 — le modèle tient une charge initiale hétérogène")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	QString error;

	// Fields the office adds later, of the types the specification lists.
	const int component_id = classId(catalog, "component");
	struct Extra { const char *name; CatalogPropertyType type; };
	const Extra extras[] = {
		{ "Corrente nominal",   CatalogPropertyType::Decimal },
		{ "Curva",              CatalogPropertyType::Text },
		{ "Data de homologação", CatalogPropertyType::Date },
		{ "Cor",                CatalogPropertyType::Color },
		{ "Homologado",         CatalogPropertyType::Boolean },
		{ "Passo",              CatalogPropertyType::Measure },
		{ "Preço",              CatalogPropertyType::Currency }
	};
	for (const Extra &extra : extras)
	{
		CatalogProperty property(QString(),
					 QString::fromUtf8(extra.name),
					 extra.type);
		property.class_id = component_id;
		REQUIRE(catalog.addProperty(property, &error) > 0);
	}

	// Fifty parts spread over the component subclasses, each with pins and
	// values. The real load uses real ACME parts and is run outside the
	// repository, because manufacturer references may not be committed here.
	const QList<int> target_classes = { classId(catalog, "contactor"),
					    classId(catalog, "breaker"),
					    classId(catalog, "motor"),
					    classId(catalog, "push_button"),
					    classId(catalog, "indicator") };

	for (int index = 0 ; index < 50 ; ++index)
	{
		CatalogPart part(QStringLiteral("CARGA-%1").arg(index, 3, 10, QLatin1Char('0')),
				 target_classes.at(index % target_classes.size()));
		part.setValue(QStringLiteral("designation"),
			      QStringLiteral("Peça de carga %1").arg(index));
		part.setValue(QStringLiteral("corrente_nominal"), QString::number(index + 1));
		part.setValue(QStringLiteral("homologado"), index % 2 == 0 ? QStringLiteral("1")
									   : QStringLiteral("0"));
		part.setValue(QStringLiteral("preco"), QString::number(12.5 * (index + 1)));
		part.pins.append(CatalogPin(QStringLiteral("A1"), CatalogPinRole::Coil));
		part.pins.append(CatalogPin(QStringLiteral("A2"), CatalogPinRole::Coil));
		CatalogPin no_1(QStringLiteral("13"), CatalogPinRole::ContactNo);
		no_1.pair = QStringLiteral("13-14");
		CatalogPin no_2(QStringLiteral("14"), CatalogPinRole::ContactNo);
		no_2.pair = QStringLiteral("13-14");
		part.pins.append(no_1);
		part.pins.append(no_2);
		REQUIRE(catalog.savePart(part, &error));
	}

	CHECK(catalog.partCount() == 50);

	// A part read back carries its pins, and the contact count is by pair.
	const CatalogPart reread = catalog.partByCode(QStringLiteral("CARGA-007"));
	REQUIRE_FALSE(reread.isNull());
	CHECK(reread.pins.size() == 4);
	CHECK(reread.pinsWithRole(CatalogPinRole::Coil).size() == 2);
	CHECK(reread.contactCount(CatalogPinRole::ContactNo) == 1);
	CHECK(reread.value(QStringLiteral("preco")) == QStringLiteral("100"));

	// Search combines text and class, and clearing a criterion widens again.
	CHECK(catalog.searchParts(QStringLiteral("CARGA-01")).size() == 10);
	CHECK(catalog.searchParts(QStringLiteral("CARGA-01"),
				  classId(catalog, "contactor")).size() == 2);
	CHECK(catalog.searchParts(QString()).size() == 50);
}

TEST_CASE("Catalog - la recherche trouve une pièce par le nom de sa classe")
{
				// The results table shows the class name in its second column, so somebody
				// types what is on screen. Matching the code and the values only made the
				// list shorten to nothing while the sought part was right there.
		OpenCatalog fixture;
		Catalog &catalog = fixture.catalog;
		QString error;

		CatalogClass computer(QStringLiteral("industrial_computer"),
						      QStringLiteral("Computador industrial"));
		computer.parent_id = classId(catalog, "component");
		const int computer_id = catalog.addClass(computer, &error);
		INFO("addClass: " << error.toStdString());
		REQUIRE(computer_id > 0);

				// Neither the code nor any value carries the sought word: the name of the
				// class is the only place it appears.
		CatalogPart unit(QStringLiteral("SC1642"), computer_id);
		unit.setValue(QStringLiteral("designation"), QStringLiteral("Unidade de controle"));
		REQUIRE(catalog.savePart(unit, &error));

		CatalogPart other(QStringLiteral("K-101"), classId(catalog, "contactor"));
		REQUIRE(catalog.savePart(other, &error));

		const QList<CatalogPart> found = catalog.searchParts(QStringLiteral("Computador"));
		REQUIRE(found.size() == 1);
		CHECK(found.first().code == QStringLiteral("SC1642"));

				// A fragment is enough, and the case does not matter: the list shortens
				// while a word is being typed.
		CHECK(catalog.searchParts(QStringLiteral("computador ind")).size() == 1);

				// The class filter still narrows. The name matches, the class does not.
		CHECK(catalog.searchParts(QStringLiteral("Computador"),
								  classId(catalog, "contactor")).isEmpty());

				// And the two older ways of matching keep working.
		CHECK(catalog.searchParts(QStringLiteral("SC16")).size() == 1);
		CHECK(catalog.searchParts(QStringLiteral("Unidade")).size() == 1);
}

TEST_CASE("Catalog - une propriété ajoutée peut imposer sa valeur initiale")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	QString error;

	const int contactor_id = classId(catalog, "contactor");
	CatalogPart before(QStringLiteral("ANTES-1"), contactor_id);
	REQUIRE(catalog.savePart(before, &error));
	CatalogPart other(QStringLiteral("ANTES-2"), contactor_id);
	REQUIRE(catalog.savePart(other, &error));

	CatalogProperty voltage(QString(), QStringLiteral("Tensão de comando"),
				CatalogPropertyType::Text);
	voltage.class_id = classId(catalog, "component");
	voltage.default_value = QStringLiteral("220 V");
	const int property_id = catalog.addProperty(voltage, &error);
	REQUIRE(property_id > 0);

	const QString key = QStringLiteral("tensao_de_comando");

	// Before applying, the parts read the initial value without storing it.
	CatalogPart reread = catalog.partByCode(QStringLiteral("ANTES-1"));
	CHECK_FALSE(reread.hasValue(key));
	CHECK(catalog.effectiveValues(reread).value(key) == QStringLiteral("220 V"));

	// Applying writes it into the parts that never had the field filled.
	CHECK(catalog.applyDefaultToExistingParts(property_id, &error) == 2);
	reread = catalog.partByCode(QStringLiteral("ANTES-1"));
	CHECK(reread.hasValue(key));
	CHECK(reread.value(key) == QStringLiteral("220 V"));

	// Running it again touches nothing: a value already there is not
	// overwritten by the initial value.
	CatalogPart edited = catalog.partByCode(QStringLiteral("ANTES-2"));
	edited.setValue(key, QStringLiteral("24 Vcc"));
	REQUIRE(catalog.savePart(edited, &error));
	CHECK(catalog.applyDefaultToExistingParts(property_id, &error) == 0);
	CHECK(catalog.partByCode(QStringLiteral("ANTES-2")).value(key)
	      == QStringLiteral("24 Vcc"));
}

TEST_CASE("Catalog - l'ordre des propriétés est l'ordre des colonnes")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	QString error;

	const int component_id = classId(catalog, "component");
	QList<CatalogProperty> own = catalog.ownProperties(component_id);
	REQUIRE(own.size() > 3);

	QList<int> reversed;
	for (int index = own.size() - 1 ; index >= 0 ; --index) {
		reversed.append(own.at(index).id);
	}
	REQUIRE(catalog.setPropertyOrder(component_id, reversed, &error));

	own = catalog.ownProperties(component_id);
	QList<int> after;
	for (const CatalogProperty &property : own) {
		after.append(property.id);
	}
	CHECK(after == reversed);
}

TEST_CASE("Catalog - le schéma se migre tout seul et refuse d'être rétrogradé")
{
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	const QString file = directory.filePath(QStringLiteral("antigo.sqlite"));

	// Build a catalog at version 1 only, the way an older build left it.
	const QString connection = QStringLiteral("catalog_migration_test");
	{
		QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
								  connection);
		database.setDatabaseName(file);
		REQUIRE(database.open());
		CatalogSchema::applyConnectionPragmas(database);
		QString error;
		REQUIRE(CatalogSchema::applyUpTo(database, 1, &error));
		REQUIRE(error.isEmpty());
		CHECK(CatalogSchema::versionOf(database) == 1);
		CHECK_FALSE(database.tables().contains(QStringLiteral("catalog_part_accessory")));
		database.close();
	}
	QSqlDatabase::removeDatabase(connection);

	// Opening it with this build migrates it, without a script and without
	// asking anybody.
	Catalog catalog;
	QString error;
	REQUIRE(catalog.open(file, &error));
	REQUIRE(error.isEmpty());
	CHECK(catalog.schemaVersion() == CatalogSchema::currentVersion());
	CHECK(CatalogSchema::currentVersion() >= 2);

	// The accessories that version 2 brought are usable right away.
	const int contactor_id = classId(catalog, "contactor");
	CatalogPart holder(QStringLiteral("PORTA-FUSIVEL-1"), contactor_id);
	holder.accessories.append(CatalogAccessory(QStringLiteral("FUSIVEL-2A"), 1));
	REQUIRE(catalog.savePart(holder, &error));
	const CatalogPart reread = catalog.partByCode(QStringLiteral("PORTA-FUSIVEL-1"));
	REQUIRE(reread.accessories.size() == 1);
	CHECK(reread.accessories.first().code == QStringLiteral("FUSIVEL-2A"));

	// A catalog newer than the running build is refused, not worked on.
	const QString newer_connection = QStringLiteral("catalog_newer_test");
	{
		QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
								  newer_connection);
		database.setDatabaseName(file);
		REQUIRE(database.open());
		REQUIRE(CatalogSchema::setMeta(database, QStringLiteral("schema_version"),
					       QString::number(CatalogSchema::currentVersion() + 5)));
		database.close();
	}
	QSqlDatabase::removeDatabase(newer_connection);

	Catalog too_new;
	error.clear();
	CHECK_FALSE(too_new.open(file, &error));
	CHECK_FALSE(error.isEmpty());
	CHECK_FALSE(too_new.isOpen());
}

TEST_CASE("Catalog - une classe qui porte des pièces ou des filles ne s'efface pas")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	QString error;

	const int contactor_id = classId(catalog, "contactor");
	CatalogPart part(QStringLiteral("NAO-APAGA-1"), contactor_id);
	REQUIRE(catalog.savePart(part, &error));

	error.clear();
	CHECK_FALSE(catalog.removeClass(contactor_id, &error));
	CHECK_FALSE(error.isEmpty());

	error.clear();
	CHECK_FALSE(catalog.removeClass(classId(catalog, "component"), &error));
	CHECK_FALSE(error.isEmpty());

	// A class with neither parts nor subclasses goes away.
	CatalogClass empty(QStringLiteral("vazia"), QStringLiteral("Classe vazia"));
	empty.parent_id = classId(catalog, "component");
	const int empty_id = catalog.addClass(empty, &error);
	REQUIRE(empty_id > 0);
	CHECK(catalog.removeClass(empty_id, &error));
	CHECK(catalog.classById(empty_id).isNull());

	// And a class cannot become its own descendant.
	CatalogClass component = catalog.classById(classId(catalog, "component"));
	component.parent_id = contactor_id;
	error.clear();
	CHECK_FALSE(catalog.updateClass(component, &error));
	CHECK_FALSE(error.isEmpty());
}

TEST_CASE("Catalog - retirer une pièce promeut la révision qui reste")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	QString error;

	const int breaker_id = classId(catalog, "breaker");
	CatalogPart part(QStringLiteral("PROMOVE-1"), breaker_id);
	REQUIRE(catalog.savePart(part, &error));
	CatalogPart second = part;
	REQUIRE(catalog.savePartAsNewRevision(second, &error));
	REQUIRE(second.revision == 2);

	REQUIRE(catalog.removePart(second.id, &error));
	const CatalogPart current = catalog.partByCode(QStringLiteral("PROMOVE-1"));
	REQUIRE_FALSE(current.isNull());
	CHECK(current.revision == 1);
	CHECK(current.is_current);
}

/*
	T30 — the pinout a generated block is built from. What is proved here is
	the data: the roles a card of a controller needs, the channel that groups
	the pins of one point of the field, and the fact that all of it survives
	the catalog and the package. The block itself is drawn in step 3.
*/
TEST_CASE("CU-30.4 — un point à deux fils est un canal, et le canal se garde")
{
	OpenCatalog fixture;
	Catalog &catalog = fixture.catalog;
	QString error;

	const int plc_id = classId(catalog, "plc");

		//A two wire digital input: one point of the field, two pins, and the
		//two pins do not share a role. That is what the pair cannot say, and
		//why the channel is a field of its own.
	CatalogPart card(QStringLiteral("CLP-DI16"), plc_id);
	CatalogPin input(QStringLiteral("I0.0"), CatalogPinRole::Input);
	input.channel = QStringLiteral("DI0");
	input.secondary_label = QStringLiteral("STOP");
	input.connector = QStringLiteral("X1");
	CatalogPin common(QStringLiteral("1M"), CatalogPinRole::ReturnCommon);
	common.channel = QStringLiteral("DI0");
	common.secondary_label = QStringLiteral("COM");
	common.connector = QStringLiteral("X1");
	CatalogPin analog(QStringLiteral("AI0+"), CatalogPinRole::InputAnalog);
	analog.channel = QStringLiteral("AI0");
	analog.connector = QStringLiteral("X2");
	CatalogPin supply(QStringLiteral("L+"), CatalogPinRole::SupplyCommon);
	card.pins.append(input);
	card.pins.append(common);
	card.pins.append(analog);
	card.pins.append(supply);

	SECTION("um ponto de dois fios é um canal com dois pinos de papéis diferentes")
	{
		CHECK(card.channelKeys() == QStringList({ QStringLiteral("DI0"),
							  QStringLiteral("AI0") }));

		const QList<CatalogPin> point = card.pinsInChannel(QStringLiteral("DI0"));
		REQUIRE(point.size() == 2);
		CHECK(point.at(0).role == CatalogPinRole::Input);
		CHECK(point.at(1).role == CatalogPinRole::ReturnCommon);

			//The pair stayed empty on purpose. Grouping by pair would ask
			//the symbol generator for two terminals of one role, and this
			//point has two roles: the block would not be saveable.
		CHECK(point.at(0).pair.isEmpty());
		CHECK(point.at(1).pair.isEmpty());

		CHECK(CatalogPin::isIoRole(CatalogPinRole::Input));
		CHECK(CatalogPin::isIoRole(CatalogPinRole::OutputRelay));
		CHECK_FALSE(CatalogPin::isIoRole(CatalogPinRole::ReturnCommon));
		CHECK_FALSE(CatalogPin::isIoRole(CatalogPinRole::CommPort));

			//A pin with no channel is not a point of the field.
		CHECK(card.pinsInChannel(QString()).isEmpty());
		CHECK(card.pinsInChannel(QStringLiteral("DI9")).isEmpty());
		CHECK(card.channelKeys().size() == 2);
	}

	SECTION("os tipos novos vão e voltam pelo nome, nunca pelo número")
	{
		const QList<CatalogPinRole> roles = CatalogPin::allRoles();
		for (const CatalogPinRole role : roles)
		{
			CHECK(CatalogPin::roleFromString(CatalogPin::roleToString(role)) == role);
			CHECK_FALSE(CatalogPin::translatedRoleName(role).isEmpty());
		}
		CHECK(roles.size() == 14);
		CHECK(CatalogPin::roleToString(CatalogPinRole::ReturnCommon)
		      == QStringLiteral("return_common"));
		CHECK(CatalogPin::roleToString(CatalogPinRole::OutputRelay)
		      == QStringLiteral("output_relay"));

			//The eight roles that existed before still read the same, which
			//is what lets an old catalog be opened by this build.
		CHECK(CatalogPin::roleFromString(QStringLiteral("coil")) == CatalogPinRole::Coil);
		CHECK(CatalogPin::roleFromString(QStringLiteral("input")) == CatalogPinRole::Input);
		CHECK(CatalogPin::roleFromString(QStringLiteral("nada")) == CatalogPinRole::Unknown);
	}

	SECTION("o catálogo guarda rótulo, canal e conector")
	{
		REQUIRE(catalog.savePart(card, &error));
		REQUIRE(error.isEmpty());

		const CatalogPart reread = catalog.partByCode(QStringLiteral("CLP-DI16"));
		REQUIRE_FALSE(reread.isNull());
		REQUIRE(reread.pins.size() == 4);
		CHECK(reread.channelKeys() == QStringList({ QStringLiteral("DI0"),
							    QStringLiteral("AI0") }));

		const QList<CatalogPin> point = reread.pinsInChannel(QStringLiteral("DI0"));
		REQUIRE(point.size() == 2);
		CHECK(point.at(0).label == QStringLiteral("I0.0"));
		CHECK(point.at(0).secondary_label == QStringLiteral("STOP"));
		CHECK(point.at(0).connector == QStringLiteral("X1"));
		CHECK(point.at(1).secondary_label == QStringLiteral("COM"));
		CHECK(point.at(1).role == CatalogPinRole::ReturnCommon);
		CHECK(reread.pinsWithRole(CatalogPinRole::InputAnalog).size() == 1);
		CHECK(reread.pinsWithRole(CatalogPinRole::SupplyCommon).size() == 1);

			//The pin that has nothing to say keeps saying nothing.
		CHECK(reread.pinsWithRole(CatalogPinRole::SupplyCommon)
		      .first().channel.isEmpty());
	}

	SECTION("o pacote da peça leva a pinagem para outra máquina")
	{
		QTemporaryDir directory;
		REQUIRE(directory.isValid());
		const QString path = directory.filePath(CatalogPackage::suggestedFileName(card));
		REQUIRE(CatalogPackage::write(path, catalog, card, &error));
		REQUIRE(error.isEmpty());

		const CatalogPart returned = CatalogPackage::read(path, catalog, &error);
		REQUIRE(error.isEmpty());
		REQUIRE(returned.pins.size() == 4);

		const QList<CatalogPin> point = returned.pinsInChannel(QStringLiteral("DI0"));
		REQUIRE(point.size() == 2);
		CHECK(point.at(0).secondary_label == QStringLiteral("STOP"));
		CHECK(point.at(0).connector == QStringLiteral("X1"));
		CHECK(point.at(1).role == CatalogPinRole::ReturnCommon);
		CHECK(returned.pinsWithRole(CatalogPinRole::InputAnalog).size() == 1);

			//Nothing is written for a pin that has nothing to say, so a part
			//with no pinout keeps exactly the file it had. That is why the
			//format version does not move: an older build reads these files
			//and ignores the three attributes it does not know.
		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
		const QString written = QString::fromUtf8(file.readAll());
		file.close();
		CHECK(written.contains(QStringLiteral("version=\"1\"")));
		CHECK(written.contains(QStringLiteral("channel=\"DI0\"")));
		CHECK(written.contains(QStringLiteral("connector=\"X2\"")));
		CHECK_FALSE(written.contains(QStringLiteral("channel=\"\"")));
		CHECK_FALSE(written.contains(QStringLiteral("secondary-label=\"\"")));
	}
}
