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

#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/ElementsCollection/embeddedelementrefresh.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/scripting/qetscriptapi.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QPainterPath>
#include <QPointF>
#include <QStringList>
#include <QUuid>
#include <QVariantMap>

#include <memory>

/*
	The terminals of the symbols of a project put on whole numbers (T72):
	the copy of the symbol in the project changed, the element placed from
	it reloaded from memory, and every wire on the terminal it was on.

	The symbol is the input module the defect was measured on, its four
	terminals at x -21.92 and y -15.83, -4.83, 6.17 and 17.17, placed at
	(613, 644): its first terminal lands at y 628.17 on the folio, 0.17 off
	the arrow wired to it at y 628. Two arrows are wired to it: the first by
	a wire drawn by hand, with the step of 0.17 in it, which is what a
	reload alone would keep and what the wire has to lose; the second by a
	wire the program lays itself. The first terminal also carries a name
	given on the folio, which the element reads only at the place of the
	terminal.

	Everything goes through what the command calls, without its two message
	boxes: a modal box under the offscreen platform waits forever.
*/
namespace
{
	const QString module_path = QStringLiteral("import/boards/mod-entrada.elmt");
	const QString arrow_path = QStringLiteral("import/bench/arrow.elmt");

	const QString module_uuid = QStringLiteral("{72400000-0000-4000-8000-000000000001}");
	const QString first_arrow_uuid = QStringLiteral("{72400000-0000-4000-8000-000000000002}");
	const QString second_arrow_uuid = QStringLiteral("{72400000-0000-4000-8000-000000000003}");

	const QString first_terminal = QStringLiteral("{72410000-0000-4000-8000-000000000001}");
	const QString third_terminal = QStringLiteral("{72410000-0000-4000-8000-000000000003}");
	const QString arrow_terminal = QStringLiteral("{72410000-0000-4000-8000-00000000000a}");

	const QString hand_wire_uuid = QStringLiteral("{72420000-0000-4000-8000-000000000001}");
	const QString laid_wire_uuid = QStringLiteral("{72420000-0000-4000-8000-000000000002}");

	QString moduleDefinition()
	{
		return QStringLiteral(
			"<element name=\"mod-entrada.elmt\">"
			"<definition type=\"element\" version=\"0.80\" width=\"50\" height=\"50\""
			" hotspot_x=\"25\" hotspot_y=\"25\" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">Input module</name></names>"
			"<description>"
			"<rect x=\"-21.92\" y=\"-20\" width=\"40\" height=\"40\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"-21.92\" y=\"-15.83\" orientation=\"w\" name=\"1\" uuid=\"%1\"/>"
			"<terminal x=\"-21.92\" y=\"-4.83\" orientation=\"w\" name=\"2\""
			" uuid=\"{72410000-0000-4000-8000-000000000002}\"/>"
			"<terminal x=\"-21.92\" y=\"6.17\" orientation=\"w\" name=\"3\" uuid=\"%2\"/>"
			"<terminal x=\"-21.92\" y=\"17.17\" orientation=\"w\" name=\"4\""
			" uuid=\"{72410000-0000-4000-8000-000000000004}\"/>"
			"</description>"
			"</definition>"
			"</element>").arg(first_terminal, third_terminal);
	}

	QString arrowDefinition()
	{
		return QStringLiteral(
			"<element name=\"arrow.elmt\">"
			"<definition type=\"element\" version=\"0.80\" width=\"20\" height=\"20\""
			" hotspot_x=\"10\" hotspot_y=\"10\" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">Arrow</name></names>"
			"<description>"
			"<line x1=\"-8\" y1=\"0\" x2=\"8\" y2=\"0\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\" uuid=\"%1\"/>"
			"</description>"
			"</definition>"
			"</element>").arg(arrow_terminal);
	}

