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

#include "../../../../sources/QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountedprofileitem.h"
#include "../../../../sources/location/layout/mountinglayouteditor.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/location/mountingprofile.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/mountpartcommand.h"
#include "../../../../sources/undocommand/stretchmountedprofilecommand.h"

#include <catch2/catch.hpp>

#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QUndoStack>

/*
	O trilho e a canaleta no desenho: montar, desmontar, esticar — e o que
	cada gesto deixa na pilha de desfazer.

	Está nesta suíte, e não junto das regras puras, porque uma QGraphicsScene
	precisa de uma QApplication e porque a metade que interessa aqui é a que
	uma lista de retângulos não tem: que pôr uma peça na placa não jogue fora
	o desfazer do que já estava lá, que a alça da ponta mude o modelo e não
	só a tinta, e que o perfil chegue ao projeto. A aritmética do perfil —
	qual lado é o corte, onde a ponta puxada para — fica um andar abaixo, em
	src/mountingprofile_test.cpp, e não se repete aqui.

	O que não está em suíte nenhuma é a janela como o olho a vê: se a alça
	azul é visível o bastante, se o pente da canaleta se lê a esta escala.
	Isso é roteiro manual, e continua sendo.
*/

namespace
{
	/// O menor projeto que abre: uma folha, nada nela.
	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Sheet\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/// @return o trilho DIN de todo painel
	MountingProfile trilho()
	{
		return MountingProfile::rail(35.0, 7.5);
	}

	/// @return um pedaço de trilho deitado, de 480 mm, com identidade
	MountedItem barra(const QString &uuid = QStringLiteral("t1"),
			  qreal x = 10.0,
			  qreal y = 100.0,
			  qreal comprimento = 480.0)
	{
		MountedItem item;
		item.uuid = uuid;
		item.profile = trilho();
		item.run = MountingRun::Across;
		item.position = QPointF(x, y);
		item.size = trilho().sizeFor(comprimento, MountingRun::Across);
		return item;
	}

	/// @return um disjuntor, que se compra e não se corta
	MountedItem disjuntor()
	{
		MountedItem item(QStringLiteral("-Q1"),
				 QPointF(60.0, 110.0),
				 QSizeF(22.5, 85.0));
		item.uuid = QStringLiteral("q1");
		item.part_code = QStringLiteral("A9F74210");
		return item;
	}

	/// @return uma placa de 600 × 800 com um disjuntor nela
	MountingSurface placa()
	{
		MountingSurface superficie(QStringLiteral("QCM1"),
					   QStringLiteral("plate"),
					   MountingArea(600.0, 800.0));
		superficie.uuid = QStringLiteral("face");
		superficie.name = QStringLiteral("Platine");
		superficie.items << disjuntor();
		return superficie;
	}

	/// @return quantas alças de redimensionamento há na cena
	int alcas(const MountingScene &cena)
	{
		int total = 0;
		const QList<QGraphicsItem *> itens = cena.items();

		for (QGraphicsItem *item : itens)
		{
			if (item->type() == QetGraphicsHandlerItem::Type) {
				++ total;
			}
		}

		return total;
	}
}

TEST_CASE("T19 — a peça cortada se desenha com o item que sabe ser cortado",
	  "[uibench][calepinagem]")
{
	MountingScene cena;
	MountingSurface superficie = placa();
	superficie.items << barra();
	cena.setSurface(superficie);

	SECTION("o trilho é um item de perfil, e o disjuntor não")
	{
		MountedPartItem *peca = cena.partItem(QStringLiteral("t1"));
		MountedPartItem *breaker = cena.partItem(QStringLiteral("q1"));

		REQUIRE(peca != nullptr);
		REQUIRE(breaker != nullptr);
		CHECK(peca->type() == MountedProfileItem::Type);
		CHECK(breaker->type() == MountedPartItem::Type);
	}

	SECTION("o milímetro do modelo é o milímetro do desenho, aqui também")
	{
		MountedPartItem *peca = cena.partItem(QStringLiteral("t1"));
		REQUIRE(peca != nullptr);

		CHECK(peca->drawnRect().width() == Approx(480.0));
		CHECK(peca->drawnRect().height() == Approx(35.0));
		CHECK(peca->millimetrePosition().x() == Approx(10.0));
	}

	SECTION("as alças aparecem ao selecionar e vão embora ao largar")
	{
		MountedPartItem *peca = cena.partItem(QStringLiteral("t1"));
		REQUIRE(peca != nullptr);
		REQUIRE(alcas(cena) == 0);

		peca->setSelected(true);
		CHECK(alcas(cena) == 2);

		peca->setSelected(false);
		CHECK(alcas(cena) == 0);
	}

	SECTION("o disjuntor não ganha alça nenhuma, porque não se corta")
	{
		MountedPartItem *breaker = cena.partItem(QStringLiteral("q1"));
		REQUIRE(breaker != nullptr);

		breaker->setSelected(true);
		CHECK(alcas(cena) == 0);
	}

	SECTION("uma barra que ninguém cortou não ganha alça: o marcador não é medida")
	{
		MountedItem sem_corte = barra(QStringLiteral("t2"));
		sem_corte.size = QSizeF();

		MountingSurface outra = placa();
		outra.items << sem_corte;
		cena.setSurface(outra);

		MountedPartItem *peca = cena.partItem(QStringLiteral("t2"));
		REQUIRE(peca != nullptr);
		CHECK_FALSE(peca->hasDeclaredSize());

		peca->setSelected(true);
		CHECK(alcas(cena) == 0);
	}
}

