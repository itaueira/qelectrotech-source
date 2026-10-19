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
#include "namesimilarity.h"

#include <QSet>

#include <algorithm>
#include <cmath>

namespace
{
		/// The words that join others and say nothing of the symbol, in the
		/// languages the collections are named in. Words under three letters
		/// ("de", "du", "of") are left out anyway.
	const QSet<QString> &linkingWords()
	{
		static const QSet<QString> words {
			QStringLiteral("das"), QStringLiteral("dos"), QStringLiteral("com"),
			QStringLiteral("para"), QStringLiteral("por"), QStringLiteral("sem"),
			QStringLiteral("des"), QStringLiteral("les"), QStringLiteral("pour"),
			QStringLiteral("avec"), QStringLiteral("sans"), QStringLiteral("the"),
			QStringLiteral("and"), QStringLiteral("with"), QStringLiteral("for"),
			QStringLiteral("der"), QStringLiteral("die"), QStringLiteral("und"),
			QStringLiteral("mit"), QStringLiteral("del"), QStringLiteral("con")};
		return words;
	}

		/// @a text in lower case and without accents: "Combinação" is "combinacao"
	QString plain(const QString &text)
	{
		const QString decomposed = text.normalized(QString::NormalizationForm_D);
		QString result;
		result.reserve(decomposed.size());
		for (const QChar c : decomposed) {
			if (c.category() != QChar::Mark_NonSpacing) {
				result.append(c.toLower());
			}
		}
		return result;
	}

		/// One word of a name stands for the other: the same, or one the
		/// start of the other ("borne" and "bornes")
	bool sameWord(const QString &a, const QString &b)
	{
		return a.startsWith(b) || b.startsWith(a);
	}
}

/**
	@brief NameSimilarity::words
	@return the words of @a name that tell what it is: in lower case and
	without accents, three letters or more, and none of the words that only
	join others ("para", "com", "pour"...)
*/
QStringList NameSimilarity::words(const QString &name)
{
	QStringList result;
	QString word;
	auto close = [&result, &word]() {
		if (word.size() >= 3 && !linkingWords().contains(word) && !result.contains(word)) {
			result << word;
		}
		word.clear();
	};
	for (const QChar c : plain(name))
	{
		if (c.isLetterOrNumber()) {
			word.append(c);
		} else {
			close();
		}
	}
	close();
	return result;
}

/**
	@brief NameSimilarity::matchedWords
	@return for each of @a query_words (from words()), whether @a name has it
*/
QVector<bool> NameSimilarity::matchedWords(const QStringList &query_words, const QString &name)
{
	const QStringList name_words = words(name);
	QVector<bool> matched(query_words.size(), false);
	for (int i = 0 ; i < query_words.size() ; ++i)
	{
		for (const QString &name_word : name_words)
		{
			if (sameWord(query_words.at(i), name_word)) {
				matched[i] = true;
				break;
			}
		}
	}
	return matched;
}

/**
	@brief NameSimilarity::sharedWords
	@return how many of @a query_words (from words()) @a name has
*/
int NameSimilarity::sharedWords(const QStringList &query_words, const QString &name)
{
	const QVector<bool> matched = matchedWords(query_words, name);
	return static_cast<int>(std::count(matched.cbegin(), matched.cend(), true));
}

/**
	@brief NameSimilarity::rarityScore
	How much the words a name shares with the search say: a word in many
	names says little, a word in few says a lot. Among the look-alikes of
	"Borne de combinacao", "Terminal de combinação" shares the rare word and
	"Borne de aterramento" the common one, and the first is the one wanted.
	@param matched : for each word of the search, whether the name has it
	(matchedWords())
	@param names_with_word : for each word of the search, how many names have it
	@param name_count : how many names there are
	@return the sum, over the words the name has, of log(1 + names / names
	with that word)
*/
double NameSimilarity::rarityScore(const QVector<bool> &matched,
				   const QVector<int> &names_with_word, int name_count)
{
	double score = 0;
	for (int i = 0 ; i < matched.size() && i < names_with_word.size() ; ++i) {
		if (matched.at(i)) {
			score += std::log(1.0 + double(qMax(1, name_count)) / qMax(1, names_with_word.at(i)));
		}
	}
	return score;
}
