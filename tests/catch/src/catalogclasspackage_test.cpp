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
#include "../../../sources/catalog/catalogclass.h"
#include "../../../sources/catalog/catalogclasspackage.h"
#include "../../../sources/catalog/catalogproperty.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QFileInfo>
#include <QTemporaryDir>

namespace
{
	/**
		@brief A catalog holding the branch the ACME needs and no other
		catalog has: a power supply class, a DC subclass under it, typed
		properties and one controlled list.

		Built on a class the seeded tree does not have on purpose - a class
		that every catalog already carries would never prove that the file
		creates anything.
	*/
	struct BranchFixture
	{
		BranchFixture()
		{
			QString error;
			REQUIRE(source.openInMemory(&error));

			const int component_id = source.classByKey(QStringLiteral("component")).id;
			REQUIRE(component_id > 0);

			REQUIRE(source.setListValues(QStringLiteral("curva_disjuntor"),
						     { QStringLiteral("B"),
						       QStringLiteral("C"),
						       QStringLiteral("D") },
						     &error));

			CatalogClass supply(QStringLiteral("power_supply"), QStringLiteral("Fonte"));
			supply.parent_id = component_id;
			supply.root = QStringLiteral("PS");
			supply.root_iec = QStringLiteral("T");
			supply.description = QStringLiteral("Fonte chaveada de painel");
			supply.numbering_format = QStringLiteral("<numbering_format name=\"fonte\"/>");
			supply_id = source.addClass(supply, &error);
			REQUIRE(supply_id > 0);

			CatalogClass supply_dc(QStringLiteral("power_supply_dc"),
					       QStringLiteral("Fonte DC/DC"));
			supply_dc.parent_id = supply_id;
			supply_dc.has_symbol = false;
			supply_dc_id = source.addClass(supply_dc, &error);
			REQUIRE(supply_dc_id > 0);

			CatalogProperty current;
			current.class_id = supply_id;
			current.key = QStringLiteral("corrente");
			current.name = QStringLiteral("Corrente");
			current.type = CatalogPropertyType::Measure;
			current.unit = QStringLiteral("A");
			current.default_value = QStringLiteral("0");
			REQUIRE(source.addProperty(current, &error) > 0);

			CatalogProperty power;
			power.class_id = supply_id;
			power.key = QStringLiteral("potencia");
			power.name = QStringLiteral("Potência");
			power.type = CatalogPropertyType::Measure;
			power.unit = QStringLiteral("W");
			REQUIRE(source.addProperty(power, &error) > 0);

			CatalogProperty curve;
			curve.class_id = supply_id;
			curve.key = QStringLiteral("curva");
			curve.name = QStringLiteral("Curva");
			curve.type = CatalogPropertyType::Text;
			curve.list_name = QStringLiteral("curva_disjuntor");
			curve.list_behaviour = CatalogListBehaviour::Mandatory;
			REQUIRE(source.addProperty(curve, &error) > 0);

			CatalogProperty output;
			output.class_id = supply_dc_id;
			output.key = QStringLiteral("tensao_saida");
			output.name = QStringLiteral("Tensão de saída");
			output.type = CatalogPropertyType::Measure;
			output.unit = QStringLiteral("V");
			REQUIRE(source.addProperty(output, &error) > 0);

			REQUIRE(directory.isValid());
			file_path = directory.filePath(QStringLiteral("fonte.qetclasses"));
			REQUIRE(CatalogClassPackage::write(file_path, source, supply_id, &error));
		}

		Catalog source;
		QTemporaryDir directory;
		QString file_path;
		int supply_id = 0;
		int supply_dc_id = 0;
	};
}

