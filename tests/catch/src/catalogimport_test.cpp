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
#include "../../../sources/catalog/catalogclasspackage.h"
#include "../../../sources/catalog/catalogimport.h"
#include "../../../sources/catalog/catalogpackage.h"
#include "../../../sources/catalog/catalogproperty.h"
#include "../../../sources/catalog/catalogtablereader.h"
#include "qt_catch_tostring.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
	struct ImportFixture
	{
		ImportFixture()
		{
			QString error;
			REQUIRE(catalog.openInMemory(&error));
			contactor_id = catalog.classByKey(QStringLiteral("contactor")).id;
			breaker_id = catalog.classByKey(QStringLiteral("breaker")).id;
			REQUIRE(contactor_id > 0);
			REQUIRE(breaker_id > 0);

			// A price and a lead time, the two fields a supplier list updates
			// every month, added the way the office would add them.
			CatalogProperty price(QString(), QStringLiteral("Preço"),
					      CatalogPropertyType::Currency);
			price.class_id = catalog.classByKey(QStringLiteral("component")).id;
			REQUIRE(catalog.addProperty(price, &error) > 0);

			CatalogProperty lead_time(QString(), QStringLiteral("Prazo"),
						  CatalogPropertyType::Integer);
			lead_time.class_id = price.class_id;
			REQUIRE(catalog.addProperty(lead_time, &error) > 0);
		}

		CatalogImportProfile profileFor(const CatalogTable &table) const
		{
			CatalogImportProfile profile;
			profile.class_key = QStringLiteral("contactor");
			profile.code_column = QStringLiteral("codigo");
			profile.value_columns.insert(QStringLiteral("designation"),
						     QStringLiteral("descricao"));
			profile.value_columns.insert(QStringLiteral("manufacturer"),
						     QStringLiteral("fabricante"));
			profile.value_columns.insert(QStringLiteral("preco"),
						     QStringLiteral("preco"));
			profile.value_columns.insert(QStringLiteral("prazo"),
						     QStringLiteral("prazo"));
			Q_UNUSED(table)
			return profile;
		}

		Catalog catalog;
		int contactor_id = 0;
		int breaker_id = 0;
	};

	void writeText(const QString &path, const QString &content)
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write(content.toUtf8());
	}
}

TEST_CASE("CU-14.1 — le CSV réel se laisse lire, virgules dans les champs comprises")
{
	// A description with a semicolon in it, a quoted field, a doubled quote,
	// CRLF, and the blank lines every spreadsheet export leaves at the end.
	const QString text = QStringLiteral(
		"codigo;descricao;fabricante;preco;prazo\r\n"
		"CONT-9A;\"Contator 9 A; bobina 24 Vcc\";Fornecedor A;123,45;10\r\n"
		"CONT-12A;\"Contator 12 A, \"\"padrão\"\" da casa\";Fornecedor A;150,00;15\r\n"
		"\r\n"
		";;;;\r\n");

	const CatalogTable table = CatalogTableReader::parseCsv(text);
	CHECK(table.headers == QStringList({ QStringLiteral("codigo"),
					     QStringLiteral("descricao"),
					     QStringLiteral("fabricante"),
					     QStringLiteral("preco"),
					     QStringLiteral("prazo") }));
	REQUIRE(table.rowCount() == 2);

	// The semicolon inside quotes did not split the row - which is what broke
	// on the first real list and is why this is a character parser.
	CHECK(table.value(0, QStringLiteral("descricao"))
	      == QStringLiteral("Contator 9 A; bobina 24 Vcc"));
	CHECK(table.value(1, QStringLiteral("descricao"))
	      == QStringLiteral("Contator 12 A, \"padrão\" da casa"));
	CHECK(table.value(1, QStringLiteral("prazo")) == QStringLiteral("15"));

	// Header lookup ignores case and spaces, because a header typed by hand
	// does neither.
	CHECK(table.value(0, QStringLiteral("  CODIGO ")) == QStringLiteral("CONT-9A"));
	CHECK(table.columnIndex(QStringLiteral("nao-existe")) == -1);
	CHECK(table.value(0, QStringLiteral("nao-existe")).isEmpty());
}

TEST_CASE("CU-14.1 — le délimiteur se devine, et le point-virgule passe avant la virgule")
{
	// A machine set to Portuguese writes semicolons, because the comma is the
	// decimal separator. Guessing comma first would split every price.
	CHECK(CatalogTableReader::detectDelimiter(
		      QStringLiteral("codigo;descricao;preco\nA;B;1,50\n")) == QLatin1Char(';'));
	CHECK(CatalogTableReader::detectDelimiter(
		      QStringLiteral("code,description,price\nA,B,1.50\n")) == QLatin1Char(','));
	CHECK(CatalogTableReader::detectDelimiter(
		      QStringLiteral("code\tdescription\tprice\n")) == QLatin1Char('\t'));

	// A quoted delimiter is not a delimiter.
	CHECK(CatalogTableReader::detectDelimiter(
		      QStringLiteral("\"a,b,c,d\";segundo\n")) == QLatin1Char(';'));
}

