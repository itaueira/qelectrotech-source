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
#include "../../../sources/dataBase/bomquery.h"
#include "../../../sources/qetinformation.h"
#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QString>
#include <QStringList>

/*
	As regras de que uma lista de material é feita, sem banco nenhum.

	As três são texto: qual linha nomeia alguma coisa, por que uma lista de
	compra agrupa, e quais colunas uma consulta montada pela janela publica.
	Nenhuma precisa de projeto aberto, e é justamente por isso que elas
	podem ser fixadas aqui — a consulta que sai delas só é exercitada com um
	projeto, em src/ui/bomnameless_test.cpp, e o que se prova lá é outra
	coisa: que a regra, aplicada, devolve as linhas certas.

	O caso que dá nome a este arquivo é o do sinalizador: `exclude_from_bom`
	está na lista de chaves de informação porque a tabela element_info é
	construída daquela lista, mas ele não é informação sobre o componente —
	é um sinalizador sobre esta lista, com caixa de seleção própria. Uma
	condição que o contasse responderia "sim, tem informação" para todo
	componente cuja caixa já foi marcada e desmarcada alguma vez, porque
	isso grava o literal "false" na coluna. O filtro funcionaria num projeto
	e pararia de funcionar no seguinte, sem dizer por quê.
*/

TEST_CASE("T16 — a condição que separa a linha que nomeia algo da que não "
	  "nomeia nada", "[bom][database]")
{
	SECTION("o sinalizador da própria lista não conta como informação")
	{
			//A lista de verdade, e não uma inventada: o que se mede é que
			//a chave que o programa usa é a chave que a condição pula.
		const QStringList keys = QETInformation::elementInfoKeys();
		REQUIRE(keys.contains(QStringLiteral("exclude_from_bom")));

		const QString condition =
			QETBom::informationPresentCondition(QStringLiteral("ei."),
							    keys);

		CHECK_FALSE(condition.contains(QStringLiteral("exclude_from_bom")));

			//E a sonda da linha acima: sem ela, uma condição vazia
			//passaria no mesmo teste.
		CHECK(condition.contains(QStringLiteral("ei.label")));
		CHECK(condition.contains(QStringLiteral("ei.designation")));
		CHECK(condition.contains(QStringLiteral("ei.part_code")));
	}

	SECTION("toda chave de informação, e nenhuma a menos")
	{
		const QStringList keys = QETInformation::elementInfoKeys();
		const QString condition =
			QETBom::informationPresentCondition(QString(), keys);

			//Contado, e não amostrado: uma chave esquecida é uma coluna
			//em que o projetista escreveu e que a lista trata como
			//vazia — a linha some com o texto dentro dela.
		CHECK(condition.count(QStringLiteral("COALESCE("))
		      == keys.size() - 1);

		for (const QString &key : keys)
		{
			if (key == QStringLiteral("exclude_from_bom")) {
				continue;
			}
			INFO(key.toStdString());
			CHECK(condition.contains(QStringLiteral("COALESCE(") + key
						 + QStringLiteral(",'') <> ''")));
		}
	}

	SECTION("as duas formas de vazio, porque as duas acontecem")
	{
			//Uma chave que o componente nunca carregou é gravada como
			//NULL; uma que ele carrega sem nada digitado é gravada como
			//string vazia. Uma condição que testasse só uma das duas
			//deixaria passar metade das linhas sem nome, e qual metade
			//depende de como o componente foi desenhado.
		const QString condition =
			QETBom::informationPresentCondition(
				QString(), QStringList {QStringLiteral("label")});

		CHECK(condition == QStringLiteral("(COALESCE(label,'') <> '')"));
	}

	SECTION("o prefixo é escrito em toda coluna, e em nenhuma outra parte")
	{
		const QString condition =
			QETBom::informationPresentCondition(
				QStringLiteral("ei."),
				QStringList {QStringLiteral("label"),
					     QStringLiteral("designation")});

		CHECK(condition
		      == QStringLiteral("(COALESCE(ei.label,'') <> ''"
					" OR COALESCE(ei.designation,'') <> '')"));
	}

	SECTION("sem coluna de informação nenhuma, a condição é verdadeira")
	{
			//E não falsa, que é o que elementTypeClause() responde ao
			//conjunto vazio de espécies. As duas perguntas não são a
			//mesma: lá, quem não pede espécie nenhuma disse que não quer
			//nada; aqui, um esquema sem coluna de informação não disse
			//nada, e um filtro que esvaziasse toda lista do programa por
			//isso seria a pior falha disponível.
		CHECK(QETBom::informationPresentCondition(QStringLiteral("ei."),
							  QStringList())
		      == QStringLiteral("(1)"));
	}
}

