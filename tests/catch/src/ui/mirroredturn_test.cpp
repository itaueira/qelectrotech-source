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

#include "../../../../sources/QPropertyUndoCommand/qpropertyundocommand.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QPainterPath>
#include <QUndoStack>

#include <cmath>

/*
	A mirrored symbol, turned. Two defects measured on the folio reference
	arrows of a real project, both mirrored, in 20/10/2026:

	- turned by a quarter, the wire left the point opposite its terminal,
	  across the symbol: the wires of an element were brought up to date by
	  its turn before its mirror followed the turn, so with the mirror of
	  the turn before;
	- a text turned about its own centre was turned, on opening, about the
	  centre of the text it had before its value was read, and stood away
	  from where it had been saved.
*/
namespace
{
	QString definition(const char *name, const QString &terminal)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), terminal);
	}

		/// An element of @a definition at (@a x, @a y), turned by
		/// @a orientation quarters and mirrored by @a mirror ("" for none),
		/// with @a texts as its dynamic texts
	QString instance(const char *definition, int x, int y, int orientation,
			 const QString &mirror, const char *label, int index,
			 const QString &texts = QString())
	{
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"%3\"%4 type=\"embed://bench/%5\""
			" uuid=\"{73000000-0000-4000-8000-0000000000%6}\">"
			"<terminals/><inputs/>"
			"<elementInformations>"
			"<elementInformation show=\"1\" name=\"label\">%7</elementInformation>"
			"</elementInformations>"
			"<dynamic_texts>%8</dynamic_texts><texts_groups/>"
			"</element>")
			.arg(x).arg(y).arg(orientation)
			.arg(mirror.isEmpty() ? QString()
					      : QStringLiteral(" mirror=\"%1\"").arg(mirror))
			.arg(QLatin1String(definition))
			.arg(index, 2, 10, QLatin1Char('0'))
			.arg(QLatin1String(label))
			.arg(texts);
	}

	QString project(const QString &elements)
	{
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definition("module.elmt",
					QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"))
				 + definition("arrow.elmt",
					      QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>")),
			     elements);
	}

	Element *tagged(Diagram *sheet, const char *label)
	{
		const auto elements = sheet->elements();
		for (Element *element : elements) {
			if (element->actualLabel() == QLatin1String(label)) return element;
		}
		return nullptr;
	}

	bool samePoint(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
	}

	bool sameRect(const QRectF &a, const QRectF &b)
	{
		return samePoint(a.topLeft(), b.topLeft()) && samePoint(a.bottomRight(), b.bottomRight());
	}
}

TEST_CASE("CU-74.1 — Mirrored and turned, the wire stays on its terminal", "[T74][mirror]")
{
	const QString mirror = GENERATE(QString(), QStringLiteral("horizontal"),
					QStringLiteral("vertical"));
	CAPTURE(mirror);
	UiBench::ScratchProject bench(project(instance("module.elmt", 100, 100, 0, mirror, "M", 0)
					      + instance("arrow.elmt", 200, 300, 0, QString(), "A", 1)),
				      QStringLiteral("mirroredturn.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);
	Element *module = tagged(sheet, "M");
	Element *arrow = tagged(sheet, "A");
	REQUIRE(module);
	REQUIRE(arrow);
	REQUIRE(module->terminals().size() == 1);
	REQUIRE(arrow->terminals().size() == 1);
	Terminal *own = module->terminals().first();
	Terminal *far = arrow->terminals().first();
	auto *wire = new Conductor(own, far);
	sheet->addItem(wire);

	for (int quarter = 1 ; quarter <= 4 ; ++quarter)
	{
		CAPTURE(quarter);
			//What a press of the space bar pushes for one element
			//(RotateSelectionCommand)
		sheet->undoStack().push(new QPropertyUndoCommand(
						module, "rotation", module->rotation(),
						module->rotation() + 90));
		const QPainterPath path = wire->mapToScene(wire->path());
		REQUIRE(path.elementCount() >= 2);
		const QPointF first = path.elementAt(0);
		const QPointF last = path.elementAt(path.elementCount() - 1);
		const QPointF dock = own->dockConductor();
		CHECK((samePoint(first, dock) || samePoint(last, dock)));
	}
}

TEST_CASE("CU-74.2 — A text turned about its centre opens where saved", "[T74][mirror]")
{
	const QString mirror = GENERATE(QString(), QStringLiteral("horizontal"));
	CAPTURE(mirror);
		//Turned half a turn in an element turned half a turn: it reads
		//upright, and turned about its own centre its box stays where it
		//is in the element
	const QString text = QStringLiteral(
		"<dynamic_elmt_text Halignment=\"AlignLeft\" Valignment=\"AlignVCenter\""
		" font=\"Sans Serif,5,-1,5,50,0,0,0,0,0\" frame=\"false\""
		" keep_visual_rotation=\"true\" rotation=\"-180\" rotation_point_center=\"true\""
		" text_from=\"ElementInfo\" text_width=\"-1\""
		" uuid=\"{73000000-0000-4000-8000-0000000000aa}\" x=\"1\" y=\"-7\">"
		"<text/><info_name>label</info_name></dynamic_elmt_text>");
	UiBench::ScratchProject bench(project(instance("arrow.elmt", 300, 200, 2, mirror,
						       "Folha:4-B11", 2, text)),
				      QStringLiteral("turnedtext.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);
	Element *arrow = tagged(sheet, "Folha:4-B11");
	REQUIRE(arrow);
	REQUIRE(arrow->dynamicTextItems().size() == 1);
	DynamicElementTextItem *label = arrow->dynamicTextItems().first();
		//The fixture is what it says: the value was read, and is longer
		//than the empty text the item starts with
	REQUIRE(label->toPlainText() == QStringLiteral("Folha:4-B11"));

	CHECK(samePoint(label->transformOriginPoint(), label->boundingRect().center()));
	CHECK(sameRect(label->mapRectToParent(label->boundingRect()),
		       QRectF(label->pos(), label->boundingRect().size())));
}
