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
#ifndef IOCIRCUIT_H
#define IOCIRCUIT_H

#include "iolist.h"

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class CircuitTable;

/**
	@brief Turning the assigned I/O points of a card into a table of
	circuits to draw.

	An imported point says two things nobody has drawn yet: what is on the
	other end of the wire - IoPoint::connect_to, the "connect to" column of
	the sheet - and whether it reaches the field through a terminal -
	IoPoint::needs_terminal. Ninety-six points is ninety-six stretches of
	schematic, and drawing them by hand is the work this task exists to
	remove.

	The stretch itself is not invented here. T09 gives the library of
	.qetmak groupings, T05 the variables they declare, T08 the table that
	says one row is one circuit and the generator that draws a whole table
	in one undoable step. What was missing is the step in between: a point
	is not a row. This class is that step, and nothing else - it reads a
	list of points and writes a CircuitTable.

	Which means it is all plain values, and provable on a bench: no
	project, no folio, no element. What holds the project, runs the
	generator and links what it drew to the card is IoDrawing.

	A macro takes what it declares. The six names below are what a point
	can fill in, and a macro that declares none of them still draws - it
	just draws the same thing every time. The names are matched with case,
	accents and double spaces folded away, and with a small list of
	synonyms each, because the person who writes the macro and the person
	who writes this class are not the same person. The match is on the
	whole name and never on a prefix: a library where MARCACAO_DISJUNTOR
	and MARCACAO_RELE both exist would get the same tag written into both,
	and a wrong tag on the folio costs more than an empty one.

	What "connect to" has to name is a .qetmak, and not a bare symbol. It
	is not a limitation of the generator - it is what the requirement
	itself asks for: the stretch has to arrive with its insertion point
	already defined, so that it lands on the channel with no adjusting.
	A lone .elmt has no such point, and wrapping it in a macro is one
	command in the folio.
*/
class IoCircuit
{
	Q_DECLARE_TR_FUNCTIONS(IoCircuit)

	public:
		/// why one point draws nothing
		enum Refusal {
			NoRefusal,
			/// no point of the list carries that id
			PointNotFound,
			/// the point is not in a card yet, so there is no channel
			NotAssigned,
			/// the point names nothing to connect to
			NothingToDraw,
			/// what it names is not a .qetmak, so it has no insertion point
			NotAMacro
		};

		/**
			@brief One point, and the circuit it is going to become.
		*/
		struct Job
		{
			Job() {}

			/// the id of the point, which is what never changes
			QString point_id;
			/// how the sheet names it, for the messages
			QString label;
			/// the .qetmak the "connect to" column named
			QString macro_path;
			/// the row of the card table the point took
			int io_index = -1;
			/// true when the point has to reach the field through a terminal
			bool needs_terminal = false;
			/// what the point can fill in, by the names of valueKeys()
			QHash<QString, QString> values;
			/// id of the table row fill() made for it, empty until then
			QString row_id;
		};

		/**
			@brief One point that draws nothing, and why.
		*/
		struct Rejected
		{
			Rejected() {}
			Rejected(const QString &point_id, const QString &label,
				 Refusal reason) :
				point_id(point_id),
				label(label),
				reason(reason) {}

			QString point_id;
			/// how the sheet names it, so the message can say it out loud
			QString label;
			Refusal reason = NoRefusal;
		};

		/**
			@brief What a generation would draw, before it draws it.
		*/
		struct Plan
		{
			/// the points that become circuits, in the order they were given
			QList<Job> jobs;
			/// and the ones that do not, each with its reason
			QList<Rejected> rejected;

			bool isEmpty() const {return jobs.isEmpty();}
			/// @return true when every point asked for draws something
			bool isClean() const {return rejected.isEmpty();}
			/// @return the macros this plan needs, each once, in order
			QStringList macroPaths() const;
			/// @return how many of the circuits carry a field terminal
			int terminals() const;
			/// @return the whole thing said in one paragraph
			QString text() const;
		};

		/// @return true when @a connect_to names a macro file
		static bool isMacroPath(const QString &connect_to);

		/// @return the names a macro can declare to be filled from a point
		static QStringList valueKeys();
		/// @return the spellings @a key answers to, @a key included
		static QStringList aliasesOf(const QString &key);
		/**
			@brief Which of valueKeys() a declared parameter is asking for.
			@param column the name the macro declared
			@return the key, empty when the macro means something else
		*/
		static QString keyForColumn(const QString &column);

		/**
			@brief What @a point has to say, by key.
			@param point
			@param card_label how the card names itself, for the CARTAO key
		*/
		static QHash<QString, QString> valuesOf(const IoPoint &point,
							const QString &card_label);

		/**
			@brief Work out what @a point_ids would draw.

			Nothing here remembers that a point was drawn once already:
			the caller draws what it has just assigned, and a point is
			assigned once. Asking twice for the same point draws it twice.

			@param list the list of the project
			@param point_ids the points asked for
			@param card_label how the card names itself
			@return the circuits, and the points that stay undrawn
		*/
		static Plan plan(const IoList &list,
				 const QStringList &point_ids,
				 const QString &card_label = QString());

		/**
			@brief Write @a plan into @a table, one row per job.
			@param table a table that already knows the parameters of every
			macro the plan names - a macro whose parameters are missing
			gets a row with no values, which is a circuit drawn with its
			defaults and not a circuit refused
			@param plan the jobs get their row_id written back into them
			@param problems collects the cells the table would not take
			@return how many cells were written
		*/
		static int fill(CircuitTable &table, Plan &plan,
				QStringList *problems = nullptr);

		/// @return @a reason said out loud, about @a label
		static QString refusalText(Refusal reason, const QString &label);
};

#endif // IOCIRCUIT_H
