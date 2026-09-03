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
#include "../../../sources/plc/ioassignment.h"
#include "../../../sources/plc/iolist.h"
#include "../../../sources/plc/iopoint.h"
#include "qt_catch_tostring.h"

#include <QList>
#include <QStringList>
#include <QVector>

namespace
{
	const QString CARTAO = QStringLiteral("{cartao-de-entrada}");

		/**
			A card of @a count identical channels, addressed the way a
			real one is: I0.0, I0.1, I0.2 and so on.
		*/
	QVector<ElementData::PlcIO> cartao(
			int count,
			ElementData::PlcIOType type = ElementData::EntreeDigitale)
	{
		QVector<ElementData::PlcIO> ios;
		for (int i = 0 ; i < count ; ++i)
		{
			ElementData::PlcIO row;
			row.type = type;
			row.address = QStringLiteral("I0.")
				      + QString::number(i);
			ios << row;
		}
		return ios;
	}

		/// One line of the sheet, the way the importer leaves it.
	IoPoint ponto(const char *tag,
		      const char *description,
		      ElementData::PlcIOType type = ElementData::EntreeDigitale)
	{
		IoPoint point;
		point.tag = QString::fromUtf8(tag);
		point.description = QString::fromUtf8(description);
		point.type = type;
		return point;
	}
}

TEST_CASE("T11 — a direção do canal manda, e o universal aceita os dois",
	  "[ioassignment]")
{
	SECTION("um ponto de saída não entra num canal de entrada")
	{
		REQUIRE_FALSE(IoAssignment::accepts(ElementData::EntreeDigitale,
						    ElementData::SortieDigitale));
		REQUIRE_FALSE(IoAssignment::accepts(ElementData::SortieDigitale,
						    ElementData::EntreeDigitale));
		REQUIRE_FALSE(IoAssignment::accepts(ElementData::EntreeUniverselle,
						    ElementData::SortieDigitale));
		REQUIRE_FALSE(IoAssignment::accepts(ElementData::SortieUniverselle,
						    ElementData::EntreeAnalogique));
	}

	SECTION("digital não entra em analógico, e o contrário também não")
	{
		REQUIRE_FALSE(IoAssignment::accepts(ElementData::EntreeDigitale,
						    ElementData::EntreeAnalogique));
		REQUIRE_FALSE(IoAssignment::accepts(ElementData::EntreeAnalogique,
						    ElementData::EntreeDigitale));
		REQUIRE_FALSE(IoAssignment::accepts(ElementData::SortieDigitale,
						    ElementData::SortieAnalogique));
	}

	SECTION("o canal universal recebe as duas naturezas da sua direção")
	{
		REQUIRE(IoAssignment::accepts(ElementData::EntreeUniverselle,
					      ElementData::EntreeDigitale));
		REQUIRE(IoAssignment::accepts(ElementData::EntreeUniverselle,
					      ElementData::EntreeAnalogique));
		REQUIRE(IoAssignment::accepts(ElementData::SortieUniverselle,
					      ElementData::SortieDigitale));
		REQUIRE(IoAssignment::accepts(ElementData::SortieUniverselle,
					      ElementData::SortieAnalogique));
	}

	SECTION("e a regra é simétrica: o ponto universal cabe no canal comum")
	{
		REQUIRE(IoAssignment::accepts(ElementData::EntreeDigitale,
					      ElementData::EntreeUniverselle));
		REQUIRE(IoAssignment::accepts(ElementData::SortieAnalogique,
					      ElementData::SortieUniverselle));
	}

	SECTION("igual com igual, sempre")
	{
		REQUIRE(IoAssignment::accepts(ElementData::EntreeDigitale,
					      ElementData::EntreeDigitale));
		REQUIRE(IoAssignment::accepts(ElementData::SortieAnalogique,
					      ElementData::SortieAnalogique));
	}
}

