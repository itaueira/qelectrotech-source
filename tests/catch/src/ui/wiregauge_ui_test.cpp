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

#include "../../../../sources/autoNum/numerotationcontext.h"
#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"
#include "../../../../sources/wiresections.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QGraphicsSceneMouseEvent>
#include <QLatin1String>
#include <QUndoStack>
#include <QUuid>

/*
	The cross-section of the wires of a point, taken from its components
	(T70): the largest the components ask for in their information
	"wire_section", and a component that asks for none - the terminal of a
	strip - does not count.

	The wires are drawn the way the user draws them: a press on a terminal
	and a release on another, sent to the terminal, which is what makes
	Terminal::mouseReleaseEvent() add the conductor - and, for the second
	wire of a point, copy the properties of the point instead of numbering
	it, which is the path a cross-section given only by the numbering would
	miss.

	The superscript two is written by its code, so that this file says the
	same thing whatever it is opened as.
*/
namespace
{
	const QChar two(0x00B2);

		/// @a number followed by "mm²", the writing a wire carries
	QString mm2(const char *number)
	{
		return QString::fromLatin1(number) + QLatin1String("mm") + two;
	}

		/// A component with a terminal on its right, one with a terminal on
		/// its left, and the terminal of a strip, with one on each side
	enum class Kind {Sensor, Relay, Strip};

	const char *kindName(Kind kind)
	{
		switch (kind)
		{
			case Kind::Sensor: return "sensor";
			case Kind::Relay:  return "relay";
			case Kind::Strip:  return "strip";
		}
		return "";
	}

	bool hasEast(Kind kind) {return kind != Kind::Relay;}
	bool hasWest(Kind kind) {return kind != Kind::Sensor;}

	QString definition(Kind kind)
	{
		QString terminals;
		if (hasEast(kind)) {
			terminals += QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>");
		}
		if (hasWest(kind)) {
			terminals += QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>");
		}
		return QStringLiteral(
			"<element name=\"%1.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"%2\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%3"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(kindName(kind)),
			     kind == Kind::Strip ? QStringLiteral("terminal") : QStringLiteral("simple"),
			     terminals);
	}

	struct Part
	{
		Kind kind;
		int x;
		int y;
			/// what the component says in "wire_section", "" for nothing
		const char *section;
	};

		/// A wire written in the file, from the right terminal of the part
		/// @a from to the left terminal of the part @a to - the indexes
		/// counted over the whole project
	struct Wire
	{
		int from;
		int to;
			/// the section typed in the wire, "" for none
		const char *section;
	};

	struct Folio
	{
		QList<Part> parts;
		QList<Wire> wires;
	};

	QString partUuid(int index)
	{
		return QStringLiteral("{70000000-0000-4000-8000-%1}")
				.arg(index, 12, 10, QLatin1Char('0'));
	}

	QString wireUuid(int index)
	{
		return QStringLiteral("{70c00000-0000-4000-8000-%1}")
				.arg(index, 12, 10, QLatin1Char('0'));
	}

