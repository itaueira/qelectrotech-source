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

#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/connector/connectorcheck.h"
#include "../../../../sources/connector/connectorswap.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	Duas vias de um conector trocando de número num projeto aberto
	(T34, passo 5).

	A regra — qual rótulo vai para onde, e qual troca é recusada — é
	`connectorswap_test.cpp` da suíte pura e não se repete aqui. O que se
	prova aqui é a metade que uma regra sobre lista não pode ter:

	- **nada se move.** Depois da troca, a lista do conector lida de novo
	  traz os mesmos componentes nas mesmas posições, e cada um continua
	  desenhado onde estava. Uma implementação que trocasse os dois símbolos
	  de lugar em vez dos números imprimiria a mesma tabela ordenada por
	  via, e é a coordenada na folha que a reprova;
	- **o esquema acompanha.** O texto que a folha desenha ao lado do pino
	  passa a mostrar o número novo sem que ninguém peça redesenho, porque
	  quem o atualiza é o sinal de mudança de informação do elemento;
	- **um passo só.** Um Ctrl+Z desfaz a troca inteira, e duas trocas do
	  mesmo par continuam sendo duas — o comando reusado se funde com o
	  anterior quando os dois seguram os mesmos componentes, e é a macro que
	  segura isso.

	Rotulado T34 e não CU-34.4: o caso de uso é a troca feita a partir da
	tabela do conector, e a tabela é o passo 6. Nenhuma janela é aberta aqui
	— não há diálogo nenhum nesta suíte —, então o que a tela faz continua
	sendo roteiro para uma pessoa.
*/

namespace
{
	using DrawnConnector = ConnectorCheck::DrawnConnector;
	using Refusal = ConnectorSwap::Refusal;
	using Report = ConnectorCheck::Report;

	/// Um componente da bancada.
	struct Drawn
	{
		int folio;             ///< 1-based, como o leitor os conta
		int x;
		int y;
		const char *label;     ///< o número da via
		const char *connector; ///< o conector, pode ser nada
	};

