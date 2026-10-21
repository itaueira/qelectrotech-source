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
#include "../../../sources/drc/drctitleblock.h"

#include "../../../sources/diagramcontext.h"
#include "../../../sources/qetinformation.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVariant>

/*
	Which fields of the title block of one folio are blank, and the
	sentence a folio with any of them is reported with.

	Here, in the pure suite, because none of it needs a folio: it is
	handed the context a title block is drawn from and the list of fields
	that must be filled in, and answers from that. Which folios there are,
	and that the context handed in is the one the sheet prints, is the
	walk's half, proved in drctitleblockrules_test.cpp of C_uitests.

	Three things are settled:

	1. the factory list is the title and the author, and both are keys
	   the folio's title block actually carries - a misspelt key would be
	   reported blank on every folio of every project;
	2. blank is what prints as nothing: empty, made only of spacing, or not
	   in the context at all - and a field that is not asked for is never
	   reported, however blank;
	3. the sentence names every blank field, one or several.

	The sentences are compared whole, in the source language. No translator
	is installed in this suite, and the sentence is part of the rule.
*/

namespace
{
	/// A context holding @a title and @a author, as a folio's would.
	DiagramContext folio(const QString &title, const QString &author)
	{
		DiagramContext context;
		context.addValue(QStringLiteral("title"), title);
		context.addValue(QStringLiteral("author"), author);
		return context;
	}
}

TEST_CASE("T24 — os campos obrigatórios de fábrica são o título e o autor, e os dois são campos do carimbo da folha",
	  "[drc][t24]")
{
	const QStringList factory = DrcTitleBlock::factoryRequiredKeys();
	CHECK(factory == QStringList({QStringLiteral("title"),
				      QStringLiteral("author")}));

		//The keys diagram_info has a column for, and the context of every
		//folio is given a value for.
	const QStringList folio_keys = QETInformation::diagramInfoKeys();
	for (const QString &key : factory)
	{
		INFO(key.toStdString());
		CHECK(folio_keys.contains(key));
	}
}

TEST_CASE("T24 — campo em branco é o que imprime nada: vazio, só espaço, ou fora do contexto; e campo não pedido não é acusado",
	  "[drc][t24]")
{
	const QStringList factory = DrcTitleBlock::factoryRequiredKeys();

	SECTION("o que conta como branco")
	{
		CHECK(DrcTitleBlock::isBlank(QVariant()));
		CHECK(DrcTitleBlock::isBlank(QString()));
		CHECK(DrcTitleBlock::isBlank(QStringLiteral("")));
		CHECK(DrcTitleBlock::isBlank(QStringLiteral("   ")));
		CHECK(DrcTitleBlock::isBlank(QStringLiteral("\t \n")));
			//The no-break space a value pasted from a spreadsheet
			//brings along: it draws nothing either.
		CHECK(DrcTitleBlock::isBlank(QString(QChar(0x00A0))));

		CHECK_FALSE(DrcTitleBlock::isBlank(QStringLiteral("A")));
		CHECK_FALSE(DrcTitleBlock::isBlank(QStringLiteral("  A  ")));
		CHECK_FALSE(DrcTitleBlock::isBlank(QStringLiteral("0")));
		CHECK_FALSE(DrcTitleBlock::isBlank(QStringLiteral("_")));
	}

	SECTION("título e autor preenchidos: nada")
	{
		CHECK(DrcTitleBlock::emptyKeys(
			      folio(QStringLiteral("Power supply"),
				    QStringLiteral("J. Doe")),
			      factory).isEmpty());
	}

	SECTION("um dos dois em branco: só ele")
	{
		CHECK(DrcTitleBlock::emptyKeys(
			      folio(QString(), QStringLiteral("J. Doe")), factory)
		      == QStringList({QStringLiteral("title")}));
		CHECK(DrcTitleBlock::emptyKeys(
			      folio(QStringLiteral("Control"), QStringLiteral("  ")),
			      factory)
		      == QStringList({QStringLiteral("author")}));
	}

	SECTION("um campo que o contexto nem tem é branco: a folha o imprime vazio")
	{
		DiagramContext context;
		context.addValue(QStringLiteral("title"), QStringLiteral("Control"));
		REQUIRE_FALSE(context.contains(QStringLiteral("author")));

		CHECK(DrcTitleBlock::emptyKeys(context, factory)
		      == QStringList({QStringLiteral("author")}));
		CHECK(DrcTitleBlock::emptyKeys(DiagramContext(), factory)
		      == factory);
	}

	SECTION("os dois em branco: os dois, na ordem da lista pedida")
	{
		const DiagramContext context =
				folio(QStringLiteral(" "), QString(QChar(0x00A0)));
		CHECK(DrcTitleBlock::emptyKeys(context, factory)
		      == QStringList({QStringLiteral("title"),
				      QStringLiteral("author")}));
		CHECK(DrcTitleBlock::emptyKeys(context,
					       {QStringLiteral("author"),
						QStringLiteral("title")})
		      == QStringList({QStringLiteral("author"),
				      QStringLiteral("title")}));
	}

	SECTION("campo não pedido não é acusado, por mais vazio que esteja")
	{
			//The fields the program fills in, the ones of the plant
			//structure and the revision, all blank: the factory list does
			//not ask for them, and the folio is filled in as far as it is
			//concerned.
		DiagramContext context = folio(QStringLiteral("Power supply"),
					       QStringLiteral("J. Doe"));
		for (const char *key : {"filename", "folio", "plant", "locmach",
					"indexrev", "date"}) {
			context.addValue(QLatin1String(key), QString());
		}
		CHECK(DrcTitleBlock::emptyKeys(context, factory).isEmpty());

			//And asked for, it is reported: the list is what decides,
			//not the rule.
		CHECK(DrcTitleBlock::emptyKeys(context,
					       {QStringLiteral("title"),
						QStringLiteral("author"),
						QStringLiteral("date")})
		      == QStringList({QStringLiteral("date")}));
	}

	SECTION("lista vazia, nome vazio e nome repetido")
	{
		const DiagramContext context = folio(QString(), QString());
		CHECK(DrcTitleBlock::emptyKeys(context, QStringList()).isEmpty());
		CHECK(DrcTitleBlock::emptyKeys(context, {QString()}).isEmpty());
		CHECK(DrcTitleBlock::emptyKeys(context,
					       {QStringLiteral("title"),
						QString(),
						QStringLiteral("title")})
		      == QStringList({QStringLiteral("title")}));
	}
}

