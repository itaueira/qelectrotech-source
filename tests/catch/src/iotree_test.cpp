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
#include "../../../sources/plc/iolist.h"
#include "../../../sources/plc/iopoint.h"
#include "../../../sources/plc/iotree.h"
#include "qt_catch_tostring.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace
{
		/// @return a point that names a card, and takes a channel when asked
	IoPoint ponto(const char *description,
		      const char *uuid = nullptr,
		      int canal = -1)
	{
		IoPoint p;
		p.description = QString::fromUtf8(description);
		if (uuid) {
			p.master_uuid = QString::fromUtf8(uuid);
		}
		p.io_index = canal;
		if (canal >= 0) {
			p.channel = QString::fromUtf8("I") + QString::number(canal);
		}
		return p;
	}

		/// @return a card of a controller, as the panel will hand it over
	IoTree::Card cartao(const char *uuid,
			    const char *label,
			    const char *unit = "",
			    int channels = 16)
	{
		IoTree::Card c;
		c.uuid = QString::fromUtf8(uuid);
		c.label = QString::fromUtf8(label);
		c.unit = QString::fromUtf8(unit);
		c.channels = channels;
		return c;
	}

		/// @return the labels of the units, in the order the tree gives them
	QStringList rotulos(const IoTree::Tree &tree)
	{
		QStringList out;
		for (const IoTree::UnitGroup &unit : tree.units) {
			out << unit.label;
		}
		return out;
	}

		/// @return the labels of the cards of one unit, in tree order
	QStringList rotulosDosCartoes(const IoTree::UnitGroup &unit)
	{
		QStringList out;
		for (const IoTree::CardGroup &card : unit.cards) {
			out << card.label;
		}
		return out;
	}

		/// @return the descriptions of the points of one card, in tree order
	QStringList descricoes(const IoTree::CardGroup &card, const IoList &list)
	{
		QStringList out;
		for (int index : card.points) {
			out << list.at(index).description;
		}
		return out;
	}

		/**
			O projeto de referência dos testes: dois CLPs, quatro cartões,
			um cartão que ninguém tem, e três pontos que não nomearam
			cartão nenhum. Quinze pontos ao todo.
		*/
	IoList projeto()
	{
		IoList list;
		list.append(ponto("Botão de partida", "u-a", 0));
		list.append(ponto("Botão de parada", "u-a", 1));
		list.append(ponto("Emergência", "u-a", 2));

		list.append(ponto("Térmico M1", "u-b", 0));
		list.append(ponto("Térmico M2", "u-b", 1));
		list.append(ponto("Térmico M3", "u-b"));

		list.append(ponto("Nível alto", "u-c", 0));
		list.append(ponto("Nível baixo", "u-c", 1));
		list.append(ponto("Pressostato", "u-c", 2));
		list.append(ponto("Fluxostato", "u-c", 3));

		list.append(ponto("Sinaleiro verde", "u-fantasma", 0));
		list.append(ponto("Sinaleiro vermelho", "u-fantasma", 1));

		list.append(ponto("Ainda sem cartão 1"));
		list.append(ponto("Ainda sem cartão 2"));
		list.append(ponto("Ainda sem cartão 3"));
		return list;
	}

		/// @return the four cards of the reference project
	QVector<IoTree::Card> cartoes()
	{
		QVector<IoTree::Card> cards;
		cards << cartao("u-a", "1A", "CLP-1");
		cards << cartao("u-b", "1B", "CLP-1");
		cards << cartao("u-c", "2C", "CLP-2");
		cards << cartao("u-d", "2D", "CLP-2");
		return cards;
	}
}

