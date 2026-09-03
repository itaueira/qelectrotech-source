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
#include "../../../sources/location/locationtree.h"
#include "../../../sources/location/projectlocation.h"
#include "qt_catch_tostring.h"
#include "../../../sources/qetinformation.h"
#include "../../../sources/diagramcontext.h"
#include "../../../sources/catalog/catalogassignment.h"

#include <QDomDocument>
#include <QDomElement>
#include <QMap>
#include <QString>
#include <QStringList>

namespace
{
		/// @return a location as the dialogue will hand it over
	ProjectLocation local(const char *codigo,
			      const char *nome = "",
			      const char *peca = "")
	{
		ProjectLocation l(QString::fromUtf8(codigo), QString::fromUtf8(nome));
		l.part_code = QString::fromUtf8(peca);
		return l;
	}

		/// @return a location element written by hand, as a file holds it
	QDomElement elemento(QDomDocument &documento,
			     const char *uuid,
			     const char *codigo,
			     const char *pai = nullptr)
	{
		QDomElement e = documento.createElement(ProjectLocation::tagName());
		e.setAttribute(QStringLiteral("uuid"), QString::fromUtf8(uuid));
		e.setAttribute(QStringLiteral("code"), QString::fromUtf8(codigo));
		if (pai) {
			e.setAttribute(QStringLiteral("parent"),
				       QString::fromUtf8(pai));
		}
		return e;
	}
}

TEST_CASE("T32 — a árvore guarda o armário, o que está dentro dele e o caminho de cada um",
	  "[localizacao]")
{
	LocationTree arvore;
	REQUIRE(arvore.isEmpty());

	const QString armario = arvore.append(local("QCM1", "Quadro de comando"));
	REQUIRE_FALSE(armario.isEmpty());

	ProjectLocation porta = local("PORTA", "Porta frontal");
	porta.parent_uuid = armario;
	const QString folha_de_porta = arvore.append(porta);
	REQUIRE_FALSE(folha_de_porta.isEmpty());

	SECTION("o caminho é feito dos códigos, e não dos nomes")
	{
		CHECK(arvore.count() == 2);
		CHECK(arvore.path(armario) == QString("QCM1"));
		CHECK(arvore.path(folha_de_porta) == QString("QCM1/PORTA"));
	}

	SECTION("o nome é o que o almoxarifado lê, e vem separado")
	{
		CHECK(arvore.displayPath(folha_de_porta)
		      == QString("Quadro de comando / Porta frontal"));
	}

	SECTION("o nível diz a que profundidade a localização está")
	{
		CHECK(arvore.depth(armario) == 0);
		CHECK(arvore.depth(folha_de_porta) == 1);
		CHECK(arvore.depth(QString("nao-existe")) == -1);
	}

	SECTION("o caminho encontra a localização de volta")
	{
		CHECK(arvore.uuidOfPath(QString("QCM1/PORTA")) == folha_de_porta);
		CHECK(arvore.indexOfPath(QString("QCM1/TAMPA")) == -1);
	}

	SECTION("as descendentes vêm todas, e o pai antes das filhas")
	{
		CHECK(arvore.rootUuids() == QStringList{armario});
		CHECK(arvore.childUuids(armario) == QStringList{folha_de_porta});
		CHECK(arvore.paths()
		      == QStringList{QString("QCM1"), QString("QCM1/PORTA")});
	}
}

