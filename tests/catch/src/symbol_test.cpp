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
#include <catch2/catch.hpp>

#include <QDir>
#include <QFile>
#include <QLineF>
#include <QTemporaryDir>

#include "qt_catch_tostring.h"

#include "../../../sources/ElementsCollection/symbolbuilder.h"
#include "../../../sources/ElementsCollection/symbolgroup.h"

namespace
{
	/**
		The coil of a contactor as somebody would draw it on the sheet: a
		rectangle with a line coming out of the top and one out of the
		bottom, both ending on the main grid.
	*/
	SymbolDefinition drawnCoil()
	{
		SymbolDefinition symbol;
		symbol.name = QStringLiteral("Bobina de contator");
		symbol.class_key = QStringLiteral("contactor");

		SymbolShape body(SymbolShapeType::Rectangle,
				 QPolygonF() << QPointF(90.0, 90.0)
					     << QPointF(110.0, 110.0));
		SymbolShape top(SymbolShapeType::Line,
				QPolygonF() << QPointF(100.0, 90.0)
					    << QPointF(100.0, 80.0));
		SymbolShape bottom(SymbolShapeType::Line,
				   QPolygonF() << QPointF(100.0, 110.0)
					       << QPointF(100.0, 120.0));
		symbol.shapes << body << top << bottom;
		return symbol;
	}
}

TEST_CASE("CU-35.2 — o ponto de ligação cai na grade principal", "[symbol]")
{
	const SymbolGrid grid;

	SECTION("a grade padrão é a de 10, que é a que o condutor usa")
	{
		CHECK(grid.main_step == 10.0);
		CHECK(grid.isValid());
		CHECK(grid.isOnMain(QPointF(20.0, 30.0)));
		CHECK_FALSE(grid.isOnMain(QPointF(23.0, 30.0)));
		CHECK(grid.snapToMain(QPointF(23.0, 26.0)) == QPointF(20.0, 30.0));
	}

	SECTION("o corpo do símbolo não é ponta livre")
	{
		const SymbolDefinition symbol = drawnCoil();
		const QList<SymbolTerminal> suggested = symbol.suggestedTerminals(grid);

			//Duas linhas, quatro pontas — mas as duas que encostam no
			//retângulo são onde o traço entra no corpo do símbolo, e ali não
			//chega condutor nenhum. Sobram duas, que são as certas. Sem isto
			//o projetista apagaria duas linhas da tabela toda vez.
		REQUIRE(suggested.size() == 2);
		CHECK(suggested.at(0).position == QPointF(100.0, 80.0));
		CHECK(suggested.at(1).position == QPointF(100.0, 120.0));
	}

	SECTION("o ponto de ligação aponta para fora do desenho")
	{
			//Seção própria, e não mais asserções na de cima: um REQUIRE que
			//falha aborta a seção inteira, e uma seção que testa duas coisas
			//esconde a segunda quando a primeira quebra. Foi o que aconteceu
			//ao conferir o verde plantando defeito.
		const SymbolDefinition symbol = drawnCoil();
		const QList<SymbolTerminal> suggested = symbol.suggestedTerminals(grid);
		for (const SymbolTerminal &terminal : suggested) {
			if (terminal.position.y() < 100.0) {
					//Acima do corpo: o fio desce até ele, então ele olha para
					//cima. É o que faz o condutor encostar do lado certo.
				CHECK(terminal.orientation == Qet::North);
			} else {
				CHECK(terminal.orientation == Qet::South);
			}
		}
		CHECK_FALSE(suggested.isEmpty());
	}

	SECTION("desenho só de linhas não perde ponta nenhuma")
	{
			//Sem forma fechada não há corpo, então nada é descartado: um
			//contato desenhado com dois traços continua com dois pontos.
		SymbolDefinition symbol;
		symbol.name = QStringLiteral("contato");
		symbol.class_key = QStringLiteral("component");
		symbol.shapes << SymbolShape(SymbolShapeType::Line,
					     QPolygonF() << QPointF(50.0, 50.0)
							 << QPointF(50.0, 60.0));
		symbol.shapes << SymbolShape(SymbolShapeType::Line,
					     QPolygonF() << QPointF(50.0, 80.0)
							 << QPointF(50.0, 90.0));
		const QList<SymbolTerminal> suggested = symbol.suggestedTerminals(grid);
		CHECK(suggested.size() == 4);
	}

	SECTION("ponta fora da grade é empurrada para dentro, e isso é relatado")
	{
		SymbolDefinition symbol;
		symbol.name = QStringLiteral("teste");
		symbol.class_key = QStringLiteral("component");
		symbol.shapes << SymbolShape(SymbolShapeType::Line,
					     QPolygonF() << QPointF(0.0, 0.0)
							 << QPointF(0.0, 23.0));
		symbol.terminals << SymbolTerminal(QPointF(0.0, 23.0), Qet::South);
		symbol.hotspot = QPointF(0.0, 0.0);

		CHECK(symbol.problems(grid).contains(SymbolProblem::TerminalOffGrid));

		const SymbolSnapReport report = symbol.snapToGrid(grid);
		CHECK(report.moved == 1);
		CHECK(report.largest_move == Approx(3.0));
		CHECK(symbol.terminals.first().position == QPointF(0.0, 20.0));
		CHECK_FALSE(symbol.problems(grid).contains(SymbolProblem::TerminalOffGrid));
	}

	SECTION("dois pontos no mesmo lugar são um defeito, não um detalhe")
	{
		SymbolDefinition symbol = drawnCoil();
		symbol.terminals << SymbolTerminal(QPointF(100.0, 80.0), Qet::North);
		symbol.terminals << SymbolTerminal(QPointF(100.0, 80.0), Qet::North);
		symbol.hotspot = QPointF(100.0, 80.0);
		CHECK(symbol.problems(grid).contains(SymbolProblem::TerminalsOverlap));
	}
}

