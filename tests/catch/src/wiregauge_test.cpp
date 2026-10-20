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

#include "../../../sources/wiregauge.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QLatin1String>

#include <cmath>

/*
	The cross-section of a wire as a component carries it (T70): read from
	what a person types, written back in one writing, and the largest of a
	point chosen on the number. The rule alone - the wires of a folio taking
	it are in ui/wiregauge_ui_test.cpp, which needs a project.

	The superscript two is written by its code, as in the source it tests,
	so that this file says the same thing whatever it is opened as.
*/
namespace
{
	const QChar two(0x00B2);

		/// @a number followed by "mm²", the writing a wire carries
	QString mm2(const char *number)
	{
		return QString::fromLatin1(number) + QLatin1String("mm") + two;
	}
}

TEST_CASE("T70 — reading a cross-section", "[T70][wiregauge]")
{
	SECTION("the ways a section is typed")
	{
		CHECK(WireGauge::parse(QStringLiteral("1")) == Approx(1.0));
		CHECK(WireGauge::parse(QStringLiteral("1,5")) == Approx(1.5));
		CHECK(WireGauge::parse(QStringLiteral("1.5")) == Approx(1.5));
		CHECK(WireGauge::parse(mm2("2,5")) == Approx(2.5));
		CHECK(WireGauge::parse(QStringLiteral("2,5 mm") + two) == Approx(2.5));
		CHECK(WireGauge::parse(QStringLiteral("2.5mm2")) == Approx(2.5));
		CHECK(WireGauge::parse(QStringLiteral("2,5 MM2")) == Approx(2.5));
		CHECK(WireGauge::parse(QStringLiteral("0,75mm")) == Approx(0.75));
		CHECK(WireGauge::parse(QStringLiteral("10")) == Approx(10.0));
	}
	SECTION("blanks around, and the non-breaking space before the unit")
	{
		CHECK(WireGauge::parse(QStringLiteral("  1,5  ")) == Approx(1.5));
		CHECK(WireGauge::parse(QStringLiteral("1,5") + QChar(0x00A0)
				       + QStringLiteral("mm") + two)
		      == Approx(1.5));
	}
	SECTION("nothing, or anything that is not a section, is no section")
	{
		CHECK(WireGauge::parse(QString()) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("   ")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("abc")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("mm2")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("1,5 cm2")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("1,5,2")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral(",5")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("-1")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("2 x 1,5")) == 0.0);
	}
	SECTION("a section of zero is no section")
	{
		CHECK(WireGauge::parse(QStringLiteral("0")) == 0.0);
		CHECK(WireGauge::parse(QStringLiteral("0,0")) == 0.0);
			//Below what format() writes: no section, rather than "0mm²"
		CHECK(WireGauge::parse(QStringLiteral("0,0001")) == 0.0);
	}
}

TEST_CASE("T70 — writing a cross-section", "[T70][wiregauge]")
{
	SECTION("a decimal comma, no trailing zero, mm² glued to it")
	{
		CHECK(WireGauge::format(1.0) == mm2("1"));
		CHECK(WireGauge::format(1.5) == mm2("1,5"));
		CHECK(WireGauge::format(0.75) == mm2("0,75"));
		CHECK(WireGauge::format(0.5) == mm2("0,5"));
		CHECK(WireGauge::format(10.0) == mm2("10"));
		CHECK(WireGauge::format(0.14) == mm2("0,14"));
	}
	SECTION("no section, nothing written")
	{
		CHECK(WireGauge::format(0.0).isEmpty());
		CHECK(WireGauge::format(-1.5).isEmpty());
		CHECK(WireGauge::format(std::nan("")).isEmpty());
	}
	SECTION("each typed writing comes back in the one writing")
	{
		CHECK(WireGauge::normalized(QStringLiteral("1")) == mm2("1"));
		CHECK(WireGauge::normalized(QStringLiteral("1,50")) == mm2("1,5"));
		CHECK(WireGauge::normalized(QStringLiteral("1.5")) == mm2("1,5"));
		CHECK(WireGauge::normalized(QStringLiteral("2,5 mm") + two) == mm2("2,5"));
		CHECK(WireGauge::normalized(QStringLiteral("2.5mm2")) == mm2("2,5"));
		CHECK(WireGauge::normalized(QStringLiteral("abc")).isEmpty());
			//What it writes, it reads back as the same section
		CHECK(WireGauge::normalized(mm2("0,75")) == mm2("0,75"));
	}
}

TEST_CASE("T70 — the largest section of a point", "[T70][wiregauge]")
{
	SECTION("chosen on the number, and not on the text")
	{
			//As text, "10" < "2,5" < "4"; as sections, 10 is the largest
		CHECK(WireGauge::largest({QStringLiteral("2,5"), QStringLiteral("10"),
					  QStringLiteral("4")})
		      == mm2("10"));
		CHECK(WireGauge::largest({QStringLiteral("0,5"), QStringLiteral("1")})
		      == mm2("1"));
		CHECK(WireGauge::largest({QStringLiteral("1"), QStringLiteral("1,5mm2")})
		      == mm2("1,5"));
	}
	SECTION("a component that says nothing does not count")
	{
			//The terminal of a strip, with no section of its own, does not
			//lower the section of its point
		CHECK(WireGauge::largest({QStringLiteral("0,5"), QString()})
		      == mm2("0,5"));
		CHECK(WireGauge::largest({QString(), QStringLiteral("abc"),
					  QStringLiteral("0,75")})
		      == mm2("0,75"));
	}
	SECTION("none of them says anything: no section")
	{
		CHECK(WireGauge::largest({}).isEmpty());
		CHECK(WireGauge::largest({QString(), QString()}).isEmpty());
		CHECK(WireGauge::largest({QStringLiteral("abc"), QStringLiteral("0")})
		      .isEmpty());
	}
	SECTION("the same section typed two ways is one section")
	{
		CHECK(WireGauge::largest({QStringLiteral("1,5"), QStringLiteral("1.5mm2")})
		      == mm2("1,5"));
	}
}
