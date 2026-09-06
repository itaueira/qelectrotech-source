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

#include "../../../sources/diagramcontext.h"
#include "../../../sources/qetinformation.h"
#include "qt_catch_tostring.h"

/*
	Drawing a title block blind.

	An unfilled field prints nothing on the folio, and that is the fix the
	suite next door guards (titleblockvariables_test.cpp). The title block
	editor renders its preview through the very same path, with an empty
	context, so the same fix left every attribute cell blank *while it is
	being drawn* - the one moment somebody needs to see what is already in
	the cell. Before the fix the cell showed "%{name}", and that accident was
	doing the job.

	What puts the name back is a context built for authoring: every variable
	the cell names, mapped to its own name. The folio is untouched, because
	the folio renders with the context of its project.

	Both halves of the rule live in QETInformation, next to
	stripUnresolvedVariables, on purpose: the editor has to show exactly what
	the folio erases, and two readings of "what is a variable" would drift
	apart without anybody noticing until a title block was being drawn.
*/

TEST_CASE("T26 — o editor nomeia a variável que a folha apaga", "[titleblock]")
{
	SECTION("a forma com chaves conta, seja qual for o nome dentro")
	{
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%{revisor_4}"))
		      == QStringList{QStringLiteral("revisor_4")});

		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%{campo-que-o-cliente-inventou}"))
		      == QStringList{QStringLiteral("campo-que-o-cliente-inventou")});

		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("Salvo em %{saveddate} por %{author}"))
		      == QStringList{QStringLiteral("saveddate"),
				     QStringLiteral("author")});

		// "%{}" names nothing, and the folio drops it all the same: there
		// is no name to put back in its place.
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%{}")).isEmpty());
	}

	SECTION("a forma sem chaves conta só para o vocabulário do carimbo")
	{
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%author"))
		      == QStringList{QStringLiteral("author")});

		// A title block legitimately says "100%", and a cell that lost its
		// per cent sign would be a worse bug than the one being fixed. The
		// same asymmetry that protects it on the folio protects it here.
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("Escala 100%")).isEmpty());
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("50% de carga")).isEmpty());
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%naoexiste")).isEmpty());
	}

	SECTION("o mais comprido conta primeiro, senão sobra um pedaço")
	{
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%folio-total"))
		      == QStringList{QStringLiteral("folio-total")});
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%saveddate-eu"))
		      == QStringList{QStringLiteral("saveddate-eu")});
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%previous-folio-num"))
		      == QStringList{QStringLiteral("previous-folio-num")});
	}

	SECTION("o carimbo que vem com o programa escreve sem chaves")
	{
		// This is the measurement that decided the shape of the rule.
		// Five of the ten templates shipped in titleblocks/ use the bare
		// form, and default.titleblock - the one embedded in the resources
		// and the fallback of every project - uses *only* the bare form:
		// zero braced references, five bare ones, named below. A scan that
		// collected the braced form alone would name nothing at all in it,
		// and every cell of the default title block would go on being
		// drawn blank in the editor.
		const QStringList of_the_default_template = {
			QStringLiteral("author"),
			QStringLiteral("title"),
			QStringLiteral("filename"),
			QStringLiteral("date"),
			QStringLiteral("folio") };
		for (const QString &name : of_the_default_template)
		{
			CHECK(QETInformation::titleblockVariablesIn(
				      QStringLiteral("%") + name)
			      == QStringList{name});
		}
	}

	SECTION("o que a folha apaga é exatamente o que o editor nomeia")
	{
		// The rule is stated once and read twice, so the pair is what has
		// to hold: a reference the folio erases is a reference the editor
		// can name, and a per cent sign the folio keeps is not a reference
		// at all.
		const QStringList erased = {
			QStringLiteral("%{saveddate}"),
			QStringLiteral("%{campo-do-cliente}"),
			QStringLiteral("%folio-total"),
			QStringLiteral("%projecttitle"),
			QStringLiteral("%author") };
		for (const QString &text : erased)
		{
			CHECK(QETInformation::stripUnresolvedVariables(text)
			      .isEmpty());
			CHECK(QETInformation::titleblockVariablesIn(text).count()
			      == 1);
		}

		const QStringList kept = {
			QStringLiteral("Escala 100%"),
			QStringLiteral("50% de carga"),
			QStringLiteral("%naoexiste"),
			QStringLiteral("Indústria Elétrica S.A.") };
		for (const QString &text : kept)
		{
			CHECK(QETInformation::stripUnresolvedVariables(text)
			      == text);
			CHECK(QETInformation::titleblockVariablesIn(text)
			      .isEmpty());
		}
	}

	SECTION("texto sem por cento nenhum não nomeia nada")
	{
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("Indústria Elétrica S.A.")).isEmpty());
		CHECK(QETInformation::titleblockVariablesIn(QString()).isEmpty());
	}

	SECTION("o contexto de autoria mapeia cada nome para si mesmo")
	{
		const DiagramContext context =
			QETInformation::titleblockAuthoringContext(
				{QStringLiteral("%{revisor_4}"),
				 QStringLiteral("%author")});

		REQUIRE(context.contains(QStringLiteral("revisor_4")));
		REQUIRE(context.contains(QStringLiteral("author")));
		CHECK(context.value(QStringLiteral("revisor_4")).toString()
		      == QStringLiteral("revisor_4"));
		CHECK(context.value(QStringLiteral("author")).toString()
		      == QStringLiteral("author"));
		CHECK(context.keys().count() == 2);
	}

	SECTION("valor e rótulo entram no mesmo contexto")
	{
		// finalTextForCell() substitutes in the label as well as in the
		// value, so a variable that only ever appears as a label has to be
		// named too - otherwise the label is the half of the cell that
		// goes on being blank.
		const DiagramContext context =
			QETInformation::titleblockAuthoringContext(
				{QStringLiteral("%{data_rev_1}"),
				 QStringLiteral("%{setor}")});

		CHECK(context.contains(QStringLiteral("data_rev_1")));
		CHECK(context.contains(QStringLiteral("setor")));
	}

	SECTION("nome que o contexto do projeto não pode guardar fica de fora")
	{
		// DiagramContext::validKeyRegExp() is "^[a-z0-9-_]+$", so an
		// upper-case name is a name no project can ever give a value to.
		// The scan still names it - that is a fact about the text - and
		// the context is where it is dropped, which is the truth about
		// what such a cell will print: nothing, ever.
		CHECK(QETInformation::titleblockVariablesIn(
			      QStringLiteral("%{Cliente}"))
		      == QStringList{QStringLiteral("Cliente")});

		const DiagramContext context =
			QETInformation::titleblockAuthoringContext(
				{QStringLiteral("%{Cliente}")});
		CHECK_FALSE(context.contains(QStringLiteral("Cliente")));
		CHECK(context.keys().isEmpty());
	}
}
