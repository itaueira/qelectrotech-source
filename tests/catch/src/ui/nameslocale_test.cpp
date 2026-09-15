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

#include "../../../../sources/NameList/nameslist.h"

/*
	The settings store the locale the way the language selector wrote it - lower
	case, "pt_br" - while every collection file keys its names the way the locale
	is spelled, "pt_BR".  QMap compares case-sensitively, so the two never met.

	Measured on the shipped collection at the time this was written: of the 9904
	symbol files carrying a pt_BR name, 8704 had NO short "pt" name either, so
	they fell all the way through to English with their Brazilian translation
	sitting unread in the file.  The remaining 1200 were worse in a quieter way:
	they carry a short "pt" name, which is European Portuguese, so the title
	block said "Ficheiro" where Brazil says "Arquivo".

	These cases pin the lookup order.  They are deliberately written against
	caseInsensitiveName(), which takes the language code as an argument: name()
	itself reads QETApp::langFromSetting(), and a test that changed the running
	application's language would leak into every case that runs after it.
*/

TEST_CASE("T27 — o nome regional e achado mesmo com a caixa diferente", "[nameslist]")
{
	SECTION("pt_br da configuracao acha o pt_BR do arquivo")
	{
		NamesList nl;
		nl.addName("en", "Circuit breaker");
		nl.addName("pt_BR", "Disjuntor");

		CHECK(nl.caseInsensitiveName("pt_br") == QString("Disjuntor"));
		CHECK(nl.caseInsensitiveName("pt_BR") == QString("Disjuntor"));
		CHECK(nl.caseInsensitiveName("PT_BR") == QString("Disjuntor"));
	}

	SECTION("a caixa exata do arquivo tambem e achada, e nao so a da configuracao")
	{
		/*
			Not every collection spells it the same way: some files carry "pt_br"
			and some carry "pt_BR".  Both have to answer, or fixing one spelling
			would break the other.
		*/
		NamesList nl;
		nl.addName("en", "Contactor");
		nl.addName("pt_br", "Contator");

		CHECK(nl.caseInsensitiveName("pt_BR") == QString("Contator"));
		CHECK(nl.caseInsensitiveName("pt_br") == QString("Contator"));
	}

	SECTION("idioma que nao esta na lista nao inventa resposta")
	{
		NamesList nl;
		nl.addName("en", "Relay");
		nl.addName("pt_BR", "Rele");

		CHECK(nl.caseInsensitiveName("de_DE").isEmpty());
		CHECK(nl.caseInsensitiveName("nl").isEmpty());
	}

	SECTION("entrada vazia nao conta como achada - senao ela venceria o ingles")
	{
		NamesList nl;
		nl.addName("pt_BR", QString());
		nl.addName("en", "Terminal block");

		CHECK(nl.caseInsensitiveName("pt_br").isEmpty());
	}

	SECTION("lista vazia devolve vazio, nao trava")
	{
		NamesList nl;
		CHECK(nl.caseInsensitiveName("pt_br").isEmpty());
	}
}

TEST_CASE("T27 — pt_BR nao pode virar pt, que e outra lingua", "[nameslist]")
{
	/*
		This is the case that says WHY the case-insensitive step sits BEFORE the
		base-language step rather than after it.  Both spellings are present, and
		they disagree on purpose: "Arquivo" is Brazil, "Ficheiro" is Portugal.  A
		lookup that trimmed "pt_br" down to "pt" first would answer the European
		word to a Brazilian user and look entirely reasonable while doing it -
		which is exactly what the shipped title block did before this change.
	*/
	NamesList nl;
	nl.addName("en", "File");
	nl.addName("pt", "Ficheiro");
	nl.addName("pt_BR", "Arquivo");

	CHECK(nl.caseInsensitiveName("pt_br") == QString("Arquivo"));
	CHECK(nl.caseInsensitiveName("pt_br") != QString("Ficheiro"));

	SECTION("e quem realmente fala o portugues de Portugal continua atendido")
	{
		CHECK(nl.caseInsensitiveName("pt") == QString("Ficheiro"));
		CHECK(nl.caseInsensitiveName("pt_PT") .isEmpty());
	}
}
