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

#include "qt_catch_tostring.h"

#include "../../../sources/autoNum/renumberplan.h"
#include "../../../sources/connector/connectorswap.h"
#include "../../../sources/connector/connectorways.h"

#include <QPair>
#include <QSet>
#include <QStringList>

/*
	Trocar duas vias de um conector, como regra sobre uma lista (T34, passo 5).

	O caso de uso cabe numa frase — "a posição na lista não muda, o conteúdo
	das linhas troca" — e a frase esconde a única coisa que se pode errar
	aqui: **reordenar e trocar produzem a mesma tabela**. Uma tabela ordenada
	por número de via lê igual nos dois casos, então uma implementação que
	mova os dois pinos de lugar e renomeie de volta imprime exatamente o mesmo
	que a certa, com os dois símbolos trocados no desenho. É por isso que o
	pino carrega identificador aqui: é o que separa as duas, e é a asserção
	que uma reordenação disfarçada tem de derrubar.

	O resto é o que se recusa, e por quê: trocar um pino com ele mesmo, trocar
	dois que já têm o mesmo rótulo, e trocar entre conectores diferentes — o
	último não por escrúpulo, mas porque tira uma via de um e põe no outro,
	deixando o número 1 duas vezes de um lado e nenhuma do outro.

	Rotulado T34 e não CU-34.4: o caso é a troca feita a partir da tabela do
	conector, com o esquema acompanhando, e a tabela é o passo 6. O que se
	prova aqui é qual rótulo vai para onde, e o que fica parado.
*/

namespace
{
	using Pin = ConnectorSwap::Pin;
	using Plan = ConnectorSwap::Plan;
	using Refusal = ConnectorSwap::Refusal;

	/// Quatro vias de um conector, na ordem em que uma lista as mostra.
	QList<Pin> fourWays()
	{
		return QList<Pin>({Pin(QStringLiteral("a"), QStringLiteral("CN1"),
				       QStringLiteral("1")),
				   Pin(QStringLiteral("b"), QStringLiteral("CN1"),
				       QStringLiteral("2")),
				   Pin(QStringLiteral("c"), QStringLiteral("CN1"),
				       QStringLiteral("3")),
				   Pin(QStringLiteral("d"), QStringLiteral("CN1"),
				       QStringLiteral("4"))});
	}

	/// Os identificadores de @a pins, na ordem em que estão.
	QStringList ids(const QList<Pin> &pins)
	{
		QStringList found;
		for (const Pin &pin : pins) {
			found << pin.id;
		}
		return found;
	}

	/// Os rótulos de @a pins, na ordem em que estão.
	QStringList labels(const QList<Pin> &pins)
	{
		QStringList found;
		for (const Pin &pin : pins) {
			found << pin.label;
		}
		return found;
	}

	/// O identificador do pino que carrega @a label, vazio quando nenhum carrega.
	QString idOfLabel(const QList<Pin> &pins, const QString &label)
	{
		for (const Pin &pin : pins)
		{
			if (pin.label == label) {
				return pin.id;
			}
		}
		return QString();
	}

	/**
		A operação vizinha, escrita à mão: mover o item de @a from para
		@a to, que é o que TerminalStrip::setOrderTo faz numa régua.

		Está aqui para ser comparada, e não para ser usada. Sem ela, "não
		é reordenação" é uma frase do comentário; com ela, é um número
		diferente medido ao lado.
	*/
	QList<Pin> reordered(const QList<Pin> &pins, int from, int to)
	{
		QList<Pin> after = pins;
		after.move(from, to);
		return after;
	}
}

