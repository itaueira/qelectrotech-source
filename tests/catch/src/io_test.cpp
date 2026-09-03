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
#include "../../../sources/plc/iolist.h"
#include "../../../sources/plc/iopoint.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QDomElement>
#include <QList>

namespace
{
		/**
			One line of the two column sheet of CU-11.1: a description and
			nothing else, which is what the automation hands over first.
		*/
	IoPoint basica(const char *description)
	{
		return IoPoint(QString::fromUtf8(description));
	}

		/// One line of the full sheet, the one that arrives later.
	IoPoint completa(const char *tag,
			 const char *address,
			 const char *description,
			 ElementData::PlcIOType type = ElementData::EntreeDigitale)
	{
		IoPoint point;
		point.tag = QString::fromUtf8(tag);
		point.address = QString::fromUtf8(address);
		point.description = QString::fromUtf8(description);
		point.type = type;
		return point;
	}
}

TEST_CASE("T11 — o tipo de E/S vai e volta pelas duas letras da planilha",
	  "[io]")
{
	SECTION("as seis letras que o programa escreve")
	{
		REQUIRE(IoPoint::typeToString(ElementData::EntreeDigitale)
			== QStringLiteral("DI"));
		REQUIRE(IoPoint::typeToString(ElementData::SortieDigitale)
			== QStringLiteral("DO"));
		REQUIRE(IoPoint::typeToString(ElementData::EntreeAnalogique)
			== QStringLiteral("AI"));
		REQUIRE(IoPoint::typeToString(ElementData::SortieAnalogique)
			== QStringLiteral("AO"));
		REQUIRE(IoPoint::typeToString(ElementData::EntreeUniverselle)
			== QStringLiteral("UI"));
		REQUIRE(IoPoint::typeToString(ElementData::SortieUniverselle)
			== QStringLiteral("UO"));
	}

	SECTION("ida e volta")
	{
		const ElementData::PlcIOType todos[] = {
			ElementData::EntreeDigitale,
			ElementData::SortieDigitale,
			ElementData::EntreeAnalogique,
			ElementData::SortieAnalogique,
			ElementData::EntreeUniverselle,
			ElementData::SortieUniverselle
		};
		for (const ElementData::PlcIOType tipo : todos)
		{
			bool ok = false;
			REQUIRE(IoPoint::typeFromString(IoPoint::typeToString(tipo), &ok)
				== tipo);
			REQUIRE(ok);
		}
	}
}

TEST_CASE("T11 — a coluna de tipo é lida do jeito que a pessoa escreveu",
	  "[io]")
{
	bool ok = false;

	SECTION("as letras em qualquer caixa")
	{
		REQUIRE(IoPoint::typeFromString(QStringLiteral("di"), &ok)
			== ElementData::EntreeDigitale);
		REQUIRE(ok);
		REQUIRE(IoPoint::typeFromString(QStringLiteral(" Do "), &ok)
			== ElementData::SortieDigitale);
		REQUIRE(ok);
	}

	SECTION("as letras como se escreve aqui, entrada e saída primeiro")
	{
		REQUIRE(IoPoint::typeFromString(QStringLiteral("ED"), &ok)
			== ElementData::EntreeDigitale);
		REQUIRE(IoPoint::typeFromString(QStringLiteral("SD"), &ok)
			== ElementData::SortieDigitale);
		REQUIRE(IoPoint::typeFromString(QStringLiteral("EA"), &ok)
			== ElementData::EntreeAnalogique);
		REQUIRE(IoPoint::typeFromString(QStringLiteral("SA"), &ok)
			== ElementData::SortieAnalogique);
		REQUIRE(IoPoint::typeFromString(QStringLiteral("EU"), &ok)
			== ElementData::EntreeUniverselle);
		REQUIRE(IoPoint::typeFromString(QStringLiteral("SU"), &ok)
			== ElementData::SortieUniverselle);
		REQUIRE(ok);
	}

	SECTION("por extenso, em três línguas e nas duas ordens")
	{
		REQUIRE(IoPoint::typeFromString(
				QString::fromUtf8("Entrada Digital"), &ok)
			== ElementData::EntreeDigitale);
		REQUIRE(ok);
		REQUIRE(IoPoint::typeFromString(
				QString::fromUtf8("Saída Analógica"), &ok)
			== ElementData::SortieAnalogique);
		REQUIRE(ok);
		REQUIRE(IoPoint::typeFromString(
				QString::fromUtf8("digital input"), &ok)
			== ElementData::EntreeDigitale);
		REQUIRE(ok);
		REQUIRE(IoPoint::typeFromString(
				QString::fromUtf8("Sortie analogique"), &ok)
			== ElementData::SortieAnalogique);
		REQUIRE(ok);
		REQUIRE(IoPoint::typeFromString(
				QString::fromUtf8("Entrada Universal"), &ok)
			== ElementData::EntreeUniverselle);
		REQUIRE(ok);
	}

	SECTION("o que a planilha não disse é dito por ok, não por um palpite")
	{
			//Meia informação é informação nenhuma: uma coluna que diz
			//apenas "Entrada" não diz se é digital ou analógica, e o
			//padrão devolvido tem de vir acompanhado do aviso.
		IoPoint::typeFromString(QString::fromUtf8("Entrada"), &ok);
		REQUIRE_FALSE(ok);
		IoPoint::typeFromString(QStringLiteral("analog"), &ok);
		REQUIRE_FALSE(ok);
		REQUIRE(IoPoint::typeFromString(QStringLiteral(""), &ok)
			== ElementData::EntreeDigitale);
		REQUIRE_FALSE(ok);
		IoPoint::typeFromString(QStringLiteral("xyz"), &ok);
		REQUIRE_FALSE(ok);
	}
}

