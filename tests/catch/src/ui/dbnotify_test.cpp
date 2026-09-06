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

#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/conductortextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QSignalSpy>
#include <QString>
#include <QUndoStack>

/*
	How many times a change to the wires tells the data base about itself.

	The rule that decides is UpdateCoalescer, and it is proved on its own in
	updatecoalescer_test.cpp of C_unittests, where no project is open. That
	suite cannot see any of what is below, and the difference is the whole
	point of this file: a rule that counts perfectly, called from nowhere,
	still leaves the list on the folio showing yesterday's wire numbers.

	Three joints are measured here, and each one can be cut without the pure
	suite noticing:

	1. updateConductor() announcing at all. It used to announce nothing, on
	   purpose and for a reason that named a data base column which does not
	   exist. Cut this and a wire renamed on the folio never reaches the
	   list beside it.
	2. the data base honouring an open operation. Cut this and the grouping
	   rule computes the right answer and nobody asks it.
	3. Conductor::displayedTextChanged() pushing a grouped command. Cut this
	   and renaming one wire of a potential of three announces three times -
	   which is exactly the storm the silence of point 1 was avoiding, and
	   the reason the silence could not simply be removed.

	Labelled T17 and not CU-17.18 or CU-17.19: both use cases ask for a list
	drawn on a folio to be seen refreshing, once, with a stopwatch on the
	renumbering. What is counted here is the announcement the refresh hangs
	off. The folio is a screen.
*/

namespace
{
	/**
		Four boxes, and three wires that all land on the same terminal of
		the first one - so the three are one potential.

		A junction and not a chain on purpose: relatedPotentialConductors()
		walks from terminal to terminal, and three wires sharing terminal 0
		is the shortest arrangement that gives a potential of three without
		depending on how a folio report or a terminal element links two
		sheets.
	*/
	QString fixtureXml()
	{
		struct Box
		{
			int x;
			int y;
			const char *label;
		};

		const Box boxes[] = {
			{200, 200, "K1"},
			{340, 200, "K2"},
			{340, 300, "K3"},
			{340, 400, "K4"}};

			//The docking point of a terminal, which is what the instance
			//stores.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString instances;
		int index = 0;
		for (const Box &box : boxes)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/box.elmt\""
					     " uuid=\"{decaf000-0000-4000-8000-00000000000%3}\">"
					     "<terminals>"
					     "<terminal x=\"%4\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
					     "<terminal x=\"%6\" y=\"0\" orientation=\"3\" id=\"%7\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%8"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(box.x)
				     .arg(box.y)
				     .arg(index)
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(QLatin1String(box.label));
			++index;
		}

			//Terminal 0 is the east terminal of K1. The three wires leave it
			//for the west terminal of K2, K3 and K4 - ids 3, 5 and 7.
		const QString conductors = QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"3\" num=\"W1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"0\" terminal2=\"5\" num=\"W1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"0\" terminal2=\"7\" num=\"W1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"30\" height=\"20\""
			       " hotspot_x=\"15\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors>%2</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(instances, conductors);
	}

	/// A new text on @a wire, the way any other caller of the model does it.
	void rename(Conductor *wire, const QString &text)
	{
		ConductorProperties properties = wire->properties();
		properties.m_formula.clear();
		properties.text = text;
		wire->setProperties(properties);
	}
}

TEST_CASE("T17 — a wire renamed reaches the data base", "[t17][database][conductor]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("dbnotify.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->conductors().size() == 3);

	projectDataBase *data_base = bench.project()->dataBase();
	REQUIRE(data_base != nullptr);

	QSignalSpy spy(data_base, &projectDataBase::dataBaseUpdated);
	REQUIRE(spy.isValid());

	SECTION("one edit, one notice")
	{
			//This is the joint the old comment closed on purpose: the row was
			//written and nobody was told, so a list drawn beside the wire kept
			//the number the wire used to carry.
		rename(sheet->conductors().first(), QStringLiteral("W7"));
		REQUIRE(spy.count() == 1);
	}

	SECTION("a change that changes nothing says nothing")
	{
			//setProperties() returns early when the properties are equal, so
			//no row is written and there is nothing to announce. Said here
			//because a count of 1 would mean the folio redraws every time a
			//dialogue is confirmed without a change.
		Conductor *wire = sheet->conductors().first();
		wire->setProperties(wire->properties());
		REQUIRE(spy.count() == 0);
	}

	SECTION("three edits outside an operation, three notices")
	{
			//The behaviour without grouping, measured rather than assumed:
			//it is the number the section below has to beat.
			//
			//The new numbers start at X and not at W because the fixture
			//gives all three wires the number W1: renaming the first one to
			//W1 changes nothing, announces nothing, and this section counted
			//2 instead of 3 until the letter changed. A fixture that makes
			//one of the three edits a no-op measures the wrong thing.
		const QList<Conductor *> wires = sheet->conductors();
		int serial = 0;
		for (Conductor *wire : wires) {
			rename(wire, QStringLiteral("X%1").arg(++serial));
		}
		REQUIRE(spy.count() == 3);
	}

	SECTION("the same three edits inside one operation, one notice")
	{
		const QList<Conductor *> wires = sheet->conductors();
		{
			projectDataBase::Operation operation(bench.project());
			int serial = 0;
			for (Conductor *wire : wires) {
				rename(wire, QStringLiteral("X%1").arg(++serial));
			}
				//Nothing has been said yet: the gesture is not over.
			REQUIRE(spy.count() == 0);
		}
		REQUIRE(spy.count() == 1);
	}

	SECTION("an operation in which nothing changed says nothing")
	{
		{
			projectDataBase::Operation operation(bench.project());
		}
		REQUIRE(spy.count() == 0);
	}
}

TEST_CASE("T17 — renaming one wire of a potential announces once, not once per wire",
	  "[t17][database][conductor]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("dbnotify.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	Conductor *wire = sheet->conductors().first();
	REQUIRE(wire != nullptr);
	REQUIRE(wire->textItem() != nullptr);

		//The fixture is only worth anything if the three wires really are one
		//potential. Checked, and not assumed: a junction that did not take
		//would leave a potential of one, and every count below would be 1
		//whether the grouping works or not.
	REQUIRE(wire->relatedPotentialConductors().size() == 2);

	projectDataBase *data_base = bench.project()->dataBase();
	REQUIRE(data_base != nullptr);
	QSignalSpy spy(data_base, &projectDataBase::dataBaseUpdated);
	REQUIRE(spy.isValid());

		//What the draughtsman does: type a number over the wire on the folio.
		//The slot is the one the text item calls, and it renames the whole
		//potential - three rows of the data base for one gesture.
	wire->textItem()->setPlainText(QStringLiteral("W42"));
	wire->displayedTextChanged();

	REQUIRE(spy.count() == 1);

		//And it really did rename three wires, so the 1 above is grouping and
		//not a rename that quietly touched a single row. Counted on the
		//formula, which the command writes as given: the text beside it is
		//recomputed through formulaToLabel(), and what is being measured here
		//is how many rows were touched, not how a label is composed.
	const QList<Conductor *> wires = sheet->conductors();
	int renamed = 0;
	for (Conductor *other : wires)
	{
		if (other->properties().m_formula == QStringLiteral("W42")) {
			++renamed;
		}
	}
	REQUIRE(renamed == 3);

		//Taking it back is the same gesture in the other direction, and costs
		//the same one notice. This is the half that a guard placed around the
		//push instead of inside the command would miss.
	spy.clear();
	sheet->undoStack().undo();
	REQUIRE(spy.count() == 1);
}
