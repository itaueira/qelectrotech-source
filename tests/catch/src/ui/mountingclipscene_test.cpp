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
#include "../qt_catch_tostring.h"

#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/mountingclip.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/location/mountingprofile.h"
#include "../../../../sources/undocommand/movemountedrailcommand.h"

#include <catch2/catch.hpp>

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	O gesto do encaixe na cena da placa: soltar um componente sobre um
	trilho, arrastar o trilho, e o que cada um dos dois deixa na pilha de
	desfazer.

	Está nesta suíte, e não junto das regras puras, porque uma QGraphicsScene
	precisa de uma QApplication. A geometria — o que cobre o trilho, qual
	trilho leva qual peça, onde a peça para em milímetro — fica um andar
	abaixo, em src/mountingclip_test.cpp, e não se repete aqui. O que se
	prova aqui é o que uma lista de retângulos não tem: que arrastar o
	trilho leva os disjuntores **num passo só**, que desfazer devolve todos,
	e que o trilho do lado não leva os do vizinho.

	Os números são milímetro e são afirmados como milímetro. O trilho deste
	arquivo tem o topo em y 100 e seção de 35, logo o eixo dele é 117,5; um
	disjuntor cujo eixo está 45 abaixo do próprio topo encaixa com o canto
	em 72,5. É essa conta que vira furo na chapa, e é por isso que ela está
	escrita e não descrita.
*/

namespace
{
	/// milímetro: a seção do trilho DIN
	const qreal SECAO = 35.0;

	/// milímetro: onde o eixo do disjuntor está dentro do corpo dele
	const QPointF EIXO(11.25, 45.0);

	const QString CODIGO = QStringLiteral("A9F74210");

	/// @return um trilho deitado de 600, com o topo em @a y
	MountedItem trilho(const QString &uuid, qreal y)
	{
		const MountingProfile perfil = MountingProfile::rail(SECAO, 7.5);

		MountedItem item(QString(), QPointF(0.0, y),
				 perfil.sizeFor(600.0, MountingRun::Across));
		item.uuid = uuid;
		item.profile = perfil;
		item.run = MountingRun::Across;

		return item;
	}

	/// @return um disjuntor unipolar com o canto superior esquerdo em (x, y)
	MountedItem disjuntor(const QString &uuid, qreal x, qreal y)
	{
			//O rótulo é o identificador em maiúscula: "q1" é o
			//-Q1 que a folha mostra.
		MountedItem item(QStringLiteral("-") + uuid.toUpper(),
				 QPointF(x, y), QSizeF(22.5, 85.0));
		item.uuid = uuid;
		item.part_code = CODIGO;

		return item;
	}

	/// @return o que o catálogo diz sobre o eixo do disjuntor
	QHash<QString, QPointF> eixos()
	{
		QHash<QString, QPointF> axes;
		axes.insert(CODIGO, EIXO);

		return axes;
	}

	/**
		@return uma placa de 600 por 800 com dois trilhos e quatro
		disjuntores: três encaixados no de cima, um no de baixo.

		As posições já são as do encaixe — 72,5 no trilho de y 100 e
		272,5 no de y 300 —, escritas como número e não calculadas, para
		que o arquivo diga em que milímetro a placa começa.
	*/
	MountingSurface placa()
	{
		MountingSurface surface(QStringLiteral("QCM1"),
					QStringLiteral("plate"),
					MountingArea(600.0, 800.0));
		surface.uuid = QStringLiteral("face");
		surface.name = QStringLiteral("Platine");

		surface.items << trilho(QStringLiteral("trilho-1"), 100.0)
			      << trilho(QStringLiteral("trilho-2"), 300.0)
			      << disjuntor(QStringLiteral("q1"), 0.0, 72.5)
			      << disjuntor(QStringLiteral("q2"), 22.5, 72.5)
			      << disjuntor(QStringLiteral("q3"), 45.0, 72.5)
			      << disjuntor(QStringLiteral("q4"), 0.0, 272.5);

		return surface;
	}

	/// @return onde a peça de @a uuid está na cena, em milímetro
	QPointF onde(const MountingScene &scene, const QString &uuid)
	{
		MountedPartItem *part = scene.partItem(uuid);

		return part ? part->millimetrePosition() : QPointF();
	}
}

