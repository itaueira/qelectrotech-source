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

#include "../../../sources/symbolterminals.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QList>
#include <QString>
#include <QStringList>

/*
	The terminals of a symbol put on whole numbers (T72), on the XML of the
	symbol alone. What a project does with it - the copies changed, the
	elements placed from them reloaded, the wires kept on their terminals -
	needs a project open and is in src/ui/symbolterminals_test.cpp.
*/
namespace
{
		/// A <definition> with the drawing of a line at fractions, and
		/// @a terminals as its terminals
	QString definitionXml(const QString &terminals)
	{
		return QStringLiteral(
			"<definition type=\"element\" version=\"0.80\" width=\"50\" height=\"50\""
			" hotspot_x=\"25\" hotspot_y=\"25\" link_type=\"simple\">"
			"<description>"
			"<line x1=\"-21.92\" y1=\"-15.83\" x2=\"-11.92\" y2=\"-15.83\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%1"
			"</description>"
			"</definition>").arg(terminals);
	}

	QString terminalXml(const char *x, const char *y, const char *uuid)
	{
		return QStringLiteral("<terminal x=\"%1\" y=\"%2\" orientation=\"w\" uuid=\"%3\"/>")
				.arg(QLatin1String(x), QLatin1String(y), QLatin1String(uuid));
	}

	QList<QDomElement> terminalsOf(const QDomDocument &document)
	{
		QList<QDomElement> terminals;
		const QDomElement description =
				document.documentElement().firstChildElement(QStringLiteral("description"));
		for (QDomElement terminal = description.firstChildElement(QStringLiteral("terminal")) ;
			 !terminal.isNull() ;
			 terminal = terminal.nextSiblingElement(QStringLiteral("terminal"))) {
			terminals << terminal;
		}
		return terminals;
	}

	QStringList attributes(const QDomDocument &document, const QString &name)
	{
		QStringList values;
		for (const QDomElement &terminal : terminalsOf(document)) {
			values << terminal.attribute(name);
		}
		return values;
	}

		/// The four terminals of the input module of the report, each with
		/// its uuid
	QString inputModule()
	{
		return terminalXml("-21.92", "-15.83", "{72000000-0000-4000-8000-000000000001}")
				+ terminalXml("-21.92", "-4.83", "{72000000-0000-4000-8000-000000000002}")
				+ terminalXml("-21.92", "6.17", "{72000000-0000-4000-8000-000000000003}")
				+ terminalXml("-21.92", "17.17", "{72000000-0000-4000-8000-000000000004}");
	}
}

TEST_CASE("T72 — Each terminal goes to the nearest whole number", "[T72][symbolterminals]")
{
	QDomDocument document;
	REQUIRE(document.setContent(definitionXml(inputModule())));
	const QStringList uuids_before = attributes(document, QStringLiteral("uuid"));

	const SymbolTerminals::Rounding planned = SymbolTerminals::planned(document.documentElement());
	CHECK(planned.terminals == 4);
	CHECK_FALSE(planned.merged);
		//Planning changes nothing
	CHECK(attributes(document, QStringLiteral("y"))
		  == QStringList({QStringLiteral("-15.83"), QStringLiteral("-4.83"),
						  QStringLiteral("6.17"), QStringLiteral("17.17")}));

	QList<SymbolTerminals::Move> moves;
	const SymbolTerminals::Rounding done =
			SymbolTerminals::putOnWholeNumbers(document.documentElement(), &moves);
	CHECK(done.terminals == 4);
	CHECK_FALSE(done.merged);
	CHECK(attributes(document, QStringLiteral("x"))
		  == QStringList({QStringLiteral("-22"), QStringLiteral("-22"),
						  QStringLiteral("-22"), QStringLiteral("-22")}));
	CHECK(attributes(document, QStringLiteral("y"))
		  == QStringList({QStringLiteral("-16"), QStringLiteral("-5"),
						  QStringLiteral("6"), QStringLiteral("17")}));

		//Only the place: the uuid the wires are attached by, the orientation
		//and the drawing stay
	CHECK(attributes(document, QStringLiteral("uuid")) == uuids_before);
	CHECK(attributes(document, QStringLiteral("orientation"))
		  == QStringList({QStringLiteral("w"), QStringLiteral("w"),
						  QStringLiteral("w"), QStringLiteral("w")}));
	const QDomElement line = document.documentElement()
			.firstChildElement(QStringLiteral("description"))
			.firstChildElement(QStringLiteral("line"));
	CHECK(line.attribute(QStringLiteral("x1")) == QStringLiteral("-21.92"));
	CHECK(line.attribute(QStringLiteral("y1")) == QStringLiteral("-15.83"));

		//What moved, from where to where: what an element placed from the
		//symbol has to move with it
	REQUIRE(moves.size() == 4);
	CHECK(moves.at(0).from == QPointF(-21.92, -15.83));
	CHECK(moves.at(0).to == QPointF(-22, -16));
	CHECK(moves.at(2).to == QPointF(-22, 6));
	CHECK(moves.at(0).terminal.attribute(QStringLiteral("uuid")) == uuids_before.at(0));

		//Twice is once
	CHECK(SymbolTerminals::putOnWholeNumbers(document.documentElement()).terminals == 0);
}

