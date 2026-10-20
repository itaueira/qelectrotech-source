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

#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/conductorsegment.h"
#include "../../../../sources/conductortextfit.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/conductortextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>

#include <algorithm>
#include <cmath>

/*
	The name of a conductor drawn smaller so that it fits its longest run,
	never under 5, while the size written in the file stays the one chosen
	(T70).

	Three conductors, each drawn by mouse between a terminal facing east and
	one facing west, then given a path of three segments as a drag would:

	- across:  40 to the east, 10 down, 10 to the east - the longest run is
	           the first one, 40 long and horizontal;
	- upright: 10 to the east, 40 down, 10 to the east - 40 long, vertical,
	           the name turned to run along it;
	- short:   5 to the east, 10 down, 5 to the east - the longest run is 10.

	No width of text is written down here: the font the folio draws with is
	whatever the machine has (see uibench.h), so the name given to the two
	runs of 40 is chosen by measuring - the shortest wire number that is
	wider than the run at 9 - and every check compares a width measured on
	the folio with the length of the run.
*/
namespace
{
		/// The smallest size the person asked for, written here and not
		/// read from the code: a floor changed in the code must fail here
	constexpr int asked_floor = 5;

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

	struct Instance
	{
		const char *definition;
		int x;
		int y;
		const char *label;
	};

		/// The ends of the three conductors: the dock of a terminal is the
		/// component's position plus 10 towards the side it faces
	QList<Instance> instances()
	{
		return {{"east.elmt", 100, 100, "A1"},
			{"west.elmt", 170, 110, "A2"},
			{"east.elmt", 100, 200, "U1"},
			{"west.elmt", 140, 240, "U2"},
			{"east.elmt", 100, 320, "S1"},
			{"west.elmt", 130, 330, "S2"}};
	}