		/// An element as a folio saves it: the place of each of its
		/// terminals is written at the end the wires do not dock on
	QString elementXml(const QString &path, const QString &uuid, int x, int y,
					   const QString &terminals)
	{
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://%3\" uuid=\"%4\">"
			"<terminals>%5</terminals><inputs/><links_uuids/><elementInformations/>"
			"<dynamic_texts/><texts_groups/>"
			"</element>")
			.arg(QString::number(x), QString::number(y), path, uuid, terminals);
	}

	QString projectXml()
	{
			//West terminals saved four to the right of where wires dock, the
			//east one of the arrow four to the left (Terminal::init())
		const QString module_terminals = QStringLiteral(
			"<terminal x=\"-17.92\" y=\"-15.83\" orientation=\"3\" id=\"0\" name=\"X1:1\"/>"
			"<terminal x=\"-17.92\" y=\"-4.83\" orientation=\"3\" id=\"1\"/>"
			"<terminal x=\"-17.92\" y=\"6.17\" orientation=\"3\" id=\"2\"/>"
			"<terminal x=\"-17.92\" y=\"17.17\" orientation=\"3\" id=\"3\"/>");
		const QString elements =
				elementXml(module_path, module_uuid, 613, 644, module_terminals)
				+ elementXml(arrow_path, first_arrow_uuid, 500, 628, QStringLiteral(
							 "<terminal x=\"6\" y=\"0\" orientation=\"1\" id=\"4\"/>"))
				+ elementXml(arrow_path, second_arrow_uuid, 500, 650, QStringLiteral(
							 "<terminal x=\"6\" y=\"0\" orientation=\"1\" id=\"5\"/>"));

			//From the arrow at (510, 628) to the first terminal at
			//(591.08, 628.17): a run, the step of 0.17, and a run
		const QString hand_wire = QStringLiteral(
			"<conductor element1=\"%1\" terminal1=\"%2\" element2=\"%3\" terminal2=\"%4\""
			" uuid=\"%5\" num=\"\" displaytext=\"1\" type=\"multi\" condsize=\"1\">"
			"<segment orientation=\"horizontal\" length=\"40\"/>"
			"<segment orientation=\"vertical\" length=\"0.17\"/>"
			"<segment orientation=\"horizontal\" length=\"41.08\"/>"
			"</conductor>")
			.arg(first_arrow_uuid, arrow_terminal, module_uuid, first_terminal, hand_wire_uuid);
		const QString laid_wire = QStringLiteral(
			"<conductor element1=\"%1\" terminal1=\"%2\" element2=\"%3\" terminal2=\"%4\""
			" uuid=\"%5\" num=\"\" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>")
			.arg(second_arrow_uuid, arrow_terminal, module_uuid, third_terminal, laid_wire_uuid);

		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection>"
			"<category name=\"import\">"
			"<category name=\"boards\">%1</category>"
			"<category name=\"bench\">%2</category>"
			"</category>"
			"</collection>"
			"<diagram title=\"Bench 1\" order=\"1\" folio=\"1\" height=\"800\""
			" cols=\"17\" colsize=\"60\" rows=\"10\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%3</elements><inputs/><conductors>%4</conductors>"
			"</diagram>"
			"</project>")
			.arg(moduleDefinition(), arrowDefinition(), elements, hand_wire + laid_wire);
	}

	Element *elementWithUuid(QETProject *project, const QString &uuid)
	{
		const QList<Diagram *> diagrams = project->diagrams();
		for (Diagram *diagram : diagrams) {
			const DiagramContent content(diagram, false);
			for (Element *element : content.m_elements) {
				if (element->uuid() == QUuid(uuid)) {
					return element;
				}
			}
		}
		return nullptr;
	}

	Terminal *terminalWithUuid(const Element *element, const QString &uuid)
	{
		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *terminal : terminals) {
			if (terminal->uuid() == QUuid(uuid)) {
				return terminal;
			}
		}
		return nullptr;
	}

	Conductor *wireWithUuid(QETProject *project, const QString &uuid)
	{
		const QList<Diagram *> diagrams = project->diagrams();
		for (Diagram *diagram : diagrams) {
			const QList<Conductor *> wires = diagram->conductors();
			for (Conductor *wire : wires) {
				if (wire->uuid() == QUuid(uuid)) {
					return wire;
				}
			}
		}
		return nullptr;
	}

	int wireCount(QETProject *project)
	{
		int count = 0;
		const QList<Diagram *> diagrams = project->diagrams();
		for (Diagram *diagram : diagrams) {
			count += diagram->conductors().size();
		}
		return count;
	}

		/// The y of each terminal of the project's copy at @a path
	QStringList terminalYs(QETProject *project, const QString &path)
	{
		QStringList ys;
		const QDomElement description = ElementsLocation(path, project).xml()
				.firstChildElement(QStringLiteral("description"));
		for (QDomElement terminal = description.firstChildElement(QStringLiteral("terminal")) ;
			 !terminal.isNull() ;
			 terminal = terminal.nextSiblingElement(QStringLiteral("terminal"))) {
			ys << terminal.attribute(QStringLiteral("y"));
		}
		return ys;
	}

	QStringList terminalXs(QETProject *project, const QString &path)
	{
		QStringList xs;
		const QDomElement description = ElementsLocation(path, project).xml()
				.firstChildElement(QStringLiteral("description"));
		for (QDomElement terminal = description.firstChildElement(QStringLiteral("terminal")) ;
			 !terminal.isNull() ;
			 terminal = terminal.nextSiblingElement(QStringLiteral("terminal"))) {
			xs << terminal.attribute(QStringLiteral("x"));
		}
		return xs;
	}

		/// The points @a wire is drawn through, on the folio
	QList<QPointF> pathOf(const Conductor *wire)
	{
		QList<QPointF> points;
		const QPainterPath path = wire->path();
		for (int i = 0 ; i < path.elementCount() ; ++i) {
			const QPainterPath::Element point = path.elementAt(i);
			points << wire->mapToScene(QPointF(point.x, point.y));
		}
		return points;
	}

		/// Whether @a wire is on the terminal of uuid @a terminal_uuid of the
		/// element of uuid @a element_uuid, at one of its ends
	bool joins(const Conductor *wire, const QString &element_uuid, const QString &terminal_uuid)
	{
		for (const Terminal *end : {wire->terminal1, wire->terminal2}) {
			if (end->parentElement()->uuid() == QUuid(element_uuid)
					&& end->uuid() == QUuid(terminal_uuid)) {
				return true;
			}
		}
		return false;
	}
}

