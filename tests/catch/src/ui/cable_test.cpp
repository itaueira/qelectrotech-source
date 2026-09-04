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
#include "../../../../sources/diagramcommands.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QUndoStack>
#include <QUuid>
#include <QVariant>
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
// Labelled T15 rather than CU-15.13 on purpose. The use case asks that the
// orphan wire APPEAR as a marked spare and that the project REPORT it, and
// neither half has a user interface yet: sources/cable/ holds the model, the
// wire and the report, and CableReport has no user outside sources/cable/.
// What is provable today - the rule and the round trip - is what this proves.
TEST_CASE("T15 - a wire whose conductor is gone comes back as a marked spare", "[cable]")
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

/*
	Copy and paste, and the two things a copied wire must not bring with it.

	A pasted conductor is a new wire, and it has to be new in the two ways
	that matter to the cables: it must not answer to the identity of the wire
	it was copied from, and it must not claim membership of the cable that
	wire belongs to. The identity is the uuid a cable's wire reaches its
	conductor by (CableWire::conductorUuid()), and the membership has a
	one-line mirror in the "cable" attribute of the conductor itself, written
	by the program and travelling literally through the XML the copy is made
	of.

	The case that motivates all this is copying a stretch of cable: a
	selection of wires and nothing else. That is also the selection the reset
	of the conductor labels used to miss entirely, because its loop sat
	inside the loop over the pasted elements - with no element in the
	content, the loop body never ran, and the copy came out wearing the
	original's wire numbers.

	Read this before trusting the sections below: a content of wires and no
	element is not something the clipboard can hand over today.
	Diagram::toXml(false, true) writes a conductor out only when both of its
	elements are selected, and Diagram::fromXml builds a pasted conductor
	only when both of its terminals are found among the elements it has just
	added - so every content that reaches PasteDiagramCommand through the
	clipboard has at least one element in it. The sections therefore do both:
	one drives the whole clipboard round trip, and the two others hand the
	command the content its own contract is written for, built out of
	conductors that the clipboard round trip really produced.

	The fixture is written here rather than taken from examples/ for one
	reason: no example ships a conductor whose "cable" attribute is already
	filled, and it is the copy of a filled one that is the whole subject.
*/

namespace {

		/// The two terminals of the symbol the fixture embeds, by uuid.
	const QString top_terminal_uuid =
			QStringLiteral("{beef0001-0000-4000-8000-000000000001}");
	const QString bottom_terminal_uuid =
			QStringLiteral("{beef0001-0000-4000-8000-000000000002}");

		/// The identity each of the two source conductors carries in the file.
	const QString first_wire_uuid =
			QStringLiteral("{d0d0cafe-0000-4000-8000-000000000001}");
	const QString second_wire_uuid =
			QStringLiteral("{d0d0cafe-0000-4000-8000-000000000002}");

		/// The cable both source conductors say they belong to.
	const QString source_cable = QStringLiteral("W12");

		/// The wire number each source conductor carries.
	const QString first_wire_text = QStringLiteral("L1");
	const QString second_wire_text = QStringLiteral("L2");

		/// The identities of the two source conductors, sorted.
	QStringList sourceWireUuids()
	{
		QStringList uuids{first_wire_uuid, second_wire_uuid};
		uuids.sort();
		return uuids;
	}