	/**
		Oito componentes em duas folhas, e cada um responde a uma pergunta.

		CN1 tem cinco vias, uma delas escrita "cn1" e uma delas desenhada
		na segunda folha: a primeira porque a troca não pode ser recusada
		por causa de uma maiúscula, a segunda porque a pilha de desfazer é
		do projeto e não da folha, e uma troca entre folhas tem de
		continuar sendo um passo só.

		XS2 é o outro conector, para a recusa; e B1 é uma broche que não
		pertence a conector nenhum, que tem recusa própria.

		Cada componente carrega um texto ligado à informação `label`, que é
		o que a folha desenha ao lado dele. É por ele que se mede "o esquema
		acompanha": ninguém o atualiza à mão, ele segue o sinal do elemento.
	*/
	QString fixtureXml()
	{
		const Drawn drawn[] = {
			{1, 100, 100, "1",  "CN1"},
			{1, 140, 100, "2",  "CN1"},
			{1, 180, 100, "3",  "CN1"},
			{1, 220, 100, "4",  "cn1"},
			{1, 100, 200, "7",  "XS2"},
			{1, 140, 200, "8",  "XS2"},
			{1, 100, 300, "B1", ""},
			{2, 100, 100, "5",  "CN1"}};

		QString folio_one;
		QString folio_two;
		int index = 0;
		for (const Drawn &component : drawn)
		{
			QString information = QStringLiteral(
						      "<elementInformation show=\"1\" name=\"label\">%1"
						      "</elementInformation>")
					      .arg(QLatin1String(component.label));
			if (component.connector[0] != '\0') {
				information += QStringLiteral(
						       "<elementInformation show=\"1\" name=\"connector\">%1"
						       "</elementInformation>")
					       .arg(QLatin1String(component.connector));
			}

				//Um uuid por instância e um id por borne, pelo mesmo
				//motivo que a bancada vizinha registra: um XML que
				//repete o par faz o programa guardar o primeiro e
				//descartar os outros em silêncio.
			const QString instance = QStringLiteral(
							 "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
							 " freezeLabel=\"false\" orientation=\"0\""
							 " type=\"embed://bench/pin.elmt\""
							 " uuid=\"{c0ffee05-0000-4000-8000-%3}\">"
							 "<terminals>"
							 "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
							 "</terminals>"
							 "<inputs/>"
							 "<elementInformations>%5</elementInformations>"
							 "<dynamic_texts>"
							 "<dynamic_elmt_text text_from=\"ElementInfo\""
							 " uuid=\"{dadada05-0000-4000-8000-%3}\""
							 " x=\"0\" y=\"-12\" rotation=\"0\""
							 " font=\"Liberation Sans,9,-1,5,50,0,0,0,0,0\""
							 " frame=\"false\" text_width=\"-1\""
							 " Halignment=\"AlignLeft\" Valignment=\"AlignTop\">"
							 "<text>(vazio)</text>"
							 "<info_name>label</info_name>"
							 "</dynamic_elmt_text>"
							 "</dynamic_texts>"
							 "<texts_groups/>"
							 "</element>")
						 .arg(component.x)
						 .arg(component.y)
						 .arg(index, 12, 16, QLatin1Char('0'))
						 .arg(index)
						 .arg(information);

			if (component.folio == 1) {
				folio_one += instance;
			} else {
				folio_two += instance;
			}
			++index;
		}

		auto folio = [](int order, const QString &title, const QString &elements)
		{
			return QStringLiteral(
				       "<diagram title=\"%1\" order=\"%2\" height=\"900\""
				       " cols=\"17\" colsize=\"50\" rows=\"10\" rowsize=\"80\""
				       " displaycols=\"true\" displayrows=\"true\">"
				       "<elements>%3</elements>"
				       "<inputs/><conductors/>"
				       "</diagram>")
			       .arg(title).arg(order).arg(elements);
		};

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">"
			       "<element name=\"pin.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Pin</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category></collection>"
			       "%1%2"
			       "</project>")
		       .arg(folio(1, QStringLiteral("Bench 1"), folio_one),
			    folio(2, QStringLiteral("Bench 2"), folio_two));
	}

	/**
		A lista de broches de @a key, na ordem de leitura.

		Lida pelo relatório do passo 4 com um catálogo fechado: quais
		broches dizem pertencer a qual conector está escrito na folha e não
		no catálogo, então a lista sai inteira sem que a bancada precise
		cadastrar peça nenhuma. Medido, e não suposto: a bancada vizinha
		prova que um catálogo que não abriu ainda devolve os conectores e
		as broches deles.
	*/
	QList<Element *> pinsOf(QETProject *project, const QString &key)
	{
		Catalog closed;
		const Report report = ConnectorCheck::report(project, closed);
		for (const DrawnConnector &connector : report.connectors)
		{
			if (connector.key == key) {
				return connector.pins;
			}
		}
		return QList<Element *>();
	}

	/// O componente que carrega @a label, nulo quando nenhum carrega.
	Element *pinLabelled(QETProject *project, const QString &label)
	{
		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			const QList<Element *> elements = folio->elements();
			for (Element *element : elements)
			{
				if (element->elementInformations()
						.value(QETInformation::ELMT_LABEL)
						.toString() == label) {
					return element;
				}
			}
		}
		return nullptr;
	}

	/// Os uuid de @a pins, na ordem em que estão.
	QStringList uuids(const QList<Element *> &pins)
	{
		QStringList found;
		for (Element *pin : pins) {
			found << (pin ? pin->uuid().toString() : QString());
		}
		return found;
	}

	/// Os rótulos guardados de @a pins, na ordem em que estão.
	QStringList storedLabels(const QList<Element *> &pins)
	{
		QStringList found;
		for (Element *pin : pins)
		{
			found << (pin ? pin->elementInformations()
					.value(QETInformation::ELMT_LABEL).toString()
				      : QString());
		}
		return found;
	}

	/// Onde @a pins estão desenhados, na ordem em que estão.
	QList<QPointF> positions(const QList<Element *> &pins)
	{
		QList<QPointF> found;
		for (Element *pin : pins) {
			found << (pin ? pin->scenePos() : QPointF());
		}
		return found;
	}

	/**
		O que a folha escreve ao lado de @a pin.

		O item de texto de verdade, e não o valor guardado: é ele que muda
		por causa do sinal do elemento, e é ele que o projetista vê.
	*/
	QString drawnText(Element *pin)
	{
		if (!pin) {
			return QString();
		}
		const QList<DynamicElementTextItem *> texts = pin->dynamicTextItems();
		if (texts.isEmpty()) {
			return QString();
		}
		return texts.first()->toPlainText();
	}
}