TEST_CASE("CU-14.1 — le rapport d'importation dit ce qui est entré")
{
	ImportFixture fixture;
	const QString text = QStringLiteral(
		"codigo;descricao;fabricante;preco;prazo\n"
		"CONT-9A;Contator 9 A;Fornecedor A;123,45;10\n"
		"CONT-12A;Contator 12 A;Fornecedor A;150,00;15\n"
		"CONT-18A;Contator 18 A;Fornecedor B;190,00;20\n");

	const CatalogTable table = CatalogTableReader::parseCsv(text);
	const CatalogImportProfile profile = fixture.profileFor(table);

	const CatalogImportReport report =
		CatalogImporter::import(fixture.catalog, table, profile,
					QStringLiteral("planilha-de-material"));

	CHECK(report.created == 3);
	CHECK(report.updated == 0);
	CHECK(report.rejected() == 0);
	CHECK(report.total() == 3);
	CHECK(report.changedAnything());
	CHECK(fixture.catalog.partCount() == 3);
	CHECK_FALSE(report.toText().isEmpty());

	const CatalogPart part = fixture.catalog.partByCode(QStringLiteral("CONT-12A"));
	REQUIRE_FALSE(part.isNull());
	CHECK(part.value(QStringLiteral("designation")) == QStringLiteral("Contator 12 A"));
	CHECK(part.value(QStringLiteral("manufacturer")) == QStringLiteral("Fornecedor A"));
	CHECK(part.class_id == fixture.contactor_id);

	// Provenance: which file, and when. It is what says whether a wrong value
	// is the source's mistake or the typist's.
	CHECK(part.origin == QStringLiteral("planilha-de-material"));
	CHECK_FALSE(part.origin_date.isEmpty());
}

TEST_CASE("CU-14.2 — le profil de mapping se sauve et se relit")
{
	ImportFixture fixture;
	CatalogImportProfile profile;
	profile.name = QStringLiteral("Distribuidor X");
	profile.class_key = QStringLiteral("contactor");
	profile.code_column = QStringLiteral("codigo");
	profile.class_column = QStringLiteral("classe");
	profile.policy = CatalogDuplicatePolicy::NewRevision;
	profile.delimiter = QLatin1Char('|');
	profile.value_columns.insert(QStringLiteral("designation"), QStringLiteral("descricao"));
	profile.value_columns.insert(QStringLiteral("preco"), QStringLiteral("preco"));

	const CatalogImportProfile reread = CatalogImportProfile::fromXml(profile.toXml());
	CHECK(reread.name == profile.name);
	CHECK(reread.class_key == profile.class_key);
	CHECK(reread.class_column == profile.class_column);
	CHECK(reread.code_column == profile.code_column);
	CHECK(reread.policy == CatalogDuplicatePolicy::NewRevision);
	CHECK(reread.delimiter == QLatin1Char('|'));
	CHECK(reread.value_columns == profile.value_columns);

	// A profile without a code column is refused, and says why.
	CatalogImportProfile broken;
	broken.class_key = QStringLiteral("contactor");
	QString error;
	CHECK_FALSE(broken.isValid(&error));
	CHECK_FALSE(error.isEmpty());

	// And one without any destination class either.
	CatalogImportProfile no_class;
	no_class.code_column = QStringLiteral("codigo");
	error.clear();
	CHECK_FALSE(no_class.isValid(&error));
	CHECK_FALSE(error.isEmpty());

	// The guess fills what is obvious so the user corrects a few instead of
	// typing them all. It matches a header against the technical key of a
	// property or against its user visible name - the first is the case of a
	// sheet exported from this very catalog, the second of a sheet somebody
	// wrote by hand in the language of the interface.
	const CatalogTable by_key = CatalogTableReader::parseCsv(
		QStringLiteral("codigo;designation;manufacturer;preco\nA;B;C;1\n"));
	const CatalogImportProfile guessed =
		CatalogImportProfile::guess(fixture.catalog, fixture.contactor_id, by_key);
	CHECK(guessed.code_column == QStringLiteral("codigo"));
	CHECK(guessed.class_key == QStringLiteral("contactor"));
	CHECK(guessed.value_columns.value(QStringLiteral("designation"))
	      == QStringLiteral("designation"));
	CHECK(guessed.value_columns.value(QStringLiteral("manufacturer"))
	      == QStringLiteral("manufacturer"));

	// By name: the seeded properties carry the very names QElectroTech itself
	// shows for those keys, so this reads whatever the program calls the
	// manufacturer field - not a name the catalog invented for it.
	const CatalogProperty manufacturer =
		fixture.catalog.effectiveProperty(fixture.contactor_id,
						  QStringLiteral("manufacturer"));
	REQUIRE_FALSE(manufacturer.isNull());
	const CatalogTable by_name = CatalogTableReader::parseCsv(
		QStringLiteral("codigo;%1\nA;B\n").arg(manufacturer.name));
	const CatalogImportProfile by_name_guess =
		CatalogImportProfile::guess(fixture.catalog, fixture.contactor_id, by_name);
	CHECK(by_name_guess.value_columns.value(QStringLiteral("manufacturer"))
	      == manufacturer.name);
}

TEST_CASE("CU-14.3 — réimporter un tarif ne touche ni les bornes ni les accessoires")
{
	ImportFixture fixture;
	QString error;

	// A part already registered with everything a part carries.
	CatalogPart part(QStringLiteral("CONT-9A"), fixture.contactor_id);
	part.setValue(QStringLiteral("designation"), QStringLiteral("Contator 9 A"));
	part.setValue(QStringLiteral("width"), QStringLiteral("45"));
	part.setValue(QStringLiteral("preco"), QStringLiteral("100,00"));
	part.pins.append(CatalogPin(QStringLiteral("A1"), CatalogPinRole::Coil));
	part.pins.append(CatalogPin(QStringLiteral("A2"), CatalogPinRole::Coil));
	part.accessories.append(CatalogAccessory(QStringLiteral("BLOCO-AUX-1"), 1));
	REQUIRE(fixture.catalog.savePart(part, &error));

	// The price list of the month: two columns, nothing else.
	const CatalogTable table = CatalogTableReader::parseCsv(
		QStringLiteral("codigo;preco;prazo\nCONT-9A;123,45;10\n"));
	CatalogImportProfile profile;
	profile.class_key = QStringLiteral("contactor");
	profile.code_column = QStringLiteral("codigo");
	profile.value_columns.insert(QStringLiteral("preco"), QStringLiteral("preco"));
	profile.value_columns.insert(QStringLiteral("prazo"), QStringLiteral("prazo"));
	profile.policy = CatalogDuplicatePolicy::Update;

	const CatalogImportReport report =
		CatalogImporter::import(fixture.catalog, table, profile,
					QStringLiteral("tabela-de-preco-agosto"));
	CHECK(report.updated == 1);
	CHECK(report.created == 0);

	const CatalogPart reread = fixture.catalog.partByCode(QStringLiteral("CONT-9A"));
	REQUIRE_FALSE(reread.isNull());
	CHECK(reread.value(QStringLiteral("preco")) == QStringLiteral("123,45"));
	CHECK(reread.value(QStringLiteral("prazo")) == QStringLiteral("10"));

	// Everything the price list did not mention is still there. This is the
	// half of the use case a naive importer destroys.
	CHECK(reread.value(QStringLiteral("designation")) == QStringLiteral("Contator 9 A"));
	CHECK(reread.value(QStringLiteral("width")) == QStringLiteral("45"));
	CHECK(reread.pins.size() == 2);
	REQUIRE(reread.accessories.size() == 1);
	CHECK(reread.accessories.first().code == QStringLiteral("BLOCO-AUX-1"));
	CHECK(reread.revision == 1);
}

