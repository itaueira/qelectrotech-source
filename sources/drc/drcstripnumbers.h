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
#ifndef DRCSTRIPNUMBERS_H
#define DRCSTRIPNUMBERS_H

#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief When two terminals of one strip carry the same number, what a
	strip is called in a sentence, and the sentences of the strip rules.

	The half of the strip rules that needs no project: it is handed numbers,
	places on the rail, folios and cells, and the three fields of a strip's
	name, and it answers from that alone. Walking the strips of a project and
	reading the number each terminal draws is the other half, and it lives
	in DrcStripRules, because only the project holds the strips.

	@par The number compared is the one the plan draws
	It is handed in already cut: what follows the last ':' of the tag, as
	the plan of the strip writes it under each terminal - so "X1:3" and "3"
	are both 3, because both are drawn 3. The cut is not repeated here: it
	is the drawer's, and a second copy of it would be a second opinion on
	what the plan shows.

	@par What counts as the same number
	Trimmed and with the case folded, the rule DrcDuplicateTags holds for a
	tag: " 3" and "3" are one number, and so are "PE" and "pe". A space in
	the middle and a leading zero are kept - "03" is not "3" - because both
	are what somebody wrote.

	@par No number is not a number
	A terminal whose number is empty, or only spacing, is left out: two
	terminals with no number are not two terminals sharing one.

	@par The levels of one terminal are one terminal
	A physical terminal of two levels may carry the same number on both,
	and that is one piece with one number, not a repetition. Two marks with
	the same physical place on the rail are levels of one terminal; a group
	counts each terminal once, by its first level carrying that number.

	@par One finding per number, not per pair
	Three terminals numbered 3 in one strip are one finding naming the
	three places, as three components on one tag are.
*/
namespace DrcStripNumbers
{
	/**
		@brief One level of a terminal of a strip, as far as this rule sees it.
	*/
	struct Mark
	{
		/**
			The physical terminal the level belongs to: its place on the
			rail, PhysicalTerminal::pos() - the number the Position column
			of the strip editor shows, counted from 0. Two marks with the
			same value are two levels of one terminal.
		*/
		int physical = -1;
		/// The number, as the plan of the strip draws it.
		QString number;
		/// The number printed on the sheet the symbol is drawn on, 1-based; 0 when unknown.
		int folio = 0;
		/// The cell of the grid of that sheet, e.g. "B2". May be empty.
		QString cell;
	};

	/**
		What makes two numbers the same: trimmed, case folded.
		@return an empty string for a number that is empty or only spacing,
		which takes part in no comparison.
	*/
	QString key(const QString &number);

	/**
		The numbers carried by more than one terminal of @a marks.

		@param marks the levels of one strip, in the order of its rail
		@return one list per such number, holding the index in @a marks of
		the first level carrying it on each terminal, in the order of
		@a marks. The lists come in the order their first member appears, so
		the first member of each is the terminal nearest the start of the
		rail. A number carried by the levels of one terminal only is in none
		of them, and nor are marks whose key() is empty.
	*/
	QList<QList<int>> groups(const QList<Mark> &marks);

	/**
		The name of a strip, as a sentence says it.

		The three fields the header of the plan draws, installation,
		location and name, trimmed and joined by a space, leaving out an
		installation or a location that is empty. A strip with no name of
		its own says so in words, since a sentence ending in "= +" reads as
		cut short.
	*/
	QString stripName(const QString &installation,
			  const QString &location,
			  const QString &name);

	/**
		What makes two strips have the same name: the three fields, each
		trimmed and case folded, compared field by field.

		@return an empty string for a strip whose own name is empty: two
		strips nobody named are not two strips sharing a name.
	*/
	QString stripKey(const QString &installation,
			 const QString &location,
			 const QString &name);

	/**
		The strips that share a name.

		@param keys one stripKey() per strip, in the order of the project
		@return one list per key carried more than once, holding the indexes
		in @a keys, in order; the lists in the order their first member
		appears. Empty keys are in none of them.
	*/
	QList<QList<int>> sameNameGroups(const QStringList &keys);

	/**
		The sentence of a finding about the terminals of @a group.

		@param strip_name the strip, as stripName() says it
		@param group the marks of one number, one per terminal, as groups()
		gathers them
		@return an empty string when @a group holds fewer than two marks, a
		mark with no number, marks whose key() differ, or two marks of one
		terminal: there is nothing to report then. Otherwise the number, how
		many terminals carry it, the strip, and the place on the rail, the
		folio and the cell of each of them.

		The sentence names places, because the finding points at one
		terminal and the panel has one folio column: the others could not
		be found from the panel without it.
	*/
	QString duplicateText(const QString &strip_name, const QList<Mark> &group);

	/**
		The sentence of a finding about a strip with no terminal.
		@return an empty string when @a strip_name is empty.
	*/
	QString emptyStripText(const QString &strip_name);

	/**
		The sentence of a finding about @a count strips sharing one name.
		@return an empty string when @a count is below two or @a strip_name
		is empty.
	*/
	QString sameNameText(const QString &strip_name, int count);
}

#endif // DRCSTRIPNUMBERS_H