TEST_CASE("T34 — trocar as vias 2 e 3 troca os números e não move nada",
	  "[uibench][t34][connector][swap]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("connectorswap.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagramCount() == 2);

	const QList<Element *> before = pinsOf(scratch.project(),
					       QStringLiteral("cn1"));
	REQUIRE(before.size() == 5);
	REQUIRE(storedLabels(before) == QStringList({QStringLiteral("1"),
						     QStringLiteral("2"),
						     QStringLiteral("3"),
						     QStringLiteral("4"),
						     QStringLiteral("5")}));

		//A folha já escreve o número antes de qualquer troca: sem isto, o
		//caso do esquema mediria dois textos vazios e daria verde.
	REQUIRE(drawnText(before.at(1)) == QStringLiteral("2"));
	REQUIRE(drawnText(before.at(2)) == QStringLiteral("3"));

	const QStringList uuids_before = uuids(before);
	const QList<QPointF> where_before = positions(before);
	const int steps_before = scratch.project()->undoStack()->index();

	const Refusal answer = ConnectorCheck::swapPins(before.at(1), before.at(2));
	REQUIRE(answer == Refusal::None);

	SECTION("os dois rótulos trocam, e nenhum outro muda")
	{
		CHECK(before.at(1)->elementInformations()
		      .value(QETInformation::ELMT_LABEL).toString()
		      == QStringLiteral("3"));
		CHECK(before.at(2)->elementInformations()
		      .value(QETInformation::ELMT_LABEL).toString()
		      == QStringLiteral("2"));

		CHECK(storedLabels(before) == QStringList({QStringLiteral("1"),
							   QStringLiteral("3"),
							   QStringLiteral("2"),
							   QStringLiteral("4"),
							   QStringLiteral("5")}));

			//E o outro conector não foi tocado.
		CHECK(storedLabels(pinsOf(scratch.project(), QStringLiteral("xs2")))
		      == QStringList({QStringLiteral("7"), QStringLiteral("8")}));
	}

	SECTION("a posição na lista não muda, e nem a posição na folha")
	{
		const QList<Element *> after = pinsOf(scratch.project(),
						      QStringLiteral("cn1"));

			//A asserção central do caso, e a que separa trocar de
			//reordenar: a lista lida de novo traz os mesmos cinco
			//componentes nos mesmos cinco lugares. Uma implementação
			//que movesse os símbolos e mantivesse os números imprimiria
			//a mesma tabela ordenada por via e cairia aqui.
		CHECK(after.size() == before.size());
		CHECK(uuids(after) == uuids_before);
		CHECK(positions(after) == where_before);

			//Dito pela outra ponta: cada componente continua desenhado
			//onde estava, e o que mudou nele foi o número.
		CHECK(before.at(1)->scenePos() == where_before.at(1));
		CHECK(before.at(2)->scenePos() == where_before.at(2));
		CHECK(before.at(1)->scenePos() != before.at(2)->scenePos());
	}

	SECTION("o esquema acompanha, sem pedir redesenho a ninguém")
	{
			//O texto que a folha desenha, e não o valor guardado. Ele
			//segue o sinal de mudança de informação do elemento; se a
			//troca escrevesse o dado sem passar por lá, a lista estaria
			//certa e o desenho continuaria mostrando os números velhos
			//até o projeto ser reaberto.
		CHECK(drawnText(before.at(1)) == QStringLiteral("3"));
		CHECK(drawnText(before.at(2)) == QStringLiteral("2"));

			//E o que a folha compõe concorda com o que ela desenha.
		const QStringList composed = UiBench::displayedLabels(scratch.diagram(0));
		CHECK(composed.contains(QStringLiteral("2")));
		CHECK(composed.contains(QStringLiteral("3")));
		CHECK(before.at(1)->displayedLabel() == QStringLiteral("3"));
		CHECK(before.at(2)->displayedLabel() == QStringLiteral("2"));
	}

	SECTION("é um passo de desfazer, e um Ctrl+Z põe tudo de volta")
	{
		CHECK(scratch.project()->undoStack()->index() == steps_before + 1);

			//E o passo se identifica pelos dois números, que é o que o
			//projetista reconhece na lista de desfazer.
		const QString step = UiBench::undoTopText(scratch.project());
		INFO(step.toStdString());
		CHECK(step.contains(QStringLiteral("2")));
		CHECK(step.contains(QStringLiteral("3")));

		scratch.project()->undoStack()->undo();

		CHECK(scratch.project()->undoStack()->index() == steps_before);
		CHECK(storedLabels(before) == QStringList({QStringLiteral("1"),
							   QStringLiteral("2"),
							   QStringLiteral("3"),
							   QStringLiteral("4"),
							   QStringLiteral("5")}));
		CHECK(drawnText(before.at(1)) == QStringLiteral("2"));
		CHECK(drawnText(before.at(2)) == QStringLiteral("3"));
		CHECK(positions(pinsOf(scratch.project(), QStringLiteral("cn1")))
		      == where_before);
	}
}