TEST_CASE("T32 — o código recusa tudo o que tornaria o caminho ambíguo",
	  "[localizacao]")
{
	SECTION("um código vazio não nomeia lugar nenhum")
	{
		QString erro;
		CHECK_FALSE(ProjectLocation::isValidCode(QString(), &erro));
		CHECK_FALSE(erro.isEmpty());
	}

	SECTION("os prefixos da IEC 81346 e o separador ficam de fora")
	{
		CHECK_FALSE(ProjectLocation::isValidCode(QString("QCM+1")));
		CHECK_FALSE(ProjectLocation::isValidCode(QString("QCM-1")));
		CHECK_FALSE(ProjectLocation::isValidCode(QString("=QCM1")));
		CHECK_FALSE(ProjectLocation::isValidCode(QString("QCM1:A")));
		CHECK_FALSE(ProjectLocation::isValidCode(QString("QCM1/PORTA")));
		CHECK(ProjectLocation::isValidCode(QString("QCM1")));
		CHECK(ProjectLocation::isValidCode(QString("QCM_1")));
	}

	SECTION("duas irmãs não podem responder pelo mesmo código")
	{
		LocationTree arvore;
		QString erro;
		REQUIRE_FALSE(arvore.append(local("QCM1")).isEmpty());
		CHECK(arvore.append(local("qcm1"), &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(arvore.count() == 1);
	}

	SECTION("mas duas de níveis diferentes podem")
	{
		LocationTree arvore;
		const QString a = arvore.append(local("QCM1"));
		const QString b = arvore.append(local("QCM2"));
		ProjectLocation dentro_de_a = local("PLACA");
		dentro_de_a.parent_uuid = a;
		ProjectLocation dentro_de_b = local("PLACA");
		dentro_de_b.parent_uuid = b;
		CHECK_FALSE(arvore.append(dentro_de_a).isEmpty());
		CHECK_FALSE(arvore.append(dentro_de_b).isEmpty());
		CHECK(arvore.count() == 4);
	}

	SECTION("e uma localização não entra dentro de um pai que não existe")
	{
		LocationTree arvore;
		QString erro;
		ProjectLocation orfa = local("PLACA");
		orfa.parent_uuid = QString("nao-existe");
		CHECK(arvore.append(orfa, &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(arvore.isEmpty());
	}
}

TEST_CASE("T32 — renomear um armário devolve o caminho antigo e o novo de tudo que está dentro",
	  "[localizacao]")
{
	LocationTree arvore;
	const QString armario = arvore.append(local("QCM1", "Quadro"));
	ProjectLocation porta = local("PORTA");
	porta.parent_uuid = armario;
	const QString folha_de_porta = arvore.append(porta);

	SECTION("mudar o código reescreve a filharada junto")
	{
		ProjectLocation nova = arvore.location(armario);
		nova.code = QString("QCM2");

		QMap<QString, QString> movidas;
		QString erro;
		REQUIRE(arvore.update(nova, &movidas, &erro));
		CHECK(erro.isEmpty());

		CHECK(movidas.size() == 2);
		CHECK(movidas.value(QString("QCM1")) == QString("QCM2"));
		CHECK(movidas.value(QString("QCM1/PORTA")) == QString("QCM2/PORTA"));
		CHECK(arvore.path(folha_de_porta) == QString("QCM2/PORTA"));
	}

	SECTION("mudar só o nome não move caminho nenhum")
	{
		ProjectLocation nova = arvore.location(armario);
		nova.name = QString("Quadro de comando principal");

		QMap<QString, QString> movidas;
		REQUIRE(arvore.update(nova, &movidas));
		CHECK(movidas.isEmpty());
		CHECK(arvore.path(armario) == QString("QCM1"));
	}

	SECTION("gravar a mesma localização de volta não muda nada, e diz isso")
	{
		QString erro;
		CHECK_FALSE(arvore.update(arvore.location(armario), nullptr, &erro));
		CHECK(erro.isEmpty());
	}

	SECTION("um código que a irmã já usa é recusado, e nada é gravado")
	{
		arvore.append(local("QCM2"));
		ProjectLocation nova = arvore.location(armario);
		nova.code = QString("QCM2");

		QString erro;
		CHECK_FALSE(arvore.update(nova, nullptr, &erro));
		CHECK_FALSE(erro.isEmpty());
		CHECK(arvore.path(armario) == QString("QCM1"));
	}
}

TEST_CASE("T32 — mover uma localização leva o que está dentro e recusa o laço",
	  "[localizacao]")
{
	LocationTree arvore;
	const QString primeiro = arvore.append(local("QCM1"));
	const QString segundo = arvore.append(local("QCM2"));

	ProjectLocation placa = local("PLACA");
	placa.parent_uuid = primeiro;
	const QString folha_de_placa = arvore.append(placa);

	ProjectLocation trilho = local("TRILHO");
	trilho.parent_uuid = folha_de_placa;
	const QString folha_de_trilho = arvore.append(trilho);

	SECTION("a placa muda de armário e o trilho vai junto")
	{
		QMap<QString, QString> movidas;
		QString erro;
		REQUIRE(arvore.move(folha_de_placa, segundo, &movidas, &erro));
		CHECK(erro.isEmpty());

		CHECK(movidas.size() == 2);
		CHECK(movidas.value(QString("QCM1/PLACA")) == QString("QCM2/PLACA"));
		CHECK(movidas.value(QString("QCM1/PLACA/TRILHO"))
		      == QString("QCM2/PLACA/TRILHO"));
		CHECK(arvore.path(folha_de_trilho) == QString("QCM2/PLACA/TRILHO"));
	}

	SECTION("a placa sobe para o topo")
	{
		QMap<QString, QString> movidas;
		REQUIRE(arvore.move(folha_de_placa, QString(), &movidas));
		CHECK(arvore.path(folha_de_placa) == QString("PLACA"));
		CHECK(arvore.depth(folha_de_placa) == 0);
		CHECK(movidas.value(QString("QCM1/PLACA")) == QString("PLACA"));
	}

	SECTION("um armário não entra dentro da própria placa")
	{
		QString erro;
		CHECK_FALSE(arvore.move(primeiro, folha_de_trilho, nullptr, &erro));
		CHECK_FALSE(erro.isEmpty());
		CHECK(arvore.path(folha_de_trilho) == QString("QCM1/PLACA/TRILHO"));
	}

	SECTION("nem dentro de si mesmo")
	{
		QString erro;
		CHECK_FALSE(arvore.move(primeiro, primeiro, nullptr, &erro));
		CHECK_FALSE(erro.isEmpty());
	}
}

TEST_CASE("T32 — apagar um armário devolve todo caminho que deixou de existir",
	  "[localizacao]")
{
	LocationTree arvore;
	const QString armario = arvore.append(local("QCM1"));
	ProjectLocation placa = local("PLACA");
	placa.parent_uuid = armario;
	const QString folha_de_placa = arvore.append(placa);
	ProjectLocation porta = local("PORTA");
	porta.parent_uuid = armario;
	arvore.append(porta);
	arvore.append(local("QCM2"));

	QStringList apagadas;
	REQUIRE(arvore.remove(armario, &apagadas));

	CHECK(arvore.count() == 1);
	CHECK(arvore.indexOfUuid(folha_de_placa) == -1);
	CHECK(apagadas.size() == 3);
	CHECK(apagadas.first() == QString("QCM1"));
	CHECK(apagadas.contains(QString("QCM1/PLACA")));
	CHECK(apagadas.contains(QString("QCM1/PORTA")));

	SECTION("apagar o que não está lá não apaga nada")
	{
		QStringList nenhuma;
		CHECK_FALSE(arvore.remove(QString("nao-existe"), &nenhuma));
		CHECK(nenhuma.isEmpty());
		CHECK(arvore.count() == 1);
	}
}

TEST_CASE("CU-32.2 — a etiqueta da norma repete o mais em vez de aninhar",
	  "[localizacao]")
{
	CHECK(LocationTree::iecTag(QString("QCM1")) == QString("+QCM1"));
	CHECK(LocationTree::iecTag(QString("QCM1/PORTA"))
	      == QString("+QCM1+PORTA"));
	CHECK(LocationTree::iecTag(QString()).isEmpty());

	SECTION("o caminho se parte e se junta sem perder nem inventar passo")
	{
		const QStringList codigos =
			LocationTree::splitPath(QString("QCM1/PLACA/TRILHO"));
		CHECK(codigos.size() == 3);
		CHECK(LocationTree::joinPath(codigos)
		      == QString("QCM1/PLACA/TRILHO"));
	}

	SECTION("separador a mais é ruído, e não um passo em branco")
	{
		CHECK(LocationTree::splitPath(QString("/QCM1//PORTA/")).size() == 2);
	}
}

TEST_CASE("T32 — a lista de material conta os armários e deixa de fora a peça virtual",
	  "[localizacao]")
{
	LocationTree arvore;
	arvore.append(local("QCM1", "Quadro de comando", "GAB-600"));
	arvore.append(local("QCM2", "Quadro auxiliar", "GAB-600"));
	arvore.append(local("QCM3", "Quadro pequeno", "GAB-400"));

		//A porta veio junto com o armário: tem código, foi paga, e não é
		//linha de lista - a linha do armário já a contém.
	ProjectLocation porta = local("PORTA", "Porta frontal", "GAB-600-P");
	porta.virtual_part = true;
	arvore.append(porta);

		//E a que ninguém especificou ainda não tem o que comprar.
	arvore.append(local("CAMPO", "Instalação em campo"));

	const QList<LocationTree::BomLine> linhas = arvore.bomLines();
	REQUIRE(linhas.size() == 2);

	CHECK(linhas.at(0).part_code == QString("GAB-600"));
	CHECK(linhas.at(0).quantity == 2);
	CHECK(linhas.at(0).name == QString("Quadro de comando"));
	CHECK(linhas.at(0).paths
	      == QStringList{QString("QCM1"), QString("QCM2")});

	CHECK(linhas.at(1).part_code == QString("GAB-400"));
	CHECK(linhas.at(1).quantity == 1);

	SECTION("a revisão separa duas compras da mesma peça")
	{
		ProjectLocation antigo = local("QCM4", "Quadro antigo", "GAB-600");
		antigo.part_revision = 2;
		arvore.append(antigo);

		const QList<LocationTree::BomLine> agora = arvore.bomLines();
		CHECK(agora.size() == 3);
		CHECK(agora.at(2).part_revision == 2);
		CHECK(agora.at(2).quantity == 1);
	}

	SECTION("sem peça e com a bandeira, as duas respondem a mesma pergunta")
	{
		CHECK(local("CAMPO").isVirtual());
		CHECK(porta.isVirtual());
		CHECK_FALSE(local("QCM1", "", "GAB-600").isVirtual());
		CHECK(ProjectLocation().isNull());
		CHECK_FALSE(local("QCM1").isNull());
	}
}

TEST_CASE("T32 — a árvore volta do arquivo igual ao que entrou",
	  "[localizacao]")
{
	LocationTree arvore;
	const QString armario = arvore.append(local("QCM1", "Quadro", "GAB-600"));
	ProjectLocation porta = local("PORTA", "Porta frontal", "GAB-600-P");
	porta.parent_uuid = armario;
	porta.virtual_part = true;
	porta.description = QString::fromUtf8("Com visor");
	porta.part_revision = 3;
	arvore.append(porta);

	QDomDocument documento;
	const QDomElement escrita = arvore.toXml(documento);
	documento.appendChild(escrita);

	LocationTree lida;
	REQUIRE(lida.fromXml(documento.documentElement()));
	CHECK(lida == arvore);
	CHECK(lida.paths() == arvore.paths());

	SECTION("um elemento que não é nosso não é lido como se fosse")
	{
		QDomDocument outro;
		QDomElement raiz = outro.createElement(QStringLiteral("io_list"));
		outro.appendChild(raiz);
		LocationTree vazia;
		CHECK_FALSE(vazia.fromXml(outro.documentElement()));
		CHECK(vazia.isEmpty());
	}
}

TEST_CASE("T32 — o arquivo pode guardar o que a árvore não aceita, e a leitura conserta",
	  "[localizacao]")
{
	QDomDocument documento;
	QDomElement raiz = documento.createElement(LocationTree::tagName());
	documento.appendChild(raiz);

	SECTION("um pai que não está no arquivo vira localização de topo")
	{
		raiz.appendChild(elemento(documento, "A", "QCM1"));
		raiz.appendChild(elemento(documento, "B", "PLACA", "nao-existe"));

		LocationTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);
		CHECK(arvore.path(QString("B")) == QString("PLACA"));
	}

	SECTION("dois irmãos com o mesmo código são numerados à parte")
	{
		raiz.appendChild(elemento(documento, "A", "QCM1"));
		raiz.appendChild(elemento(documento, "B", "qcm1"));

		LocationTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);
		CHECK(arvore.path(QString("A")) == QString("QCM1"));
		CHECK(arvore.path(QString("B")) == QString("qcm1_2"));
	}

	SECTION("um ramo que dá a volta em si mesmo é cortado uma vez")
	{
		raiz.appendChild(elemento(documento, "C", "QCM1", "D"));
		raiz.appendChild(elemento(documento, "D", "PLACA", "C"));

		LocationTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);
		CHECK(arvore.paths().size() == 2);
		CHECK(arvore.depth(QString("C")) == 0);
		CHECK(arvore.depth(QString("D")) == 1);
	}

	SECTION("um código com o que o caminho não carrega é limpo, não recusado")
	{
		raiz.appendChild(elemento(documento, "A", "+QCM-1"));

		LocationTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 1);
		CHECK(arvore.path(QString("A")) == QString("QCM1"));
	}

	SECTION("um código que o arquivo perdeu ganha um, para poder ser apontado")
	{
		raiz.appendChild(elemento(documento, "A", ""));

		LocationTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 1);
		CHECK_FALSE(arvore.at(0).code.isEmpty());
	}

	SECTION("dois uuid iguais deixam de ser iguais")
	{
		raiz.appendChild(elemento(documento, "A", "QCM1"));
		raiz.appendChild(elemento(documento, "A", "QCM2"));

		LocationTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);
		CHECK(arvore.at(0).uuid != arvore.at(1).uuid);
	}
}

