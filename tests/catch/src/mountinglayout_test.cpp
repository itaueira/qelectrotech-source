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
#include "../../../sources/location/mountinglayout.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QDomElement>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>

namespace
{
		/// @return uma peça como o editor de calepinagem vai entregá-la
	MountedItem peca(const char *rotulo,
			 const char *codigo,
			 qreal x, qreal y,
			 qreal largura, qreal altura)
	{
		MountedItem item(QString::fromUtf8(rotulo),
				 QPointF(x, y),
				 QSizeF(largura, altura));
		item.part_code = QString::fromUtf8(codigo);
		return item;
	}

		/// @return a placa de fundo de uma localização, medida em milímetro
	MountingSurface placa(const char *localizacao, qreal largura, qreal altura)
	{
		return MountingSurface(QString::fromUtf8(localizacao),
				       QStringLiteral("plate"),
				       MountingArea(largura, altura));
	}

		/// @return o elemento de uma face escrito à mão, como um arquivo o guarda
	QDomElement faceEscrita(QDomDocument &documento,
				const char *uuid,
				const char *localizacao,
				const char *face = nullptr)
	{
		QDomElement e = documento.createElement(MountingSurface::tagName());
		e.setAttribute(QStringLiteral("uuid"), QString::fromUtf8(uuid));
		e.setAttribute(QStringLiteral("location"),
			       QString::fromUtf8(localizacao));
		if (face) {
			e.setAttribute(QStringLiteral("kind"),
				       QString::fromUtf8(face));
		}
		return e;
	}
}