TEST_CASE("T11 — o canal se chama pelo que o cartão diz, nunca pelo T1",
	  "[ioassignment]")
{
	QVector<ElementData::PlcIO> ios;
	ios << ElementData::PlcIO() << ElementData::PlcIO()
	    << ElementData::PlcIO();

	SECTION("o endereço do canal é o nome dele")
	{
		ios[0].address = QStringLiteral("%I1.0");
		REQUIRE(IoAssignment::channelName(ios, 0)
			== QStringLiteral("%I1.0"));
	}

	SECTION("sem endereço, o primeiro borne que o cartão nomeia")
	{
		ios[1].terminals << QString() << QStringLiteral("11")
				 << QStringLiteral("12");
		REQUIRE(IoAssignment::channelName(ios, 1)
			== QStringLiteral("11"));
	}

	SECTION("sem endereço e sem borne, o número da linha contado de um")
	{
			//O effectiveTerminals() devolveria T1 aqui, e todo
			//cartão do projeto teria os mesmos quatro nomes.
		REQUIRE_FALSE(ios.at(2).effectiveTerminals().isEmpty());
		REQUIRE(ios.at(2).effectiveTerminals().first()
			== QStringLiteral("T1"));
		REQUIRE(IoAssignment::channelName(ios, 2)
			== QStringLiteral("#3"));
	}

	SECTION("linha que não existe não tem nome")
	{
		REQUIRE(IoAssignment::channelName(ios, -1).isEmpty());
		REQUIRE(IoAssignment::channelName(ios, 3).isEmpty());
	}
}

TEST_CASE("T11 — canal ocupado é canal ocupado, inclusive o digitado à mão",
	  "[ioassignment]")
{
	QVector<ElementData::PlcIO> ios = cartao(4);
	IoList lista;
	const QString id = lista.append(ponto("S1", "Botão de emergência"));

	SECTION("o cartão vazio tem as quatro voltas livres")
	{
		const QList<int> livres =
			IoAssignment::freeChannels(ios, lista, CARTAO);
		REQUIRE(livres.count() == 4);
		REQUIRE(livres.at(0) == 0);
		REQUIRE(livres.at(3) == 3);
	}

	SECTION("um ponto que segura a linha 2 tira ela do conjunto")
	{
		IoPoint atribuido = lista.at(0);
		atribuido.master_uuid = CARTAO;
		atribuido.io_index = 2;
		atribuido.channel = QStringLiteral("I0.2");
		REQUIRE(lista.setPoint(0, atribuido));

		REQUIRE(IoAssignment::isTaken(ios, lista, CARTAO, 2));
		const QList<int> livres =
			IoAssignment::freeChannels(ios, lista, CARTAO);
		REQUIRE(livres.count() == 3);
		REQUIRE_FALSE(livres.contains(2));
	}

	SECTION("mas o mesmo ponto não ocupa a linha 2 de outro cartão")
	{
		IoPoint atribuido = lista.at(0);
		atribuido.master_uuid = QStringLiteral("{outro-cartao}");
		atribuido.io_index = 2;
		REQUIRE(lista.setPoint(0, atribuido));

		REQUIRE_FALSE(IoAssignment::isTaken(ios, lista, CARTAO, 2));
	}

	SECTION("texto digitado na linha 1 vale tanto quanto uma atribuição")
	{
			//Alguém escreveu ali antes de existir lista de E/S.
			//Importar não é motivo para apagar isso.
		ios[1].functionText = QString::fromUtf8("Térmico do motor 1");
		REQUIRE(IoAssignment::isTaken(ios, lista, CARTAO, 1));

		const QList<int> livres =
			IoAssignment::freeChannels(ios, lista, CARTAO);
		REQUIRE(livres.count() == 3);
		REQUIRE_FALSE(livres.contains(1));

		const IoAssignment::Plan plano = IoAssignment::plan(
				lista, QStringList() << id, ios, CARTAO);
		REQUIRE(plano.pairs.count() == 1);
		REQUIRE(plano.pairs.at(0).io_index == 0);
	}

	SECTION("a linha que o chamador diz ocupada sai do conjunto")
	{
			//Ligar um escravo desenhado a um canal nao escreve nada
			//na tabela do cartao: o LinkElementCommand le dela e
			//copia para o escravo. Entao o modelo nao ve esse laco
			//sozinho, e quem tem o Element precisa dizer.
		const QList<int> livres = IoAssignment::freeChannels(
					ios, lista, CARTAO, QList<int>() << 0 << 2);
		REQUIRE(livres.count() == 2);
		REQUIRE(livres.at(0) == 1);
		REQUIRE(livres.at(1) == 3);

		const IoAssignment::Plan plano = IoAssignment::plan(
					lista, QStringList() << id, ios, CARTAO,
					QList<int>() << 0 << 2);
		REQUIRE(plano.pairs.count() == 1);
		REQUIRE(plano.pairs.at(0).io_index == 1);
	}

	SECTION("linha fora do cartão nunca está livre")
	{
		REQUIRE(IoAssignment::isTaken(ios, lista, CARTAO, 4));
		REQUIRE(IoAssignment::isTaken(ios, lista, CARTAO, -1));
	}
}

