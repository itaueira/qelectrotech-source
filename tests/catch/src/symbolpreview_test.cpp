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
#include <catch2/catch.hpp>

#include <QSignalSpy>
#include <QTest>

#include "qt_catch_tostring.h"

#include "../../../sources/ElementsCollection/ui/symbolpreview.h"

/*
	O primeiro teste de widget deste projeto.

	Ele existe para responder uma pergunta do projetista — "dá para um script
	interagir com o programa e fazer os testes mais massantes?" — com um exemplo
	em vez de uma opinião. E ele testa exatamente a coisa que o teste E.3 da
	bancada guiada não conseguiu testar: se dá para saber qual linha da tabela
	é qual ponto do desenho.

	Nada aparece na tela: o `main.cpp` da suíte força QT_QPA_PLATFORM=offscreen,
	e o QTest::mouseClick entrega o evento ao widget, não à área de trabalho.
	A distinção é a razão de isto ser permitido aqui.
*/

namespace
{
	/// Bobina desenhada como retângulo com dois rabichos, como no E.1.
	SymbolDefinition drawnCoil()
	{
		SymbolDefinition symbol;
		symbol.name = QStringLiteral("Bobina");
		symbol.class_key = QStringLiteral("contactor");
		symbol.shapes << SymbolShape(SymbolShapeType::Rectangle,
					     QPolygonF() << QPointF(90.0, 90.0)
							 << QPointF(110.0, 110.0));
		symbol.shapes << SymbolShape(SymbolShapeType::Line,
					     QPolygonF() << QPointF(100.0, 90.0)
							 << QPointF(100.0, 80.0));
		symbol.shapes << SymbolShape(SymbolShapeType::Line,
					     QPolygonF() << QPointF(100.0, 110.0)
							 << QPointF(100.0, 120.0));

		SymbolTerminal cima(QPointF(100.0, 80.0), Qet::North);
		cima.label = QStringLiteral("A1");
		SymbolTerminal baixo(QPointF(100.0, 120.0), Qet::South);
		baixo.label = QStringLiteral("A2");
		symbol.terminals << cima << baixo;
		symbol.hotspot = QPointF(100.0, 80.0);
		return symbol;
	}
}

TEST_CASE("CU-35.3 — o desenho diz qual ponto é qual", "[widget][symbol]")
{
	SymbolPreview preview;
	preview.resize(280, 240);
	preview.setSymbol(drawnCoil());

	SECTION("cada ponto tem um lugar no widget, e são lugares diferentes")
	{
			//É o que o teste E.3 da bancada guiada não conseguiu exercitar: sem
			//correspondência entre linha e ponto, declarar contato é
			//adivinhação.
		const QPointF primeiro = preview.widgetPositionOf(0);
		const QPointF segundo = preview.widgetPositionOf(1);

		CHECK_FALSE(primeiro.isNull());
		CHECK_FALSE(segundo.isNull());
		CHECK(QLineF(primeiro, segundo).length() > 10.0);

			//E o de cima está mesmo acima do de baixo na tela: o desenho não
			//está de cabeça para baixo.
		CHECK(primeiro.y() < segundo.y());

			//Índice que não existe devolve nulo em vez de estourar.
		CHECK(preview.widgetPositionOf(2).isNull());
		CHECK(preview.widgetPositionOf(-1).isNull());
	}

	SECTION("clicar o ponto no desenho seleciona aquele ponto")
	{
			//O caminho de volta, que é o que o projetista mais usa: ele está
			//olhando o desenho e quer dizer o que aquele ponto é.
		QSignalSpy escolhido(&preview, &SymbolPreview::terminalPicked);
		REQUIRE(escolhido.isValid());

		const QPoint alvo = preview.widgetPositionOf(1).toPoint();
		QTest::mouseClick(&preview, Qt::LeftButton, Qt::KeyboardModifiers(),
				  alvo);

		REQUIRE(escolhido.count() == 1);
		CHECK(escolhido.takeFirst().at(0).toInt() == 1);
		CHECK(preview.highlighted() == 1);
	}

	SECTION("clicar longe de qualquer ponto não escolhe nada")
	{
			//Senão o clique para dar zoom, ou o clique acidental, trocaria a
			//linha selecionada por baixo do projetista.
		QSignalSpy escolhido(&preview, &SymbolPreview::terminalPicked);
		preview.setHighlighted(0);

		QTest::mouseClick(&preview, Qt::LeftButton, Qt::KeyboardModifiers(),
				  QPoint(4, 4));

		CHECK(escolhido.count() == 0);
		CHECK(preview.highlighted() == 0);
	}

	SECTION("a tabela escolhe, e o desenho acompanha")
	{
		preview.setHighlighted(1);
		CHECK(preview.highlighted() == 1);
		preview.setHighlighted(0);
		CHECK(preview.highlighted() == 0);
		preview.setHighlighted(-1);
		CHECK(preview.highlighted() == -1);
	}

	SECTION("símbolo vazio não estoura e não desenha ponto nenhum")
	{
		SymbolPreview vazio;
		vazio.resize(120, 100);
		vazio.setSymbol(SymbolDefinition());
		CHECK(vazio.widgetPositionOf(0).isNull());

		QSignalSpy escolhido(&vazio, &SymbolPreview::terminalPicked);
		QTest::mouseClick(&vazio, Qt::LeftButton, Qt::KeyboardModifiers(),
				  QPoint(60, 50));
		CHECK(escolhido.count() == 0);
	}

	SECTION("desenho de uma linha só, sem largura, ainda tem posição")
	{
			//A transformação divide pela largura da caixa; uma linha reta
			//vertical tem largura zero. É o caso que estoura se ninguém
			//pensar nele.
		SymbolDefinition reta;
		reta.shapes << SymbolShape(SymbolShapeType::Line,
					   QPolygonF() << QPointF(50.0, 10.0)
						       << QPointF(50.0, 90.0));
		reta.terminals << SymbolTerminal(QPointF(50.0, 10.0), Qet::North);
		reta.hotspot = QPointF(50.0, 10.0);

		SymbolPreview linha;
		linha.resize(200, 200);
		linha.setSymbol(reta);

		const QPointF onde = linha.widgetPositionOf(0);
		CHECK_FALSE(onde.isNull());
		CHECK(onde.x() > 0.0);
		CHECK(onde.x() < 200.0);
	}
}