TEST_CASE("CU-35.3 — o símbolo declara os contatos que tem", "[symbol]")
{
	const SymbolGrid grid;
	SymbolDefinition symbol = drawnCoil();
	symbol.terminals.clear();

		//Um NA e um NF, cada um com seus dois pontos emparelhados. É a
		//declaração que a verificação da T24 vai ler.
	SymbolTerminal no_1(QPointF(100.0, 80.0), Qet::North);
	no_1.label = QStringLiteral("13");
	no_1.role = CatalogPinRole::ContactNo;
	no_1.pair = QStringLiteral("na1");

	SymbolTerminal no_2(QPointF(100.0, 120.0), Qet::South);
	no_2.label = QStringLiteral("14");
	no_2.role = CatalogPinRole::ContactNo;
	no_2.pair = QStringLiteral("na1");

	SymbolTerminal nc_1(QPointF(120.0, 80.0), Qet::North);
	nc_1.label = QStringLiteral("21");
	nc_1.role = CatalogPinRole::ContactNc;
	nc_1.pair = QStringLiteral("nf1");

	SymbolTerminal nc_2(QPointF(120.0, 120.0), Qet::South);
	nc_2.label = QStringLiteral("22");
	nc_2.role = CatalogPinRole::ContactNc;
	nc_2.pair = QStringLiteral("nf1");

	symbol.terminals << no_1 << no_2 << nc_1 << nc_2;
	symbol.hotspot = QPointF(100.0, 80.0);

	SECTION("uma paire conta como um contato, não como dois")
	{
		CHECK(symbol.contactCount(CatalogPinRole::ContactNo) == 1);
		CHECK(symbol.contactCount(CatalogPinRole::ContactNc) == 1);
		CHECK(symbol.contactCount(CatalogPinRole::PowerContactNo) == 0);
		CHECK(symbol.pairNames() == QStringList{QStringLiteral("na1"),
							QStringLiteral("nf1")});
		CHECK(symbol.problems(grid).isEmpty());
	}

	SECTION("meia paire é pior que nenhuma: contaria um contato inexistente")
	{
		symbol.terminals.removeLast();
		const QList<SymbolProblem> problems = symbol.problems(grid);
		CHECK(problems.contains(SymbolProblem::PairIncomplete));
	}

	SECTION("os dois lados de uma paire têm de declarar a mesma coisa")
	{
		symbol.terminals[3].role = CatalogPinRole::ContactNo;
		CHECK(symbol.problems(grid).contains(SymbolProblem::PairRoleMismatch));
	}

	SECTION("emparelhar sem dizer o que é não declara nada")
	{
		symbol.terminals[2].role = CatalogPinRole::Unknown;
		symbol.terminals[3].role = CatalogPinRole::Unknown;
		CHECK(symbol.problems(grid).contains(SymbolProblem::PairRoleMissing));
	}

	SECTION("papel sem paire conta como um contato — é o que a folha de dados dá")
	{
		SymbolDefinition simple = drawnCoil();
		simple.terminals.clear();
		SymbolTerminal single(QPointF(100.0, 80.0), Qet::North);
		single.role = CatalogPinRole::PowerContactNo;
		simple.terminals << single;
		CHECK(simple.contactCount(CatalogPinRole::PowerContactNo) == 1);
	}
}