TEST_CASE("T19 — pôr uma peça na placa é um passo, e não joga fora os anteriores",
	  "[uibench][calepinagem]")
{
	MountingScene cena;
	cena.setSurface(placa());
	REQUIRE(cena.partCount() == 1);

	SECTION("o passo existe, desfaz e refaz")
	{
			//É este o defeito que a primeira metade desta janela
			//tinha: pôr uma peça reconstruía o desenho inteiro, e
			//com ele ia embora a pilha. Um desfazer que existe às
			//vezes é pior que desfazer nenhum.
		REQUIRE(cena.moveItem(QStringLiteral("q1"),
				      QPointF(80.0, 120.0)));
		REQUIRE(cena.undoStack().count() == 1);

		QString erro;
		REQUIRE(cena.mountItem(barra(), &erro));
		CHECK(erro.isEmpty());
		CHECK(cena.undoStack().count() == 2);
		CHECK(cena.partCount() == 2);

		cena.undoStack().undo();
		CHECK(cena.partCount() == 1);
		CHECK(cena.partItem(QStringLiteral("t1")) == nullptr);

			//O passo anterior continua lá: é isso que se perdia.
		CHECK(cena.surface().item(QStringLiteral("q1"))
		      .position.x() == Approx(80.0));

		cena.undoStack().redo();
		CHECK(cena.partCount() == 2);
		REQUIRE(cena.partItem(QStringLiteral("t1")) != nullptr);
		CHECK(cena.surface().item(QStringLiteral("t1"))
		      .cutLength() == Approx(480.0));
	}

	SECTION("a peça volta ao lugar que ocupava na lista")
	{
			//A ordem da lista entra na comparação que decide se o
			//projeto tem algo a salvar. Uma peça que voltasse para o
			//fim faria um projeto idêntico a si mesmo pedir para ser
			//salvo.
		REQUIRE(cena.mountItem(barra()));
		REQUIRE(cena.mountItem(barra(QStringLiteral("t2"), 10.0, 300.0)));
		REQUIRE(cena.indexOfItem(QStringLiteral("t1")) == 1);

		REQUIRE(cena.unmountItem(QStringLiteral("t1")));
		CHECK(cena.indexOfItem(QStringLiteral("t1")) == -1);

		cena.undoStack().undo();
		CHECK(cena.indexOfItem(QStringLiteral("t1")) == 1);
	}

	SECTION("desmontar devolve a peça inteira, perfil e corte incluídos")
	{
		REQUIRE(cena.mountItem(barra()));
		REQUIRE(cena.unmountItem(QStringLiteral("t1")));
		CHECK(cena.partCount() == 1);

		cena.undoStack().undo();

		const MountedItem devolvida = cena.surface()
					      .item(QStringLiteral("t1"));
		CHECK(devolvida.profile.isRail());
		CHECK(devolvida.profile.section == Approx(35.0));
		CHECK(devolvida.cutLength() == Approx(480.0));
		CHECK(cena.partItem(QStringLiteral("t1"))->type()
		      == MountedProfileItem::Type);
	}

	SECTION("peça sem identidade é recusada, e diz por quê")
	{
		MountedItem sem_nome = barra(QString());
		QString erro;

		CHECK_FALSE(cena.mountItem(sem_nome, &erro));
		CHECK_FALSE(erro.isEmpty());
		CHECK(cena.undoStack().count() == 0);
	}

	SECTION("a mesma identidade duas vezes é recusada")
	{
		QString erro;

		CHECK_FALSE(cena.mountItem(disjuntor(), &erro));
		CHECK_FALSE(erro.isEmpty());
		CHECK(cena.partCount() == 1);
	}

	SECTION("desmontar o que não está lá é recusado")
	{
		QString erro;

		CHECK_FALSE(cena.unmountItem(QStringLiteral("nao-existe"),
					     &erro));
		CHECK_FALSE(erro.isEmpty());
	}

	SECTION("a legenda do passo chama o trilho pelo perfil")
	{
		MountPartCommand comando(&cena, barra(),
					 MountPartCommand::Mount);

		CHECK_FALSE(comando.isNull());
		CHECK(comando.itemUuid() == QStringLiteral("t1"));
		CHECK(comando.text().contains(
			      trilho().designation()));
	}
}