	/**
		The project the paste case works on, as text.

		Four instances of one symbol, in two vertical pairs, and one
		conductor running down each pair. Both conductors say they belong to
		the same cable and carry a wire number of their own.

		The formula is written empty on purpose, and not left out to be
		tidy: Conductor::setProperties() recomputes the text from the formula
		whenever there is one, so a conductor carrying a formula would not
		come out of a paste with an empty text however well the reset works.
		A fixture with a formula would measure that path and call it this one.
	*/
	QString pasteFixtureXml()
	{
		struct Instance
		{
			int x;
			int y;
			const char *label;
		};

			// Two pairs, one instance above the other in each pair.
		const Instance instances[] = {
			{100, 100, "X1"},
			{100, 200, "X2"},
			{300, 100, "X3"},
			{300, 200, "X4"}};

		QString elements_xml;
		int index = 0;
		for (const Instance &instance : instances)
		{
			++ index;
			elements_xml += QStringLiteral(
						"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
						" freezeLabel=\"false\" orientation=\"0\""
						" type=\"embed://bench/wireend.elmt\""
						" uuid=\"{c0ffee01-0000-4000-8000-00000000000%3}\">"
						"<terminals/><inputs/>"
						"<elementInformations>"
						"<elementInformation show=\"1\" name=\"label\">%4"
						"</elementInformation>"
						"</elementInformations>"
						"<dynamic_texts/><texts_groups/>"
						"</element>")
					.arg(instance.x)
					.arg(instance.y)
					.arg(index)
					.arg(QLatin1String(instance.label));
		}

		struct Wire
		{
			const QString &uuid;
			const QString &text;
			int upper_instance;
			int lower_instance;
		};

		const Wire wires[] = {
			{first_wire_uuid, first_wire_text, 1, 2},
			{second_wire_uuid, second_wire_text, 3, 4}};

		QString conductors_xml;
		for (const Wire &wire : wires)
		{
			conductors_xml += QStringLiteral(
						  "<conductor uuid=\"%1\""
						  " element1=\"{c0ffee01-0000-4000-8000-00000000000%2}\""
						  " terminal1=\"%3\""
						  " element2=\"{c0ffee01-0000-4000-8000-00000000000%4}\""
						  " terminal2=\"%5\""
						  " type=\"multi\" num=\"%6\" formula=\"\""
						  " cable=\"%7\" freezeLabel=\"false\"/>")
					  .arg(wire.uuid)
					  .arg(wire.upper_instance)
					  .arg(bottom_terminal_uuid)
					  .arg(wire.lower_instance)
					  .arg(top_terminal_uuid)
					  .arg(wire.text)
					  .arg(source_cable);
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"wireend.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"40\""
			       " hotspot_x=\"10\" hotspot_y=\"20\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Wire end</name></names>"
			       "<description>"
			       "<line x1=\"0\" y1=\"-15\" x2=\"0\" y2=\"15\""
			       " length1=\"1.5\" end1=\"none\""
			       " length2=\"1.5\" end2=\"none\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal uuid=\"%3\" name=\"\" type=\"Generic\""
			       " orientation=\"n\" x=\"0\" y=\"-15\"/>"
			       "<terminal uuid=\"%4\" name=\"\" type=\"Generic\""
			       " orientation=\"s\" x=\"0\" y=\"15\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"15\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors>%2</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(elements_xml, conductors_xml)
		       .arg(top_terminal_uuid, bottom_terminal_uuid);
	}

		/// The uuid of each conductor, as text, sorted.
	QStringList uuidsOf(const QList<Conductor *> &conductors)
	{
		QStringList uuids;
		for (const Conductor *conductor : conductors) {
			uuids << conductor->uuid().toString();
		}
		uuids.sort();
		return uuids;
	}

		/// The cable each conductor claims to belong to, sorted.
	QStringList cablesOf(const QList<Conductor *> &conductors)
	{
		QStringList cables;
		for (const Conductor *conductor : conductors) {
			cables << conductor->properties().m_cable;
		}
		cables.sort();
		return cables;
	}

		/// The wire number each conductor shows, sorted.
	QStringList textsOf(const QList<Conductor *> &conductors)
	{
		QStringList texts;
		for (const Conductor *conductor : conductors) {
			texts << conductor->properties().text;
		}
		texts.sort();
		return texts;
	}

	/**
		The copy and the paste of DiagramView, without the clipboard.

		The same three steps DiagramView::copy() and DiagramView::paste()
		take - toXml(false, true), a trip through text, fromXml - so that
		what lands in @a pasted is what a real copy and paste would have
		produced, serialisation included. The undo command is left to the
		caller: it is the one thing each section drives differently.
	*/
	bool copySelectionAndReadItBack(Diagram *folio, DiagramContent *pasted)
	{
		const QString clipboard_text = folio->toXml(false, true).toString(4);

		QDomDocument document;
		if (!document.setContent(clipboard_text)) {
			return false;
		}

		return folio->fromXml(document, QPointF(600, 100), false, pasted);
	}

		/// What the "erase label on copy" preference reads while a guard below lives.
	enum class EraseLabels
	{
			/// The key removed, so the default written in the code applies.
		AtItsDefault,
			/// The key set to false, the way a user who wants the labels kept sets it.
		Off
	};

	/**
		The "erase label on copy" preference, put back on the way out.

		The suite writes into settings of its own and not into those of the
		program (see main.cpp), but they are still one store shared by every
		case of the run and by every run on the machine. Without the reset, a
		section that turns the preference off would leave the next case
		measuring a preference it never set - and the sections below are
		about both sides of that preference.
	*/
	class ErasePreference
	{
		public:
			explicit ErasePreference(EraseLabels wanted)
			{
				QSettings settings;
				m_was_stored = settings.contains(m_key);
				m_stored = settings.value(m_key);

				if (wanted == EraseLabels::Off) {
					settings.setValue(m_key, false);
				} else {
					settings.remove(m_key);
				}
			}

			~ErasePreference()
			{
				QSettings settings;
				if (m_was_stored) {
					settings.setValue(m_key, m_stored);
				} else {
					settings.remove(m_key);
				}
			}

			ErasePreference(const ErasePreference &) = delete;
			ErasePreference &operator=(const ErasePreference &) = delete;

		private:
			const QString m_key =
					QStringLiteral("diagramcommands/erase-label-on-copy");
			bool m_was_stored = false;
			QVariant m_stored;
	};
}

TEST_CASE("T15 — a pasted wire loses the identity and the cable of the wire it came from",
	  "[cable][paste]")
{
	UiBench::ScratchProject scratch{pasteFixtureXml(),
					QStringLiteral("paste.qet")};
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.error().isEmpty());