TEST_CASE("T11 — a dobra de texto ignora caixa, acento e espaço duplo",
	  "[io]")
{
	REQUIRE(IoPoint::normalize(QString::fromUtf8("Botão de Emergência"))
		== IoPoint::normalize(QString::fromUtf8("botao  de emergencia")));
	REQUIRE(IoPoint::normalize(QString::fromUtf8("  Nível  Alto  "))
		== QStringLiteral("nivel alto"));
	REQUIRE(IoPoint::normalize(QStringLiteral("")).isEmpty());
}

TEST_CASE("T11 — o ponto de E/S escreve só o que tem", "[io]")
{
	QDomDocument documento;

	SECTION("campo vazio não vira atributo")
	{
		IoPoint ponto = basica("Botão de emergência");
		const QDomElement elemento = ponto.toXml(documento);

		REQUIRE(elemento.tagName() == IoPoint::tagName());
		REQUIRE(elemento.hasAttribute(QStringLiteral("description")));
		REQUIRE(elemento.hasAttribute(QStringLiteral("type")));
		REQUIRE_FALSE(elemento.hasAttribute(QStringLiteral("tag")));
		REQUIRE_FALSE(elemento.hasAttribute(QStringLiteral("address")));
		REQUIRE_FALSE(elemento.hasAttribute(QStringLiteral("terminal")));
		REQUIRE_FALSE(elemento.hasAttribute(QStringLiteral("master")));
	}

	SECTION("ida e volta do ponto inteiro")
	{
		IoPoint ponto = completa("S1", "I0.0", "Botão de emergência",
					 ElementData::EntreeDigitale);
		ponto.id = QStringLiteral("{um-id-qualquer}");
		ponto.card = QStringLiteral("CP1");
		ponto.connect_to = QStringLiteral("botao_emergencia");
		ponto.needs_terminal = true;
		ponto.comment = QString::fromUtf8("chega pela régua X1");
		ponto.master_uuid = QStringLiteral("{mestre}");
		ponto.io_index = 3;
		ponto.channel = QStringLiteral("I0.3");

		IoPoint lido;
		REQUIRE(lido.fromXml(ponto.toXml(documento)));
		REQUIRE(lido == ponto);
	}

	SECTION("a atribuição só viaja inteira")
	{
			//Um mestre sem índice voltaria como um ponto que acredita
			//estar numa carta e não sabe dizer onde.
		IoPoint ponto = completa("S1", "I0.0", "Botão");
		ponto.io_index = 7;
		ponto.channel = QStringLiteral("I0.7");
		REQUIRE_FALSE(ponto.isAssigned());

		const QDomElement elemento = ponto.toXml(documento);
		REQUIRE_FALSE(elemento.hasAttribute(QStringLiteral("io")));
		REQUIRE_FALSE(elemento.hasAttribute(QStringLiteral("channel")));

		IoPoint lido;
		REQUIRE(lido.fromXml(elemento));
		REQUIRE(lido.io_index == -1);
		REQUIRE(lido.channel.isEmpty());
	}

	SECTION("elemento de outra gente não é lido")
	{
		QDomElement estranho = documento.createElement(
					QStringLiteral("element"));
		IoPoint ponto;
		REQUIRE_FALSE(ponto.fromXml(estranho));
		REQUIRE_FALSE(ponto.fromXml(QDomElement()));
	}
}

