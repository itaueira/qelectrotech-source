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

#include "../../../../sources/diagram.h"
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountedprofileitem.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/location/mountingprofile.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/mountinglayoutviewitem.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QDomElement>
#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

/*
	A platine desenhada numa folha do projeto, e o fio que a mantém viva.

	O que este arquivo prova é o que nenhuma outra suíte alcança: que o item
	da folha lê a placa do QETProject, que ele se redesenha sozinho quando
	alguém muda a placa em outro lugar, e que **nada** do que ele desenha
	volta para o modelo. A aritmética da placa fica em
	src/mountinglayout_test.cpp, a cena em milímetro em
	src/ui/mountingscene_test.cpp, e nenhuma das duas se repete aqui.

	O tamanho do desenho no papel é a outra metade, e ela é do olho: a
	conferência de escala com régua sobre a folha impressa continua sendo
	roteiro manual. O que se prova aqui por número é o fator — quanto de
	folio um milímetro vira — e não o que a impressora faz com ele.
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

	/// @return um disjuntor de largura real, num milímetro que ninguém arredonda
	MountedItem disjuntor(const QString &uuid = QStringLiteral("q1"),
			      qreal x = 10.5,
			      qreal y = 20.25)
	{
		MountedItem item(QStringLiteral("-Q1"),
				 QPointF(x, y),
				 QSizeF(22.5, 85.0));
		item.uuid = uuid;
		item.part_code = QStringLiteral("A9F74210");
		return item;
	}

	/// @return uma peça que ninguém mediu: o modelo continua sem saber o tamanho
	MountedItem naoMedida(const QString &uuid = QStringLiteral("x1"))
	{
		MountedItem item;
		item.uuid = uuid;
		item.label = QStringLiteral("-X1");
		item.position = QPointF(200.0, 300.0);
		return item;
	}

	/// @return um pedaço de trilho deitado, de 480 mm
	MountedItem barra(const QString &uuid = QStringLiteral("t1"))
	{
		MountedItem item;
		item.uuid = uuid;
		item.profile = MountingProfile::rail(35.0, 7.5);
		item.run = MountingRun::Across;
		item.position = QPointF(10.0, 100.0);
		item.size = item.profile.sizeFor(480.0, MountingRun::Across);
		return item;
	}

	/// @return um layout com uma placa de 600 x 800 e o que se pedir nela
	MountingLayout layoutCom(const QList<MountedItem> &pecas,
				 QString *face_uuid)
	{
		MountingLayout layout;
		MountingSurface face(QStringLiteral("QCM1"),
				     QStringLiteral("plate"),
				     MountingArea(600.0, 800.0));
		face.name = QStringLiteral("Platine principale");

		const QString added = layout.appendSurface(face);
		if (face_uuid) {
			*face_uuid = added;
		}

		for (const MountedItem &peca : pecas) {
			layout.mountItem(added, peca);
		}

		return layout;
	}

	/// @return o item de calepinage desenhado nesta folha, nullptr quando não há
	MountingLayoutViewItem *vistaDaFolha(Diagram *diagram)
	{
		if (!diagram) {
			return nullptr;
		}

		const QList<QGraphicsItem *> itens = diagram->items();
		for (QGraphicsItem *item : itens)
		{
			if (item->type() == MountingLayoutViewItem::Type) {
				return static_cast<MountingLayoutViewItem *>(item);
			}
		}

		return nullptr;
	}
}

TEST_CASE("T19 — a platine entra numa folha do projeto",
	  "[uibench][calepinagem]")
{
	SECTION("a vista desenha o que está montado na face que ela nomeia")
	{
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("vista.qet"));
		INFO(scratch.error().toStdString());
		REQUIRE(scratch.isOpen());
		REQUIRE(scratch.diagram(0));

		QString face;
		scratch->setMountingLayout(
					layoutCom({disjuntor(), barra()}, &face));
		REQUIRE_FALSE(face.isEmpty());

		auto *vista = new MountingLayoutViewItem();
		scratch.diagram(0)->addItem(vista);
		vista->setProject(scratch.project());
		vista->setSurfaceUuid(face);

		REQUIRE(vista->hasSurface());
		CHECK(vista->error().isEmpty());
		CHECK(vista->partCount() == 2);
		CHECK(vista->drawnItemUuids()
		      == QStringList({QStringLiteral("q1"),
				      QStringLiteral("t1")}));

			//O disjuntor no milímetro que o modelo guarda, sem
			//arredondamento nenhum pelo caminho: é o que separa esta
			//vista de um item de folha comum.
		MountedPartItem *peca = vista->partItem(QStringLiteral("q1"));
		REQUIRE(peca);
		CHECK(peca->millimetrePosition().x() == Approx(10.5));
		CHECK(peca->millimetrePosition().y() == Approx(20.25));
		CHECK(peca->drawnRect().width() == Approx(22.5));

			//E o trilho é desenhado pela mesma classe que o desenha na
			//janela de calepinage, e não por um segundo pintor.
		MountedPartItem *trilho = vista->partItem(QStringLiteral("t1"));
		REQUIRE(trilho);
		CHECK(trilho->type() == MountedProfileItem::Type);
	}

	SECTION("mudar a placa no projeto redesenha a folha sem ninguém pedir")
	{
		/*
			O passo inteiro está aqui. Se esta seção cair, a vista é uma
			fotografia: ela continuará desenhando a placa da tarde em que
			alguém a posou, e o projeto passará a ter duas verdades sobre
			o mesmo painel.
		*/
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("vista.qet"));
		REQUIRE(scratch.isOpen());

		QString face;
		MountingLayout layout = layoutCom({disjuntor()}, &face);
		scratch->setMountingLayout(layout);

		auto *vista = new MountingLayoutViewItem();
		scratch.diagram(0)->addItem(vista);
		vista->setProject(scratch.project());
		vista->setSurfaceUuid(face);
		REQUIRE(vista->partCount() == 1);

			//Um segundo componente é parafusado na placa, pelo modelo.
			//Ninguém toca no item da folha.
		layout.mountItem(face, disjuntor(QStringLiteral("q2"), 80.0, 20.0));
		scratch->setMountingLayout(layout);

		CHECK(vista->partCount() == 2);
		REQUIRE(vista->partItem(QStringLiteral("q2")));
		CHECK(vista->partItem(QStringLiteral("q2"))
		      ->millimetrePosition().x() == Approx(80.0));

			//E arrastar um componente na placa move o desenho da folha.
		MountedItem movido = layout.item(QStringLiteral("q1"));
		movido.position = QPointF(300.0, 400.0);
		REQUIRE(layout.updateItem(movido));
		scratch->setMountingLayout(layout);

		REQUIRE(vista->partItem(QStringLiteral("q1")));
		CHECK(vista->partItem(QStringLiteral("q1"))
		      ->millimetrePosition() == QPointF(300.0, 400.0));

			//E tirar o componente da placa o tira da folha.
		REQUIRE(layout.unmountItem(QStringLiteral("q1")));
		scratch->setMountingLayout(layout);
		CHECK(vista->partCount() == 1);
		CHECK(vista->partItem(QStringLiteral("q1")) == nullptr);
	}

	SECTION("a folha não escreve na placa")
	{
		/*
			Três fechaduras, e a seção mede as três. Sem elas existem dois
			donos do mesmo número, e o milímetro da placa passa a depender
			de em qual janela alguém arrastou por último.
		*/
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("vista.qet"));
		REQUIRE(scratch.isOpen());

		QString face;
		scratch->setMountingLayout(layoutCom({disjuntor(), barra()}, &face));
		const MountingLayout antes = scratch->mountingLayout();

		auto *vista = new MountingLayoutViewItem();
		scratch.diagram(0)->addItem(vista);
		vista->setProject(scratch.project());
		vista->setSurfaceUuid(face);
		vista->setDrawingScale(2.0);
		REQUIRE(vista->partCount() == 2);

			//Nenhum gesto alcança uma peça desenhada na folha.
		const QStringList uuids = vista->drawnItemUuids();
		for (const QString &uuid : uuids)
		{
			MountedPartItem *peca = vista->partItem(uuid);
			REQUIRE(peca);
			INFO(uuid.toStdString());
			CHECK_FALSE(peca->flags().testFlag(
					    QGraphicsItem::ItemIsMovable));
			CHECK_FALSE(peca->flags().testFlag(
					    QGraphicsItem::ItemIsSelectable));
			CHECK(peca->acceptedMouseButtons() == Qt::NoButton);
		}

			//E pôr, redesenhar e reescalar não mexeram num milímetro.
		CHECK(scratch->mountingLayout() == antes);
	}

	SECTION("peça sem medida continua sem medida na folha")
	{
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("vista.qet"));
		REQUIRE(scratch.isOpen());

		QString face;
		scratch->setMountingLayout(layoutCom({naoMedida()}, &face));

		auto *vista = new MountingLayoutViewItem();
		scratch.diagram(0)->addItem(vista);
		vista->setProject(scratch.project());
		vista->setSurfaceUuid(face);

		MountedPartItem *peca = vista->partItem(QStringLiteral("x1"));
		REQUIRE(peca);

			//O marcador é desenhado, e é do tamanho que a classe nomeia.
		CHECK(peca->drawnRect().width()
		      == Approx(MountedPartItem::unmeasuredMarkerSide()));

			//E nem o marcador nem a folha ensinaram ao modelo um tamanho
			//que ninguém mediu.
		CHECK_FALSE(peca->hasDeclaredSize());
		CHECK(peca->declaredSize() == QSizeF(0.0, 0.0));
		CHECK_FALSE(scratch->mountingLayout()
			    .item(QStringLiteral("x1")).hasDeclaredSize());
	}

	SECTION("uma placa que sumiu não derruba o programa, e diz o que houve")
	{
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("vista.qet"));
		REQUIRE(scratch.isOpen());

		QString face;
		MountingLayout layout = layoutCom({disjuntor()}, &face);
		scratch->setMountingLayout(layout);

		auto *vista = new MountingLayoutViewItem();
		scratch.diagram(0)->addItem(vista);
		vista->setProject(scratch.project());
		vista->setSurfaceUuid(face);
		REQUIRE(vista->hasSurface());

		REQUIRE(layout.removeSurface(face));
		scratch->setMountingLayout(layout);

		CHECK_FALSE(vista->hasSurface());
		CHECK(vista->partCount() == 0);
		CHECK_FALSE(vista->error().isEmpty());
			//Ela continua sabendo qual placa procurava, para que voltar a
			//criar a face com o mesmo identificador reacenda o desenho.
		CHECK(vista->surfaceUuid() == face);
			//E um item sem caixa nenhuma seria um item que não se pode
			//clicar para apagar.
		CHECK_FALSE(vista->boundingRect().isEmpty());

			//As outras duas maneiras de não ter o que desenhar dizem
			//coisas diferentes, porque o que se faz a respeito é
			//diferente.
		MountingLayoutViewItem solta;
		CHECK_FALSE(solta.error().isEmpty());
		CHECK(solta.error() != vista->error());
	}

	SECTION("o fator é o único lugar onde milímetro vira unidade de folio")
	{
		MountingLayoutViewItem vista;
		CHECK(vista.drawingScale()
		      == Approx(MountingLayoutViewItem::defaultDrawingScale()));

			//A placa de 600 x 800, que é a mais comum aqui, cabe na área
			//desenhável de 1020 x 640 de um folio padrão -- que é a
			//conta que justifica o padrão.
		CHECK(vista.folioLengthFromMillimetre(600.0) == Approx(300.0));
		CHECK(vista.folioLengthFromMillimetre(800.0) == Approx(400.0));

		vista.setDrawingScale(2.0);
		CHECK(vista.folioLengthFromMillimetre(22.5) == Approx(45.0));
		CHECK(vista.millimetreFromFolioLength(45.0) == Approx(22.5));

			//Ida e volta, que é o que impede o fator de existir em dois
			//lugares com dois valores.
		CHECK(vista.millimetreFromFolioLength(
			      vista.folioLengthFromMillimetre(137.5))
		      == Approx(137.5));

			//Um fator impossível é limitado, e não recusado: ele chega de
			//arquivo tanto quanto de gente, e zero desenharia uma placa de
			//tamanho nenhum.
		vista.setDrawingScale(0.0);
		CHECK(vista.drawingScale()
		      == Approx(MountingLayoutViewItem::minimumDrawingScale()));
		vista.setDrawingScale(-3.0);
		CHECK(vista.drawingScale()
		      == Approx(MountingLayoutViewItem::minimumDrawingScale()));
		vista.setDrawingScale(1e9);
		CHECK(vista.drawingScale()
		      == Approx(MountingLayoutViewItem::maximumDrawingScale()));
	}

	SECTION("a vista atravessa salvar e reabrir, e volta viva")
	{
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("vista.qet"));
		REQUIRE(scratch.isOpen());

		QString face;
		scratch->setMountingLayout(layoutCom({disjuntor(), barra()}, &face));

		auto *vista = new MountingLayoutViewItem();
		scratch.diagram(0)->addItem(vista);
		vista->setProject(scratch.project());
		vista->setSurfaceUuid(face);
		vista->setDrawingScale(0.25);
		vista->setPos(120.0, 80.0);
		REQUIRE(vista->partCount() == 2);

			//Lida de volta do item e não escrita como literal: setPos de
			//um item de folha encaixa na grade, e o passo da grade vem
			//das preferências do usuário.
		const QPointF posto = vista->pos();

			//O que o item escreve de si, atributo por atributo. Quatro, e
			//nenhum deles é um milímetro da placa: a lista do que está
			//parafusado é escrita uma vez, pelo projeto, e uma cópia aqui
			//seria uma segunda resposta envelhecendo em silêncio.
		QDomDocument documento;
		const QDomElement escrito_item = vista->toXml(documento);
		CHECK(escrito_item.tagName() == MountingLayoutViewItem::tagName());
		CHECK(escrito_item.attributes().count() == 4);
		CHECK(escrito_item.attribute(QStringLiteral("surface")) == face);
		CHECK_FALSE(escrito_item.hasChildNodes());

		REQUIRE(scratch.saveAndReopen());

		const QString escrito = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(escrito.isEmpty());
			//O identificador da face escrito na folha, e não só o nome da
			//seção: "mounting_layout_views" contém "mounting_layout_view"
			//como pedaço de texto, então procurar só a etiqueta passaria
			//com a seção vazia dentro.
		CHECK(escrito.contains(QStringLiteral("surface=\"%1\"").arg(face)));
		CHECK(escrito.contains(QStringLiteral("scale=\"0.25\"")));

		MountingLayoutViewItem *relida = vistaDaFolha(scratch.diagram(0));
		REQUIRE(relida);
		CHECK(relida->surfaceUuid() == face);
		CHECK(relida->drawingScale() == Approx(0.25));
		CHECK(relida->pos() == posto);
		CHECK(relida->project() == scratch.project());

			//E ela volta desenhando, e volta ligada: uma vista que
			//sobrevivesse morta seria pior que nenhuma.
		REQUIRE(relida->partCount() == 2);
		MountingLayout layout = scratch->mountingLayout();
		MountedItem movido = layout.item(QStringLiteral("q1"));
		movido.position = QPointF(55.0, 66.0);
		REQUIRE(layout.updateItem(movido));
		scratch->setMountingLayout(layout);

		REQUIRE(relida->partItem(QStringLiteral("q1")));
		CHECK(relida->partItem(QStringLiteral("q1"))
		      ->millimetrePosition() == QPointF(55.0, 66.0));
	}

	SECTION("uma folha que nunca recebeu calepinage sai do arquivo como sempre saiu")
	{
		/*
			O controle negativo da seção acima: sem esta, aquela passaria
			num Diagram que escrevesse a seção tivesse ele item ou não.
		*/
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("vista.qet"));
		REQUIRE(scratch.isOpen());
		REQUIRE(scratch.saveAndReopen());

		const QString escrito = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(escrito.isEmpty());
		CHECK_FALSE(escrito.contains(MountingLayoutViewItem::tagName()));
		CHECK(vistaDaFolha(scratch.diagram(0)) == nullptr);
	}
}