TEST_CASE("CU-35.1 — o símbolo desenhado vira uma definição gravável", "[symbol]")
{
	const SymbolGrid grid;
	SymbolDefinition symbol = drawnCoil();
	symbol.terminals = symbol.suggestedTerminals(grid);
	symbol.hotspot = symbol.suggestedHotspot(grid);
	symbol.texts << SymbolText::tagField(QPointF(112.0, 78.0));

	REQUIRE(symbol.canBeSaved(grid));

	const QDomDocument document = symbol.toXml();
	const QDomElement root = document.documentElement();

	SECTION("é uma definição de elemento que o QET reconhece")
	{
		CHECK(root.tagName() == QStringLiteral("definition"));
		CHECK(root.attribute(QStringLiteral("type")) == QStringLiteral("element"));
			//Os quatro atributos que o Element exige para abrir o arquivo.
		CHECK(root.hasAttribute(QStringLiteral("width")));
		CHECK(root.hasAttribute(QStringLiteral("height")));
		CHECK(root.hasAttribute(QStringLiteral("hotspot_x")));
		CHECK(root.hasAttribute(QStringLiteral("hotspot_y")));
		CHECK(root.attribute(QStringLiteral("link_type")) ==
		      QStringLiteral("simple"));
	}

	SECTION("as coordenadas passam a ser relativas ao ponto de inserção")
	{
		const QDomElement description =
				root.firstChildElement(QStringLiteral("description"));
		REQUIRE_FALSE(description.isNull());

			//O ponto de inserção sugerido é o de cima, em (100, 80). Depois
			//da translação, ele tem de estar na origem.
		CHECK(symbol.hotspot == QPointF(100.0, 80.0));
		bool found_origin = false;
		QDomElement terminal =
				description.firstChildElement(QStringLiteral("terminal"));
		while (!terminal.isNull()) {
			if (terminal.attribute(QStringLiteral("x")).toDouble() == 0.0 &&
					terminal.attribute(QStringLiteral("y")).toDouble() == 0.0) {
				found_origin = true;
			}
			terminal = terminal.nextSiblingElement(QStringLiteral("terminal"));
		}
		CHECK(found_origin);
	}

	SECTION("o nome fica sob pt_BR, e é o único, o que basta")
	{
			//O NamesList cai no primeiro nome da lista quando não reconhece
			//idioma nenhum, então um idioma só aparece em qualquer locale.
			//Inventar um nome em inglês seria inventar, não traduzir.
		const QDomElement names =
				root.firstChildElement(QStringLiteral("names"));
		REQUIRE_FALSE(names.isNull());
		const QDomElement name =
				names.firstChildElement(QStringLiteral("name"));
		REQUIRE_FALSE(name.isNull());
		CHECK(name.attribute(QStringLiteral("lang")) ==
		      QStringLiteral("pt_BR"));
		CHECK(name.text() == QStringLiteral("Bobina de contator"));
		CHECK(name.nextSiblingElement(QStringLiteral("name")).isNull());
	}

	SECTION("a classe do símbolo viaja como informação do elemento")
	{
		const QDomElement informations = root.firstChildElement(
					QStringLiteral("elementInformations"));
		REQUIRE_FALSE(informations.isNull());
		const QDomElement info = informations.firstChildElement(
					QStringLiteral("elementInformation"));
		CHECK(info.attribute(QStringLiteral("name")) ==
		      QStringLiteral("catalog_class"));
		CHECK(info.text() == QStringLiteral("contactor"));
	}

	SECTION("o campo da tag é um texto dinâmico ligado à chave label")
	{
		const QDomElement description =
				root.firstChildElement(QStringLiteral("description"));
		const QDomElement text = description.firstChildElement(
					QStringLiteral("dynamic_text"));
		REQUIRE_FALSE(text.isNull());
		CHECK(text.attribute(QStringLiteral("text_from")) ==
		      QStringLiteral("ElementInfo"));
		CHECK(text.firstChildElement(QStringLiteral("info_name")).text() ==
		      QStringLiteral("label"));
	}

	SECTION("o retângulo sai como rect, a linha como line")
	{
		const QDomElement description =
				root.firstChildElement(QStringLiteral("description"));
		CHECK_FALSE(description.firstChildElement(QStringLiteral("rect")).isNull());
		CHECK_FALSE(description.firstChildElement(QStringLiteral("line")).isNull());
	}
}