		/// Part @a index: its right terminal is 2 index, its left one
		/// 2 index + 1, written at the docking point, as an instance stores
		/// them
	QString instanceXml(int index, const Part &part)
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;
		QString terminals;
		if (hasEast(part.kind)) {
			terminals += QStringLiteral("<terminal x=\"%1\" y=\"0\" orientation=\"1\" id=\"%2\"/>")
					.arg(east_dock).arg(2 * index);
		}
		if (hasWest(part.kind)) {
			terminals += QStringLiteral("<terminal x=\"%1\" y=\"0\" orientation=\"3\" id=\"%2\"/>")
					.arg(west_dock).arg(2 * index + 1);
		}
		QString informations;
		if (*part.section) {
			informations = QStringLiteral("<elementInformation show=\"1\" name=\"wire_section\">%1"
						      "</elementInformation>")
					.arg(QLatin1String(part.section));
		}
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/%3.elmt\" uuid=\"%4\">"
			"<terminals>%5</terminals><inputs/>"
			"<elementInformations>%6</elementInformations>"
			"<dynamic_texts/><texts_groups/>"
			"</element>")
			.arg(part.x).arg(part.y)
			.arg(QLatin1String(kindName(part.kind)), partUuid(index), terminals,
			     informations);
	}

	QString wireXml(int index, const Wire &wire)
	{
		QString section;
		if (*wire.section) {
			section = QStringLiteral(" conductor_section=\"%1\"").arg(QLatin1String(wire.section));
		}
		return QStringLiteral(
			"<conductor terminal1=\"%1\" terminal2=\"%2\" num=\"\" uuid=\"%3\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"%4/>")
			.arg(2 * wire.from).arg(2 * wire.to + 1).arg(wireUuid(index), section);
	}

	QString projectXml(const QList<Folio> &folios)
	{
		const QString definitions = definition(Kind::Sensor) + definition(Kind::Relay)
				+ definition(Kind::Strip);
		QString diagrams;
		int part_index = 0;
		int wire_index = 0;
		for (int f = 0 ; f < folios.size() ; ++f)
		{
			QString elements;
			QString conductors;
			for (const Part &part : folios.at(f).parts) {
				elements += instanceXml(part_index++, part);
			}
			for (const Wire &wire : folios.at(f).wires) {
				conductors += wireXml(wire_index++, wire);
			}
			diagrams += QStringLiteral(
				"<diagram title=\"Bench %1\" order=\"%1\" folio=\"%1\" height=\"600\""
				" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
				" displaycols=\"true\" displayrows=\"true\">"
				"<elements>%2</elements><inputs/><conductors>%3</conductors>"
				"</diagram>")
				.arg(f + 1).arg(elements, conductors);
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"%2"
			"</project>")
			.arg(definitions, diagrams);
	}

	struct Bench
	{
		explicit Bench(const QList<Folio> &folios) : project(projectXml(folios)) {}

			/// Part @a index of the project, by the uuid the file gives it
		Element *part(int index) const
		{
			const QUuid uuid(partUuid(index));
			const QList<Diagram *> diagrams = project.diagrams();
			for (Diagram *diagram : diagrams)
			{
				const QList<Element *> elements = diagram->elements();
				for (Element *element : elements) {
					if (element->uuid() == uuid) return element;
				}
			}
			return nullptr;
		}

			/// Wire @a index of the file, by the uuid the file gives it
		Conductor *wire(int index) const
		{
			const QUuid uuid(wireUuid(index));
			const QList<Diagram *> diagrams = project.diagrams();
			for (Diagram *diagram : diagrams)
			{
				const QList<Conductor *> conductors = diagram->conductors();
				for (Conductor *conductor : conductors) {
					if (conductor->uuid() == uuid) return conductor;
				}
			}
			return nullptr;
		}

		Terminal *side(int index, Qet::Orientation orientation) const
		{
			Element *element = part(index);
			if (!element) return nullptr;
			const QList<Terminal *> terminals = element->terminals();
			for (Terminal *terminal : terminals) {
				if (terminal->orientation() == orientation) return terminal;
			}
			return nullptr;
		}

		QUndoStack &stack() const {return *project->undoStack();}

		UiBench::ScratchProject project;
	};

		/// A press on @a from and a release on @a to, as the user draws
	Conductor *draw(Terminal *from, Terminal *to)
	{
		Diagram *sheet = from->diagram();
		const QList<Conductor *> before = sheet->conductors();
		QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
		press.setScenePos(from->dockConductor());
		press.setButton(Qt::LeftButton);
		press.setButtons(Qt::LeftButton);
		sheet->sendEvent(from, &press);
		QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
		release.setScenePos(to->dockConductor());
		release.setButton(Qt::LeftButton);
		sheet->sendEvent(from, &release);
		const QList<Conductor *> after = sheet->conductors();
		for (Conductor *c : after) {
			if (!before.contains(c)) return c;
		}
		return nullptr;
	}

	QString componentSection(Element *element)
	{
		return element->elementInformations()
				.value(QETInformation::ELMT_WIRE_SECTION).toString();
	}

	QString wireSection(Conductor *wire)
	{
		return wire->properties().m_wire_section;
	}

		/// What the information panel of the component pushes when the
		/// section is typed in it
	void setComponentSection(QUndoStack &stack, Element *element, const QString &section)
	{
		const DiagramContext old_info = element->elementInformations();
		DiagramContext new_info = old_info;
		new_info.addValue(QETInformation::ELMT_WIRE_SECTION, section);
		stack.push(new ChangeElementInformationCommand(element, old_info, new_info));
	}

		/// A wire numbering on @a sheet, so that "the same number" means a
		/// number and not two empty texts
	void useWireNumbering(QETProject *project, Diagram *sheet)
	{
		NumerotationContext context;
		context.addValue(QStringLiteral("unit"), 500, 1, 500);
		project->addConductorAutoNum(QStringLiteral("bench"), context);
		project->setCurrentConductorAutoNum(QStringLiteral("bench"));
		sheet->setConductorsAutonumName(QStringLiteral("bench"));
	}
}

