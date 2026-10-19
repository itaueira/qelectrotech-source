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
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/addgraphicsobjectcommand.h"
#include "../../../../sources/undocommand/exchangeelementscommand.h"

#include <catch2/catch.hpp>

#include <QUndoStack>

/*
	Two elements each in the place of the other (T66): position, rotation
	and mirrors. The wires stay where they are, and each element takes the
	wires of the place it comes to - as in the schematic editors this was
	asked to match.

	The bench: K1 and K2, of a symbol with a terminal on top and one under,
	side by side; A above K1 and B above K2, each wired to the top of the
	one under it; a wire under, from K1 to K2. And S, a symbol whose two
	terminals are on its sides, where no terminal of K1 would ever lie.
*/
namespace
{
	QString definition(const char *name, bool sides)
	{
		const QString terminals = sides
			? QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>"
					 "<terminal x=\"10\" y=\"0\" orientation=\"e\"/>")
			: QStringLiteral("<terminal x=\"0\" y=\"-20\" orientation=\"n\"/>"
					 "<terminal x=\"0\" y=\"20\" orientation=\"s\"/>");
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-18\" width=\"16\" height=\"36\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>").arg(QLatin1String(name), terminals);
	}

	struct Instance
	{
		const char *definition;
		int x;
		int y;
		int rotation;
		const char *label;
	};

	QString fixtureXml(const QList<Instance> &instances)
	{
		QString xml;
		for (int i = 0 ; i < instances.size() ; ++i)
		{
			const Instance &instance = instances.at(i);
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"%3\" type=\"embed://bench/%4\""
				" uuid=\"{e7c40000-0000-4000-8000-00000000000%5}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%6</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(instance.x).arg(instance.y).arg(instance.rotation / 90)
				.arg(QLatin1String(instance.definition)).arg(i)
				.arg(QLatin1String(instance.label));
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1%2</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"800\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"10\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%3</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definition("block.elmt", false), definition("side.elmt", true), xml);
	}

	Element *tagged(Diagram *sheet, const char *label)
	{
		const auto elements = sheet->elements();
		for (Element *element : elements) {
			if (element->actualLabel() == QLatin1String(label)) {
				return element;
			}
		}
		return nullptr;
	}

	Conductor *wire(Diagram *sheet, Terminal *a, Terminal *b)
	{
		auto *conductor = new Conductor(a, b);
		sheet->undoStack().push(new AddGraphicsObjectCommand(conductor, sheet, QPointF()));
		return conductor;
	}

	/// The element at the other end of @a conductor from @a element
	Element *otherEnd(const Conductor *conductor, const Element *element)
	{
		return conductor->terminal1->parentElement() == element
				? conductor->terminal2->parentElement()
				: conductor->terminal1->parentElement();
	}

	const QList<Instance> bench{
		{"block.elmt", 200, 200, 0, "K1"},
		{"block.elmt", 400, 200, 0, "K2"},
		{"block.elmt", 200, 80, 0, "A"},
		{"block.elmt", 400, 80, 0, "B"},
		{"side.elmt", 600, 200, 0, "S"}};
}