TEST_CASE("CU-12.10 — a árvore de classes é um arquivo", "[catalog]")
{
	BranchFixture fixture;

	SECTION("importar o ramo num catálogo que não tem a classe")
	{
		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));
		REQUIRE(target.classByKey(QStringLiteral("power_supply")).isNull());

		CatalogClassPackage::Report report;
		REQUIRE(CatalogClassPackage::read(fixture.file_path, target, &report, &error));
		CHECK(error.isEmpty());
		CHECK(report.refused.isEmpty());

			//Fonte and Fonte DC/DC. The ancestry travels along and is
			//already there, which is what "déjà présente" counts.
		CHECK(report.classes_created == 2);
		CHECK(report.classes_found >= 2);
		CHECK(report.properties_created == 4);
		CHECK(report.lists_created == 1);
		CHECK_FALSE(report.changesNothing());

		const CatalogClass supply = target.classByKey(QStringLiteral("power_supply"));
		REQUIRE_FALSE(supply.isNull());
		CHECK(supply.name == QStringLiteral("Fonte"));
		CHECK(supply.parent_id == target.classByKey(QStringLiteral("component")).id);
		CHECK(supply.root == QStringLiteral("PS"));
		CHECK(supply.root_iec == QStringLiteral("T"));
		CHECK(supply.description == QStringLiteral("Fonte chaveada de painel"));
		CHECK(supply.numbering_format
		      == QStringLiteral("<numbering_format name=\"fonte\"/>"));

		const CatalogClass supply_dc = target.classByKey(QStringLiteral("power_supply_dc"));
		REQUIRE_FALSE(supply_dc.isNull());
		CHECK(supply_dc.parent_id == supply.id);
		CHECK_FALSE(supply_dc.has_symbol);

			//A property is a declaration, not a name: the type, the unit
			//and the initial value are the point of carrying it.
		const CatalogProperty current =
				target.effectiveProperty(supply.id, QStringLiteral("corrente"));
		REQUIRE_FALSE(current.isNull());
		CHECK(current.type == CatalogPropertyType::Measure);
		CHECK(current.unit == QStringLiteral("A"));
		CHECK(current.default_value == QStringLiteral("0"));
		CHECK(current.name == QStringLiteral("Corrente"));

		const CatalogProperty curve =
				target.effectiveProperty(supply.id, QStringLiteral("curva"));
		REQUIRE_FALSE(curve.isNull());
		CHECK(curve.list_name == QStringLiteral("curva_disjuntor"));
		CHECK(curve.list_behaviour == CatalogListBehaviour::Mandatory);
			//A mandatory list whose values did not travel is a field
			//nobody can fill.
		CHECK(target.listValues(QStringLiteral("curva_disjuntor"))
		      == QStringList({ QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D") }));
		CHECK(curve.options
		      == QStringList({ QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D") }));

			//The subclass inherits what the parent declares, which is the
			//whole reason the parent came along.
		const CatalogProperty inherited =
				target.effectiveProperty(supply_dc.id, QStringLiteral("potencia"));
		REQUIRE_FALSE(inherited.isNull());
		CHECK(inherited.unit == QStringLiteral("W"));
	}

	SECTION("importar duas vezes não duplica nada")
	{
		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));
		REQUIRE(CatalogClassPackage::read(fixture.file_path, target, nullptr, &error));

		const int classes_after_first = target.classes().size();
		const int properties_after_first =
				target.effectiveProperties(
					target.classByKey(QStringLiteral("power_supply_dc")).id).size();

		CatalogClassPackage::Report again;
		REQUIRE(CatalogClassPackage::read(fixture.file_path, target, &again, &error));
		CHECK(again.classes_created == 0);
		CHECK(again.properties_created == 0);
		CHECK(again.lists_created == 0);
		CHECK(again.changesNothing());
		CHECK(again.refused.isEmpty());
		CHECK(target.classes().size() == classes_after_first);
		CHECK(target.effectiveProperties(
			      target.classByKey(QStringLiteral("power_supply_dc")).id).size()
		      == properties_after_first);
	}

	SECTION("a identidade é a chave, não o uuid")
	{
		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));
		REQUIRE(CatalogClassPackage::read(fixture.file_path, target, nullptr, &error));

			//Every catalog draws its own uuid, so a branch recognised by
			//uuid would be a branch created twice.
		const CatalogClass here = target.classByKey(QStringLiteral("power_supply"));
		const CatalogClass there = fixture.source.classById(fixture.supply_id);
		REQUIRE_FALSE(here.uuid.isEmpty());
		REQUIRE_FALSE(there.uuid.isEmpty());
		CHECK(here.uuid != there.uuid);

			//And the seeded classes prove it on their own: both catalogs
			//have Composant, with different uuids.
		CHECK(target.classByKey(QStringLiteral("component")).uuid
		      != fixture.source.classByKey(QStringLiteral("component")).uuid);
	}

	SECTION("propriedade que já existe com outro tipo é nomeada, não sobrescrita")
	{
		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));

		CatalogClass supply(QStringLiteral("power_supply"), QStringLiteral("Fonte"));
		supply.parent_id = target.classByKey(QStringLiteral("component")).id;
		const int supply_id = target.addClass(supply, &error);
		REQUIRE(supply_id > 0);

		CatalogProperty typed_as_text;
		typed_as_text.class_id = supply_id;
		typed_as_text.key = QStringLiteral("corrente");
		typed_as_text.name = QStringLiteral("Corrente");
		typed_as_text.type = CatalogPropertyType::Text;
		REQUIRE(target.addProperty(typed_as_text, &error) > 0);

		CatalogClassPackage::Report report;
		REQUIRE(CatalogClassPackage::read(fixture.file_path, target, &report, &error));

			//Named, and the import went on: the rest of the branch is
			//still worth having.
		REQUIRE(report.refused.size() == 1);
		CHECK(report.refused.first().contains(QStringLiteral("corrente")));
		CHECK(report.classes_created == 1);
		CHECK(report.properties_created == 3);

			//Changing the type of a property reinterprets the value of
			//every part that uses it, so it stays as it was.
		CHECK(target.effectiveProperty(supply_id, QStringLiteral("corrente")).type
		      == CatalogPropertyType::Text);
	}

	SECTION("o plano diz de antemão exatamente o que vai ser criado")
	{
		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));

		const CatalogClassPackage::Report plan =
				CatalogClassPackage::summary(fixture.file_path, target, &error);
		CHECK(error.isEmpty());
		CHECK(plan.missing_classes
		      == QStringList({ QStringLiteral("Fonte"), QStringLiteral("Fonte DC/DC") }));
		CHECK_FALSE(plan.toText().isEmpty());

		CatalogClassPackage::Report done;
		REQUIRE(CatalogClassPackage::read(fixture.file_path, target, &done, &error));

			//A dialog that promises two classes and creates something
			//else is worse than no dialog at all.
		CHECK(done.classes_created == plan.classes_created);
		CHECK(done.classes_found == plan.classes_found);
		CHECK(done.properties_created == plan.properties_created);
		CHECK(done.properties_found == plan.properties_found);
		CHECK(done.lists_created == plan.lists_created);
		CHECK(done.missing_classes == plan.missing_classes);
		CHECK(done.refused == plan.refused);
	}

	SECTION("classe sem lugar é recusada e o resto do ramo entra")
	{
		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));

		QDomDocument document;
		QDomElement root = document.createElement(CatalogClassPackage::blockTagName());
		root.setAttribute(QStringLiteral("version"), QStringLiteral("1"));

		QDomElement orphan = document.createElement(QStringLiteral("class"));
		orphan.setAttribute(QStringLiteral("key"), QStringLiteral("orfa"));
		orphan.setAttribute(QStringLiteral("name"), QStringLiteral("Órfã"));
		orphan.setAttribute(QStringLiteral("parent-key"), QStringLiteral("nao_existe"));
		root.appendChild(orphan);

		QDomElement placed = document.createElement(QStringLiteral("class"));
		placed.setAttribute(QStringLiteral("key"), QStringLiteral("power_supply"));
		placed.setAttribute(QStringLiteral("name"), QStringLiteral("Fonte"));
		placed.setAttribute(QStringLiteral("parent-key"), QStringLiteral("component"));
		root.appendChild(placed);
		document.appendChild(root);

		CatalogClassPackage::Report report;
		REQUIRE(CatalogClassPackage::applyXml(root, target, &report, &error));
		CHECK(report.classes_created == 1);
		REQUIRE(report.refused.size() == 1);
		CHECK(report.refused.first().contains(QStringLiteral("nao_existe")));
		CHECK(target.classByKey(QStringLiteral("orfa")).isNull());
		CHECK_FALSE(target.classByKey(QStringLiteral("power_supply")).isNull());
	}

	SECTION("um arquivo que não é nosso é recusado sem tocar no catálogo")
	{
		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));
		const int before = target.classes().size();

		QDomDocument document;
		QDomElement root = document.createElement(QStringLiteral("qet-catalog-part"));
		document.appendChild(root);

		CHECK_FALSE(CatalogClassPackage::applyXml(root, target, nullptr, &error));
		CHECK_FALSE(error.isEmpty());
		CHECK(target.classes().size() == before);
	}

	SECTION("a árvore da ACME entregue entra num catálogo semeado")
	{
			//This reads the file that actually ships,
			//todo/exemplos/classes-acme.qetclasses, and not a copy of it
			//written here. A copy would keep passing after the shipped file
			//broke, which is the failure mode that makes a green suite
			//worthless.
			//
			//The branch is the one the ACME project of 14 folios needs:
			//nine classes no seeded tree has, and typed properties in place
			//of the free text that carries the ratings today.
		const QString path = QStringLiteral(QET_TEST_DATA_DIR) +
				     QStringLiteral("/classes-acme.qetclasses");
		REQUIRE(QFileInfo::exists(path));

		Catalog target;
		QString error;
		REQUIRE(target.openInMemory(&error));

		const CatalogClassPackage::Report plan =
				CatalogClassPackage::summary(path, target, &error);
		REQUIRE(error.isEmpty());
		CHECK(plan.classes_created == 9);
			//The five anchors travel with the file so that it also works on
			//a catalog that is not the ACME one; here they are
			//recognised by key instead of duplicated.
		CHECK(plan.classes_found == 5);
		CHECK(plan.properties_created == 42);
		CHECK(plan.properties_found == 0);
		CHECK(plan.lists_created == 11);
		CHECK(plan.lists_found == 0);
		CHECK(plan.refused.isEmpty());

		CatalogClassPackage::Report done;
		REQUIRE(CatalogClassPackage::read(path, target, &done, &error));
			//What was announced is what got written.
		CHECK(done.classes_created == plan.classes_created);
		CHECK(done.classes_found == plan.classes_found);
		CHECK(done.properties_created == plan.properties_created);
		CHECK(done.lists_created == plan.lists_created);
		CHECK(done.refused.isEmpty());

			//The house tag and the IEC letter are the point of the branch:
			//the drawing says PS2, the parts list and the standard say T.
		const CatalogClass supply = target.classByKey(QStringLiteral("power_supply"));
		REQUIRE_FALSE(supply.isNull());
		CHECK(supply.root == QStringLiteral("PS"));
		CHECK(supply.root_iec == QStringLiteral("T"));
		CHECK(supply.parent_id == target.classByKey(QStringLiteral("component")).id);

			//Two classes, same house tag, different IEC letter: an auxiliary
			//relay processes a signal (K) and a monitoring relay protects
			//(F). The project draws both as RL and that is why the letter
			//has to live on the class.
		const CatalogClass control = target.classByKey(QStringLiteral("control_relay"));
		const CatalogClass monitoring = target.classByKey(QStringLiteral("monitoring_relay"));
		REQUIRE_FALSE(control.isNull());
		REQUIRE_FALSE(monitoring.isNull());
		CHECK(control.root == monitoring.root);
		CHECK(control.root_iec == QStringLiteral("K"));
		CHECK(monitoring.root_iec == QStringLiteral("F"));

			//A rating is a number with a unit. The project keeps "20A / 480W"
			//in one comment today; here it is two Decimal properties, and
			//Decimal and not Measure because Measure means a length in
			//millimetre.
		const CatalogProperty current =
				target.effectiveProperty(supply.id, QStringLiteral("corrente_saida"));
		REQUIRE(current.id > 0);
		CHECK(current.type == CatalogPropertyType::Decimal);
		CHECK(current.unit == QStringLiteral("A"));
		const CatalogProperty power =
				target.effectiveProperty(supply.id, QStringLiteral("potencia"));
		REQUIRE(power.id > 0);
		CHECK(power.type == CatalogPropertyType::Decimal);
		CHECK(power.unit == QStringLiteral("W"));

			//A property declared on a class the catalog already had: the
			//file adds to Disjoncteur without touching its name or prefix.
		const CatalogClass breaker = target.classByKey(QStringLiteral("breaker"));
		REQUIRE_FALSE(breaker.isNull());
		CHECK(breaker.root_iec == QStringLiteral("Q"));
		const CatalogProperty curve =
				target.effectiveProperty(breaker.id, QStringLiteral("curva"));
		REQUIRE(curve.id > 0);
		CHECK(curve.list_behaviour == CatalogListBehaviour::Mandatory);
		CHECK(curve.list_name == QStringLiteral("Curva de disjuntor"));
			//The list arrived before the property that reads it, so the
			//values are already in the field and not an empty mandatory box.
		CHECK(curve.options == QStringList({ QStringLiteral("B"),
						     QStringLiteral("C"),
						     QStringLiteral("D"),
						     QStringLiteral("K"),
						     QStringLiteral("Z") }));

			//And the inherited one, because a breaker has a rated current
			//like every other component: declared once on Composant.
		const CatalogProperty rated =
				target.effectiveProperty(breaker.id, QStringLiteral("corrente_nominal"));
		REQUIRE(rated.id > 0);
		CHECK(rated.class_id == target.classByKey(QStringLiteral("component")).id);
		CHECK(rated.unit == QStringLiteral("A"));

			//Loading the office tree twice is a thing that happens on a
			//shared catalog. The second time has to be a no-op.
		CatalogClassPackage::Report again;
		REQUIRE(CatalogClassPackage::read(path, target, &again, &error));
		CHECK(again.changesNothing());
	}
}
