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
#include "../../../sources/location/mountingclip.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <limits>

/*
	O que está clipsado em qual trilho, e onde a peça para quando é.

	Tudo aqui é retângulo e milímetro — nada de cena, nada de item gráfico,
	nada de catálogo —, e é de propósito: uma regra de geometria que só se
	exercita arrastando alguma coisa na tela é uma regra que ninguém confere
	duas vezes. O gesto — soltar sobre o trilho, arrastar o trilho, e o que
	cada um deixa na pilha de desfazer — está em src/ui/mountingclipscene_test.cpp,
	e não se repete aqui.

	Cada afirmação vem em PAR, e o par não é decoração: um encaixe que
	grudasse tudo em tudo passaria em todos os casos positivos deste arquivo.
	Para cada peça que encaixa há a que não encaixa, para cada trilho que
	leva os seus há o que não leva os do vizinho, e a posição afirmada é o
	milímetro resultante — porque é em milímetro que a chapa é furada, e
	"mudou" não é resposta que se leve para a oficina.
*/

namespace
{
	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();

	/// milímetro: a seção do trilho DIN de todo painel
	const qreal SECAO = 35.0;

	/// milímetro: o comprimento em que os trilhos deste arquivo são cortados
	const qreal COMPRIMENTO = 600.0;

	/// milímetro: a largura de um disjuntor unipolar, e a altura dele
	const qreal LARGURA_DISJUNTOR = 22.5;
	const qreal ALTURA_DISJUNTOR = 85.0;

	/**
		milímetro: onde o eixo do disjuntor fica dentro do corpo dele —
		no meio da largura, e 45 abaixo do topo. É o número que o
		catálogo guarda por código de peça, e é ele que separa uma
		fileira alinhada de uma fileira alinhada pelo canto.
	*/
	const QPointF EIXO_DISJUNTOR(11.25, 45.0);

	const QString CODIGO_DISJUNTOR = QStringLiteral("A9F74210");

	/// @return um trilho deitado, cortado em @a comprimento, com o topo em @a y
	MountedItem trilho(const QString &uuid,
			   qreal y,
			   qreal comprimento = COMPRIMENTO)
	{
		const MountingProfile perfil = MountingProfile::rail(SECAO, 7.5);

		MountedItem item(QString(), QPointF(0.0, y),
				 perfil.sizeFor(comprimento, MountingRun::Across));
		item.uuid = uuid;
		item.profile = perfil;
		item.run = MountingRun::Across;

		return item;
	}

	/// @return um trilho em pé, cortado em @a comprimento, com o canto em @a x
	MountedItem trilhoEmPe(const QString &uuid,
			       qreal x,
			       qreal comprimento = COMPRIMENTO)
	{
		const MountingProfile perfil = MountingProfile::rail(SECAO, 7.5);

		MountedItem item(QString(), QPointF(x, 0.0),
				 perfil.sizeFor(comprimento, MountingRun::Down));
		item.uuid = uuid;
		item.profile = perfil;
		item.run = MountingRun::Down;

		return item;
	}

	/// @return uma canaleta deitada, que é peça cortada e não trilho
	MountedItem canaleta(const QString &uuid, qreal y)
	{
		const MountingProfile perfil = MountingProfile::duct(40.0, 60.0);

		MountedItem item(QString(), QPointF(0.0, y),
				 perfil.sizeFor(COMPRIMENTO, MountingRun::Across));
		item.uuid = uuid;
		item.profile = perfil;
		item.run = MountingRun::Across;

		return item;
	}

	/// @return um disjuntor unipolar com o canto superior esquerdo em (x, y)
	MountedItem disjuntor(const QString &uuid, qreal x, qreal y)
	{
		MountedItem item(QStringLiteral("-Q1"), QPointF(x, y),
				 QSizeF(LARGURA_DISJUNTOR, ALTURA_DISJUNTOR));
		item.uuid = uuid;
		item.part_code = CODIGO_DISJUNTOR;

		return item;
	}

