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

#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/TerminalStrip/GraphicsItem/trueterminalstrip.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/undocommand/addgraphicsobjectcommand.h"

#include <catch2/catch.hpp>

#include <QSharedPointer>
#include <QString>
#include <QUndoStack>

/*
	Where the wires of a terminal go, as the plan of a terminal strip
	writes them above and under each terminal: "K1:24", the tag of the
	element at the other end and the name of its point; "X2:2" when the
	other end is the terminal 2 of the strip X2; "Free" for a point with
	no wire; all the wires of a point, joined by " / ".

	The entry of a terminal is the first point of its symbol and its exit
	the second, in the order of the symbol file - here the top one and the
	bottom one -, whatever the rotation of the symbol on the folio.

	The bench: terminals 1, 2 and 3, of two points each, and the elements
	K1 and M1, whose two points are named 24 and A1. The wires are drawn by
	the test, in the order the cases read them.
*/

namespace
{
	struct Instance
	{
		const char *definition;
		int x;
		int y;
		const char *label;
	};

	const Instance bench_instances[] = {
		{"terminal.elmt", 200, 200, "1"},
		{"terminal.elmt", 300, 200, "2"},
		{"terminal.elmt", 400, 200, "3"},
		{"device.elmt",   200, 80,  "K1"},
		{"device.elmt",   200, 320, "M1"}};

	QString definition(const char *name, const char *link_type,
			   const char *top_name, const char *bottom_name)
	{
		return QStringLiteral(
			       "<element name=\"%1\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"40\""
			       " hotspot_x=\"10\" hotspot_y=\"20\""
			       " orientation=\"dnnn\" link_type=\"%2\">"
			       "<names><name lang=\"en\">%1</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"n\" name=\"%3\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"s\" name=\"%4\"/>"
			       "</description>"
			       "</definition>"
			       "</element>")
		       .arg(QLatin1String(name), QLatin1String(link_type),
			    QLatin1String(top_name), QLatin1String(bottom_name));
	}