TEST_CASE("T34 — trocar as vias 2 e 3 cruza os dois rótulos e não mexe em mais nada",
	  "[t34][connector][swap]")
{
	const QList<Pin> before = fourWays();
	const Plan swap = ConnectorSwap::plan(before, 1, 2);

	REQUIRE(swap.isValid());
	CHECK(swap.describe().isEmpty());

	SECTION("cada um recebe o rótulo do outro")
	{
			//As posições voltam como entraram: o chamador que escreve o
			//resultado escreve nas mesmas duas linhas que leu.
		CHECK(swap.first_index == 1);
		CHECK(swap.second_index == 2);
		CHECK(swap.first_label == QStringLiteral("3"));
		CHECK(swap.second_label == QStringLiteral("2"));
	}

	SECTION("a posição na lista não muda, e é isso que separa trocar de reordenar")
	{
		const QList<Pin> after = ConnectorSwap::applied(before, swap);

			//A asserção do caso de uso, e a que uma reordenação
			//disfarçada tem de derrubar: mesmo tamanho, mesmos pinos,
			//na mesma ordem. Só os rótulos andaram.
		CHECK(after.size() == before.size());
		CHECK(ids(after) == ids(before));
		CHECK(ids(after) == QStringList({QStringLiteral("a"),
						 QStringLiteral("b"),
						 QStringLiteral("c"),
						 QStringLiteral("d")}));
		CHECK(labels(after) == QStringList({QStringLiteral("1"),
						    QStringLiteral("3"),
						    QStringLiteral("2"),
						    QStringLiteral("4")}));

			//E o conector de cada um continua o dele: a troca escreve
			//rótulo, e o campo que diz de quem o pino é não é dela.
		for (int index = 0 ; index < after.size() ; ++index)
		{
			INFO(index);
			CHECK(after.at(index).connector == before.at(index).connector);
		}
	}

	SECTION("a reordenação vizinha produz outra coisa, e a diferença é medida")
	{
		const QList<Pin> swapped = ConnectorSwap::applied(before, swap);
		const QList<Pin> moved = reordered(before, 1, 2);

			//Mover o item de 1 para 2 devolve os mesmos rótulos na
			//mesma ordem que a troca — 1, 3, 2, 4 —, e é exatamente
			//por isso que uma tabela ordenada por via não distingue as
			//duas. O que distingue são os identificadores.
		CHECK(labels(moved) == labels(swapped));
		CHECK(ids(moved) != ids(swapped));
		CHECK(ids(moved) == QStringList({QStringLiteral("a"),
						 QStringLiteral("c"),
						 QStringLiteral("b"),
						 QStringLiteral("d")}));
	}

	SECTION("lida pelo número da via, a lista mantém 1..4 e quem troca é o pino")
	{
		const QList<Pin> after = ConnectorSwap::applied(before, swap);

			//A outra metade da frase do caso, na outra leitura: numa
			//tabela ordenada por via, as linhas continuam 1, 2, 3, 4 e
			//o que mudou foi o pino de cada uma. As duas leituras são a
			//mesma operação, e as duas precisam valer.
		CHECK(labels(after).size() == 4);
		for (const QString &way : QStringList({QStringLiteral("1"),
						       QStringLiteral("2"),
						       QStringLiteral("3"),
						       QStringLiteral("4")}))
		{
			INFO(way.toStdString());
			CHECK(labels(after).contains(way));
		}

		CHECK(idOfLabel(before, QStringLiteral("2")) == QStringLiteral("b"));
		CHECK(idOfLabel(before, QStringLiteral("3")) == QStringLiteral("c"));
		CHECK(idOfLabel(after, QStringLiteral("2")) == QStringLiteral("c"));
		CHECK(idOfLabel(after, QStringLiteral("3")) == QStringLiteral("b"));

			//E as vias que ninguém tocou continuam com o pino delas.
		CHECK(idOfLabel(after, QStringLiteral("1")) == QStringLiteral("a"));
		CHECK(idOfLabel(after, QStringLiteral("4")) == QStringLiteral("d"));
	}
}

TEST_CASE("T34 — trocar um pino com ele mesmo é recusado, e pelo motivo certo",
	  "[t34][connector][swap]")
{
	const QList<Pin> pins = fourWays();

	SECTION("pela posição")
	{
		const Plan swap = ConnectorSwap::plan(pins, 2, 2);
		CHECK_FALSE(swap.isValid());
		CHECK(swap.refusal == Refusal::SamePin);
		CHECK_FALSE(swap.describe().isEmpty());
	}

	SECTION("e pelo identificador, que é o que o chamador do projeto tem")
	{
			//Duas posições diferentes e um pino só: é o que o lado do
			//projeto monta quando recebe o mesmo componente duas vezes.
			//Sem a comparação por identificador, a resposta seria
			//"os dois já têm o mesmo rótulo" — verdade, e não o que se
			//perguntou.
		const QList<Pin> twice({pins.at(1), pins.at(1)});
		const Plan swap = ConnectorSwap::plan(twice, 0, 1);
		CHECK(swap.refusal == Refusal::SamePin);
	}

	SECTION("e nada é escrito")
	{
		const Plan swap = ConnectorSwap::plan(pins, 2, 2);
		CHECK(ConnectorSwap::applied(pins, swap) == pins);
		CHECK(labels(ConnectorSwap::applied(pins, swap)) == labels(pins));
	}
}

