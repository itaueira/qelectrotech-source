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
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/conductornumexport.h"
#include "../../../../sources/diagram.h"

#include <catch2/catch.hpp>

#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>

/*
	How many labels the workshop is told to print, and for which number.

	The list this file measures is one column of text: one row per length
	of wire, holding the number written on it. Somebody reads it, prints
	that many labels and ties them on. So the only thing it can get wrong
	is how many times a number appears - and that is exactly the kind of
	error nothing downstream catches. A roll with twice the labels does
	not fail; it runs out of nothing, jams nothing, and is discovered by a
	person who counts what is left over at the end of the shift.

	What was measured, on the rule as it stood: the count was incremented
	once per end of the conductor that was not a folio report arrow. An
	ordinary wire has two such ends, so it counted twice; a wire crossing
	folios has one, so it counted once. Ten ordinary wires and two
	crossing ones added up to 22 against a drawing that holds 12 - and the
	list was not uniformly doubled, which is worse than if it had been,
	because a doubled list is recognised at a glance and a list doubled in
	part is not.

	@par Why the two assertions and not one
	The number of rows is the cheap question and it catches gross
	contamination. It is not enough: a count that lands on the wrong
	number - six labels reading 200 and four reading 100 instead of the
	other way round - keeps the total at twelve and sends the wrong label
	to the wrong wire. So every case here asserts the total and the
	breakdown, and a case that asserted only one of the two would pass
	over half of what can go wrong.

	@par Why the conductors are written into a project rather than built
	The rule under test reads two things the drawing decides: the text of
	the conductor and the kind of element each of its ends is docked to.
	Conductors assembled in memory would let this file choose both
	directly, which is the same as asserting the rule against itself. The
	fixture is a .qet that the very loading path of the program reads, so
	the terminals, the report arrows and the numbers arrive the way they
	arrive for a person.
*/

namespace
{
	/// One wire of the fixture.
	struct Wire
	{
		/// What is written on the conductor, empty for none.
		const char *num;
		/// How many of its two ends sit on a folio report arrow.
		int report_ends;
	};

	/**
		The wires the first case is drawn on.

		Ten ordinary wires and two crossing ones, which is the drawing
		CU-17.9 describes, with the ten split over two numbers so that
		the breakdown says something the total cannot. Then two wires the
		list must not hold at all: one with both ends on report arrows,
		which is a link between two folios and not a length of wire, and
		one whose number is nothing but spaces, which is how a number
		that was cleared reaches the file.
	*/
	const Wire wires[] = {
		{"100", 0}, {"100", 0}, {"100", 0},
		{"100", 0}, {"100", 0}, {"100", 0},
		{"200", 0}, {"200", 0}, {"200", 0}, {"200", 0},
		{"300", 1}, {"300", 1},
		{"400", 2},
		{"   ", 0}};

	/// @return the two embedded symbols the fixture draws with
	QString definitions()
	{
		auto symbol = [](const QString &name,
				 const QString &link_type,
				 bool two_terminals)
		{
			QString terminals =
				QStringLiteral("<terminal x=\"0\" y=\"-10\""
					       " orientation=\"n\"/>");
			if (two_terminals) {
				terminals += QStringLiteral(
					"<terminal x=\"0\" y=\"10\""
					" orientation=\"s\"/>");
			}

			return QStringLiteral(
				       "<element name=\"%1\">"
				       "<definition type=\"element\" version=\"0.80\""
				       " width=\"20\" height=\"40\""
				       " hotspot_x=\"10\" hotspot_y=\"20\""
				       " orientation=\"dnnn\" link_type=\"%2\">"
				       "<names><name lang=\"en\">%1</name></names>"
				       "<description>"
				       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
				       " end1=\"none\" end2=\"none\" length1=\"1.5\""
				       " length2=\"1.5\" antialias=\"false\""
				       " style=\"line-style:normal;line-weight:normal;"
				       "filling:none;color:black\"/>"
				       "%3"
				       "</description>"
				       "</definition>"
				       "</element>")
			       .arg(name, link_type, terminals);
		};

			//The report arrow carries one terminal, like the folio
			//reference symbols of the shipped collection do: a link
			//between two folios has one wire landing on it and not two.
		return symbol(QStringLiteral("part.elmt"),
			      QStringLiteral("simple"), true)
		       + symbol(QStringLiteral("report.elmt"),
				QStringLiteral("next_report"), false);
	}