TEST_CASE("CU-14.4 — une ligne invalide est refusée avec son motif, et l'import continue")
{
	ImportFixture fixture;

	const CatalogTable table = CatalogTableReader::parseCsv(
		QStringLiteral("codigo;classe;descricao\n"
			       "CONT-9A;contactor;Contator 9 A\n"
			       ";contactor;Sem referência nenhuma\n"
			       "DISJ-25A;classe-que-nao-existe;Disjuntor 25 A\n"
			       "CONT-12A;contactor;Contator 12 A\n"));

	CatalogImportProfile profile;
	profile.code_column = QStringLiteral("codigo");
	profile.class_column = QStringLiteral("classe");
	profile.value_columns.insert(QStringLiteral("designation"), QStringLiteral("descricao"));

	const CatalogImportReport report =
		CatalogImporter::import(fixture.catalog, table, profile,
					QStringLiteral("planilha-com-erro"));

	// The import went to the end: the two good rows are in.
	CHECK(report.created == 2);
	CHECK(report.rejected() == 2);
	CHECK(fixture.catalog.partCount() == 2);
	CHECK_FALSE(fixture.catalog.partByCode(QStringLiteral("CONT-12A")).isNull());

	// Each rejection carries a row number the user can find in the
	// spreadsheet, and a reason they can act on.
	REQUIRE(report.rejections.size() == 2);
	CHECK(report.rejections.at(0).row == 3);
	CHECK_FALSE(report.rejections.at(0).reason.isEmpty());
	CHECK(report.rejections.at(1).row == 4);
	CHECK(report.rejections.at(1).code == QStringLiteral("DISJ-25A"));
	CHECK(report.rejections.at(1).reason.contains(QStringLiteral("classe-que-nao-existe")));

	// And the report says all of it in one text.
	const QString text = report.toText();
	CHECK(text.contains(QStringLiteral("DISJ-25A")));
}

TEST_CASE("CU-14.6 — duplicité : ignorer, mettre à jour, nouvelle révision")
{
	ImportFixture fixture;
	QString error;

	CatalogPart part(QStringLiteral("CONT-9A"), fixture.contactor_id);
	part.setValue(QStringLiteral("designation"), QStringLiteral("Original"));
	REQUIRE(fixture.catalog.savePart(part, &error));

	const CatalogTable table = CatalogTableReader::parseCsv(
		QStringLiteral("codigo;descricao\nCONT-9A;Vindo da planilha\n"));

	CatalogImportProfile profile;
	profile.class_key = QStringLiteral("contactor");
	profile.code_column = QStringLiteral("codigo");
	profile.value_columns.insert(QStringLiteral("designation"), QStringLiteral("descricao"));

	SECTION("ignorer garde ce que le catalogue a")
	{
		profile.policy = CatalogDuplicatePolicy::Ignore;
		const CatalogImportReport report =
			CatalogImporter::import(fixture.catalog, table, profile, QStringLiteral("x"));
		CHECK(report.ignored == 1);
		CHECK(report.updated == 0);
		CHECK_FALSE(report.changedAnything());
		CHECK(fixture.catalog.partByCode(QStringLiteral("CONT-9A"))
		      .value(QStringLiteral("designation")) == QStringLiteral("Original"));
	}

	SECTION("mettre à jour écrase en place")
	{
		profile.policy = CatalogDuplicatePolicy::Update;
		const CatalogImportReport report =
			CatalogImporter::import(fixture.catalog, table, profile, QStringLiteral("x"));
		CHECK(report.updated == 1);
		CHECK(fixture.catalog.partRevisions(QStringLiteral("CONT-9A")).size() == 1);
		CHECK(fixture.catalog.partByCode(QStringLiteral("CONT-9A"))
		      .value(QStringLiteral("designation")) == QStringLiteral("Vindo da planilha"));
	}

	SECTION("nouvelle révision garde l'ancienne donnée")
	{
		profile.policy = CatalogDuplicatePolicy::NewRevision;
		const CatalogImportReport report =
			CatalogImporter::import(fixture.catalog, table, profile, QStringLiteral("x"));
		CHECK(report.revised == 1);
		CHECK(fixture.catalog.partRevisions(QStringLiteral("CONT-9A"))
		      == QList<int>({ 1, 2 }));
		CHECK(fixture.catalog.partByCode(QStringLiteral("CONT-9A"), 1)
		      .value(QStringLiteral("designation")) == QStringLiteral("Original"));
		CHECK(fixture.catalog.partByCode(QStringLiteral("CONT-9A"))
		      .value(QStringLiteral("designation")) == QStringLiteral("Vindo da planilha"));
	}
}

