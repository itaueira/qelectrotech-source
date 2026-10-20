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
#include "qt_catch_tostring.h"

#include "../../../sources/wirecolorname.h"

#include <catch2/catch.hpp>

#include <QColor>
#include <QString>

/*
	The name of the colour of a wire, and the marker of its square (T70).
	No translator is installed in this suite, so the names come out in the
	language of the source, English.
*/
TEST_CASE("T70 — The colours wires are drawn in, by their exact value", "[T70][wirecolorname]")
{
	CHECK(WireColorName::name(QColor(QStringLiteral("#ff0000"))) == QStringLiteral("Red"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#0000ff"))) == QStringLiteral("Blue"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#b3b3b3"))) == QStringLiteral("Light grey"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#00ffff"))) == QStringLiteral("Cyan"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#55ff00"))) == QStringLiteral("Lime green"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#00ff00"))) == QStringLiteral("Green"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#636363"))) == QStringLiteral("Dark grey"));
	CHECK(WireColorName::name(QColor(Qt::black)) == QStringLiteral("Black"));
}

TEST_CASE("T70 — The other colours of the table", "[T70][wirecolorname]")
{
	CHECK(WireColorName::name(QColor(QStringLiteral("#8b4513"))) == QStringLiteral("Brown"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#ff8000"))) == QStringLiteral("Orange"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#ffff00"))) == QStringLiteral("Yellow"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#8000ff"))) == QStringLiteral("Violet"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#808080"))) == QStringLiteral("Grey"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#ffffff"))) == QStringLiteral("White"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#ffc0cb"))) == QStringLiteral("Pink"));
	CHECK(WireColorName::name(QColor(QStringLiteral("#40e0d0"))) == QStringLiteral("Turquoise"));
}

TEST_CASE("T70 — A colour off the table is written as its code", "[T70][wirecolorname]")
{
	SECTION("one unit away from Blue is not Blue")
	{
		CHECK(WireColorName::name(QColor(QStringLiteral("#0000fe"))) == QStringLiteral("#0000fe"));
	}
	SECTION("in small letters, whatever it was written with")
	{
		CHECK(WireColorName::name(QColor(QStringLiteral("#DADADA"))) == QStringLiteral("#dadada"));
	}
	SECTION("the transparency is not part of the name")
	{
		QColor half_blue(0, 0, 255);
		half_blue.setAlpha(128);
		CHECK(WireColorName::name(half_blue) == QStringLiteral("Blue"));
	}
	SECTION("an invalid colour has no name")
	{
		CHECK(WireColorName::name(QColor()).isEmpty());
	}
}

TEST_CASE("T70 — A two-colour wire: the two names, the main one first", "[T70][wirecolorname]")
{
	const QColor green(QStringLiteral("#00ff00"));
	const QColor yellow(QStringLiteral("#ffff00"));

	CHECK(WireColorName::name(green, true, yellow) == QStringLiteral("Green/Yellow"));
	CHECK(WireColorName::name(yellow, true, green) == QStringLiteral("Yellow/Green"));
		//The second colour of a wire that is not two-colour is not read:
		//the properties of every wire carry one
	CHECK(WireColorName::name(green, false, yellow) == QStringLiteral("Green"));
		//Each half by the same table
	CHECK(WireColorName::name(green, true, QColor(QStringLiteral("#123456")))
	      == QStringLiteral("Green/#123456"));
}

TEST_CASE("T70 — The colour a list writes: the typed one first", "[T70][wirecolorname]")
{
	const QColor blue(QStringLiteral("#0000ff"));

	CHECK(WireColorName::label(QStringLiteral("BU"), blue, false, blue) == QStringLiteral("BU"));
	CHECK(WireColorName::label(QString(), blue, false, blue) == QStringLiteral("Blue"));
		//Spaces are nothing typed
	CHECK(WireColorName::label(QStringLiteral("   "), blue, false, blue) == QStringLiteral("Blue"));
	CHECK(WireColorName::label(QString(), QColor(QStringLiteral("#00ff00")), true,
				   QColor(QStringLiteral("#ffff00")))
	      == QStringLiteral("Green/Yellow"));
}

TEST_CASE("T70 — The marker of the square leaves the text of a list", "[T70][wirecolorname]")
{
	const QChar square = WireColorName::marker();
		//U+25A0, BLACK SQUARE: the character a font draws in its place
	CHECK(int(square.unicode()) == 0x25A0);
	CHECK(QString(square) == QString::fromUtf8("■"));

	SECTION("the marker and the space after it go")
	{
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("F1-1 (■ 1mm²)"))
		      == QString::fromUtf8("F1-1 (1mm²)"));
	}
	SECTION("a text with no marker comes back as it is, spaces included")
	{
		CHECK(WireColorName::withoutMarker(QStringLiteral("F1-1 ( 1mm)"))
		      == QStringLiteral("F1-1 ( 1mm)"));
		CHECK(WireColorName::withoutMarker(QString()).isEmpty());
	}
	SECTION("a marker with no space after it goes alone")
	{
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("W■1")) == QStringLiteral("W1"));
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("W1 ■")) == QStringLiteral("W1 "));
	}
	SECTION("every marker of the text")
	{
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("■ A ■ B")) == QStringLiteral("A B"));
	}
	SECTION("a text that is only the marker is empty")
	{
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("■ ")).isEmpty());
	}
	SECTION("a wire with no cross-section yet: no empty parentheses")
	{
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("F5-2 (■)")) == QStringLiteral("F5-2"));
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("F5 PE (■)")) == QStringLiteral("F5 PE"));
			//With what follows the square, the parentheses stay
		CHECK(WireColorName::withoutMarker(QString::fromUtf8("F5-3 (■ Cabo CAT6 STP)"))
		      == QStringLiteral("F5-3 (Cabo CAT6 STP)"));
	}
}
