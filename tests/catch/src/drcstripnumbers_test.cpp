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
#include "../../../sources/drc/drcstripnumbers.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QString>
#include <QStringList>

/*
	When two terminals of one strip carry the same number, what a strip is
	called in a sentence, and the sentences of the strip rules.

	Here, in the pure suite, because none of it needs a strip: it is handed
	numbers, places on the rail, folios, cells and the fields of a strip's
	name, and answers from that. Walking the strips of a project, reading
	the number the plan draws, and leaving out what is not on a folio is
	the project's half, proved in drcstriprules_test.cpp of C_uitests.

	Four things are settled:

	1. what makes two numbers the same - trimmed and case folded, and an
	   empty number is no number;
	2. one group per number, and the levels of one terminal are one
	   terminal: two levels numbered 5 are not a repetition, and a terminal
	   counts once in a group whatever number of its levels carry it;
	3. what a strip is called, and when two strips have the same name;
	4. the sentences, compared whole, in the source language. No translator
	   is installed in this suite, and the sentence is part of the rule.
*/

namespace
{
	DrcStripNumbers::Mark mark(const char *number,
				   int physical,
				   int folio = 0,
				   const char *cell = "")
	{
		DrcStripNumbers::Mark built;
		built.number = QString::fromUtf8(number);
		built.physical = physical;
		built.folio = folio;
		built.cell = QString::fromUtf8(cell);
		return built;
	}

	QString keyOf(const char *installation,
		      const char *location,
		      const char *name)
	{
		return DrcStripNumbers::stripKey(QString::fromUtf8(installation),
						 QString::fromUtf8(location),
						 QString::fromUtf8(name));
	}
}

TEST_CASE("T24 — o mesmo número de borne é o mesmo aparado e sem distinção de caixa, e número vazio não é número",
	  "[drc][t24]")
{
	SECTION("espaço nas pontas e caixa não separam dois números")
	{
		CHECK(DrcStripNumbers::key(QStringLiteral("3"))
		      == DrcStripNumbers::key(QStringLiteral(" 3 ")));
		CHECK(DrcStripNumbers::key(QStringLiteral("PE"))
		      == DrcStripNumbers::key(QStringLiteral("pe")));
	}

	SECTION("espaço no meio e zero à esquerda separam: é o que alguém escreveu")
	{
		CHECK(DrcStripNumbers::key(QStringLiteral("03"))
		      != DrcStripNumbers::key(QStringLiteral("3")));
		CHECK(DrcStripNumbers::key(QStringLiteral("3 a"))
		      != DrcStripNumbers::key(QStringLiteral("3a")));
	}

	SECTION("vazio e só espaço não têm chave")
	{
		CHECK(DrcStripNumbers::key(QString()).isEmpty());
		CHECK(DrcStripNumbers::key(QStringLiteral("   ")).isEmpty());
		CHECK(DrcStripNumbers::key(QString(QChar(0x00A0))).isEmpty());
	}
}

