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
#include "../../../../sources/cable/cablereport.h"
#include "../../../../sources/cable/cablewire.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QUuid>
#include <algorithm>

/*
	A cable of the project, saved and read again.

	The cable is data of the project and not of a folio, so none of this can
	be proved without a project open: it is the loading path that has to link
	each wire to its conductor, and it is the loading path that decides what
	happens to a wire whose conductor is not there any more.

	The conductors are never built by hand here. They are read from an
	example that ships with QElectroTech, and their uuids are the ones the
	loading path minted for them - an example written before conductors
	carried a uuid has none in the file, which is precisely the project a
	cable will be drawn on. Building conductors in memory would prove the
	class against identities this test invented, and the identity is the
	whole mechanism.

	What stays out: the cable editor, the numbering of the wires and the
	cross reference drawn at each end. Those are windows, and they are not
	what these cases are about.
*/

namespace {

		/// The example the cases read their conductors from: 188 conductors, several folios.
	const char *reference_example = "convertisseur.qet";

	/**
		The conductors of @a project, once each, in an order that does not
		move between runs.

		QGraphicsScene::items() answers in an order of its own, so the
		conductors are sorted by uuid here. Which conductor a wire gets is
		of no interest to any of these cases; that the same one is picked on
		the next run is.
	*/
	QList<Conductor *> sortedConductors(Diagram *folio)
	{
		QList<Conductor *> conductors = folio->conductors();

		std::sort(conductors.begin(), conductors.end(),
			  [](const Conductor *a, const Conductor *b)
		{
			return a->uuid().toString() < b->uuid().toString();
		});

		return conductors;
	}

		/// The first folio of @a project carrying at least @a count conductors
	Diagram *folioWithConductors(QETProject *project, int count)
	{
		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			if (folio->conductors().count() >= count) {
				return folio;
			}
		}

		return nullptr;
	}
}