TEST_CASE("CU-14.5 — aller et retour en paquet, sans le symbole et sans le prix")
{
	ImportFixture fixture;
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	QString error;

	CatalogPart part(QStringLiteral("CONT-9A/24VCC"), fixture.contactor_id);
	part.setValue(QStringLiteral("designation"), QStringLiteral("Contator 9 A"));
	part.setValue(QStringLiteral("manufacturer"), QStringLiteral("Fornecedor A"));
	part.setValue(QStringLiteral("width"), QStringLiteral("45"));
	part.setValue(QStringLiteral("preco"), QStringLiteral("123,45"));
	part.pins.append(CatalogPin(QStringLiteral("A1"), CatalogPinRole::Coil));
	part.pins.append(CatalogPin(QStringLiteral("A2"), CatalogPinRole::Coil));
	CatalogPin contact(QStringLiteral("13"), CatalogPinRole::ContactNo);
	contact.pair = QStringLiteral("13-14");
	contact.group = QStringLiteral("contato_na.elmt");
	part.pins.append(contact);
	part.accessories.append(CatalogAccessory(QStringLiteral("BLOCO-AUX-1"), 2));
	REQUIRE(fixture.catalog.savePart(part, &error));

	// The file name comes from the code, with the slash made safe.
	const QString file_name = CatalogPackage::suggestedFileName(part);
	CHECK(file_name == QStringLiteral("CONT-9A_24VCC.qetpart"));
	const QString file = directory.filePath(file_name);

	REQUIRE(CatalogPackage::write(file, fixture.catalog, part, &error));
	CHECK(error.isEmpty());

	// What the package must not carry, checked on the file itself and not on
	// the code that wrote it.
	QFile written(file);
	REQUIRE(written.open(QIODevice::ReadOnly | QIODevice::Text));
	const QString content = QString::fromUtf8(written.readAll());
	written.close();
	CHECK_FALSE(content.contains(QStringLiteral("123,45")));
	CHECK_FALSE(content.contains(QStringLiteral("\"preco\"")));
	CHECK_FALSE(content.contains(QStringLiteral(".elmt\"><")));    // no symbol definition
	CHECK(content.contains(QStringLiteral("class-key=\"contactor\"")));
	CHECK(CatalogPackage::classKeyOf(file) == QStringLiteral("contactor"));

	// Delete it from the catalog and bring it back from the package.
	REQUIRE(fixture.catalog.removePart(fixture.catalog.partByCode(part.code).id, &error));
	CHECK(fixture.catalog.partByCode(part.code).isNull());

	CatalogPart returned = CatalogPackage::read(file, fixture.catalog, &error);
	REQUIRE_FALSE(returned.isNull());
	CHECK(returned.class_id == fixture.contactor_id);
	CHECK(returned.code == QStringLiteral("CONT-9A/24VCC"));
	CHECK(returned.value(QStringLiteral("designation")) == QStringLiteral("Contator 9 A"));
	CHECK(returned.value(QStringLiteral("manufacturer")) == QStringLiteral("Fornecedor A"));
	CHECK(returned.value(QStringLiteral("width")) == QStringLiteral("45"));
	CHECK(returned.value(QStringLiteral("preco")).isEmpty());
	REQUIRE(returned.pins.size() == 3);
	CHECK(returned.pins.at(0).label == QStringLiteral("A1"));
	CHECK(returned.pins.at(2).pair == QStringLiteral("13-14"));
	CHECK(returned.pins.at(2).group == QStringLiteral("contato_na.elmt"));
	REQUIRE(returned.accessories.size() == 1);
	CHECK(returned.accessories.first().code == QStringLiteral("BLOCO-AUX-1"));
	CHECK(returned.accessories.first().quantity == 2.0);

	// Where it came from, and when.
	CHECK(returned.origin.startsWith(QStringLiteral("package:")));
	CHECK_FALSE(returned.origin_date.isEmpty());

	REQUIRE(fixture.catalog.savePart(returned, &error));
	CHECK(fixture.catalog.partCount() == 1);
}

TEST_CASE("CU-14.5 — un paquet dont la classe manque ici est lu, pas jeté")
{
	ImportFixture fixture;
	QTemporaryDir directory;
	REQUIRE(directory.isValid());

	const QString file = directory.filePath(QStringLiteral("estranho.qetpart"));
	writeText(file, QStringLiteral(
			  "<qet-catalog-part version=\"1\" code=\"XYZ-1\" revision=\"3\" "
			  "class-key=\"classe_que_nao_existe\" class-name=\"Coisa\">\n"
			  " <property key=\"designation\">Peça de fora</property>\n"
			  " <pin label=\"1\" role=\"terminal\" order=\"1\"/>\n"
			  "</qet-catalog-part>\n"));

	QString error;
	const CatalogPart part = CatalogPackage::read(file, fixture.catalog, &error);

	// The data is there, the class is not resolved, and the reason says which
	// class is missing - the caller can offer to create it, which beats
	// refusing the file.
	CHECK(part.code == QStringLiteral("XYZ-1"));
	CHECK(part.class_id == 0);
	CHECK(part.value(QStringLiteral("designation")) == QStringLiteral("Peça de fora"));
	CHECK(error.contains(QStringLiteral("classe_que_nao_existe")));

	// The revision of the sender is not kept: this catalog numbers its own.
	CHECK(part.revision == 1);

	// And a file that is not a package at all is refused with a reason.
	const QString not_a_package = directory.filePath(QStringLiteral("qualquer.qetpart"));
	writeText(not_a_package, QStringLiteral("<html><body>nope</body></html>"));
	error.clear();
	CHECK(CatalogPackage::read(not_a_package, fixture.catalog, &error).isNull());
	CHECK_FALSE(error.isEmpty());
}