	Diagram *folio = scratch.diagram(0);
	REQUIRE(folio);

		//The fixture as the loading path read it: four components, two
		//conductors, and the two conductors carrying the identities, the
		//cable and the wire numbers written in the file. Checked and not
		//assumed, because every expected value below is one of these.
	REQUIRE(folio->elements().count() == 4);
	REQUIRE(folio->conductors().count() == 2);
	REQUIRE(uuidsOf(folio->conductors()) == sourceWireUuids());
	REQUIRE(cablesOf(folio->conductors())
		== QStringList({source_cable, source_cable}));
	REQUIRE(textsOf(folio->conductors())
		== QStringList({first_wire_text, second_wire_text}));

	const QList<Element *> sources = folio->elements();
	for (Element *element : sources) {
		element->setSelected(true);
	}

	SECTION("the whole selection, components and wires: what the clipboard carries today")
	{
		ErasePreference preference{EraseLabels::AtItsDefault};

		DiagramContent pasted;
		REQUIRE(copySelectionAndReadItBack(folio, &pasted));
		REQUIRE(pasted.m_elements.count() == 4);
		REQUIRE(pasted.m_conductors_to_move.count() == 2);

			//Before the command: the copy is a twin. It answers to the same
			//uuid, claims the same cable and shows the same wire number -
			//which is what makes the work of the command visible below, and
			//what a paste that skipped it would leave in the project.
		CHECK(uuidsOf(pasted.m_conductors_to_move) == sourceWireUuids());
		CHECK(cablesOf(pasted.m_conductors_to_move)
		      == QStringList({source_cable, source_cable}));
		CHECK(textsOf(pasted.m_conductors_to_move)
		      == QStringList({first_wire_text, second_wire_text}));

		folio->undoStack().push(new PasteDiagramCommand(folio, pasted));

		const QStringList pasted_uuids = uuidsOf(pasted.m_conductors_to_move);
		CHECK(pasted_uuids.count() == 2);
		CHECK_FALSE(pasted_uuids.contains(first_wire_uuid));
		CHECK_FALSE(pasted_uuids.contains(second_wire_uuid));
		CHECK(cablesOf(pasted.m_conductors_to_move)
		      == QStringList({QString(), QString()}));
		CHECK(textsOf(pasted.m_conductors_to_move)
		      == QStringList({QString(), QString()}));

			//And the folio now holds four conductors with four different
			//identities. Written as the size of a set because a cable
			//reaches its wire by uuid: two conductors sharing one is not a
			//duplicated label, it is a project that cannot say which wire a
			//cable holds.
		const QStringList folio_uuids = uuidsOf(folio->conductors());
		CHECK(folio_uuids.count() == 4);
		CHECK(QSet<QString>(folio_uuids.begin(), folio_uuids.end()).count() == 4);

			//The two originals are untouched: a paste clears the copy, not
			//the drawing the copy was made from.
		CHECK(cablesOf(folio->conductors())
		      == QStringList({QString(), QString(), source_cable, source_cable}));
	}