TEST_CASE("T16 — o agrupamento da lista de compra é a identidade da peça "
	  "mais o que a lista mostra", "[bom][database]")
{
	SECTION("o código e a revisão vêm primeiro, sempre")
	{
			//Agrupar pela designação junta duas peças diferentes
			//descritas com as mesmas palavras, e quem lê a lista não tem
			//como ver que aconteceu: uma linha, uma quantidade, um dos
			//dois códigos, e o item errado comprado.
		const QStringList group = QETBom::groupByColumns(
			QStringList {QStringLiteral("designation")});

		REQUIRE(group.size() == 3);
		CHECK(group.at(0) == QETInformation::ELMT_PART_CODE);
		CHECK(group.at(1) == QETInformation::ELMT_PART_REVISION);
		CHECK(group.at(2) == QStringLiteral("designation"));
	}

	SECTION("coluna publicada duas vezes não aparece duas vezes")
	{
		const QStringList group = QETBom::groupByColumns(
			QStringList {QETInformation::ELMT_PART_CODE,
				     QStringLiteral("designation"),
				     QETInformation::ELMT_PART_CODE});

		REQUIRE(group.size() == 3);
		CHECK(group.count(QETInformation::ELMT_PART_CODE) == 1);
	}

	SECTION("sem coluna publicada, resta a identidade da peça")
	{
		const QStringList group =
			QETBom::groupByColumns(QStringList());

		CHECK(group == QStringList {QETInformation::ELMT_PART_CODE,
					    QETInformation::ELMT_PART_REVISION});
	}
}

TEST_CASE("T16 — as colunas de uma consulta montada são lidas do fim dela",
	  "[bom][database]")
{
	SECTION("a lista depois do ORDER BY, e não a de depois do SELECT")
	{
			//A primeira cópia carrega a coluna de contagem e o apelido
			//dela; a segunda não carrega nada além das chaves.
		const QString query = QStringLiteral(
			"SELECT label, designation, COUNT(*) AS designation_qty "
			"FROM element_nomenclature_view"
			" GROUP BY part_code, part_revision, label, designation"
			" ORDER BY label, designation");

		CHECK(QETBom::publishedColumns(query)
		      == QStringList {QStringLiteral("label"),
				      QStringLiteral("designation")});

			//E o agrupamento que sai disso, que é o que a janela escreve.
		CHECK(QETBom::groupBy(query)
		      == QStringLiteral("part_code, part_revision, label,"
					" designation"));
	}

	SECTION("a consulta que o próprio usuário escreveu não é reescrita")
	{
			//Nenhum ORDER BY: a janela entrega a consulta inteira, e
			//nenhum GROUP BY nosso toma parte nela. Sobra a identidade da
			//peça, que continua sendo uma resposta.
		const QString typed = QStringLiteral(
			"SELECT label FROM element_nomenclature_view");

		CHECK(QETBom::publishedColumns(typed).isEmpty());
		CHECK(QETBom::groupBy(typed)
		      == QStringLiteral("part_code, part_revision"));
	}

	SECTION("função, apelido ou direção de ordenação no fim recusam tudo")
	{
			//Nada disso é nome de coluna, e nada disso entra num GROUP BY
			//escrito daqui. Recusar a lista inteira é o que impede que
			//metade dela entre.
		for (const QString &tail : {QStringLiteral(" ORDER BY label DESC"),
					    QStringLiteral(" ORDER BY lower(label)"),
					    QStringLiteral(" ORDER BY label, ")})
		{
			INFO(tail.toStdString());
			CHECK(QETBom::publishedColumns(
				      QStringLiteral("SELECT label FROM v") + tail)
			      .isEmpty());
		}
	}
}
