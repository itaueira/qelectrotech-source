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
#ifndef NAMESIMILARITY_H
#define NAMESIMILARITY_H

#include <QString>
#include <QStringList>
#include <QVector>

/**
	@brief How alike two symbol names are, word by word.
	Used to list the symbols that can replace a selected one: the name of
	the selected symbol and the names of its look-alikes are often written
	differently - "Borne de combinacao" and "Terminal de combinação" - and
	searching the whole name, accents included, finds none of them.
	Pure on purpose: strings in, numbers out.
*/
namespace NameSimilarity
{
	QStringList words(const QString &name);
	QVector<bool> matchedWords(const QStringList &query_words, const QString &name);
	int sharedWords(const QStringList &query_words, const QString &name);
	double rarityScore(const QVector<bool> &matched, const QVector<int> &names_with_word,
			   int name_count);
}

#endif // NAMESIMILARITY_H