TEST_CASE("T24 — a frase nomeia os campos em branco da folha, um ou vários, e não diz só que o carimbo está incompleto",
	  "[drc][t24]")
{
	SECTION("nada em branco não é frase nenhuma")
	{
		CHECK(DrcTitleBlock::emptyFieldsText(QStringList()).isEmpty());
	}

	SECTION("um campo, no singular, pelo nome que o programa já mostra")
	{
		CHECK(DrcTitleBlock::emptyFieldsText({QStringLiteral("title")})
		      == QStringLiteral("Empty title block field: Title"));
		CHECK(DrcTitleBlock::emptyFieldsText({QStringLiteral("author")})
		      == QStringLiteral("Empty title block field: Author"));
	}

	SECTION("vários, no plural, na ordem dada")
	{
		CHECK(DrcTitleBlock::emptyFieldsText({QStringLiteral("title"),
						      QStringLiteral("author")})
		      == QStringLiteral("Empty title block fields: Title, Author"));
		CHECK(DrcTitleBlock::emptyFieldsText({QStringLiteral("author"),
						      QStringLiteral("title")})
		      == QStringLiteral("Empty title block fields: Author, Title"));
		CHECK(DrcTitleBlock::emptyFieldsText({QStringLiteral("title"),
						      QStringLiteral("author"),
						      QStringLiteral("date")})
		      == QStringLiteral(
			      "Empty title block fields: Title, Author, Date"));
	}

	SECTION("um campo do usuário, que não tem outro nome, sai pela chave")
	{
		CHECK(DrcTitleBlock::fieldName(QStringLiteral("client"))
		      == QStringLiteral("client"));
		CHECK(DrcTitleBlock::emptyFieldsText({QStringLiteral("client")})
		      == QStringLiteral("Empty title block field: client"));
	}

	SECTION("um nome com %1 dentro não é reescrito")
	{
			//Each name goes into the sentence once, through a single
			//arg(): a second substitution would turn the "%1" here into
			//the list itself.
		CHECK(DrcTitleBlock::emptyFieldsText({QStringLiteral("%1"),
						      QStringLiteral("title")})
		      == QStringLiteral("Empty title block fields: %1, Title"));
	}
}

TEST_CASE("T24 — só se pede o campo que o modelo de carimbo da folha mostra (P139)",
	  "[drc][t24]")
{
	const QStringList factory = DrcTitleBlock::factoryRequiredKeys();

		//A template with the title and no author: the author is not asked for
	CHECK(DrcTitleBlock::printedKeys(factory,
					 {QStringLiteral("folio"), QStringLiteral("title"),
					  QStringLiteral("date")})
	      == QStringList{QStringLiteral("title")});
		//Both shown, in the order of the fields asked for, whatever the
		//order of the cells
	CHECK(DrcTitleBlock::printedKeys(factory,
					 {QStringLiteral("author"), QStringLiteral("title")})
	      == factory);
		//A template showing none of them asks for nothing
	CHECK(DrcTitleBlock::printedKeys(factory, {QStringLiteral("folio")}).isEmpty());
	CHECK(DrcTitleBlock::printedKeys(factory, {}).isEmpty());
}
