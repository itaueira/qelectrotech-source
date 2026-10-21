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

#include "../../../../sources/autoNum/assemblystate.h"
#include "../../../../sources/autoNum/assignvariables.h"
#include "../../../../sources/autoNum/numerotationcontext.h"
#include "../../../../sources/conductorautonumerotation.h"
#include "../../../../sources/QPropertyUndoCommand/qpropertyundocommand.h"
#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/assemblystatecommand.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"
#include "../../../../sources/ui/conductorpropertieseditorwidget.h"
#include "../../../../sources/wirecolorname.h"
#include "../../../../sources/wirenames.h"
#include "../../../../sources/wireremoval.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QCheckBox>
#include <QCoreApplication>
#include <QGraphicsSceneMouseEvent>
#include <QLatin1String>
#include <QSet>
#include <QStringList>
#include <QUndoStack>
#include <QUuid>

/*
	The name of a wire, F<folio>-<number on the folio> (<square>
	<cross-section>), given by the numbering of a folio that follows it, and
	a project whose names were typed by hand taken to it (T70).

	The wires are drawn the way the user draws them - a press on a terminal
	and a release on another - and a change of folio is what a change of tab
	does to it: Diagram::diagramActivated(), which puts the count of that
	folio in place (Diagram::loadCndFolioSeq()).

	The superscript two is written by its code, and the square by the
	program's own marker, so that this file says the same thing whatever it
	is opened as.
*/
namespace
{
	const QChar superscript_two(0x00B2);

		/// @a number followed by "mm²", the writing a wire carries
	QString mm2(const char *number)
	{
		return QString::fromLatin1(number) + QLatin1String("mm") + superscript_two;
	}

		/// "F<folio>-<number> (<square>)", the cross-section after the
		/// square when there is one
	QString wireName(int folio, int number, const char *section = "")
	{
		QString name = QStringLiteral("F%1-%2 (").arg(folio).arg(number);
		name += WireColorName::marker();
		if (*section) {
			name += QLatin1Char(' ');
			name += mm2(section);
		}
		name += QLatin1Char(')');
		return name;
	}

		/// A name typed by hand, as a project drawn before carries it
	QString typedName(int folio, int number, const char *section)
	{
		QString name = QStringLiteral("F%1-%2 (").arg(folio).arg(number);
		name += mm2(section);
		name += QLatin1Char(')');
		return name;
	}

		/// A component with a terminal on its right, one with a terminal on
		/// its left, the terminal of a strip, with one on each side, and the
		/// two arrows of a folio report: the one a wire leaves a folio by,
		/// its terminal on its left, and the one it arrives by, on its right
	enum class Kind {Sensor, Relay, Strip, Going, Coming};

	const char *kindName(Kind kind)
	{
		switch (kind)
		{
			case Kind::Sensor: return "sensor";
			case Kind::Relay:  return "relay";
			case Kind::Strip:  return "strip";
			case Kind::Going:  return "going";
			case Kind::Coming: return "coming";
		}
		return "";
	}

	bool hasEast(Kind kind)
	{
		return kind == Kind::Sensor || kind == Kind::Strip || kind == Kind::Coming;
	}
	bool hasWest(Kind kind)
	{
		return kind == Kind::Relay || kind == Kind::Strip || kind == Kind::Going;
	}

