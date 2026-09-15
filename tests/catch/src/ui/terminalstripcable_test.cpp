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

#include "../../../../sources/cable/cable.h"
#include "../../../../sources/cable/cablewire.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/TerminalStrip/realterminal.h"

#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

/*
	Three columns of the terminal strip manager, and what each of them says.

	Two of them - the cable and the wire of the cable - answered an empty
	string for every terminal of every project, because RealTerminal::cable()
	and RealTerminal::cableWire() were a "return QString()" waiting for a
	cable model to exist. The model exists since sources/cable/, and an empty
	column is indistinguishable from "this terminal is in no cable": nobody
	looking at the manager could tell a missing answer from a true one.

	The third - the conductor - answered for the first conductor of the
	terminal and dropped the rest. A terminal carrying two wires read exactly
	like a terminal carrying one, and the second wire was missing from the
	list with nothing said.

	The three defects are the same defect, and it is why the negative
	controls of this file are not decoration. Counting the rows of the
	manager never caught any of them, because the row count was right all
	along; what was wrong was inside the cell. So every case here states the
	content of the cell it fills, and the terminals that legitimately have
	nothing to say - one outside every cable, one with no conductor docked at
	all - are checked to keep saying nothing. Without those, filling the
	columns with any constant whatsoever would pass.

	Labelled with the tasks and not with the numbers of the use cases they
	serve: what a person reads in the manager is still a screen test, and
	these cases prove the answers the manager asks for, not the window that
	shows them.
*/

namespace
{
		//The wires of the bench, by the number the drawing gives them.
	const char *const C1 = "C1";   // terminal 1, field side, in cable W10
	const char *const C2 = "C2";   // terminal 1, panel side, in no cable
	const char *const C3 = "C3";   // terminal 2, in cable W20
	const char *const C4 = "C4";   // terminal 3, in no cable
	const char *const C5 = "C5";   // terminal 4, in cable W10
	const char *const C6 = "C6";   // terminal 4, in cable W10 as well

	/**
		A project of one folio with five terminals and one component.

		Four of the terminals carry wires and the fifth carries none, which
		is the second of the two legitimate empties this file measures.
		Terminals 1 and 4 have a conductor on each side - which is what a
		terminal strip terminal is for, one wire from the field and one from
		the panel - and they are the two that used to answer for their first
		conductor alone.

		The component is one box with six docking points rather than six
		boxes, so that each conductor has a docking point of its own at each
		end: a docking point can hold several conductors, and sharing one
		would have made the order the terminals list their conductors depend
		on the order the file happens to name them.

		The docking coordinates of an instance are not the coordinates of the
		definition: Terminal offsets them by Terminal::terminalSize towards
		the middle of the element, and Terminal::fromXml() compares the
		offset ones. Written here as the arithmetic rather than as numbers so
		that the bench does not have to be edited the day that size changes.
	*/
	QString fixtureXml()
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

