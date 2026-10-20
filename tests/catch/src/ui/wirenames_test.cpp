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
#include "../../../../sources/wirecolorname.h"
#include "../../../../sources/wirenames.h"

#include <catch2/catch.hpp>

#include <QChar>
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
		return QStringLiteral(
			"<conductor terminal1=\"%1\" terminal2=\"%2\" num=\"%3\" uuid=\"%4\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>")
			.arg(2 * wire.from).arg(2 * wire.to + 1)
			.arg(wire.text.toHtmlEscaped(), wireUuid(index));
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