TEST_CASE("T24 — um grupo por número repetido na régua, e os níveis de um borne são um borne só",
	  "[drc][t24]")
{
	SECTION("1, 2, 3, 3: o 3 repetido é um grupo, com os dois bornes")
	{
		const QList<QList<int>> found = DrcStripNumbers::groups(
					{mark("1", 0), mark("2", 1), mark("3", 2),
					 mark("3", 3)});
		CHECK(found == QList<QList<int>>({{2, 3}}));
	}

	SECTION("e cala: um borne de dois níveis com 5 e 5 é um borne só")
	{
		CHECK(DrcStripNumbers::groups({mark("5", 0), mark("5", 0)})
		      .isEmpty());
	}

	SECTION("um nível de um borne contra outro borne conta, e o borne entra uma vez")
	{
			//Terminal 0 carries 3 on both levels and 4 on a third; terminal
			//1 carries 3. The 3 is on two terminals, and terminal 0 is
			//named by its first level carrying it.
		const QList<QList<int>> found = DrcStripNumbers::groups(
					{mark("3", 0), mark("3", 0), mark("4", 0),
					 mark("3", 1)});
		CHECK(found == QList<QList<int>>({{0, 3}}));
	}

	SECTION("três iguais, escritos de três jeitos, são um grupo de três")
	{
		const QList<QList<int>> found = DrcStripNumbers::groups(
					{mark("PE", 0), mark("pe", 1), mark(" PE", 2)});
		CHECK(found == QList<QList<int>>({{0, 1, 2}}));
	}

	SECTION("dois números repetidos são dois grupos, na ordem do trilho")
	{
		const QList<QList<int>> found = DrcStripNumbers::groups(
					{mark("9", 0), mark("8", 1), mark("8", 2),
					 mark("9", 3)});
		CHECK(found == QList<QList<int>>({{0, 3}, {1, 2}}));
	}

	SECTION("e cala: bornes sem número não compartilham número nenhum")
	{
		CHECK(DrcStripNumbers::groups(
			      {mark("", 0), mark("   ", 1), mark("", 2), mark("3", 3)})
		      .isEmpty());
	}

	SECTION("e cala: todos diferentes")
	{
		CHECK(DrcStripNumbers::groups(
			      {mark("3", 0), mark("03", 1), mark("3 a", 2),
			       mark("4", 3)})
		      .isEmpty());
	}
}

TEST_CASE("T24 — o nome de uma régua numa frase, e quando duas réguas têm o mesmo nome",
	  "[drc][t24]")
{
	SECTION("as três partes, como o cabeçalho do plano as junta")
	{
		CHECK(DrcStripNumbers::stripName(QStringLiteral("="),
						 QStringLiteral("+"),
						 QStringLiteral("X1"))
		      == QStringLiteral("= + X1"));
		CHECK(DrcStripNumbers::stripName(QStringLiteral(" =CT1 "),
						 QStringLiteral("+A1"),
						 QStringLiteral(" X9"))
		      == QStringLiteral("=CT1 +A1 X9"));
	}

	SECTION("instalação e localização vazias ficam de fora; nome vazio é dito")
	{
		CHECK(DrcStripNumbers::stripName(QString(), QStringLiteral("  "),
						 QStringLiteral("X1"))
		      == QStringLiteral("X1"));
		CHECK(DrcStripNumbers::stripName(QStringLiteral("="),
						 QStringLiteral("+"),
						 QString())
		      == QStringLiteral("= + (no name)"));
	}

	SECTION("o mesmo nome é o mesmo aparado e sem distinção de caixa, campo a campo")
	{
		CHECK(keyOf("=", "+", "X1") == keyOf(" = ", "+", "x1 "));
		CHECK(keyOf("=", "+", "X1") != keyOf("=", "+A1", "X1"));
			//Joined by a space these two would read alike; they are two
			//strips filled in differently.
		CHECK(keyOf("=A", "+B", "X1") != keyOf("=A +B", "", "X1"));
	}

	SECTION("régua sem nome próprio não tem chave")
	{
		CHECK(keyOf("=", "+", "").isEmpty());
		CHECK(keyOf("=", "+", "   ").isEmpty());
	}

	SECTION("os grupos de nome repetido, na ordem do projeto, sem as réguas sem nome")
	{
		const QStringList keys {keyOf("=", "+", "X1"),
					keyOf("=", "+", "X2"),
					keyOf("=", "+", ""),
					keyOf("=", "+", "x1"),
					keyOf("=", "+", "")};
		CHECK(DrcStripNumbers::sameNameGroups(keys)
		      == QList<QList<int>>({{0, 3}}));
	}
}