	/**
		@param list the wires to draw
		@return the whole .qet, as text

		Each wire is two elements one above the other and a conductor
		between them. The terminals are written with the coordinates the
		program writes them with - the docking point, which sits four
		units inside the terminal from the point the symbol declares -
		because that is what the loading path matches an instance
		terminal to a symbol terminal by. Written at the declared
		coordinates instead, the terminals do not match, the identifiers
		never reach the table the conductors are resolved through, and
		the project opens with every element in place and no conductor at
		all: a fixture that proves nothing and looks like it proves
		something.
	*/
	QString fixtureXml(const Wire *list, int count)
	{
		QString drawn;
		QString wired;
		int terminal_id = 0;
		int uuid = 0;
		int x = 100;

		for (int i = 0 ; i < count ; ++i)
		{
			const Wire &wire = list[i];

			auto instance = [&](bool is_report, int y)
			{
				const int north = terminal_id++;
					//The report symbol has one terminal, so
					//it takes one identifier; the ordinary
					//one takes two.
				QString terminals = QStringLiteral(
					"<terminal x=\"0\" y=\"-6\""
					" orientation=\"0\" id=\"%1\"/>")
						    .arg(north);
				int south = north;
				if (!is_report)
				{
					south = terminal_id++;
					terminals += QStringLiteral(
						"<terminal x=\"0\" y=\"6\""
						" orientation=\"2\" id=\"%1\"/>")
						     .arg(south);
				}

				++uuid;
				drawn += QStringLiteral(
						 "<element x=\"%1\" y=\"%2\" z=\"10\""
						 " prefix=\"\" freezeLabel=\"false\""
						 " orientation=\"0\" type=\"embed://bench/%3\""
						 " uuid=\"{cafe0000-0000-4000-8000-%4}\">"
						 "<terminals>%5</terminals><inputs/>"
						 "<elementInformations/>"
						 "<dynamic_texts/><texts_groups/>"
						 "</element>")
					 .arg(x)
					 .arg(y)
					 .arg(is_report
					      ? QStringLiteral("report.elmt")
					      : QStringLiteral("part.elmt"))
					 .arg(uuid, 12, 10, QLatin1Char('0'))
					 .arg(terminals);

					//The end a conductor of this wire docks
					//on: the bottom terminal of the upper
					//element, the top terminal of the lower
					//one. A report arrow has only the top.
				return QPair<int, int>(north, south);
			};

				//One end on a report arrow is a wire crossing
				//folios; two is a link between two folios and
				//no wire at all.
			const QPair<int, int> upper = instance(wire.report_ends >= 1, 100);
			const QPair<int, int> lower = instance(wire.report_ends >= 2, 300);

			wired += QStringLiteral(
					 "<conductor x=\"0\" y=\"0\" terminal1=\"%1\""
					 " terminal2=\"%2\" num=\"%3\" type=\"multi\""
					 " freezeLabel=\"false\" displaytext=\"1\""
					 " numsize=\"9\" condsize=\"1\"/>")
				 .arg(upper.second)
				 .arg(lower.first)
				 .arg(QString::fromUtf8(wire.num).toHtmlEscaped());

			x += 40;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"40\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/><conductors>%3</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(definitions(), drawn, wired);
	}

	/// @return the rows of @a csv, the empty tail dropped
	QStringList rowsOf(const QString &csv)
	{
		QStringList rows;
		const QStringList split = csv.split(QLatin1Char('\n'));
		for (const QString &row : split)
		{
			if (!row.isEmpty()) {
				rows << row;
			}
		}
		return rows;
	}

	/// @return how many times each number appears in @a rows
	QHash<QString, int> countOf(const QStringList &rows)
	{
		QHash<QString, int> counted;
		for (const QString &row : rows) {
			counted[row] = counted.value(row, 0) + 1;
		}
		return counted;
	}
}

TEST_CASE("T17 — a lista de números de fio conta por fio, não por ponta",
	  "[wire][export]")
{
	const int count = int(sizeof(wires) / sizeof(wires[0]));
	UiBench::ScratchProject scratch(fixtureXml(wires, count),
					QStringLiteral("wirenumber.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

		//The fixture only proves something if the conductors arrived.
		//Fourteen wires are drawn, and a terminal that failed to match
		//its symbol would drop them silently - so the case stops here
		//rather than measuring an empty drawing.
	REQUIRE(scratch.diagramCount() == 1);
	REQUIRE(scratch.diagram(0) != nullptr);
	REQUIRE(scratch.diagram(0)->conductors().count() == count);

	ConductorNumExport exporter(scratch.project());
	const QString csv = exporter.wiresNum();
	INFO(csv.toStdString());

	const QStringList rows = rowsOf(csv);
	const QHash<QString, int> counted = countOf(rows);

		//The total: one row per length of wire the drawing holds. Ten
		//ordinary, two crossing folios, and the two that are not wires
		//left out. Counted once per end this was 22.
	CHECK(rows.count() == 12);

		//And the breakdown, because the total alone would accept the
		//right number of labels carrying the wrong numbers.
	CHECK(counted.value(QStringLiteral("100")) == 6);
	CHECK(counted.value(QStringLiteral("200")) == 4);
	CHECK(counted.value(QStringLiteral("300")) == 2);

		//Three numbers in the drawing, three in the list: the count of
		//distinct rows is the shape of the list, the sum above is its
		//size, and a defect can move either one without the other.
	CHECK(counted.count() == 3);
}

TEST_CASE("T17 — o que não é fio fica fora da lista de números",
	  "[wire][export]")
{
	const int count = int(sizeof(wires) / sizeof(wires[0]));
	UiBench::ScratchProject scratch(fixtureXml(wires, count),
					QStringLiteral("wirenumberout.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	REQUIRE(scratch.diagram(0) != nullptr);
	REQUIRE(scratch.diagram(0)->conductors().count() == count);

	ConductorNumExport exporter(scratch.project());
	const QString csv = exporter.wiresNum();
	INFO(csv.toStdString());

	const QHash<QString, int> counted = countOf(rowsOf(csv));

		//Anchored on a number that must be there. Three assertions that
		//something is absent all pass over an empty list, and an empty
		//list is what a fixture that failed to draw its conductors
		//produces - so the case says what it expects to find before it
		//says what it expects not to.
	REQUIRE(counted.value(QStringLiteral("100")) == 6);

		//Both ends on report arrows: the drawing of a link between two
		//folios, not a length of wire. It is the half of the old rule
		//that was right, and counting one per conductor without a
		//thought would have let it in.
	CHECK(counted.value(QStringLiteral("400")) == 0);

		//A number that is nothing but spaces is a number somebody
		//cleared, and it names no wire.
	CHECK(counted.value(QStringLiteral("   ")) == 0);
	CHECK(counted.value(QString()) == 0);
}