TEST_CASE("T11 — a chave da importação é uma cascata", "[io]")
{
	IoList lista;
	lista.append(completa("S1", "I0.0", "Botão de emergência"));
	lista.append(completa("S2", "I0.1", "Sinaleiro verde"));

	SECTION("a etiqueta vem primeiro")
	{
		IoPoint entrando;
		entrando.tag = QStringLiteral("S2");
		entrando.description = QString::fromUtf8("outra coisa qualquer");
		REQUIRE(lista.indexOfKey(entrando) == 1);
	}

	SECTION("sem etiqueta, o endereço")
	{
		IoPoint entrando;
		entrando.address = QStringLiteral("I0.0");
		REQUIRE(lista.indexOfKey(entrando) == 0);
	}

	SECTION("sem etiqueta nem endereço, a descrição dobrada")
	{
		IoPoint entrando = basica("SINALEIRO  VERDE");
		REQUIRE(lista.indexOfKey(entrando) == 1);
	}

	SECTION("o que não é de ninguém não casa com ninguém")
	{
		REQUIRE(lista.indexOfKey(basica("Pressostato")) == -1);
		REQUIRE(lista.indexOfKey(IoPoint()) == -1);
	}
}

TEST_CASE("T11 — a planilha completa completa o que a básica importou",
	  "[io]")
{
	IoList lista;
	QList<IoPoint> primeira;
	primeira << basica("Botão de emergência")
		 << basica("Sinaleiro verde");
	lista.merge(primeira);
	REQUIRE(lista.count() == 2);

	const QString id_botao = lista.at(0).id;
	REQUIRE_FALSE(id_botao.isEmpty());

	QList<IoPoint> segunda;
	segunda << completa("S1", "I0.0", "Botão de emergência",
			    ElementData::EntreeDigitale)
		<< completa("H1", "Q0.0", "Sinaleiro verde",
			    ElementData::SortieDigitale);
	const IoList::MergeReport relatorio = lista.merge(segunda);

		//Reconheceu os dois pela descrição em vez de duplicá-los.
	REQUIRE(lista.count() == 2);
	REQUIRE(relatorio.added.isEmpty());
	REQUIRE(relatorio.updated.count() == 2);
	REQUIRE(relatorio.missing.isEmpty());

		//E o ponto continua sendo o mesmo ponto.
	REQUIRE(lista.at(0).id == id_botao);
	REQUIRE(lista.at(0).tag == QStringLiteral("S1"));
	REQUIRE(lista.at(0).address == QStringLiteral("I0.0"));
	REQUIRE(lista.at(1).type == ElementData::SortieDigitale);

		//Importar a mesma planilha de novo não muda mais nada.
	const IoList::MergeReport terceira = lista.merge(segunda);
	REQUIRE(terceira.added.isEmpty());
	REQUIRE(terceira.updated.isEmpty());
	REQUIRE(terceira.unchanged.count() == 2);
	REQUIRE(lista.count() == 2);
}

