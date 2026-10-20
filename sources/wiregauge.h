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
#ifndef WIREGAUGE_H
#define WIREGAUGE_H

#include <QString>
#include <QStringList>

/**
	@brief The cross-section of a wire, read from what a person typed and
	written back the way a wire label carries it (T70).

	A component says which cross-section the wires landing on it need, in
	its information "wire_section"; the wires of an electrical point take
	the largest of the components of the point. What is typed there is
	free text, and the same section reaches it written in several ways -
	"1,5", "1.5", "1,5mm²", "1,5 mm²", "1.5mm2" - so the comparison is made
	on the number, never on the text.

	What goes back on the wire is always one writing: the number with a
	decimal comma, no trailing zeros, and "mm²" glued to it - "1mm²",
	"1,5mm²", "0,75mm²". One writing is what lets two wires of the same
	point be seen to carry the same section, on the folio and in a list.

	Pure on purpose: text in, text out, no element and no folio.
*/
namespace WireGauge
{
		/**
			@param text : a cross-section as a person types it
			@return the section in square millimetres, rounded to the
			thousandth; 0 when @p text holds none - empty, anything that
			is not a number with an optional "mm", "mm2" or "mm²", or a
			section of zero
		*/
	double parse(const QString &text);

		/**
			@param section : in square millimetres
			@return @p section written as a wire label carries it -
			"1mm²", "1,5mm²", "0,75mm²"; empty for a section of zero or
			less
		*/
	QString format(double section);

		/// format(parse(text)): @p text in the one writing, or empty
	QString normalized(const QString &text);

		/**
			@param texts : what each component of a point says, empty for
			the ones that say nothing
			@return the largest cross-section among @p texts, formatted;
			empty when none of them holds one. A text that holds no
			section does not count - a terminal of a strip has no
			section of its own, and must not lower the one of its point.
		*/
	QString largest(const QStringList &texts);
}

#endif // WIREGAUGE_H