TEST_CASE("T11 — dezesseis pontos entram num cartão de dezesseis, "
	  "sem digitação",
	  "[ioassignment]")
{
	QVector<ElementData::PlcIO> ios = cartao(16);
	IoList lista;
	QStringList ids;
	for (int i = 0 ; i < 16 ; ++i)
	{
		IoPoint point;
		point.tag = QStringLiteral("S") + QString::number(i + 1);
		point.description = QString::fromUtf8("Descrição ")
				    + QString::number(i + 1);
		ids << lista.append(point);
	}

	const IoAssignment::Plan plano =
		IoAssignment::plan(lista, ids, ios, CARTAO);

	SECTION("o plano coloca os dezesseis, na ordem da planilha")
	{
		REQUIRE(plano.pairs.count() == 16);
		REQUIRE(plano.rejected.isEmpty());
		REQUIRE(plano.isClean());
		REQUIRE_FALSE(plano.isEmpty());

		for (int i = 0 ; i < 16 ; ++i)
		{
			REQUIRE(plano.pairs.at(i).point_id == ids.at(i));
			REQUIRE(plano.pairs.at(i).io_index == i);
			REQUIRE(plano.pairs.at(i).channel
				== QStringLiteral("I0.") + QString::number(i));
		}
	}

	SECTION("aplicar escreve a descrição em cada canal e prende o ponto")
	{
		REQUIRE(IoAssignment::apply(plano, lista, ios, CARTAO) == 16);

		for (int i = 0 ; i < 16 ; ++i)
		{
			REQUIRE(ios.at(i).functionText
				== QString::fromUtf8("Descrição ")
				   + QString::number(i + 1));
			REQUIRE(lista.at(i).isAssigned());
			REQUIRE(lista.at(i).master_uuid == CARTAO);
			REQUIRE(lista.at(i).io_index == i);
			REQUIRE(lista.at(i).channel
				== QStringLiteral("I0.") + QString::number(i));
		}
		REQUIRE(lista.unassigned().isEmpty());
		REQUIRE(IoAssignment::pointsOf(lista, CARTAO).count() == 16);
	}

	SECTION("o resumo diz quantos são antes de qualquer coisa acontecer")
	{
		const QString texto = plano.text();
		REQUIRE(texto.contains(QStringLiteral("16")));
		REQUIRE(texto.contains(QStringLiteral("I0.0")));
			//Oito nomes e um sinal de que há mais.
		REQUIRE(texto.contains(QString::fromUtf8("…")));
		REQUIRE_FALSE(texto.contains(QStringLiteral("I0.15")));

			//E nada foi escrito ainda.
		REQUIRE(ios.at(0).functionText.isEmpty());
		REQUIRE_FALSE(lista.at(0).isAssigned());
	}
}