TEST_CASE("T11 — chave que casa com dois pontos não casa com nenhum",
	  "[io]")
{
	SECTION("dois pontos guardados com a mesma descrição")
	{
		IoList lista;
		lista.append(completa("S1", "I0.0", "Pressostato"));
		lista.append(completa("S2", "I0.1", "Pressostato"));

		bool ambiguo = false;
		REQUIRE(lista.indexOfKey(basica("Pressostato"), &ambiguo) == -1);
		REQUIRE(ambiguo);

		QList<IoPoint> entrando;
		entrando << basica("Pressostato");
		const IoList::MergeReport relatorio = lista.merge(entrando);

			//Entrou como novo, e o relatório diz por quê - o que a
			//importação não faz é escolher um dos dois no escuro.
		REQUIRE(lista.count() == 3);
		REQUIRE(relatorio.added.count() == 1);
		REQUIRE(relatorio.ambiguous == relatorio.added);
	}

	SECTION("duas linhas da mesma planilha para um ponto só")
	{
		IoList lista;
		lista.append(completa("S1", "I0.0", "Pressostato"));

		QList<IoPoint> entrando;
		entrando << completa("S1", "I0.0", "Pressostato de linha")
			 << completa("S1", "I0.2", "Pressostato de retorno");
		const IoList::MergeReport relatorio = lista.merge(entrando);

		REQUIRE(lista.count() == 2);
		REQUIRE(relatorio.updated.count() == 1);
		REQUIRE(relatorio.added.count() == 1);
		REQUIRE(relatorio.ambiguous == relatorio.added);
	}
}

TEST_CASE("T11 — a importação nunca apaga, ela relata", "[io]")
{
	IoList lista;
	const QString id_a = lista.append(completa("S1", "I0.0", "Botão"));
	const QString id_b = lista.append(completa("S2", "I0.1", "Sinaleiro"));
	const QString id_c = lista.append(completa("S3", "I0.2", "Pressostato"));

	QList<IoPoint> revisada;
	revisada << completa("S2", "I0.1", "Sinaleiro verde");
	const IoList::MergeReport relatorio = lista.merge(revisada);

	REQUIRE(lista.count() == 3);
	REQUIRE(relatorio.updated == QStringList{id_b});
	REQUIRE(relatorio.missing.count() == 2);
	REQUIRE(relatorio.missing.contains(id_a));
	REQUIRE(relatorio.missing.contains(id_c));
	REQUIRE(lista.indexOfId(id_a) == 0);
	REQUIRE(lista.indexOfId(id_c) == 2);
	REQUIRE_FALSE(relatorio.isEmpty());
}

TEST_CASE("T11 — o desenho não é assunto da planilha", "[io]")
{
	IoList lista;
	const QString id = lista.append(completa("S1", "I0.0", "Botão"));

	IoPoint atribuido = lista.at(0);
	atribuido.master_uuid = QStringLiteral("{cartao-de-entrada}");
	atribuido.io_index = 4;
	atribuido.channel = QStringLiteral("I0.4");
	REQUIRE(lista.setPoint(0, atribuido));
	REQUIRE(lista.at(0).isAssigned());

	SECTION("a atribuição sobrevive a uma reimportação")
	{
		QList<IoPoint> revisada;
		IoPoint linha = completa("S1", "I0.0", "Botão de emergência");
			//Mesmo que a planilha tenha uma coluna assim, ela não
			//manda no desenho.
		linha.master_uuid = QStringLiteral("{outro-cartao}");
		linha.io_index = 9;
		linha.channel = QStringLiteral("I0.9");
		revisada << linha;
		lista.merge(revisada);

		REQUIRE(lista.at(0).master_uuid
			== QStringLiteral("{cartao-de-entrada}"));
		REQUIRE(lista.at(0).io_index == 4);
		REQUIRE(lista.at(0).channel == QStringLiteral("I0.4"));
		REQUIRE(lista.at(0).description
			== QString::fromUtf8("Botão de emergência"));
	}

	SECTION("o ponto novo entra sempre solto")
	{
		QList<IoPoint> outra;
		IoPoint linha = completa("S9", "I1.0", "Chave seletora");
		linha.master_uuid = QStringLiteral("{cartao-inventado}");
		linha.io_index = 2;
		outra << linha;
		lista.merge(outra);

		REQUIRE(lista.count() == 2);
		REQUIRE_FALSE(lista.at(1).isAssigned());
		REQUIRE(lista.at(1).master_uuid.isEmpty());
		REQUIRE(lista.at(1).io_index == -1);
	}

	SECTION("a identidade não é a chave da planilha")
	{
			//Corrigir a etiqueta não faz o ponto virar outro ponto.
		IoPoint corrigido = lista.at(0);
		corrigido.id = QStringLiteral("{id-que-o-chamador-inventou}");
		corrigido.tag = QStringLiteral("S1A");
		REQUIRE(lista.setPoint(0, corrigido));
		REQUIRE(lista.at(0).id == id);
		REQUIRE(lista.at(0).tag == QStringLiteral("S1A"));
	}

	SECTION("unassigned é a lista de trabalho de quem atribui")
	{
		lista.append(completa("S2", "I0.1", "Sinaleiro"));
		lista.append(completa("S3", "I0.2", "Pressostato"));
		const QList<int> soltos = lista.unassigned();
		REQUIRE(soltos.count() == 2);
		REQUIRE(soltos.at(0) == 1);
		REQUIRE(soltos.at(1) == 2);
	}
}