TEST_CASE("CU-14.11 — o pacote leva a classe declarada, e ela nasce tipada do outro lado",
	  "[catalog]")
{
	ImportFixture fixture;
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	QString error;

	// A breaker class declared the way the office declares one: a measure with
	// its unit, a mandatory controlled list, and a count.
	REQUIRE(fixture.catalog.setListValues(QStringLiteral("curva_disjuntor"),
					      QStringList({ QStringLiteral("B"),
							    QStringLiteral("C"),
							    QStringLiteral("D") }), &error));

	CatalogProperty current(QStringLiteral("corrente"), QStringLiteral("Corrente"),
				CatalogPropertyType::Measure);
	current.class_id = fixture.breaker_id;
	current.unit = QStringLiteral("A");
	current.default_value = QStringLiteral("0");
	REQUIRE(fixture.catalog.addProperty(current, &error) > 0);

	CatalogProperty curve(QStringLiteral("curva"), QStringLiteral("Curva"),
			      CatalogPropertyType::Text);
	curve.class_id = fixture.breaker_id;
	curve.list_name = QStringLiteral("curva_disjuntor");
	curve.list_behaviour = CatalogListBehaviour::Mandatory;
	REQUIRE(fixture.catalog.addProperty(curve, &error) > 0);

	CatalogProperty poles(QStringLiteral("polos"), QStringLiteral("Polos"),
			      CatalogPropertyType::Integer);
	poles.class_id = fixture.breaker_id;
	REQUIRE(fixture.catalog.addProperty(poles, &error) > 0);

	// A subclass the sender happens to have below the class. It must not
	// travel: a package carries the class of its part, not the tree of
	// whoever exported it.
	CatalogClass motor(QStringLiteral("breaker_motor"), QStringLiteral("Disjuntor motor"));
	motor.parent_id = fixture.breaker_id;
	REQUIRE(fixture.catalog.addClass(motor, &error) > 0);

	CatalogPart part(QStringLiteral("DISJ-3P-25A"), fixture.breaker_id);
	part.setValue(QStringLiteral("designation"), QStringLiteral("Disjuntor 25 A"));
	part.setValue(QStringLiteral("corrente"), QStringLiteral("25"));
	part.setValue(QStringLiteral("curva"), QStringLiteral("C"));
	part.setValue(QStringLiteral("polos"), QStringLiteral("3"));
	REQUIRE(fixture.catalog.savePart(part, &error));

	const QString file = directory.filePath(QStringLiteral("disjuntor.qetpart"));
	REQUIRE(CatalogPackage::write(file, fixture.catalog, part, &error));

	QFile written(file);
	REQUIRE(written.open(QIODevice::ReadOnly | QIODevice::Text));
	const QString content = QString::fromUtf8(written.readAll());
	written.close();

	// What is in the file: the declaration of the class, its ancestry, and
	// neither the subclass nor the commercial field. The price does not
	// travel, and neither does the column that would hold one.
	CHECK(content.contains(QStringLiteral("<qet-catalog-classes")));
	CHECK(content.contains(QStringLiteral("key=\"breaker\"")));
	CHECK(content.contains(QStringLiteral("unit=\"A\"")));
	CHECK_FALSE(content.contains(QStringLiteral("breaker_motor")));
	CHECK_FALSE(content.contains(QStringLiteral("preco")));
	CHECK_FALSE(content.contains(QStringLiteral("Preço")));

	// The receiver: the same tree with the breaker class taken out, so that
	// the class really is missing and not merely named otherwise.
	const QString class_name = fixture.catalog.classById(fixture.breaker_id).name;
	Catalog target;
	REQUIRE(target.openInMemory(&error));
	REQUIRE(target.removeClass(target.classByKey(QStringLiteral("breaker")).id, &error));
	REQUIRE(target.classByKey(QStringLiteral("breaker")).isNull());

	// Reading the package still creates nothing by itself, and still says
	// which class is missing.
	CatalogPart arrived = CatalogPackage::read(file, target, &error);
	REQUIRE_FALSE(arrived.isNull());
	CHECK(arrived.class_id == 0);
	CHECK(error.contains(QStringLiteral("breaker")));
	CHECK(target.classByKey(QStringLiteral("breaker")).isNull());

	// What the dialog says before asking, said without writing anything.
	CatalogClassPackage::Report plan;
	error.clear();
	REQUIRE(CatalogPackage::classPlan(file, target, &plan, &error));
	CHECK(plan.classes_created == 1);
	CHECK(plan.classes_found == 2);
	CHECK(plan.properties_created == 3);
	CHECK(plan.lists_created == 1);
	CHECK(plan.missing_classes == QStringList({ class_name }));
	CHECK(target.classByKey(QStringLiteral("breaker")).isNull());

	// The "yes": what was declared is created, where it belongs.
	CatalogClassPackage::Report done;
	REQUIRE(CatalogPackage::applyClass(file, target, &done, &error));
	CHECK(done.refused.isEmpty());
	CHECK(done.classes_created == 1);

	const CatalogClass created = target.classByKey(QStringLiteral("breaker"));
	REQUIRE_FALSE(created.isNull());
	CHECK(created.parent_id == target.classByKey(QStringLiteral("component")).id);
	CHECK(created.root == QStringLiteral("Q"));
	CHECK(created.name == class_name);
	CHECK(target.classByKey(QStringLiteral("breaker_motor")).isNull());

	// Typed fields, not an empty class: the unit, the list and the count are
	// the difference between a value and a piece of loose text.
	const CatalogProperty typed = target.effectiveProperty(created.id,
							       QStringLiteral("corrente"));
	REQUIRE_FALSE(typed.isNull());
	CHECK(typed.type == CatalogPropertyType::Measure);
	CHECK(typed.unit == QStringLiteral("A"));
	CHECK(typed.default_value == QStringLiteral("0"));

	const CatalogProperty listed = target.effectiveProperty(created.id,
							       QStringLiteral("curva"));
	REQUIRE_FALSE(listed.isNull());
	CHECK(listed.list_name == QStringLiteral("curva_disjuntor"));
	CHECK(listed.list_behaviour == CatalogListBehaviour::Mandatory);
	CHECK(target.listValues(QStringLiteral("curva_disjuntor"))
	      == QStringList({ QStringLiteral("B"), QStringLiteral("C"),
			       QStringLiteral("D") }));

	CHECK(target.effectiveProperty(created.id, QStringLiteral("polos")).type
	      == CatalogPropertyType::Integer);

	// And the commercial field of the sender did not come along with the class.
	CHECK(target.effectiveProperty(created.id, QStringLiteral("preco")).isNull());

	// Now the same file resolves, and the values of the part land in those
	// fields instead of under an empty node.
	error.clear();
	arrived = CatalogPackage::read(file, target, &error);
	CHECK(error.isEmpty());
	REQUIRE(arrived.class_id == created.id);
	REQUIRE(target.savePart(arrived, &error));

	const CatalogPart saved = target.partByCode(QStringLiteral("DISJ-3P-25A"));
	REQUIRE_FALSE(saved.isNull());
	CHECK(saved.class_id == created.id);
	CHECK(saved.value(QStringLiteral("corrente")) == QStringLiteral("25"));
	CHECK(saved.value(QStringLiteral("curva")) == QStringLiteral("C"));
	CHECK(saved.value(QStringLiteral("polos")) == QStringLiteral("3"));

	// Importing the same package twice changes nothing the second time.
	CatalogClassPackage::Report again;
	REQUIRE(CatalogPackage::classPlan(file, target, &again, &error));
	CHECK(again.changesNothing());

	// A package written before this existed names its class and declares
	// nothing. That is not an error: it is the answer the dialog needs to
	// offer the empty class instead of promising a declaration.
	const QString old_package = directory.filePath(QStringLiteral("antigo.qetpart"));
	writeText(old_package, QStringLiteral(
			  "<qet-catalog-part version=\"1\" code=\"XYZ-2\" "
			  "class-key=\"classe_que_nao_existe\" class-name=\"Coisa\">\n"
			  " <property key=\"designation\">Peça antiga</property>\n"
			  "</qet-catalog-part>\n"));
	error.clear();
	CHECK_FALSE(CatalogPackage::classPlan(old_package, target, nullptr, &error));
	CHECK(error.isEmpty());
	CHECK_FALSE(CatalogPackage::applyClass(old_package, target, nullptr, &error));
	CHECK_FALSE(error.isEmpty());
}

