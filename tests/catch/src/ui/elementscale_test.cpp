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

#include "../../../../sources/conductorsegment.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/scaleelementscommand.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QUndoStack>

#include <cmath>

/*
	An element drawn at another scale on the folio, its symbol as it is
	(T83): the drawing and the terminals at the scale, about the hotspot,
	the texts at their size.
*/
namespace
{
		/// A component 40 by 40 with a terminal on top and one below, 20
		/// from its hotspot, and its label on the left of it
	QString component(const char *name, int terminal_y)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"40\" height=\"40\" hotspot_x=\"20\" hotspot_y=\"20\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-18\" y=\"-18\" width=\"36\" height=\"36\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-%2\" orientation=\"n\"/>"
			"<terminal x=\"0\" y=\"%2\" orientation=\"s\"/>"
			"<dynamic_text text_from=\"ElementInfo\" x=\"-80\" y=\"-8\" z=\"3\" rotation=\"0\""
			" frame=\"false\" text_width=\"-1\" font=\"Sans Serif,9,-1,5,50,0,0,0,0,0\""
			" uuid=\"{83000000-0000-4000-8000-0000000000aa}\" Halignment=\"AlignLeft\""
			" Valignment=\"AlignTop\" keep_visual_rotation=\"false\""
			" rotation_point_center=\"false\"><text/><info_name>label</info_name></dynamic_text>"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name))
			.arg(terminal_y);
	}

	struct Instance
	{
		const char *definition;
		int x;
		int y;
		const char *label;
			/// More attributes of the element in the file: its turn, its
			/// mirror, its scale
		const char *more;
	};

	QString fixtureXml(const QList<Instance> &instances)
	{
		QString xml;
		for (int i = 0 ; i < instances.size() ; ++i)
		{
			const Instance &instance = instances.at(i);
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" type=\"embed://bench/%3\" %6"
				" uuid=\"{83000000-0000-4000-8000-0000000000%4}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(instance.x).arg(instance.y)
				.arg(QLatin1String(instance.definition))
				.arg(i, 2, 10, QLatin1Char('0'))
				.arg(QLatin1String(instance.label))
				.arg(QLatin1String(instance.more));
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"800\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"10\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(component("comp.elmt", 20) + component("odd.elmt", 15), xml);
	}

	Element *tagged(Diagram *sheet, const char *label)
	{
		const auto elements = sheet->elements();
		for (Element *element : elements) {
			if (element->actualLabel() == QLatin1String(label)) return element;
		}
		return nullptr;
	}

		/// The ends of the terminals of @a element, from its hotspot
	QList<QPointF> ends(const Element *element)
	{
		QList<QPointF> out;
		const QList<Terminal *> terminals = element->terminals();
		for (const Terminal *t : terminals) out << t->dockConductor() - element->pos();
		return out;
	}

		/// The label of @a element, given the way the folio gives it: the
		/// bench builds no text of the symbol. On the left of the drawing.
	DynamicElementTextItem *label(Element *element)
	{
		const QList<DynamicElementTextItem *> texts = element->dynamicTextItems();
		if (!texts.isEmpty()) return texts.first();
		auto *given = new DynamicElementTextItem(element);
		given->setTextFrom(DynamicElementTextItem::ElementInfo);
		given->setInfoName(QStringLiteral("label"));
		element->addDynamicTextItem(given);
		given->setPos(-80, -8);
		return given;
	}

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
		for (Conductor *c : sheet->conductors()) {
			if (!before.contains(c)) return c;
		}
		return nullptr;
	}

	void scale(Diagram *sheet, Element *element, qreal factor)
	{
		auto *command = new ScaleElementsCommand({element}, factor);
		REQUIRE(command->isValid());
		sheet->undoStack().push(command);
	}

	bool near(const QPointF &a, const QPointF &b)
	{
		return (a - b).manhattanLength() < 0.01;
	}
}

TEST_CASE("CU-83.1 — The element is smaller, and only it", "[T83][elementscale]")
{
	UiBench::ScratchProject project(fixtureXml({{"comp.elmt", 200, 200, "A", ""},
						    {"comp.elmt", 400, 200, "B", ""}}));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *a = tagged(sheet, "A");
	Element *b = tagged(sheet, "B");
	REQUIRE((a && b));
	const QRectF box = a->sceneBoundingRect();
	const QPointF hotspot = a->pos();

	scale(sheet, a, 0.5);

	CHECK(a->symbolScale() == Approx(0.5));
	CHECK(b->symbolScale() == Approx(1));
	CHECK(a->pos() == hotspot);
		//Half the size, about the hotspot
	CHECK(a->sceneBoundingRect().width() == Approx(box.width() / 2).margin(0.5));
	CHECK(a->sceneBoundingRect().height() == Approx(box.height() / 2).margin(0.5));
	const QList<QPointF> a_ends = ends(a);
	REQUIRE(a_ends.size() == 2);
	CHECK(near(a_ends.at(0), QPointF(0, -10)));
	CHECK(near(a_ends.at(1), QPointF(0, 10)));
		//The other element of the same symbol as it was
	const QList<QPointF> b_ends = ends(b);
	REQUIRE(b_ends.size() == 2);
	CHECK(near(b_ends.at(0), QPointF(0, -20)));
	CHECK(near(b_ends.at(1), QPointF(0, 20)));
}

