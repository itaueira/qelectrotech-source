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
#include "../../../sources/location/mountingalign.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <limits>

/*
	Alinhar e distribuir uma seleção: média, mínimo e divisão sobre uma
	lista de retângulos.

	Tudo aqui é número — nada de cena, nada de seleção —, e o que se afirma
	é o milímetro de chegada de cada peça, nunca "mudou". A conta que mais
	erra é a do espaçamento: a sobra se divide pelo número de ESPAÇOS, que é
	um a menos que o de peças, e o erro de um só aparece quando se soma o
	resultado de volta. Por isso cada caso de distribuição afirma também
	onde termina a última peça.

	E cada afirmação vem em par: a peça que se move e a que não se move; a
	seleção que o gesto aceita e a que ele devolve vazia; o trilho que fica
	parado e os componentes que não ficam.
*/

namespace
{
	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();

	/// @return um componente de @a largura por @a altura no canto (x, y)
	MountedItem peca(const QString &uuid, qreal x, qreal y,
			 qreal largura = 22.5, qreal altura = 85.0)
	{
		MountedItem item(QStringLiteral("-") + uuid.toUpper(),
				 QPointF(x, y), QSizeF(largura, altura));
		item.uuid = uuid;
		item.part_code = QStringLiteral("A9F74210");

		return item;
	}

	/// @return um trilho deitado de 600, que nenhum dos dois gestos move
	MountedItem trilho(const QString &uuid, qreal y)
	{
		const MountingProfile perfil = MountingProfile::rail(35.0, 7.5);

		MountedItem item(QString(), QPointF(0.0, y),
				 perfil.sizeFor(600.0, MountingRun::Across));
		item.uuid = uuid;
		item.profile = perfil;
		item.run = MountingRun::Across;

		return item;
	}

	/// @return uma peça que ninguém mediu: um ponto com nome
	MountedItem semMedida(const QString &uuid, qreal x, qreal y)
	{
		MountedItem item(QStringLiteral("-KA1"), QPointF(x, y), QSizeF());
		item.uuid = uuid;

		return item;
	}
}

TEST_CASE("T19 — alinhar leva ao extremo da seleção, e só quem não está lá",
	  "[calepinagem]")
{
	SECTION("à esquerda: o extremo fica, os outros vêm")
	{
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 10.0, 0.0)
						   << peca(QStringLiteral("b"), 50.0, 100.0)
						   << peca(QStringLiteral("c"), 30.0, 200.0);

		const QHash<QString, QPointF> movidos =
				MountingAlign::aligned(escolha,
						       MountingAlignment::LeftEdges);

		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::LeftEdges)
		      == Approx(10.0));

			//Duas entradas e não três: a peça que já estava na
			//linha não é passo de desfazer nenhum.
		CHECK(movidos.count() == 2);
		CHECK_FALSE(movidos.contains(QStringLiteral("a")));

		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(10.0));
		CHECK(movidos.value(QStringLiteral("c")).x() == Approx(10.0));

			//E o outro eixo não se move: alinhar à esquerda não é
			//empilhar tudo numa linha.
		CHECK(movidos.value(QStringLiteral("b")).y() == Approx(100.0));
		CHECK(movidos.value(QStringLiteral("c")).y() == Approx(200.0));
	}

	SECTION("à direita: a borda direita é que fica igual, não o canto")
	{
			//Uma peça de 22,5 e uma de 45: alinhar à direita põe os
			//cantos em lugares diferentes de propósito.
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 100.0, 0.0, 22.5)
						   << peca(QStringLiteral("b"), 0.0, 100.0, 45.0);

		const QHash<QString, QPointF> movidos =
				MountingAlign::aligned(escolha,
						       MountingAlignment::RightEdges);

			//A borda direita mais à direita é 100 + 22,5 = 122,5.
		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::RightEdges)
		      == Approx(122.5));

		CHECK(movidos.count() == 1);
		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(77.5));
	}

	SECTION("em cima e no centro horizontal são dois números diferentes")
	{
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 0.0, 100.0, 22.5, 85.0)
						   << peca(QStringLiteral("b"), 50.0, 300.0, 22.5, 45.0);

			//A caixa da seleção vai de y 100 a y 345.
		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::TopEdges)
		      == Approx(100.0));
		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::BottomEdges)
		      == Approx(345.0));
		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::HorizontalAxes)
		      == Approx(222.5));

		const QHash<QString, QPointF> centrados =
				MountingAlign::aligned(escolha,
						       MountingAlignment::HorizontalAxes);

			//222,5 menos metade de cada altura: 180 e 200.
		CHECK(centrados.value(QStringLiteral("a")).y() == Approx(180.0));
		CHECK(centrados.value(QStringLiteral("b")).y() == Approx(200.0));
	}

	SECTION("uma peça sozinha não se alinha com coisa nenhuma")
	{
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 10.0, 0.0);

		CHECK(MountingAlign::aligned(escolha,
					     MountingAlignment::LeftEdges)
		      .isEmpty());
	}
}

