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
#include "../../../sources/macro/circuitclipboard.h"
#include "../../../sources/plc/iolist.h"
#include "../../../sources/plc/iopoint.h"
#include "../../../sources/plc/iosheet.h"
#include "qt_catch_tostring.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace
{
		/// @return one row of cells, from what the sheet holds
	QStringList linha(const char *a,
			  const char *b = nullptr,
			  const char *c = nullptr,
			  const char *d = nullptr,
			  const char *e = nullptr,
			  const char *f = nullptr,
			  const char *g = nullptr,
			  const char *h = nullptr)
	{
		const char *cells[] = {a, b, c, d, e, f, g, h};

		QStringList row;
		for (const char *cell : cells)
		{
			if (!cell) {
				break;
			}
			row << QString::fromUtf8(cell);
		}
		return row;
	}

		/**
			The sheet of CU-11.1: sixty rows, type and description, and not
			one line of configuration.
		*/
	QList<QStringList> planilhaDeSessenta()
	{
		QList<QStringList> grid;
		for (int i = 1 ; i <= 60 ; ++i)
		{
			grid << (QStringList()
				 << QStringLiteral("DI")
				 << QStringLiteral("Ponto %1").arg(i));
		}
		return grid;
	}
}

TEST_CASE("CU-11.1 — a planilha de duas colunas entra sem configuração",
	  "[io][iosheet]")
{
	SECTION("tipo na primeira coluna, descrição na segunda")
	{
		QList<QStringList> grade;
		grade << linha("DI", "Botão de emergência");
		grade << linha("DO", "Contator do motor");
		grade << linha("AI", "Pressão da linha");

		const IoSheet::Mapping mapa = IoSheet::mappingFor(grade);
		REQUIRE_FALSE(mapa.has_header);
		REQUIRE(mapa.columnOf(IoTypeField) == 0);
		REQUIRE(mapa.columnOf(IoDescriptionField) == 1);
		REQUIRE(mapa.columnOf(IoAddressField) == -1);

		const IoSheet::Report leitura = IoSheet::read(grade, mapa);
		REQUIRE(leitura.points.count() == 3);
		REQUIRE(leitura.isClean());

		REQUIRE(leitura.points.at(0).type == ElementData::EntreeDigitale);
		REQUIRE(leitura.points.at(0).description
			== QString::fromUtf8("Botão de emergência"));
		REQUIRE(leitura.points.at(1).type == ElementData::SortieDigitale);
		REQUIRE(leitura.points.at(2).type
			== ElementData::EntreeAnalogique);
	}

	SECTION("sessenta linhas dão sessenta pontos")
	{
		const QList<QStringList> grade = planilhaDeSessenta();
		const IoSheet::Mapping mapa = IoSheet::mappingFor(grade);
		const IoSheet::Report leitura = IoSheet::read(grade, mapa);

		REQUIRE(leitura.points.count() == 60);
		REQUIRE(leitura.isClean());
		REQUIRE(leitura.points.first().description
			== QStringLiteral("Ponto 1"));
		REQUIRE(leitura.points.last().description
			== QStringLiteral("Ponto 60"));
	}

	SECTION("a planilha por extenso e em três idiomas é a mesma planilha")
	{
		QList<QStringList> grade;
		grade << linha("Entrada Digital", "Botão de emergência");
		grade << linha("Sortie analogique", "Consigne de vitesse");
		grade << linha("digital output", "Sinaleiro de marcha");

		const IoSheet::Report leitura =
			IoSheet::read(grade, IoSheet::mappingFor(grade));

		REQUIRE(leitura.points.count() == 3);
		REQUIRE(leitura.isClean());
		REQUIRE(leitura.points.at(0).type == ElementData::EntreeDigitale);
		REQUIRE(leitura.points.at(1).type
			== ElementData::SortieAnalogique);
		REQUIRE(leitura.points.at(2).type == ElementData::SortieDigitale);
	}
}