	/// @return uma peça que ninguém mediu: tem nome, e não tem tamanho
	MountedItem semMedida(const QString &uuid, qreal x, qreal y)
	{
		MountedItem item(QStringLiteral("-KA1"), QPointF(x, y), QSizeF());
		item.uuid = uuid;
		item.part_code = QStringLiteral("RXM4AB2BD");

		return item;
	}

	/// @return o que o catálogo diz sobre o eixo do disjuntor
	QHash<QString, QPointF> eixos()
	{
		QHash<QString, QPointF> axes;
		axes.insert(CODIGO_DISJUNTOR, EIXO_DISJUNTOR);

		return axes;
	}

	/// @return o eixo do trilho, em milímetro, lido do próprio retângulo
	qreal eixoDe(const MountedItem &item)
	{
		return item.footprint().center().y();
	}
}

TEST_CASE("T19 — o que cobre o trilho encaixa nele, e o que está longe não",
	  "[calepinagem]")
{
	const MountedItem barra = trilho(QStringLiteral("trilho-1"), 100.0);
	const qreal eixo = eixoDe(barra);

		//O trilho vai de y 100 a y 135, e o eixo dele é 117,5.
	REQUIRE(eixo == Approx(117.5));
	REQUIRE(MountingClip::axisOf(barra) == Approx(117.5));

	SECTION("a peça que cobre o trilho é segurada por ele")
	{
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);

			//O corpo vai de 60 a 145 e passa por 117,5.
		CHECK(MountingClip::holds(barra, peca));

		const QList<MountedItem> tudo = QList<MountedItem>()
						<< barra << peca;

		CHECK(MountingClip::carrierOf(peca, tudo, eixos())
		      == QStringLiteral("trilho-1"));
	}

	SECTION("a peça longe do trilho não é segurada por ele")
	{
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 300.0);

			//O corpo vai de 300 a 385 e o eixo está 182,5 acima.
		CHECK_FALSE(MountingClip::holds(barra, peca));

		const QList<MountedItem> tudo = QList<MountedItem>()
						<< barra << peca;

		CHECK(MountingClip::carrierOf(peca, tudo, eixos()).isEmpty());
	}

	SECTION("a peça que para pouco antes do trilho ainda é pega")
	{
			//O corpo termina a dois milímetros menos que a
			//distância de pega, acima do eixo: a mão errou por
			//pouco e o gesto vale.
		const qreal falta = MountingClip::grabMargin() - 2.0;
		const qreal topo = eixo - ALTURA_DISJUNTOR - falta;
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, topo);

		CHECK(MountingClip::holds(barra, peca));
	}

	SECTION("a peça que para um pouco além da distância de pega não é")
	{
		const qreal falta = MountingClip::grabMargin() + 2.0;
		const qreal topo = eixo - ALTURA_DISJUNTOR - falta;
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, topo);

		CHECK_FALSE(MountingClip::holds(barra, peca));
	}

	SECTION("a peça além da ponta do trilho não é segurada, mesmo na altura")
	{
			//O trilho vai de x 0 a x 600. Esta peça começa em 620,
			//na altura exata do eixo: é o par da de baixo, e o que
			//as separa é só o eixo do comprimento.
		const MountedItem fora = disjuntor(QStringLiteral("q1"),
						   620.0, 60.0);
		const MountedItem dentro = disjuntor(QStringLiteral("q2"),
						     590.0, 60.0);

		CHECK_FALSE(MountingClip::holds(barra, fora));
		CHECK(MountingClip::holds(barra, dentro));
	}
}