			//Terminal element i docks its east side on id 2i and its west
			//side on id 2i+1.
		QString instances;
		for (int index = 0 ; index < 5 ; ++index)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/terminal.elmt\""
					     " uuid=\"{c0ffee03-0000-4000-8000-00000000000%2}\">"
					     "<terminals>"
					     "<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
					     "<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%7"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(240 + index * 60)
				     .arg(index)
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(index + 1);
		}

			//The one component, and its six docking points: ids 100 to 105,
			//far from the terminal ids so that a shifted id is legible in a
			//failure instead of quietly landing on another terminal.
		QString box_terminals;
		QString box_definition_terminals;
		for (int index = 0 ; index < 6 ; ++index)
		{
			const int y = -50 + index * 20;

			box_terminals += QStringLiteral(
						 "<terminal x=\"%1\" y=\"%2\""
						 " orientation=\"1\" id=\"%3\"/>")
					 .arg(east_dock)
					 .arg(y)
					 .arg(100 + index);

			box_definition_terminals += QStringLiteral(
							    "<terminal x=\"10\" y=\"%1\""
							    " orientation=\"e\" name=\"%2\"/>")
						    .arg(y)
						    .arg(index + 1);
		}

		instances += QStringLiteral(
				     "<element x=\"120\" y=\"200\" z=\"10\" prefix=\"\""
				     " freezeLabel=\"false\" orientation=\"0\""
				     " type=\"embed://bench/box.elmt\""
				     " uuid=\"{decaf003-0000-4000-8000-000000000000}\">"
				     "<terminals>%1</terminals>"
				     "<inputs/>"
				     "<elementInformations>"
				     "<elementInformation show=\"1\" name=\"label\">K1"
				     "</elementInformation>"
				     "</elementInformations>"
				     "<dynamic_texts/><texts_groups/>"
				     "</element>")
			     .arg(box_terminals);

		struct Wire
		{
			int box_id;
			int terminal_id;
			const char *number;
		};

		const Wire wires[] = {
			{100, 1, C1},     // west side of terminal 1
			{101, 0, C2},     // east side of terminal 1
			{102, 3, C3},     // west side of terminal 2
			{103, 5, C4},     // west side of terminal 3
			{104, 7, C5},     // west side of terminal 4
			{105, 6, C6}};    // east side of terminal 4

		QString conductors;
		for (const Wire &wire : wires)
		{
			conductors += QStringLiteral(
					      "<conductor terminal1=\"%1\" terminal2=\"%2\""
					      " num=\"%3\" displaytext=\"1\" type=\"multi\""
					      " condsize=\"1\"/>")
				      .arg(wire.box_id)
				      .arg(wire.terminal_id)
				      .arg(QLatin1String(wire.number));
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"terminal.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"terminal\">"
			       "<names><name lang=\"en\">Terminal</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"40\" height=\"140\""
			       " hotspot_x=\"20\" hotspot_y=\"70\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-60\" width=\"16\" height=\"120\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "%1"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " folio=\"3\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors>%3</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(box_definition_terminals, instances, conductors);
	}

	/// The real terminal of the terminal element labelled @a label, null when none.
	QSharedPointer<RealTerminal> terminalLabelled(Diagram *diagram,
						      const QString &label)
	{
		if (!diagram) {
			return QSharedPointer<RealTerminal>();
		}

		const auto elements = diagram->elements();
		for (const auto element : elements)
		{
			if (element->elementData().m_type != ElementData::Terminal) {
				continue;
			}

			const auto real_t =
					static_cast<TerminalElement *>(element)->realTerminal();
			if (!real_t.isNull() &&
				real_t->label() == label) {
				return real_t;
			}
		}

		return QSharedPointer<RealTerminal>();
	}

	/// The conductor of @a diagram numbered @a number, nullptr when none.
	Conductor *conductorNumbered(Diagram *diagram, const QString &number)
	{
		if (!diagram) {
			return nullptr;
		}

		const auto conductors = diagram->conductors();
		for (Conductor *conductor : conductors)
		{
			if (conductor->properties().text == number) {
				return conductor;
			}
		}

		return nullptr;
	}

	/// A new number on @a wire, the way any other caller of the model does it.
	void renumber(Conductor *wire, const QString &number)
	{
		ConductorProperties properties = wire->properties();
		properties.m_formula.clear();
		properties.text = number;
		wire->setProperties(properties);
	}

	/**
		Give @a wire_index of @a cable the colour @a color and the conductor
		numbered @a conductor_number of @a diagram.

		Written as a helper because four wires are filled the same way, and
		four hand written copies of three lines would be four chances to
		fill one of them differently - which would show up as a defect of
		the column rather than of the bench.
	*/
	bool carry(Cable *cable,
		   int wire_index,
		   const QString &color,
		   Diagram *diagram,
		   const QString &conductor_number)
	{
		Conductor *conductor = conductorNumbered(diagram, conductor_number);
		if (!cable || !conductor || conductor->uuid().isNull()) {
			return false;
		}

		CableWire wire = cable->wire(wire_index);
		wire.setColor(color);
		wire.setConductor(conductor);

		return cable->setWire(wire_index, wire);
	}

	/**
		The two cables of the bench, over the folio @a diagram of @a project.

		W10 carries three of the six conductors and W20 carries one; two of
		the six are in no cable at all, which is what makes the empty columns
		of terminal 3 an answer rather than a silence.
	*/
	bool buildCables(QETProject *project, Diagram *diagram)
	{
		Cable *w10 = project->newCable(QStringLiteral("W10"));
		Cable *w20 = project->newCable(QStringLiteral("W20"));
		if (!w10 || !w20) {
			return false;
		}

		w10->setWireCount(3);
		w20->setWireCount(2);

		return carry(w10, 0, QStringLiteral("BK"), diagram, QLatin1String(C1))
		       && carry(w10, 1, QStringLiteral("BU"), diagram, QLatin1String(C5))
		       && carry(w10, 2, QStringLiteral("BN"), diagram, QLatin1String(C6))
		       && carry(w20, 0, QStringLiteral("GN"), diagram, QLatin1String(C3));
	}
}