TEST_CASE("T66 — Two elements swap places, the wires stay", "[T66][exchange]")
{
	UiBench::ScratchProject project(fixtureXml(bench));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *k1 = tagged(sheet, "K1");
	Element *k2 = tagged(sheet, "K2");
	Element *a = tagged(sheet, "A");
	Element *b = tagged(sheet, "B");
	REQUIRE((k1 && k2 && a && b));

	Conductor *from_a = wire(sheet, a->terminals().at(1), k1->terminals().at(0));
	Conductor *from_b = wire(sheet, b->terminals().at(1), k2->terminals().at(0));
	Conductor *between = wire(sheet, k1->terminals().at(1), k2->terminals().at(1));
	const QPointF a_end = k1->terminals().at(0)->dockConductor();
	const QPointF b_end = k2->terminals().at(0)->dockConductor();

	const ExchangeElementsCommand::Plan plan = ExchangeElementsCommand::plan(k1, k2);
	REQUIRE(plan.error.isEmpty());
	CHECK(plan.lost_conductors.isEmpty());
	CHECK(plan.moved_conductors.size() == 3);
	sheet->undoStack().push(new ExchangeElementsCommand(plan));

		//Each one in the place of the other, with its own tag
	CHECK(k1->pos() == QPointF(400, 200));
	CHECK(k2->pos() == QPointF(200, 200));
	CHECK(k1->actualLabel() == QStringLiteral("K1"));

		//The wire from A ends where it ended, now on K2; the one from B on K1
	CHECK(otherEnd(from_a, a) == k2);
	CHECK(otherEnd(from_b, b) == k1);
	CHECK(k2->terminals().at(0)->dockConductor() == a_end);
	CHECK(k1->terminals().at(0)->dockConductor() == b_end);
		//The wire between them still joins them
	CHECK(otherEnd(between, k1) == k2);
	CHECK(sheet->conductors().size() == 3);

	SECTION("undo and redo")
	{
		sheet->undoStack().undo();
		CHECK(k1->pos() == QPointF(200, 200));
		CHECK(k2->pos() == QPointF(400, 200));
		CHECK(otherEnd(from_a, a) == k1);
		CHECK(otherEnd(from_b, b) == k2);
		CHECK(from_a->terminal2 == k1->terminals().at(0));

		sheet->undoStack().redo();
		CHECK(k1->pos() == QPointF(400, 200));
		CHECK(otherEnd(from_a, a) == k2);
	}
}

TEST_CASE("T66 — The rotation and the mirrors swap too", "[T66][exchange]")
{
	QList<Instance> turned = bench;
	turned[1].rotation = 180;
	UiBench::ScratchProject project(fixtureXml(turned));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *k1 = tagged(sheet, "K1");
	Element *k2 = tagged(sheet, "K2");
	Element *b = tagged(sheet, "B");
	REQUIRE((k1 && k2 && b));
	k2->setHorizontalMirror(true);
	REQUIRE(qFuzzyCompare(k2->rotation(), 180));

		//Turned over, the terminal of K2 that faces B is the one under
	Conductor *from_b = wire(sheet, b->terminals().at(1), k2->terminals().at(1));

	const ExchangeElementsCommand::Plan plan = ExchangeElementsCommand::plan(k1, k2);
	REQUIRE(plan.error.isEmpty());
	CHECK(plan.lost_conductors.isEmpty());
	sheet->undoStack().push(new ExchangeElementsCommand(plan));

	CHECK(qFuzzyCompare(k1->rotation(), 180));
	CHECK(k1->hasHorizontalMirror());
	CHECK(qFuzzyIsNull(k2->rotation()));
	CHECK_FALSE(k2->hasHorizontalMirror());
		//Turned over in its turn, K1 takes the wire on its own terminal under
	CHECK(otherEnd(from_b, b) == k1);
	CHECK((from_b->terminal1 == k1->terminals().at(1) || from_b->terminal2 == k1->terminals().at(1)));
}

TEST_CASE("T66 — A wire with no terminal at its end is taken off, and comes back", "[T66][exchange]")
{
	UiBench::ScratchProject project(fixtureXml(bench));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *k1 = tagged(sheet, "K1");
	Element *s = tagged(sheet, "S");
	Element *a = tagged(sheet, "A");
	REQUIRE((k1 && s && a));

		//The terminals of S are on its sides, where K1 has none
	Conductor *from_a = wire(sheet, a->terminals().at(1), k1->terminals().at(0));

	const ExchangeElementsCommand::Plan plan = ExchangeElementsCommand::plan(k1, s);
	REQUIRE(plan.error.isEmpty());
	CHECK(plan.lost_conductors == QList<Conductor *>{from_a});
	sheet->undoStack().push(new ExchangeElementsCommand(plan));
	CHECK(from_a->scene() == nullptr);
	CHECK(s->pos() == QPointF(200, 200));

	sheet->undoStack().undo();
	CHECK(from_a->scene() == sheet);
	CHECK(otherEnd(from_a, a) == k1);
}

TEST_CASE("T66 — Only two elements of the same folio", "[T66][exchange]")
{
	UiBench::ScratchProject project(fixtureXml(bench));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *k1 = tagged(sheet, "K1");
	REQUIRE(k1);
	CHECK_FALSE(ExchangeElementsCommand::plan(k1, k1).error.isEmpty());
	CHECK_FALSE(ExchangeElementsCommand::plan(k1, nullptr).error.isEmpty());
}
