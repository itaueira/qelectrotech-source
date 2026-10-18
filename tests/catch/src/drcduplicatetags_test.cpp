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
#include "../../../sources/autoNum/iecstructure.h"
#include "../../../sources/diagramcontext.h"
#include "../../../sources/drc/drcduplicatetags.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QString>

/*
	When two components carry the same tag, and the sentence the finding
	says it with.

	Here, in the pure suite, because none of it needs a component: it is
	handed tags, folios, cells, and the information of a component and of
	its title block, and answers from that. Which components are compared,
	and reading their tags off the folios, is the scene's half, proved in
	drctagrules_test.cpp of C_uitests.

	Four things are settled:

	1. what makes two tags the same - trimmed and case folded, and an empty
	   tag is no tag;
	2. one group per tag, whatever the number of components carrying it:
	   three alike are one finding, not three pairs;
	3. the tag compared is the one the component carries with the IEC
	   81346 structure off, and the full designation with it on - whatever
	   the display form, so that -K3 in two cabinets is two tags;
	4. the sentence names the tag, the place of every member, and which of
	   the readings produced it.

	The sentences are compared whole, in the source language. No translator
	is installed in this suite, and the sentence is part of the rule.
*/

namespace
{
	DrcDuplicateTags::Mark mark(const char *tag,
				    int folio = 0,
				    const char *position = "")
	{
		DrcDuplicateTags::Mark built;
		built.tag = QString::fromUtf8(tag);
		built.folio = folio;
		built.position = QString::fromUtf8(position);
		return built;
	}

	/// The title block of a folio of plant CT1, saying no location.
	DiagramContext folioOfPlant()
	{
		DiagramContext folio;
		folio.addValue(IecStructure::plantKey(), QStringLiteral("CT1"));
		return folio;
	}

	/// A component whose + part comes from the location tree.
	DiagramContext placedInTree(const QString &path)
	{
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(), path);
		return info;
	}

	/// A component whose cabinet was typed into the free location field.
	DiagramContext typedInField(const QString &location)
	{
		DiagramContext info;
		info.addValue(IecStructure::locationKey(), location);
		return info;
	}

	IecStructureSettings structureOn()
	{
		IecStructureSettings settings;
		settings.enabled = true;
			//Said, although it is the default: the case that matters is
			//the short form, where both components draw -K3.
		settings.display = IecTagDisplay::Short;
		return settings;
	}
}

TEST_CASE("T24 — a mesma tag é a mesma aparada e sem distinção de caixa, e tag vazia não é tag",
	  "[drc][t24]")
{
	SECTION("espaço nas pontas e caixa não separam duas tags")
	{
		CHECK(DrcDuplicateTags::key(QStringLiteral("K3"))
		      == DrcDuplicateTags::key(QStringLiteral(" K3 ")));
		CHECK(DrcDuplicateTags::key(QStringLiteral("K3"))
		      == DrcDuplicateTags::key(QStringLiteral("k3")));
		CHECK(DrcDuplicateTags::key(QStringLiteral("=CT1+A1-K3"))
		      == DrcDuplicateTags::key(QStringLiteral("=ct1+a1-k3")));
	}

	SECTION("espaço no meio e zero à esquerda separam: é o que alguém escreveu")
	{
		CHECK(DrcDuplicateTags::key(QStringLiteral("K 3"))
		      != DrcDuplicateTags::key(QStringLiteral("K3")));
		CHECK(DrcDuplicateTags::key(QStringLiteral("K03"))
		      != DrcDuplicateTags::key(QStringLiteral("K3")));
	}

	SECTION("vazio e só espaço não têm chave, e não entram em comparação")
	{
		CHECK(DrcDuplicateTags::key(QString()).isEmpty());
		CHECK(DrcDuplicateTags::key(QStringLiteral("   ")).isEmpty());
		CHECK(DrcDuplicateTags::key(QStringLiteral("\t")).isEmpty());
			//A no-break space pasted from a sheet is spacing too.
		CHECK(DrcDuplicateTags::key(QString(QChar(0x00A0))).isEmpty());
	}
}

TEST_CASE("T24 — um grupo por tag repetida, e não um por par: três iguais são um achado só",
	  "[drc][t24]")
{
	SECTION("duas iguais, entre outras diferentes, formam um grupo")
	{
		const QList<QList<int>> found = DrcDuplicateTags::groups(
					{mark("K3", 1), mark("K4", 1), mark("K3", 3)});
		CHECK(found == QList<QList<int>>({{0, 2}}));
	}

	SECTION("três iguais, escritas de três jeitos, são um grupo de três")
	{
		const QList<QList<int>> found = DrcDuplicateTags::groups(
					{mark("K3", 1), mark("k3", 2), mark(" K3", 3)});
		REQUIRE(found.count() == 1);
		CHECK(found.first() == QList<int>({0, 1, 2}));
	}

	SECTION("duas tags repetidas são dois grupos, na ordem em que a primeira de cada aparece")
	{
		const QList<QList<int>> found = DrcDuplicateTags::groups(
					{mark("K4"), mark("K3"), mark("K4"), mark("K3"),
					 mark("K5")});
		CHECK(found == QList<QList<int>>({{0, 2}, {1, 3}}));
	}

	SECTION("e cala: todas diferentes")
	{
		CHECK(DrcDuplicateTags::groups(
			      {mark("K3"), mark("K4"), mark("K 3"), mark("K03")})
		      .isEmpty());
	}

	SECTION("e cala: componentes sem tag não compartilham tag nenhuma")
	{
		CHECK(DrcDuplicateTags::groups(
			      {mark(""), mark("   "), mark(""), mark("K3")})
		      .isEmpty());
	}
}

