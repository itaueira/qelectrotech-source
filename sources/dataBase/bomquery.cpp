/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/
#include "bomquery.h"

#include "../qetinformation.h"

/**
	@brief QETBom::informationPresentCondition
	See the header for the rule and for why the flag is not part of it.
*/
QString QETBom::informationPresentCondition(const QString &prefix,
					    const QStringList &keys)
{
		//The flag, by the only name it has. It is a literal in
		//qetinformation.cpp too - the list that creates the columns of
		//element_info spells it out rather than naming a constant - and
		//this is the second place that has to spell it. Named here so
		//that a reader of the condition can see which key is missing
		//from it and why, instead of counting columns.
	static const QString flag = QStringLiteral("exclude_from_bom");

	QStringList terms;
	for (const QString &key : keys)
	{
		if (key == flag) {
			continue;
		}

			//COALESCE and not "IS NOT NULL", because the two empty
			//forms both occur and mean the same thing to a reader. A
			//key the component never carried is bound as a null string
			//and stored as NULL; a key it carries with nothing typed
			//into it is stored as the empty string. A condition testing
			//only one of the two lets half of the nameless rows through,
			//and which half depends on how the component was drawn.
		terms << QStringLiteral("COALESCE(") + prefix + key
			 + QStringLiteral(",'') <> ''");
	}

		//Always true, deliberately. See the header: a schema with no
		//information column at all has stated nothing, and a filter that
		//emptied every list on that ground would be worse than no filter.
	if (terms.isEmpty()) {
		return QStringLiteral("(1)");
	}

	return QStringLiteral("(")
			+ terms.join(QStringLiteral(" OR "))
			+ QStringLiteral(")");
}

/**
	@brief QETBom::groupByColumns
	See the header.
*/
QStringList QETBom::groupByColumns(const QStringList &published)
{
	QStringList columns;
	columns << QETInformation::ELMT_PART_CODE
		<< QETInformation::ELMT_PART_REVISION;

	for (const QString &column : published)
	{
		if (!columns.contains(column)) {
			columns << column;
		}
	}

	return columns;
}

/**
	@brief QETBom::publishedColumns
	See the header.
*/
QStringList QETBom::publishedColumns(const QString &query)
{
	const QString marker = QStringLiteral(" ORDER BY ");
		//auto and not int: the index is qsizetype in Qt6 and int in
		//Qt5, and this file has to compile under both.
	const auto at = query.lastIndexOf(marker);
	if (at < 0) {
		return QStringList();
	}

	QStringList columns;
	const QStringList parts =
		query.mid(at + marker.size()).split(QLatin1Char(','));
	for (const QString &part : parts)
	{
		const QString column = part.trimmed();
			//A column name and nothing else. Anything holding a space
			//or a parenthesis is a function, an alias or a sort
			//direction, and none of those belong in a GROUP BY written
			//from here.
		if (column.isEmpty()
		    || column.contains(QLatin1Char(' '))
		    || column.contains(QLatin1Char('('))) {
			return QStringList();
		}
		columns << column;
	}
	return columns;
}

/**
	@brief QETBom::groupBy
	See the header.
*/
QString QETBom::groupBy(const QString &query)
{
	return groupByColumns(publishedColumns(query))
			.join(QStringLiteral(", "));
}