TEST_CASE("CU-11.9 — os números batem: o projeto é a soma dos CLPs",
	  "[io][iotree]")
{
	const IoList list = projeto();
	const IoTree::Tree tree = IoTree::build(list, cartoes());

	SECTION("nenhum ponto se perde entre os três níveis")
	{
		REQUIRE(list.count() == 15);
		REQUIRE(tree.total() == list.count());
	}

	SECTION("cada CLP é a soma dos cartões dele")
	{
		REQUIRE(tree.units.count() == 2);
		for (const IoTree::UnitGroup &unit : tree.units)
		{
			int soma = 0;
			for (const IoTree::CardGroup &card : unit.cards) {
				soma += card.total();
			}
			REQUIRE(unit.total() == soma);
		}
	}

	SECTION("os totais de cada nível são os esperados")
	{
		REQUIRE(tree.units.at(0).label == QString::fromUtf8("CLP-1"));
		REQUIRE(tree.units.at(0).total() == 6);
		REQUIRE(tree.units.at(1).label == QString::fromUtf8("CLP-2"));
		REQUIRE(tree.units.at(1).total() == 4);
		REQUIRE(tree.missing.count() == 1);
		REQUIRE(tree.missing.at(0).total() == 2);
		REQUIRE(tree.cardless.count() == 3);
	}

	SECTION("o atribuído conta canal ocupado, não cartão nomeado")
	{
			// o "Térmico M3" nomeou o cartão 1B sem tomar canal
		REQUIRE(tree.units.at(0).total() == 6);
		REQUIRE(tree.units.at(0).assigned(list) == 5);
		REQUIRE(tree.assigned(list) == 11);
		REQUIRE(tree.assigned(list) < tree.total());
	}
}

TEST_CASE("CU-11.9 — o cartão que ninguém usou aparece assim mesmo",
	  "[io][iotree]")
{
	const IoList list = projeto();
	const IoTree::Tree tree = IoTree::build(list, cartoes());

	const IoTree::UnitGroup &clp2 = tree.units.at(1);
	REQUIRE(rotulosDosCartoes(clp2)
		== QStringList({QString::fromUtf8("2C"), QString::fromUtf8("2D")}));
	REQUIRE(clp2.cards.at(1).total() == 0);
	REQUIRE(clp2.cards.at(1).channels == 16);
	REQUIRE_FALSE(clp2.cards.at(1).missing);
}

TEST_CASE("CU-11.9 — ponto que nomeia cartão inexistente não some da conta",
	  "[io][iotree]")
{
	const IoList list = projeto();
	const IoTree::Tree tree = IoTree::build(list, cartoes());

	REQUIRE(tree.missing.count() == 1);

	const IoTree::CardGroup &ausente = tree.missing.at(0);
	REQUIRE(ausente.missing);
	REQUIRE(ausente.uuid == QString::fromUtf8("u-fantasma"));

	SECTION("o uuid fica à vista, para o desenhista saber o que procurar")
	{
		REQUIRE(ausente.label.contains(QString::fromUtf8("u-fantasma")));
	}

	SECTION("e os dois pontos continuam somando no projeto")
	{
		REQUIRE(ausente.total() == 2);
		REQUIRE(tree.total() == list.count());
	}
}

TEST_CASE("CU-11.9 — quem não nomeou CLP cai num só, chamado Automate",
	  "[io][iotree]")
{
	IoList list;
	list.append(ponto("Entrada 1", "u-x", 0));
	list.append(ponto("Entrada 2", "u-y", 0));

	QVector<IoTree::Card> cards;
	cards << cartao("u-x", "X");
	cards << cartao("u-y", "Y");

	const IoTree::Tree tree = IoTree::build(list, cards);

	SECTION("um projeto de um CLP só não pergunta nada")
	{
		REQUIRE(tree.units.count() == 1);
		REQUIRE(tree.units.at(0).name.isEmpty());
		REQUIRE(tree.units.at(0).label == IoTree::unitLabel(QString()));
		REQUIRE(tree.units.at(0).cards.count() == 2);
		REQUIRE(tree.total() == 2);
	}

	SECTION("e o sem nome vem primeiro quando alguém nomeia um segundo")
	{
		cards << cartao("u-z", "Z", "CLP-B");
		list.append(ponto("Entrada 3", "u-z", 0));

		const IoTree::Tree dois = IoTree::build(list, cards);
		REQUIRE(dois.units.count() == 2);
		REQUIRE(dois.units.at(0).name.isEmpty());
		REQUIRE(dois.units.at(1).name == QString::fromUtf8("CLP-B"));
	}
}