TEST_CASE("CU-14.1 — le catalogue ressort en planilha, et le rond-point se referme")
{
	ImportFixture fixture;
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	QString error;

	for (int index = 0 ; index < 4 ; ++index)
	{
		CatalogPart part(QStringLiteral("CONT-%1").arg(index), fixture.contactor_id);
		part.setValue(QStringLiteral("designation"),
			      QStringLiteral("Contator com ; ponto e vírgula %1").arg(index));
		part.setValue(QStringLiteral("preco"), QStringLiteral("10,50"));
		REQUIRE(fixture.catalog.savePart(part, &error));
	}

	const CatalogTable exported =
		CatalogImporter::exportToTable(fixture.catalog, fixture.contactor_id);
	CHECK(exported.rowCount() == 4);
	CHECK(exported.columnIndex(QStringLiteral("code")) >= 0);
	CHECK(exported.columnIndex(QStringLiteral("revision")) >= 0);
	CHECK(exported.columnIndex(QStringLiteral("designation")) >= 0);

	const QString file = directory.filePath(QStringLiteral("catalogo.csv"));
	REQUIRE(CatalogTableReader::writeCsv(file, exported, QLatin1Char(';'), &error));

	// Reading back what was written has to give the same thing, semicolons
	// inside a description included - otherwise the export is a trap for
	// whoever tries to re-import it.
	const CatalogTable reread = CatalogTableReader::readCsv(file, QLatin1Char(';'), &error);
	CHECK(reread.headers == exported.headers);
	REQUIRE(reread.rowCount() == exported.rowCount());
	CHECK(reread.value(2, QStringLiteral("designation"))
	      == exported.value(2, QStringLiteral("designation")));
	CHECK(reread.value(2, QStringLiteral("designation")).contains(QLatin1Char(';')));
}

TEST_CASE("CU-14.1 — data de planilha chega como data, não como número", "[catalog]")
{
	CatalogProperty date_property(QStringLiteral("entrega"),
				      QStringLiteral("Data de entrega"),
				      CatalogPropertyType::Date);
	CatalogProperty text_property(QStringLiteral("obs"),
				      QStringLiteral("Observação"),
				      CatalogPropertyType::Text);

	SECTION("a época é 30/12/1899, e isto é onde o erro de um dia mora")
	{
			//Serial 1 é 31/12/1899 e não 01/01/1900, porque o formato conta um
			//29/02/1900 que nunca existiu. Fixado aqui para ninguém "corrigir".
		CHECK(CatalogProperty::dateFromSpreadsheetSerial(1) ==
		      QDate(1899, 12, 31));
		CHECK(CatalogProperty::dateFromSpreadsheetSerial(45292) ==
		      QDate(2024, 1, 1));
	}

	SECTION("número de série numa coluna de data vira data")
	{
		CHECK(date_property.fromSpreadsheetCell(QStringLiteral("45292")) ==
		      QStringLiteral("2024-01-01"));
	}

	SECTION("data já escrita como texto fica como está")
	{
			//Quem digitou sabe o formato que quis; converter seria adivinhar.
		CHECK(date_property.fromSpreadsheetCell(QStringLiteral("01/01/2024")) ==
		      QStringLiteral("01/01/2024"));
		CHECK(date_property.fromSpreadsheetCell(QString()).isEmpty());
	}

	SECTION("número que não é data plausível fica visível como número")
	{
			//Melhor um número que se vê que está errado do que uma data do
			//ano 1900 que passa desapercebida.
		CHECK(date_property.fromSpreadsheetCell(QStringLiteral("0")) ==
		      QStringLiteral("0"));
		CHECK(date_property.fromSpreadsheetCell(QStringLiteral("99999999")) ==
		      QStringLiteral("99999999"));
		CHECK_FALSE(CatalogProperty::dateFromSpreadsheetSerial(0).isValid());
	}

	SECTION("coluna que não é de data não é tocada")
	{
		CHECK(text_property.fromSpreadsheetCell(QStringLiteral("45292")) ==
		      QStringLiteral("45292"));
	}
}