TEST_CASE("T34 — dois pinos de conectores diferentes não trocam de via",
	  "[t34][connector][swap]")
{
	const QList<Pin> pins({Pin(QStringLiteral("a"), QStringLiteral("CN1"),
				   QStringLiteral("1")),
			       Pin(QStringLiteral("b"), QStringLiteral("CN1"),
				   QStringLiteral("2")),
			       Pin(QStringLiteral("c"), QStringLiteral("XS2"),
				   QStringLiteral("1")),
			       Pin(QStringLiteral("d"), QString(),
				   QStringLiteral("7"))});

	SECTION("a recusa é declarada, e não silenciosa")
	{
		const Plan swap = ConnectorSwap::plan(pins, 1, 2);
		CHECK_FALSE(swap.isValid());
		CHECK(swap.refusal == Refusal::OtherConnector);
		CHECK_FALSE(swap.describe().isEmpty());
	}

	SECTION("e o motivo é que a troca poria o número 1 duas vezes num conector")
	{
			//A medição por trás da recusa: se a troca fosse feita, CN1
			//ficaria com duas vias 1 e XS2 com nenhuma. É a conta que
			//torna a recusa uma regra e não um gosto.
		const Plan forced = ConnectorSwap::plan(
					    QList<Pin>({pins.at(1), pins.at(2)}), 0, 1);
		REQUIRE_FALSE(forced.isValid());

		QList<Pin> as_if = pins;
		as_if[1].label = pins.at(2).label;
		as_if[2].label = pins.at(1).label;

		QStringList cn1_ways;
		for (const Pin &pin : as_if)
		{
			if (Renumberer::connectorKey(pin.connector)
					== QStringLiteral("cn1")) {
				cn1_ways << pin.label;
			}
		}
		CHECK(cn1_ways == QStringList({QStringLiteral("1"),
					       QStringLiteral("1")}));
	}

	SECTION("um pino sem conector nenhum tem a sua própria recusa")
	{
			//Não é o mesmo caso: ele não está numa lista para manter a
			//posição dentro dela. Quem quiser pô-lo num conector usa a
			//outra porta, que escreve o campo do conector.
		const Plan swap = ConnectorSwap::plan(pins, 0, 3);
		CHECK(swap.refusal == Refusal::NoConnector);
	}
}

TEST_CASE("T34 — duas grafias de um conector são um conector, e as vias trocam",
	  "[t34][connector][swap]")
{
	const QList<Pin> pins({Pin(QStringLiteral("a"), QStringLiteral("CN1"),
				   QStringLiteral("1")),
			       Pin(QStringLiteral("b"), QStringLiteral("cn1"),
				   QStringLiteral("2")),
			       Pin(QStringLiteral("c"), QStringLiteral("CN1 "),
				   QStringLiteral("3"))});

		//A chave do passo 3 e não o campo cru. Recusar aqui seria esta
		//regra discordando da renumeração sobre quantos conectores o
		//projeto tem — e o projetista veria a troca ser negada por causa de
		//uma maiúscula que ele nem sabe que digitou.
	REQUIRE(Renumberer::connectorKey(QStringLiteral("CN1"))
		== Renumberer::connectorKey(QStringLiteral("cn1")));

	const Plan swap = ConnectorSwap::plan(pins, 1, 2);
	CHECK(swap.isValid());

	const QList<Pin> after = ConnectorSwap::applied(pins, swap);
	CHECK(labels(after) == QStringList({QStringLiteral("1"),
					    QStringLiteral("3"),
					    QStringLiteral("2")}));

		//E a grafia de cada pino fica como estava: a troca escreve rótulo,
		//e corrigir texto que o usuário digitou não é dela — é a mesma
		//decisão que a renumeração já registrou.
	CHECK(after.at(1).connector == QStringLiteral("cn1"));
	CHECK(after.at(2).connector == QStringLiteral("CN1 "));
}

