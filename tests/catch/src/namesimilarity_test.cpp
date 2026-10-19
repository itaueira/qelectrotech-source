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
#include "qt_catch_tostring.h"

#include "../../../sources/ElementsCollection/namesimilarity.h"

#include <catch2/catch.hpp>

/*
	The symbols that can replace a selected one are found by the words of
	their names, accents and case aside (T54). The selected terminal was
	"Borne de combinacao", and the ones of the other collection, "Terminal
	de combinação": searching the whole name found none of them.
*/
TEST_CASE("T54 — The words of a name, accents and case aside", "[t54][namesimilarity]")
{
	using NameSimilarity::words;
	CHECK(words(QStringLiteral("Borne de combinacao"))
	      == QStringList{QStringLiteral("borne"), QStringLiteral("combinacao")});
	CHECK(words(QStringLiteral("Terminal de combinação"))
	      == QStringList{QStringLiteral("terminal"), QStringLiteral("combinacao")});
		//Short words and the ones that only join others are left out
	CHECK(words(QStringLiteral("Relé com bobina para 24 V"))
	      == QStringList{QStringLiteral("rele"), QStringLiteral("bobina")});
	CHECK(words(QStringLiteral("  ")).isEmpty());
}

TEST_CASE("T54 — Look-alikes share a word", "[t54][namesimilarity]")
{
	using NameSimilarity::sharedWords;
	using NameSimilarity::words;
	const QStringList query = words(QStringLiteral("Borne de combinacao"));
	CHECK(sharedWords(query, QStringLiteral("Terminal de combinação")) == 1);
	CHECK(sharedWords(query, QStringLiteral("Borne de combinação triplo")) == 2);
		//A plural is the same word
	CHECK(sharedWords(query, QStringLiteral("Bornes e borneiras")) == 1);
	CHECK(sharedWords(query, QStringLiteral("Contator tripolar")) == 0);
}

TEST_CASE("T54 — A rare shared word says more than a common one", "[t54][namesimilarity]")
{
	using NameSimilarity::matchedWords;
	using NameSimilarity::rarityScore;
	using NameSimilarity::words;
	const QStringList query = words(QStringLiteral("Borne de combinacao"));
	const QVector<bool> rare = matchedWords(query, QStringLiteral("Terminal de combinação"));
	const QVector<bool> common = matchedWords(query, QStringLiteral("Borne de aterramento"));
	const QVector<bool> both = matchedWords(query, QStringLiteral("Borne de combinacao"));
	CHECK(rare == QVector<bool>{false, true});
	CHECK(common == QVector<bool>{true, false});

		//"borne" in 100 names of 1000, "combinacao" in 10
	const QVector<int> names_with_word{100, 10};
	CHECK(rarityScore(rare, names_with_word, 1000) > rarityScore(common, names_with_word, 1000));
	CHECK(rarityScore(both, names_with_word, 1000) > rarityScore(rare, names_with_word, 1000));
	CHECK(rarityScore({false, false}, names_with_word, 1000) == 0);
}
