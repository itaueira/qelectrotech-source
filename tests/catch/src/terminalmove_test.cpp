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
#include "../../../sources/TerminalStrip/terminalmove.h"
#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QString>
#include <QVector>

/*
	Moving a terminal into a terminal strip: what the button does, and what
	it says when it does nothing.

	The defect this answers was reported from a drawing office, and it is
	worth stating because it is not a wrong result - it is no result at
	all. Somebody tried to move an independent terminal into the strip X2,
	clicked, and nothing happened: no terminal moved, no sentence, no
	disabled button, nothing to tell "I refused" from "I did not
	understand". The move slot had three ways out that wrote nothing and
	said nothing.

	What is proved here is the rule alone: which of the refusals a reading
	of the dialogue earns, and that every one of them carries a sentence.
	No window is opened - there is no window in this suite - so what the
	status bar of the terminal strip window ends up showing stays a matter
	for a person to look at, and is listed as such in the task.

	Labelled T33 and not CU-33.x: the use cases of the terminal strip are
	about a strip being edited on screen, and this is the rule underneath
	one button of it.
*/

namespace
{
	using Decision = TerminalMove::Decision;
	using Destination = TerminalMove::Destination;
	using Refusal = TerminalMove::Refusal;

		/// The four refusals, so that a case can walk all of them.
	QVector<Refusal> refusals()
	{
		return QVector<Refusal>{Refusal::NothingSelected,
					Refusal::SelectionNotTerminal,
					Refusal::NoDestination,
					Refusal::DestinationVanished};
	}
}

TEST_CASE("T33 — a recusa de mover diz o que houve, e nunca sai calada",
	  "[terminalstrip]")
{
	SECTION("cada recusa tem uma frase, e a não recusa não tem nenhuma")
	{
			//A razão de o módulo existir: o defeito era um botão que
			//voltava sem dizer nada, e uma recusa nova sem frase
			//seria o mesmo defeito entrando de novo.
		CHECK(Decision::describe(Refusal::None).isEmpty());

		for (const Refusal refusal : refusals()) {
			CHECK_FALSE(Decision::describe(refusal).isEmpty());
		}
	}

	SECTION("duas recusas não dizem a mesma coisa")
	{
			//Frase repetida em duas recusas é uma frase que não diz
			//qual das duas aconteceu — e quem lê fica sabendo tanto
			//quanto sabia com o silêncio.
		const auto all = refusals();
		for (int i = 0 ; i < all.size() ; ++i)
		{
			for (int j = i + 1 ; j < all.size() ; ++j) {
				CHECK(Decision::describe(all.at(i))
				      != Decision::describe(all.at(j)));
			}
		}
	}
}

TEST_CASE("T33 — a leitura do diálogo decide qual recusa é", "[terminalstrip]")
{
	SECTION("nada selecionado")
	{
		const auto decision = TerminalMove::decide(0, 0, Destination::Resolved);

		CHECK_FALSE(decision.isValid());
		CHECK(decision.refusal == Refusal::NothingSelected);
	}

	SECTION("selecionado, mas a seleção não nomeia borne nenhum")
	{
			//Não é a mesma coisa que não ter selecionado nada, e
			//mandar "selecione uma borne" quem acabou de selecionar
			//uma linha é conselho que não leva a lugar nenhum.
		const auto decision = TerminalMove::decide(3, 0, Destination::Resolved);

		CHECK(decision.refusal == Refusal::SelectionNotTerminal);
	}

	SECTION("a lista de destinos está vazia")
	{
			//Julgada antes da seleção de propósito: sem régua
			//nenhuma no projeto, selecionar borne não adianta, e
			//mandar selecionar levaria a um segundo beco sem saída.
		const auto decision = TerminalMove::decide(0, 0, Destination::Empty);

		CHECK(decision.refusal == Refusal::NoDestination);
	}

	SECTION("o destino escolhido não foi encontrado")
	{
		const auto decision = TerminalMove::decide(2, 1, Destination::Vanished);

		CHECK(decision.refusal == Refusal::DestinationVanished);
	}

	SECTION("pedido inteiro: nada a recusar")
	{
		const auto decision = TerminalMove::decide(2, 1, Destination::Resolved);

		CHECK(decision.isValid());
		CHECK(decision.refusal == Refusal::None);
		CHECK(decision.describe().isEmpty());
	}

	SECTION("uma seleção de uma célula só é seleção")
	{
			//A tabela devolve célula, e não linha: uma linha
			//selecionada pode chegar aqui como uma célula ou como
			//cinco, e as duas contam como seleção.
		CHECK(TerminalMove::decide(1, 1, Destination::Resolved).isValid());
		CHECK(TerminalMove::decide(5, 1, Destination::Resolved).isValid());
	}
}

TEST_CASE("T33 — o que o clique fez também é dito", "[terminalstrip]")
{
	SECTION("a frase do que foi movido nomeia o destino que a pessoa leu")
	{
		const auto said = Decision::describeMove(
					2, QStringLiteral("=INST +LOC X2"));

		CHECK_FALSE(said.isEmpty());
		CHECK(said.contains(QStringLiteral("=INST +LOC X2")));
		CHECK(said.contains(QStringLiteral("2")));
	}

	SECTION("aplicar sem nada para gravar nomeia o botão que move")
	{
			//É o conserto da confusão medida: o relato foi de quem
			//clicou em Aplicar esperando que a borne andasse. A
			//frase que ele recebe agora diz o nome do botão que
			//anda com ela.
		const auto said = Decision::describeApply(0);

		CHECK_FALSE(said.isEmpty());
		CHECK(said.contains(QString::fromUtf8("Déplacer")));
	}

	SECTION("aplicar com algo gravado não repete a lição")
	{
			//Dica repetida a cada clique bem-sucedido é dica que
			//ninguém lê mais.
		const auto said = Decision::describeApply(3);

		CHECK_FALSE(said.isEmpty());
		CHECK_FALSE(said.contains(QString::fromUtf8("Déplacer")));
		CHECK(said != Decision::describeApply(0));
	}
}