TEST_CASE("T34 — duas trocas do mesmo par são dois passos, e não um",
	  "[uibench][t34][connector][swap][undo]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("swaptwice.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const QList<Element *> pins = pinsOf(scratch.project(),
					     QStringLiteral("cn1"));
	REQUIRE(pins.size() == 5);

	const int steps_before = scratch.project()->undoStack()->index();

	REQUIRE(ConnectorCheck::swapPins(pins.at(1), pins.at(2)) == Refusal::None);
	REQUIRE(ConnectorCheck::swapPins(pins.at(1), pins.at(2)) == Refusal::None);

		//Dois gestos, dois passos. O comando reusado se funde com o
		//anterior quando os dois seguram os mesmos componentes — que é
		//exatamente o que duas trocas de um par são —, e fundidos a segunda
		//troca não poderia ser desfeita sozinha. É a macro que segura isso,
		//e é este número que a mede.
	CHECK(scratch.project()->undoStack()->index() == steps_before + 2);

		//A segunda troca devolveu os números ao lugar, e um desfazer só
		//tem de deixá-los cruzados de novo.
	CHECK(storedLabels(pins) == QStringList({QStringLiteral("1"),
						 QStringLiteral("2"),
						 QStringLiteral("3"),
						 QStringLiteral("4"),
						 QStringLiteral("5")}));

	scratch.project()->undoStack()->undo();
	CHECK(storedLabels(pins) == QStringList({QStringLiteral("1"),
						 QStringLiteral("3"),
						 QStringLiteral("2"),
						 QStringLiteral("4"),
						 QStringLiteral("5")}));

	scratch.project()->undoStack()->undo();
	CHECK(storedLabels(pins) == QStringList({QStringLiteral("1"),
						 QStringLiteral("2"),
						 QStringLiteral("3"),
						 QStringLiteral("4"),
						 QStringLiteral("5")}));
	CHECK(scratch.project()->undoStack()->index() == steps_before);
}

TEST_CASE("T34 — a troca recusada não gasta passo nem escreve nada",
	  "[uibench][t34][connector][swap][undo]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("refused.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const QList<Element *> cn1 = pinsOf(scratch.project(),
					    QStringLiteral("cn1"));
	const QList<Element *> xs2 = pinsOf(scratch.project(),
					    QStringLiteral("xs2"));
	REQUIRE(cn1.size() == 5);
	REQUIRE(xs2.size() == 2);

	const int steps_before = scratch.project()->undoStack()->index();
	const QStringList labels_before = storedLabels(cn1);

	SECTION("uma broche com ela mesma")
	{
		CHECK(ConnectorCheck::swapPins(cn1.at(1), cn1.at(1))
		      == Refusal::SamePin);
		CHECK(scratch.project()->undoStack()->index() == steps_before);
		CHECK(storedLabels(cn1) == labels_before);
	}

	SECTION("duas broches de conectores diferentes")
	{
			//Recusada e não feita pela metade: a troca entre conectores
			//tiraria uma via de um e poria no outro, e mover uma broche
			//para outro conector é a outra porta, a que escreve o campo
			//do conector.
		CHECK(ConnectorCheck::swapPins(cn1.at(1), xs2.at(0))
		      == Refusal::OtherConnector);
		CHECK(scratch.project()->undoStack()->index() == steps_before);
		CHECK(storedLabels(cn1) == labels_before);
		CHECK(storedLabels(xs2) == QStringList({QStringLiteral("7"),
							QStringLiteral("8")}));
	}

	SECTION("uma broche que não pertence a conector nenhum")
	{
		Element *loose = pinLabelled(scratch.project(),
					     QStringLiteral("B1"));
		REQUIRE(loose != nullptr);
		REQUIRE(loose->elementInformations()
			.value(QETInformation::ELMT_CONNECTOR).toString().isEmpty());

		CHECK(ConnectorCheck::swapPins(cn1.at(1), loose)
		      == Refusal::NoConnector);
		CHECK(scratch.project()->undoStack()->index() == steps_before);
		CHECK(storedLabels(cn1) == labels_before);
	}

	SECTION("um ponteiro nulo")
	{
		CHECK(ConnectorCheck::swapPins(cn1.at(1), nullptr)
		      == Refusal::NoSuchPin);
		CHECK(ConnectorCheck::swapPins(nullptr, nullptr)
		      == Refusal::NoSuchPin);
		CHECK(scratch.project()->undoStack()->index() == steps_before);
	}
}