TEST_CASE("CU-72.4 — Fixing the terminals of the symbols", "[T72][symbolterminals]")
{
	UiBench::ScratchProject bench(projectXml());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();

		//The defect, before anything: the terminal 0.17 off the arrow, and
		//both wires on the module
	Element *module = elementWithUuid(project, module_uuid);
	REQUIRE(module);
	Terminal *first = terminalWithUuid(module, first_terminal);
	REQUIRE(first);
	REQUIRE(first->dockConductor().y() == Approx(628.17));
	REQUIRE(first->name() == QStringLiteral("X1:1"));
	REQUIRE(wireCount(project) == 2);
	REQUIRE(wireWithUuid(project, hand_wire_uuid));
	REQUIRE(wireWithUuid(project, laid_wire_uuid));

		//What the command tells the user before it runs
	const EmbeddedElementRefresh::OffWholeNumbers off =
			EmbeddedElementRefresh::offWholeNumbers(project);
	CHECK(off.copies == QStringList({module_path}));
	CHECK(off.terminals == 4);
	CHECK(off.placed == 1);
	CHECK(off.merged.isEmpty());

	QString error;
	std::unique_ptr<QETProject> reloaded(
				EmbeddedElementRefresh::reopenedOnWholeNumbers(project, &error));
	CAPTURE(error);
	REQUIRE(reloaded);
	CHECK(reloaded->filePath() == project->filePath());
	CHECK(reloaded->projectWasModified());

		//The copy of the symbol: every terminal on whole numbers, the
		//drawing as it was, the arrow untouched
	CHECK(terminalXs(reloaded.get(), module_path)
		  == QStringList({QStringLiteral("-22"), QStringLiteral("-22"),
						  QStringLiteral("-22"), QStringLiteral("-22")}));
	CHECK(terminalYs(reloaded.get(), module_path)
		  == QStringList({QStringLiteral("-16"), QStringLiteral("-5"),
						  QStringLiteral("6"), QStringLiteral("17")}));
	CHECK(ElementsLocation(module_path, reloaded.get()).xml()
		  .firstChildElement(QStringLiteral("description"))
		  .firstChildElement(QStringLiteral("rect")).attribute(QStringLiteral("x"))
		  == QStringLiteral("-21.92"));
	CHECK(terminalYs(reloaded.get(), arrow_path) == QStringList({QStringLiteral("0")}));
	CHECK(EmbeddedElementRefresh::offWholeNumbers(reloaded.get()).copies.isEmpty());

		//The element placed where it was, its terminal on y 628
	Element *rebuilt = elementWithUuid(reloaded.get(), module_uuid);
	REQUIRE(rebuilt);
	CHECK(rebuilt->pos() == QPointF(613, 644));
	Terminal *first_now = terminalWithUuid(rebuilt, first_terminal);
	Terminal *third_now = terminalWithUuid(rebuilt, third_terminal);
	REQUIRE(first_now);
	REQUIRE(third_now);
	CHECK(first_now->dockConductor().x() == Approx(591).margin(1e-6));
	CHECK(first_now->dockConductor().y() == Approx(628).margin(1e-6));
	CHECK(third_now->dockConductor().y() == Approx(650).margin(1e-6));
		//The name given on the folio came with the terminal
	CHECK(first_now->name() == QStringLiteral("X1:1"));

		//Every wire, on the same terminals
	CHECK(EmbeddedElementRefresh::wiresLeftOut(reloaded.get()).isEmpty());
	CHECK(wireCount(reloaded.get()) == 2);
	Conductor *hand_wire = wireWithUuid(reloaded.get(), hand_wire_uuid);
	Conductor *laid_wire = wireWithUuid(reloaded.get(), laid_wire_uuid);
	REQUIRE(hand_wire);
	REQUIRE(laid_wire);
	CHECK(joins(hand_wire, first_arrow_uuid, arrow_terminal));
	CHECK(joins(hand_wire, module_uuid, first_terminal));
	CHECK(joins(laid_wire, second_arrow_uuid, arrow_terminal));
	CHECK(joins(laid_wire, module_uuid, third_terminal));

		//And drawn to them: the wire drawn by hand ends on the terminal and
		//lost its step - one straight line at y 628
	for (const Conductor *wire : {hand_wire, laid_wire})
	{
		const QList<QPointF> points = pathOf(wire);
		REQUIRE(points.size() >= 2);
		CHECK(points.first().x() == Approx(wire->terminal1->dockConductor().x()).margin(0.01));
		CHECK(points.first().y() == Approx(wire->terminal1->dockConductor().y()).margin(0.01));
		CHECK(points.last().x() == Approx(wire->terminal2->dockConductor().x()).margin(0.01));
		CHECK(points.last().y() == Approx(wire->terminal2->dockConductor().y()).margin(0.01));
	}
	for (const QPointF &point : pathOf(hand_wire)) {
		CHECK(point.y() == Approx(628).margin(0.01));
	}
}

TEST_CASE("T72 — The script fixes the terminals too, headless", "[T72][symbolterminals]")
{
	UiBench::ScratchProject bench(projectXml());
	REQUIRE(bench.isOpen());

	QetScriptApi api(bench.project());
	CHECK(api.roundSymbolTerminals() == 4);
	CHECK(api.conductorCount(0) == 2);
	const int index = api.terminalIndex(0, module_uuid, first_terminal);
	REQUIRE(index >= 0);
	const QVariantMap at = api.terminalPosition(0, module_uuid, index);
	CHECK(at.value(QStringLiteral("y")).toDouble() == Approx(628).margin(1e-6));
		//Twice is once: nothing is off any more
	CHECK(api.roundSymbolTerminals() == 0);
}