TEST_CASE("T19 — encaixar move um eixo só, e o milímetro é o do catálogo",
	  "[calepinagem]")
{
	const MountedItem barra = trilho(QStringLiteral("trilho-1"), 100.0);

	SECTION("o eixo da peça vai para o eixo do trilho, e o x não se mexe")
	{
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);
		const QPointF onde = MountingClip::clippedPosition(peca, barra,
								   EIXO_DISJUNTOR);

			//117,5 menos os 45 que separam o topo do corpo do eixo
			//dele: o canto superior esquerdo para em 72,5, e o
			//corpo fica de 72,5 a 157,5, com o eixo em 117,5.
		CHECK(onde.y() == Approx(72.5));
		CHECK(onde.x() == Approx(50.0));

			//E o eixo da peça, que é o canto mais o que o catálogo
			//diz, para exatamente sobre a linha do trilho.
		CHECK(onde.y() + EIXO_DISJUNTOR.y() == Approx(117.5));
	}

	SECTION("sem eixo cadastrado, o canto é que para sobre a linha")
	{
			//A aproximação declarada: MountingPartView devolve
			//(0, 0) para a peça cujo eixo ninguém preencheu, e
			//(0, 0) é o canto. A fileira fica alinhada pelo canto —
			//visivelmente, que é o ponto: quem olhar o desenho vê
			//que o catálogo não foi medido.
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);
		const QList<MountedItem> tudo = QList<MountedItem>()
						<< barra << peca;

		const QPointF onde = MountingClip::clippedPosition(
					peca, tudo, QHash<QString, QPointF>());

		CHECK(onde.y() == Approx(117.5));
		CHECK(onde.x() == Approx(50.0));
	}

	SECTION("com eixo cadastrado, a mesma peça para trinta e cinco acima")
	{
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);
		const QList<MountedItem> tudo = QList<MountedItem>()
						<< barra << peca;

		const QPointF com = MountingClip::clippedPosition(peca, tudo,
								  eixos());
		const QPointF sem = MountingClip::clippedPosition(
					peca, tudo, QHash<QString, QPointF>());

			//Quarenta e cinco milímetros de diferença entre as
			//duas, que é exatamente o eixo cadastrado: a fileira
			//sai no lugar ou sai 45 mm abaixo dele.
		CHECK(sem.y() - com.y() == Approx(45.0));
	}

	SECTION("a peça que não encaixa em trilho nenhum fica onde foi solta")
	{
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 300.0);
		const QList<MountedItem> tudo = QList<MountedItem>()
						<< barra << peca;

		const QPointF onde = MountingClip::clippedPosition(peca, tudo,
								   eixos());

		CHECK(onde.x() == Approx(50.0));
		CHECK(onde.y() == Approx(300.0));
	}

	SECTION("encaixar duas vezes não move a peça na segunda")
	{
		MountedItem peca = disjuntor(QStringLiteral("q1"), 50.0, 60.0);
		peca.position = MountingClip::clippedPosition(peca, barra,
							      EIXO_DISJUNTOR);

		const QPointF outra = MountingClip::clippedPosition(
					peca, barra, EIXO_DISJUNTOR);

			//Idempotente, e isso é o que garante que a peça
			//encaixada continua sendo do trilho em que ela está:
			//o eixo dela já está sobre a linha, a distância é
			//zero, e nenhum outro trilho pode estar mais perto.
		CHECK(outra.y() == Approx(peca.position.y()));
		CHECK(outra.x() == Approx(peca.position.x()));
	}
}

TEST_CASE("T19 — o trilho em pé encaixa no outro eixo, e só nele",
	  "[calepinagem]")
{
	const MountedItem barra = trilhoEmPe(QStringLiteral("trilho-1"), 200.0);

		//O trilho vai de x 200 a x 235, e o eixo dele é 217,5.
	REQUIRE(MountingClip::axisOf(barra) == Approx(217.5));

	SECTION("a peça sobre ele encaixa pelo x, e o y fica como estava")
	{
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   190.0, 400.0);

		REQUIRE(MountingClip::holds(barra, peca));

		const QPointF onde = MountingClip::clippedPosition(peca, barra,
								   EIXO_DISJUNTOR);

			//217,5 menos os 11,25 do eixo dentro do corpo.
		CHECK(onde.x() == Approx(206.25));
		CHECK(onde.y() == Approx(400.0));
	}

	SECTION("a peça a trezentos milímetros dele não encaixa")
	{
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   490.0, 400.0);

		CHECK_FALSE(MountingClip::holds(barra, peca));
	}
}

