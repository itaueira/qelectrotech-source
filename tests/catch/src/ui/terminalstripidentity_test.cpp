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
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/terminalstripdata.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QString>
#include <QUuid>
#include <QVector>

/*
	A régua de bornes continua sendo a mesma régua depois de gravar as
	propriedades dela.

	Isto parece contabilidade interna e é a causa de um defeito relatado da
	prancheta: uma borne independente que não ia para a X2 de jeito nenhum.
	O caminho é este, e cada passo dele é código que existe hoje:

	1. a lista "deslocar para" é preenchida quando a página abre, e guarda o
	   uuid de cada régua, não o ponteiro dela;
	2. o botão Aplicar monta um TerminalStripData novo com os cinco campos
	   da tela e o entrega a TerminalStrip::setData();
	3. um TerminalStripData recém-construído traz um uuid próprio, e o
	   operador de atribuição dele copia o uuid como copia qualquer campo;
	4. a régua passava, então, a atender por um uuid que ninguém tinha
	   guardado — e o botão de mover procurava pelo antigo, não achava
	   régua nenhuma, e voltava **sem dizer nada**.

	O comentário de documentação de setData() sempre prometeu o contrário:
	"the uuid of the new data is set to the uuid of the previous data to
	keep the uuid of the terminal strip unchanged". O corpo é que não fazia.
	É por isso que o caso está escrito contra o contrato documentado, e não
	contra o botão: quem apagar as duas linhas do conserto derruba este
	caso, e não precisa de janela aberta para descobrir.

	Rotulado T33 e não CU-33.x: o caso de uso é mover a borne olhando a
	tela, e o que se prova aqui é a identidade que o movimento depende.
*/

namespace
{
	/**
		O menor projeto que abre: uma folha vazia, e nada nela.

		Copiado de backup_test.cpp em vez de encurtado mais: a coleção
		vazia está ali porque é assim que o projeto abre, e bancada que
		economiza no que não entendeu falha por motivo que não é o do
		caso.
	*/
	QString fixtureXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/**
		A régua de @a project cujo uuid é @a uuid, nula quando nenhuma é.

		A mesma busca que TerminalStripEditor::on_m_move_to_pb_clicked()
		faz para achar a régua de destino, e é de propósito: o que se
		mede é se aquela busca ainda acha o que a lista prometeu.
	*/
	TerminalStrip *stripForUuid(QETProject *project, const QUuid &uuid)
	{
		const auto strips = project->terminalStrip();
		for (const auto &strip : strips)
		{
			if (strip->uuid() == uuid) {
				return strip;
			}
		}
		return nullptr;
	}
}

TEST_CASE("T33 — gravar as propriedades da régua não troca a identidade dela",
	  "[terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml());
	REQUIRE(bench.isOpen());

	auto x1 = bench->newTerminalStrip(QStringLiteral("="),
					  QStringLiteral("+"),
					  QStringLiteral("X1"));
	auto x2 = bench->newTerminalStrip(QStringLiteral("="),
					  QStringLiteral("+"),
					  QStringLiteral("X2"));
	REQUIRE(x1);
	REQUIRE(x2);
	REQUIRE(x1->uuid() != x2->uuid());

		//O que a lista de destinos guardou quando a página abriu.
	const auto announced = x2->uuid();

	SECTION("o uuid entregue é descartado, e o da régua fica")
	{
			//Os dados de outra régua servem de "dado novo com uuid
			//diferente" sem que o caso precise construir um
			//TerminalStripData de campos privados — e provam mais:
			//nem mesmo um uuid que existe entra por aqui.
		x2->setData(x1->data());

		CHECK(x2->uuid() == announced);
		CHECK(x2->uuid() != x1->uuid());
	}

	SECTION("os cinco campos continuam viajando")
	{
			//O conserto guarda o uuid e não pode guardar o resto:
			//uma implementação que ignorasse o dado inteiro também
			//passaria na seção acima.
		x2->setData(x1->data());

		CHECK(x2->name() == QStringLiteral("X1"));
	}

	SECTION("a busca do botão de mover continua achando a régua")
	{
		x2->setData(x1->data());

		CHECK(stripForUuid(bench.project(), announced) == x2);
	}
}