TEST_CASE("T11 — o ponto sem canal compatível fica de fora, e é dito pelo nome",
	  "[ioassignment]")
{
	QVector<ElementData::PlcIO> ios = cartao(2);
	IoList lista;
	const QString id_a = lista.append(ponto("S1", "Botão liga"));
	const QString id_b = lista.append(ponto("S2", "Botão desliga"));
	const QString id_c = lista.append(ponto("K1", "Contator do motor",
						ElementData::SortieDigitale));

	SECTION("a saída não cabe num cartão de entradas")
	{
		const IoAssignment::Plan plano = IoAssignment::plan(
				lista, QStringList() << id_c, ios, CARTAO);

		REQUIRE(plano.pairs.isEmpty());
		REQUIRE(plano.isEmpty());
		REQUIRE(plano.rejected.count() == 1);
		REQUIRE(plano.rejected.at(0).reason
			== IoAssignment::NoFreeChannel);
		REQUIRE(plano.rejected.at(0).label == QStringLiteral("K1"));
		REQUIRE(plano.text().contains(QStringLiteral("K1")));
	}

	SECTION("o terceiro ponto de um cartão de dois canais fica de fora")
	{
		IoPoint terceiro = ponto("S3", "Chave seletora");
		const QString id_d = lista.append(terceiro);

		const IoAssignment::Plan plano = IoAssignment::plan(
				lista,
				QStringList() << id_a << id_b << id_d,
				ios, CARTAO);

		REQUIRE(plano.pairs.count() == 2);
		REQUIRE(plano.rejected.count() == 1);
		REQUIRE(plano.rejected.at(0).point_id == id_d);
		REQUIRE(plano.rejected.at(0).reason
			== IoAssignment::NoFreeChannel);
		REQUIRE_FALSE(plano.isClean());
	}

	SECTION("um ponto já atribuído não é atribuído de novo")
	{
		IoPoint atribuido = lista.at(0);
		atribuido.master_uuid = QStringLiteral("{outro-cartao}");
		atribuido.io_index = 7;
		REQUIRE(lista.setPoint(0, atribuido));

		const IoAssignment::Plan plano = IoAssignment::plan(
				lista, QStringList() << id_a << id_b,
				ios, CARTAO);

		REQUIRE(plano.pairs.count() == 1);
		REQUIRE(plano.pairs.at(0).point_id == id_b);
		REQUIRE(plano.rejected.count() == 1);
		REQUIRE(plano.rejected.at(0).reason
			== IoAssignment::AlreadyAssigned);
		REQUIRE(plano.text().contains(QStringLiteral("S1")));
	}

	SECTION("id que a lista não conhece é recusado, e não derruba o resto")
	{
		const IoAssignment::Plan plano = IoAssignment::plan(
				lista,
				QStringList() << QStringLiteral("{fantasma}")
					      << id_a,
				ios, CARTAO);

		REQUIRE(plano.pairs.count() == 1);
		REQUIRE(plano.pairs.at(0).point_id == id_a);
		REQUIRE(plano.rejected.count() == 1);
		REQUIRE(plano.rejected.at(0).reason
			== IoAssignment::PointNotFound);
	}

	SECTION("o mesmo ponto pedido duas vezes é um ponto, não duas recusas")
	{
		const IoAssignment::Plan plano = IoAssignment::plan(
				lista, QStringList() << id_a << id_a,
				ios, CARTAO);

		REQUIRE(plano.pairs.count() == 1);
		REQUIRE(plano.rejected.isEmpty());
	}

	SECTION("nenhum ponto para atribuir tem um resumo que diz isso")
	{
		const IoAssignment::Plan plano = IoAssignment::plan(
				lista, QStringList(), ios, CARTAO);

		REQUIRE(plano.isEmpty());
		REQUIRE_FALSE(plano.text().isEmpty());
	}
}

TEST_CASE("T11 — atribuir preenche a célula vazia e não toca no resto",
	  "[ioassignment]")
{
	QVector<ElementData::PlcIO> ios = cartao(3);
	ios[0].address.clear();
	ios[0].comment = QString::fromUtf8("Comentário do projetista");

	IoList lista;
	IoPoint point = ponto("S1", "Botão de emergência");
	point.address = QStringLiteral("%I1.0");
	point.comment = QString::fromUtf8("Vem da porta do painel");
	const QString id = lista.append(point);

	const IoAssignment::Plan plano =
		IoAssignment::plan(lista, QStringList() << id, ios, CARTAO);

	SECTION("o canal sem endereço toma o do ponto, e o plano já diz isso")
	{
		REQUIRE(plano.pairs.count() == 1);
		REQUIRE(plano.pairs.at(0).channel == QStringLiteral("%I1.0"));

		REQUIRE(IoAssignment::apply(plano, lista, ios, CARTAO) == 1);
		REQUIRE(ios.at(0).address == QStringLiteral("%I1.0"));
		REQUIRE(lista.at(0).channel == QStringLiteral("%I1.0"));
	}

	SECTION("o comentário que já estava lá não é substituído")
	{
		REQUIRE(IoAssignment::apply(plano, lista, ios, CARTAO) == 1);
		REQUIRE(ios.at(0).comment
			== QString::fromUtf8("Comentário do projetista"));
	}

	SECTION("o tipo do canal é do cartão, e a planilha não mexe nele")
	{
		ios[0].type = ElementData::EntreeUniverselle;
		REQUIRE(IoAssignment::apply(plano, lista, ios, CARTAO) == 1);
		REQUIRE(ios.at(0).type == ElementData::EntreeUniverselle);
	}

	SECTION("sem descrição, a etiqueta é o que o canal passa a mostrar")
	{
		IoList outra;
		IoPoint so_etiqueta;
		so_etiqueta.tag = QStringLiteral("S9");
		const QString id_b = outra.append(so_etiqueta);

		QVector<ElementData::PlcIO> outros = cartao(1);
		const IoAssignment::Plan plano_b = IoAssignment::plan(
				outra, QStringList() << id_b, outros, CARTAO);
		REQUIRE(IoAssignment::apply(plano_b, outra, outros, CARTAO) == 1);
		REQUIRE(outros.at(0).functionText == QStringLiteral("S9"));
	}

	SECTION("aplicar duas vezes o mesmo plano não atribui duas vezes")
	{
		REQUIRE(IoAssignment::apply(plano, lista, ios, CARTAO) == 1);
		REQUIRE(IoAssignment::apply(plano, lista, ios, CARTAO) == 0);
		REQUIRE(IoAssignment::pointsOf(lista, CARTAO).count() == 1);
	}
}