TEST_CASE("T24 — a tag comparada: como digitada com a estrutura desligada, a designação completa com ela ligada",
	  "[drc][t24]")
{
	const DiagramContext folio = folioOfPlant();

	SECTION("desligada, a tag sai intacta, e o armário não a separa")
	{
		const IecStructureSettings off;
		REQUIRE_FALSE(off.enabled);

		const QString a1 = DrcDuplicateTags::comparedTag(
					   off, QStringLiteral("K3"),
					   placedInTree(QStringLiteral("A1")), folio);
		const QString a2 = DrcDuplicateTags::comparedTag(
					   off, QStringLiteral("K3"),
					   placedInTree(QStringLiteral("A2")), folio);
		CHECK(a1 == QStringLiteral("K3"));
		CHECK(a2 == QStringLiteral("K3"));
		CHECK(DrcDuplicateTags::groups({mark("K3"), mark("K3")}).count() == 1);
		CHECK(DrcDuplicateTags::readingOf(off)
		      == DrcDuplicateTags::Reading::AsTyped);
	}

	SECTION("ligada na forma curta, o desenho mostra -K3 nos dois, e a comparação vê dois armários")
	{
		const IecStructureSettings on = structureOn();

			//What the folio draws, through the same composition: the
			//same -K3 twice. This is what comparing the drawn form would
			//report, and what the rule must not.
		CHECK(on.composedTag(QStringLiteral("K3"),
				     placedInTree(QStringLiteral("A1")), folio)
		      == QStringLiteral("-K3"));
		CHECK(on.composedTag(QStringLiteral("K3"),
				     placedInTree(QStringLiteral("A2")), folio)
		      == QStringLiteral("-K3"));

		const QString a1 = DrcDuplicateTags::comparedTag(
					   on, QStringLiteral("K3"),
					   placedInTree(QStringLiteral("A1")), folio);
		const QString a2 = DrcDuplicateTags::comparedTag(
					   on, QStringLiteral("K3"),
					   placedInTree(QStringLiteral("A2")), folio);
		CHECK(a1 == QStringLiteral("=CT1+A1-K3"));
		CHECK(a2 == QStringLiteral("=CT1+A2-K3"));

		DrcDuplicateTags::Mark first;
		first.tag = a1;
		DrcDuplicateTags::Mark second;
		second.tag = a2;
		CHECK(DrcDuplicateTags::groups({first, second}).isEmpty());

			//And the settings of the project are not touched: the full
			//form is forced on a copy.
		CHECK(on.display == IecTagDisplay::Short);
		CHECK(DrcDuplicateTags::readingOf(on)
		      == DrcDuplicateTags::Reading::ComposedFromTree);
	}

	SECTION("ligada, o mesmo armário dá a mesma designação, e ela é a que a frase nomeia")
	{
		const IecStructureSettings on = structureOn();
		const QString a = DrcDuplicateTags::comparedTag(
					  on, QStringLiteral("K3"),
					  placedInTree(QStringLiteral("A1")), folio);
		const QString b = DrcDuplicateTags::comparedTag(
					  on, QStringLiteral("K3"),
					  placedInTree(QStringLiteral("A1")), folio);
		CHECK(a == QStringLiteral("=CT1+A1-K3"));
		CHECK(a == b);
	}

	SECTION("ligada, o armário digitado no campo livre não separa, e a regra acusa com razão")
	{
		const IecStructureSettings on = structureOn();
		REQUIRE_FALSE(on.location_from_element);

		const QString a1 = DrcDuplicateTags::comparedTag(
					   on, QStringLiteral("K3"),
					   typedInField(QStringLiteral("A1")), folio);
		const QString a2 = DrcDuplicateTags::comparedTag(
					   on, QStringLiteral("K3"),
					   typedInField(QStringLiteral("A2")), folio);
		CHECK(a1 == QStringLiteral("=CT1-K3"));
		CHECK(a2 == QStringLiteral("=CT1-K3"));
	}

	SECTION("e, se o projeto disser que o campo é o + da norma, ele separa")
	{
		IecStructureSettings on = structureOn();
		on.location_from_element = true;

		CHECK(DrcDuplicateTags::comparedTag(
			      on, QStringLiteral("K3"),
			      typedInField(QStringLiteral("A1")), folio)
		      == QStringLiteral("=CT1+A1-K3"));
		CHECK(DrcDuplicateTags::comparedTag(
			      on, QStringLiteral("K3"),
			      typedInField(QStringLiteral("A2")), folio)
		      == QStringLiteral("=CT1+A2-K3"));
		CHECK(DrcDuplicateTags::readingOf(on)
		      == DrcDuplicateTags::Reading::ComposedFromTreeOrField);
	}

	SECTION("ligada, o componente sem tag continua sem tag: o armário sozinho não é designação")
	{
		const IecStructureSettings on = structureOn();
		const QString none = DrcDuplicateTags::comparedTag(
					     on, QString(),
					     placedInTree(QStringLiteral("A1")), folio);
		CHECK(DrcDuplicateTags::key(none).isEmpty());
	}
}

