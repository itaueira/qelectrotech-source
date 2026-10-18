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
#ifndef ASSEMBLYCHANGES_H
#define ASSEMBLYCHANGES_H

#include "assemblystate.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

/// Which half of the photograph an item belongs to.
enum class AssemblyItemKind
{
	Component, ///< a part that carries a tag
	Wire       ///< a conductor, numbered on its ferrules
};

/// What the shop has to do about one item of the panel.
enum class AssemblyAction
{
	Add,     ///< drawn now, absent from the photograph: mount it
	Remove,  ///< in the photograph, drawn nowhere now: take it out
	Relabel  ///< in both, carrying another label now: print a new one
};

/**
	@brief One item of the drawing as it is now, handed in by whoever walked
	the project.

	Data and nothing else, so that the comparison below never meets an
	Element or a Conductor: the walk that fills this needs a project open,
	the comparison does not, and it is the comparison that can be got wrong.
*/
struct AssemblyItem
{
		/// The identity the photograph names it by.
	QString uuid;
		/// The tag of a component, the text of a wire - read from the very
		/// field the photograph read it from, or every item reads as
		/// relabelled.
	QString label;
		/// The folio it is drawn on, counted from 1; 0 when not known.
	int folio = 0;
		/// The cell of the folio grid it stands in.
	QString position;
		/// What it is: the designation of a component, the two ends of a wire.
	QString description;
		/// The product code of a component, empty for a wire.
	QString part_code;
};

/**
	@brief One line of the order of service: an item and what to do about it.
*/
struct AssemblyChange
{
	AssemblyItemKind kind = AssemblyItemKind::Component;
	AssemblyAction action = AssemblyAction::Add;
	QString uuid;
		/// The label the photograph holds; empty for an addition.
	QString old_label;
		/// The label the drawing carries now; empty for a removal.
	QString new_label;
		/// Where it is drawn now: 0, and an empty position, for a removal,
		/// which is drawn nowhere any more.
	int folio = 0;
	QString position;
	QString description;
	QString part_code;
		/**
			The index, in the list of items handed to compare(), of the item
			this line was made from; -1 for a removal.

			It is how a window goes from the line back to the object on the
			folio without this file knowing what an object is: whoever
			walked the project keeps the objects in the same order as the
			items, and the index points into both.
		*/
	int source = -1;
};

/**
	@brief The three lists of one kind of item.
*/
struct AssemblyChangeLists
{
	QList<AssemblyChange> to_add;
	QList<AssemblyChange> to_remove;
	QList<AssemblyChange> to_relabel;

	int count() const;
	bool isEmpty() const;
		/// to_add, to_remove and to_relabel, joined in that order.
	QList<AssemblyChange> all() const;
};

/**
	@brief What changed on the drawing since the panel was marked as wired.

	@par What it is for
	The order of service of a modification to a panel already on the bench:
	what to mount, what to take out, what to label again. The shop has to be
	able to carry it out reading this alone, without laying two drawings
	side by side (CU-29.4), which is why each line says where the item is
	drawn and what it is, and not only its tag.

	@par Where the answer comes from
	The photograph AssemblyState took when the project was marked: a uuid
	per component and per wire, with the label each carried. An item drawn
	now whose uuid the photograph does not hold was drawn afterwards; a uuid
	the photograph holds and the drawing does not was deleted afterwards; a
	uuid in both whose label differs was relabelled afterwards. That last
	question is the one the labels were kept in the photograph for - see the
	note on AssemblyState.

	@par An unmarked project has no "since"
	The stage decides it, exactly as it decides the freezing
	(AssemblyState::freezesComponent): a photograph left over from a marking
	that was taken back compares against nothing, and the lists come back
	empty. isMarked() is what tells that apart from a marked project in
	which nothing changed - an empty list means "nothing to do" only when
	there was something to compare with.

	@par Nothing is dropped
	An item drawn twice under one uuid - a copy, for the moment a paste
	takes to give it an identity of its own - is two pieces on the panel:
	the first is the one the photograph knew, the second is listed as one
	to add. An item with no uuid at all cannot be in the photograph, which
	never records one (AssemblyStateCommand::photograph skips it), and the
	renumbering therefore treats it as new: it is listed as one to add too,
	rather than left out of an order of service that would then be silently
	short.

	@par The order of the lines
	Fixed here, and not left to whoever hands the items in: the walk of a
	project follows the order of the scene, which is not the same from one
	opening to the next, and two exports of the same modification that
	disagree on their order cannot be compared line by line. Additions and
	relabellings go by folio, then by the label the shop finds on the panel
	(the old one when there is one), then by what the item is; removals,
	which are drawn nowhere, by the label they carried. Labels are compared
	as text, case aside: K10 comes before K2. The natural order the rest of
	the program uses (QETUtils::naturalLessThan) lives in a file that pulls
	the application in, and this one stays free of it.

	@par The file for the shop
	A delimited text in the shape of the label export (LabelCsvWriter):
	';' between the cells, quoted only where needed by QETCsv, CRLF between
	the rows, UTF-8 behind a byte order mark, and a first row of fixed keys
	that are not translated, because the file is read back - by a
	spreadsheet, by a comparison of two exports - and a header in the
	language of the machine that wrote it cannot be. The rows are all(), in
	that order.
*/
class AssemblyChanges
{
	public:
			/// The stage of the photograph the drawing was compared with.
		AssemblyStage stage = AssemblyStage::InProject;
		AssemblyChangeLists components;
		AssemblyChangeLists wires;

			/// Whether there was a photograph to compare with at all.
		bool isMarked() const;
			/// Whether nothing at all is to be done.
		bool isEmpty() const;
		int count() const;
			/// The components' three lists, then the wires', in that order.
		QList<AssemblyChange> all() const;

		/**
			@param photographed uuid -> label, one half of the photograph
			@param now the items of that kind drawn now
			@param kind what they are, written on each line
			@return the three lists
		*/
		static AssemblyChangeLists compareItems(
				const QMap<QString, QString> &photographed,
				const QList<AssemblyItem> &now,
				AssemblyItemKind kind);

		/**
			@param photograph the state the project was marked with
			@param components the components drawn now
			@param wires the wires drawn now
			@return the changes, empty and not marked when @a photograph
			is not at a frozen stage
		*/
		static AssemblyChanges compare(const AssemblyState &photograph,
					       const QList<AssemblyItem> &components,
					       const QList<AssemblyItem> &wires);

			/// The delimiter between the cells of a row: ';'.
		static QString separator();
		/**
			The first row of the file, one key per column: action, kind,
			old_label, new_label, folio, position, description, part_code.
		*/
		static QStringList csvHeader();
			/// The word the action column writes: add, remove or relabel.
		static QString actionKey(AssemblyAction action);
			/// The word the kind column writes: component or wire - the
			/// words the label export already writes for the same things.
		static QString kindKey(AssemblyItemKind kind);
			/// The cells of one row, unquoted, in the order of csvHeader().
		static QStringList csvCells(const AssemblyChange &change);

			/// The whole file as text, without the byte order mark.
		QString csvText() const;
			/// csvText() as the file holds it: UTF-8, behind a byte order mark.
		QByteArray csvBytes() const;
		/**
			Write the file at @a path, through QSaveFile: a write that fails
			part-way leaves the previous file where it was rather than half
			of a new one. An order of service cut short in silence is worse
			than none.

			@return an empty string when the file was written, else the
			reason, ready to be shown.
		*/
		QString writeCsv(const QString &path) const;
};

#endif // ASSEMBLYCHANGES_H