/*
	CU-15.13 - a wire whose conductor no longer exists.

	Build a cable, delete one of the conductors it carries, save, close and
	open again. The wire has to be there, marked as having lost its
	conductor, and the project has to say so.

	A wire that comes back as an ordinary spare fails the case as surely as
	one that disappears: "the designer left this one empty" and "this one
	lost its wire and nobody was told" are the two facts that must not be
	confused, and the deliberate spare in the same cable is here so that the
	difference is measured and not assumed.
*/
TEST_CASE("CU-15.13 - a wire whose conductor is gone comes back as a marked spare", "[cable]")
{
	const QString content =
			UiBench::fileContent(UiBench::examplePath(
						     QLatin1String(reference_example)));
	REQUIRE_FALSE(content.isEmpty());

	UiBench::ScratchProject scratch(content, QStringLiteral("cable.qet"));
	REQUIRE(scratch.isOpen());
	INFO(scratch.error().toStdString());

	Diagram *folio = folioWithConductors(scratch.project(), 3);
	REQUIRE(folio != nullptr);

	const QList<Conductor *> conductors = sortedConductors(folio);
	REQUIRE(conductors.count() >= 3);

		//A four wire cable: three carrying a conductor and one left empty on
		//purpose. The empty one is the control - it must still read as a
		//plain spare at the end, or the mark means nothing.
	Cable *cable = scratch->newCable(QStringLiteral("W1"));
	REQUIRE(cable != nullptr);
	cable->setPartCode(QStringLiteral("CBL-4G1"));
	cable->setWireCount(4);
	REQUIRE(cable->wireCount() == 4);

	const QUuid cable_uuid = cable->uuid();
	QVector<QUuid> carried;

	for (int i = 0 ; i < 3 ; ++i)
	{
		CableWire wire = cable->wire(i);
		wire.setColor(QStringLiteral("BK"));
		wire.setSection(QStringLiteral("1.5"));
		wire.setConductor(conductors.at(i));
		REQUIRE(cable->setWire(i, wire));

		carried.append(conductors.at(i)->uuid());
		REQUIRE_FALSE(carried.last().isNull());
	}

	SECTION("with every conductor in place, nothing is reported")
	{
			//The negative control of the case. Without it, an orphan found
			//later could just as well come from a resolution that finds no
			//conductor at all - which is the defect the load order guards
			//against - and the case would pass while proving nothing.
		scratch->resolveCables();
		const CableReport report = scratch->cableReport();

		CHECK(report.cables == 1);
		CHECK(report.wires_occupied == 3);
		CHECK(report.wires_reserved == 1);
		CHECK(report.wires_orphan == 0);
		CHECK_FALSE(report.hasOrphan());
		CHECK(report.orphans.isEmpty());
		CHECK(report.toText().isEmpty() == false);
	}

	SECTION("the conductor of the second wire is deleted, and the project is saved and reopened")
	{
		const QUuid lost_uuid = carried.at(1);
		Conductor *doomed = conductors.at(1);

			//Deleted the way DeleteQGraphicsItemCommand does it: out of the
			//folio, then destroyed. Its destructor takes it off both of its
			//terminals, so what is saved below is a project where that
			//conductor simply is not.
		folio->removeItem(doomed);
		delete doomed;

		REQUIRE(scratch.saveAndReopen());
			//Every pointer taken above dangles from here on.

		Cable *reloaded = scratch->cable(cable_uuid);
		REQUIRE(reloaded != nullptr);

		SECTION("the wire is still there")
		{
				//The heart of it. TerminalStrip::fromXml() drops a member
				//whose uuid matches nothing, and a physical terminal that
				//loses all of its members disappears from the strip. The
				//cable does not inherit that: four wires went in and four
				//wires come out.
			CHECK(reloaded->wireCount() == 4);
			CHECK_FALSE(reloaded->wireByNumber(QStringLiteral("2")).number().isEmpty());
		}

		SECTION("and it is marked, not turned into a spare")
		{
			const CableWire wire = reloaded->wireByNumber(QStringLiteral("2"));

			CHECK(wire.state() == CableWire::Orphan);
			CHECK(wire.hasLostConductor());
			CHECK_FALSE(wire.isReserved());
			CHECK_FALSE(wire.isOccupied());
			CHECK(wire.conductor() == nullptr);
				//The identity is kept, so the wire is whole again the day
				//the conductor comes back - an undone deletion, a folio
				//pasted back.
			CHECK(wire.conductorUuid() == lost_uuid);
		}

		SECTION("its neighbours found their conductors")
		{
			const CableWire first = reloaded->wireByNumber(QStringLiteral("1"));
			const CableWire third = reloaded->wireByNumber(QStringLiteral("3"));

			CHECK(first.isOccupied());
			CHECK(third.isOccupied());
			REQUIRE(first.conductor() != nullptr);
			REQUIRE(third.conductor() != nullptr);
			CHECK(first.conductor()->uuid() == carried.at(0));
			CHECK(third.conductor()->uuid() == carried.at(2));
				//What was typed on the wire survived the round trip too.
			CHECK(first.color() == QStringLiteral("BK"));
			CHECK(first.section() == QStringLiteral("1.5"));
		}

		SECTION("the wire left empty on purpose is still a plain spare")
		{
			const CableWire spare = reloaded->wireByNumber(QStringLiteral("4"));

			CHECK(spare.isReserved());
			CHECK_FALSE(spare.hasLostConductor());
			CHECK(spare.conductorUuid().isNull());
		}

		SECTION("the project says which wire lost what")
		{
			const CableReport report = scratch->cableReport();

			CHECK(report.cables == 1);
			CHECK(report.wires_occupied == 2);
			CHECK(report.wires_reserved == 1);
			CHECK(report.wires_orphan == 1);
			CHECK(report.duplicated_conductors == 0);
			CHECK(report.hasOrphan());
			REQUIRE(report.orphans.count() == 1);

				//The wording is translated and is not what is checked. What
				//is checked is that the line names the two things somebody
				//has to be told apart by: which cable, and which conductor.
				//The wire number is not looked for in the line, and on
				//purpose: any short number is a substring of a uuid, so
				//such a check would pass whether the number was there or
				//not. Which wire it is, is checked above, on the wire.
			const QString line = report.orphans.first();
			CHECK(line.contains(QStringLiteral("W1")));
			CHECK(line.contains(lost_uuid.toString()));
			CHECK_FALSE(report.toText().isEmpty());
		}

		SECTION("and the cable still carries its own identification")
		{
			CHECK(reloaded->label() == QStringLiteral("W1"));
			CHECK(reloaded->partCode() == QStringLiteral("CBL-4G1"));
			CHECK_FALSE(reloaded->isShielded());
			CHECK(reloaded->lostWires().count() == 1);
		}
	}
}