TEST_CASE("T19 — o trilho leva os seus, e não leva os do vizinho",
	  "[calepinagem]")
{
	const MountedItem primeiro = trilho(QStringLiteral("trilho-1"), 100.0);
	const MountedItem segundo = trilho(QStringLiteral("trilho-2"), 300.0);

	SECTION("cada peça é de um trilho só, e é do que está mais perto")
	{
		MountedItem de_cima = disjuntor(QStringLiteral("q1"), 50.0, 0.0);
		MountedItem de_baixo = disjuntor(QStringLiteral("q2"), 50.0, 0.0);

		de_cima.position = MountingClip::clippedPosition(de_cima,
								 primeiro,
								 EIXO_DISJUNTOR);
		de_baixo.position = MountingClip::clippedPosition(de_baixo,
								  segundo,
								  EIXO_DISJUNTOR);

		const QList<MountedItem> tudo = QList<MountedItem>()
						<< primeiro << segundo
						<< de_cima << de_baixo;

		CHECK(MountingClip::carrierOf(de_cima, tudo, eixos())
		      == QStringLiteral("trilho-1"));
		CHECK(MountingClip::carrierOf(de_baixo, tudo, eixos())
		      == QStringLiteral("trilho-2"));

		const QStringList do_primeiro =
				MountingClip::carried(QStringLiteral("trilho-1"),
						      tudo, eixos());
		const QStringList do_segundo =
				MountingClip::carried(QStringLiteral("trilho-2"),
						      tudo, eixos());

		CHECK(do_primeiro.count() == 1);
		CHECK(do_primeiro.contains(QStringLiteral("q1")));
		CHECK_FALSE(do_primeiro.contains(QStringLiteral("q2")));

		CHECK(do_segundo.count() == 1);
		CHECK(do_segundo.contains(QStringLiteral("q2")));
		CHECK_FALSE(do_segundo.contains(QStringLiteral("q1")));
	}

	SECTION("doze disjuntores num trilho são doze, e o vizinho fica vazio")
	{
		QList<MountedItem> tudo;
		tudo << primeiro << segundo;

		for (int numero = 0 ; numero < 12 ; ++ numero)
		{
			MountedItem peca = disjuntor(
					QStringLiteral("q%1").arg(numero + 1),
					numero * LARGURA_DISJUNTOR, 0.0);
			peca.position = MountingClip::clippedPosition(
					peca, primeiro, EIXO_DISJUNTOR);
			tudo << peca;
		}

		CHECK(MountingClip::carried(QStringLiteral("trilho-1"), tudo,
					    eixos()).count() == 12);
		CHECK(MountingClip::carried(QStringLiteral("trilho-2"), tudo,
					    eixos()).isEmpty());
	}

	SECTION("uma canaleta que cruza o trilho não é levada por ele")
	{
			//Peça cortada não se clipsa em peça cortada: trilho e
			//canaleta são parafusados na chapa, e uma canaleta em
			//pé cruzando um trilho deitado cobre o eixo dele sem
			//pertencer a ele.
		MountedItem cruzando = canaleta(QStringLiteral("canaleta-1"),
						90.0);
		cruzando.run = MountingRun::Down;
		cruzando.position = QPointF(300.0, 0.0);
		cruzando.size = QSizeF(40.0, 800.0);

		const QList<MountedItem> tudo = QList<MountedItem>()
						<< primeiro << cruzando;

		CHECK_FALSE(MountingClip::holds(primeiro, cruzando));
		CHECK(MountingClip::carried(QStringLiteral("trilho-1"), tudo,
					    eixos()).isEmpty());
	}

	SECTION("uma canaleta não segura nada, mesmo com peça em cima dela")
	{
		const MountedItem duto = canaleta(QStringLiteral("canaleta-1"),
						  100.0);
		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);

		const QList<MountedItem> tudo = QList<MountedItem>()
						<< duto << peca;

		CHECK_FALSE(MountingClip::isCarrier(duto));
		CHECK_FALSE(MountingClip::holds(duto, peca));
		CHECK(MountingClip::carrierOf(peca, tudo, eixos()).isEmpty());
	}

	SECTION("um trilho sem identidade não leva ninguém")
	{
			//O mesmo guarda que o desenho faz ao fim de um
			//arraste: um passo que não sabe nomear o que move não
			//sabe desfazê-lo, e um trilho anônimo não pode sair
			//levando doze disjuntores.
		MountedItem anonimo = trilho(QString(), 100.0);
		anonimo.uuid.clear();

		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);
		const QList<MountedItem> tudo = QList<MountedItem>()
						<< anonimo << peca;

		CHECK(MountingClip::holds(anonimo, peca));
		CHECK(MountingClip::carrierOf(peca, tudo, eixos()).isEmpty());
	}
}

