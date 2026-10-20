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
#include "wirecolorname.h"

#include <QCoreApplication>

namespace
{
	struct NamedColor
	{
		QRgb rgb;
		const char *name;
	};

		//By the exact value. The first eight are the colours wires are
		//drawn in in practice; the others the remaining colours of
		//IEC 60757, by their common value.
	const NamedColor named_colors[] = {
		{0xff0000, QT_TRANSLATE_NOOP("WireColorName", "Red")},
		{0x0000ff, QT_TRANSLATE_NOOP("WireColorName", "Blue")},
		{0xb3b3b3, QT_TRANSLATE_NOOP("WireColorName", "Light grey")},
		{0x00ffff, QT_TRANSLATE_NOOP("WireColorName", "Cyan")},
		{0x55ff00, QT_TRANSLATE_NOOP("WireColorName", "Lime green")},
		{0x00ff00, QT_TRANSLATE_NOOP("WireColorName", "Green")},
		{0x636363, QT_TRANSLATE_NOOP("WireColorName", "Dark grey")},
		{0x000000, QT_TRANSLATE_NOOP("WireColorName", "Black")},
		{0x8b4513, QT_TRANSLATE_NOOP("WireColorName", "Brown")},
		{0xff8000, QT_TRANSLATE_NOOP("WireColorName", "Orange")},
		{0xffff00, QT_TRANSLATE_NOOP("WireColorName", "Yellow")},
		{0x8000ff, QT_TRANSLATE_NOOP("WireColorName", "Violet")},
		{0x808080, QT_TRANSLATE_NOOP("WireColorName", "Grey")},
		{0xffffff, QT_TRANSLATE_NOOP("WireColorName", "White")},
		{0xffc0cb, QT_TRANSLATE_NOOP("WireColorName", "Pink")},
		{0x40e0d0, QT_TRANSLATE_NOOP("WireColorName", "Turquoise")}};
}

QChar WireColorName::marker()
{
	return QChar(0x25A0);
}

QString WireColorName::withoutMarker(const QString &text)
{
	const QChar square = marker();
	if (!text.contains(square)) {
		return text;
	}

	QString plain;
	plain.reserve(text.size());
	for (int i = 0 ; i < text.size() ; ++i)
	{
		if (text.at(i) == square)
		{
				//The space the formula writes between the square and what
				//follows it goes with the square
			if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char(' ')) {
				++i;
			}
			continue;
		}
		plain.append(text.at(i));
	}
		//A wire with no cross-section yet is named "F5-2 (■)": without the
		//square, the parentheses it was alone in go too, and the list
		//reads "F5-2", not "F5-2 ()"
	plain.replace(QStringLiteral(" ()"), QString());
	plain.replace(QStringLiteral("()"), QString());
	return plain;
}

QString WireColorName::name(const QColor &color)
{
	if (!color.isValid()) {
		return QString();
	}

		//The alpha is not part of the name: a wire is not half Blue
	const QRgb rgb = color.rgb() & 0x00ffffff;
	for (const NamedColor &named : named_colors)
	{
		if (named.rgb == rgb) {
			return QCoreApplication::translate("WireColorName", named.name);
		}
	}
	return color.name(QColor::HexRgb).toLower();
}

QString WireColorName::name(const QColor &color, bool bicolor, const QColor &second)
{
	const QString main = name(color);
	if (!bicolor) {
		return main;
	}
	return main + QLatin1Char('/') + name(second);
}

QString WireColorName::label(const QString &typed,
							 const QColor &color,
							 bool bicolor,
							 const QColor &second)
{
	if (!typed.trimmed().isEmpty()) {
		return typed;
	}
	return name(color, bicolor, second);
}