TEST_CASE("T19 — arrastar o trilho leva o que está clipsado nele",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setPartAxes(eixos());
	scene.setSurface(placa());

	REQUIRE(scene.partCount() == 6);
	REQUIRE(scene.undoStack().index() == 0);

	SECTION("o trilho sabe o que leva, e o vizinho sabe que não é dele")
	{
		const QStringList do_primeiro =
				scene.carriedBy(QStringLiteral("trilho-1"));
		const QStringList do_segundo =
				scene.carriedBy(QStringLiteral("trilho-2"));

		CHECK(do_primeiro.count() == 3);
		CHECK(do_segundo.count() == 1);
		CHECK(do_segundo.contains(QStringLiteral("q4")));
		CHECK_FALSE(do_segundo.contains(QStringLiteral("q1")));

		CHECK(scene.carrierOf(QStringLiteral("q1"))
		      == QStringLiteral("trilho-1"));
		CHECK(scene.carrierOf(QStringLiteral("q4"))
		      == QStringLiteral("trilho-2"));

			//Um trilho não é levado por nada, nem por ele mesmo.
		CHECK(scene.carrierOf(QStringLiteral("trilho-1")).isEmpty());
	}

	SECTION("cinquenta milímetros para baixo são cinquenta para todos")
	{
		REQUIRE(scene.moveItem(QStringLiteral("trilho-1"),
				       QPointF(0.0, 150.0)));

		CHECK(onde(scene, QStringLiteral("trilho-1")).y()
		      == Approx(150.0));
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(122.5));
		CHECK(onde(scene, QStringLiteral("q2")).y() == Approx(122.5));
		CHECK(onde(scene, QStringLiteral("q3")).y() == Approx(122.5));

			//E o espaçamento entre eles é o mesmo de antes: 22,5
			//de largura, encostados um no outro.
		CHECK(onde(scene, QStringLiteral("q1")).x() == Approx(0.0));
		CHECK(onde(scene, QStringLiteral("q2")).x() == Approx(22.5));
		CHECK(onde(scene, QStringLiteral("q3")).x() == Approx(45.0));
	}

	SECTION("é um passo só, e desfazer devolve todos de uma vez")
	{
		REQUIRE(scene.moveItem(QStringLiteral("trilho-1"),
				       QPointF(0.0, 150.0)));

			//Um passo, e não quatro: a pessoa fez uma coisa.
		CHECK(scene.undoStack().index() == 1);
		CHECK(scene.undoStack().count() == 1);

		scene.undoStack().undo();

		CHECK(onde(scene, QStringLiteral("trilho-1")).y()
		      == Approx(100.0));
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(72.5));
		CHECK(onde(scene, QStringLiteral("q2")).y() == Approx(72.5));
		CHECK(onde(scene, QStringLiteral("q3")).y() == Approx(72.5));

		scene.undoStack().redo();

		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(122.5));
		CHECK(onde(scene, QStringLiteral("q3")).y() == Approx(122.5));
	}

	SECTION("o trilho do lado não leva os do vizinho")
	{
		REQUIRE(scene.moveItem(QStringLiteral("trilho-2"),
				       QPointF(0.0, 350.0)));

		CHECK(onde(scene, QStringLiteral("q4")).y() == Approx(322.5));

			//E os três do primeiro trilho ficam onde estavam, no
			//milímetro: sem este par, um encaixe que grudasse tudo
			//em tudo passaria no caso de cima.
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(72.5));
		CHECK(onde(scene, QStringLiteral("q2")).y() == Approx(72.5));
		CHECK(onde(scene, QStringLiteral("q3")).y() == Approx(72.5));
	}

	SECTION("o que o trilho carrega é lido de onde ele estava, não de onde foi")
	{
			//A pergunta feita depois do arraste responde nada: um
			//trilho que já desceu 300 mm não cobre mais nenhum dos
			//disjuntores que ele deixou para trás.
		CHECK(scene.carriedBy(QStringLiteral("trilho-1"),
				      QPointF(0.0, 400.0)).isEmpty());
		CHECK(scene.carriedBy(QStringLiteral("trilho-1"),
				      QPointF(0.0, 100.0)).count() == 3);
	}

	SECTION("um trilho que não se move não deixa passo, com doze ou com zero")
	{
		CHECK_FALSE(scene.moveItem(QStringLiteral("trilho-1"),
					   QPointF(0.0, 100.0)));
		CHECK(scene.undoStack().index() == 0);
	}
}