TEST_CASE("CU-35.3 — a declaração de contato sobrevive ao arquivo", "[symbol]")
{
	const SymbolGrid grid;
	SymbolDefinition symbol = drawnCoil();
	symbol.terminals.clear();

	SymbolTerminal no_1(QPointF(100.0, 80.0), Qet::North);
	no_1.label = QStringLiteral("13");
	no_1.role = CatalogPinRole::ContactNo;
	no_1.pair = QStringLiteral("na1");
	SymbolTerminal no_2(QPointF(100.0, 120.0), Qet::South);
	no_2.label = QStringLiteral("14");
	no_2.role = CatalogPinRole::ContactNo;
	no_2.pair = QStringLiteral("na1");
	symbol.terminals << no_1 << no_2;
	symbol.hotspot = QPointF(100.0, 80.0);
	symbol.link_type = SymbolLinkType::Master;

	const QDomDocument document = symbol.toXml();
	const SymbolDefinition read =
			SymbolDefinition::fromXml(document.documentElement());

	SECTION("ida e volta preserva papel, paire e rótulo provisório")
	{
		REQUIRE(read.terminals.size() == 2);
		CHECK(read.contactCount(CatalogPinRole::ContactNo) == 1);
		CHECK(read.pairNames() == QStringList{QStringLiteral("na1")});
		CHECK(read.link_type == SymbolLinkType::Master);
		CHECK(read.class_key == QStringLiteral("contactor"));
		CHECK(read.name == QStringLiteral("Bobina de contator"));

			//O rótulo provisório é o do símbolo, não o de um fabricante.
			//Ele é o que a atribuição de peça (T13) substitui depois.
		QStringList labels;
		for (const SymbolTerminal &terminal : read.terminals) {
			labels << terminal.label;
		}
		labels.sort();
		CHECK(labels == QStringList{QStringLiteral("13"), QStringLiteral("14")});
	}

	SECTION("a distância entre os pontos é a que foi desenhada")
	{
			//O arquivo não guarda onde na folha o desenho estava, e ninguém
			//precisa disso. O que tem de voltar igual é a geometria: 40
			//unidades entre um ponto e o outro, como foram desenhados.
		REQUIRE(read.terminals.size() == 2);
		const QPointF drawn =
				symbol.terminals.at(1).position - symbol.terminals.at(0).position;
		const QPointF back =
				read.terminals.at(1).position - read.terminals.at(0).position;
		CHECK(back == drawn);
		CHECK(back == QPointF(0.0, 40.0));
	}

	SECTION("as formas voltam com o mesmo tipo e a mesma geometria")
	{
		REQUIRE(read.shapes.size() == symbol.shapes.size());
		int rectangles = 0, lines = 0;
		for (const SymbolShape &shape : read.shapes) {
			if (shape.type == SymbolShapeType::Rectangle) rectangles++;
			if (shape.type == SymbolShapeType::Line) lines++;
		}
		CHECK(rectangles == 1);
		CHECK(lines == 2);
	}

	SECTION("símbolo sem declaração não ganha atributo nenhum")
	{
		SymbolDefinition plain = drawnCoil();
		plain.terminals << SymbolTerminal(QPointF(100.0, 80.0), Qet::North);
		plain.hotspot = QPointF(100.0, 80.0);
		const QString xml = plain.toXml().toString();
		CHECK_FALSE(xml.contains(QStringLiteral("role=")));
		CHECK_FALSE(xml.contains(QStringLiteral("pair=")));
	}
}