TEST_CASE("a chave que o componente carrega", "[localizacao]")
{
	SECTION("location_path é uma informação de elemento como as outras")
	{
		CHECK(QETInformation::elementInfoKeys().contains(
					  QETInformation::ELMT_LOCATION_PATH));
		CHECK(QETInformation::ELMT_LOCATION_PATH == "location_path");
	}

	SECTION("location não deixou de existir")
	{
		// O projeto real usa location para a régua de bornes em 23
		// componentes. A T32 acrescenta uma chave, não substitui a que
		// já está preenchida.
		CHECK(QETInformation::elementInfoKeys().contains(
					  QETInformation::ELMT_LOCATION));
	}

	SECTION("a chave tem nome de gente")
	{
		CHECK_FALSE(QETInformation::translatedInfoKey(
					    QETInformation::ELMT_LOCATION_PATH).isEmpty());
	}

	SECTION("atribuir peça não apaga o caminho")
	{
		CHECK(CatalogAssignment::protectedElementKeys().contains(
					      QETInformation::ELMT_LOCATION_PATH));
	}

	SECTION("o QET de origem guarda a chave sem entendê-la")
	{
		// É o preço combinado da decisão C: o componente guarda o caminho
		// como informação comum, e por isso um projeto nosso aberto no
		// QElectroTech de origem não perde a localização.
		DiagramContext contexto;
		// addValue e quem cobra o formato de chave: recusa a que o
		// QET nao saberia reler.
		CHECK_FALSE(contexto.addValue("Chave Invalida", "x"));
		CHECK(contexto.addValue(QETInformation::ELMT_LOCATION_PATH,
					"QCM1/PORTA"));
		CHECK(contexto.value(QETInformation::ELMT_LOCATION_PATH)
					.toString() == "QCM1/PORTA");
	}
}

