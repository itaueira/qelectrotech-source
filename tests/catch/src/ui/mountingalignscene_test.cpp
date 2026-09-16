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
#include "../../../../sources/location/mountingalign.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/location/mountingprofile.h"
#include "../../../../sources/undocommand/alignmountedpartscommand.h"

#include <catch2/catch.hpp>

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	Alinhar e distribuir como gesto: o que a seleção da cena entrega, o que
	a cena recusa e com que motivo, e quantos passos de desfazer um gesto
	deixa.

	A aritmética — qual milímetro cada peça toma — fica um andar abaixo, em
	src/mountingalign_test.cpp, e não se repete aqui. O que se prova aqui é
	o que uma lista de retângulos não tem: que seis peças alinhadas são UM
	passo e não seis, que desfazer devolve as seis de uma vez, e que a
	recusa chega com uma frase e não em silêncio — um botão que não faz nada
	e não diz nada é lido como botão quebrado.
*/

namespace
{
	/// @return um componente de @a largura por 85 no canto (x, y)
	MountedItem peca(const QString &uuid, qreal x, qreal y,
			 qreal largura = 22.5)
	{
		MountedItem item(QStringLiteral("-") + uuid.toUpper(),
				 QPointF(x, y), QSizeF(largura, 85.0));
		item.uuid = uuid;
		item.part_code = QStringLiteral("A9F74210");

		return item;
	}

	/// @return um trilho deitado de 600, com o topo em @a y
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

	/**
		@return uma placa com um trilho e três componentes desalinhados,
		cada um num x diferente.
	*/
	MountingSurface placa()
	{
		MountingSurface surface(QStringLiteral("QCM1"),
					QStringLiteral("plate"),
					MountingArea(600.0, 800.0));
		surface.uuid = QStringLiteral("face");
		surface.name = QStringLiteral("Platine");

		surface.items << trilho(QStringLiteral("trilho-1"), 100.0)
			      << peca(QStringLiteral("a"), 10.0, 300.0)
			      << peca(QStringLiteral("b"), 50.0, 400.0)
			      << peca(QStringLiteral("c"), 30.0, 500.0);

		return surface;
	}

	/// @return onde a peça de @a uuid está na cena, em milímetro
	QPointF onde(const MountingScene &scene, const QString &uuid)
	{
		MountedPartItem *part = scene.partItem(uuid);

		return part ? part->millimetrePosition() : QPointF();
	}

	/// @brief Selecionar na cena as peças de @a uuids, e nada mais
	void selecionar(MountingScene &scene, const QStringList &uuids)
	{
		scene.clearSelection();

		for (const QString &uuid : uuids)
		{
			MountedPartItem *part = scene.partItem(uuid);
			if (part) {
				part->setSelected(true);
			}
		}
	}
}

TEST_CASE("T19 — a seleção da cena é o que o gesto recebe",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setSurface(placa());

	SECTION("nada selecionado é lista vazia, e não uma identidade vazia")
	{
		CHECK(scene.selectedUuids().isEmpty());
	}

	SECTION("o trilho entra na seleção, e é a regra que decide o que fazer")
	{
			//Não se filtra aqui: uma seleção que escondesse o
			//trilho faria a regra decidir sobre uma seleção que
			//não é a que a pessoa fez, e a frase "os rails não
			//contam" não teria como ser dita.
		selecionar(scene, QStringList()
				  << QStringLiteral("trilho-1")
				  << QStringLiteral("a"));

		const QStringList selecionados = scene.selectedUuids();

		CHECK(selecionados.count() == 2);
		CHECK(selecionados.contains(QStringLiteral("trilho-1")));
		CHECK(selecionados.contains(QStringLiteral("a")));
	}
}