TEST_CASE("CU-11.2 — linha em branco no meio não trunca a leitura",
	  "[io][iosheet]")
{
	QList<QStringList> grade = planilhaDeSessenta();
	grade.insert(24, linha("", ""));
	REQUIRE(grade.count() == 61);

	const IoSheet::Report leitura =
		IoSheet::read(grade, IoSheet::mappingFor(grade));

		//Vinte e quatro pontos e um relatório de sucesso é exatamente o
		//resultado que a tarefa proíbe.
	REQUIRE(leitura.points.count() == 60);
	REQUIRE(leitura.points.last().description
		== QStringLiteral("Ponto 60"));

	REQUIRE_FALSE(leitura.isClean());
	REQUIRE(leitura.blank_rows.count() == 1);
	REQUIRE(leitura.blank_rows.first() == 25);
	REQUIRE(leitura.text().contains(QStringLiteral("25")));

	SECTION("a linha vazia de verdade, sem célula nenhuma, também conta")
	{
		QList<QStringList> vazia = planilhaDeSessenta();
		vazia.insert(9, QStringList());

		const IoSheet::Report outra =
			IoSheet::read(vazia, IoSheet::mappingFor(vazia));

		REQUIRE(outra.points.count() == 60);
		REQUIRE(outra.blank_rows.count() == 1);
		REQUIRE(outra.blank_rows.first() == 10);
	}
}

TEST_CASE("O cabeçalho é reconhecido nos três idiomas", "[io][iosheet]")
{
	SECTION("português")
	{
		const IoSheet::Mapping mapa = IoSheet::guess(
			linha("Tipo", "Tag", "Descrição", "Endereço",
			      "Cartão", "Ligar a", "Exige borne",
			      "Comentário"));

		REQUIRE(mapa.has_header);
		REQUIRE(mapa.columnOf(IoTypeField) == 0);
		REQUIRE(mapa.columnOf(IoTagField) == 1);
		REQUIRE(mapa.columnOf(IoDescriptionField) == 2);
		REQUIRE(mapa.columnOf(IoAddressField) == 3);
		REQUIRE(mapa.columnOf(IoCardField) == 4);
		REQUIRE(mapa.columnOf(IoConnectField) == 5);
		REQUIRE(mapa.columnOf(IoTerminalField) == 6);
		REQUIRE(mapa.columnOf(IoCommentField) == 7);
	}

	SECTION("francês — os mesmos nomes que o próprio diálogo escreve")
	{
		QStringList cabecalho;
		const QList<IoField> campos = IoSheet::mappableFields();
		for (const IoField campo : campos) {
			cabecalho << IoSheet::fieldName(campo);
		}

		const IoSheet::Mapping mapa = IoSheet::guess(cabecalho);
		REQUIRE(mapa.has_header);
		for (int i = 0 ; i < campos.count() ; ++i) {
			REQUIRE(mapa.columnOf(campos.at(i)) == i);
		}
	}

	SECTION("inglês")
	{
		const IoSheet::Mapping mapa = IoSheet::guess(
			linha("Type", "Name", "Description", "Address",
			      "Card", "Comment"));

		REQUIRE(mapa.has_header);
		REQUIRE(mapa.columnOf(IoTypeField) == 0);
		REQUIRE(mapa.columnOf(IoTagField) == 1);
		REQUIRE(mapa.columnOf(IoCommentField) == 5);
		REQUIRE(mapa.columnOf(IoTerminalField) == -1);
	}

	SECTION("acento e caixa não decidem nada")
	{
		const IoSheet::Mapping mapa =
			IoSheet::guess(linha("  TIPO  ", "DESCRICAO"));

		REQUIRE(mapa.has_header);
		REQUIRE(mapa.columnOf(IoTypeField) == 0);
		REQUIRE(mapa.columnOf(IoDescriptionField) == 1);
	}

	SECTION("uma coluna reconhecida sozinha não é um cabeçalho")
	{
		QList<QStringList> grade;
		grade << linha("Tipo", "Botão de emergência");
		grade << linha("DO", "Contator do motor");

		const IoSheet::Mapping adivinhado = IoSheet::guess(grade.first());
		REQUIRE_FALSE(adivinhado.has_header);

			//E como não é cabeçalho, a primeira linha é dado: os dois
			//pontos entram, e o primeiro deles tem o tipo que não foi
			//entendido relatado em vez de virar entrada digital calada.
		const IoSheet::Report leitura =
			IoSheet::read(grade, IoSheet::mappingFor(grade));
		REQUIRE(leitura.points.count() == 2);
		REQUIRE(leitura.unknown_type_rows.count() == 1);
		REQUIRE(leitura.unknown_type_rows.first() == 1);
	}
}