TEST_CASE("T11 — a coluna vazia não apaga o que está preenchido", "[io]")
{
	IoList lista;
	lista.append(completa("S1", "I0.0", "Botão de emergência"));

	SECTION("valor vazio é silêncio, não é uma ordem de apagar")
	{
		QList<IoPoint> meia;
		IoPoint linha;
		linha.tag = QStringLiteral("S1");
		meia << linha;
		const IoList::MergeReport relatorio = lista.merge(meia);

		REQUIRE(lista.at(0).address == QStringLiteral("I0.0"));
		REQUIRE(lista.at(0).description
			== QString::fromUtf8("Botão de emergência"));
		REQUIRE(relatorio.unchanged.count() == 1);
	}

	SECTION("o campo fora do conjunto não é escrito")
	{
		QList<IoPoint> outra;
		outra << completa("S1", "I9.9", "Outra descrição",
				  ElementData::SortieAnalogique);
		lista.merge(outra, IoTagField | IoAddressField);

		REQUIRE(lista.at(0).address == QStringLiteral("I9.9"));
		REQUIRE(lista.at(0).description
			== QString::fromUtf8("Botão de emergência"));
		REQUIRE(lista.at(0).type == ElementData::EntreeDigitale);
	}
}

TEST_CASE("T11 — o bloco io_list vai e volta pelo arquivo do projeto",
	  "[io]")
{
	QDomDocument documento;

	SECTION("lista vazia não escreve ponto nenhum")
	{
		IoList vazia;
		REQUIRE(vazia.isEmpty());
		const QDomElement elemento = vazia.toXml(documento);
		REQUIRE(elemento.tagName() == QStringLiteral("io_list"));
		REQUIRE(elemento.firstChildElement().isNull());
	}

	SECTION("ida e volta com a ordem preservada")
	{
		IoList lista;
		lista.append(completa("S1", "I0.0", "Botão de emergência"));
		lista.append(completa("H1", "Q0.0", "Sinaleiro verde",
				      ElementData::SortieDigitale));
		lista.append(completa("B1", "IW64", "Pressão da linha",
				      ElementData::EntreeAnalogique));

		IoList lida;
		REQUIRE(lida.fromXml(lista.toXml(documento)));
		REQUIRE(lida.count() == 3);
		REQUIRE(lida == lista);
		REQUIRE(lida.at(2).type == ElementData::EntreeAnalogique);
	}

	SECTION("a leitura é tolerante")
	{
		IoList lista;
		REQUIRE_FALSE(lista.fromXml(QDomElement()));
		REQUIRE_FALSE(lista.fromXml(documento.createElement(
						    QStringLiteral("project"))));
		REQUIRE(lista.isEmpty());
	}

	SECTION("ponto sem identidade ganha uma ao ser lido")
	{
		QDomElement raiz = documento.createElement(
					QStringLiteral("io_list"));
		QDomElement ponto = documento.createElement(
					QStringLiteral("io_point"));
		ponto.setAttribute(QStringLiteral("type"), QStringLiteral("DO"));
		ponto.setAttribute(QStringLiteral("tag"), QStringLiteral("H1"));
		raiz.appendChild(ponto);

		IoList lista;
		REQUIRE(lista.fromXml(raiz));
		REQUIRE(lista.count() == 1);
		REQUIRE_FALSE(lista.at(0).id.isEmpty());
		REQUIRE(lista.at(0).type == ElementData::SortieDigitale);
	}
}