TEST_CASE("T24 — a frase diz o número, a régua, e a posição, a folha e a casa de cada borne",
	  "[drc][t24]")
{
	const QString x1 = QStringLiteral("= + X1");

		//The marks hold PhysicalTerminal::pos(), counted from 0, and the
		//sentence says each place counted from 1, as the Position column
		//of the strip editor does: the mark at 2 is "position 3".
	SECTION("dois bornes, em duas folhas")
	{
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("3", 2, 1, "B3"), mark("3", 3, 2, "C2")})
		      == QStringLiteral(
			      "Number 3 is on 2 terminals of strip = + X1: "
			      "position 3 (folio 1, B3); position 4 (folio 2, C2)."));
	}

	SECTION("três iguais: uma frase só, com os três lugares")
	{
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("7", 0, 1, "A1"), mark("7", 4, 1, "A5"),
				   mark("7", 9, 3, "D2")})
		      == QStringLiteral(
			      "Number 7 is on 3 terminals of strip = + X1: "
			      "position 1 (folio 1, A1); position 5 (folio 1, A5); "
			      "position 10 (folio 3, D2)."));
	}

	SECTION("grafias diferentes: cada lugar diz qual está lá")
	{
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("PE", 0, 1, "A1"), mark("pe", 4, 1, "A5")})
		      == QStringLiteral(
			      "Number PE is on 2 terminals of strip = + X1: "
			      "PE at position 1 (folio 1, A1); "
			      "pe at position 5 (folio 1, A5)."));
	}

	SECTION("o que falta do lugar não vira texto vazio")
	{
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("3", 2, 1), mark("3", 3, 0, "C2")})
		      == QStringLiteral(
			      "Number 3 is on 2 terminals of strip = + X1: "
			      "position 3 (folio 1); position 4 (C2)."));
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("3", 2), mark("3", -1, 2, "C2")})
		      == QStringLiteral(
			      "Number 3 is on 2 terminals of strip = + X1: "
			      "position 3; folio 2, C2."));
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("3", 2), mark("3", -1)})
		      == QStringLiteral(
			      "Number 3 is on 2 terminals of strip = + X1: "
			      "position 3; place unknown."));
	}

	SECTION("um texto digitado com %2 dentro não é reescrito")
	{
		CHECK(DrcStripNumbers::duplicateText(
			      QStringLiteral("X%3"),
			      {mark("%2", 0, 1, "A1"), mark("%2", 1, 1, "A2")})
		      == QStringLiteral(
			      "Number %2 is on 2 terminals of strip X%3: "
			      "position 1 (folio 1, A1); position 2 (folio 1, A2)."));
	}

	SECTION("e cala quando não há o que dizer")
	{
		CHECK(DrcStripNumbers::duplicateText(x1, {}).isEmpty());
		CHECK(DrcStripNumbers::duplicateText(x1, {mark("3", 2, 1, "B3")})
		      .isEmpty());
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("3", 2, 1, "B3"), mark("4", 3, 1, "B4")})
		      .isEmpty());
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("", 2, 1, "B3"), mark("", 3, 1, "B4")})
		      .isEmpty());
			//Two levels of one terminal: a terminal is not reported
			//against itself.
		CHECK(DrcStripNumbers::duplicateText(
			      x1, {mark("5", 2, 1, "B3"), mark("5", 2, 1, "B3")})
		      .isEmpty());
	}
}

TEST_CASE("T24 — as frases da régua vazia e do nome repetido",
	  "[drc][t24]")
{
	SECTION("régua vazia: a frase nomeia a régua")
	{
		CHECK(DrcStripNumbers::emptyStripText(QStringLiteral("= + X9"))
		      == QStringLiteral(
			      "Terminal strip = + X9 has no terminal on any folio."));
		CHECK(DrcStripNumbers::emptyStripText(QString()).isEmpty());
	}

	SECTION("nome repetido: quantas réguas e qual nome")
	{
		CHECK(DrcStripNumbers::sameNameText(QStringLiteral("= + X1"), 2)
		      == QStringLiteral("2 terminal strips are named = + X1."));
		CHECK(DrcStripNumbers::sameNameText(QStringLiteral("= + X1"), 1)
		      .isEmpty());
		CHECK(DrcStripNumbers::sameNameText(QString(), 2).isEmpty());
	}
}
