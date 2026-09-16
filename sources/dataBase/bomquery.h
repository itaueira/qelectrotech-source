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
#ifndef BOMQUERY_H
#define BOMQUERY_H

#include <QString>
#include <QStringList>

/**
	@brief The rules a bill of materials is made of, as text.

	Three questions, and none of them needs a project, a data base or a
	window to answer : which rows of the element tables name something,
	what a purchase list has to group by, and which columns a query built
	by ElementQueryWidget publishes. They are here, together and pure,
	because each of them was written once inside a caller and then needed
	by a second one.

	The second caller is the command line. @c --export-bom and the export
	window are two roads to the same list, and they disagreed : the window
	grouped the rows and counted them, while the command line published
	the @c quantity column - a property the designer types, empty in every
	row of every project where nobody types it. One road answered a count
	and the other answered nothing, for the same project, on the same day.
	Sharing the rule rather than writing it twice is what keeps them from
	drifting apart again.
*/
namespace QETBom
{
		/**
			@brief The SQL condition that is true of a row naming something.
			@param prefix : what to write before each column name, table
			alias and dot included ("ei." over the element_info table), or
			an empty string when the columns are selected from a view
			@param keys : the element information keys, as
			QETInformation::elementInfoKeys() gives them
			@return a parenthesised condition, without a leading operator

			A row of the parts list that holds nothing in any of its
			information columns names no component : no label to find it
			by, no designation, no manufacturer, no reference, no part
			code. Nobody can order from it and nobody can mark it.

			@c exclude_from_bom never counts as information, and leaving
			it in would be the silent end of this whole rule. It is in
			QETInformation::elementInfoKeys() because the element_info
			table is built from that list, but it is a flag about this
			very list rather than a property of the component - it has a
			check box of its own, and elementinfowidget.cpp drops it from
			its rows for the same reason. It is also stored as the literal
			"false" on every component whose box was ever ticked and
			unticked, so a condition counting it would answer true for
			those rows and for no others : the filter would work on a
			project and stop working on the next one, and nothing would
			say why.

			An empty @p keys gives back a condition that is always true,
			and that is the opposite of what elementTypeClause() does with
			an empty set of kinds. The two empty sets are not the same
			question. There, a caller asking for no kind of element has
			stated that it wants nothing; here, a schema offering no
			information column has stated nothing at all, and a filter
			that emptied every list in the program on that ground would be
			the worst failure available.
		*/
	QString informationPresentCondition(const QString &prefix,
					    const QStringList &keys);

		/**
			@brief What a purchase list has to group by.
			@param published : the columns the list shows, in order
			@return the columns of the GROUP BY, in order

			The two columns that identify a part - its code and the
			revision of that code - plus every published column that is
			not summed.

			The part identity is there because grouping by the designation
			merges two different parts described with the same words, and
			whoever reads the purchase list has no way of seeing that it
			happened : one line, one quantity, one of the two codes, and
			the wrong item ordered. It cuts the other way too - the same
			part described twice was split into two lines that are bought
			twice.

			The rest of the published columns are there because a column
			that is neither grouped nor aggregated is answered by SQLite
			from whichever row of the group it likes, so a quantity that
			is right can sit beside a manufacturer reference belonging to
			another item of the same group. Grouping by all of them makes
			that unrepresentable, which is how the list by location
			already does it.

			@par The component with no part assigned
			Nothing special, and that is the decision rather than an
			oversight. Its part code is empty or null, and SQLite groups
			nulls together, so such components fall into one bucket per
			distinct set of published columns - which for a purchase list
			means grouped by their designation, because with no code the
			designation is the only identity they have.

			@par What this can and cannot do to a list that exists today
			It only ever splits. Every column that decided a group before
			still decides it, and two were added, so two rows that are
			separate today can never merge; rows that were merged and
			should not have been come apart. The sum of the quantity
			column over the whole list is therefore unchanged - it is
			still one per component - and only the number of lines moves.
		*/
	QStringList groupByColumns(const QStringList &published);

		/**
			@brief The columns a query built by ElementQueryWidget shows.
			@param query : the finished query string
			@return the columns, in order, or an empty list

			Read back from the tail of the query rather than asked of the
			widget, and the reason is that the widget does not offer them:
			the list of chosen keys is a private slot of
			ElementQueryWidget, and its callers only ever receive the
			finished string. The tail is the safe end to read it from -
			ElementQueryWidget writes the very same comma separated list
			twice, once after SELECT and once after ORDER BY, but the
			first copy carries the count column and its alias, and the
			second carries nothing but the keys.

			An empty list comes back for the cases that must not be
			rewritten from outside : a query the user typed himself, which
			the widget hands over whole and in which no GROUP BY of ours
			takes part, and anything whose shape is not the one described
			above. The caller falls back on the part identity alone, which
			is still an answer.
		*/
	QStringList publishedColumns(const QString &query);

		/**
			@brief What a purchase list built by ElementQueryWidget has to
			group by.
			@param query : the query the widget has just built
			@return the GROUP BY clause, without the words "GROUP BY"

			groupByColumns() over publishedColumns(), joined. The two
			halves are separate because the command line has its columns
			in hand and has no query to read them back from.
		*/
	QString groupBy(const QString &query);
}

#endif // BOMQUERY_H