TEST_CASE("as quatro letras da orientação são o formato de arquivo", "[symbol]")
{
		//Não é uma escolha nossa: é o que está escrito em todo .elmt que
		//existe. O teste está aqui para que ninguém "melhore" isso.
	CHECK(SymbolTerminal::orientationToString(Qet::North) == QStringLiteral("n"));
	CHECK(SymbolTerminal::orientationToString(Qet::South) == QStringLiteral("s"));
	CHECK(SymbolTerminal::orientationToString(Qet::East)  == QStringLiteral("e"));
	CHECK(SymbolTerminal::orientationToString(Qet::West)  == QStringLiteral("w"));

	for (Qet::Orientation orientation : SymbolTerminal::allOrientations()) {
		CHECK(SymbolTerminal::orientationFromString(
			      SymbolTerminal::orientationToString(orientation)) ==
		      orientation);
		CHECK_FALSE(SymbolTerminal::translatedOrientation(orientation).isEmpty());
	}
}

TEST_CASE("o símbolo recusa ser gravado quando falta o essencial", "[symbol]")
{
	const SymbolGrid grid;

	SECTION("sem nome, sem classe, sem desenho e sem ponto de ligação")
	{
		SymbolDefinition empty;
		const QList<SymbolProblem> problems = empty.problems(grid);
		CHECK(problems.contains(SymbolProblem::NoName));
		CHECK(problems.contains(SymbolProblem::NoClass));
		CHECK(problems.contains(SymbolProblem::NoShape));
		CHECK(problems.contains(SymbolProblem::NoTerminal));
		CHECK_FALSE(empty.canBeSaved(grid));
	}

	SECTION("linha de comprimento zero não conta como desenho")
	{
		SymbolDefinition symbol;
		symbol.name = QStringLiteral("nada");
		symbol.class_key = QStringLiteral("component");
			//Uma forma que existe no arquivo e em nenhuma tela.
		symbol.shapes << SymbolShape(SymbolShapeType::Line,
					     QPolygonF() << QPointF(10.0, 10.0)
							 << QPointF(10.0, 10.0));
		symbol.terminals << SymbolTerminal(QPointF(10.0, 10.0), Qet::North);
		symbol.hotspot = QPointF(10.0, 10.0);
		CHECK_FALSE(symbol.shapes.first().isValid());
		CHECK(symbol.problems(grid).contains(SymbolProblem::NoShape));
	}

	SECTION("todo problema tem uma frase, senão o diálogo mostra vazio")
	{
		SymbolDefinition empty;
		const QStringList messages = empty.problemMessages(grid);
		CHECK(messages.size() == empty.problems(grid).size());
		for (const QString &message : messages) {
			CHECK_FALSE(message.isEmpty());
		}
	}

	SECTION("o nome do arquivo não carrega acento nem espaço")
	{
		CHECK(SymbolDefinition::fileNameFor(
			      QStringLiteral("Contator 3 polos!")) ==
		      QStringLiteral("contator_3_polos"));
		CHECK(SymbolDefinition::fileNameFor(QStringLiteral("  ")).isEmpty());
	}
}