TEST_CASE("T19 — soltar sobre o trilho encaixa; um número digitado é respeitado",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setPartAxes(eixos());
	scene.setSurface(placa());

	SECTION("um número digitado põe a peça no número, mesmo sobre o trilho")
	{
			//(50, 60) fica em cima do trilho de y 100: o corpo vai
			//de 60 a 145 e cobre o eixo. Digitar não encaixa —
			//quem digita um milímetro quer aquele milímetro.
		REQUIRE(scene.moveItem(QStringLiteral("q1"),
				       QPointF(50.0, 60.0)));

		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(60.0));
		CHECK(onde(scene, QStringLiteral("q1")).x() == Approx(50.0));
	}

	SECTION("clipar a mesma peça a leva para o eixo do trilho")
	{
		REQUIRE(scene.moveItem(QStringLiteral("q1"),
				       QPointF(50.0, 60.0)));
		REQUIRE(scene.clipItem(QStringLiteral("q1")));

		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(72.5));
		CHECK(onde(scene, QStringLiteral("q1")).x() == Approx(50.0));

			//E é passo desfazível: dois passos, o que moveu e o
			//que encaixou.
		CHECK(scene.undoStack().index() == 2);

		scene.undoStack().undo();
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(60.0));
	}

	SECTION("a peça longe do trilho recusa o encaixe, e diz por quê")
	{
		REQUIRE(scene.moveItem(QStringLiteral("q1"),
				       QPointF(50.0, 550.0)));

		QString motivo;
		CHECK_FALSE(scene.clipItem(QStringLiteral("q1"), &motivo));
		CHECK_FALSE(motivo.isEmpty());

			//E ela fica exatamente onde foi posta.
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(550.0));
	}

	SECTION("sem eixo cadastrado, é o canto que para sobre a linha")
	{
		scene.setPartAxes(QHash<QString, QPointF>());

		REQUIRE(scene.moveItem(QStringLiteral("q1"),
				       QPointF(50.0, 60.0)));
		REQUIRE(scene.clipItem(QStringLiteral("q1")));

			//117,5 e não 72,5: a aproximação declarada para a peça
			//cujo eixo o catálogo não guarda. A fileira sai 45 mm
			//abaixo, e isso se vê no desenho — que é o ponto.
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(117.5));
	}

	SECTION("onde a peça pararia se fosse clipsada é pergunta, não gesto")
	{
		REQUIRE(scene.moveItem(QStringLiteral("q1"),
				       QPointF(50.0, 60.0)));

		CHECK(scene.clipTarget(QStringLiteral("q1")).y()
		      == Approx(72.5));

			//Perguntar não moveu nada.
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(60.0));

			//E perguntar por uma peça que não existe devolve uma
			//posição que não é uma, para que ninguém a confunda
			//com o canto da chapa.
		CHECK(qIsNaN(scene.clipTarget(QStringLiteral("nada")).x()));
	}
}