TEST_CASE("O mapeamento diz quais campos a planilha carrega", "[io][iosheet]")
{
	SECTION("a planilha básica carrega dois campos e só")
	{
		const IoSheet::Mapping mapa = IoSheet::basic();

		REQUIRE_FALSE(mapa.isEmpty());
		REQUIRE(mapa.fields().testFlag(IoTypeField));
		REQUIRE(mapa.fields().testFlag(IoDescriptionField));
		REQUIRE_FALSE(mapa.fields().testFlag(IoAddressField));
		REQUIRE_FALSE(mapa.fields().testFlag(IoCardField));
	}

	SECTION("uma coluna alimenta um campo só")
	{
		IoSheet::Mapping mapa;
		mapa.setColumn(IoTagField, 2);
		mapa.setColumn(IoDescriptionField, 2);

		REQUIRE(mapa.columnOf(IoTagField) == -1);
		REQUIRE(mapa.columnOf(IoDescriptionField) == 2);
		REQUIRE(mapa.mappedFields().count() == 1);
	}

	SECTION("desmapear é apontar para coluna nenhuma")
	{
		IoSheet::Mapping mapa = IoSheet::basic();
		mapa.setColumn(IoTypeField, -1);

		REQUIRE(mapa.columnOf(IoTypeField) == -1);
		REQUIRE_FALSE(mapa.fields().testFlag(IoTypeField));

		mapa.unsetField(IoDescriptionField);
		REQUIRE(mapa.isEmpty());
	}

	SECTION("os campos saem na ordem da planilha, não na ordem do enum")
	{
		IoSheet::Mapping mapa;
		mapa.setColumn(IoDescriptionField, 0);
		mapa.setColumn(IoTypeField, 1);

		const QList<IoField> ordem = mapa.mappedFields();
		REQUIRE(ordem.count() == 2);
		REQUIRE(ordem.first() == IoDescriptionField);
		REQUIRE(ordem.last() == IoTypeField);
	}
}

TEST_CASE("A planilha estreita não apaga o que a larga preencheu",
	  "[io][iosheet]")
{
	IoList lista;

	IoPoint larga;
	larga.tag = QStringLiteral("S1");
	larga.description = QString::fromUtf8("Botão de emergência");
	larga.address = QStringLiteral("I0.0");
	larga.card = QStringLiteral("DI16");
	larga.type = ElementData::EntreeDigitale;
	lista.append(larga);

	SECTION("a coluna que a planilha não tem não é escrita")
	{
		QList<QStringList> grade;
		grade << linha("DO", "Botão de emergência");

		const IoSheet::Mapping mapa = IoSheet::mappingFor(grade);
		const IoSheet::Report leitura = IoSheet::read(grade, mapa);
		const IoList::MergeReport fusao =
			lista.merge(leitura.points, mapa.fields());

		REQUIRE(fusao.added.isEmpty());
		REQUIRE(fusao.updated.count() == 1);

			//O tipo, que a planilha traz, mudou. O endereço e o cartão,
			//que ela não traz, continuam lá.
		REQUIRE(lista.at(0).type == ElementData::SortieDigitale);
		REQUIRE(lista.at(0).address == QStringLiteral("I0.0"));
		REQUIRE(lista.at(0).card == QStringLiteral("DI16"));
		REQUIRE(lista.at(0).tag == QStringLiteral("S1"));
	}

	SECTION("a coluna que a planilha tem mas deixou vazia também não apaga")
	{
		QList<QStringList> grade;
		grade << linha("Tipo", "Tag", "Endereço", "Cartão");
		grade << linha("DI", "S1", "", "");

		const IoSheet::Mapping mapa = IoSheet::mappingFor(grade);
		REQUIRE(mapa.has_header);
		REQUIRE(mapa.fields().testFlag(IoAddressField));

		const IoSheet::Report leitura = IoSheet::read(grade, mapa);
		REQUIRE(leitura.points.count() == 1);

		lista.merge(leitura.points, mapa.fields());
		REQUIRE(lista.at(0).address == QStringLiteral("I0.0"));
		REQUIRE(lista.at(0).card == QStringLiteral("DI16"));
	}
}

