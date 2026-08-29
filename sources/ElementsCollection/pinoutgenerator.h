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
#ifndef PINOUT_GENERATOR_H
#define PINOUT_GENERATOR_H

#include "../catalog/catalogpart.h"
#include "pinoutblocktemplate.h"
#include "symbolbuilder.h"

#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief Draws the symbol of a part from the pins the catalogue knows
	about.

	This is the third step of the pinout blocks, and the one that turns a
	list into a drawing. What comes in is a CatalogPart with its pins; what
	comes out is one SymbolDefinition per block, ready for the symbol
	writer that already exists.

	Three things decide the drawing, and none of them is here:

	@li the PinoutConvention says which side a role goes to, and lives in
	the shared environment, so a company sharing a folder shares a
	convention;
	@li the PinoutBlockTemplate says how wide, how far apart and how many
	fit, and lives on the class, so changing how a class is drawn changes
	the next card of every project;
	@li the SymbolGrid says what a length may be, and every length here
	goes through it.

	The generator holds a copy of the three and asks nothing else. It never
	touches the repository, never reads a setting and never writes a file,
	which is why the whole of it can be proved on a bench without a project
	being open.

	The class key is carried the same way and for the same reason. A symbol
	without a class is a symbol SymbolDefinition::problems() refuses, and
	asking the repository for the class of a part here would put the one
	dependency in that would make the rest untestable.
*/
class PinoutGenerator
{
	public:
		PinoutGenerator() {}
		PinoutGenerator(const PinoutBlockTemplate &block_template,
				const PinoutConvention &convention,
				const SymbolGrid &grid,
				const QString &class_key) :
			block_template(block_template),
			convention(convention),
			grid(grid),
			class_key(class_key) {}

		PinoutBlockTemplate block_template;
		PinoutConvention convention = PinoutConvention::iec();
		SymbolGrid grid;
		/// the class the generated blocks belong to
		QString class_key;

		bool isValid(QString *error = nullptr) const;

		/**
			@brief The pins of @a part that are to be drawn, in the
			order the catalogue declares them.

			An empty @a pin_labels means every pin: drawing the whole
			part is what the dialog asks for nine times out of ten,
			and having to name thirty two points to get thirty two
			points would be a way of getting thirty one.
		*/
		QList<CatalogPin> selectedPins(
				const CatalogPart &part,
				const QStringList &pin_labels = QStringList()) const;

		/**
			@brief The same pins, cut into the blocks they will be
			drawn as.

			The cut respects two things that may not be separated: a
			channel, because an input and the common that returns it
			are one point of the field, and a pair, because half a
			contact is a symbol that cannot be saved.
		*/
		QList<QList<CatalogPin> > split(
				const CatalogPart &part,
				const QStringList &pin_labels = QStringList()) const;

		QList<SymbolDefinition> generate(
				const CatalogPart &part,
				const QStringList &pin_labels = QStringList()) const;

		SymbolDefinition generateOne(const CatalogPart &part,
					     const QList<CatalogPin> &pins,
					     int index, int total) const;

		/**
			@brief What a block is called: the code of the part, and
			which of how many blocks it is when there is more than
			one.

			One block keeps the bare code, because the number of a
			thing that has no second is noise.
		*/
		static QString blockName(const CatalogPart &part,
					 int index, int total);
};

#endif