TEST_CASE("T19 — o que o trilho leva sai na ordem em que está nele",
	  "[calepinagem]")
{
	const MountedItem barra = trilho(QStringLiteral("trilho-1"), 100.0);

	SECTION("a ordem é a do trilho, e não a da lista")
	{
		QList<MountedItem> tudo;
		tudo << barra;

			//Entregues fora de ordem de propósito: 300, 100, 500.
		const QList<qreal> lugares = QList<qreal>() << 300.0 << 100.0
							    << 500.0;
		const QStringList nomes = QStringList()
					  << QStringLiteral("meio")
					  << QStringLiteral("inicio")
					  << QStringLiteral("fim");

		for (int indice = 0 ; indice < lugares.count() ; ++ indice)
		{
			MountedItem peca = disjuntor(nomes.at(indice),
						     lugares.at(indice), 0.0);
			peca.position = MountingClip::clippedPosition(
					peca, barra, EIXO_DISJUNTOR);
			tudo << peca;
		}

		QStringList esperada;
		esperada << QStringLiteral("inicio")
			 << QStringLiteral("meio")
			 << QStringLiteral("fim");

		CHECK(MountingClip::carried(QStringLiteral("trilho-1"), tudo,
					    eixos())
		      == esperada);
	}
}

TEST_CASE("T19 — a ocupação do trilho é a soma que já existia",
	  "[calepinagem]")
{
	const MountedItem barra = trilho(QStringLiteral("trilho-1"), 100.0);
	const MountedItem vizinho = trilho(QStringLiteral("trilho-2"), 300.0);

	QList<MountedItem> tudo;
	tudo << barra << vizinho;

	for (int numero = 0 ; numero < 12 ; ++ numero)
	{
		MountedItem peca = disjuntor(
				QStringLiteral("q%1").arg(numero + 1),
				numero * LARGURA_DISJUNTOR, 0.0);
		peca.position = MountingClip::clippedPosition(peca, barra,
							      EIXO_DISJUNTOR);
		tudo << peca;
	}

	SECTION("doze disjuntores tomam duzentos e setenta milímetros")
	{
		const MountingRailFill ocupacao =
				MountingClip::fillOf(QStringLiteral("trilho-1"),
						     tudo, eixos());

		CHECK(ocupacao.item_count == 12);
		CHECK(ocupacao.used == Approx(270.0));
		CHECK(ocupacao.length == Approx(600.0));
		CHECK(ocupacao.free() == Approx(330.0));
		CHECK(ocupacao.isConclusive());
		CHECK_FALSE(ocupacao.isOverfilled());
	}

	SECTION("a soma do vizinho não é contaminada pela do primeiro")
	{
			//A asserção que conta linhas pega o item que virou
			//linha de outro trilho; a que soma milímetro pega o
			//que somou dentro da linha certa. As duas, e não uma
			//escolhida.
		const MountingRailFill ocupacao =
				MountingClip::fillOf(QStringLiteral("trilho-2"),
						     tudo, eixos());

		CHECK(ocupacao.item_count == 0);
		CHECK(ocupacao.used == Approx(0.0));
		CHECK(ocupacao.free() == Approx(600.0));
	}

	SECTION("a peça que ninguém mediu é contada e não é somada")
	{
		MountedItem peca = semMedida(QStringLiteral("ka1"), 400.0, 0.0);
		peca.position = MountingClip::clippedPosition(peca, barra,
							      QPointF(0.0, 0.0));
		tudo << peca;

		const MountingRailFill ocupacao =
				MountingClip::fillOf(QStringLiteral("trilho-1"),
						     tudo, eixos());

		CHECK(ocupacao.item_count == 13);
		CHECK(ocupacao.used == Approx(270.0));
		CHECK(ocupacao.unknown_width_count == 1);
		CHECK_FALSE(ocupacao.isConclusive());
	}

	SECTION("no trilho em pé a soma é a altura, e não a largura")
	{
			//O mesmo disjuntor de 22,5 por 85: deitado ele toma
			//22,5 do trilho, em pé ele toma 85. Um trilho vertical
			//que somasse larguras diria que cabem vinte e seis
			//disjuntores onde cabem sete.
		const MountedItem em_pe = trilhoEmPe(QStringLiteral("trilho-3"),
						     200.0);

		MountedItem peca = disjuntor(QStringLiteral("q-vertical"),
					     0.0, 300.0);
		peca.position = MountingClip::clippedPosition(peca, em_pe,
							      EIXO_DISJUNTOR);

		const QList<MountedItem> vertical = QList<MountedItem>()
						    << em_pe << peca;

		const MountingRailFill ocupacao =
				MountingClip::fillOf(QStringLiteral("trilho-3"),
						     vertical, eixos());

		CHECK(ocupacao.item_count == 1);
		CHECK(ocupacao.used == Approx(ALTURA_DISJUNTOR));
	}
}