TEST_CASE("T19 — soltar o componente é que encaixa, e é um passo só",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setPartAxes(eixos());
	scene.setSurface(placa());

	MountedPartItem *peca = scene.partItem(QStringLiteral("q1"));
	REQUIRE(peca != nullptr);

	SECTION("largar sobre o trilho põe a peça no eixo, e desfazer devolve")
	{
			//O arraste é do framework: ele já pôs a peça onde o
			//mouse a largou. O que a cena recebe é o sinal com o
			//lugar de onde ela veio, e é esse sinal que se emite
			//aqui — o mesmo que mouseReleaseEvent emite.
		peca->setMillimetrePosition(QPointF(50.0, 60.0));
		emit peca->dragged(QPointF(400.0, 500.0));

		CHECK(peca->millimetrePosition().y() == Approx(72.5));
		CHECK(peca->millimetrePosition().x() == Approx(50.0));

			//Um passo, e não dois: arrastar e encaixar foi um
			//gesto, e um desfazer devolve a peça ao lugar de onde
			//ela saiu — não ao lugar onde o mouse a largou.
		CHECK(scene.undoStack().index() == 1);

		scene.undoStack().undo();

		CHECK(peca->millimetrePosition().x() == Approx(400.0));
		CHECK(peca->millimetrePosition().y() == Approx(500.0));
	}

	SECTION("largar longe de tudo deixa a peça onde a mão a largou")
	{
		peca->setMillimetrePosition(QPointF(400.0, 600.0));
		emit peca->dragged(QPointF(0.0, 72.5));

		CHECK(peca->millimetrePosition().x() == Approx(400.0));
		CHECK(peca->millimetrePosition().y() == Approx(600.0));
		CHECK(scene.undoStack().index() == 1);
	}

	SECTION("o arraste que não tem o que desfazer ainda assim encaixa")
	{
			//A peça já estava encaixada em 72,5, foi arrastada até
			//71 e largada. O passo seria nulo — ela volta ao mesmo
			//milímetro de onde saiu —, e mesmo assim ela tem de
			//acabar sobre o trilho, e não a um milímetro dele.
		peca->setMillimetrePosition(QPointF(0.0, 71.0));
		emit peca->dragged(QPointF(0.0, 72.5));

		CHECK(peca->millimetrePosition().y() == Approx(72.5));
		CHECK(scene.undoStack().index() == 0);
	}

	SECTION("arrastar o trilho pelo mouse leva os três, num passo")
	{
		MountedPartItem *barra = scene.partItem(QStringLiteral("trilho-1"));
		REQUIRE(barra != nullptr);

		barra->setMillimetrePosition(QPointF(0.0, 150.0));
		emit barra->dragged(QPointF(0.0, 100.0));

		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(122.5));
		CHECK(onde(scene, QStringLiteral("q3")).y() == Approx(122.5));
		CHECK(onde(scene, QStringLiteral("q4")).y() == Approx(272.5));
		CHECK(scene.undoStack().index() == 1);

		scene.undoStack().undo();

		CHECK(onde(scene, QStringLiteral("trilho-1")).y()
		      == Approx(100.0));
		CHECK(onde(scene, QStringLiteral("q1")).y() == Approx(72.5));
	}
}

TEST_CASE("T19 — o passo do trilho diz o que leva, e o que não leva nada",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setPartAxes(eixos());
	scene.setSurface(placa());

	SECTION("ele guarda de onde cada peça saiu, uma por uma")
	{
		const QStringList levados =
				scene.carriedBy(QStringLiteral("trilho-1"));

		MoveMountedRailCommand comando(&scene,
					       QStringLiteral("trilho-1"),
					       QPointF(0.0, 100.0),
					       QPointF(0.0, 150.0),
					       levados);

		CHECK_FALSE(comando.isNull());
		CHECK(comando.carriedCount() == 3);
		CHECK(comando.delta().y() == Approx(50.0));
		CHECK(comando.carriedBefore(QStringLiteral("q2")).x()
		      == Approx(22.5));
		CHECK(comando.carriedBefore(QStringLiteral("q2")).y()
		      == Approx(72.5));

			//Perguntar por quem ele não leva devolve uma posição
			//que não é uma, e nunca o canto da chapa.
		CHECK(qIsNaN(comando.carriedBefore(QStringLiteral("q4")).x()));
	}

	SECTION("o mesmo milímetro nos dois lados não é passo, com carga ou sem")
	{
		const QStringList levados =
				scene.carriedBy(QStringLiteral("trilho-1"));

		MoveMountedRailCommand parado(&scene, QStringLiteral("trilho-1"),
					      QPointF(0.0, 100.0),
					      QPointF(0.0, 100.0), levados);

		CHECK(parado.carriedCount() == 3);
		CHECK(parado.isNull());
	}

	SECTION("sem cena, ou sem trilho, não é passo")
	{
		MoveMountedRailCommand orfao(nullptr,
					     QStringLiteral("trilho-1"),
					     QPointF(0.0, 100.0),
					     QPointF(0.0, 150.0),
					     QStringList());
		MoveMountedRailCommand anonimo(&scene, QString(),
					       QPointF(0.0, 100.0),
					       QPointF(0.0, 150.0),
					       QStringList());

		CHECK(orfao.isNull());
		CHECK(anonimo.isNull());
	}

	SECTION("um trilho não leva ele mesmo, nem a mesma peça duas vezes")
	{
		QStringList repetidos;
		repetidos << QStringLiteral("q1") << QStringLiteral("q1")
			  << QStringLiteral("trilho-1");

		MoveMountedRailCommand comando(&scene,
					       QStringLiteral("trilho-1"),
					       QPointF(0.0, 100.0),
					       QPointF(0.0, 150.0),
					       repetidos);

		CHECK(comando.carriedCount() == 1);
		CHECK(comando.carried().contains(QStringLiteral("q1")));
	}
}
