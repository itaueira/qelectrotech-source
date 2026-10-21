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
#include "partletters.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QGraphicsTextItem>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <cmath>
#include <limits>

/**
	@brief PartLetters::lettersRect
	@return the letters of @a text, in its own coordinates: from the left of
	the first letter to the right of the last, among all its lines, and from
	the top of a capital on its first line to the line the letters of its
	last line stand on. The box of the text when it has no letter at all.
*/
QRectF PartLetters::lettersRect(const QGraphicsTextItem *text)
{
	if (!text) {
		return QRectF();
	}
	QTextDocument *document = text->document();
	const QFontMetricsF metrics(text->font());
	qreal left = std::numeric_limits<qreal>::infinity();
	qreal right = -std::numeric_limits<qreal>::infinity();
	qreal top = std::numeric_limits<qreal>::quiet_NaN();
	qreal bottom = std::numeric_limits<qreal>::quiet_NaN();

	for (QTextBlock block = document->begin() ; block.isValid() ; block = block.next())
	{
		const QTextLayout *layout = block.layout();
		if (!layout) {
			continue;
		}
		const QPointF origin = document->documentLayout()->blockBoundingRect(block).topLeft();
		for (int i = 0 ; i < layout->lineCount() ; ++i)
		{
			const QTextLine line = layout->lineAt(i);
			const QString letters = block.text().mid(line.textStart(), line.textLength());
				//Spaces leave no ink: a line of them has no letter
			const QRectF ink = metrics.tightBoundingRect(letters);
			if (letters.trimmed().isEmpty() || ink.isEmpty()) {
				continue;
			}
				//Where the first character of the line starts, and the line
				//the letters stand on
			const qreal start = origin.x() + line.cursorToX(line.textStart());
			const qreal baseline = origin.y() + line.y() + line.ascent();
			left = qMin(left, start + ink.left());
			right = qMax(right, start + ink.right());
			if (std::isnan(top)) {
				top = baseline - metrics.capHeight();
			}
			bottom = baseline;
		}
	}

	if (std::isnan(top)) {
		return text->boundingRect();
	}
	return QRectF(QPointF(left, top), QPointF(right, bottom));
}

/**
	@brief PartLetters::lettersInScene
	@return lettersRect() of @a text in the coordinates of its scene
*/
QRectF PartLetters::lettersInScene(const QGraphicsTextItem *text)
{
	if (!text) {
		return QRectF();
	}
	return text->mapRectToScene(lettersRect(text));
}
