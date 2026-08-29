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
#ifndef PINOUTBLOCKTEMPLATE_H
#define PINOUTBLOCKTEMPLATE_H

#include "../catalog/catalogpart.h"
#include "../qet.h"
#include "symbolbuilder.h"

#include <QList>
#include <QMap>
#include <QString>

/**
	@brief Which side of a block a kind of terminal goes to, house wide.

	The convention of the drawing office, and not a choice made on each
	block: in CEI, inputs on top and outputs at the bottom, the supply
	common on top and the return common at the bottom. Other conventions
	put inputs on the right and outputs on the left.

	The side is held as a Qet::Orientation and not as an enumeration of its
	own. A terminal drawn on the top edge of a block is a terminal that
	points north, which is what the element definition already writes and
	what SymbolTerminal already carries. A second enumeration would need a
	mapping table between the two, and a mapping table is a thing that
	drifts - the same reason SymbolTerminal reuses CatalogPinRole.

	The convention belongs to the environment (T38), so that a company
	that shares one folder shares one convention. A class may disagree
	about one kind of terminal, through PinoutBlockTemplate; it may not
	replace the convention.
*/
class PinoutConvention
{
	public:
		PinoutConvention() {}

		/// stable name of the convention, in ascii: iec, horizontal, custom
		QString key = QStringLiteral("iec");
		/// which way a terminal of each role points
		QMap<CatalogPinRole, Qet::Orientation> sides;

		bool declares(CatalogPinRole role) const;
		Qet::Orientation sideOf(CatalogPinRole role) const;
		bool isValid(QString *error = nullptr) const;

		QString toXml() const;
		static PinoutConvention fromXml(const QString &xml);

		/// inputs on top, outputs at the bottom
		static PinoutConvention iec();
		/// inputs on the right, outputs on the left
		static PinoutConvention horizontal();
		static QList<PinoutConvention> builtinConventions();
		static QString translatedName(const QString &key);
		/**
			@return every role a convention has to have an opinion about,
			which is every role but Unknown. A generator meeting a role
			nobody placed would have to invent a side, and inventing is
			what a convention exists to stop.
		*/
		static QList<CatalogPinRole> conventionalRoles();
		/// the side a terminal moves to when it is sent across the block
		static Qet::Orientation opposite(Qet::Orientation side);

		/// the convention in use: the configured one, or CEI
		static PinoutConvention current();
		/// true when someone chose a convention instead of taking CEI
		static bool isConfigured();
		static void setCurrent(const PinoutConvention &convention);
		static void clearCurrent();
};

/**
	@brief One deliberate disagreement between a class and the convention.

	A power supply has no "input on top, output at the bottom" in the sense
	an I/O card has, so a class has to be able to disagree. What it may not
	do is disagree about everything: that is a second convention wearing the
	clothes of an exception, and then nobody knows which one the company
	actually uses.

	The reason is required, and that is the whole mechanism of the decision:
	the exceptions of every class can be listed, each with the sentence that
	justifies it. An exception nobody can explain is one that was made by
	accident.
*/
class PinoutSideOverride
{
	public:
		PinoutSideOverride() {}
		PinoutSideOverride(CatalogPinRole role,
				   Qet::Orientation side,
				   const QString &reason) :
			role(role), side(side), reason(reason) {}

		CatalogPinRole role = CatalogPinRole::Unknown;
		Qet::Orientation side = Qet::North;
		/// why this class contradicts the convention
		QString reason;
};

/**
	@brief How a block of one class of component is laid out.

	Held on CatalogClass, in one column of serialised XML, the way
	numbering_format already is and for the reason recorded in T07: changing
	how a class is drawn has to change the next card of any project, and a
	rule that lives in a dialog changes nothing.

	Everything here is counted in **steps of the main grid**, never in
	millimetres. A conductor docks where the terminal is and the sheet moves
	conductors by whole grid steps, so a spacing of 3.5 mm is a block whose
	wires never quite touch. Asking for steps and multiplying by
	SymbolGrid::main_step is what makes the wrong answer unsayable.

	Editing the template changes the blocks generated from here on. The ones
	already inserted are files, not views of the template, so they keep the
	drawing they were born with - which is the behaviour the specification
	asks for, and which the dialog has to say out loud, or the right
	behaviour reads as a bug.
*/
class PinoutBlockTemplate
{
	public:
		PinoutBlockTemplate() {}

		/// true when nothing was declared: the convention, unchanged
		bool isNull() const;
		bool isValid(QString *error = nullptr) const;

		/// the width of the block, in steps of the main grid
		int width_steps = 6;
		/// the distance between two consecutive terminals, in steps
		int pitch_steps = 1;
		/// the steps left empty between the corner and the first terminal
		int margin_steps = 1;
		/**
			How many terminals one block carries before the pinout is
			split over several of them; 0 means no limit. This is what
			turns a card of 32 points into two blocks on two folios
			instead of one block taller than the sheet.
		*/
		int max_terminals = 0;

		/// what this class does differently from the convention, and why
		QList<PinoutSideOverride> side_overrides;

		bool overridesSide(CatalogPinRole role) const;
		void setOverride(CatalogPinRole role,
				 Qet::Orientation side,
				 const QString &reason);
		void clearOverride(CatalogPinRole role);
		/// the side of @a role: the exception of the class, or the convention
		Qet::Orientation sideOf(CatalogPinRole role,
					const PinoutConvention &convention) const;

			//The geometry, resolved against a grid. Nothing below returns
			//a value that is not a whole number of main grid steps.
		qreal width(const SymbolGrid &grid) const;
		qreal pitch(const SymbolGrid &grid) const;
		qreal margin(const SymbolGrid &grid) const;
		/// where the terminal number @a index of a side sits along it
		qreal offsetOf(int index, const SymbolGrid &grid) const;
		/// the length a side needs to carry @a count terminals
		qreal lengthFor(int count, const SymbolGrid &grid) const;
		/// how many blocks @a count terminals are split over
		int blocksFor(int count) const;

		QString toXml() const;
		static PinoutBlockTemplate fromXml(const QString &xml);
};

#endif // PINOUTBLOCKTEMPLATE_H