TEST_CASE("T32 — o componente segue a árvore por consulta direta, e o vizinho de nome parecido fica onde está",
	  "[localizacao]")
{
	LocationTree arvore;
	const QString armario = arvore.append(local("QCM1", "Quadro"));
	const QString vizinho = arvore.append(local("QCM10", "Quadro dez"));

	ProjectLocation porta = local("PORTA");
	porta.parent_uuid = armario;
	const QString folha_de_porta = arvore.append(porta);

	ProjectLocation trilho = local("TR1");
	trilho.parent_uuid = folha_de_porta;
	arvore.append(trilho);

	SECTION("caminho que ninguém mexeu volta como estava")
	{
		QMap<QString, QString> movidas;
		movidas.insert(QString("QCM1"), QString("QCP1"));

		CHECK(LocationTree::rewrittenPath(QString("QCM10"), movidas)
					== QString("QCM10"));
	}

	SECTION("caminho vazio continua vazio")
	{
		// Vazio é o "não atribuído" da decisão F, e não uma chave a
		// procurar no mapa.
		QMap<QString, QString> movidas;
		movidas.insert(QString(), QString("QCP1"));

		CHECK(LocationTree::rewrittenPath(QString(), movidas).isEmpty());
	}

	SECTION("renomear o armário arrasta o que está três níveis abaixo")
	{
		ProjectLocation nova = arvore.location(armario);
		nova.code = QString("QCP1");

		QMap<QString, QString> movidas;
		REQUIRE(arvore.update(nova, &movidas));

		// O ramo inteiro vem no mapa, e é por isso que uma consulta
		// direta basta: nada aqui precisa casar prefixo.
		CHECK(LocationTree::rewrittenPath(QString("QCM1/PORTA/TR1"), movidas)
					== QString("QCP1/PORTA/TR1"));
		CHECK(LocationTree::rewrittenPath(QString("QCM1/PORTA"), movidas)
					== QString("QCP1/PORTA"));
		CHECK(LocationTree::rewrittenPath(QString("QCM1"), movidas)
					== QString("QCP1"));

		// E o vizinho não é tocado. Casar prefixo o reescreveria como
		// QCP10, que é o defeito que este teste existe para impedir.
		CHECK(LocationTree::rewrittenPath(QString("QCM10"), movidas)
					== QString("QCM10"));
		CHECK(arvore.path(vizinho) == QString("QCM10"));
	}

	SECTION("mover o armário para dentro de outro arrasta a mesma filharada")
	{
		QMap<QString, QString> movidas;
		REQUIRE(arvore.move(armario, vizinho, &movidas));

		CHECK(LocationTree::rewrittenPath(QString("QCM1/PORTA/TR1"), movidas)
					== QString("QCM10/QCM1/PORTA/TR1"));
		CHECK(LocationTree::rewrittenPath(QString("QCM10"), movidas)
					== QString("QCM10"));
	}

	SECTION("o que foi apagado volta como não atribuído")
	{
		QStringList apagadas;
		REQUIRE(arvore.remove(armario, &apagadas));

		const QMap<QString, QString> perdidas =
					LocationTree::lostPaths(apagadas);
		CHECK(perdidas.size() == apagadas.size());

		// Uma localização que deixou de existir não é substituída por
		// outra: o componente fica sem localização, e nada se perde
		// além dela.
		CHECK(LocationTree::rewrittenPath(QString("QCM1/PORTA/TR1"), perdidas)
					.isEmpty());
		CHECK(LocationTree::rewrittenPath(QString("QCM1"), perdidas)
					.isEmpty());
		CHECK(LocationTree::rewrittenPath(QString("QCM10"), perdidas)
					== QString("QCM10"));
	}

	SECTION("lista vazia de apagadas não reescreve nada")
	{
		const QMap<QString, QString> perdidas =
					LocationTree::lostPaths(QStringList());
		CHECK(perdidas.isEmpty());
		CHECK(LocationTree::rewrittenPath(QString("QCM1"), perdidas)
					== QString("QCM1"));
	}
}