	SECTION("wires alone, with no component in the content")
	{
		ErasePreference preference{EraseLabels::AtItsDefault};

		DiagramContent pasted;
		REQUIRE(copySelectionAndReadItBack(folio, &pasted));
		REQUIRE(pasted.m_conductors_to_move.count() == 2);

			//The content the contract of the command is written for, and the
			//one the clipboard cannot build today (see the head of this
			//case): the two conductors the round trip above really produced,
			//and nothing else.
		DiagramContent wires_only;
		wires_only.m_conductors_to_move = pasted.m_conductors_to_move;
		REQUIRE(wires_only.m_elements.isEmpty());
		REQUIRE(wires_only.conductors().count() == 2);

		folio->undoStack().push(new PasteDiagramCommand(folio, wires_only));

		const QStringList pasted_uuids = uuidsOf(wires_only.m_conductors_to_move);
		CHECK(pasted_uuids.count() == 2);
		CHECK_FALSE(pasted_uuids.contains(first_wire_uuid));
		CHECK_FALSE(pasted_uuids.contains(second_wire_uuid));

		CHECK(cablesOf(wires_only.m_conductors_to_move)
		      == QStringList({QString(), QString()}));

			//The assertion the nesting used to make impossible: with no
			//element in the content, the reset of the wire numbers has to
			//run all the same.
		CHECK(textsOf(wires_only.m_conductors_to_move)
		      == QStringList({QString(), QString()}));
	}

	SECTION("wires alone with the preference off: the number is kept, the identity is not")
	{
		ErasePreference preference{EraseLabels::Off};

		DiagramContent pasted;
		REQUIRE(copySelectionAndReadItBack(folio, &pasted));
		REQUIRE(pasted.m_conductors_to_move.count() == 2);

		DiagramContent wires_only;
		wires_only.m_conductors_to_move = pasted.m_conductors_to_move;
		REQUIRE(wires_only.m_elements.isEmpty());

		folio->undoStack().push(new PasteDiagramCommand(folio, wires_only));

			//The other side of the same border. "Erase label on copy" is a
			//preference about labels, so the wire numbers stay - and the
			//uuid and the cable go anyway, because neither is a label: one
			//is the identity a cable reaches its wire by, the other is the
			//written mirror of a membership this copy does not have.
		CHECK(textsOf(wires_only.m_conductors_to_move)
		      == QStringList({first_wire_text, second_wire_text}));

		const QStringList pasted_uuids = uuidsOf(wires_only.m_conductors_to_move);
		CHECK_FALSE(pasted_uuids.contains(first_wire_uuid));
		CHECK_FALSE(pasted_uuids.contains(second_wire_uuid));
		CHECK(cablesOf(wires_only.m_conductors_to_move)
		      == QStringList({QString(), QString()}));
	}
}