TEST_CASE("T72 — A symbol on whole numbers is left as it is written", "[T72][symbolterminals]")
{
	QDomDocument document;
	REQUIRE(document.setContent(definitionXml(
					terminalXml("10.0", "-20", "{72000000-0000-4000-8000-000000000011}")
					+ terminalXml("-10", "0", "{72000000-0000-4000-8000-000000000012}"))));

	CHECK(SymbolTerminals::planned(document.documentElement()).terminals == 0);
	QList<SymbolTerminals::Move> moves;
	CHECK(SymbolTerminals::putOnWholeNumbers(document.documentElement(), &moves).terminals == 0);
	CHECK(moves.isEmpty());
		//"10.0" is a whole number already: not written again
	CHECK(attributes(document, QStringLiteral("x"))
		  == QStringList({QStringLiteral("10.0"), QStringLiteral("-10")}));
}

TEST_CASE("T72 — Only the coordinate that is off is written", "[T72][symbolterminals]")
{
	QDomDocument document;
	REQUIRE(document.setContent(definitionXml(
					terminalXml("10.0", "6.17", "{72000000-0000-4000-8000-000000000021}"))));

	CHECK(SymbolTerminals::putOnWholeNumbers(document.documentElement()).terminals == 1);
	CHECK(attributes(document, QStringLiteral("x")) == QStringList({QStringLiteral("10.0")}));
	CHECK(attributes(document, QStringLiteral("y")) == QStringList({QStringLiteral("6")}));
}

TEST_CASE("T72 — Half a unit goes up, whatever the sign", "[T72][symbolterminals]")
{
	CHECK(SymbolTerminals::nearestWhole(0.5) == 1.0);
	CHECK(SymbolTerminals::nearestWhole(-0.5) == 0.0);
	CHECK(SymbolTerminals::nearestWhole(2.5) == 3.0);
	CHECK(SymbolTerminals::nearestWhole(-2.5) == -2.0);
	CHECK(SymbolTerminals::nearestWhole(-15.83) == -16.0);
	CHECK(SymbolTerminals::nearestWhole(6.17) == 6.0);
	CHECK(SymbolTerminals::isWhole(-16.0));
	CHECK_FALSE(SymbolTerminals::isWhole(6.17));
	CHECK_FALSE(SymbolTerminals::isWhole(-15.9999));
}

TEST_CASE("T72 — Two terminals rounding would join: nothing moves", "[T72][symbolterminals]")
{
	QDomDocument document;
	REQUIRE(document.setContent(definitionXml(
					terminalXml("0.3", "0", "{72000000-0000-4000-8000-000000000031}")
					+ terminalXml("-0.2", "0", "{72000000-0000-4000-8000-000000000032}")
					+ terminalXml("10.4", "0", "{72000000-0000-4000-8000-000000000033}"))));

	CHECK(SymbolTerminals::planned(document.documentElement()).merged);
	QList<SymbolTerminals::Move> moves;
	const SymbolTerminals::Rounding done =
			SymbolTerminals::putOnWholeNumbers(document.documentElement(), &moves);
	CHECK(done.merged);
	CHECK(done.terminals == 0);
	CHECK(moves.isEmpty());
		//The whole symbol, the third terminal included
	CHECK(attributes(document, QStringLiteral("x"))
		  == QStringList({QStringLiteral("0.3"), QStringLiteral("-0.2"), QStringLiteral("10.4")}));
}

TEST_CASE("T72 — Two terminals the symbol puts on one point stay together", "[T72][symbolterminals]")
{
	QDomDocument document;
	REQUIRE(document.setContent(definitionXml(
					terminalXml("5.4", "0", "{72000000-0000-4000-8000-000000000041}")
					+ terminalXml("5.4", "0", "{72000000-0000-4000-8000-000000000042}")
					+ terminalXml("x", "0", "{72000000-0000-4000-8000-000000000043}"))));

	const SymbolTerminals::Rounding done = SymbolTerminals::putOnWholeNumbers(document.documentElement());
	CHECK_FALSE(done.merged);
	CHECK(done.terminals == 2);
		//A place the program cannot read is not read here either
	CHECK(attributes(document, QStringLiteral("x"))
		  == QStringList({QStringLiteral("5"), QStringLiteral("5"), QStringLiteral("x")}));
}