TEST_CASE("CU-70.1 — The cross-section comes from the component", "[T70][wiregauge]")
{
	const Folio folio{{{Kind::Sensor, 100, 100, "0,5"},
			   {Kind::Relay, 400, 100, "1"}},
			  {}};
	Bench bench({folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Element *sensor = bench.part(0);
	Terminal *from = bench.side(0, Qet::East);
	Terminal *to = bench.side(1, Qet::West);
	REQUIRE((sensor && from && to));

		//The larger of the two: 1 against 0,5
	Conductor *wire = draw(from, to);
	REQUIRE(wire);
	CHECK(wireSection(wire) == mm2("1"));

		//The sensor raised to 1,5: the wire follows, in the same step
	QUndoStack &stack = bench.stack();
	const int index = stack.index();
	setComponentSection(stack, sensor, QStringLiteral("1,5"));
	CHECK(stack.index() == index + 1);
	CHECK(componentSection(sensor) == QStringLiteral("1,5"));
	CHECK(wireSection(wire) == mm2("1,5"));

		//One undo puts the two back
	stack.undo();
	CHECK(stack.index() == index);
	CHECK(componentSection(sensor) == QStringLiteral("0,5"));
	CHECK(wireSection(wire) == mm2("1"));

		//And one redo does the two again
	stack.redo();
	CHECK(componentSection(sensor) == QStringLiteral("1,5"));
	CHECK(wireSection(wire) == mm2("1,5"));
}

TEST_CASE("CU-70.2 — The terminal of a strip does not count", "[T70][wiregauge]")
{
	const Folio folio{{{Kind::Sensor, 100, 100, "0,5"},
			   {Kind::Strip, 300, 100, ""}},
			  {}};
	Bench bench({folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Element *strip = bench.part(1);
	Terminal *from = bench.side(0, Qet::East);
	Terminal *to = bench.side(1, Qet::West);
	REQUIRE((strip && from && to));
	REQUIRE(componentSection(strip).isEmpty());

	Conductor *wire = draw(from, to);
	REQUIRE(wire);
	CHECK(wireSection(wire) == mm2("0,5"));
}

TEST_CASE("CU-70.3 — The wires of one point have one cross-section", "[T70][wiregauge]")
{
		//Sensor 0,5 to a terminal of a strip, the strip to a relay 1, and
		//the relay on to a second terminal: three wires, one point, since
		//a terminal of a strip joins its two sides
	const Folio folio{{{Kind::Sensor, 100, 100, "0,5"},
			   {Kind::Strip, 250, 100, ""},
			   {Kind::Relay, 450, 100, "1"},
			   {Kind::Strip, 600, 250, ""}},
			  {}};
	Bench bench({folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Terminal *sensor = bench.side(0, Qet::East);
	Terminal *strip_in = bench.side(1, Qet::West);
	Terminal *strip_out = bench.side(1, Qet::East);
	Terminal *relay = bench.side(2, Qet::West);
	Terminal *second_strip = bench.side(3, Qet::West);
	REQUIRE((sensor && strip_in && strip_out && relay && second_strip));
	useWireNumbering(bench.project.project(), sensor->diagram());

		//Alone with the sensor, the point is 0,5
	Conductor *first = draw(sensor, strip_in);
	REQUIRE(first);
	CHECK(wireSection(first) == mm2("0,5"));

		//The relay comes in, and the point is 1 - the first wire too. This
		//wire copies the properties of the point instead of being numbered
	Conductor *second = draw(strip_out, relay);
	REQUIRE(second);
	CHECK(wireSection(second) == mm2("1"));
	CHECK(wireSection(first) == mm2("1"));

	Conductor *third = draw(relay, second_strip);
	REQUIRE(third);
	for (Conductor *wire : {first, second, third})
	{
		CHECK(wireSection(wire) == mm2("1"));
			//And the same number
		CHECK_FALSE(wire->properties().text.isEmpty());
		CHECK(wire->properties().text == first->properties().text);
	}

		//Undoing the wire that brought the relay gives the point back its
		//0,5, in the same step
	QUndoStack &stack = bench.stack();
	stack.undo();
	CHECK(wireSection(first) == mm2("1"));
	stack.undo();
	CHECK_FALSE(bench.project->diagrams().first()->conductors().contains(second));
	CHECK(wireSection(first) == mm2("0,5"));
}

TEST_CASE("T70 — Updating the cross-sections of a project", "[T70][wiregauge]")
{
		//Drawn before the components said a section: every wire of the
		//file has none, but the last one, typed by hand
	const Folio one{{{Kind::Sensor, 100, 100, "0,5"},
			 {Kind::Relay, 400, 100, "1"}},
			{{0, 1, ""}}};
	const Folio two_folio{{{Kind::Sensor, 100, 100, "1,5"},
			       {Kind::Strip, 400, 100, ""},
			       {Kind::Strip, 100, 300, ""},
			       {Kind::Relay, 400, 300, ""}},
			      {{2, 3, ""}, {4, 5, "2,5"}}};
	Bench bench({one, two_folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	REQUIRE(bench.project.diagramCount() == 2);
	Conductor *first = bench.wire(0);
	Conductor *second = bench.wire(1);
	Conductor *typed = bench.wire(2);
	REQUIRE((first && second && typed));
	REQUIRE(wireSection(first).isEmpty());
	REQUIRE(wireSection(second).isEmpty());
	REQUIRE(wireSection(typed) == QStringLiteral("2,5"));

	QUndoStack &stack = bench.stack();
	const int index = stack.index();
	CHECK(WireSections::updateProject(bench.project.project()) == 2);
		//One step per folio
	CHECK(stack.index() == index + 2);
	CHECK(wireSection(first) == mm2("1"));
	CHECK(wireSection(second) == mm2("1,5"));
		//No component of its point says a section: it keeps the one typed
	CHECK(wireSection(typed) == QStringLiteral("2,5"));

		//Nothing left to change, and no step for nothing
	CHECK(WireSections::updateProject(bench.project.project()) == 0);
	CHECK(stack.index() == index + 2);

		//One undo takes back the second folio, and only it
	stack.undo();
	CHECK(wireSection(second).isEmpty());
	CHECK(wireSection(first) == mm2("1"));
	stack.undo();
	CHECK(wireSection(first).isEmpty());
}

TEST_CASE("T70 — The text of the wire shows its cross-section", "[T70][wiregauge]")
{
	/*
		What %ws writes in the text of the wire is what the wire carries
		when the text is worked out. A placed wire answers its properties
		from the project's store, and Conductor::setProperties() works the
		formula out before it writes the store: without the formula reading
		the wire's own properties, the text shows the section the wire had
		before - the field right, the name on the folio wrong.
	*/
	const Folio folio{{{Kind::Sensor, 100, 100, "0,5"},
			   {Kind::Relay, 400, 100, "1"}},
			  {}};
	Bench bench({folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Element *sensor = bench.part(0);
	Terminal *from = bench.side(0, Qet::East);
	Terminal *to = bench.side(1, Qet::West);
	REQUIRE((sensor && from && to));
	Conductor *wire = draw(from, to);
	REQUIRE(wire);

	ConductorProperties properties = wire->properties();
	properties.m_formula = QStringLiteral("%ws");
	wire->setProperties(properties);
	REQUIRE(wire->properties().text == mm2("1"));

	QUndoStack &stack = bench.stack();
	setComponentSection(stack, sensor, QStringLiteral("1,5"));
	CHECK(wireSection(wire) == mm2("1,5"));
	CHECK(wire->properties().text == mm2("1,5"));

	stack.undo();
	CHECK(wireSection(wire) == mm2("1"));
	CHECK(wire->properties().text == mm2("1"));
}