TEST_CASE("A leitura relata o que não entendeu em vez de calar",
	  "[io][iosheet]")
{
	SECTION("tipo que ninguém reconhece é relatado pela linha")
	{
		QList<QStringList> grade;
		grade << linha("DI", "Botão de emergência");
		grade << linha("XPTO", "Contator do motor");
		grade << linha("AO", "Consigna de velocidade");

		const IoSheet::Report leitura =
			IoSheet::read(grade, IoSheet::mappingFor(grade));

			//A linha entra: ela tem descrição, e jogá-la fora seria pior
			//do que dizer que o tipo dela precisa de uma olhada.
		REQUIRE(leitura.points.count() == 3);
		REQUIRE(leitura.unknown_type_rows.count() == 1);
		REQUIRE(leitura.unknown_type_rows.first() == 2);
		REQUIRE(leitura.points.at(1).type == ElementData::EntreeDigitale);
		REQUIRE(leitura.text().contains(QStringLiteral("2")));
	}

	SECTION("linha que não diz de que ponto está falando é relatada")
	{
		QList<QStringList> grade;
		grade << linha("Tipo", "Descrição", "Comentário");
		grade << linha("DI", "Botão de emergência", "conferir");
		grade << linha("", "", "faltou preencher esta");

		const IoSheet::Report leitura =
			IoSheet::read(grade, IoSheet::mappingFor(grade));

		REQUIRE(leitura.points.count() == 1);
		REQUIRE(leitura.blank_rows.isEmpty());
		REQUIRE(leitura.nameless_rows.count() == 1);
		REQUIRE(leitura.nameless_rows.first() == 3);
		REQUIRE_FALSE(leitura.isClean());
	}

	SECTION("a leitura limpa não tem nada a relatar além da conta")
	{
		QList<QStringList> grade;
		grade << linha("DI", "Botão de emergência");

		const IoSheet::Report leitura =
			IoSheet::read(grade, IoSheet::mappingFor(grade));

		REQUIRE(leitura.isClean());
		REQUIRE_FALSE(leitura.isEmpty());
		REQUIRE(leitura.text().contains(QStringLiteral("1")));
	}

	SECTION("grade vazia não é leitura nenhuma")
	{
		const QList<QStringList> grade;
		const IoSheet::Report leitura =
			IoSheet::read(grade, IoSheet::mappingFor(grade));

		REQUIRE(leitura.isEmpty());
		REQUIRE(leitura.isClean());
	}
}

