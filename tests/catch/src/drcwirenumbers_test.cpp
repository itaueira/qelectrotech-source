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
#include "../../../sources/drc/drcwirenumbers.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QString>
#include <QStringList>

/*
	What counts as a wire number on one potential, and the sentence a
	potential with more than one of them is reported with.

	Here, in the pure suite, because none of it needs a conductor: it is
	handed the texts of the conductors of one potential and the folio of
	each, and answers from that. Which conductors make up a potential is
	the scene's half, proved in drcpotentialrules_test.cpp of C_uitests.

	Three things are settled, and each one is a way the rule could speak
	when it should not, or say the wrong thing when it should:

	1. a blank text and the placeholder of a new conductor are absence, not
	   a second number - a potential numbered on one conductor and left
	   alone on the next is not a finding;
	2. the numbers come out distinct, trimmed and in the order the wiring
	   list reads them: 24 before 105;
	3. the sentence names every number, and the folio of each only when the
	   numbered conductors are on more than one folio.

	The sentences are compared whole, in the source language. No translator
	is installed in this suite, and the sentence is part of the rule: a
	finding that does not say what to change comes back as a question.
*/

namespace
{
	DrcWireNumbers::Mark mark(const char *text, int folio)
	{
		DrcWireNumbers::Mark built;
		built.text = QString::fromUtf8(text);
		built.folio = folio;
		return built;
	}
}

TEST_CASE("T24 — texto vazio, espaço e o marcador de condutor novo não são número de fio",
	  "[drc][t24]")
{
	SECTION("o que é ausência")
	{
		CHECK(DrcWireNumbers::isAbsent(QString()));
		CHECK(DrcWireNumbers::isAbsent(QStringLiteral("")));
		CHECK(DrcWireNumbers::isAbsent(QStringLiteral("   ")));
		CHECK(DrcWireNumbers::isAbsent(QStringLiteral("\t")));
			//A no-break space pasted from a sheet is spacing too.
		CHECK(DrcWireNumbers::isAbsent(QString(QChar(0x00A0))));
			//The text a new conductor is given, and the same with spaces.
		CHECK(DrcWireNumbers::isAbsent(DrcWireNumbers::placeholder()));
		CHECK(DrcWireNumbers::isAbsent(QStringLiteral(" _ ")));
	}

	SECTION("e o que é número, por mais curto que seja")
	{
			//Without this half, an isAbsent() that answered true for
			//everything would pass the section above.
		CHECK_FALSE(DrcWireNumbers::isAbsent(QStringLiteral("24")));
		CHECK_FALSE(DrcWireNumbers::isAbsent(QStringLiteral("0")));
		CHECK_FALSE(DrcWireNumbers::isAbsent(QStringLiteral("_1")));
		CHECK_FALSE(DrcWireNumbers::isAbsent(QStringLiteral("__")));
		CHECK_FALSE(DrcWireNumbers::isAbsent(QStringLiteral("L1")));
	}

	SECTION("um potencial numerado pela metade tem um número só")
	{
		CHECK(DrcWireNumbers::numbersOf({QStringLiteral("24"),
						 QStringLiteral(""),
						 QStringLiteral("_"),
						 QStringLiteral("   "),
						 QStringLiteral(" 24 ")})
		      == QStringList({QStringLiteral("24")}));

			//And a potential nobody numbered has none.
		CHECK(DrcWireNumbers::numbersOf({QStringLiteral(""),
						 QStringLiteral("_"),
						 QStringLiteral("  ")})
		      .isEmpty());
	}
}

TEST_CASE("T24 — os números de um potencial saem distintos e na ordem da lista de fiação",
	  "[drc][t24]")
{
	SECTION("distintos e aparados")
	{
		CHECK(DrcWireNumbers::numbersOf({QStringLiteral("105"),
						 QStringLiteral("24"),
						 QStringLiteral("24"),
						 QStringLiteral(" 105")})
		      == QStringList({QStringLiteral("24"),
				      QStringLiteral("105")}));
	}

	SECTION("números primeiro, pelo valor; depois o resto, como texto")
	{
			//Plain text order would put 105 before 24 and 7 last.
		CHECK(DrcWireNumbers::numbersOf({QStringLiteral("B2"),
						 QStringLiteral("105"),
						 QStringLiteral("A1"),
						 QStringLiteral("7"),
						 QStringLiteral("24")})
		      == QStringList({QStringLiteral("7"),
				      QStringLiteral("24"),
				      QStringLiteral("105"),
				      QStringLiteral("A1"),
				      QStringLiteral("B2")}));
	}

	SECTION("dois textos do mesmo valor são dois números, em ordem fixa")
	{
			//"024" and "24" print two different labels, so the potential
			//carrying both carries two numbers; the order between them is
			//the text order, and the same on every run.
		CHECK(DrcWireNumbers::numbersOf({QStringLiteral("24"),
						 QStringLiteral("024")})
		      == QStringList({QStringLiteral("024"),
				      QStringLiteral("24")}));
		CHECK_FALSE(DrcWireNumbers::isBefore(QStringLiteral("24"),
						     QStringLiteral("24")));
	}
}

TEST_CASE("T24 — a frase nomeia os números do potencial, e a folha de cada um só quando estão em folhas diferentes",
	  "[drc][t24]")
{
	SECTION("um número só, ou nenhum, não é frase nenhuma")
	{
			//The half that keeps the rule quiet: a number with blanks and
			//placeholders around it, on two folios, is one number.
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("24", 1), mark("", 1),
			       mark("_", 2), mark("  ", 2)}).isEmpty());
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("", 1), mark("_", 2)}).isEmpty());
		CHECK(DrcWireNumbers::severalNumbersText({}).isEmpty());

			//The same number on every conductor, across folios, is the
			//potential numbered right.
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("24", 1), mark("24", 2), mark(" 24", 3)})
		      .isEmpty());
	}

	SECTION("dois números na mesma folha: os números, sem folha")
	{
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("105", 3), mark("24", 3)})
		      == QStringLiteral("Potential with 2 wire numbers: 24, 105"));

			//A conductor left blank on another folio does not spread
			//the numbers: they are both still on folio 3.
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("105", 3), mark("24", 3), mark("_", 4)})
		      == QStringLiteral("Potential with 2 wire numbers: 24, 105"));
	}

	SECTION("dois números em folhas diferentes: cada um com a sua folha")
	{
		const QString expected = QStringLiteral(
				"Potential with 2 wire numbers: 24 (folio 1), "
				"105 (folio 2)");

		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("24", 1), mark("105", 2)}) == expected);

			//The order the marks come in changes nothing.
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("105", 2), mark("24", 1)}) == expected);

			//A number on two folios is placed on the lower one.
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("105", 2), mark("24", 4), mark("24", 1)})
		      == expected);
	}

	SECTION("três números: todos, na ordem")
	{
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("105", 2), mark("7", 2), mark("24", 2)})
		      == QStringLiteral("Potential with 3 wire numbers: 7, 24, 105"));
	}

	SECTION("folha desconhecida não vira folha zero")
	{
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("24", 0), mark("105", 0)})
		      == QStringLiteral("Potential with 2 wire numbers: 24, 105"));
	}

	SECTION("um número com marcador de argumento dentro sai como foi digitado")
	{
			//A number is text somebody typed. Two chained arg() calls
			//would rewrite a "%2" inside it with the folio.
		CHECK(DrcWireNumbers::severalNumbersText(
			      {mark("%2", 1), mark("24", 2)})
		      == QStringLiteral("Potential with 2 wire numbers: 24 (folio 2), "
					"%2 (folio 1)"));
	}
}
