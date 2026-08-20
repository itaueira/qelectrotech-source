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
#ifndef SHEETSYMBOLEXTRACTOR_H
#define SHEETSYMBOLEXTRACTOR_H

#include <QBrush>
#include <QPen>
#include <QString>

#include "symbolbuilder.h"

class DiagramContent;
class QetShapeItem;

/**
	@brief Reads what is selected on the sheet into a symbol being built.

	The whole point of the task this belongs to: the drawing tools of the
	sheet are the drawing tools people already know, so a symbol should be
	made with them and not in another program. This class is the bridge, and
	it is deliberately thin — it maps one item type to one struct and
	translates a pen into the style string the element definition uses.
	Everything that could get a decision wrong lives in SymbolDefinition
	instead, where it can be tested without a window.
*/
class SheetSymbolExtractor
{
	public:
		/**
			@brief Read the shapes and the texts of @a content into a symbol.
			Connection points are not read: the sheet has none to read. They
			are suggested from the free ends of the drawing
			(SymbolDefinition::suggestedTerminals) and confirmed by the
			person who drew it.
		*/
		static SymbolDefinition fromSelection(const DiagramContent &content,
						      const SymbolGrid &grid);

		/**
			@return why @a content cannot become a symbol, or an empty string
			when it can. Said before the dialog opens, because a dialog that
			opens only to refuse is a dialog that wasted a click.
		*/
		static QString refusal(const DiagramContent &content);

		/// the css like style string of the element definition for this pen
		static QString styleOf(const QPen &pen, const QBrush &brush);

	private:
		static SymbolShape shapeOf(const QetShapeItem *item);
		/// the name the element definition gives @a color, or an empty string
		static QString colorName(const QColor &color);
};

#endif // SHEETSYMBOLEXTRACTOR_H