TEST_CASE("T19 — o trilho não é alinhado, e os componentes são",
	  "[calepinagem]")
{
	const QList<MountedItem> escolha = QList<MountedItem>()
					   << trilho(QStringLiteral("trilho-1"), 100.0)
					   << peca(QStringLiteral("a"), 40.0, 60.0)
					   << peca(QStringLiteral("b"), 90.0, 60.0);

	SECTION("a barra fica onde está, mesmo sendo o extremo da seleção")
	{
			//O trilho começa em x 0 e seria o extremo esquerdo. Ele
			//não entra na conta: o extremo é o componente em 40.
		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::LeftEdges)
		      == Approx(40.0));

		const QHash<QString, QPointF> movidos =
				MountingAlign::aligned(escolha,
						       MountingAlignment::LeftEdges);

		CHECK(movidos.count() == 1);
		CHECK_FALSE(movidos.contains(QStringLiteral("trilho-1")));
		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(40.0));
	}

	SECTION("quem se move é dito por escrito, e o trilho não está na lista")
	{
		const QStringList moveis = MountingAlign::movableUuids(escolha);

		CHECK(moveis.count() == 2);
		CHECK_FALSE(moveis.contains(QStringLiteral("trilho-1")));
		CHECK_FALSE(MountingAlign::isMovable(trilho(QStringLiteral("x"),
							   0.0)));
	}

	SECTION("dois trilhos e nada mais não alinham nada")
	{
		const QList<MountedItem> barras = QList<MountedItem>()
						  << trilho(QStringLiteral("trilho-1"), 100.0)
						  << trilho(QStringLiteral("trilho-2"), 300.0);

		CHECK(MountingAlign::aligned(barras,
					     MountingAlignment::LeftEdges)
		      .isEmpty());
	}
}

TEST_CASE("T19 — a peça sem medida entra na conta como o ponto que ela é",
	  "[calepinagem]")
{
	SECTION("ela é o extremo, e os outros vêm até ela")
	{
			//O caso que a caixa da seleção erraria se fosse feita
			//com QRectF::united: a pegada de uma peça sem medida é
			//nula, united devolve "a outra", e a peça sumiria da
			//seleção — alinhando tudo pela segunda da esquerda.
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << semMedida(QStringLiteral("ka1"), 5.0, 0.0)
						   << peca(QStringLiteral("a"), 10.0, 100.0)
						   << peca(QStringLiteral("b"), 50.0, 200.0);

		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::LeftEdges)
		      == Approx(5.0));

		const QHash<QString, QPointF> movidos =
				MountingAlign::aligned(escolha,
						       MountingAlignment::LeftEdges);

		CHECK(movidos.count() == 2);
		CHECK(movidos.value(QStringLiteral("a")).x() == Approx(5.0));
		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(5.0));
	}

	SECTION("e ela não ganha tamanho nenhum por ter sido alinhada")
	{
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << semMedida(QStringLiteral("ka1"), 500.0, 0.0)
						   << peca(QStringLiteral("a"), 10.0, 100.0);

		const QHash<QString, QPointF> movidos =
				MountingAlign::aligned(escolha,
						       MountingAlignment::LeftEdges);

			//Ela vem para 10, como ponto: a esquerda e a direita
			//dela são o mesmo lugar, e nada aqui inventa 20 mm de
			//caixa para ela.
		CHECK(movidos.value(QStringLiteral("ka1")).x() == Approx(10.0));
	}
}