TEST_CASE("T19 — esticar a peça muda o modelo, e desfazer devolve o corte",
	  "[uibench][calepinagem]")
{
	MountingScene cena;
	MountingSurface superficie = placa();
	superficie.items << barra();
	cena.setSurface(superficie);

	SECTION("o comprimento novo entra no modelo, e o antigo volta")
	{
		const QRectF esticado(10.0, 100.0, 560.0, 35.0);

		REQUIRE(cena.stretchItem(QStringLiteral("t1"), esticado));
		REQUIRE(cena.undoStack().count() == 1);

		CHECK(cena.surface().item(QStringLiteral("t1"))
		      .cutLength() == Approx(560.0));

		cena.undoStack().undo();
		CHECK(cena.surface().item(QStringLiteral("t1"))
		      .cutLength() == Approx(480.0));

		cena.undoStack().redo();
		CHECK(cena.surface().item(QStringLiteral("t1"))
		      .cutLength() == Approx(560.0));
	}

	SECTION("puxar pela ponta de trás move o canto, e o modelo sabe")
	{
		const QRectF puxado(60.0, 100.0, 430.0, 35.0);

		REQUIRE(cena.stretchItem(QStringLiteral("t1"), puxado));

		const MountedItem depois = cena.surface()
					   .item(QStringLiteral("t1"));
		CHECK(depois.position.x() == Approx(60.0));
		CHECK(depois.cutLength() == Approx(430.0));

		cena.undoStack().undo();

		const MountedItem antes = cena.surface()
					  .item(QStringLiteral("t1"));
		CHECK(antes.position.x() == Approx(10.0));
		CHECK(antes.cutLength() == Approx(480.0));
	}

	SECTION("o perfil atravessa o corte sem se perder")
	{
		REQUIRE(cena.stretchItem(QStringLiteral("t1"),
					 QRectF(10.0, 100.0, 300.0, 35.0)));

		const MountedItem cortada = cena.surface()
					    .item(QStringLiteral("t1"));
		CHECK(cortada.profile.isRail());
		CHECK(cortada.profile.depth == Approx(7.5));
		CHECK(cortada.run == MountingRun::Across);
	}

	SECTION("cortar um disjuntor é recusado, e diz por quê")
	{
			//A medida de um artigo comprado é a do catálogo. Um
			//desenho que a deixasse esticar seria um desenho
			//discordando do produto.
		QString erro;

		CHECK_FALSE(cena.stretchItem(QStringLiteral("q1"),
					     QRectF(60.0, 110.0, 400.0, 85.0),
					     &erro));
		CHECK_FALSE(erro.isEmpty());
		CHECK(cena.undoStack().count() == 0);
	}

	SECTION("cortar para o mesmo retângulo não deixa passo nenhum")
	{
		CHECK_FALSE(cena.stretchItem(QStringLiteral("t1"),
					     QRectF(10.0, 100.0, 480.0, 35.0)));
		CHECK(cena.undoStack().count() == 0);
	}

	SECTION("o passo de corte sabe quando não é passo")
	{
		StretchMountedProfileCommand nulo(
					&cena, QStringLiteral("t1"),
					QRectF(10.0, 100.0, 480.0, 35.0),
					QRectF(10.0, 100.0, 480.0, 35.0));
		StretchMountedProfileCommand orfao(
					nullptr, QStringLiteral("t1"),
					QRectF(0.0, 0.0, 100.0, 35.0),
					QRectF(0.0, 0.0, 200.0, 35.0));

		CHECK(nulo.isNull());
		CHECK(orfao.isNull());
	}

	SECTION("as alças seguem o corte, em vez de ficarem onde estavam")
	{
		MountedProfileItem *peca = static_cast<MountedProfileItem *>(
					cena.partItem(QStringLiteral("t1")));
		REQUIRE(peca != nullptr);

		peca->setSelected(true);
		REQUIRE(alcas(cena) == 2);

		REQUIRE(cena.stretchItem(QStringLiteral("t1"),
					 QRectF(10.0, 100.0, 560.0, 35.0)));

		CHECK(alcas(cena) == 2);
		CHECK(peca->footprintRect().right() == Approx(570.0));
	}
}