TEST_CASE("CU-35.6 — o agrupamento traz as peças atribuídas de volta", "[symbol]")
{
		//Um pedaço de folha como o comando de copiar produz: dois
		//componentes, um deles com peça atribuída, e um condutor.
	const QString fragment_xml = QStringLiteral(
		"<diagram>"
		" <elements>"
		"  <element type=\"contactor\" x=\"10\" y=\"20\">"
		"   <elementInformations>"
		"    <elementInformation name=\"part_code\">C-3P-9A</elementInformation>"
		"    <elementInformation name=\"label\">K1</elementInformation>"
		"   </elementInformations>"
		"  </element>"
		"  <element type=\"button\" x=\"10\" y=\"60\">"
		"   <elementInformations>"
		"    <elementInformation name=\"part_code\">B-NA-VD</elementInformation>"
		"   </elementInformations>"
		"  </element>"
		" </elements>"
		" <conductors><conductor num=\"1\"/></conductors>"
		"</diagram>");

	QDomDocument fragment;
	REQUIRE(fragment.setContent(fragment_xml));

	SymbolGroup group = SymbolGroup::fromFragment(
				QStringLiteral("Comando liga-desliga"), fragment);

	SECTION("sabe dizer o que carrega antes de ser inserido")
	{
		CHECK_FALSE(group.isNull());
		CHECK(group.elementCount() == 2);
		CHECK(group.conductorCount() == 1);
		CHECK(group.partCodes() == QStringList{QStringLiteral("B-NA-VD"),
						      QStringLiteral("C-3P-9A")});
	}

	SECTION("gravar e ler devolve o mesmo pedaço de folha")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString path = dir.path() + QStringLiteral("/comando.") +
				SymbolGroup::extension();

		QString error;
		REQUIRE(group.save(path, &error));
		CHECK(error.isEmpty());

		const SymbolGroup read = SymbolGroup::load(path, &error);
		CHECK(error.isEmpty());
		CHECK(read.name == QStringLiteral("Comando liga-desliga"));
		CHECK(read.elementCount() == 2);
		CHECK(read.conductorCount() == 1);
		CHECK(read.partCodes() == group.partCodes());

			//Editar o que foi inserido não altera o gravado: o arquivo foi
			//copiado para a folha, não ligado a ela. Aqui isso aparece como
			//o arquivo continuar inteiro depois de mexer no objeto lido.
		SymbolGroup touched = read;
		touched.name = QStringLiteral("outro nome");
		const SymbolGroup again = SymbolGroup::load(path, &error);
		CHECK(again.name == QStringLiteral("Comando liga-desliga"));
	}

	SECTION("a pasta lista só os agrupamentos")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		QString error;
		REQUIRE(group.save(dir.path() + QStringLiteral("/a.") +
				   SymbolGroup::extension(), &error));
		REQUIRE(group.save(dir.path() + QStringLiteral("/b.") +
				   SymbolGroup::extension(), &error));
		QFile other(dir.path() + QStringLiteral("/nao_e_agrupamento.txt"));
		REQUIRE(other.open(QIODevice::WriteOnly));
		other.write("x");
		other.close();

		CHECK(SymbolGroup::listFolder(dir.path()).size() == 2);
	}

	SECTION("arquivo ilegível é recusado com o motivo, não com um vazio")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString path = dir.path() + QStringLiteral("/quebrado.") +
				SymbolGroup::extension();
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write("<qet-group><diagram>");
		file.close();

		QString error;
		const SymbolGroup broken = SymbolGroup::load(path, &error);
		CHECK(broken.isNull());
		CHECK_FALSE(error.isEmpty());
	}

	SECTION("agrupamento vazio não é gravado")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		SymbolGroup empty;
		QString error;
		CHECK_FALSE(empty.save(dir.path() + QStringLiteral("/vazio.") +
				       SymbolGroup::extension(), &error));
		CHECK_FALSE(error.isEmpty());
	}
}
