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
#ifndef LABELENTRY_H
#define LABELENTRY_H

#include <QString>
#include <QStringList>
#include <QVector>

/**
	@brief What a label was made from.

	Kept on the entry rather than on the list, because one export can mix
	them: a cabinet is marked in one pass, and whoever holds the roll wants
	the terminals and the components of that cabinet in the same run.
*/
enum class LabelSource
{
	Component,   ///< A device drawn on a sheet.
	Terminal,    ///< A terminal of a terminal strip.
	Wire,        ///< A conductor.
	Cable,       ///< A cable, or one of its wires.
	CabinetDoor  ///< The door plate of an enclosure.
};

/**
	@brief One label: what goes on one piece of tape, and where it came from.

	This is the whole contract between the collectors and the writers. A
	collector fills it from wherever its data lives - the live terminal strip
	for a terminal, the project data base for a component - and a writer
	turns it into a file, a sheet of adhesive labels or whatever comes next.
	Neither knows about the other, which is what stops each new output format
	from being a copy of the collector.

	Deliberately plain: no Qt object, no pointer into the project, nothing
	that has to be alive later. A list of these outlives the project it was
	read from, which is what lets a caller collect once and write twice.

	@par Why there is more here than a text
	The obvious shape - one string per label - loses the two things whoever
	is holding the roll needs to check it: which sheet the item is on, and at
	which revision of that sheet it was read. Both are per row and neither
	can be recovered afterwards from the text alone.
*/
struct LabelEntry
{
	/// The text that identifies the item. The one line that must be legible.
	QString primary_text;
	/**
		The lines under it, in the order they should be printed. May be
		empty: a terminal label is very often a number and nothing else.
	*/
	QStringList secondary_texts;
	/// Which collector produced this entry.
	LabelSource source = LabelSource::Component;
	/**
		The location of the item, as it was stored.

		Not as it should be written: turning a stored location path into the
		mark the standard uses belongs to the step that applies
		QETInformation::displayedInfoValue(), and doing it here would put a
		second, silent implementation of that rule in the code. Held apart
		from secondary_texts for that reason - it is the one field that is
		not yet in its final written form.
	*/
	QString location;
	/// The folio number of the sheet the item is drawn on, as the sheet says it.
	QString folio;
	/**
		The position of that sheet in the project, counting from 1.

		This, and not @c folio, is what a revision is looked up by. The folio
		number is a free text field the draughtsman fills in - two sheets may
		carry the same one, and a project may leave them all empty - while
		the position is the sheet's rank in the project and is unique by
		construction.
	*/
	int folio_position = -1;
	/**
		The revision index of that sheet, empty when the sheet carries none.

		Per sheet, never per project: the file format has no project-wide
		revision, so a single value for a whole run would be a number the
		project does not hold. A strip crossing two sheets therefore prints
		two revisions, which is correct and will look wrong.
	*/
	QString revision;
	/**
		Whether the user asked to keep this item out of the bill of
		materials.

		Carried, not obeyed. The box means "not on the purchase list", never
		"not in the cabinet", so an excluded item is labelled like any other.
		It is on the entry so that a writer can show the distinction, and so
		that the fact can be checked on the list itself rather than by
		running a second query.
	*/
	bool excluded_from_bom = false;
};

using LabelEntryList = QVector<LabelEntry>;

#endif // LABELENTRY_H