	QString fixtureXml()
	{
			// The docking point of a terminal, which is what the instance stores
		const qreal north_dock = -10. + Terminal::terminalSize;
		const qreal south_dock = 10. - Terminal::terminalSize;

		QString instances;
		int index = 0;
		for (const Instance &instance : bench_instances)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/%3\""
					     " uuid=\"{de57a000-0000-4000-8000-00000000000%4}\">"
					     "<terminals>"
					     "<terminal x=\"0\" y=\"%5\" orientation=\"0\" id=\"%6\"/>"
					     "<terminal x=\"0\" y=\"%7\" orientation=\"2\" id=\"%8\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%9"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(instance.x)
				     .arg(instance.y)
				     .arg(QLatin1String(instance.definition))
				     .arg(index)
				     .arg(north_dock)
				     .arg(index * 2)
				     .arg(south_dock)
				     .arg(index * 2 + 1)
				     .arg(QLatin1String(instance.label));
			++index;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">%1%2</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " folio=\"1\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%3</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definition("terminal.elmt", "terminal", "", ""),
			    definition("device.elmt", "simple", "24", "A1"),
			    instances);
	}

	/// The element of @a sheet tagged @a label, nullptr when none
	Element *elementTagged(Diagram *sheet, const char *label)
	{
		if (!sheet) {
			return nullptr;
		}
		const auto elements = sheet->elements();
		for (Element *element : elements) {
			if (element->actualLabel() == QLatin1String(label)) {
				return element;
			}
		}
		return nullptr;
	}

	/// Point @a index (0: the first of the symbol) of the element tagged @a label
	Terminal *point(Diagram *sheet, const char *label, int index)
	{
		Element *element = elementTagged(sheet, label);
		if (!element || element->terminals().size() <= index) {
			return nullptr;
		}
		return element->terminals().at(index);
	}

	/// Draw a wire from @a a to @a b, as the folio does
	bool wire(Diagram *sheet, Terminal *a, Terminal *b)
	{
		if (!sheet || !a || !b) {
			return false;
		}
		auto *conductor = new Conductor(a, b);
		sheet->undoStack().push(new AddGraphicsObjectCommand(conductor, sheet, QPointF()));
		return true;
	}

	QSharedPointer<RealTerminal> realTerminal(Diagram *sheet, const char *label)
	{
		Element *element = elementTagged(sheet, label);
		if (!element || element->elementData().m_type != ElementData::Terminal) {
			return QSharedPointer<RealTerminal>();
		}
		return static_cast<TerminalElement *>(element)->realTerminal();
	}

		/// What the plan writes on @a side of the terminal tagged @a label
	QString written(Diagram *sheet, const char *label, int side)
	{
		const auto real_t = realTerminal(sheet, label);
		if (!real_t) {
			return QStringLiteral("(no terminal %1)").arg(QLatin1String(label));
		}
		return TerminalStripDrawer::TrueRealTerminal(real_t).destination(side);
	}

	/**
		The bench with its wires:
		- 1: entry to K1:24, exit to M1:A1 and to the entry of 2;
		- 2: entry from 1, no wire on its exit;
		- 3: entry to K1:A1 and to M1:24.
		And the strips: 1 in X1, 2 in X2, 3 in none.
	*/
	bool wireTheBench(UiBench::ScratchProject &project)
	{
		Diagram *sheet = project.diagram(0);
		if (!wire(sheet, point(sheet, "1", 0), point(sheet, "K1", 0))
		    || !wire(sheet, point(sheet, "1", 1), point(sheet, "M1", 1))
		    || !wire(sheet, point(sheet, "1", 1), point(sheet, "2", 0))
		    || !wire(sheet, point(sheet, "3", 0), point(sheet, "K1", 1))
		    || !wire(sheet, point(sheet, "3", 0), point(sheet, "M1", 0))) {
			return false;
		}

		TerminalStrip *x1 = project->newTerminalStrip(QString(), QString(), QStringLiteral("X1"));
		TerminalStrip *x2 = project->newTerminalStrip(QString(), QString(), QStringLiteral("X2"));
		auto *one = static_cast<TerminalElement *>(elementTagged(sheet, "1"));
		auto *two = static_cast<TerminalElement *>(elementTagged(sheet, "2"));
		return x1 && x2 && one && two && x1->addTerminal(one) && x2->addTerminal(two);
	}
}

TEST_CASE("T64 — The plan writes where the wires of each point of a terminal go",
	  "[terminaldestination]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("destination.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(wireTheBench(project));

	SECTION("a point with one wire: the tag and the point at the other end")
	{
		CHECK(written(sheet, "1", 0) == QStringLiteral("K1:24"));
	}

	SECTION("a point with two wires: both, in the order of the point")
	{
		CHECK(written(sheet, "1", 1) == QStringLiteral("M1:A1 / X2:2"));
		CHECK(written(sheet, "3", 0) == QStringLiteral("K1:A1 / M1:24"));
	}

	SECTION("a terminal of a strip at the other end: the strip and the tag")
	{
		CHECK(written(sheet, "2", 0) == QStringLiteral("X1:1"));
	}

	SECTION("a point with no wire: Free")
	{
		CHECK(written(sheet, "2", 1) == QStringLiteral("Free"));
	}

	SECTION("no third point: nothing")
	{
		CHECK(written(sheet, "1", 2).isEmpty());
	}
}

TEST_CASE("T64 — Turning a terminal on the folio does not swap its entry and its exit",
	  "[terminaldestination]")
{
	UiBench::ScratchProject project(fixtureXml(), QStringLiteral("destination.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(wireTheBench(project));

	Element *one = elementTagged(sheet, "1");
	REQUIRE(one != nullptr);
	const Terminal *entry = one->terminals().at(0);
	const QPointF entry_before = entry->dockConductor();

	one->setRotation(180);
	REQUIRE(one->rotation() == 180);
		//The entry is drawn somewhere else now: under the terminal
	REQUIRE(entry->dockConductor().y() > entry_before.y());

	CHECK(written(sheet, "1", 0) == QStringLiteral("K1:24"));
	CHECK(written(sheet, "1", 1) == QStringLiteral("M1:A1 / X2:2"));
}
