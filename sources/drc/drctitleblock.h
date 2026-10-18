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
#ifndef DRCTITLEBLOCK_H
#define DRCTITLEBLOCK_H

#include <QString>
#include <QStringList>
#include <QVariant>

class DiagramContext;

/**
	@brief Which fields of the title block of one folio are left blank, and
	the sentence a folio with any of them is reported with.

	The half of the title block rule that needs no project: it is handed
	the context the title block of one folio is drawn from, and the list of
	fields that must be filled in, and it answers from those two alone.
	Which folios there are, and where each one's context comes from, is the
	other half, and it lives in DrcTitleBlockRules.

	@par The context, and not the fields of the folio one by one
	What is read is the DiagramContext the title block renderer draws the
	cells with - BorderTitleBlock::titleblockInformation(), which is also
	what the diagram_info table of the project data base is filled from.
	For the historical fields the two readings agree: they are written
	into the context last, and with no test for emptiness, so a blank
	title on the folio is a blank title in the context however the
	project-wide variables are set, and a blank cell on the printed sheet.
	For a field of the user's own, the context is the only right reading: a
	blank value on the folio leaves a project-wide value of the same name
	in place, and that value is what the sheet prints.

	@par Blank is what prints as nothing
	A value made only of spacing - a space, a tab, a no-break space pasted
	from a spreadsheet - draws an empty cell, so it is blank here; and so
	is a field the context does not hold at all, since a variable nobody
	gave a value to renders as nothing too.

	@par The list of fields is handed in, never written here
	Which fields make a folio "filled in" is a choice of whoever delivers
	the project, and the factory one is only a default: see
	factoryRequiredKeys().
*/
namespace DrcTitleBlock
{
	/**
		The fields a folio of the factory profile must have filled in:
		the title and the author.

		Of the keys of QETInformation::diagramInfoKeys(), three are filled
		in by the program (the file name, the folio number and the folio
		label), two only mean something in a project that uses the plant
		and location structure, the revision index only exists once the
		project has been revised, and whether the date arrives filled in by
		itself has not been measured - it is the first candidate to be
		added, and adding it is a matter of handing a longer list.
	*/
	QStringList factoryRequiredKeys();

	/// Whether @a value prints as nothing: empty, or made only of spacing.
	bool isBlank(const QVariant &value);

	/**
		The fields of @a required that are blank in @a context.

		@param context what the title block of one folio is drawn from
		@param required the fields that must be filled in
		@return the blank ones, in the order of @a required, each one
		once. An empty name in @a required is not a field and is skipped.
	*/
	QStringList emptyKeys(const DiagramContext &context,
			      const QStringList &required);

	/**
		The name a field is given in a sentence.

		The name the program already shows the field under, so that the
		reader finds it in the folio properties by the word the finding
		used; and the key itself for a field of the user's own, which has
		no other name.
	*/
	QString fieldName(const QString &key);

	/**
		The sentence of a finding about a folio whose fields @a empty_keys
		are blank.

		@return an empty string when @a empty_keys is empty - a folio with
		nothing missing is not what this sentence reports. Otherwise every
		blank field, by name and in the order given, and never "incomplete
		title block": a finding that does not say which field to fill in
		comes back as a question.
	*/
	QString emptyFieldsText(const QStringList &empty_keys);
}

#endif // DRCTITLEBLOCK_H