TEST_CASE("T19 — alinhar a seleção é um passo, e desfazer devolve todos",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setSurface(placa());

	const QStringList tres = QStringList() << QStringLiteral("a")
					       << QStringLiteral("b")
					       << QStringLiteral("c");

	SECTION("três peças vão para a borda do extremo, em um passo só")
	{
		QString motivo;
		REQUIRE(scene.alignItems(tres, MountingAlignment::LeftEdges,
					 &motivo));
		CHECK(motivo.isEmpty());

		CHECK(onde(scene, QStringLiteral("a")).x() == Approx(10.0));
		CHECK(onde(scene, QStringLiteral("b")).x() == Approx(10.0));
		CHECK(onde(scene, QStringLiteral("c")).x() == Approx(10.0));

			//E o y de cada uma continua o dela.
		CHECK(onde(scene, QStringLiteral("b")).y() == Approx(400.0));

		CHECK(scene.undoStack().index() == 1);
		CHECK(scene.undoStack().count() == 1);

		scene.undoStack().undo();

		CHECK(onde(scene, QStringLiteral("b")).x() == Approx(50.0));
		CHECK(onde(scene, QStringLiteral("c")).x() == Approx(30.0));
		CHECK(scene.undoStack().index() == 0);
	}

	SECTION("alinhar de novo não deixa passo, e diz que já estavam")
	{
		REQUIRE(scene.alignItems(tres, MountingAlignment::LeftEdges));

		QString motivo;
		CHECK_FALSE(scene.alignItems(tres, MountingAlignment::LeftEdges,
					     &motivo));

			//Sem motivo: não é recusa, é "não havia o que fazer".
			//Quem chama distingue os dois pelo texto vazio.
		CHECK(motivo.isEmpty());
		CHECK(scene.undoStack().index() == 1);
	}

	SECTION("uma peça sozinha é recusada, com motivo")
	{
		QString motivo;
		CHECK_FALSE(scene.alignItems(QStringList()
					     << QStringLiteral("a"),
					     MountingAlignment::LeftEdges,
					     &motivo));
		CHECK_FALSE(motivo.isEmpty());
		CHECK(scene.undoStack().index() == 0);
	}

	SECTION("o trilho na seleção fica parado, e os componentes não")
	{
		QString motivo;
		REQUIRE(scene.alignItems(QStringList()
					 << QStringLiteral("trilho-1")
					 << QStringLiteral("b")
					 << QStringLiteral("c"),
					 MountingAlignment::LeftEdges, &motivo));

			//A barra começa em x 0 e seria o extremo esquerdo da
			//seleção. Ela não entra na conta e não se move: quem
			//move um trilho é o arraste, que leva o que está
			//clipsado nele.
		CHECK(onde(scene, QStringLiteral("trilho-1")).x()
		      == Approx(0.0));
		CHECK(onde(scene, QStringLiteral("b")).x() == Approx(30.0));
		CHECK(onde(scene, QStringLiteral("c")).x() == Approx(30.0));
	}

	SECTION("um trilho e um componente não são dois componentes")
	{
		QString motivo;
		CHECK_FALSE(scene.alignItems(QStringList()
					     << QStringLiteral("trilho-1")
					     << QStringLiteral("a"),
					     MountingAlignment::LeftEdges,
					     &motivo));
		CHECK_FALSE(motivo.isEmpty());
	}
}

