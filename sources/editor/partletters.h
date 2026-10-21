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
#ifndef PARTLETTERS_H
#define PARTLETTERS_H

#include <QRectF>

class QGraphicsTextItem;

/**
	@brief The letters of a text, and not its box (T73).
	The box of a text item is the box of its document: a margin on the four
	sides - the default of QTextDocument, which no text of a symbol
	changes - and the height of a whole line, above and below the letters.
	Its centre is not the centre of the letters, the less so the larger the
	font, and a text lined up by its box looks out of line with one of
	another size. The letters are what the eye lines up: from the left of
	the first letter to the right of the last, and from the top of a
	capital on the first line to the line the letters of the last stand
	on - as the tag of a folio report is placed (ReportTags, tagPlace()).
*/
namespace PartLetters
{
	QRectF lettersRect(const QGraphicsTextItem *text);
	QRectF lettersInScene(const QGraphicsTextItem *text);
}

#endif // PARTLETTERS_H
