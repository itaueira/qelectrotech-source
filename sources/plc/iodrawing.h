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
#ifndef IODRAWING_H
#define IODRAWING_H

#include "iocircuit.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

class CircuitTable;
class Element;
class QETProject;
class TerminalStrip;

/**
	@brief Draw the circuits an IoCircuit::Plan asks for, and tie each one
	to the channel it belongs to.

	This is the half of "connect to" that cannot exist without a project.
	IoCircuit did the part that can be tested on a bench - which points draw,
	from which macro, with which values - and left a table's worth of work
	behind. Here that work becomes folios.

	Nothing is drawn twice by accident and nothing is drawn from scratch:
	the generation is CircuitGenerator's, the same one the table of T08
	uses, so a circuit generated from an I/O point and a circuit generated
	from a hand written row are the same object, laid out by the same rules
	and undone by the same command.

	Three things happen that the table alone would not do:

	- the slave the macro drew is linked to the channel of the card, which
	  is what makes the address, the function text and the comment appear
	  on the folio without anybody typing them, and what gives the card its
	  cross reference;
	- the field terminal the macro drew, when the point asked for one, is
	  put into a terminal strip, so that the strip editor already has it;
	- all of it, generation included, sits under one entry of the undo
	  stack. Twelve circuits a person does not like is one Ctrl+Z.

	Linking is what upstream already does when somebody drags a slave onto
	a master by hand: LinkElementCommand reads the channel of the card and
	pushes its values onto the slave. Doing it from here only spares the
	dragging - the result is the file upstream would have written.
*/
class IoDrawing
{
	Q_DECLARE_TR_FUNCTIONS(IoDrawing)

	public:
			/**
				@brief What one drawing did.
			*/
		struct Report
		{
			Report() {}

				/// circuits drawn
			int drawn = 0;
				/// folios made for them
			int sheets = 0;
				/// slaves tied to a channel of the card
			int linked = 0;
				/// field terminals put into a strip
			int terminals = 0;
				/// one line per thing that did not happen, and why
			QStringList problems;

				/// @return the whole thing in one paragraph
			QString text() const;
		};

		explicit IoDrawing(QETProject *project);

			/**
				Draw @a plan.
				@param plan what IoCircuit::plan() decided, jobs only
				@param master the card the points sit in, null to draw
				without linking anything
				@param sheet_title written on the folios made, empty for
				the default
				@return what was drawn, linked and bound
			*/
		Report draw(const IoCircuit::Plan &plan, Element *master,
			    const QString &sheet_title = QString());

			/**
				@param master the card, may be null
				@return how the strip of the field terminals of @a master
				names itself
			*/
		static QString stripNameFor(Element *master);

	private:
			/// @return false when not one macro of @a plan could be opened
		bool loadMacros(const IoCircuit::Plan &plan, CircuitTable &table,
				QStringList *problems) const;
			/// @return the elements of the project whose uuid is in @a uuids
		QList<Element *> elementsOf(const QStringList &uuids) const;
			/// @return how many slaves of @a elements were tied to @a io_index
		int link(Element *master, const QList<Element *> &elements,
			 int io_index);
			/// @return the terminals @a elements carries, free ones only
		static QList<Element *> terminalsOf(const QList<Element *> &elements);
			/// @return how many of @a terminals went into @a strip
		int bind(const QList<Element *> &terminals, TerminalStrip *strip);
			/// @return the strip of the field terminals of @a master, made if needed
		TerminalStrip *stripFor(Element *master);

		QETProject *m_project = nullptr;
};

#endif // IODRAWING_H