TEST_CASE("T19 — distribuir deixa o mesmo vão, e recusa quando não cabe",
	  "[uibench][calepinagem]")
{
	MountingScene scene;

	MountingSurface surface(QStringLiteral("QCM1"),
				QStringLiteral("plate"),
				MountingArea(600.0, 800.0));
	surface.uuid = QStringLiteral("face");
	surface.items << peca(QStringLiteral("a"), 0.0, 100.0)
		      << peca(QStringLiteral("b"), 40.0, 100.0)
		      << peca(QStringLiteral("c"), 120.0, 100.0)
		      << peca(QStringLiteral("d"), 277.5, 100.0)
		      << peca(QStringLiteral("g1"), 0.0, 500.0, 100.0)
		      << peca(QStringLiteral("g2"), 20.0, 500.0, 100.0)
		      << peca(QStringLiteral("g3"), 50.0, 500.0, 100.0);

	scene.setSurface(surface);

	SECTION("quatro disjuntores ficam com setenta de vão, num passo")
	{
		const QStringList quatro = QStringList()
					   << QStringLiteral("a")
					   << QStringLiteral("b")
					   << QStringLiteral("c")
					   << QStringLiteral("d");

		QString motivo;
		REQUIRE(scene.distributeItems(quatro, MountingRun::Across,
					      &motivo));
		CHECK(motivo.isEmpty());

		CHECK(onde(scene, QStringLiteral("a")).x() == Approx(0.0));
		CHECK(onde(scene, QStringLiteral("b")).x() == Approx(92.5));
		CHECK(onde(scene, QStringLiteral("c")).x() == Approx(185.0));
		CHECK(onde(scene, QStringLiteral("d")).x() == Approx(277.5));

		CHECK(scene.undoStack().index() == 1);

		scene.undoStack().undo();

		CHECK(onde(scene, QStringLiteral("b")).x() == Approx(40.0));
		CHECK(onde(scene, QStringLiteral("c")).x() == Approx(120.0));
	}

	SECTION("duas peças são recusadas, com motivo")
	{
		QString motivo;
		CHECK_FALSE(scene.distributeItems(QStringList()
						  << QStringLiteral("a")
						  << QStringLiteral("b"),
						  MountingRun::Across,
						  &motivo));
		CHECK_FALSE(motivo.isEmpty());
	}

	SECTION("o que não cabe é recusado, e a frase diz quanto falta")
	{
			//Três peças de 100 numa fileira que ocupa 150: faltam
			//150 mm. Distribuí-las daria uma fileira sobreposta
			//por igual, que é a única resposta errada com cara de
			//proposital.
		QString motivo;
		CHECK_FALSE(scene.distributeItems(QStringList()
						  << QStringLiteral("g1")
						  << QStringLiteral("g2")
						  << QStringLiteral("g3"),
						  MountingRun::Across,
						  &motivo));

		CHECK_FALSE(motivo.isEmpty());
		CHECK(motivo.contains(QStringLiteral("150")));
		CHECK(scene.undoStack().index() == 0);

			//E elas ficam exatamente onde estavam.
		CHECK(onde(scene, QStringLiteral("g2")).x() == Approx(20.0));
	}
}

TEST_CASE("T19 — o passo do alinhamento guarda de onde cada peça saiu",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setSurface(placa());

	SECTION("ele nomeia as peças, e diz de onde e para onde")
	{
		QHash<QString, QPointF> destinos;
		destinos.insert(QStringLiteral("b"), QPointF(10.0, 400.0));
		destinos.insert(QStringLiteral("c"), QPointF(10.0, 500.0));

		AlignMountedPartsCommand comando(&scene, destinos,
						 QStringLiteral("Aligner"));

		CHECK_FALSE(comando.isNull());
		CHECK(comando.count() == 2);
		CHECK(comando.before(QStringLiteral("b")).x() == Approx(50.0));
		CHECK(comando.after(QStringLiteral("b")).x() == Approx(10.0));

			//Perguntar por quem ele não move devolve uma posição
			//que não é uma, e nunca o canto da chapa.
		CHECK(qIsNaN(comando.before(QStringLiteral("a")).x()));
		CHECK(qIsNaN(comando.after(QStringLiteral("a")).x()));
	}

	SECTION("a peça que já está no destino não entra no passo")
	{
		QHash<QString, QPointF> destinos;
		destinos.insert(QStringLiteral("a"), QPointF(10.0, 300.0));

		AlignMountedPartsCommand comando(&scene, destinos,
						 QStringLiteral("Aligner"));

		CHECK(comando.count() == 0);
		CHECK(comando.isNull());
	}

	SECTION("sem cena não é passo, e uma identidade que não existe some")
	{
		QHash<QString, QPointF> destinos;
		destinos.insert(QStringLiteral("nada"), QPointF(10.0, 10.0));

		AlignMountedPartsCommand fantasma(&scene, destinos,
						  QStringLiteral("Aligner"));
		AlignMountedPartsCommand orfao(nullptr, destinos,
					       QStringLiteral("Aligner"));

		CHECK(fantasma.count() == 0);
		CHECK(fantasma.isNull());
		CHECK(orfao.isNull());
	}
}