TEST_CASE("T19 — o que não é número não se alinha, e não estraga a linha",
	  "[calepinagem]")
{
	MountedItem perdida = peca(QStringLiteral("a"), 0.0, 0.0);
	perdida.position = QPointF(not_a_number, 0.0);

	const QList<MountedItem> escolha = QList<MountedItem>()
					   << perdida
					   << peca(QStringLiteral("b"), 60.0, 100.0)
					   << peca(QStringLiteral("c"), 90.0, 200.0);

	SECTION("a linha é a dos que têm posição")
	{
		CHECK(MountingAlign::referenceOf(escolha,
						 MountingAlignment::LeftEdges)
		      == Approx(60.0));
	}

	SECTION("e a peça sem posição não é movida para lugar nenhum")
	{
		const QHash<QString, QPointF> movidos =
				MountingAlign::aligned(escolha,
						       MountingAlignment::LeftEdges);

		CHECK_FALSE(movidos.contains(QStringLiteral("a")));
		CHECK(movidos.value(QStringLiteral("c")).x() == Approx(60.0));
	}

	SECTION("a peça sem identidade também não")
	{
		MountedItem anonima = peca(QStringLiteral("d"), 500.0, 0.0);
		anonima.uuid.clear();

		const QList<MountedItem> com_anonima = QList<MountedItem>()
						       << anonima
						       << peca(QStringLiteral("b"), 60.0, 100.0)
						       << peca(QStringLiteral("c"), 90.0, 200.0);

			//Ela não entra nem na linha: a caixa vai de 60 a 112,5,
			//e não até 500.
		CHECK(MountingAlign::referenceOf(com_anonima,
						 MountingAlignment::LeftEdges)
		      == Approx(60.0));
		CHECK(MountingAlign::aligned(com_anonima,
					     MountingAlignment::LeftEdges)
		      .count() == 1);
	}
}

