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
#ifndef WIRECOLORNAME_H
#define WIRECOLORNAME_H

#include <QChar>
#include <QColor>
#include <QString>

/**
	The colour of a wire in words, and the marker a conductor's text
	carries where the folio draws a square of that colour.

	The variable %wq of a conductor's formula puts the marker in the text.
	The text stays plain text - it is what the file, the project database
	and the lists hold - and the folio paints a square of the wire's colour
	over the marker (ConductorTextItem::paint()). A list cannot paint, so
	it takes the marker out and writes the name of the colour in a column
	of its own.

	A pure rule: a colour in, a name out, no conductor and no folio. The
	names are matched on the exact value, never on the nearest colour: a
	colour outside the table is written as its code, which is the truth,
	rather than as the name of a colour that is only close to it.
*/
namespace WireColorName
{
	/// @return the character %wq puts in a conductor's text: U+25A0,
	/// BLACK SQUARE, which is also what a font without the folio's
	/// drawing shows in its place
	QChar marker();

	/// @return @p text without the marker and without the space that
	/// follows each one; @p text unchanged when it holds no marker
	QString withoutMarker(const QString &text);

	/// @return the name of @p color, translated and not abbreviated, when
	/// its value is one of the table; its code "#rrggbb" otherwise; empty
	/// for an invalid colour
	QString name(const QColor &color);

	/// @return the name of a wire drawn in @p color; for a two-colour
	/// wire (@p bicolor), the two names joined by a slash, the main colour
	/// first: "Green/Yellow"
	QString name(const QColor &color, bool bicolor, const QColor &second);

	/// @return what a list writes as the colour of a wire: @p typed, the
	/// colour typed in the properties of the wire, when it holds anything
	/// but spaces; the name of the colour it is drawn in otherwise
	QString label(const QString &typed,
				  const QColor &color,
				  bool bicolor,
				  const QColor &second);
}

#endif // WIRECOLORNAME_H