TEST_CASE("Duas grafias do mesmo nome são o mesmo CLP", "[io][iotree]")
{
	IoList list;
	list.append(ponto("Entrada 1", "u-x", 0));
	list.append(ponto("Entrada 2", "u-y", 0));

	QVector<IoTree::Card> cards;
	cards << cartao("u-x", "X", "CLP1");
	cards << cartao("u-y", "Y", "clp1");

	const IoTree::Tree tree = IoTree::build(list, cards);

	REQUIRE(tree.units.count() == 1);
	REQUIRE(tree.units.at(0).cards.count() == 2);
	REQUIRE(tree.units.at(0).label == QString::fromUtf8("CLP1"));
	REQUIRE(tree.total() == 2);
}

TEST_CASE("A ordem da árvore é a mesma em duas leituras", "[io][iotree]")
{
	const IoList list = projeto();

	SECTION("os CLPs saem em ordem, e os cartões dentro deles também")
	{
		QVector<IoTree::Card> baralhado;
		baralhado << cartao("u-d", "2D", "CLP-2");
		baralhado << cartao("u-b", "1B", "CLP-1");
		baralhado << cartao("u-c", "2C", "CLP-2");
		baralhado << cartao("u-a", "1A", "CLP-1");

		const IoTree::Tree tree = IoTree::build(list, baralhado);
		REQUIRE(rotulos(tree)
			== QStringList({QString::fromUtf8("CLP-1"),
					QString::fromUtf8("CLP-2")}));
		REQUIRE(rotulosDosCartoes(tree.units.at(0))
			== QStringList({QString::fromUtf8("1A"),
					QString::fromUtf8("1B")}));
		REQUIRE(rotulosDosCartoes(tree.units.at(1))
			== QStringList({QString::fromUtf8("2C"),
					QString::fromUtf8("2D")}));
	}

	SECTION("dentro do cartão manda o canal, e quem não tem canal vai por último")
	{
		const IoTree::Tree tree = IoTree::build(list, cartoes());
		const IoTree::CardGroup &b = tree.units.at(0).cards.at(1);

		REQUIRE(descricoes(b, list)
			== QStringList({QString::fromUtf8("Térmico M1"),
					QString::fromUtf8("Térmico M2"),
					QString::fromUtf8("Térmico M3")}));
	}

	SECTION("a ordem de entrada dos cartões não muda a árvore")
	{
		QVector<IoTree::Card> baralhado;
		baralhado << cartao("u-c", "2C", "CLP-2");
		baralhado << cartao("u-a", "1A", "CLP-1");
		baralhado << cartao("u-d", "2D", "CLP-2");
		baralhado << cartao("u-b", "1B", "CLP-1");

		const IoTree::Tree um = IoTree::build(list, cartoes());
		const IoTree::Tree outro = IoTree::build(list, baralhado);

		REQUIRE(rotulos(um) == rotulos(outro));
		REQUIRE(rotulosDosCartoes(um.units.at(0))
			== rotulosDosCartoes(outro.units.at(0)));
		REQUIRE(um.total() == outro.total());
	}
}

TEST_CASE("A árvore vazia se diz vazia, e a de um cartão só não",
	  "[io][iotree]")
{
	SECTION("sem ponto e sem cartão")
	{
		const IoTree::Tree tree = IoTree::build(IoList(),
						       QVector<IoTree::Card>());
		REQUIRE(tree.isEmpty());
		REQUIRE(tree.total() == 0);
		REQUIRE(tree.assigned(IoList()) == 0);
	}

	SECTION("um cartão sem ponto nenhum já é coisa a mostrar")
	{
		QVector<IoTree::Card> cards;
		cards << cartao("u-a", "1A", "CLP-1");

		const IoTree::Tree tree = IoTree::build(IoList(), cards);
		REQUIRE_FALSE(tree.isEmpty());
		REQUIRE(tree.total() == 0);
		REQUIRE(tree.units.count() == 1);
		REQUIRE(tree.units.at(0).cards.count() == 1);
	}

	SECTION("uma lista importada e ainda não atribuída é toda cardless")
	{
		IoList list;
		list.append(ponto("Entrada 1"));
		list.append(ponto("Entrada 2"));

		const IoTree::Tree tree = IoTree::build(list,
						       QVector<IoTree::Card>());
		REQUIRE_FALSE(tree.isEmpty());
		REQUIRE(tree.cardless.count() == 2);
		REQUIRE(tree.total() == list.count());
		REQUIRE(tree.assigned(list) == 0);
	}
}