TEST_CASE("CU-83.2 — The wires follow", "[T83][elementscale]")
{
	UiBench::ScratchProject project(fixtureXml({{"comp.elmt", 200, 200, "A", ""},
						    {"comp.elmt", 200, 400, "C", ""}}));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *a = tagged(sheet, "A");
	Element *c = tagged(sheet, "C");
	REQUIRE((a && c));
	Conductor *wire = draw(a->terminals().at(1), c->terminals().at(0));
	REQUIRE(wire != nullptr);

	scale(sheet, a, 0.5);

		//Each end of the wire on its terminal, where the scale put it
	const QPointF from = wire->terminal1->dockConductor();
	const QPointF to = wire->terminal2->dockConductor();
	const QPointF first = wire->mapToScene(wire->segmentsList().first()->firstPoint());
	const QPointF last = wire->mapToScene(wire->segmentsList().last()->secondPoint());
	CAPTURE(from, to, first, last);
	CHECK(near(a->terminals().at(1)->dockConductor(), QPointF(200, 210)));
	CHECK(((near(first, from) && near(last, to)) || (near(first, to) && near(last, from))));
}

TEST_CASE("CU-83.3 — The texts keep their size", "[T83][elementscale]")
{
	UiBench::ScratchProject project(fixtureXml({{"comp.elmt", 300, 200, "A", ""}}));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *a = tagged(sheet, "A");
	REQUIRE(a != nullptr);
	DynamicElementTextItem *text = label(a);
	REQUIRE(text != nullptr);
	const QRectF before = text->sceneBoundingRect();
	REQUIRE(before.right() < a->pos().x());

	scale(sheet, a, 0.5);

	const QRectF after = text->sceneBoundingRect();
	CAPTURE(before, after);
		//The same size
	CHECK(after.width() == Approx(before.width()).margin(0.01));
	CHECK(after.height() == Approx(before.height()).margin(0.01));
		//On the left of the drawing, its side towards it where the scale
		//puts that side
	CHECK(after.right() - a->pos().x() == Approx((before.right() - a->pos().x()) / 2).margin(0.01));
	CHECK(after.right() < a->sceneBoundingRect().left() + 0.01);
}

TEST_CASE("CU-83.4 — Terminals off the grid, smaller all the same", "[T83][elementscale]")
{
	/*
		The request: a symbol whose terminals are at odd places from its
		hotspot - here 15 - has no smaller scale that keeps them on the
		grid, and an element drawn only to show what the project meets is
		drawn smaller all the same. Every scale is offered.
	*/
	CHECK(Element::symbolScaleChoices()
	      == QList<qreal>{0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.5, 2.0});
	UiBench::ScratchProject project(fixtureXml({{"odd.elmt", 200, 200, "O", ""},
						    {"comp.elmt", 200, 400, "C", ""}}));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *odd = tagged(sheet, "O");
	Element *c = tagged(sheet, "C");
	REQUIRE((odd && c));
	Conductor *wire = draw(odd->terminals().at(1), c->terminals().at(0));
	REQUIRE(wire != nullptr);

	scale(sheet, odd, 0.5);

		//Between two points of the grid, and the wire on it
	CHECK(near(ends(odd).at(1), QPointF(0, 7.5)));
	const QPointF end = odd->terminals().at(1)->dockConductor();
	const QPointF first = wire->mapToScene(wire->segmentsList().first()->firstPoint());
	const QPointF last = wire->mapToScene(wire->segmentsList().last()->secondPoint());
	CAPTURE(end, first, last);
	CHECK((near(first, end) || near(last, end)));
}

TEST_CASE("CU-83.5 — The scale is in the file, and undo takes it off", "[T83][elementscale]")
{
	UiBench::ScratchProject project(fixtureXml({{"comp.elmt", 200, 200, "A", ""},
						    {"comp.elmt", 400, 200, "S", "scale=\"0.5\""}}));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *a = tagged(sheet, "A");
	Element *saved = tagged(sheet, "S");
	REQUIRE((a && saved));

		//Read from the file
	CHECK(saved->symbolScale() == Approx(0.5));
	CHECK(near(ends(saved).at(1), QPointF(0, 10)));

		//Written to it only when not 1
	QDomDocument document;
	QHash<Terminal *, int> ids;
	CHECK_FALSE(a->toXml(document, ids).hasAttribute(QStringLiteral("scale")));
	scale(sheet, a, 0.5);
	CHECK(a->toXml(document, ids).attribute(QStringLiteral("scale")) == QStringLiteral("0.5"));

		//Undone, the symbol as it is
	sheet->undoStack().undo();
	CHECK(a->symbolScale() == Approx(1));
	CHECK(near(ends(a).at(1), QPointF(0, 20)));
}

TEST_CASE("CU-83.6 — Turned and mirrored", "[T83][elementscale]")
{
	UiBench::ScratchProject project(fixtureXml({{"comp.elmt", 300, 300, "A",
						     "orientation=\"1\" mirror=\"horizontal\""}}));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *a = tagged(sheet, "A");
	REQUIRE(a != nullptr);
	REQUIRE(a->isMirrored());
	const QList<QPointF> before = ends(a);
	DynamicElementTextItem *text = label(a);
	REQUIRE(text != nullptr);
	const QRectF text_before = text->sceneBoundingRect();

	scale(sheet, a, 0.5);

		//Each terminal half as far from the hotspot, the same way
	const QList<QPointF> after = ends(a);
	REQUIRE(after.size() == before.size());
	for (int i = 0 ; i < after.size() ; ++i)
	{
		CAPTURE(i, before.at(i), after.at(i));
		CHECK(near(after.at(i), before.at(i) / 2));
	}
		//The text the same size, and reading normally: turned with the
		//element, as the text of a turned element is, but neither mirrored
		//nor scaled - its transformation keeps the sense and the area
	CHECK(text->sceneBoundingRect().width() == Approx(text_before.width()).margin(0.01));
	CHECK(text->sceneBoundingRect().height() == Approx(text_before.height()).margin(0.01));
	const QTransform t = text->sceneTransform();
	CHECK(t.m11() * t.m22() - t.m12() * t.m21() == Approx(1).margin(0.001));
}