	QString linkType(Kind kind)
	{
		switch (kind)
		{
			case Kind::Strip:  return QStringLiteral("terminal");
			case Kind::Going:  return QStringLiteral("next_report");
			case Kind::Coming: return QStringLiteral("previous_report");
			default:           return QStringLiteral("simple");
		}
	}

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
			.arg(QLatin1String(kindName(kind)), linkType(kind), terminals);
	}

	struct Part
	{
		Kind kind;
		int x;
		int y;
			/// what the component says in "wire_section", "" for nothing
		const char *section;
			/// the arrow of the other folio an arrow is linked to, by its
			/// index in the project; -1 for none
		int linked = -1;
	};

		/// A wire written in the file, from the right terminal of the part
		/// @a from to the left terminal of the part @a to - the indexes
		/// counted over the whole project - with the text typed in it
	struct Wire
	{
		int from;
		int to;
		QString text;
	};

	struct Folio
	{
		QList<Part> parts;
		QList<Wire> wires;
	};

	QString partUuid(int index)
	{
		return QStringLiteral("{70d00000-0000-4000-8000-%1}")
				.arg(index, 12, 10, QLatin1Char('0'));
	}

	QString wireUuid(int index)
	{
		return QStringLiteral("{70e00000-0000-4000-8000-%1}")
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
		QString links;
		if (part.linked >= 0) {
			links = QStringLiteral("<link_uuid uuid=\"%1\"/>").arg(partUuid(part.linked));
		}
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/%3.elmt\" uuid=\"%4\">"
			"<terminals>%5</terminals><inputs/>"
			"<links_uuids>%7</links_uuids>"
			"<elementInformations>%6</elementInformations>"
			"<dynamic_texts/><texts_groups/>"
			"</element>")
			.arg(part.x).arg(part.y)
			.arg(QLatin1String(kindName(part.kind)), partUuid(index), terminals,
			     informations, links);
	}

	QString wireXml(int index, const Wire &wire)
	{
		return QStringLiteral(
			"<conductor terminal1=\"%1\" terminal2=\"%2\" num=\"%3\" uuid=\"%4\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>")
			.arg(2 * wire.from).arg(2 * wire.to + 1)
			.arg(wire.text.toHtmlEscaped(), wireUuid(index));
	}

	QString projectXml(const QList<Folio> &folios)
	{
		const QString definitions = definition(Kind::Sensor) + definition(Kind::Relay)
				+ definition(Kind::Strip) + definition(Kind::Going)
				+ definition(Kind::Coming);
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

		/// A wire drawn from the right of the part @a from to the left of
		/// the part @a to
	Conductor *drawBetween(const Bench &bench, int from, int to)
	{
		Terminal *start = bench.side(from, Qet::East);
		Terminal *end = bench.side(to, Qet::West);
		REQUIRE(start != nullptr);
		REQUIRE(end != nullptr);
		return draw(start, end);
	}

	QString textOf(Conductor *wire)
	{
		return wire->properties().text;
	}

	QString componentSection(Element *element)
	{
		return element->elementInformations()
				.value(QETInformation::ELMT_WIRE_SECTION).toString();
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

		/// The scheme of the name, as the numbering dialog writes it: the
		/// count of each folio starts from 1
	NumerotationContext dialogScheme()
	{
		NumerotationContext context;
		context.addValue(QStringLiteral("string"), QStringLiteral("F"));
		context.addValue(QStringLiteral("idfolio"), QString());
		context.addValue(QStringLiteral("string"), QStringLiteral("-"));
		context.addValue(QStringLiteral("unitfolio"), 1, 1, 1);
		context.addValue(QStringLiteral("string"), QStringLiteral(" (%wq %ws)"));
		return context;
	}

		/// @a sheets follow the scheme of the name, the one new wires take
	void followScheme(QETProject *project, const QList<Diagram *> &sheets)
	{
		project->addConductorAutoNum(QStringLiteral("bench"), dialogScheme());
		project->setCurrentConductorAutoNum(QStringLiteral("bench"));
		for (Diagram *sheet : sheets) {
			sheet->setConductorsAutonumName(QStringLiteral("bench"));
		}
	}

		/// What a change of tab does to the folio it shows
	void showFolio(Diagram *sheet)
	{
		emit sheet->diagramActivated();
	}

		/// Two folios: three pairs of components on the first, one on the
		/// second, nothing wired yet
	QList<Folio> twoFolios()
	{
		const Folio first{{{Kind::Sensor, 100, 100, "1"},
				   {Kind::Relay, 400, 100, ""},
				   {Kind::Sensor, 100, 300, "1,5"},
				   {Kind::Relay, 400, 300, ""},
				   {Kind::Sensor, 100, 500, ""},
				   {Kind::Relay, 400, 500, ""}},
				  {}};
		const Folio second{{{Kind::Sensor, 100, 100, "2,5"},
				    {Kind::Relay, 400, 100, ""}},
				   {}};
		return {first, second};
	}

		/**
			Five folios, the fifth drawn before the name had a numbering:
			a point of two wires through the terminal of a strip, typed
			F5-2 (1mm²); a point typed F5-7 (1,5mm²); a point with no
			text; and two components left unwired. No component says a
			cross-section.
		*/
	QList<Folio> drawnByHand()
	{
		const Folio empty{{}, {}};
		const Folio fifth{{{Kind::Sensor, 100, 100, ""},
				   {Kind::Strip, 250, 100, ""},
				   {Kind::Relay, 400, 100, ""},
				   {Kind::Sensor, 100, 250, ""},
				   {Kind::Relay, 400, 250, ""},
				   {Kind::Sensor, 100, 400, ""},
				   {Kind::Relay, 400, 400, ""},
				   {Kind::Sensor, 600, 250, ""},
				   {Kind::Relay, 800, 250, ""}},
				  {{0, 1, typedName(5, 2, "1")},
				   {1, 2, typedName(5, 2, "1")},
				   {3, 4, typedName(5, 7, "1,5")},
				   {5, 6, QString()}}};
		return {empty, empty, empty, empty, fifth};
	}

		/**
			Two folios joined by two folio reports, as the two between
			folios 4 and 5 of the project measured: a point typed F2-2
			(1mm²) on both ends - named after the folio it arrives on - and
			a point with no text on either end. On the second folio alone,
			a point typed F9-3 (1mm²), a folio it is not drawn on, one with
			no text, and two components left unwired. The relay the first
			point arrives at says 1 for its cross-section.
			Parts: 0 to 3 on the first folio, 4 to 13 on the second; wires:
			0 and 1 on the first, 2 to 5 on the second.
		*/
	QList<Folio> acrossReports()
	{
		const Folio first{{{Kind::Sensor, 100, 100, ""},
				   {Kind::Going, 300, 100, "", 4},
				   {Kind::Sensor, 100, 300, ""},
				   {Kind::Going, 300, 300, "", 6}},
				  {{0, 1, typedName(2, 2, "1")},
				   {2, 3, QString()}}};
		const Folio second{{{Kind::Coming, 100, 100, "", 1},
				    {Kind::Relay, 400, 100, "1"},
				    {Kind::Coming, 100, 300, "", 3},
				    {Kind::Relay, 400, 300, ""},
				    {Kind::Sensor, 100, 500, ""},
				    {Kind::Relay, 400, 500, ""},
				    {Kind::Sensor, 600, 100, ""},
				    {Kind::Relay, 800, 100, ""},
				    {Kind::Sensor, 600, 300, ""},
				    {Kind::Relay, 800, 300, ""}},
				   {{4, 5, typedName(2, 2, "1")},
				    {6, 7, QString()},
				    {8, 9, typedName(9, 3, "1")},
				    {10, 11, QString()}}};
		return {first, second};
	}

		/**
			A sensor and the arrow it leaves the first folio by, and the
			arrow it arrives by on the second, linked to it, with a relay:
			nothing wired yet. Parts 0 and 1 on the first folio, 2 and 3 on
			the second.
		*/
	QList<Folio> reportNotWired()
	{
		const Folio first{{{Kind::Sensor, 100, 100, ""},
				   {Kind::Going, 300, 100, "", 2}},
				  {}};
		const Folio second{{{Kind::Coming, 100, 100, "", 1},
				    {Kind::Relay, 400, 100, ""}},
				   {}};
		return {first, second};
	}
}

TEST_CASE("T70 — No cross-section, no space", "[T70][wirenames]")
{
	/*
		%ws with nothing in it takes the space written right before it
		along: "(%wq %ws)" closes on the square. One space, and only the
		one right before - the rest is what the formula says.
	*/
	const QString square(WireColorName::marker());
	autonum::sequentialNumbers seq;
	autonum::FormulaContext context;
	context.has_conductor = true;

	CHECK(autonum::AssignVariables::formulaToLabel(QStringLiteral("(%wq %ws)"), seq, context)
	      == QStringLiteral("(") + square + QStringLiteral(")"));
	CHECK(autonum::AssignVariables::formulaToLabel(QStringLiteral("(%wq  %ws)"), seq, context)
	      == QStringLiteral("(") + square + QStringLiteral(" )"));
	CHECK(autonum::AssignVariables::formulaToLabel(QStringLiteral("A%ws"), seq, context)
	      == QStringLiteral("A"));

		//The counter-needle: a cross-section keeps its space
	context.wire_section = mm2("1");
	CHECK(autonum::AssignVariables::formulaToLabel(QStringLiteral("(%wq %ws)"), seq, context)
	      == QStringLiteral("(") + square + QStringLiteral(" ") + mm2("1") + QStringLiteral(")"));
}

TEST_CASE("T70 — The number a typed name carries", "[T70][wirenames]")
{
	CHECK(WireNames::typedNumber(typedName(5, 2, "1")) == 2);
	CHECK(WireNames::typedNumber(typedName(5, 7, "1,5")) == 7);
	CHECK(WireNames::typedNumber(QStringLiteral("F12-34")) == 34);
	CHECK(WireNames::typedNumber(QStringLiteral("  F5-7")) == 7);
	CHECK(WireNames::typedNumber(QStringLiteral("F5-02 (x)")) == 2);
		//A name already converted reads the same
	CHECK(WireNames::typedNumber(wireName(3, 9)) == 9);

	CHECK(WireNames::typedNumber(QStringLiteral("F5")) == -1);
	CHECK(WireNames::typedNumber(QStringLiteral("F-2")) == -1);
	CHECK(WireNames::typedNumber(QStringLiteral("5-2")) == -1);
	CHECK(WireNames::typedNumber(QStringLiteral("XF5-2")) == -1);
	CHECK(WireNames::typedNumber(QStringLiteral("L1")) == -1);
	CHECK(WireNames::typedNumber(QString()) == -1);

		//And the scheme writes the formula
	CHECK(autonum::numerotationContextToFormula(WireNames::scheme()) == WireNames::formula());
}

TEST_CASE("CU-70.4 — The name of the wire", "[T70][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	REQUIRE(bench.project.diagramCount() == 2);
	Diagram *one = bench.project.diagram(0);
	Diagram *other = bench.project.diagram(1);
	REQUIRE(autonum::numerotationContextToFormula(dialogScheme()) == WireNames::formula());
	followScheme(bench.project.project(), {one, other});

	showFolio(one);
	Conductor *first = drawBetween(bench, 0, 1);
	Conductor *second = drawBetween(bench, 2, 3);
	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK(textOf(first) == wireName(1, 1, "1"));
	CHECK(textOf(second) == wireName(1, 2, "1,5"));

		//On the second folio, the count starts again
	showFolio(other);
	Conductor *third = drawBetween(bench, 6, 7);
	REQUIRE(third != nullptr);
	CHECK(textOf(third) == wireName(2, 1, "2,5"));
}

TEST_CASE("T70 — Back on a folio, its count goes on", "[T70][wirenames]")
{
	/*
		A folio shown a second time goes on from the highest number it
		gave, and does not start over: the next wire would otherwise take
		the number of the first one.
	*/
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	Diagram *other = bench.project.diagram(1);
	followScheme(bench.project.project(), {one, other});

	showFolio(one);
	REQUIRE(drawBetween(bench, 0, 1) != nullptr);
	REQUIRE(drawBetween(bench, 2, 3) != nullptr);
	showFolio(other);
	Conductor *elsewhere = drawBetween(bench, 6, 7);
	REQUIRE(elsewhere != nullptr);
	REQUIRE(textOf(elsewhere) == wireName(2, 1, "2,5"));

	showFolio(one);
	Conductor *back = drawBetween(bench, 4, 5);
	REQUIRE(back != nullptr);
	CHECK(textOf(back) == wireName(1, 3));
}

TEST_CASE("CU-70.8 — Converting a drawn project", "[T70][wirenames]")
{
	Bench bench(drawnByHand());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	REQUIRE(bench.project.diagramCount() == 5);
	QETProject *project = bench.project.project();
	Diagram *fifth = bench.project.diagram(4);
	Conductor *first = bench.wire(0);
	Conductor *first_on = bench.wire(1);
	Conductor *second = bench.wire(2);
	Conductor *third = bench.wire(3);
	REQUIRE((first && first_on && second && third));
	REQUIRE(textOf(first) == typedName(5, 2, "1"));
	REQUIRE(textOf(first_on) == typedName(5, 2, "1"));
	REQUIRE(textOf(second) == typedName(5, 7, "1,5"));
	REQUIRE(textOf(third).isEmpty());
	REQUIRE(componentSection(bench.part(0)).isEmpty());
	REQUIRE(WireNames::schemeOf(project).isEmpty());

	QUndoStack &stack = bench.stack();
	const int index = stack.index();
	CHECK(WireNames::convertProject(project, fifth) == 4);
	CHECK(stack.index() == index + 1);
		//Each point keeps its number, the one with none takes the next free
		//one, and the cross-section of the old text is gone: no component
		//says one
	CHECK(textOf(first) == wireName(5, 2));
	CHECK(textOf(first_on) == wireName(5, 2));
	CHECK(textOf(second) == wireName(5, 7));
	CHECK(textOf(third) == wireName(5, 8));
	CHECK(first->properties().m_formula == WireNames::formula());

		//The folio follows the numbering, the one new wires take
	const QString scheme = WireNames::schemeOf(project);
	CHECK_FALSE(scheme.isEmpty());
	CHECK(project->conductorCurrentAutoNum() == scheme);
	CHECK(fifth->conductorsAutonumName() == scheme);

		//Nothing left to change, and no step for nothing
	CHECK(WireNames::convertProject(project, fifth) == 0);
	CHECK(stack.index() == index + 1);

	SECTION("A wire drawn afterwards takes the next number")
	{
		Conductor *added = drawBetween(bench, 7, 8);
		REQUIRE(added != nullptr);
		CHECK(textOf(added) == wireName(5, 9));
	}

	SECTION("One undo puts the three texts back")
	{
		stack.undo();
		CHECK(stack.index() == index);
		CHECK(textOf(first) == typedName(5, 2, "1"));
		CHECK(textOf(first_on) == typedName(5, 2, "1"));
		CHECK(textOf(second) == typedName(5, 7, "1,5"));
		CHECK(textOf(third).isEmpty());
		CHECK(first->properties().m_formula.isEmpty());
		CHECK(WireNames::schemeOf(project).isEmpty());
		CHECK(fifth->conductorsAutonumName().isEmpty());

			//And a redo does it all again, the count of the folio with it
		stack.redo();
		CHECK(textOf(first) == wireName(5, 2));
		CHECK(textOf(third) == wireName(5, 8));
		Conductor *added = drawBetween(bench, 7, 8);
		REQUIRE(added != nullptr);
		CHECK(textOf(added) == wireName(5, 9));
	}

	SECTION("The cross-section of a component reaches the name")
	{
		setComponentSection(stack, bench.part(0), QStringLiteral("1"));
		CHECK(textOf(first) == wireName(5, 2, "1"));
		CHECK(textOf(first_on) == wireName(5, 2, "1"));
		CHECK(textOf(second) == wireName(5, 7));
	}
}

TEST_CASE("T70 — A wire of a built panel keeps its name", "[T70][wirenames]")
{
	const Folio folio{{{Kind::Sensor, 100, 100, ""},
			   {Kind::Relay, 400, 100, ""},
			   {Kind::Sensor, 100, 300, ""},
			   {Kind::Relay, 400, 300, ""}},
			  {{0, 1, typedName(1, 3, "1")},
			   {2, 3, QString()}}};
	Bench bench({folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Conductor *built = bench.wire(0);
	Conductor *drawn = bench.wire(1);
	REQUIRE((built && drawn));

		//The panel is wired with the first wire only in it
	AssemblyState state;
	state.stage = AssemblyStage::Assembled;
	state.conductors.insert(built->uuid().toString(), textOf(built));
	bench.stack().push(new AssemblyStateCommand(bench.project.project(), state));
	REQUIRE(ConductorAutoNumerotation::isFrozen(built));
	REQUIRE_FALSE(ConductorAutoNumerotation::isFrozen(drawn));

	CHECK(WireNames::convertProject(bench.project.project()) == 1);
	CHECK(textOf(built) == typedName(1, 3, "1"));
	CHECK(built->properties().m_formula.isEmpty());
		//Its number still counts: the next free one comes after it
	CHECK(textOf(drawn) == wireName(1, 4));
}

TEST_CASE("T70 — The project's own scheme is taken", "[T70][wirenames]")
{
	const Folio folio{{{Kind::Sensor, 100, 100, ""},
			   {Kind::Relay, 400, 100, ""}},
			  {{0, 1, QString()}}};
	Bench bench({folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	project->addConductorAutoNum(QStringLiteral("Own"), WireNames::scheme());
	const int schemes = project->conductorAutoNum().size();
	const QString current = project->conductorCurrentAutoNum();
	REQUIRE(current != QStringLiteral("Own"));

	CHECK(WireNames::convertProject(project) == 1);
		//No second scheme: the project's own, made the one new wires take
	CHECK(project->conductorAutoNum().size() == schemes);
	CHECK(project->conductorCurrentAutoNum() == QStringLiteral("Own"));
	CHECK(bench.project.diagram(0)->conductorsAutonumName() == QStringLiteral("Own"));
	CHECK(textOf(bench.wire(0)) == wireName(1, 1));

		//Undone, the scheme stays the project's, and is no longer current
	bench.stack().undo();
	CHECK(project->conductorAutoNum().contains(QStringLiteral("Own")));
	CHECK(project->conductorCurrentAutoNum() == current);
	CHECK(textOf(bench.wire(0)).isEmpty());
}

TEST_CASE("T70 — PE, N, F and the cables on conversion", "[T70][wirenames]")
{
	/*
		The texts that are not a number, as the projects drawn before carry
		them: the earth keeps its letters and takes no number; the neutral
		and the phase typed without a number take the next free one; a text
		with no cross-section is a cable, numbered, its type taken out of
		the text. One number typed, F1-2, so that the free ones start after
		it. Only the components of the first earth say a cross-section.
	*/
	const QString square(WireColorName::marker());
	const QString cable_flat = QStringLiteral("Cabo Flat (Paralelo) 26 Fios");
	QString foreseen = QStringLiteral("IHM (CAT6 STP) (Previs");
	foreseen += QChar(0x00E3);
	foreseen += QStringLiteral("o)");

		//The type of a cable, out of its text
	CHECK(WireNames::cableType(cable_flat) == QStringLiteral("Flat (Paralelo) 26 Fios"));
	CHECK(WireNames::cableType(QStringLiteral("UCM (CAT6 STP)")) == QStringLiteral("CAT6 STP"));
	CHECK(WireNames::cableType(foreseen) == QStringLiteral("CAT6 STP"));
	CHECK(WireNames::cableType(QStringLiteral(" Ethernet ")) == QStringLiteral("Ethernet"));
		//And a percent sign in it starts no variable
	{
		autonum::sequentialNumbers seq;
		seq.unit_folio << QStringLiteral("3");
		autonum::FormulaContext context;
		context.has_conductor = true;
		context.folio_index = 4;
		QString expected = QStringLiteral("F5-3 (") + square + QStringLiteral(" Cabo Shield 50");
		expected += QChar(0xFF05);
		expected += QStringLiteral("id)");
		CHECK(autonum::AssignVariables::formulaToLabel(
			      WireNames::cableFormula(QStringLiteral("Shield 50%id")), seq, context)
		      == expected);
	}

	QStringList typed;
	typed << typedName(1, 2, "1")
	      << QStringLiteral("PE (") + mm2("2,5") + QStringLiteral(")")
	      << QStringLiteral("PE (") + mm2("1,5") + QStringLiteral(")")
	      << QStringLiteral("N (") + mm2("2,5") + QStringLiteral(")")
	      << QStringLiteral("F (") + mm2("2,5") + QStringLiteral(")")
	      << QStringLiteral("UCM (CAT6 STP)")
	      << foreseen
	      << cable_flat
		//A short name with no "Cabo " and no type in brackets is a
		//potential, not a cable: numbered like N and F
	      << QStringLiteral("L1")
	      << QStringLiteral("24V");
		//Point k: a sensor 2 k and a relay 2 k + 1, one wire between them;
		//two components left unwired after them
	Folio folio;
	for (int k = 0 ; k < typed.size() ; ++k)
	{
		folio.parts << Part{Kind::Sensor, 100, 40 + 70 * k, k == 1 ? "2,5" : ""}
			    << Part{Kind::Relay, 300, 40 + 70 * k, ""};
		folio.wires << Wire{2 * k, 2 * k + 1, typed.at(k)};
	}
	const int spare = 2 * typed.size();
	folio.parts << Part{Kind::Sensor, 600, 40, ""}
		    << Part{Kind::Relay, 800, 40, ""};

	Bench bench({folio});
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *sheet = bench.project.diagram(0);
	QList<Conductor *> wires;
	for (int k = 0 ; k < typed.size() ; ++k)
	{
		wires << bench.wire(k);
		REQUIRE(wires.last() != nullptr);
		REQUIRE(textOf(wires.last()) == typed.at(k));
	}

	QUndoStack &stack = bench.stack();
	const int index = stack.index();
	CHECK(WireNames::convertProject(project, sheet) == typed.size());
	CHECK(stack.index() == index + 1);

		//The number typed is kept
	CHECK(textOf(wires.at(0)) == wireName(1, 2));
		//The earth: no number, and the cross-section of its point
	CHECK(textOf(wires.at(1)) == QStringLiteral("F1 PE (") + square + QStringLiteral(" ")
	      + mm2("2,5") + QStringLiteral(")"));
	CHECK(textOf(wires.at(2)) == QStringLiteral("F1 PE (") + square + QStringLiteral(")"));
	CHECK(wires.at(1)->properties().m_formula == WireNames::earthFormula());

		//The others numbered, each with a number of its own: which one
		//takes which follows the order the folio lists its conductors
	QSet<int> given;
	const auto numbered = [&](int k, const QString &tail)
	{
		const QString text = textOf(wires.at(k));
		INFO(text.toStdString());
		const int number = WireNames::typedNumber(text);
		CHECK(number > 0);
		CHECK(text == QStringLiteral("F1-%1 (").arg(number) + square + tail + QStringLiteral(")"));
		given.insert(number);
	};
	numbered(3, QString());
	numbered(4, QString());
	numbered(5, QStringLiteral(" Cabo CAT6 STP"));
	numbered(6, QStringLiteral(" Cabo CAT6 STP"));
	numbered(7, QStringLiteral(" Cabo Flat (Paralelo) 26 Fios"));
	numbered(8, QString());
	numbered(9, QString());
		//The seven numbers after the typed one: the earth took none
	CHECK(given == QSet<int>({3, 4, 5, 6, 7, 8, 9}));

		//Converted again, nothing changes: the earth and the cables are
		//known by their formula
	CHECK(WireNames::convertProject(project, sheet) == 0);
	CHECK(stack.index() == index + 1);
	CHECK(textOf(wires.at(7)).endsWith(QStringLiteral(" Cabo Flat (Paralelo) 26 Fios)")));

		//And the next wire drawn on the folio goes on after the highest
	Conductor *added = drawBetween(bench, spare, spare + 1);
	REQUIRE(added != nullptr);
	CHECK(textOf(added) == wireName(1, 10));
}

TEST_CASE("CU-70.9 — One name on both ends of a folio report", "[T70][wirenames]")
{
	Bench bench(acrossReports());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	REQUIRE(bench.project.diagramCount() == 2);
	QETProject *project = bench.project.project();
	Conductor *typed_leaving = bench.wire(0);
	Conductor *bare_leaving = bench.wire(1);
	Conductor *typed_arriving = bench.wire(2);
	Conductor *bare_arriving = bench.wire(3);
	Conductor *elsewhere = bench.wire(4);
	Conductor *alone = bench.wire(5);
	REQUIRE((typed_leaving && bare_leaving && typed_arriving && bare_arriving
		 && elsewhere && alone));
		//The fixture is what it says: the two ends of a report are one
		//point, and the two reports are two
	REQUIRE(typed_leaving->relatedPotentialConductors().contains(typed_arriving));
	REQUIRE(bare_leaving->relatedPotentialConductors().contains(bare_arriving));
	REQUIRE_FALSE(typed_leaving->relatedPotentialConductors().contains(bare_arriving));

	QUndoStack &stack = bench.stack();
	const int index = stack.index();
	CHECK(WireNames::convertProject(project, bench.project.diagram(1)) == 6);
	CHECK(stack.index() == index + 1);

		//The folio the typed name says, on both ends, with the cross-section
		//of the point - which only the end on the second folio says
	CHECK(textOf(typed_leaving) == wireName(2, 2, "1"));
	CHECK(textOf(typed_arriving) == wireName(2, 2, "1"));
		//No text: the first folio of the point, and the next number free on
		//it - the typed point is named on the second folio, and takes none
	CHECK(textOf(bare_leaving) == wireName(1, 1));
	CHECK(textOf(bare_arriving) == wireName(1, 1));
		//On one folio only, the folio of the wire: F9-3 is not drawn on
		//folio 9
	CHECK(textOf(elsewhere) == wireName(2, 3));
	CHECK(textOf(alone) == wireName(2, 4));
		//The folio written in the formula of a point across folios; %id
		//kept on a point of one folio
	CHECK(typed_leaving->properties().m_formula == WireNames::pinned(WireNames::formula(), 2));
	CHECK(typed_arriving->properties().m_formula == WireNames::pinned(WireNames::formula(), 2));
	CHECK(bare_arriving->properties().m_formula == WireNames::pinned(WireNames::formula(), 1));
	CHECK(alone->properties().m_formula == WireNames::formula());

		//Converted again, nothing changes
	CHECK(WireNames::convertProject(project, bench.project.diagram(1)) == 0);
	CHECK(stack.index() == index + 1);

	SECTION("A wire drawn afterwards on the second folio goes on after its highest")
	{
		Conductor *added = drawBetween(bench, 12, 13);
		REQUIRE(added != nullptr);
		CHECK(textOf(added) == wireName(2, 5));
	}

	SECTION("One undo puts the texts back")
	{
		stack.undo();
		CHECK(stack.index() == index);
		CHECK(textOf(typed_leaving) == typedName(2, 2, "1"));
		CHECK(textOf(typed_arriving) == typedName(2, 2, "1"));
		CHECK(textOf(bare_arriving).isEmpty());
		CHECK(bare_arriving->properties().m_formula.isEmpty());
	}
}

TEST_CASE("T70 — Converted, saved and opened again", "[T70][wirenames]")
{
	/*
		A project converted, saved and opened again has nothing left to
		convert: the conductors read back hold one empty value in each part
		of their count the file does not write, and that is no change. The
		name of a point across folios comes back from the file whole, and
		is worked out again with its number when a component changes.
	*/
	Bench bench(acrossReports());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	REQUIRE(WireNames::convertProject(bench.project.project()) == 6);
	REQUIRE(bench.project.saveAndReopen());
	INFO(bench.project.error().toStdString());

	CHECK(WireNames::convertProject(bench.project.project()) == 0);
	CHECK(textOf(bench.wire(0)) == wireName(2, 2, "1"));
	CHECK(textOf(bench.wire(2)) == wireName(2, 2, "1"));
	CHECK(textOf(bench.wire(1)) == wireName(1, 1));
	CHECK(textOf(bench.wire(3)) == wireName(1, 1));

	setComponentSection(bench.stack(), bench.part(5), QStringLiteral("1,5"));
	CHECK(textOf(bench.wire(0)) == wireName(2, 2, "1,5"));
	CHECK(textOf(bench.wire(2)) == wireName(2, 2, "1,5"));
}

TEST_CASE("T70 — A wire drawn across a folio report takes its point's name", "[T70][wirenames]")
{
	/*
		The point is named on the first folio, and a wire drawn on the
		second from the arrow it arrives by is the same point: it carries
		the same name, F1-1, and not the F2-1 the formula would work out on
		its own folio. The first folio is written in both formulas, so that
		the two ends keep one name.
	*/
	Bench bench(reportNotWired());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	Diagram *other = bench.project.diagram(1);
	followScheme(bench.project.project(), {one, other});

	showFolio(one);
	Conductor *leaving = drawBetween(bench, 0, 1);
	REQUIRE(leaving != nullptr);
	REQUIRE(textOf(leaving) == wireName(1, 1));
	REQUIRE(leaving->properties().m_formula == WireNames::formula());

	showFolio(other);
	QUndoStack &stack = bench.stack();
	const int index = stack.index();
	Conductor *arriving = drawBetween(bench, 2, 3);
	REQUIRE(arriving != nullptr);
	REQUIRE(leaving->relatedPotentialConductors().contains(arriving));
	CHECK(stack.index() == index + 1);
	CHECK(textOf(arriving) == wireName(1, 1));
	CHECK(textOf(leaving) == wireName(1, 1));
	CHECK(arriving->properties().m_formula == WireNames::pinned(WireNames::formula(), 1));
	CHECK(leaving->properties().m_formula == WireNames::pinned(WireNames::formula(), 1));

		//One undo takes the wire away, and gives the first end its
		//formula back
	stack.undo();
	CHECK(stack.index() == index);
	CHECK(textOf(leaving) == wireName(1, 1));
	CHECK(leaving->properties().m_formula == WireNames::formula());
}

TEST_CASE("T70 — A formula pinned to a folio", "[T70][wirenames]")
{
	const QString name = WireNames::formula();
	const QString earth = WireNames::earthFormula();
	const QString cable = WireNames::cableFormula(QStringLiteral("CAT6 STP"));
	CHECK(WireNames::pinned(name, 5) == QStringLiteral("F5-%sequf_1 (%wq %ws)"));
	CHECK(WireNames::pinned(earth, 12) == QStringLiteral("F12 PE (%wq %ws)"));
	CHECK(WireNames::pinned(cable, 5) == QStringLiteral("F5-%sequf_1 (%wq Cabo CAT6 STP)"));
		//And back, and known for what it is either way
	CHECK(WireNames::unpinned(WireNames::pinned(name, 5)) == name);
	CHECK(WireNames::unpinned(WireNames::pinned(earth, 12)) == earth);
	CHECK(WireNames::unpinned(WireNames::pinned(cable, 5)) == cable);
	CHECK(WireNames::isNameFormula(WireNames::pinned(cable, 5)));
		//Any other formula is left as it is
	CHECK(WireNames::pinned(QStringLiteral("%id-%sequf_1"), 5) == QStringLiteral("%id-%sequf_1"));
	CHECK(WireNames::unpinned(QStringLiteral("F5-%sequ_1")) == QStringLiteral("F5-%sequ_1"));
	CHECK_FALSE(WireNames::isNameFormula(QStringLiteral("F5-%sequ_1")));
		//The folio a typed name carries
	CHECK(WireNames::typedFolio(QStringLiteral("F5-2 (1mm2)")) == 5);
	CHECK(WireNames::typedFolio(QStringLiteral("F12 PE (2,5mm2)")) == 12);
	CHECK(WireNames::typedFolio(QStringLiteral("PE (2,5mm2)")) == -1);
	CHECK(WireNames::typedFolio(QStringLiteral("F (2,5mm2)")) == -1);
}

/*
	A wire left out of the automatic numbering (T77): its text stays, no
	routine writes it, nobody takes its name, and its number is not counted.
*/
namespace
{
		/// What the box of the wire panel pushes: one step for @a wires
	void leaveOut(QUndoStack &stack, const QList<Conductor *> &wires, bool ignored = true)
	{
		auto *undo = new QUndoCommand(QStringLiteral("ignore"));
		for (Conductor *wire : wires) {
			new QPropertyUndoCommand(wire, "freezeLabel", wire->isFreezeLabel(), ignored, undo);
		}
		stack.push(undo);
	}

	QCheckBox *ignoreBoxOf(QWidget *panel)
	{
		const QList<QCheckBox *> boxes = panel->findChildren<QCheckBox *>();
		for (QCheckBox *box : boxes) {
			if (box->text() == QLatin1String("Leave out of automatic numbering")) {
				return box;
			}
		}
		return nullptr;
	}
}

TEST_CASE("CU-77.1 — Leaving a wire out and bringing it back", "[T77][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	followScheme(bench.project.project(), {one, bench.project.diagram(1)});
	showFolio(one);
	Conductor *wire = drawBetween(bench, 0, 1);
	REQUIRE(wire != nullptr);
	REQUIRE(textOf(wire) == wireName(1, 1, "1"));
	QUndoStack &stack = bench.stack();

	leaveOut(stack, {wire});
	CHECK(wire->isFreezeLabel());
	CHECK(textOf(wire) == wireName(1, 1, "1"));

	SECTION("undone, it is back in the numbering")
	{
		stack.undo();
		CHECK_FALSE(wire->isFreezeLabel());
		CHECK(textOf(wire) == wireName(1, 1, "1"));
	}

	SECTION("a text typed while it is left out is kept, and the formula gives its name back")
	{
		ConductorProperties typed = wire->properties();
		typed.text = QStringLiteral("JUMPER");
		stack.push(new QPropertyUndoCommand(wire, "properties",
						    QVariant::fromValue(wire->properties()),
						    QVariant::fromValue(typed)));
		CHECK(textOf(wire) == QLatin1String("JUMPER"));
		CHECK_FALSE(wire->properties().m_formula.isEmpty());

		leaveOut(stack, {wire}, false);
		CHECK_FALSE(wire->isFreezeLabel());
		CHECK(textOf(wire) == wireName(1, 1, "1"));
	}
}

TEST_CASE("CU-77.2 — The cross-section does not rewrite a wire left out", "[T77][wirenames]")
{
	Bench bench(drawnByHand());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *fifth = bench.project.diagram(4);
	REQUIRE(WireNames::convertProject(project, fifth) == 4);
	Conductor *first = bench.wire(0);
	Conductor *first_on = bench.wire(1);
	REQUIRE((first && first_on));
	REQUIRE(textOf(first) == wireName(5, 2));
	REQUIRE(textOf(first_on) == wireName(5, 2));

		//The two wires of the point through the strip: one left out
	leaveOut(bench.stack(), {first_on});
	setComponentSection(bench.stack(), bench.part(0), QStringLiteral("2,5"));
	CHECK(textOf(first) == wireName(5, 2, "2,5"));
	CHECK(textOf(first_on) == wireName(5, 2));
	CHECK(first_on->properties().m_wire_section.isEmpty());
}

TEST_CASE("CU-77.3 — A new wire does not take the name of a wire left out", "[T77][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	followScheme(bench.project.project(), {one, bench.project.diagram(1)});
	showFolio(one);
	Conductor *left_out = drawBetween(bench, 0, 1);
	REQUIRE(left_out != nullptr);
	REQUIRE(textOf(left_out) == wireName(1, 1, "1"));
	leaveOut(bench.stack(), {left_out});

		//From the terminal whose only wire is left out: a new wire, numbered
		//as one, and not a copy of its name
	Conductor *added = drawBetween(bench, 0, 3);
	REQUIRE(added != nullptr);
	CHECK(textOf(added) == wireName(1, 2, "1"));
	CHECK(textOf(left_out) == wireName(1, 1, "1"));
	CHECK_FALSE(added->isFreezeLabel());
}

TEST_CASE("CU-77.4 — The conversion does not count a wire left out", "[T77][wirenames]")
{
	Bench bench(drawnByHand());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *fifth = bench.project.diagram(4);
	Conductor *first = bench.wire(0);
	Conductor *first_on = bench.wire(1);
	Conductor *seventh = bench.wire(2);
	Conductor *bare = bench.wire(3);
	REQUIRE((first && first_on && seventh && bare));
	REQUIRE(textOf(seventh) == typedName(5, 7, "1,5"));
	REQUIRE(textOf(bare).isEmpty());

	leaveOut(bench.stack(), {seventh});
	CHECK(WireNames::convertProject(project, fifth) == 3);
		//F5-7 is left as it was typed, and does not count: the wire with no
		//number takes the one after F5-2
	CHECK(textOf(seventh) == typedName(5, 7, "1,5"));
	CHECK(textOf(first) == wireName(5, 2));
	CHECK(textOf(bare) == wireName(5, 3));

	SECTION("a wire drawn afterwards goes on from there")
	{
		Conductor *added = drawBetween(bench, 7, 8);
		REQUIRE(added != nullptr);
		CHECK(textOf(added) == wireName(5, 4));
	}
}

TEST_CASE("CU-77.5 — A wire left out stays so when reopened", "[T77][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	followScheme(bench.project.project(), {one, bench.project.diagram(1)});
	showFolio(one);
	Conductor *left_out = drawBetween(bench, 0, 1);
	Conductor *numbered = drawBetween(bench, 2, 3);
	REQUIRE((left_out && numbered));
	const QUuid left_uuid = left_out->uuid();
	leaveOut(bench.stack(), {left_out});

	REQUIRE(bench.project.saveAndReopen());
	int marked = 0;
	bool found = false;
	const QList<Conductor *> wires = bench.project.diagram(0)->conductors();
	for (Conductor *wire : wires)
	{
		if (wire->isFreezeLabel()) {
			++marked;
		}
		if (wire->uuid() == left_uuid) {
			found = true;
			CHECK(wire->isFreezeLabel());
			CHECK(textOf(wire) == wireName(1, 1, "1"));
		}
	}
	CHECK(found);
	CHECK(marked == 1);
}

TEST_CASE("CU-77.6 — The panel box marks the selected wires in one step", "[T77][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	followScheme(bench.project.project(), {one, bench.project.diagram(1)});
	showFolio(one);
	Conductor *a = drawBetween(bench, 0, 1);
	Conductor *b = drawBetween(bench, 2, 3);
	Conductor *c = drawBetween(bench, 4, 5);
	REQUIRE((a && b && c));
	QUndoStack &stack = bench.stack();

	ConductorPropertiesEditorWidget panel({a, b});
	QCheckBox *box = ignoreBoxOf(&panel);
	REQUIRE(box != nullptr);
	CHECK_FALSE(box->isHidden());
	CHECK(box->checkState() == Qt::Unchecked);

	const int before = stack.index();
	box->click();
	CHECK(a->isFreezeLabel());
	CHECK(b->isFreezeLabel());
	CHECK_FALSE(c->isFreezeLabel());
	CHECK(stack.index() == before + 1);
	CHECK(box->checkState() == Qt::Checked);

	stack.undo();
	CHECK_FALSE(a->isFreezeLabel());
	CHECK_FALSE(b->isFreezeLabel());
	QCoreApplication::processEvents();
	CHECK(box->checkState() == Qt::Unchecked);
}

/*
	The step the entry of the context menu pushes (T77, step 5). The entry
	itself is not reached here: DiagramView::contextMenuActions() needs the
	editor window, which this suite cannot build (contextmenuassign_test.cpp
	measures why). It is on the queue of screen tests, as CU-77.7.
*/
TEST_CASE("T77 — The step of the context menu leaves the wires out", "[T77][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	followScheme(bench.project.project(), {one, bench.project.diagram(1)});
	showFolio(one);
	Conductor *a = drawBetween(bench, 0, 1);
	Conductor *b = drawBetween(bench, 2, 3);
	Conductor *c = drawBetween(bench, 4, 5);
	REQUIRE((a && b && c));
	QUndoStack &stack = bench.stack();

		//Nothing to bring back: no step at all
	CHECK(ConductorPropertiesEditorWidget::leaveOutCommand({a, b}, false) == nullptr);

		//One already out: the step holds the other only
	leaveOut(stack, {a});
	QUndoCommand *out = ConductorPropertiesEditorWidget::leaveOutCommand({a, b}, true);
	REQUIRE(out != nullptr);
	CHECK(out->childCount() == 1);
	const int before = stack.index();
	stack.push(out);
	CHECK(a->isFreezeLabel());
	CHECK(b->isFreezeLabel());
	CHECK_FALSE(c->isFreezeLabel());
	CHECK(stack.index() == before + 1);
	CHECK(ConductorPropertiesEditorWidget::leaveOutCommand({a, b}, true) == nullptr);

	SECTION("undone, the one it changed is back")
	{
		stack.undo();
		CHECK(a->isFreezeLabel());
		CHECK_FALSE(b->isFreezeLabel());
	}

	SECTION("both brought back in one step")
	{
		QUndoCommand *back = ConductorPropertiesEditorWidget::leaveOutCommand({a, b}, false);
		REQUIRE(back != nullptr);
		CHECK(back->childCount() == 2);
		stack.push(back);
		CHECK_FALSE(a->isFreezeLabel());
		CHECK_FALSE(b->isFreezeLabel());
		CHECK(stack.index() == before + 2);
	}
}

/*
	The wires numbered again from their place on the folio (T78): the
	conversion of the names, with the numbers worked out of the place of
	each name instead of read from it.
*/
namespace
{
		/**
			Two folios alike: a pair of components high on the right and
			one low on the left, wired, typed F<folio>-5 and F<folio>-9.
			Parts 0 to 3 on the first folio, 4 to 7 on the second; wires 0
			and 1 on the first, 2 and 3 on the second.
		*/
	QList<Folio> highRightLowLeft()
	{
		QList<Folio> folios;
		for (int f = 1 ; f <= 2 ; ++f)
		{
			const int base = (f - 1) * 4;
			folios << Folio{{{Kind::Sensor, 500, 100, ""},
					 {Kind::Relay, 650, 100, ""},
					 {Kind::Sensor, 100, 300, ""},
					 {Kind::Relay, 250, 300, ""}},
					{{base, base + 1, typedName(f, 5, "1")},
					 {base + 2, base + 3, typedName(f, 9, "1")}}};
		}
		return folios;
	}
}

TEST_CASE("CU-78.1 — Numbered again from 1, in reading order", "[T78][wirenames]")
{
	Bench bench(drawnByHand());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *fifth = bench.project.diagram(4);
	Conductor *first = bench.wire(0);
	Conductor *first_on = bench.wire(1);
	Conductor *second = bench.wire(2);
	Conductor *third = bench.wire(3);
	REQUIRE((first && first_on && second && third));

		//F5-2, F5-7 and a wire with no number, one under the other
	CHECK(WireNames::renumberProject(project, {fifth}, false, fifth) == 4);
	CHECK(textOf(first) == wireName(5, 1));
	CHECK(textOf(first_on) == wireName(5, 1));
	CHECK(textOf(second) == wireName(5, 2));
	CHECK(textOf(third) == wireName(5, 3));

	SECTION("the next wire goes on after the last")
	{
		Conductor *added = drawBetween(bench, 7, 8);
		REQUIRE(added != nullptr);
		CHECK(textOf(added) == wireName(5, 4));
	}

	SECTION("numbered again in the same order, nothing changes")
	{
		CHECK(WireNames::renumberProject(project, {fifth}, false, fifth) == 0);
	}
}

TEST_CASE("T78 — The count of a folio renumbered goes down", "[T78][wirenames]")
{
	Bench bench(drawnByHand());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *fifth = bench.project.diagram(4);

		//The conversion keeps F5-2 and F5-7 and gives F5-8: the count of
		//the folio is 8
	REQUIRE(WireNames::convertProject(project, fifth) == 4);
	REQUIRE(textOf(bench.wire(3)) == wireName(5, 8));

	REQUIRE(WireNames::renumberProject(project, {fifth}, false, fifth) == 4);
	REQUIRE(textOf(bench.wire(3)) == wireName(5, 3));
	Conductor *added = drawBetween(bench, 7, 8);
	REQUIRE(added != nullptr);
	CHECK(textOf(added) == wireName(5, 4));
}

TEST_CASE("CU-78.2 — Top to bottom, or left to right", "[T78][wirenames]")
{
	Bench bench(highRightLowLeft());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *one = bench.project.diagram(0);
	Conductor *high_right = bench.wire(0);
	Conductor *low_left = bench.wire(1);
	REQUIRE((high_right && low_left));

	WireNames::renumberProject(project, {one}, false, one);
	CHECK(textOf(high_right) == wireName(1, 1));
	CHECK(textOf(low_left) == wireName(1, 2));

	WireNames::renumberProject(project, {one}, true, one);
	CHECK(textOf(low_left) == wireName(1, 1));
	CHECK(textOf(high_right) == wireName(1, 2));
}

TEST_CASE("CU-78.3 — A wire left out and a panel built keep their numbers", "[T78][wirenames]")
{
	Bench bench(drawnByHand());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *fifth = bench.project.diagram(4);
	Conductor *built = bench.wire(0);
	Conductor *built_on = bench.wire(1);
	Conductor *left_out = bench.wire(2);
	Conductor *low = bench.wire(3);
	REQUIRE((built && built_on && left_out && low));
		//On the row of the wire left out, further right
	Conductor *drawn = drawBetween(bench, 7, 8);
	REQUIRE(drawn != nullptr);

		//The point typed F5-2 is on the panel; F5-7 is left out
	AssemblyState state;
	state.stage = AssemblyStage::Assembled;
	state.conductors.insert(built->uuid().toString(), textOf(built));
	state.conductors.insert(built_on->uuid().toString(), textOf(built_on));
	bench.stack().push(new AssemblyStateCommand(project, state));
	leaveOut(bench.stack(), {left_out});

	WireNames::renumberProject(project, {fifth}, false, fifth);
	CHECK(textOf(built) == typedName(5, 2, "1"));
	CHECK(textOf(left_out) == typedName(5, 7, "1,5"));
		//The first in reading order takes 1, the next skips the 2 of the
		//panel built
	CHECK(textOf(drawn) == wireName(5, 1));
	CHECK(textOf(low) == wireName(5, 3));
}

TEST_CASE("CU-78.4 — This folio only", "[T78][wirenames]")
{
	Bench bench(highRightLowLeft());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *two = bench.project.diagram(1);

	CHECK(WireNames::renumberProject(project, {two}, false, two) == 2);
	CHECK(textOf(bench.wire(2)) == wireName(2, 1));
	CHECK(textOf(bench.wire(3)) == wireName(2, 2));
		//The first folio as it was: not even converted
	CHECK(textOf(bench.wire(0)) == typedName(1, 5, "1"));
	CHECK(textOf(bench.wire(1)) == typedName(1, 9, "1"));
}

TEST_CASE("CU-78.5 — One undo puts every name back", "[T78][wirenames]")
{
	Bench bench(drawnByHand());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *fifth = bench.project.diagram(4);
	QUndoStack &stack = bench.stack();
	const int index = stack.index();

	REQUIRE(WireNames::renumberProject(project, project->diagrams(), false, fifth) == 4);
	CHECK(stack.index() == index + 1);
	CHECK(textOf(bench.wire(0)) == wireName(5, 1));

	stack.undo();
	CHECK(stack.index() == index);
	CHECK(textOf(bench.wire(0)) == typedName(5, 2, "1"));
	CHECK(textOf(bench.wire(1)) == typedName(5, 2, "1"));
	CHECK(textOf(bench.wire(2)) == typedName(5, 7, "1,5"));
	CHECK(textOf(bench.wire(3)).isEmpty());
}

/*
	The name of a wire left out of the numbering, when the wires are
	numbered again (T78, step 3): the one the numbering gave goes, the one
	typed by hand stays, and neither counts.
*/
namespace
{
		/// Three wires on the first folio of twoFolios(), named F1-1, F1-2
		/// and F1-3 by the scheme, top to bottom
	QList<Conductor *> threeNamed(Bench &bench)
	{
		Diagram *one = bench.project.diagram(0);
		followScheme(bench.project.project(), {one, bench.project.diagram(1)});
		showFolio(one);
		return {drawBetween(bench, 0, 1), drawBetween(bench, 2, 3), drawBetween(bench, 4, 5)};
	}

		/// The count new wires of @a folio go on from
	QString countOf(Diagram *folio)
	{
		const QString title = WireNames::schemeOf(folio->project());
		return folio->m_cnd_unitfolio_max.value(title).value(0);
	}

		/// What the text field of the wire panel pushes: the text, and the
		/// formula as it was
	void typeInPanel(QUndoStack &stack, Conductor *wire, const QString &text)
	{
		ConductorProperties typed = wire->properties();
		typed.text = text;
		stack.push(new QPropertyUndoCommand(wire, "properties",
						    QVariant::fromValue(wire->properties()),
						    QVariant::fromValue(typed)));
	}

		/// What typing on the folio pushes (Conductor::displayedTextChanged()):
		/// the text, and the formula made of it
	void typeOnFolio(QUndoStack &stack, Conductor *wire, const QString &text)
	{
		ConductorProperties typed = wire->properties();
		typed.text = text;
		typed.m_formula = text;
		stack.push(new QPropertyUndoCommand(wire, "properties",
						    QVariant::fromValue(wire->properties()),
						    QVariant::fromValue(typed)));
	}
}

TEST_CASE("CU-78.6 — The name the numbering gave a wire left out goes", "[T78][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *one = bench.project.diagram(0);
	const QList<Conductor *> wires = threeNamed(bench);
	Conductor *a = wires.value(0);
	Conductor *b = wires.value(1);
	Conductor *c = wires.value(2);
	REQUIRE((a && b && c));
	REQUIRE(textOf(b) == wireName(1, 2, "1,5"));
	REQUIRE(WireNames::isNumberingName(b));
		//What the undo has to bring back, kept before the step that clears it
	const ConductorProperties named = b->properties();
	REQUIRE_FALSE(named.text.isEmpty());
	REQUIRE_FALSE(named.m_formula.isEmpty());
	QUndoStack &stack = bench.stack();
	leaveOut(stack, {b});
	const int index = stack.index();

		//b loses its name, c takes the 2 b no longer counts for
	CHECK(WireNames::renumberProject(project, {one}, false, one) == 2);
	CHECK(textOf(b).isEmpty());
	CHECK(b->properties().m_formula.isEmpty());
	CHECK(b->isFreezeLabel());
	CHECK(textOf(a) == wireName(1, 1, "1"));
	CHECK(textOf(c) == wireName(1, 2));
	CHECK(countOf(one) == QLatin1String("2"));
	CHECK(stack.index() == index + 1);

	SECTION("numbered again, nothing more changes")
	{
		CHECK(WireNames::renumberProject(project, {one}, false, one) == 0);
	}

	SECTION("undone, the name is back")
	{
		stack.undo();
		CHECK(textOf(b) == named.text);
		CHECK(b->properties().m_formula == named.m_formula);
		CHECK(textOf(c) == wireName(1, 3));
	}

	SECTION("a folio out of the renumbering keeps the name")
	{
		stack.undo();
		WireNames::renumberProject(project, {bench.project.diagram(1)}, false, one);
		CHECK(textOf(b) == named.text);
	}
}

TEST_CASE("CU-78.7 — A name typed on a wire left out stays", "[T78][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *one = bench.project.diagram(0);
	const QList<Conductor *> wires = threeNamed(bench);
	Conductor *a = wires.value(0);
	Conductor *b = wires.value(1);
	Conductor *c = wires.value(2);
	REQUIRE((a && b && c));
	QUndoStack &stack = bench.stack();
	leaveOut(stack, {b});
	const QString typed = QStringLiteral("RESERVA");

	SECTION("typed in the panel, once the numbering cleared it")
	{
		WireNames::renumberProject(project, {one}, false, one);
		REQUIRE(textOf(b).isEmpty());
		typeInPanel(stack, b, typed);
	}
	SECTION("typed in the panel over the name the numbering gave")
	{
			//The formula is still there, and the text is not what it gives
		REQUIRE_FALSE(b->properties().m_formula.isEmpty());
		typeInPanel(stack, b, typed);
		REQUIRE_FALSE(b->properties().m_formula.isEmpty());
	}
	SECTION("typed on the folio")
	{
		typeOnFolio(stack, b, typed);
	}

	REQUIRE(textOf(b) == typed);
	CHECK_FALSE(WireNames::isNumberingName(b));
	WireNames::renumberProject(project, {one}, false, one);
	CHECK(textOf(b) == typed);
	CHECK(b->isFreezeLabel());
		//And it does not count
	CHECK(textOf(a) == wireName(1, 1, "1"));
	CHECK(textOf(c) == wireName(1, 2));
	CHECK(countOf(one) == QLatin1String("2"));
}

/*
	The wires of the project, or of a folio, removed at once to draw them
	again (T79): every wire but the ones left out of the numbering and those
	of a panel marked as built, in one undo step.
*/
namespace
{
		/// The wires of twoFolios(): three on the first folio, one on the second
	struct FourWires
	{
		QList<Conductor *> first;
		Conductor *second = nullptr;
	};

	FourWires fourWires(Bench &bench)
	{
		FourWires wires;
		wires.first = threeNamed(bench);
		wires.second = drawBetween(bench, 6, 7);
		return wires;
	}

	int elementsOf(const Bench &bench)
	{
		int count = 0;
		for (Diagram *folio : bench.project.diagrams()) {
			count += folio->elements().size();
		}
		return count;
	}
}

TEST_CASE("CU-79.1 — The wires of the whole project go", "[T79][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	const FourWires wires = fourWires(bench);
	REQUIRE(wires.first.size() == 3);
	REQUIRE(wires.second != nullptr);
	const int elements = elementsOf(bench);
	REQUIRE(elements > 0);

	const WireRemoval::Count count = WireRemoval::removeWires(project, project->diagrams());
	CHECK(count.removed == 4);
	CHECK(count.left_out == 0);
	CHECK(count.frozen == 0);
	CHECK(bench.project.diagram(0)->conductors().isEmpty());
	CHECK(bench.project.diagram(1)->conductors().isEmpty());
	CHECK(elementsOf(bench) == elements);
}

TEST_CASE("CU-79.2 — The wires of this folio only", "[T79][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	const FourWires wires = fourWires(bench);
	REQUIRE(wires.second != nullptr);

	const WireRemoval::Count count = WireRemoval::removeWires(project, {bench.project.diagram(0)});
	CHECK(count.removed == 3);
	CHECK(bench.project.diagram(0)->conductors().isEmpty());
	CHECK(bench.project.diagram(1)->conductors().size() == 1);
}

TEST_CASE("CU-79.3 — A wire left out and a wire of a panel built stay", "[T79][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *one = bench.project.diagram(0);
	const QList<Conductor *> wires = threeNamed(bench);
	Conductor *built = wires.value(0);
	Conductor *left_out = wires.value(1);
	REQUIRE((built && left_out && wires.value(2)));

	AssemblyState state;
	state.stage = AssemblyStage::Assembled;
	state.conductors.insert(built->uuid().toString(), textOf(built));
	bench.stack().push(new AssemblyStateCommand(project, state));
	leaveOut(bench.stack(), {left_out});

	const WireRemoval::Count count = WireRemoval::removeWires(project, {one});
	CHECK(count.removed == 1);
	CHECK(count.left_out == 1);
	CHECK(count.frozen == 1);
	const QList<Conductor *> kept = one->conductors();
	CHECK(kept.size() == 2);
	CHECK(kept.contains(built));
	CHECK(kept.contains(left_out));
}

TEST_CASE("CU-79.4 — One undo puts every wire back", "[T79][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	const FourWires wires = fourWires(bench);
	REQUIRE(wires.second != nullptr);
		//The names to come back, kept before they go
	QStringList names;
	for (Conductor *wire : wires.first) {
		names << textOf(wire);
	}
	names << textOf(wires.second);
	names.sort();
	REQUIRE(names.size() == 4);
	REQUIRE_FALSE(names.contains(QString()));
	QUndoStack &stack = bench.stack();
	const int index = stack.index();

	REQUIRE(WireRemoval::removeWires(project, project->diagrams()).removed == 4);
	CHECK(stack.index() == index + 1);

	stack.undo();
	CHECK(stack.index() == index);
	QStringList back;
	for (Diagram *folio : project->diagrams()) {
		for (Conductor *wire : folio->conductors()) {
			back << textOf(wire);
		}
	}
	back.sort();
	CHECK(back == names);
	CHECK(bench.project.diagram(0)->conductors().size() == 3);
	CHECK(bench.project.diagram(1)->conductors().size() == 1);
}

TEST_CASE("T79 — The count says it before, and removes nothing", "[T79][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	const FourWires wires = fourWires(bench);
	REQUIRE(wires.second != nullptr);
	leaveOut(bench.stack(), {wires.first.value(1)});
	const int index = bench.stack().index();

	const WireRemoval::Count all = WireRemoval::count(project->diagrams());
	CHECK(all.removed == 3);
	CHECK(all.left_out == 1);
	const WireRemoval::Count first = WireRemoval::count({bench.project.diagram(0)});
	CHECK(first.removed == 2);
	CHECK(bench.stack().index() == index);
	CHECK(bench.project.diagram(0)->conductors().size() == 3);

		//And nothing to remove is no step at all
	const WireRemoval::Count none = WireRemoval::removeWires(project, {});
	CHECK(none.removed == 0);
	CHECK(bench.stack().index() == index);
}

/*
	A wire with one end on a component kept out of the bill of materials
	is out of the automatic numbering (T81): by the rule, read from the
	component each time, and with nothing written on the wire.
*/
namespace
{
		/// What the information panel of the component pushes when its
		/// box "out of the list" is ticked or cleared
	void keepOutOfList(QUndoStack &stack, Element *element, bool out)
	{
		const DiagramContext old_info = element->elementInformations();
		DiagramContext new_info = old_info;
		if (out) {
			new_info.addValue(QStringLiteral("exclude_from_bom"), QStringLiteral("true"));
		} else {
			new_info.remove(QStringLiteral("exclude_from_bom"));
		}
		stack.push(new ChangeElementInformationCommand(element, old_info, new_info));
	}
}

TEST_CASE("CU-81.1 — A new wire of a component out of the list takes no number", "[T81][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	Diagram *one = bench.project.diagram(0);
	followScheme(bench.project.project(), {one, bench.project.diagram(1)});
	showFolio(one);
	Element *point = bench.part(0);
	REQUIRE(point != nullptr);
	keepOutOfList(bench.stack(), point, true);
	REQUIRE(ConductorAutoNumerotation::isOutOfBom(point));

	Conductor *out = drawBetween(bench, 0, 1);
	REQUIRE(out != nullptr);
	CHECK(ConductorAutoNumerotation::isIgnored(out));
	CHECK_FALSE(out->isFreezeLabel());
	CHECK(textOf(out) != wireName(1, 1, "1"));
		//It did not take the 1: the next wire does
	Conductor *next = drawBetween(bench, 2, 3);
	REQUIRE(next != nullptr);
	CHECK_FALSE(ConductorAutoNumerotation::isIgnored(next));
	CHECK(textOf(next) == wireName(1, 1, "1,5"));
}

TEST_CASE("CU-81.2 — Renumbered, the wire of a component out of the list loses its number, and comes back with it",
	  "[T81][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *one = bench.project.diagram(0);
	const QList<Conductor *> wires = threeNamed(bench);
	Conductor *a = wires.value(0);
	Conductor *b = wires.value(1);
	Conductor *c = wires.value(2);
	REQUIRE((a && b && c));
	REQUIRE(textOf(b) == wireName(1, 2, "1,5"));
	QUndoStack &stack = bench.stack();

		//The component of b, out of the list
	keepOutOfList(stack, bench.part(2), true);
	REQUIRE(ConductorAutoNumerotation::isIgnored(b));
	WireNames::renumberProject(project, {one}, false, one);
	CHECK(textOf(b).isEmpty());
	CHECK(textOf(a) == wireName(1, 1, "1"));
	CHECK(textOf(c) == wireName(1, 2));
	CHECK_FALSE(b->isFreezeLabel());

		//Back on the list: renumbered, it counts again, in its place
	keepOutOfList(stack, bench.part(2), false);
	CHECK_FALSE(ConductorAutoNumerotation::isIgnored(b));
	WireNames::renumberProject(project, {one}, false, one);
	CHECK(textOf(a) == wireName(1, 1, "1"));
	CHECK(textOf(b) == wireName(1, 2, "1,5"));
	CHECK(textOf(c) == wireName(1, 3));
}

TEST_CASE("T81 — Removing the wires keeps the marked wire, and not the one of a component out of the list",
	  "[T81][wirenames]")
{
	Bench bench(twoFolios());
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.project.isOpen());
	QETProject *project = bench.project.project();
	Diagram *one = bench.project.diagram(0);
	const QList<Conductor *> wires = threeNamed(bench);
	REQUIRE(wires.size() == 3);
	keepOutOfList(bench.stack(), bench.part(0), true);
	leaveOut(bench.stack(), {wires.value(1)});

	const WireRemoval::Count count = WireRemoval::removeWires(project, {one});
	CHECK(count.removed == 2);
	CHECK(count.left_out == 1);
	const QList<Conductor *> kept = one->conductors();
	CHECK(kept.size() == 1);
	CHECK(kept.contains(wires.value(1)));
}