	QString fixtureXml()
	{
		QString xml;
		const QList<Instance> list = instances();
		for (int i = 0 ; i < list.size() ; ++i)
		{
			const Instance &instance = list.at(i);
			xml += QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
				" orientation=\"0\" type=\"embed://bench/%3\""
				" uuid=\"{70000000-0000-4000-8000-0000000000%4}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(instance.x).arg(instance.y)
				.arg(QLatin1String(instance.definition))
				.arg(i, 2, 10, QLatin1Char('0'))
				.arg(QLatin1String(instance.label));
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
			.arg(definition("east.elmt",
					QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"))
				 + definition("west.elmt",
					      QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>")),
			     xml);
	}

	Element *tagged(Diagram *sheet, const char *label)
	{
		const auto elements = sheet->elements();
		for (Element *element : elements) {
			if (element->actualLabel() == QLatin1String(label)) return element;
		}
		return nullptr;
	}

	Terminal *terminalOf(Diagram *sheet, const char *label)
	{
		Element *element = tagged(sheet, label);
		if (!element || element->terminals().isEmpty()) return nullptr;
		return element->terminals().first();
	}

		/// A press on @a from and a release on @a to, as the user draws
	Conductor *draw(Diagram *sheet, Terminal *from, Terminal *to)
	{
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

		/// The conductor drawn from the component tagged @a from to the one
		/// tagged @a to, then given @a path, in scene coordinates
	Conductor *wire(Diagram *sheet, const char *from, const char *to,
			const QList<QPointF> &path)
	{
		Terminal *start = terminalOf(sheet, from);
		Terminal *end = terminalOf(sheet, to);
		if (!start || !end) return nullptr;
		Conductor *c = draw(sheet, start, end);
		if (!c || !c->setPathPoints(path)) return nullptr;
		return c;
	}

		/// The conductor of the component tagged @a label, after a reopening
	Conductor *wireOf(Diagram *sheet, const char *label)
	{
		Terminal *terminal = terminalOf(sheet, label);
		if (!terminal || terminal->conductors().size() != 1) return nullptr;
		return terminal->conductors().first();
	}

		/// The length of the longest segment of @a c, from its own segments
	qreal longestRun(const Conductor *c)
	{
		qreal longest = 0;
		const auto segments = c->segmentsList();
		for (ConductorSegment *s : segments) {
			longest = std::max(longest, std::abs(s->length()));
		}
		return longest;
	}

		/// A name on @a c, at the size 9, written along its runs, below a
		/// horizontal one and to the right of a vertical one - whatever the
		/// settings of the machine say the default is
	void name(Conductor *c, const QString &text)
	{
		ConductorProperties properties = c->properties();
		properties.type = ConductorProperties::Multi;
		properties.m_formula.clear();
		properties.text = text;
		properties.text_size = 9;
		properties.m_show_text = true;
		properties.m_one_text_per_folio = false;
		properties.horiz_rotate_text = 0;
		properties.verti_rotate_text = 270;
		properties.m_horizontal_alignment = Qt::AlignBottom;
		properties.m_vertical_alignment = Qt::AlignRight;
		c->setProperties(properties);
	}

		/// The size the name of @a c is drawn at
	int drawnSize(const Conductor *c)
	{
		return c->textItem()->font().pointSize();
	}

		/// The width of the box of @a text in the font of a conductor's
		/// name, at @a size: measured on a text of the same kind, not on
		/// the folio
	qreal boxWidth(const QFont &font, const QString &text, int size)
	{
		ConductorTextItem probe(text);
		QFont sized = font;
		sized.setPointSize(size);
		probe.setFont(sized);
		return probe.boundingRect().width();
	}

		/// The shortest wire number wider than @a room at 9; empty when
		/// none is, or when the first that is does not fit at 5 either
	QString widerThan(const QFont &font, qreal room)
	{
		const QString digits = QStringLiteral("F1-12345678901234567890");
		for (int length = 4 ; length <= digits.size() ; ++length)
		{
			const QString name = digits.left(length);
			if (boxWidth(font, name, 9) > room)
				return boxWidth(font, name, asked_floor) <= room
					? name : QString();
		}
		return QString();
	}

		/// The three conductors of the folio, drawn and given their paths
	struct Folio
	{
		Folio() : project(fixtureXml(), QStringLiteral("wirenamefit.qet")) {}

		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			if (!sheet) return false;
			across = wire(sheet, "A1", "A2",
				      {{110, 100}, {150, 100}, {150, 110}, {160, 110}});
			upright = wire(sheet, "U1", "U2",
				       {{110, 200}, {120, 200}, {120, 240}, {130, 240}});
			shortest = wire(sheet, "S1", "S2",
					{{110, 320}, {115, 320}, {115, 330}, {120, 330}});
			return across && upright && shortest;
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		Conductor *across = nullptr;
		Conductor *upright = nullptr;
		Conductor *shortest = nullptr;
	};

		/// The name of the case: a wire number of the new pattern, with the
		/// square and a cross-section
	const QString long_name = QStringLiteral("F1-1 (■ 1,5mm²)");
}

TEST_CASE("CU-70.7 — The name fits the longest run", "[T70][wirename]")
{
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());

		//The runs the case is about, measured on the conductors
	REQUIRE(longestRun(folio.across) == Approx(40));
	REQUIRE(longestRun(folio.upright) == Approx(40));
	REQUIRE(longestRun(folio.shortest) == Approx(10));

	const QFont font = folio.across->textItem()->font();
	const qreal room = ConductorTextFit::room(40);
	const QString fitting = widerThan(font, room);
	INFO("name for the runs of 40: " << fitting.toStdString()
	     << ", " << boxWidth(font, fitting, 9) << " wide at 9");
	REQUIRE_FALSE(fitting.isEmpty());

	name(folio.across, fitting);
	name(folio.upright, fitting);
	name(folio.shortest, long_name);

	SECTION("a run of 40: the largest size that fits, inside the run")
	{
		for (Conductor *c : {folio.across, folio.upright})
		{
			const bool vertical = c == folio.upright;
			INFO((vertical ? "upright" : "across"));
			const int size = drawnSize(c);
			CHECK(size < 9);
			CHECK(size >= asked_floor);
				//The largest: one more and it would not fit
			CHECK(boxWidth(font, fitting, size + 1) > room);

				//On the folio, the text lies within its run, and beside it
			const QRectF box = c->textItem()->sceneBoundingRect();
			INFO("text " << box.left() << ", " << box.top()
			     << " " << box.width() << " x " << box.height());
			if (vertical)
			{
				CHECK(box.height() <= 40);
				CHECK(box.top() >= 200);
				CHECK(box.bottom() <= 240);
				CHECK(box.left() - 120 >= 0);
				CHECK(box.left() - 120 <= 3);
			}
			else
			{
				CHECK(box.width() <= 40);
				CHECK(box.left() >= 110);
				CHECK(box.right() <= 150);
				CHECK(box.top() - 100 >= 0);
				CHECK(box.top() - 100 <= 3);
			}
		}
	}

	SECTION("a run of 10: 5, and past the run")
	{
		CHECK(drawnSize(folio.shortest) == asked_floor);
		CHECK(folio.shortest->textItem()->sceneBoundingRect().height() > 10);
	}

	SECTION("the size in the file stays 9, and comes back drawn the same")
	{
		const int across_size = drawnSize(folio.across);
		const int upright_size = drawnSize(folio.upright);
		for (Conductor *c : {folio.across, folio.upright, folio.shortest}) {
			CHECK(c->properties().text_size == 9);
		}

		REQUIRE(folio.project.saveAndReopen());
		QDomDocument document;
		REQUIRE(document.setContent(UiBench::fileContent(folio.project.filePath())));
		const QDomNodeList conductors = document.elementsByTagName(QStringLiteral("conductor"));
		int ours = 0;
		for (int i = 0 ; i < conductors.size() ; ++i)
		{
			const QDomElement e = conductors.at(i).toElement();
			const QString num = e.attribute(QStringLiteral("num"));
			if (num != fitting && num != long_name) continue;
			++ours;
			INFO(num.toStdString());
			CHECK(e.attribute(QStringLiteral("numsize")) == QStringLiteral("9"));
		}
		CHECK(ours == 3);

		Diagram *sheet = folio.project.diagram(0);
		REQUIRE(sheet);
		Conductor *across = wireOf(sheet, "A1");
		Conductor *upright = wireOf(sheet, "U1");
		Conductor *shortest = wireOf(sheet, "S1");
		REQUIRE((across && upright && shortest));
		for (Conductor *c : {across, upright, shortest}) {
			CHECK(c->properties().text_size == 9);
		}
		CHECK(drawnSize(across) == across_size);
		CHECK(drawnSize(upright) == upright_size);
		CHECK(drawnSize(shortest) == asked_floor);
	}
}

TEST_CASE("T70 — The size comes back when the name gets shorter", "[T70][wirename]")
{
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());

		//A single digit fits 40 at 9 in any font
	const QString one = QStringLiteral("1");
	REQUIRE(boxWidth(folio.across->textItem()->font(), one, 9)
		<= ConductorTextFit::room(40));

	name(folio.across, one);
	CHECK(drawnSize(folio.across) == 9);
	name(folio.across, long_name);
	CHECK(drawnSize(folio.across) < 9);
		//From the size in the file again, not from the one drawn last
	name(folio.across, one);
	CHECK(drawnSize(folio.across) == 9);
	CHECK(folio.across->properties().text_size == 9);
}

TEST_CASE("T70 — A name moved by hand keeps its size and its place", "[T70][wirename]")
{
		//Drawn at the size that fits its run, as when the application
		//placed it: a drag never makes it jump to the size of the file
	Folio folio;
	INFO(folio.project.error().toStdString());
	REQUIRE(folio.open());

	name(folio.shortest, long_name);
	REQUIRE(drawnSize(folio.shortest) == asked_floor);

	ConductorTextItem *text = folio.shortest->textItem();
	text->forceMovedByUser(true);
	const QPointF placed = text->pos();
	folio.shortest->calculateTextItemPosition();
	CHECK(drawnSize(folio.shortest) == asked_floor);
	CHECK(text->pos() == placed);
	CHECK(folio.shortest->properties().text_size == 9);
}