TEST_CASE("T19 — a superfície guarda em milímetro o que está parafusado nela",
	  "[calepinagem]")
{
	MountingLayout layout;
	REQUIRE(layout.isEmpty());
	REQUIRE(layout.itemCount() == 0);

	const QString face = layout.appendSurface(placa("QCM1", 600, 800));
	REQUIRE_FALSE(face.isEmpty());
	CHECK_FALSE(layout.isEmpty());
	CHECK(layout.count() == 1);
	CHECK(layout.surface(face).area.width == 600.0);
	CHECK(layout.surface(face).area.height == 800.0);

	const QString q1 = layout.mountItem(
				face, peca("-Q1", "A9F74210", 10, 20, 22.5, 85));
	const QString km1 = layout.mountItem(
				face, peca("-KM1", "LC1D09", 40, 20, 45, 77));
	REQUIRE_FALSE(q1.isEmpty());
	REQUIRE_FALSE(km1.isEmpty());
	CHECK(layout.itemCount() == 2);
	CHECK(layout.surfaceOfItem(q1) == face);
	CHECK(layout.holdsItem(km1));

		//O milímetro é o que fica guardado: 22,5 de disjuntor não passa
		//por passo de grade nem por fator de escala no caminho.
	CHECK(layout.item(q1).size.width() == 22.5);
	CHECK(layout.item(q1).size.height() == 85.0);
	CHECK(layout.item(q1).position.x() == 10.0);
	CHECK(layout.item(q1).position.y() == 20.0);
	CHECK(layout.item(q1).part_code == QString("A9F74210"));

	SECTION("o mesmo componente não se parafusa em dois lugares")
	{
		MountedItem repetida = peca("-Q1", "A9F74210", 200, 300, 22.5, 85);
		repetida.uuid = q1;

		QString erro;
		CHECK(layout.mountItem(face, repetida, &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(layout.itemCount() == 2);

		const QString porta = layout.appendSurface(
					MountingSurface(QStringLiteral("QCM1/PORTA"),
							QStringLiteral("door"),
							MountingArea(500, 700)));
		REQUIRE_FALSE(porta.isEmpty());
		CHECK(layout.mountItem(porta, repetida, &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(layout.surfaceOfItem(q1) == face);
	}

	SECTION("mudar de face não inventa posição nova")
	{
		const QString porta = layout.appendSurface(
					MountingSurface(QStringLiteral("QCM1/PORTA"),
							QStringLiteral("door"),
							MountingArea(500, 700)));
		REQUIRE(layout.moveItem(q1, porta));
		CHECK(layout.surfaceOfItem(q1) == porta);
		CHECK(layout.item(q1).position.x() == 10.0);
		CHECK(layout.item(q1).position.y() == 20.0);
		CHECK(layout.surface(face).itemCount() == 1);
		CHECK(layout.itemCount() == 2);

			//Mover para onde já está não é mudança nenhuma, e não
			//pode deixar o projeto pedindo para salvar.
		QString erro;
		CHECK_FALSE(layout.moveItem(q1, porta, &erro));
		CHECK(erro.isEmpty());
	}

	SECTION("apagar a face devolve o que estava nela, e não apaga o componente")
	{
		QStringList soltos;
		REQUIRE(layout.removeSurface(face, &soltos));
		CHECK(layout.isEmpty());
		CHECK(soltos.count() == 2);
		CHECK(soltos.contains(q1));
		CHECK(soltos.contains(km1));
		CHECK_FALSE(layout.holdsItem(q1));
	}

	SECTION("reposicionar uma peça é uma operação e não quatro")
	{
		MountedItem movida = layout.item(q1);
		movida.position = QPointF(300, 400);
		REQUIRE(layout.updateItem(movida));
		CHECK(layout.item(q1).position.x() == 300.0);

			//Escrever de volta o mesmo não é mudança.
		QString erro;
		CHECK_FALSE(layout.updateItem(movida, &erro));
		CHECK(erro.isEmpty());

			//E uma peça que não está montada em lugar nenhum não se
			//escreve de volta calada.
		MountedItem estranha = peca("-Q9", "A9F74210", 0, 0, 22.5, 85);
		estranha.uuid = QStringLiteral("nao-montada");
		CHECK_FALSE(layout.updateItem(estranha, &erro));
		CHECK_FALSE(erro.isEmpty());
	}
}

TEST_CASE("T19 — a escrita recusa o que deixaria o projeto sem saber onde a peça está",
	  "[calepinagem]")
{
	MountingLayout layout;
	QString erro;

	SECTION("uma face de nenhuma localização")
	{
		MountingSurface orfa;
		orfa.kind = QStringLiteral("plate");
		orfa.area = MountingArea(600, 800);
		CHECK(layout.appendSurface(orfa, &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(layout.isEmpty());
	}

	SECTION("uma face que não diz de que face se trata")
	{
		MountingSurface sem_face;
		sem_face.location_path = QStringLiteral("QCM1");
		sem_face.area = MountingArea(600, 800);
		CHECK(layout.appendSurface(sem_face, &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(layout.isEmpty());
	}

	SECTION("um identificador que o calepinagem já usa")
	{
		MountingSurface primeira = placa("QCM1", 600, 800);
		primeira.uuid = QStringLiteral("F1");
		REQUIRE(layout.appendSurface(primeira) == QString("F1"));

		MountingSurface segunda = placa("QCM2", 400, 600);
		segunda.uuid = QStringLiteral("F1");
		CHECK(layout.appendSurface(segunda, &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(layout.count() == 1);
	}

	SECTION("duas placas na mesma localização são duas placas, e isso é aceito")
	{
			//Um armário alto tem mesmo placa de cima e placa de
			//baixo, e um gabinete tem mesmo lateral esquerda e
			//direita: a unicidade é do identificador e nunca do par
			//localização mais face.
		MountingSurface alta = placa("QCM1", 600, 400);
		alta.name = QString::fromUtf8("Placa superior");
		MountingSurface baixa = placa("QCM1", 600, 400);
		baixa.name = QString::fromUtf8("Placa inferior");
		REQUIRE_FALSE(layout.appendSurface(alta).isEmpty());
		REQUIRE_FALSE(layout.appendSurface(baixa).isEmpty());
		CHECK(layout.count() == 2);
		CHECK(layout.surfacesOfLocation(QStringLiteral("QCM1")).count() == 2);
	}

	SECTION("a face de QCM1 não é a face de QCM10")
	{
		REQUIRE_FALSE(layout.appendSurface(placa("QCM1", 600, 800)).isEmpty());
		REQUIRE_FALSE(layout.appendSurface(placa("QCM10", 300, 400)).isEmpty());
		CHECK(layout.surfacesOfLocation(QStringLiteral("QCM1")).count() == 1);
		CHECK(layout.surfacesOfLocation(QStringLiteral("QCM10")).count() == 1);
		CHECK(layout.surfacesOfLocation(QStringLiteral("QCM")).isEmpty());
	}

	SECTION("uma régua inteira entra com a face, ou não entra")
	{
		MountingSurface primeira = placa("QCM1", 600, 800);
		MountedItem x1 = peca("-X1", "BORNE-2.5", 10, 10, 6, 60);
		x1.uuid = QStringLiteral("B1");
		primeira.items << x1;
		REQUIRE_FALSE(layout.appendSurface(primeira).isEmpty());

		MountingSurface segunda = placa("QCM2", 600, 800);
		segunda.items << x1;
		CHECK(layout.appendSurface(segunda, &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(layout.count() == 1);
		CHECK(layout.itemCount() == 1);
	}
}

TEST_CASE("T19 — trocar o armário roda sobre o que está montado, e nada some calado",
	  "[calepinagem]")
{
	MountingLayout layout;
	const QString face = layout.appendSurface(placa("QCM1", 600, 800));
	const QString q1  = layout.mountItem(
				face, peca("-Q1", "A9F74210", 10, 20, 22.5, 85));
	const QString km1 = layout.mountItem(
				face, peca("-KM1", "LC1D09", 500, 20, 45, 77));
	const QString t1  = layout.mountItem(
				face, peca("-T1", "ABL8", 10, 400, 450, 200));
	REQUIRE(layout.itemCount() == 3);

	SECTION("a placa menor relata quem não passa, e não tira ninguém da lista")
	{
		EnclosureTransferPlan plano;
		QString erro;
		REQUIRE(layout.applyArea(face, MountingArea(400, 800), &plano, &erro));
		CHECK(erro.isEmpty());

		CHECK(layout.surface(face).area.width == 400.0);
		CHECK(layout.itemCount() == 3);
		CHECK(plano.transferCount() == 1);
		CHECK(plano.lossCount() == 2);
		CHECK(plano.lostDesignations().contains(QString("-KM1")));
		CHECK(plano.lostDesignations().contains(QString("-T1")));

			//Arrastar resolve um caso e não resolve o outro, e essa é
			//a diferença que o relatório tem de manter.
		CHECK(plano.entryOf(km1).fit == MountingFit::OutsideArea);
		CHECK(plano.entryOf(t1).fit == MountingFit::LargerThanArea);
		CHECK(plano.entryOf(q1).fit == MountingFit::Fits);

			//Quem não passa fica onde estava, em milímetro: nada é
			//reescalado e nada é empurrado para dentro.
		CHECK(layout.item(km1).position.x() == 500.0);
		CHECK(layout.item(q1).position.x() == 10.0);
		CHECK(layout.item(q1).position.y() == 20.0);
	}

	SECTION("uma superfície sem medida é recusada, e o relatório vem junto")
	{
		EnclosureTransferPlan plano;
		QString erro;
		CHECK_FALSE(layout.applyArea(face, MountingArea(), &plano, &erro));
		CHECK_FALSE(erro.isEmpty());

		CHECK(layout.surface(face).area.width == 600.0);
		CHECK(layout.itemCount() == 3);
		CHECK_FALSE(plano.isUsable());
		CHECK(plano.lossCount() == 3);
		CHECK(plano.entryOf(q1).fit == MountingFit::NoArea);
	}

	SECTION("o mesmo par de números não é uma alteração")
	{
		EnclosureTransferPlan plano;
		QString erro;
		CHECK_FALSE(layout.applyArea(face, MountingArea(600, 800),
					     &plano, &erro));
		CHECK(erro.isEmpty());
		CHECK(plano.transferCount() == 3);
	}

	SECTION("perguntar não altera nada")
	{
		const EnclosureTransferPlan plano =
				layout.planForSurface(face, MountingArea(400, 800));
		CHECK(plano.lossCount() == 2);
		CHECK(layout.surface(face).area.width == 600.0);
		CHECK(layout.itemCount() == 3);
	}

	SECTION("uma face que não existe devolve plano vazio e não escreve")
	{
		EnclosureTransferPlan plano;
		QString erro;
		CHECK_FALSE(layout.applyArea(QStringLiteral("nao-existe"),
					     MountingArea(400, 800),
					     &plano, &erro));
		CHECK_FALSE(erro.isEmpty());
		CHECK(plano.entries.isEmpty());
		CHECK(layout.surface(face).area.width == 600.0);
	}
}

TEST_CASE("T19 — a face sem medida continua sendo uma face, e o que está nela continua listado",
	  "[calepinagem]")
{
	MountingLayout layout;
	const QString face = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1"),
						QStringLiteral("plate")));
	REQUIRE_FALSE(face.isEmpty());
	CHECK_FALSE(layout.surface(face).area.isValid());

	const QString q1 = layout.mountItem(
				face, peca("-Q1", "A9F74210", 10, 20, 22.5, 85));
	REQUIRE_FALSE(q1.isEmpty());

		//Uma peça cuja medida ninguém cadastrou é montada, e é relatada
		//como sem medida — nunca recusada e nunca com tamanho inventado.
	MountedItem sem_medida(QStringLiteral("-X1"), QPointF(100, 100), QSizeF());
	const QString x1 = layout.mountItem(face, sem_medida);
	REQUIRE_FALSE(x1.isEmpty());
	CHECK(layout.itemCount() == 2);
	CHECK_FALSE(layout.item(x1).hasDeclaredSize());
	CHECK(layout.item(x1).declaredSize().width() == 0.0);
	CHECK(layout.item(x1).designation() == QString("-X1"));

	const EnclosureTransferPlan plano =
			layout.planForSurface(face, MountingArea(600, 800));
	CHECK(plano.withUnknownSize().count() == 1);
	CHECK(plano.transferCount() == 2);
}

TEST_CASE("T19 — o que está montado volta do arquivo igual ao que entrou",
	  "[calepinagem]")
{
	MountingLayout layout;
	MountingSurface fundo = placa("QCM1", 600, 800);
	fundo.name = QString::fromUtf8("Placa de fundo");
	const QString face = layout.appendSurface(fundo);
	const QString q1 = layout.mountItem(
				face, peca("-Q1", "A9F74210", 10.5, 20.25, 22.5, 85));
	const QString x1 = layout.mountItem(
				face, MountedItem(QStringLiteral("-X1"),
						  QPointF(0, 0),
						  QSizeF()));
		//Uma face de que se mediu só a largura: o arquivo tem de poder
		//guardar exatamente isso.
	const QString porta = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1/PORTA"),
						QStringLiteral("door"),
						MountingArea(500, 0)));
	REQUIRE_FALSE(porta.isEmpty());

	QDomDocument documento;
	documento.appendChild(layout.toXml(documento));

	MountingLayout lido;
	REQUIRE(lido.fromXml(documento.documentElement()));
	CHECK(lido == layout);
	CHECK(lido.count() == 2);
	CHECK(lido.itemCount() == 2);
	CHECK(lido.surfaceOfItem(q1) == face);
	CHECK(lido.item(q1).position.x() == 10.5);
	CHECK(lido.item(q1).position.y() == 20.25);
	CHECK(lido.item(q1).size.width() == 22.5);
	CHECK(lido.item(q1).label == QString("-Q1"));
	CHECK(lido.item(q1).part_code == QString("A9F74210"));
	CHECK(lido.surface(face).name == QString::fromUtf8("Placa de fundo"));

		//O canto de origem é um lugar como outro qualquer, e a medida
		//ausente continua ausente: nem o (0, 0) vira omissão, nem a
		//medida em branco vira zero medido.
	CHECK(lido.item(x1).position.x() == 0.0);
	CHECK_FALSE(lido.item(x1).hasDeclaredSize());
	CHECK(lido.surface(porta).area.width == 500.0);
	CHECK(lido.surface(porta).area.height == 0.0);
	CHECK_FALSE(lido.surface(porta).area.isValid());
	CHECK(lido.surface(porta).kind == QString("door"));

	SECTION("as duas grafias de « sem medida » não são uma alteração")
	{
			//Um QSizeF recém-construído carrega -1 por -1, e não zero
			//por zero; a medida ausente que volta do arquivo carrega
			//zero. As duas dizem que ninguém mediu a peça, e um
			//projeto aberto e salvo não pode pedir para ser salvo de
			//novo por causa dessa diferença de grafia.
		CHECK(layout.item(x1).size.width() == -1.0);
		CHECK(lido.item(x1).size.width() == 0.0);
		CHECK_FALSE(layout.item(x1).hasDeclaredSize());
		CHECK_FALSE(lido.item(x1).hasDeclaredSize());
		CHECK(lido == layout);

			//Uma coordenada negativa, ao contrário, é um lugar de
			//verdade — uma peça um milímetro à esquerda da placa — e
			//não se confunde com medida ausente.
		MountedItem fora = lido.item(x1);
		fora.position = QPointF(-1, 0);
		MountingLayout mexido = lido;
		REQUIRE(mexido.updateItem(fora));
		CHECK(mexido != lido);
	}

	SECTION("um projeto que nunca abriu a calepinagem abre igual")
	{
		MountingLayout vazio;
		CHECK_FALSE(vazio.fromXml(QDomElement()));
		CHECK(vazio.isEmpty());

		QDomDocument outro;
		outro.appendChild(outro.createElement(QStringLiteral("location_tree")));
		CHECK_FALSE(vazio.fromXml(outro.documentElement()));
		CHECK(vazio.isEmpty());
	}

	SECTION("ler duas vezes não soma o arquivo com ele mesmo")
	{
		REQUIRE(lido.fromXml(documento.documentElement()));
		CHECK(lido.count() == 2);
		CHECK(lido.itemCount() == 2);
	}
}

TEST_CASE("T19 — o arquivo pode guardar o que a calepinagem não aceita, e a leitura conserta",
	  "[calepinagem]")
{
	QDomDocument documento;
	QDomElement raiz = documento.createElement(MountingLayout::tagName());
	documento.appendChild(raiz);

	QDomElement primeira = faceEscrita(documento, "F1", "QCM1", "plate");
	primeira.setAttribute(QStringLiteral("width"), QStringLiteral("600"));
	primeira.setAttribute(QStringLiteral("height"), QStringLiteral("800"));

		//Uma peça sem identificador nenhum, e sem medida nenhuma.
	QDomElement anonima = documento.createElement(MountingSurface::itemTagName());
	anonima.setAttribute(QStringLiteral("label"), QStringLiteral("-Q1"));
	anonima.setAttribute(QStringLiteral("x"), QStringLiteral("10"));
	anonima.setAttribute(QStringLiteral("y"), QStringLiteral("20"));
	primeira.appendChild(anonima);

		//E uma peça com o identificador que a face já usa.
	QDomElement colidida = documento.createElement(MountingSurface::itemTagName());
	colidida.setAttribute(QStringLiteral("uuid"), QStringLiteral("F1"));
	colidida.setAttribute(QStringLiteral("label"), QStringLiteral("-KM1"));
	colidida.setAttribute(QStringLiteral("width"), QStringLiteral("45"));
	colidida.setAttribute(QStringLiteral("height"), QStringLiteral("77"));
	primeira.appendChild(colidida);

		//Uma segunda face com o mesmo identificador da primeira, e que
		//não diz de que face se trata.
	QDomElement segunda = faceEscrita(documento, "F1", "QCM1/PORTA");

	raiz.appendChild(primeira);
	raiz.appendChild(segunda);

	MountingLayout lido;
	REQUIRE(lido.fromXml(documento.documentElement()));

		//Nada é descartado por estar repetido nem por estar incompleto.
	CHECK(lido.count() == 2);
	CHECK(lido.itemCount() == 2);
	CHECK(lido.at(0).uuid == QString("F1"));
	CHECK(lido.at(1).uuid != QString("F1"));
	CHECK_FALSE(lido.at(1).uuid.isEmpty());
	CHECK(lido.at(1).kind == MountingSurface::defaultKind());
	CHECK(lido.at(0).area.width == 600.0);

	const QStringList pecas = lido.at(0).itemUuids();
	REQUIRE(pecas.count() == 2);
	CHECK_FALSE(pecas.at(0).isEmpty());
	CHECK(pecas.at(1) != QString("F1"));
	CHECK(pecas.at(0) != pecas.at(1));
	CHECK(lido.holdsItem(pecas.at(0)));
	CHECK(lido.holdsItem(pecas.at(1)));
	CHECK_FALSE(lido.item(pecas.at(0)).hasDeclaredSize());
	CHECK(lido.item(pecas.at(0)).designation() == QString("-Q1"));
	CHECK(lido.item(pecas.at(1)).size.width() == 45.0);

		//E o que a leitura consertou é aceito pela escrita: escrever e
		//ler nunca mais muda nada.
	QDomDocument segunda_volta;
	segunda_volta.appendChild(lido.toXml(segunda_volta));
	MountingLayout relido;
	REQUIRE(relido.fromXml(segunda_volta.documentElement()));
	CHECK(relido == lido);
}