TEST_CASE("T19 — o que não é número não encaixa em nada", "[calepinagem]")
{
	const MountedItem barra = trilho(QStringLiteral("trilho-1"), 100.0);

	SECTION("a peça sem posição não é segurada, e não se move")
	{
		MountedItem peca = disjuntor(QStringLiteral("q1"), 50.0, 60.0);
		peca.position = QPointF(not_a_number, not_a_number);

		CHECK_FALSE(MountingClip::holds(barra, peca));

		const QPointF onde = MountingClip::clippedPosition(peca, barra,
								   EIXO_DISJUNTOR);

		CHECK(qIsNaN(onde.x()));
		CHECK(qIsNaN(onde.y()));
	}

	SECTION("o trilho sem posição não segura nada")
	{
		MountedItem barra_perdida = barra;
		barra_perdida.position = QPointF(not_a_number, 100.0);

		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);

		CHECK_FALSE(MountingClip::holds(barra_perdida, peca));
		CHECK(qIsNaN(MountingClip::axisOf(barra_perdida)));
	}

	SECTION("um eixo que não é par de números é lido como eixo nenhum")
	{
		QHash<QString, QPointF> quebrado;
		quebrado.insert(CODIGO_DISJUNTOR,
				QPointF(not_a_number, not_a_number));

		const MountedItem peca = disjuntor(QStringLiteral("q1"),
						   50.0, 60.0);
		const QList<MountedItem> tudo = QList<MountedItem>()
						<< barra << peca;

		const QPointF onde = MountingClip::clippedPosition(peca, tudo,
								   quebrado);

			//Cai no canto, que é a resposta declarada para "o
			//catálogo não diz" — e nunca num lugar que não é lugar.
		CHECK(onde.y() == Approx(117.5));
		CHECK(onde.x() == Approx(50.0));
	}
}

TEST_CASE("T19 — a peça sem medida é levada como o ponto que ela é",
	  "[calepinagem]")
{
	const MountedItem barra = trilho(QStringLiteral("trilho-1"), 100.0);

	SECTION("sobre a linha do trilho, ela é segurada")
	{
		const MountedItem peca = semMedida(QStringLiteral("ka1"),
						   400.0, 117.5);

		CHECK(MountingClip::holds(barra, peca));

		const QList<MountedItem> tudo = QList<MountedItem>()
						<< barra << peca;
		const QStringList levados =
				MountingClip::carried(QStringLiteral("trilho-1"),
						      tudo);

		CHECK(levados.count() == 1);
		CHECK(levados.contains(QStringLiteral("ka1")));
	}

	SECTION("longe da linha, ela não é — nem por ser um ponto")
	{
		const MountedItem peca = semMedida(QStringLiteral("ka1"),
						   400.0, 400.0);

		CHECK_FALSE(MountingClip::holds(barra, peca));
	}
}