TEST_CASE("T19 — distribuir deixa o mesmo vão, e as pontas ficam",
	  "[calepinagem]")
{
	SECTION("quatro disjuntores entre 0 e 300 ficam com setenta de vão")
	{
			//Quatro peças de 22,5 tomam 90; a seleção ocupa de 0 a
			//300, logo sobram 210 para TRÊS espaços: 70 cada.
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 0.0, 100.0)
						   << peca(QStringLiteral("b"), 40.0, 100.0)
						   << peca(QStringLiteral("c"), 120.0, 100.0)
						   << peca(QStringLiteral("d"), 277.5, 100.0);

		CHECK(MountingAlign::spreadGap(escolha, MountingRun::Across)
		      == Approx(70.0));

		const QHash<QString, QPointF> movidos =
				MountingAlign::spread(escolha,
						      MountingRun::Across);

			//As duas pontas não se movem: só as duas do meio.
		CHECK(movidos.count() == 2);
		CHECK_FALSE(movidos.contains(QStringLiteral("a")));
		CHECK_FALSE(movidos.contains(QStringLiteral("d")));

		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(92.5));
		CHECK(movidos.value(QStringLiteral("c")).x() == Approx(185.0));

			//E a conta fecha na volta: 185 + 22,5 + 70 = 277,5, que
			//é exatamente onde a última já estava. É essa soma que
			//denuncia o erro de dividir por quatro em vez de três.
		CHECK(185.0 + 22.5 + 70.0 == Approx(277.5));
	}

	SECTION("o vão é entre corpos, e não entre centros")
	{
			//Duas de 22,5 e uma de 45, de 0 a 300: 90 de corpo,
			//210 de sobra, DOIS espaços de 105.
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 0.0, 100.0, 22.5)
						   << peca(QStringLiteral("b"), 200.0, 100.0, 22.5)
						   << peca(QStringLiteral("c"), 255.0, 100.0, 45.0);

		CHECK(MountingAlign::spreadGap(escolha, MountingRun::Across)
		      == Approx(105.0));

		const QHash<QString, QPointF> movidos =
				MountingAlign::spread(escolha,
						      MountingRun::Across);

		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(127.5));

			//Os centros ficam a 127,5 e a 138,75 um do outro — de
			//propósito. Distribuir centros daria vãos diferentes
			//entre corpos de larguras diferentes, e é o vão que o
			//dedo e o fio precisam.
		const qreal centro_a = 0.0 + 22.5 / 2.0;
		const qreal centro_b = 127.5 + 22.5 / 2.0;
		const qreal centro_c = 255.0 + 45.0 / 2.0;

		CHECK(centro_b - centro_a == Approx(127.5));
		CHECK(centro_c - centro_b == Approx(138.75));
	}

	SECTION("distribuir para baixo distribui o outro eixo, e só ele")
	{
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 50.0, 0.0, 22.5, 50.0)
						   << peca(QStringLiteral("b"), 50.0, 60.0, 22.5, 50.0)
						   << peca(QStringLiteral("c"), 50.0, 250.0, 22.5, 50.0);

			//Três peças de 50 de altura entre 0 e 300: 150 de
			//corpo, 150 de sobra, dois espaços de 75.
		CHECK(MountingAlign::spreadGap(escolha, MountingRun::Down)
		      == Approx(75.0));

		const QHash<QString, QPointF> movidos =
				MountingAlign::spread(escolha, MountingRun::Down);

		CHECK(movidos.count() == 1);
		CHECK(movidos.value(QStringLiteral("b")).y() == Approx(125.0));
		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(50.0));
	}

	SECTION("a ordem é a da fileira, e não a da seleção")
	{
			//Entregues fora de ordem: a distribuição é a mesma, e
			//quem não se move continua sendo quem está nas pontas.
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("c"), 120.0, 100.0)
						   << peca(QStringLiteral("d"), 277.5, 100.0)
						   << peca(QStringLiteral("a"), 0.0, 100.0)
						   << peca(QStringLiteral("b"), 40.0, 100.0);

		const QHash<QString, QPointF> movidos =
				MountingAlign::spread(escolha,
						      MountingRun::Across);

		CHECK(movidos.count() == 2);
		CHECK(movidos.value(QStringLiteral("b")).x() == Approx(92.5));
		CHECK(movidos.value(QStringLiteral("c")).x() == Approx(185.0));
	}

	SECTION("duas peças não se distribuem: as duas são ponta")
	{
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 0.0, 100.0)
						   << peca(QStringLiteral("b"), 200.0, 100.0);

		CHECK(qIsNaN(MountingAlign::spreadGap(escolha,
						      MountingRun::Across)));
		CHECK(MountingAlign::spread(escolha, MountingRun::Across)
		      .isEmpty());
	}

	SECTION("quando não cabem, o vão é negativo e diz quanto falta")
	{
			//Quatro peças de 100 numa fileira que ocupa 300: 400 de
			//corpo em 300 de espaço. O vão é negativo, e é por isso
			//que quem oferece o gesto pergunta antes de aplicar.
		const QList<MountedItem> escolha = QList<MountedItem>()
						   << peca(QStringLiteral("a"), 0.0, 100.0, 100.0)
						   << peca(QStringLiteral("b"), 50.0, 100.0, 100.0)
						   << peca(QStringLiteral("c"), 120.0, 100.0, 100.0)
						   << peca(QStringLiteral("d"), 200.0, 100.0, 100.0);

		const qreal vao = MountingAlign::spreadGap(escolha,
							   MountingRun::Across);

		CHECK(vao < 0.0);
		CHECK(vao == Approx(-100.0 / 3.0));
	}
}