TEST_CASE("T15 — as duas colunas de cabo da régua deixam de sair sempre vazias",
	  "[terminalstrip][cable]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("stripcable.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
		//If the bench loses a conductor, every count below still adds up and
		//says something else. Measured here, once, so that a failure names
		//the fixture instead of the code.
	REQUIRE(sheet->conductors().size() == 6);

	auto t1 = terminalLabelled(sheet, QStringLiteral("1"));
	auto t2 = terminalLabelled(sheet, QStringLiteral("2"));
	auto t3 = terminalLabelled(sheet, QStringLiteral("3"));
	auto t4 = terminalLabelled(sheet, QStringLiteral("4"));
	auto t5 = terminalLabelled(sheet, QStringLiteral("5"));
	REQUIRE_FALSE(t1.isNull());
	REQUIRE_FALSE(t2.isNull());
	REQUIRE_FALSE(t3.isNull());
	REQUIRE_FALSE(t4.isNull());
	REQUIRE_FALSE(t5.isNull());

		//The negative control, and it comes first on purpose: with no cable
		//in the project the two columns are empty, and that is the right
		//answer. Without it, a column filled with any constant at all would
		//pass every case below.
	REQUIRE(bench->cables().isEmpty());
	CHECK(t1->cables().isEmpty());
	CHECK(t1->cableWires().isEmpty());
	CHECK(t1->cable().isEmpty());
	CHECK(t1->cableWire().isEmpty());

	REQUIRE(buildCables(bench.project(), sheet));

	SECTION("a terminal a cable reaches names the cable and the wire")
	{
		CHECK(t1->cables() == QStringList{QStringLiteral("W10")});
		CHECK(t1->cableWires() == QStringList{QStringLiteral("BK")});
		CHECK(t1->cable() == QStringLiteral("W10"));
		CHECK(t1->cableWire() == QStringLiteral("BK"));

		CHECK(t2->cable() == QStringLiteral("W20"));
		CHECK(t2->cableWire() == QStringLiteral("GN"));
	}

	SECTION("a terminal two wires of one cable reach names it once")
	{
			//Terminal 4 carries C5 and C6, both wires of W10. The cable
			//column says W10 and not "W10, W10" - the second entry would
			//add nothing - while the wire column keeps both, because the
			//two wires are two different things.
		CHECK(t4->cables() == QStringList{QStringLiteral("W10")});
		CHECK(t4->cable() == QStringLiteral("W10"));

		CHECK(t4->cableWires() == QStringList({QStringLiteral("BU"),
						       QStringLiteral("BN")}));
		CHECK(t4->cableWire() == QStringLiteral("BU, BN"));
	}

	SECTION("a terminal no cable reaches keeps both columns empty")
	{
			//The legitimate empty, and the whole reason the columns were
			//unreadable before: a terminal outside every cable has to be
			//told apart from a terminal the code never answered for.
			//Terminal 3 draws a conductor, so the row is not empty for
			//want of anything on the drawing.
		CHECK(t3->conductor() == QString(QLatin1String(C4)));

		CHECK(t3->cables().isEmpty());
		CHECK(t3->cableWires().isEmpty());
		CHECK(t3->cable().isEmpty());
		CHECK(t3->cableWire().isEmpty());
	}

	SECTION("a terminal with nothing docked keeps every column empty")
	{
		CHECK(t5->conductors().isEmpty());
		CHECK(t5->conductor().isEmpty());
		CHECK(t5->cable().isEmpty());
		CHECK(t5->cableWire().isEmpty());
	}

	SECTION("only the conductors a cable carries take a place in the wire column")
	{
			//Terminal 1 carries two conductors and only one of them is in
			//a cable: two entries in the conductor column, one in the
			//cable wire column. The two lists are not the same length by
			//construction, and a case that asserted they were would be
			//asserting something the drawing never promised.
		CHECK(t1->conductors().count() == 2);
		CHECK(t1->cableWires().count() == 1);
	}

	SECTION("resolving the cables changes none of the answers")
	{
			//The columns are matched by uuid and never by the pointer
			//Cable::resolve() caches, so a strip asked before any
			//resolution answers what one asked after it answers. This is
			//the case that would go red if the lookup started reading the
			//cached pointer.
		const QString before_cable = t1->cable();
		const QString before_wire = t1->cableWire();

		bench->resolveCables();

		CHECK(t1->cable() == before_cable);
		CHECK(t1->cableWire() == before_wire);
		CHECK(t1->cable() == QStringLiteral("W10"));
		CHECK(t1->cableWire() == QStringLiteral("BK"));
	}

	SECTION("a wire with neither colour nor number is marked, not dropped")
	{
			//A wire of a cable that carries something and says nothing
			//about itself. Dropping the entry would shift every entry
			//after it in a terminal carrying several, so the entry keeps
			//its place and says it has no name. The mark is written out
			//here rather than taken from the code on purpose: changing it
			//has to be a deliberate act that shows up in this file too.
		Cable *w10 = bench->cables().first();
		REQUIRE(w10 != nullptr);
		REQUIRE(w10->label() == QStringLiteral("W10"));

		CableWire wire = w10->wire(0);
		wire.setColor(QString());
		wire.setNumber(QString());
		REQUIRE(w10->setWire(0, wire));

		CHECK(t1->cableWires() == QStringList{QStringLiteral("?")});
		CHECK(t1->cableWire() == QStringLiteral("?"));
			//And the cable is still named: it is the wire that has no
			//name, not the cable.
		CHECK(t1->cable() == QStringLiteral("W10"));
	}

	SECTION("the columns survive saving and reopening")
	{
		REQUIRE(bench.saveAndReopen());
			//Every pointer taken above dangles from here on.

		Diagram *reopened = bench.diagram(0);
		REQUIRE(reopened != nullptr);
		REQUIRE(bench->cables().count() == 2);

		auto r1 = terminalLabelled(reopened, QStringLiteral("1"));
		auto r3 = terminalLabelled(reopened, QStringLiteral("3"));
		auto r4 = terminalLabelled(reopened, QStringLiteral("4"));
		REQUIRE_FALSE(r1.isNull());
		REQUIRE_FALSE(r3.isNull());
		REQUIRE_FALSE(r4.isNull());

		CHECK(r1->cable() == QStringLiteral("W10"));
		CHECK(r1->cableWire() == QStringLiteral("BK"));
		CHECK(r4->cable() == QStringLiteral("W10"));
		CHECK(r4->cableWire() == QStringLiteral("BU, BN"));
			//And the legitimate empty is still empty on the way back,
			//which is the half a round trip usually forgets.
		CHECK(r3->cable().isEmpty());
		CHECK(r3->cableWire().isEmpty());
	}
}

