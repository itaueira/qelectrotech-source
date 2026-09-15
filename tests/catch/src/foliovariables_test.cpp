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
#include "../../../sources/foliovariables.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QString>

/*
	The substitution that turns the folio template of a sheet into the page
	number a reader sees.

	It is proved here, with no project and no drawing, because it is a rule
	over a string and because it now has two callers that must never
	disagree: BorderTitleBlock, which draws the number on the sheet, and
	WiringListExport, which prints it in the Page column of the cable list.
	The defect that made it a shared function was measured on a real project
	of fourteen sheets - 250 rows of cable list, every one of them carrying
	the literal "%id/%total" where the sheet number belongs.

	What is asserted below is the text of the answer and not only its shape.
	"the result holds no per cent sign" would pass on a function that gave
	back an empty string for everything, and an empty Page column is the
	same failure wearing different clothes.
*/

TEST_CASE("T17 — o modelo de folio vira número de página",
	  "[foliovariables][t17]")
{
	SECTION("o modelo padrão do carimbo")
	{
			//"%id/%total" is what TitleBlockProperties seeds a new sheet
			//with, and what every example project shipped with
			//QElectroTech carries. It is the case that matters.
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%id/%total"), 3, 14)
		      == QStringLiteral("3/14"));
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%id/%total"), 1, 1)
		      == QStringLiteral("1/1"));
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%id/%total"), 14, 14)
		      == QStringLiteral("14/14"));
	}

	SECTION("cada variável sozinha")
	{
			//examples/industrial.qet uses "%id" on its fifty sheets, so
			//the two variables really do occur apart.
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%id"), 7, 50)
		      == QStringLiteral("7"));
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%total"), 7, 50)
		      == QStringLiteral("50"));
	}

	SECTION("o texto ao redor da variável é preservado")
	{
			//A designer is free to write around the variable, and losing
			//what they wrote would be as wrong as not substituting.
		CHECK(FolioVariables::resolveIndex(QStringLiteral("Folha %id de %total"), 2, 9)
		      == QStringLiteral("Folha 2 de 9"));
		CHECK(FolioVariables::resolveIndex(QStringLiteral("A-%id"), 2, 9)
		      == QStringLiteral("A-2"));
	}

	SECTION("um folio sem variável nenhuma atravessa intacto")
	{
		CHECK(FolioVariables::resolveIndex(QStringLiteral("Capa"), 1, 14)
		      == QStringLiteral("Capa"));
		CHECK(FolioVariables::resolveIndex(QString(), 1, 14).isEmpty());
	}

	SECTION("uma variável que este vocabulário não conhece fica onde está")
	{
			//Not blanked. An unknown variable reaching the reader as
			//itself is a question somebody can answer; an empty cell is
			//not, and it is indistinguishable from a sheet with no
			//number.
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%folio"), 3, 14)
		      == QStringLiteral("%folio"));
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%autonum/%total"), 3, 14)
		      == QStringLiteral("%autonum/14"));
	}

	SECTION("a mesma variável duas vezes é substituída nas duas")
	{
		CHECK(FolioVariables::resolveIndex(QStringLiteral("%id-%id/%total"), 4, 8)
		      == QStringLiteral("4-4/8"));
	}
}

TEST_CASE("T17 — a numeração automática entra antes da posição",
	  "[foliovariables][t17]")
{
	SECTION("o valor da numeração ocupa o lugar da variável")
	{
		CHECK(FolioVariables::resolveAutonum(QStringLiteral("%autonum"),
						     QStringLiteral("A12"))
		      == QStringLiteral("A12"));
		CHECK(FolioVariables::resolveAutonum(QStringLiteral("%autonum/%total"),
						     QStringLiteral("A12"))
		      == QStringLiteral("A12/%total"));
	}

	SECTION("sem contexto de numeração, a variável é removida")
	{
			//What BorderTitleBlock::setFolioData() has always done for a
			//sheet whose numbering context is empty, and it is written
			//down here because it is the surprising half: the variable
			//does not survive to be substituted later.
		CHECK(FolioVariables::resolveAutonum(QStringLiteral("%autonum"), QString())
		      .isEmpty());
		CHECK(FolioVariables::resolveAutonum(QStringLiteral("[%autonum]"), QString())
		      == QStringLiteral("[]"));
	}

	SECTION("a ordem das duas etapas é o que decide o resultado")
	{
			//The numbering context can hand back a string carrying a
			//variable of its own. Autonum first, position second, means
			//that string is substituted in turn - which is the behaviour
			//the title block has always had and which a reordering of the
			//two calls would change without a word.
		const QString once = FolioVariables::resolveAutonum(
					QStringLiteral("%autonum"),
					QStringLiteral("F%id"));
		CHECK(once == QStringLiteral("F%id"));
		CHECK(FolioVariables::resolveIndex(once, 6, 20) == QStringLiteral("F6"));
	}
}