TEST_CASE("CU-14.1 — as peças do projeto real entram, e o arquivo entregue é o testado",
	  "[catalog][dados-reais]")
{
	// This reads the file that actually ships, todo/exemplos/pecas-do-projeto.csv,
	// and not a copy of it written here. The distinction is the point: a copy
	// would keep passing after the shipped file broke, which is the failure mode
	// that makes a green suite worthless.
	//
	// The rows are the 11 distinct part codes found in the ACME project of
	// 14 folios, taken from the designation and manufacturer fields that were
	// already filled in it. So this test answers a question no invented fixture
	// can: does the real list, with its accents, its uppercase, its decimal
	// commas and its plus signs inside a code, import without a single rejection.
	//
	// One asymmetry is deliberate: the description keeps the comma the office
	// typed ("6,2A"), while the typed columns write the point ("6.2"). The
	// catalog converts a Decimal with toDouble(), which is not locale aware, so
	// a comma in a typed column stores without complaint and reads back as
	// nothing. Do not "fix" the sheet to commas.
	const QString path = QStringLiteral(QET_TEST_DATA_DIR) +
			     QStringLiteral("/pecas-do-projeto.csv");
	QFile file(path);
	REQUIRE(file.open(QIODevice::ReadOnly));
	const QString text = QString::fromUtf8(file.readAll());
	file.close();

	const CatalogTable table = CatalogTableReader::parseCsv(text);
		//Nineteen columns for eleven parts, and most cells empty: a power
		//supply has no tripping curve and a limit switch has no output
		//current. The sheet is the union of what the classes declare, which
		//is what a purchasing list looks like once it stops being one class.
	REQUIRE(table.headers.size() == 19);
	REQUIRE(table.rowCount() == 11);

	Catalog catalog;
	QString error;
	REQUIRE(catalog.openInMemory(&error));

		//The classes come first, from the file that ships next to this one.
		//The order is the real one: a part cannot be typed before the class
		//that declares the type exists. Importing the parts into a bare
		//seeded catalog would put every value in as loose text, which is
		//exactly the state the project is being taken out of.
	REQUIRE(CatalogClassPackage::read(QStringLiteral(QET_TEST_DATA_DIR) +
					 QStringLiteral("/classes-acme.qetclasses"),
					 catalog, nullptr, &error));

	CatalogImportProfile profile;
	// The class comes from a column, and the column carries the stable key and
	// not the visible name. The name is translated when the tree is seeded, so a
	// file saying "Componente" would import here and be refused on a machine
	// running in French - the key never moves.
	profile.class_column = QStringLiteral("classe");
	profile.code_column = QStringLiteral("codigo");
	profile.value_columns.insert(QStringLiteral("manufacturer"),
				     QStringLiteral("manufacturer"));
	profile.value_columns.insert(QStringLiteral("designation"),
				     QStringLiteral("designation"));
	profile.value_columns.insert(QStringLiteral("description"),
				     QStringLiteral("description"));

		//The typed columns are named after the property key they fill, so
		//one loop is the whole mapping. A supplier sheet needs the mapping
		//dialog; the sheet the office writes itself does not.
	const QStringList typed_columns = { QStringLiteral("tensao_entrada"),
					    QStringLiteral("tensao_saida"),
					    QStringLiteral("corrente_saida"),
					    QStringLiteral("tensao_alimentacao"),
					    QStringLiteral("fases"),
					    QStringLiteral("frenagem"),
					    QStringLiteral("filtro_emc"),
					    QStringLiteral("grandeza_monitorada"),
					    QStringLiteral("tipo_sensor"),
					    QStringLiteral("interface"),
					    QStringLiteral("acionamento"),
					    QStringLiteral("contatos_na"),
					    QStringLiteral("contatos_nf"),
					    QStringLiteral("trava") };
	for (const QString &key : typed_columns) {
		profile.value_columns.insert(key, key);
	}

	const CatalogImportReport report = CatalogImporter::import(
				catalog, table, profile,
				QStringLiteral("projeto CT1-QCM"));

	// Not a single rejection, and the reasons printed when there is one - a
	// count alone would say "10 of 11" and leave whoever reads it guessing.
	for (const CatalogImportReport::Rejection &rejection : report.rejections) {
		WARN(QString("linha %1 (%2): %3").arg(rejection.row)
		     .arg(rejection.code, rejection.reason).toStdString());
	}
	CHECK(report.rejected() == 0);
	CHECK(report.created == 11);

	SECTION("a classe de cada peça é a que o arquivo diz")
	{
			//Every part now lands on the class that actually describes it, and
			//four of these classes did not exist before the branch was read.
			//"component" as a class was the honest answer while the tree was
			//flat; it is the wrong answer now that there is a tree.
		const int module_id = catalog.classByKey(QStringLiteral("plc_module")).id;
		const int button_id = catalog.classByKey(QStringLiteral("push_button")).id;
		const int limit_id = catalog.classByKey(QStringLiteral("limit_switch")).id;
		const int relay_id = catalog.classByKey(QStringLiteral("monitoring_relay")).id;
		const int supply_id = catalog.classByKey(QStringLiteral("power_supply")).id;
		REQUIRE(module_id > 0);
		REQUIRE(limit_id > 0);

		CHECK(catalog.partByCode(QStringLiteral("750-670")).class_id == module_id);
		CHECK(catalog.partByCode(
			      QStringLiteral("CSW-CWC3F45 WH + AF3F + BC10F-CSW"))
		      .class_id == button_id);
		CHECK(catalog.partByCode(QStringLiteral("LSW-PF14ALP11")).class_id
		      == limit_id);
		CHECK(catalog.partByCode(QStringLiteral("RPW-PTCE05")).class_id == relay_id);
		CHECK(catalog.partByCode(QStringLiteral("787-1200")).class_id == supply_id);

			//A module is a kind of PLC, so it answers to the PLC class too:
			//asking the catalog for automates has to keep finding it.
		CHECK(catalog.classById(module_id).parent_id
		      == catalog.classByKey(QStringLiteral("plc")).id);
	}

	SECTION("o que era frase virou valor com tipo, unidade e lista")
	{
			//This is the whole point of the exercise. The project carried
			//"RELÉ MONITORAMENTO TERMISTOR MOTOR" in a description and 220V in
			//a comment; nothing could be searched, summed or checked. Here the
			//same three facts are three typed fields.
		const CatalogPart relay = catalog.partByCode(QStringLiteral("RPW-PTCE05"));
		REQUIRE_FALSE(relay.isNull());
		CHECK(relay.values.value(QStringLiteral("grandeza_monitorada"))
		      == QStringLiteral("Temperatura do motor"));
		CHECK(relay.values.value(QStringLiteral("tipo_sensor"))
		      == QStringLiteral("Termistor PTC"));
		CHECK(relay.values.value(QStringLiteral("tensao_alimentacao"))
		      == QStringLiteral("220"));

			//And it reads back as a number, which is the difference between a
			//field and a caption. The separator is a point on purpose: the
			//catalog converts with toDouble(), which is not locale aware, so a
			//comma here would store fine and read back as nothing.
		const int inverter_id =
				catalog.classByKey(QStringLiteral("frequency_inverter")).id;
		const CatalogProperty output_current =
				catalog.effectiveProperty(inverter_id,
							  QStringLiteral("corrente_saida"));
		REQUIRE(output_current.id > 0);
		CHECK(output_current.unit == QStringLiteral("A"));

		const CatalogPart inverter =
				catalog.partByCode(QStringLiteral("CFW500B06P2T4DB20C2"));
		REQUIRE_FALSE(inverter.isNull());
		CHECK(output_current.toVariant(
			      inverter.values.value(QStringLiteral("corrente_saida")))
		      .toDouble() == Approx(6.2));
		CHECK(inverter.values.value(QStringLiteral("frenagem")) == QStringLiteral("1"));
		CHECK(inverter.values.value(QStringLiteral("filtro_emc")) == QStringLiteral("C3"));

			//A mandatory list refused nothing, because every value written in
			//the sheet is one of the offered ones. That is what the import
			//would have caught: a typo in a controlled field is a rejection,
			//not a silently odd value.
		const CatalogPart single_phase =
				catalog.partByCode(QStringLiteral("CFW500A02P6B2NB20"));
		REQUIRE_FALSE(single_phase.isNull());
		CHECK(single_phase.values.value(QStringLiteral("fases"))
		      == QStringLiteral("Monofásico ou trifásico"));
		const CatalogProperty phases =
				catalog.effectiveProperty(inverter_id, QStringLiteral("fases"));
		CHECK(phases.list_behaviour == CatalogListBehaviour::Mandatory);
		CHECK_FALSE(phases.isOutsideList(
				single_phase.values.value(QStringLiteral("fases"))));

			//An empty cell is not a value: the limit switch has no actuation
			//written in the project, and a mandatory list tolerates that. A
			//guessed "Rolete" would look like measured data forever.
		const CatalogPart limit = catalog.partByCode(QStringLiteral("LSW-PF14ALP11"));
		REQUIRE_FALSE(limit.isNull());
		CHECK_FALSE(limit.values.contains(QStringLiteral("acionamento")));
	}

	SECTION("o texto atravessa inteiro: acento, maiúscula e vírgula decimal")
	{
		// "6,2A" inside the description is the case that would break if the
		// delimiter had been guessed as the comma. The guess is tested
		// elsewhere; here it is tested on the file that ships.
		const CatalogPart part = catalog.partByCode(
					QStringLiteral("CFW500B06P2T4DB20C2"));
		REQUIRE_FALSE(part.isNull());
		CHECK(part.values.value(QStringLiteral("description"))
		      .contains(QStringLiteral("6,2A")));
		CHECK(part.values.value(QStringLiteral("description"))
		      .contains(QStringLiteral("FREQUÊNCIA")));
		CHECK(part.values.value(QStringLiteral("manufacturer"))
		      == QStringLiteral("WEG"));
	}

	SECTION("um código com sinal de mais dentro não vira dois códigos")
	{
		// Three of the real codes are a kit written as "A + B + C". Nothing
		// should split them, and nothing should trim them into each other.
		CHECK_FALSE(catalog.partByCode(
				    QStringLiteral("CSW-BESGS WH + AF3F + BC01B-CSW + PBW-1Y"))
			    .isNull());
		CHECK_FALSE(catalog.partByCode(
				    QStringLiteral("CISC-PP21A + ACIS-PFP")).isNull());
		CHECK(catalog.partByCode(QStringLiteral("CISC-PP21A")).isNull());
	}

	SECTION("reimportar o mesmo arquivo não cria peça repetida")
	{
		// The office will import the list again next month. Twice must be the
		// same as once, which is what the Update policy promises.
		const CatalogImportReport again = CatalogImporter::import(
					catalog, table, profile,
					QStringLiteral("projeto CT1-QCM"));
		CHECK(again.created == 0);
		CHECK(again.updated == 11);
		CHECK(catalog.parts(0).size() == 11);
	}
}