/*
	The model half of CU-15.4 - the non linear, wire by wire cable.

	The case itself ends in a renumbering, which is not written yet, so it
	keeps its place in the queue and this test does not take its number.
	What is proved here is the part the renumbering will stand on: the cable
	pairs a wire with a conductor one by one, and a deliberately crossed
	pairing comes back crossed. A model that renumbered by rank instead of
	by wire would pass every other case in this file and lose the crossing
	here.
*/
TEST_CASE("T15 - the pairing of a crossed cable survives the round trip", "[cable]")
{
	const QString content =
			UiBench::fileContent(UiBench::examplePath(
						     QLatin1String(reference_example)));
	REQUIRE_FALSE(content.isEmpty());

	UiBench::ScratchProject scratch(content, QStringLiteral("crossed.qet"));
	REQUIRE(scratch.isOpen());

	Diagram *folio = folioWithConductors(scratch.project(), 4);
	REQUIRE(folio != nullptr);

	const QList<Conductor *> conductors = sortedConductors(folio);
	REQUIRE(conductors.count() >= 4);

	Cable *cable = scratch->newCable(QStringLiteral("W2"));
	cable->setWireCount(4);

		//Crossed on purpose: wire 1 takes the fourth conductor, wire 2 the
		//third, and so on. Nothing about the cable is in rank order any more.
	QVector<QUuid> expected;
	for (int i = 0 ; i < 4 ; ++i)
	{
		Conductor *conductor = conductors.at(3 - i);
		CableWire wire = cable->wire(i);
		wire.setConductor(conductor);
		REQUIRE(cable->setWire(i, wire));
		expected.append(conductor->uuid());
	}

	const QUuid cable_uuid = cable->uuid();
	REQUIRE(scratch.saveAndReopen());

	Cable *reloaded = scratch->cable(cable_uuid);
	REQUIRE(reloaded != nullptr);
	REQUIRE(reloaded->wireCount() == 4);

	for (int i = 0 ; i < 4 ; ++i)
	{
		const CableWire wire = reloaded->wireByNumber(QString::number(i + 1));
		INFO("wire " << (i + 1));
		CHECK(wire.isOccupied());
		CHECK(wire.conductorUuid() == expected.at(i));
	}

	CHECK(scratch->cableReport().wires_occupied == 4);
	CHECK(scratch->cableReport().hasOrphan() == false);
}

/*
	The model half of CU-15.9 - the same cable on two folios.

	The case ends in a cross reference at each end and in navigating from one
	to the other, which is a window and stays in the queue. What is proved
	here is that there is one cable to navigate: a cable whose wires are
	conductors of two different folios is saved once, read back once, and
	knows both folios - not two cables of the same name, which is what a
	folio owned cable would have produced.
*/
TEST_CASE("T15 - a cable whose wires are on two folios stays one cable", "[cable]")
{
	const QString content =
			UiBench::fileContent(UiBench::examplePath(
						     QLatin1String(reference_example)));
	REQUIRE_FALSE(content.isEmpty());

	UiBench::ScratchProject scratch(content, QStringLiteral("twofolios.qet"));
	REQUIRE(scratch.isOpen());

		//Two folios that both draw, whichever they are in the example.
	QList<Diagram *> drawing;
	const QList<Diagram *> folios = scratch.diagrams();
	for (Diagram *folio : folios)
	{
		if (!folio->conductors().isEmpty()) {
			drawing.append(folio);
		}
		if (drawing.count() == 2) {
			break;
		}
	}
	REQUIRE(drawing.count() == 2);

	Cable *cable = scratch->newCable(QStringLiteral("W3"));
	cable->setWireCount(2);

	for (int i = 0 ; i < 2 ; ++i)
	{
		CableWire wire = cable->wire(i);
		wire.setConductor(sortedConductors(drawing.at(i)).first());
		REQUIRE(cable->setWire(i, wire));
	}

	const QUuid cable_uuid = cable->uuid();
	CHECK(cable->folios().count() == 2);

	REQUIRE(scratch.saveAndReopen());

		//One cable in the project, not one per folio.
	CHECK(scratch->cables().count() == 1);

	Cable *reloaded = scratch->cable(cable_uuid);
	REQUIRE(reloaded != nullptr);
	CHECK(reloaded->label() == QStringLiteral("W3"));
	CHECK(reloaded->wireCount() == 2);
	CHECK(reloaded->folios().count() == 2);
		//Answered in the order of the project, so a cable list prints the
		//folios of a cable the way the project is bound.
	CHECK(reloaded->folios().first() != reloaded->folios().last());
	CHECK(scratch->cableReport().wires_occupied == 2);
}

/*
	A project written before the cables existed - which is every project
	there is today - has to open, and has to open with no cable and nothing
	to report. Written down because the tolerant read is the half of
	PropertiesInterface that is easy to get right and easy to leave untested.
*/
TEST_CASE("T15 - a project with no cable block reads with no cable", "[cable]")
{
	UiBench::Project project{QLatin1String(reference_example)};
	REQUIRE(project.isOpen());

	CHECK(project->cables().isEmpty());

	const CableReport report = project->cableReport();
	CHECK(report.isEmpty());
	CHECK_FALSE(report.hasOrphan());
	CHECK(report.toText().isEmpty());
}