TEST_CASE("T24 — a frase diz a tag, onde está cada componente e qual leitura foi usada",
	  "[drc][t24]")
{
	using Reading = DrcDuplicateTags::Reading;

	SECTION("desligada: a tag como digitada, a folha e a casa de cada um")
	{
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("K3", 1, "B2"), mark("K3", 3, "C4")},
			      Reading::AsTyped)
		      == QStringLiteral(
			      "Tag K3 on 2 components: folio 1, B2; folio 3, C4. "
			      "Compared as typed: the IEC 81346 structure is off."));
	}

	SECTION("ligada, pela árvore: a designação composta que ficou igual, e que o campo livre não foi lido")
	{
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("=CT1-K3", 1, "B2"), mark("=CT1-K3", 3, "C4")},
			      Reading::ComposedFromTree)
		      == QStringLiteral(
			      "Tag =CT1-K3 on 2 components: folio 1, B2; folio 3, C4. "
			      "Compared as composed by the IEC 81346 structure, the + "
			      "part taken from the location tree only: the location "
			      "field is not read."));
	}

	SECTION("ligada, pela árvore ou pelo campo")
	{
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("=CT1+A1-K3", 1, "B2"), mark("=CT1+A1-K3", 1, "D2")},
			      Reading::ComposedFromTreeOrField)
		      == QStringLiteral(
			      "Tag =CT1+A1-K3 on 2 components: folio 1, B2; folio 1, D2. "
			      "Compared as composed by the IEC 81346 structure, the + "
			      "part taken from the location tree, or from the location "
			      "field where there is no tree path."));
	}

	SECTION("três iguais: uma frase só, com os três lugares")
	{
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("K3", 1, "B2"), mark("K3", 2, "A1"),
			       mark("K3", 3, "C4")},
			      Reading::AsTyped)
		      == QStringLiteral(
			      "Tag K3 on 3 components: folio 1, B2; folio 2, A1; "
			      "folio 3, C4. "
			      "Compared as typed: the IEC 81346 structure is off."));
	}

	SECTION("grafias diferentes: cada lugar diz qual está lá")
	{
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("K3", 1, "B2"), mark(" k3", 3, "C4")},
			      Reading::AsTyped)
		      == QStringLiteral(
			      "Tag K3 on 2 components: K3 on folio 1, B2; "
			      "k3 on folio 3, C4. "
			      "Compared as typed: the IEC 81346 structure is off."));
	}

	SECTION("a tag é texto digitado, e um %2 dentro dela não é reescrito")
	{
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("K%2", 1, "B2"), mark("K%2", 2, "B2")},
			      Reading::AsTyped)
		      == QStringLiteral(
			      "Tag K%2 on 2 components: folio 1, B2; folio 2, B2. "
			      "Compared as typed: the IEC 81346 structure is off."));
	}

	SECTION("lugar sem casa, e lugar desconhecido")
	{
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("K3", 1), mark("K3")},
			      Reading::AsTyped)
		      == QStringLiteral(
			      "Tag K3 on 2 components: folio 1; place unknown. "
			      "Compared as typed: the IEC 81346 structure is off."));
	}

	SECTION("e cala quando não há o que dizer")
	{
			//One component is not a duplicate.
		CHECK(DrcDuplicateTags::duplicateText({mark("K3", 1, "B2")},
						      Reading::AsTyped)
		      .isEmpty());
			//Two tags that are not the same are not one group.
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("K3", 1, "B2"), mark("K4", 1, "C2")},
			      Reading::AsTyped)
		      .isEmpty());
			//Two components with no tag do not share one.
		CHECK(DrcDuplicateTags::duplicateText(
			      {mark("", 1, "B2"), mark("  ", 1, "C2")},
			      Reading::AsTyped)
		      .isEmpty());
	}

	SECTION("toda leitura tem palavras próprias, e nenhuma é vazia")
	{
		const QString typed = DrcDuplicateTags::readingText(Reading::AsTyped);
		const QString tree =
				DrcDuplicateTags::readingText(Reading::ComposedFromTree);
		const QString either = DrcDuplicateTags::readingText(
					       Reading::ComposedFromTreeOrField);
		CHECK_FALSE(typed.isEmpty());
		CHECK_FALSE(tree.isEmpty());
		CHECK_FALSE(either.isEmpty());
		CHECK(typed != tree);
		CHECK(typed != either);
		CHECK(tree != either);
	}
}