TEST_CASE("T34 — duas grafias de um conector trocam, e as duas folhas também",
	  "[uibench][t34][connector][swap]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("spelling.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const QList<Element *> pins = pinsOf(scratch.project(),
					     QStringLiteral("cn1"));
	REQUIRE(pins.size() == 5);

	SECTION("«CN1» e «cn1» são um conector, e a troca não é recusada pela maiúscula")
	{
		REQUIRE(pins.at(3)->elementInformations()
			.value(QETInformation::ELMT_CONNECTOR).toString()
			== QStringLiteral("cn1"));

		CHECK(ConnectorCheck::swapPins(pins.at(2), pins.at(3))
		      == Refusal::None);
		CHECK(storedLabels(pins) == QStringList({QStringLiteral("1"),
							 QStringLiteral("2"),
							 QStringLiteral("4"),
							 QStringLiteral("3"),
							 QStringLiteral("5")}));

			//E a grafia de cada broche fica como o usuário a escreveu:
			//a troca escreve número de via, não corrige texto digitado.
		CHECK(pins.at(2)->elementInformations()
		      .value(QETInformation::ELMT_CONNECTOR).toString()
		      == QStringLiteral("CN1"));
		CHECK(pins.at(3)->elementInformations()
		      .value(QETInformation::ELMT_CONNECTOR).toString()
		      == QStringLiteral("cn1"));
	}

	SECTION("duas broches em folhas diferentes trocam num passo só")
	{
			//A pilha de desfazer é do projeto e não da folha, e um
			//conector que atravessa folhas é o caso comum de porta de
			//quadro: um lado na porta, o outro no corpo.
		REQUIRE(pins.at(0)->diagram() != pins.at(4)->diagram());

		const int steps_before = scratch.project()->undoStack()->index();
		CHECK(ConnectorCheck::swapPins(pins.at(0), pins.at(4))
		      == Refusal::None);
		CHECK(scratch.project()->undoStack()->index() == steps_before + 1);
		CHECK(storedLabels(pins) == QStringList({QStringLiteral("5"),
							 QStringLiteral("2"),
							 QStringLiteral("3"),
							 QStringLiteral("4"),
							 QStringLiteral("1")}));

			//As duas folhas mostram o número novo, e um Ctrl+Z devolve
			//as duas.
		CHECK(drawnText(pins.at(0)) == QStringLiteral("5"));
		CHECK(drawnText(pins.at(4)) == QStringLiteral("1"));

		scratch.project()->undoStack()->undo();
		CHECK(drawnText(pins.at(0)) == QStringLiteral("1"));
		CHECK(drawnText(pins.at(4)) == QStringLiteral("5"));
	}
}

TEST_CASE("T34 — a troca sobrevive a salvar e reabrir",
	  "[uibench][t34][connector][swap]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("reopen.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	{
		const QList<Element *> pins = pinsOf(scratch.project(),
						     QStringLiteral("cn1"));
		REQUIRE(pins.size() == 5);
		REQUIRE(ConnectorCheck::swapPins(pins.at(1), pins.at(2))
			== Refusal::None);
	}

		//Todo ponteiro para dentro do projeto anterior fica pendurado
		//aqui: a lista é lida de novo do outro lado.
	REQUIRE(scratch.saveAndReopen());

	const QList<Element *> reopened = pinsOf(scratch.project(),
						 QStringLiteral("cn1"));
	REQUIRE(reopened.size() == 5);

		//A ordem de leitura é a mesma porque nada se moveu, e os números
		//estão cruzados: é a prova de que a troca é dado do projeto e não
		//estado de tela.
	CHECK(storedLabels(reopened) == QStringList({QStringLiteral("1"),
						     QStringLiteral("3"),
						     QStringLiteral("2"),
						     QStringLiteral("4"),
						     QStringLiteral("5")}));
	CHECK(drawnText(reopened.at(1)) == QStringLiteral("3"));
	CHECK(drawnText(reopened.at(2)) == QStringLiteral("2"));
}