TEST_CASE("T19 — o trilho posto na janela chega ao projeto e volta do arquivo",
	  "[uibench][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("layout-profile.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	MountingLayout layout;
	const QString face = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1"),
						QStringLiteral("plate"),
						MountingArea(600.0, 800.0)));
	REQUIRE_FALSE(face.isEmpty());
	scratch->setMountingLayout(layout);

	SECTION("posto pela janela, ele está no projeto antes de qualquer salvamento")
	{
		MountingLayoutEditor editor(scratch.project());
		REQUIRE(editor.shownSurface() == face);

		QString erro;
		const QString posto = editor.addProfile(trilho(), 480.0,
							MountingRun::Across,
							&erro);
		INFO(erro.toStdString());
		REQUIRE_FALSE(posto.isEmpty());

		const MountedItem guardado = scratch->mountingLayout()
					     .item(posto);
		CHECK(guardado.profile.isRail());
		CHECK(guardado.cutLength() == Approx(480.0));

		editor.close();
	}

	SECTION("e sobrevive a salvar e reabrir, que é onde ele sumiria")
	{
		QString posto;
		{
			MountingLayoutEditor editor(scratch.project());
			REQUIRE(editor.shownSurface() == face);

			posto = editor.addProfile(MountingProfile::duct(40.0, 60.0),
						  560.0, MountingRun::Down);
			REQUIRE_FALSE(posto.isEmpty());

				//Cortado depois de posto, para que o que vai ao
				//arquivo seja o corte e não o comprimento de
				//nascença.
			REQUIRE(editor.scene()->stretchItem(
					posto,
					QRectF(10.0, 10.0, 40.0, 700.0)));
			editor.close();
		}

		REQUIRE(scratch.saveAndReopen());

		const MountedItem relido = scratch->mountingLayout().item(posto);
		CHECK(relido.profile.isDuct());
		CHECK(relido.profile.section == Approx(40.0));
		CHECK(relido.profile.depth == Approx(60.0));
		CHECK(relido.run == MountingRun::Down);
		CHECK(relido.cutLength() == Approx(700.0));
	}

	SECTION("um perfil sem largura é recusado, e nada é posto")
	{
		MountingLayoutEditor editor(scratch.project());
		QString erro;

		const QString posto = editor.addProfile(
					MountingProfile(
						MountingProfile::railKind(),
						0.0),
					480.0, MountingRun::Across, &erro);

		CHECK(posto.isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(scratch->mountingLayout().itemCount() == 0);

		editor.close();
	}

	SECTION("um comprimento que não é número é recusado")
	{
		MountingLayoutEditor editor(scratch.project());
		QString erro;

		CHECK(editor.addProfile(trilho(), 0.0, MountingRun::Across,
					&erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(scratch->mountingLayout().itemCount() == 0);

		editor.close();
	}

	SECTION("e a lista de material soma o que foi cortado, por barra")
	{
		MountingLayoutEditor editor(scratch.project());

		REQUIRE_FALSE(editor.addProfile(trilho(), 480.0,
						MountingRun::Across).isEmpty());
		REQUIRE_FALSE(editor.addProfile(trilho(), 240.0,
						MountingRun::Across).isEmpty());

		const QList<MountingProfileTotal> linhas =
				scratch->mountingLayout().profileTotals();

		REQUIRE(linhas.count() == 1);
		CHECK(linhas.first().pieces == 2);
		CHECK(linhas.first().length == Approx(720.0));
		CHECK(linhas.first().metres() == Approx(0.72));

		editor.close();
	}
}