TEST_CASE("T34 — trocar duas vias que já têm o mesmo rótulo não escreve nada",
	  "[t34][connector][swap]")
{
	const QList<Pin> pins({Pin(QStringLiteral("a"), QStringLiteral("CN1"),
				   QStringLiteral("7")),
			       Pin(QStringLiteral("b"), QStringLiteral("CN1"),
				   QStringLiteral("7"))});

	const Plan swap = ConnectorSwap::plan(pins, 0, 1);

		//Duas vias 7 num conector é falta de outra natureza, e quem a
		//levanta é o relatório. Trocá-las de lugar não a resolveria e
		//gastaria um passo de desfazer que não desfaz nada.
	CHECK(swap.refusal == Refusal::SameLabel);
	CHECK(ConnectorSwap::applied(pins, swap) == pins);
}

TEST_CASE("T34 — uma posição que não existe na lista é recusada e não estoura",
	  "[t34][connector][swap]")
{
	const QList<Pin> pins = fourWays();

	for (const QPair<int, int> &places : QList<QPair<int, int> >(
		     {qMakePair(-1, 2), qMakePair(1, 4), qMakePair(9, 9),
		      qMakePair(0, -3)}))
	{
		INFO(places.first << " " << places.second);
		const Plan swap = ConnectorSwap::plan(pins, places.first,
						      places.second);
		CHECK(swap.refusal == Refusal::NoSuchPin);
		CHECK(ConnectorSwap::applied(pins, swap) == pins);
	}

		//E numa lista vazia, que é o que uma tabela recém-aberta tem.
	const Plan empty = ConnectorSwap::plan(QList<Pin>(), 0, 1);
	CHECK(empty.refusal == Refusal::NoSuchPin);
}

TEST_CASE("T34 — o rótulo da via viaja como está, e a discordância continua aparecendo",
	  "[t34][connector][swap]")
{
		//A assimetria presa dos dois lados, como o passo 2 a deixou: o
		//nome do conector dobra, o rótulo da via não. Aqui ela é medida
		//através da troca — se a troca aparasse o rótulo ao movê-lo, o
		//"9 " que a peça não tem viraria a via "9" que ela tem, e uma via
		//que está cabeada passaria a ler reserva.
	const QList<Pin> pins({Pin(QStringLiteral("a"), QStringLiteral("XS6"),
				   QStringLiteral("1")),
			       Pin(QStringLiteral("b"), QStringLiteral("XS6"),
				   QStringLiteral("9 "))});

	const Plan swap = ConnectorSwap::plan(pins, 0, 1);
	REQUIRE(swap.isValid());

	const QList<Pin> after = ConnectorSwap::applied(pins, swap);
	CHECK(after.at(0).label == QStringLiteral("9 "));
	CHECK(after.at(1).label == QStringLiteral("1"));

	const QStringList part_ways({QStringLiteral("1"), QStringLiteral("2"),
				     QStringLiteral("9")});
	const ConnectorWays ways = ConnectorWays::fromPinout(part_ways,
							     labels(after));

	CHECK(ways.notOnPart() == QStringList({QStringLiteral("9 ")}));
	CHECK(ways.usedCount() == 1);
	CHECK(ways.reserveCount() == 2);
}

TEST_CASE("T34 — cada recusa diz uma coisa diferente, e o sucesso não diz nada",
	  "[t34][connector][swap]")
{
		//Quatro frases e não uma: o movimento que resolve cada recusa é
		//um movimento diferente, e uma linha dizendo só "não dá" mandaria
		//a mesma pessoa à mesma janela errada.
	const QList<Refusal> refusals({Refusal::NoSuchPin, Refusal::SamePin,
				       Refusal::NoConnector,
				       Refusal::OtherConnector,
				       Refusal::SameLabel});

	QStringList sentences;
	for (Refusal refusal : refusals)
	{
		const QString sentence = Plan::describe(refusal);
		INFO(sentence.toStdString());
		CHECK_FALSE(sentence.isEmpty());
		sentences << sentence;
	}

	CHECK(QSet<QString>(sentences.begin(), sentences.end()).size()
	      == refusals.size());
	CHECK(Plan::describe(Refusal::None).isEmpty());
}
