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

#include "../../../sources/qetinformation.h"
#include "qt_catch_tostring.h"

/*
	An empty footer field printed its own source code.

	Reported from a real folio: "nas info de rodapé da folha, quando estão
	vazias, o programa estava mostrando o código do campo aquele que começa
	com %".

	The cause is in TitleBlockTemplate::interpreteVariables: it walks the keys
	the diagram context *has*, so a key the context never carried survives
	untouched - as its own text. Six of the project variables (saveddate,
	saveddate-eu, saveddate-us, savedtime, savedfilename, savedfilepath) only
	reach the context when the project is saved, so a project just opened
	printed them literally.
*/

TEST_CASE("carimbo — variável que ninguém preencheu não vira texto na folha",
	  "[titleblock]")
{
	SECTION("a forma com chaves sai, seja qual for o nome dentro")
	{
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%{saveddate}")).isEmpty());

		// Including a name nobody documented: a custom field somebody added
		// to their own template and never filled is exactly the same case,
		// and the braces are what say "this is a variable".
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%{campo-que-o-cliente-inventou}"))
		      .isEmpty());

		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("Salvo em %{saveddate} por %{author}"))
		      == QStringLiteral("Salvo em  por "));
	}

	SECTION("a forma sem chaves sai só para o vocabulário do carimbo")
	{
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%savedfilename")).isEmpty());
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%projecttitle")).isEmpty());
	}

	SECTION("o mais comprido sai primeiro, senão sobra um pedaço")
	{
		// "%folio-total" must not be eaten as "%folio" plus a stray
		// "-total" left on the folio. This is the whole reason the keys are
		// sorted by length before being removed.
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%folio-total")).isEmpty());
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%saveddate-eu")).isEmpty());
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%previous-folio-num")).isEmpty());
	}

	SECTION("por cento que não é variável fica exatamente como foi digitado")
	{
		// A title block legitimately says "100%". Losing that per cent sign
		// would be a worse bug than the one being fixed, so the bare form is
		// removed only for names the title block actually has.
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("Escala 100%"))
		      == QStringLiteral("Escala 100%"));
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("50% de carga"))
		      == QStringLiteral("50% de carga"));
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("%naoexiste"))
		      == QStringLiteral("%naoexiste"));
	}

	SECTION("texto sem por cento nenhum volta idêntico, e sem trabalho")
	{
		const QString plain = QStringLiteral("ACME Industries Ltd.");
		CHECK(QETInformation::stripUnresolvedVariables(plain) == plain);
		CHECK(QETInformation::stripUnresolvedVariables(QString()).isEmpty());
	}

	SECTION("o valor que o contexto trouxe já foi trocado antes, e não se mexe")
	{
		// This function runs after substitution, so what it sees is either a
		// real value or a leftover. A value that happens to contain a per cent
		// sign - a section written "%2,5" by hand, say - must survive.
		CHECK(QETInformation::stripUnresolvedVariables(
			      QStringLiteral("Rev. 3 — 21/08/2026 — 100% conferido"))
		      == QStringLiteral("Rev. 3 — 21/08/2026 — 100% conferido"));
	}

	SECTION("as seis variáveis que só existem depois de salvar")
	{
		// Named one by one on purpose: these are the ones that produced the
		// report, and a change that puts any of them back into the printed
		// text should fail here by name.
		const QStringList only_after_saving = {
			QStringLiteral("saveddate"),
			QStringLiteral("saveddate-eu"),
			QStringLiteral("saveddate-us"),
			QStringLiteral("savedtime"),
			QStringLiteral("savedfilename"),
			QStringLiteral("savedfilepath") };
		for (const QString &key : only_after_saving)
		{
			CHECK(QETInformation::titleblockInfoKeys().contains(key));
			CHECK(QETInformation::stripUnresolvedVariables(
				      QStringLiteral("%{") + key + QStringLiteral("}"))
			      .isEmpty());
			CHECK(QETInformation::stripUnresolvedVariables(
				      QStringLiteral("%") + key).isEmpty());
		}
	}
}