TEST_CASE("T17 — o borne com dois condutores mostra os dois",
	  "[terminalstrip][conductor]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("stripcable.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->conductors().size() == 6);

	auto t1 = terminalLabelled(sheet, QStringLiteral("1"));
	auto t2 = terminalLabelled(sheet, QStringLiteral("2"));
	auto t4 = terminalLabelled(sheet, QStringLiteral("4"));
	auto t5 = terminalLabelled(sheet, QStringLiteral("5"));
	REQUIRE_FALSE(t1.isNull());
	REQUIRE_FALSE(t2.isNull());
	REQUIRE_FALSE(t4.isNull());
	REQUIRE_FALSE(t5.isNull());

	SECTION("both conductors of a terminal are listed, in the order they are docked")
	{
			//This is the defect, stated: the answer used to be "C1" and
			//C2 was nowhere. The order is the one Element::conductors()
			//gives - top to bottom, left to right - so the west side of
			//the terminal comes before the east side and the answer does
			//not move between runs.
		CHECK(t1->conductors() == QStringList({QLatin1String(C1),
						       QLatin1String(C2)}));
		CHECK(t1->conductor() == QStringLiteral("C1, C2"));

		CHECK(t4->conductors() == QStringList({QLatin1String(C5),
						       QLatin1String(C6)}));
		CHECK(t4->conductor() == QStringLiteral("C5, C6"));
	}

	SECTION("the count of the list is the count on the drawing")
	{
			//Counting rows never caught this defect, because the row
			//count of the manager was right all along - one row per
			//terminal, before and after. What was wrong was inside the
			//cell, and this is the assertion that measures it.
		CHECK(t1->conductors().count() == 2);
		CHECK(t2->conductors().count() == 1);
		CHECK(t4->conductors().count() == 2);
		CHECK(t5->conductors().count() == 0);
	}

	SECTION("a terminal with one conductor reads exactly as it did before")
	{
			//The half that must not change. A list that grew a separator
			//for every terminal would be a new defect wearing the shape
			//of a fix.
		CHECK(t2->conductors() == QStringList{QLatin1String(C3)});
		CHECK(t2->conductor() == QString(QLatin1String(C3)));
		CHECK_FALSE(t2->conductor().contains(QLatin1Char(',')));
	}

	SECTION("a terminal with nothing docked says nothing")
	{
			//The legitimate empty: this one is empty because there is no
			//conductor, and an empty answer here is the truth.
		CHECK(t5->conductors().isEmpty());
		CHECK(t5->conductor().isEmpty());
	}

	SECTION("a conductor with no number keeps its place")
	{
			//A conductor the designer has not numbered yet. Leaving it
			//out would put the manager back where it started - a terminal
			//with two conductors reading like a terminal with one - so
			//the entry stays and says it has no number. The list keeps
			//the empty string, which is the honest answer for a caller
			//that counts; the cell writes the mark, which is the legible
			//one for a caller that shows.
		Conductor *unnumbered = conductorNumbered(sheet, QLatin1String(C2));
		REQUIRE(unnumbered != nullptr);
		renumber(unnumbered, QString());

		CHECK(t1->conductors() == QStringList({QLatin1String(C1), QString()}));
		CHECK(t1->conductors().count() == 2);
		CHECK(t1->conductor() == QStringLiteral("C1, ?"));
	}

	SECTION("a terminal whose only conductor has no number is not read as empty")
	{
			//The sharpest form of the same thing, and the one that makes
			//the mark worth its ugliness: without it this cell would be
			//empty, and an empty cell in this column means "no conductor
			//docked" - which is false here and true for terminal 5.
		Conductor *unnumbered = conductorNumbered(sheet, QLatin1String(C3));
		REQUIRE(unnumbered != nullptr);
		renumber(unnumbered, QString());

		CHECK(t2->conductors().count() == 1);
		CHECK(t2->conductor() == QStringLiteral("?"));
		CHECK_FALSE(t2->conductor().isEmpty());
			//And the terminal that really has nothing still reads empty,
			//so the two are told apart.
		CHECK(t5->conductor().isEmpty());
	}
}