TEST_CASE("T11 — devolver o canal não apaga o que foi digitado depois",
	  "[ioassignment]")
{
	QVector<ElementData::PlcIO> ios = cartao(3);
	IoList lista;
	const QString id_a = lista.append(ponto("S1", "Botão liga"));
	const QString id_b = lista.append(ponto("S2", "Botão desliga"));

	const IoAssignment::Plan plano = IoAssignment::plan(
			lista, QStringList() << id_a << id_b, ios, CARTAO);
	REQUIRE(IoAssignment::apply(plano, lista, ios, CARTAO) == 2);

	SECTION("o ponto volta para o conjunto dos soltos e o canal esvazia")
	{
		REQUIRE(IoAssignment::release(lista, ios, CARTAO,
					      QList<int>() << 0) == 1);

		REQUIRE(ios.at(0).functionText.isEmpty());
		REQUIRE_FALSE(lista.at(0).isAssigned());
		REQUIRE(lista.at(0).master_uuid.isEmpty());
		REQUIRE(lista.at(0).io_index == -1);
		REQUIRE(lista.at(0).channel.isEmpty());

			//E o outro continua onde estava.
		REQUIRE(lista.at(1).isAssigned());
		REQUIRE(ios.at(1).functionText == QStringLiteral("Botão desliga"));
	}

	SECTION("o canal editado à mão desde então fica como está")
	{
		ios[0].functionText = QString::fromUtf8("Botão liga do motor 2");
		REQUIRE(IoAssignment::release(lista, ios, CARTAO,
					      QList<int>() << 0) == 1);

		REQUIRE(ios.at(0).functionText
			== QString::fromUtf8("Botão liga do motor 2"));
		REQUIRE_FALSE(lista.at(0).isAssigned());
	}

	SECTION("devolver o cartão inteiro solta os dois")
	{
		REQUIRE(IoAssignment::release(lista, ios, CARTAO,
					      QList<int>() << 0 << 1 << 2) == 2);
		REQUIRE(lista.unassigned().count() == 2);
		REQUIRE(IoAssignment::pointsOf(lista, CARTAO).isEmpty());
	}

	SECTION("devolver canal de outro cartão não solta ninguém")
	{
		REQUIRE(IoAssignment::release(lista, ios,
					      QStringLiteral("{outro-cartao}"),
					      QList<int>() << 0 << 1) == 0);
		REQUIRE(lista.at(0).isAssigned());
		REQUIRE(lista.at(1).isAssigned());
	}

	SECTION("e o canal liberado volta a ser oferecido")
	{
		REQUIRE(IoAssignment::release(lista, ios, CARTAO,
					      QList<int>() << 0) == 1);
		const QList<int> livres =
			IoAssignment::freeChannels(ios, lista, CARTAO);
		REQUIRE(livres.count() == 2);
		REQUIRE(livres.at(0) == 0);
		REQUIRE(livres.at(1) == 2);
	}
}