TEST_CASE("A coluna de sim ou não só diz sim quando diz sim", "[io][iosheet]")
{
	SECTION("o que uma pessoa escreve para dizer sim")
	{
		const char *sins[] = {"sim", "SIM", " Sim ", "s", "x", "X",
				      "yes", "y", "oui", "o", "true", "vrai",
				      "verdadeiro", "1"};
		for (const char *cell : sins) {
			REQUIRE(IoSheet::isYes(QString::fromUtf8(cell)));
		}
	}

	SECTION("todo o resto é não, inclusive a célula vazia")
	{
		const char *naos[] = {"", " ", "não", "nao", "n", "no", "non",
				      "0", "-", "talvez", "false", "faux"};
		for (const char *cell : naos) {
			REQUIRE_FALSE(IoSheet::isYes(QString::fromUtf8(cell)));
		}
	}

	SECTION("a coluna que a planilha não tem deixa o ponto sem borne")
	{
		QList<QStringList> grade;
		grade << linha("Tipo", "Descrição", "Exige borne");
		grade << linha("DI", "Botão de emergência", "sim");
		grade << linha("DO", "Contator do motor", "");

		const IoSheet::Report com =
			IoSheet::read(grade, IoSheet::mappingFor(grade));
		REQUIRE(com.points.at(0).needs_terminal);
		REQUIRE_FALSE(com.points.at(1).needs_terminal);

		IoSheet::Mapping sem = IoSheet::mappingFor(grade);
		sem.unsetField(IoTerminalField);
		const IoSheet::Report leitura = IoSheet::read(grade, sem);
		REQUIRE_FALSE(leitura.points.at(0).needs_terminal);
	}
}

TEST_CASE("O texto colado da planilha vira pontos", "[io][iosheet]")
{
	const QString colado =
		QString::fromUtf8(
			"Tipo\tTag\tDescrição\tEndereço\tCartão\tLigar a\t"
			"Exige borne\tComentário\n"
			"DI\tS1\tBotão de emergência\tI0.0\tDI16\tpartida\t"
			"sim\tcontato NF\n"
			"DO\tK1\tContator do motor\tQ0.0\tDO16\tpartida\t"
			"não\t\n"
			"AI\tPT1\tPressão da linha\tIW64\tAI4\t\tsim\t"
			"4 a 20 mA\n");

	const QList<QStringList> grade = CircuitClipboard::parse(colado);
	REQUIRE(grade.count() == 4);

	const IoSheet::Mapping mapa = IoSheet::mappingFor(grade);
	REQUIRE(mapa.has_header);
	REQUIRE(static_cast<int>(mapa.fields()) == static_cast<int>(AllIoFields));

	const IoSheet::Report leitura = IoSheet::read(grade, mapa);
	REQUIRE(leitura.points.count() == 3);
	REQUIRE(leitura.isClean());

	const IoPoint primeiro = leitura.points.first();
	REQUIRE(primeiro.type == ElementData::EntreeDigitale);
	REQUIRE(primeiro.tag == QStringLiteral("S1"));
	REQUIRE(primeiro.description == QString::fromUtf8("Botão de emergência"));
	REQUIRE(primeiro.address == QStringLiteral("I0.0"));
	REQUIRE(primeiro.card == QStringLiteral("DI16"));
	REQUIRE(primeiro.connect_to == QStringLiteral("partida"));
	REQUIRE(primeiro.needs_terminal);
	REQUIRE(primeiro.comment == QStringLiteral("contato NF"));

	REQUIRE_FALSE(leitura.points.at(1).needs_terminal);
	REQUIRE(leitura.points.at(2).connect_to.isEmpty());

	SECTION("e a lista aceita os três de uma vez")
	{
		IoList lista;
		const IoList::MergeReport fusao =
			lista.merge(leitura.points, mapa.fields());

		REQUIRE(fusao.added.count() == 3);
		REQUIRE(fusao.updated.isEmpty());
		REQUIRE(fusao.missing.isEmpty());
		REQUIRE(fusao.ambiguous.isEmpty());
		REQUIRE(lista.count() == 3);

		SECTION("reimportar a mesma planilha não muda nada")
		{
			const IoList::MergeReport outra =
				lista.merge(leitura.points, mapa.fields());

			REQUIRE(outra.added.isEmpty());
			REQUIRE(outra.updated.isEmpty());
			REQUIRE(outra.unchanged.count() == 3);
			REQUIRE(lista.count() == 3);
		}
	}
}
