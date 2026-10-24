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

#include "../../../../sources/TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "../../../../sources/TerminalStrip/physicalterminal.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/terminalstripbridge.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetgraphicsitem/terminalbusitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/wirearrangement.h"
#include "../../../../sources/wirehops.h"

#include <catch2/catch.hpp>

#include <QImage>
#include <QPainter>
#include <QUndoStack>

/*
	A bus bar (T87): terminals of a strip joined by a bridge, the comb the
	fitter puts on them, fed by one wire.

	The bench, a strip X4 standing at x = 400 as on the folio 5 of the
	request: six terminals 1.1, 1.2, 2.1, 2.2, 3.1, 3.2 from y = 200 to 300,
	20 apart - the two levels of three terminals -, each a box of 12 with an
	end 14 to its left and 14 to its right. A terminal 5 of a strip X1 at
	(300, 120), the source of the common; a board at x = 240 whose outputs
	go to the terminals .1 from the left; three loads at x = 520 on the
	right of the terminals .2.
*/
namespace
{
	QString definition(const char *name, const QString &body, bool terminal)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
			" orientation=\"dnnn\"%3>"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-6\" y=\"-6\" width=\"12\" height=\"12\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), body,
			     terminal ? QStringLiteral(" link_type=\"terminal\"") : QStringLiteral(" link_type=\"simple\""));
	}

	QString element(const char *def, int x, int y, const QString &label, const QString &comment, int n)
	{
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/%3\""
			" uuid=\"{87000000-0000-4000-8000-0000000000%4}\">"
			"<terminals/><inputs/>"
			"<elementInformations>"
			"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
			"<elementInformation show=\"0\" name=\"comment\">%6</elementInformation>"
			"</elementInformations>"
			"<dynamic_texts>"
			"<dynamic_elmt_text Halignment=\"AlignLeft\" Valignment=\"AlignTop\""
			" font=\"Sans Serif,6,-1,5,50,0,0,0,0,0\" frame=\"false\""
			" keep_visual_rotation=\"false\" rotation=\"0\" rotation_point_center=\"false\""
			" text_from=\"ElementInfo\" text_width=\"-1\""
			" uuid=\"{87000000-0000-4000-8000-0000000001%4}\" x=\"8\" y=\"-4\">"
			"<text/><info_name>label</info_name></dynamic_elmt_text>"
			"</dynamic_texts><texts_groups/>"
			"</element>")
			.arg(x).arg(y).arg(QLatin1String(def)).arg(n, 2, 10, QLatin1Char('0')).arg(label, comment);
	}

		/// @a first_comment: the comment of the terminals 1.1 and 1.2, X4 as
		/// the others; another leaves them out of the strip X4 the first
		/// bus bar makes. @a more: the terminals 4.1, 4.2, 5.1 and 5.2 too,
		/// below them, with no comment
	QString fixture(const QString &first_comment = QStringLiteral("X4"), bool more = false)
	{
		const QString sides = QStringLiteral("<terminal x=\"-14\" y=\"0\" orientation=\"w\"/>"
						    "<terminal x=\"14\" y=\"0\" orientation=\"e\"/>");
		const QString definitions =
			definition("bus-strip.elmt", sides, true)
			+ definition("bus-board.elmt", QStringLiteral("<terminal x=\"14\" y=\"0\" orientation=\"e\"/>"), false)
			+ definition("bus-load.elmt", QStringLiteral("<terminal x=\"-14\" y=\"0\" orientation=\"w\"/>"), false);
		QString xml;
		const QStringList labels {QStringLiteral("1.1"), QStringLiteral("1.2"), QStringLiteral("2.1"),
					  QStringLiteral("2.2"), QStringLiteral("3.1"), QStringLiteral("3.2")};
		for (int i = 0 ; i < labels.size() ; ++i) {
			xml += element("bus-strip.elmt", 400, 200 + 20 * i, labels.at(i),
				       i < 2 ? first_comment : QStringLiteral("X4"), i);
		}
		if (more)
		{
			const QStringList below {QStringLiteral("4.1"), QStringLiteral("4.2"),
						 QStringLiteral("5.1"), QStringLiteral("5.2")};
			for (int i = 0 ; i < below.size() ; ++i) {
				xml += element("bus-strip.elmt", 400, 320 + 20 * i, below.at(i), QString(), 40 + i);
			}
		}
		xml += element("bus-strip.elmt", 300, 120, QStringLiteral("5"), QStringLiteral("X1"), 10);
		for (int i = 0 ; i < 3 ; ++i)
		{
			xml += element("bus-board.elmt", 240, 200 + 40 * i, QStringLiteral("Q%1").arg(i + 1), QString(), 20 + i);
			xml += element("bus-load.elmt", 520, 220 + 40 * i, QStringLiteral("Y%1").arg(i + 1), QString(), 30 + i);
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
			.arg(definitions, xml);
	}

		/// No arranging of the wires the bench draws
	class Setup
	{
		public:
			Setup() : m_arrange(WireArrangement::arrangesAutomatically()) {
				WireArrangement::setArrangesAutomatically(false);
			}
			~Setup() {WireArrangement::setArrangesAutomatically(m_arrange);}
		private:
			bool m_arrange;
	};

	Element *byLabel(Diagram *sheet, const QString &label)
	{
		for (Element *e : sheet->elements()) {
			if (e->actualLabel() == label) return e;
		}
		return nullptr;
	}

	Conductor *wire(Diagram *sheet, Terminal *a, Terminal *b)
	{
		auto *conductor = new Conductor(a, b);
		sheet->addItem(conductor);
		return conductor;
	}

	TerminalBusItem *busOf(Diagram *sheet)
	{
		const QList<TerminalBusItem *> buses = TerminalBusItem::busesOn(sheet);
		return buses.size() == 1 ? buses.first() : nullptr;
	}

		/// The bench wired as the common of the request is: the feed from
		/// X1:5 to 1.2, and from 1.2 a wire to each other common; the field
		/// wires on the right of the commons, the outputs on the left of
		/// the signals
	struct Wired
	{
		explicit Wired(Diagram *sheet_) : sheet(sheet_)
		{
			for (const QString &label : {QStringLiteral("1.1"), QStringLiteral("1.2"), QStringLiteral("2.1"),
						     QStringLiteral("2.2"), QStringLiteral("3.1"), QStringLiteral("3.2")}) {
				x4 << byLabel(sheet, label);
			}
			x1 = byLabel(sheet, QStringLiteral("5"));
			if (x4.contains(nullptr) || !x1) return;
			feed = wire(sheet, x1->terminals().at(1), x4.at(1)->terminals().at(0));
			star << wire(sheet, x4.at(1)->terminals().at(0), x4.at(3)->terminals().at(0));
			star << wire(sheet, x4.at(1)->terminals().at(0), x4.at(5)->terminals().at(0));
			for (int i = 0 ; i < 3 ; ++i)
			{
				field << wire(sheet, x4.at(2 * i + 1)->terminals().at(1),
					      byLabel(sheet, QStringLiteral("Y%1").arg(i + 1))->terminals().at(0));
				outputs << wire(sheet, byLabel(sheet, QStringLiteral("Q%1").arg(i + 1))->terminals().at(0),
						x4.at(2 * i)->terminals().at(0));
			}
		}
		QList<Element *> commons() const {return {x4.at(1), x4.at(3), x4.at(5)};}

		Diagram *sheet;
		QList<Element *> x4;
		Element *x1 = nullptr;
		Conductor *feed = nullptr;
		QList<Conductor *> star;
		QList<Conductor *> field;
		QList<Conductor *> outputs;
	};
}

TEST_CASE("T87 - Joining the commons of a strip into a bus bar", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	const int wires_before = sheet->conductors().size();
	REQUIRE(wires_before == 9);
	REQUIRE(project->terminalStrip().isEmpty());

	QString why;
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString(), &why));
	CHECK(why.isEmpty());

		//The strip X4 of the strip manager, three terminals of two levels,
		//the bridge on the second level of the three
	REQUIRE(project->terminalStrip().size() == 1);
	TerminalStrip *strip = project->terminalStrip().first();
	CHECK(strip->name() == QStringLiteral("X4"));
	REQUIRE(strip->physicalTerminalCount() == 3);
	for (int i = 0 ; i < 3 ; ++i)
	{
		const auto physical = strip->physicalTerminal(i);
		REQUIRE(physical->realTerminalCount() == 2);
		CHECK(physical->realTerminals().at(0)->element() == wired.x4.at(2 * i));
		CHECK(physical->realTerminals().at(1)->element() == wired.x4.at(2 * i + 1));
	}
	const QSharedPointer<RealTerminal> first = TerminalBusItem::realTerminalOf(wired.x4.at(1));
	REQUIRE(first);
	const QSharedPointer<TerminalStripBridge> bridge = first->bridge();
	REQUIRE(bridge);
	CHECK(bridge->realTerminals().size() == 3);
	CHECK_FALSE(TerminalBusItem::realTerminalOf(wired.x4.at(0))->bridge());

		//The wires between the commons gone, the feed and the field wires
		//kept; the feed is the one wire left on the inside
	CHECK(sheet->conductors().size() == wires_before - 2);
	for (Conductor *c : wired.star) {
		CHECK_FALSE(sheet->conductors().contains(c));
	}
	CHECK(sheet->conductors().contains(wired.feed));
	CHECK(bridge->side() == 0);
	CHECK(bridge->feed() == wired.feed->uuid());

		//One potential: the field wire of 3.2 reaches the feed
	const QSet<Conductor *> potential = wired.field.at(2)->relatedPotentialConductors();
	CHECK(potential.contains(wired.feed));
	CHECK(potential.contains(wired.field.at(0)));
	CHECK_FALSE(potential.contains(wired.outputs.at(0)));

		//The bus bar on the folio: a trunk 8 off the left ends, a tooth to
		//each common, the mark on the feed
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
	CHECK(bus->feed() == wired.feed);
	CHECK(bus->teeth().size() == 3);
	CHECK(bus->trunk().p1().x() == Approx(386 - 8));
	CHECK(qMin(bus->trunk().p1().y(), bus->trunk().p2().y()) == Approx(220));
	CHECK(qMax(bus->trunk().p1().y(), bus->trunk().p2().y()) == Approx(300));
	CHECK_FALSE(bus->feedMark().isEmpty());

		//One undo: no strip, no bus bar, the wires back
	sheet->undoStack().undo();
	CHECK(project->terminalStrip().isEmpty());
	CHECK(TerminalBusItem::busesOn(sheet).isEmpty());
	CHECK(sheet->conductors().size() == wires_before);
		//(the wires between the commons, back, join them again)
	CHECK_FALSE(TerminalBusItem::realTerminalOf(wired.x4.at(1))->parentStrip());
	CHECK_FALSE(TerminalBusItem::realTerminalOf(wired.x4.at(5))->bridge());

		//And again
	sheet->undoStack().redo();
	CHECK(project->terminalStrip().size() == 1);
	REQUIRE(busOf(sheet));
	CHECK(sheet->conductors().size() == wires_before - 2);
}

TEST_CASE("T87 - A wire crossing the trunk of a bus bar hops over it", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	project->setWireHops(WireHops::Mode::Horizontal);
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
		//The output to 2.1 crosses the trunk at x = 378, y = 240; once the
		//wires between the commons are gone, nothing else crosses it
	Conductor *output = wired.outputs.at(1);
	const qreal y = 240;
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	const QRectF painted = output->mapRectToScene(output->paintedPath().boundingRect());
	CHECK(painted.top() < y - 2);
		//The feed meets the trunk at its own tooth: no hop
	const QRectF feed = wired.feed->mapRectToScene(wired.feed->paintedPath().boundingRect());
	const QRectF plain = wired.feed->mapRectToScene(wired.feed->path().boundingRect());
	CHECK(feed.top() == Approx(plain.top()));
}

TEST_CASE("T87 - The arm and its branches become the feed of the bus bar", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	QList<Element *> x4;
	for (const QString &label : {QStringLiteral("1.2"), QStringLiteral("2.2"), QStringLiteral("3.2")}) {
		x4 << byLabel(sheet, label);
	}
	Element *x1 = byLabel(sheet, QStringLiteral("5"));
	REQUIRE_FALSE(x4.contains(nullptr));
	REQUIRE(x1);
		//The arm from X1:5 to 3.2, and 1.2 and 2.2 branching off it (T86)
	Conductor *arm = wire(sheet, x1->terminals().at(1), x4.at(2)->terminals().at(0));
	QList<Conductor *> branches;
	for (int i = 0 ; i < 2 ; ++i)
	{
		const QPointF at = Conductor::nearestOnPath(arm->scenePathPoints(),
							    x4.at(i)->terminals().at(0)->dockConductor());
		Terminal *end = arm->nearerEndTo(at);
		REQUIRE(end);
		Conductor *branch = wire(sheet, x4.at(i)->terminals().at(0), end);
		REQUIRE(branch->setArm(arm, at));
		branches << branch;
	}
		//A name made of a formula and the count it reads, as the folio 5
		//of the request numbers its wires (F5-%sequf_1)
	autonum::sequentialNumbers sequence;
	sequence.unit_folio << QStringLiteral("31");
	arm->setSequenceNum(sequence);
	const ConductorProperties properties = arm->properties();
	REQUIRE(TerminalBusItem::join(sheet, x4, QString()));
	for (Conductor *branch : branches) {
		CHECK_FALSE(sheet->conductors().contains(branch));
	}
		//The arm came to 3.2, the far end of the bus bar: the feed comes
		//to 1.2, the nearest to X1:5, with what the arm was
	CHECK_FALSE(sheet->conductors().contains(arm));
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
	Conductor *feed = bus->feed();
	REQUIRE(feed);
	const QList<Element *> ends {feed->terminal1->parentElement(), feed->terminal2->parentElement()};
	CHECK(ends.contains(x1));
	CHECK(ends.contains(x4.at(0)));
	CHECK(feed->properties() == properties);
	CHECK(feed->sequenceNum() == sequence);
	CHECK(sheet->conductors().size() == 1);
		//One undo: the arm and its branches back
	sheet->undoStack().undo();
	CHECK(sheet->conductors().contains(arm));
	CHECK(sheet->conductors().size() == 3);
}

TEST_CASE("T87 - A bus bar joins neighbouring terminals of one level only", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	const int steps = sheet->undoStack().count();
	QString why;
		//1.2 and 3.2: 2.2 is between them
	CHECK_FALSE(TerminalBusItem::join(sheet, {wired.x4.at(1), wired.x4.at(5)}, QString(), &why));
	CHECK_FALSE(why.isEmpty());
		//1.1 and 1.2: two levels of one terminal
	CHECK_FALSE(TerminalBusItem::join(sheet, {wired.x4.at(0), wired.x4.at(1)}, QString(), &why));
		//1.1 and 2.2: two levels
	CHECK_FALSE(TerminalBusItem::join(sheet, {wired.x4.at(0), wired.x4.at(3)}, QString(), &why));
	CHECK(sheet->undoStack().count() == steps);
	CHECK(project->terminalStrip().isEmpty());
	CHECK(sheet->conductors().size() == 9);
}

TEST_CASE("T87 - The feed chosen, the bus bar removed, and both undone", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
		//A second wire inside, from 2.2 to a component off the strip: no
		//feed is guessed
	Conductor *other = wire(sheet, wired.x4.at(3)->terminals().at(0), byLabel(sheet, QStringLiteral("Q1"))->terminals().at(0));
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
	CHECK_FALSE(bus->feed());
	CHECK(bus->wires().contains(other));

	TerminalBusItem::setFeed(bus, wired.feed);
	CHECK(bus->feed() == wired.feed);
	sheet->undoStack().undo();
	CHECK_FALSE(bus->feed());
	sheet->undoStack().redo();
	CHECK(bus->feed() == wired.feed);

	TerminalBusItem::remove(bus);
	CHECK(TerminalBusItem::busesOn(sheet).isEmpty());
	CHECK_FALSE(TerminalBusItem::realTerminalOf(wired.x4.at(1))->bridge());
	sheet->undoStack().undo();
	REQUIRE(busOf(sheet));
	CHECK(busOf(sheet)->feed() == wired.feed);
}

TEST_CASE("T87 - The bus bar saved and opened again", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	const QUuid feed = wired.feed->uuid();
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));

	INFO(project.error().toStdString());
	REQUIRE(project.saveAndReopen());
	sheet = project.diagram(0);
	REQUIRE(project->terminalStrip().size() == 1);
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
	CHECK(bus->teeth().size() == 3);
	REQUIRE(bus->bridge());
	CHECK(bus->bridge()->feed() == feed);
	CHECK(bus->bridge()->side() == 0);
	REQUIRE(bus->feed());
	CHECK(bus->feed()->uuid() == feed);
}

TEST_CASE("T87 - A terminal of a strip is taken where it is drawn, and its texts by no click", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *terminal = byLabel(sheet, QStringLiteral("2.1"));
	Element *board = byLabel(sheet, QStringLiteral("Q1"));
	REQUIRE(terminal);
	REQUIRE(board);
		//The box of the definition is 20 wide; the drawing, 12
	CHECK(terminal->boundingRect().width() == Approx(20));
	CHECK(terminal->shape().boundingRect().width() == Approx(14));
		//Any other component keeps its box
	CHECK(board->shape().boundingRect().width() == Approx(20));
	REQUIRE_FALSE(terminal->dynamicTextItems().isEmpty());
	for (DynamicElementTextItem *text : terminal->dynamicTextItems())
	{
		CHECK_FALSE(text->flags().testFlag(QGraphicsItem::ItemIsSelectable));
		CHECK(text->acceptedMouseButtons() == Qt::NoButton);
	}
	for (DynamicElementTextItem *text : board->dynamicTextItems()) {
		CHECK(text->flags().testFlag(QGraphicsItem::ItemIsSelectable));
	}
}

TEST_CASE("T87 - The strip the manager generates draws the bridge", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	REQUIRE(project->terminalStrip().size() == 1);
	auto *generated = new TerminalStripItem(project->terminalStrip().first());
	sheet->addItem(generated);
		//Away from the bus bar of the folio, which goes with the bridge too
	generated->setPos(700, 450);
	const QRectF area = generated->sceneBoundingRect();
	REQUIRE(area.isValid());
	TerminalBusItem *drawn = busOf(sheet);
	REQUIRE(drawn);
	REQUIRE_FALSE(area.intersects(drawn->sceneBoundingRect()));
		//The pictures with the bridge and without it: the line of the
		//bridge is drawn over the grey of the terminals, so it is the
		//pixels that change that tell, and not the ink
	const auto picture = [sheet, area]() {
		QImage image(area.size().toSize() * 2, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		sheet->render(&painter, QRectF(image.rect()), area);
		painter.end();
		return image;
	};
	const QImage with_bridge = picture();
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
	TerminalBusItem::remove(bus);
	REQUIRE(TerminalBusItem::busesOn(sheet).isEmpty());
	const QImage without = picture();
	REQUIRE(without.size() == with_bridge.size());
	int changed = 0;
	for (int y = 0 ; y < without.height() ; ++y)
		for (int x = 0 ; x < without.width() ; ++x)
			if (without.pixel(x, y) != with_bridge.pixel(x, y)) ++changed;
	CHECK(changed > 20);
}

TEST_CASE("T87 - Terminals added to a bus bar", "[T87][bus]")
{
	Setup setup;
		//The terminals 1.1 and 1.2 of no strip yet: the bus bar of 2.2 and
		//3.2 makes the strip X4 of the terminals 2 and 3 only
	UiBench::ScratchProject project(fixture(QString()));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, {wired.x4.at(3), wired.x4.at(5)}, QString()));
	REQUIRE(project->terminalStrip().size() == 1);
	TerminalStrip *strip = project->terminalStrip().first();
	REQUIRE(strip->physicalTerminalCount() == 2);
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
	const QSharedPointer<TerminalStripBridge> bridge = bus->bridge();
	REQUIRE(bridge);
	REQUIRE(bridge->realTerminals().size() == 2);
	const int side = bridge->side();
	const int wires_before = sheet->conductors().size();

		//The bus bar selected with 1.2: what Join into a bus bar is given
	const QList<Element *> selection = TerminalBusItem::joinable({bus, wired.x4.at(1)});
	CHECK(selection.size() == 3);
	CHECK(selection.contains(wired.x4.at(1)));

	QString why;
	REQUIRE(TerminalBusItem::join(sheet, selection, QString(), &why));
	CHECK(why.isEmpty());
	CAPTURE(why);
		//The terminal 1, with its two levels, before 2 in the strip, and not
		//after 3, where the strip puts what it is given: the bus bar joins
		//neighbours only
	REQUIRE(strip->physicalTerminalCount() == 3);
	const auto first = strip->physicalTerminal(0);
	REQUIRE(first->realTerminalCount() == 2);
	CHECK(first->realTerminals().at(0)->element() == wired.x4.at(0));
	CHECK(first->realTerminals().at(1)->element() == wired.x4.at(1));
	CHECK(strip->physicalTerminal(1)->realTerminals().at(1)->element() == wired.x4.at(3));
		//The same bridge, given the terminal: its side, and its feed - the
		//wire from X1 now, the one from 1.2 to the bus bar gone with it
	CHECK(TerminalBusItem::realTerminalOf(wired.x4.at(1))->bridge() == bridge);
	CHECK(bridge->realTerminals().size() == 3);
	CHECK(bridge->side() == side);
	CHECK(bridge->feed() == wired.feed->uuid());
	CHECK(sheet->conductors().size() == wires_before - 1);
	REQUIRE(busOf(sheet));
	CHECK(busOf(sheet)->teeth().size() == 3);
	CHECK(sheet->undoStack().undoText() == QStringLiteral("Add terminals to the bus bar"));

		//One undo: the terminal out of the strip, the bridge as it was
	sheet->undoStack().undo();
	CHECK(strip->physicalTerminalCount() == 2);
	CHECK_FALSE(TerminalBusItem::realTerminalOf(wired.x4.at(1))->parentStrip());
	CHECK(bridge->realTerminals().size() == 2);
	CHECK(sheet->conductors().size() == wires_before);
	sheet->undoStack().redo();
	CHECK(strip->physicalTerminalCount() == 3);
	CHECK(bridge->realTerminals().size() == 3);

		//The bus bar alone with its terminals: nothing to add
	CHECK_FALSE(TerminalBusItem::join(sheet, TerminalBusItem::joinable({busOf(sheet)}), QString(), &why));
	CHECK_FALSE(why.isEmpty());
}

TEST_CASE("T87 - The colour of a bus bar, and the bar under the wires", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
		//A wire that hops over the bar is drawn on it, and not under it
	CHECK(bus->zValue() < wired.outputs.at(1)->zValue());
	CHECK(bus->zValue() > wired.x4.at(1)->zValue());

	const QColor before = bus->bridge()->color();
	const QColor after = before == QColor(Qt::blue) ? QColor(Qt::red) : QColor(Qt::blue);
	TerminalBusItem::setColor(bus, after);
	CHECK(bus->bridge()->color() == after);
	sheet->undoStack().undo();
	CHECK(bus->bridge()->color() == before);
	sheet->undoStack().redo();
	CHECK(bus->bridge()->color() == after);
}

TEST_CASE("T87 - The wire of a joined terminal starts past the trunk", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	TerminalBusItem *bus = busOf(sheet);
	REQUIRE(bus);
	REQUIRE(bus->side() == 0);
		//The trunk at 378, 8 off the left ends of the terminals at 386, and
		//3.5 wide: the feed, which comes to 1.2 from the left, is drawn from
		//its outer edge, and is still joined to the end of 1.2
	const qreal edge = 386 - 8 - 3.5 / 2;
	const QRectF plain = wired.feed->mapRectToScene(wired.feed->path().boundingRect());
	const QRectF painted = wired.feed->mapRectToScene(wired.feed->paintedPath().boundingRect());
	CHECK(plain.right() == Approx(386));
	CHECK(painted.right() == Approx(edge));
	CHECK(wired.feed->terminal2->dockConductor().x() == Approx(386));
		//A wire by the other side of a joined terminal is drawn whole
	Conductor *field = wired.field.at(0);
	CHECK(field->mapRectToScene(field->paintedPath().boundingRect()).left() == Approx(414));
		//And a wire of a terminal out of the bus bar too
	Conductor *output = wired.outputs.at(0);
	CHECK(output->mapRectToScene(output->paintedPath().boundingRect()).right()
	      == Approx(output->mapRectToScene(output->path().boundingRect()).right()));
}

TEST_CASE("T87 - A wire crossing a bus bar hops over it whatever the folio does", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
		//The output to 2.1 crosses the trunk at y = 240: it hops with no
		//hop drawn between the wires, and with the vertical wires hopping
	Conductor *output = wired.outputs.at(1);
	const qreal y = 240;
	for (const WireHops::Mode mode : {WireHops::Mode::None, WireHops::Mode::Vertical, WireHops::Mode::Horizontal})
	{
		CAPTURE(int(mode));
		project->setWireHops(mode);
		Conductor::othersChangedShape();
		const QRectF painted = output->mapRectToScene(output->paintedPath().boundingRect());
		CHECK(painted.top() < y - 2);
	}
}

TEST_CASE("T87 - A bus bar on each level of the same terminals", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
		//The commons, on the second level, and then the signals, on the
		//first: two bridges of one strip
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	QString why;
	REQUIRE(TerminalBusItem::join(sheet, {wired.x4.at(0), wired.x4.at(2), wired.x4.at(4)}, QString(), &why));
	CAPTURE(why);
	REQUIRE(project->terminalStrip().size() == 1);
	TerminalStrip *strip = project->terminalStrip().first();
	CHECK(strip->physicalTerminalCount() == 3);
	const QSharedPointer<TerminalStripBridge> signals_ = TerminalBusItem::realTerminalOf(wired.x4.at(0))->bridge();
	const QSharedPointer<TerminalStripBridge> commons = TerminalBusItem::realTerminalOf(wired.x4.at(1))->bridge();
	REQUIRE(signals_);
	REQUIRE(commons);
	CHECK(signals_ != commons);
	CHECK(signals_->realTerminals().size() == 3);
	CHECK(commons->realTerminals().size() == 3);

		//Two bus bars by the left of the strip, whose stretches overlap: the
		//first level inside, 8 off the ends of the terminals at 386, the
		//second 12 further out, room for a hop between them
	const QList<TerminalBusItem *> buses = TerminalBusItem::busesOn(sheet);
	REQUIRE(buses.size() == 2);
	TerminalBusItem *inner = buses.at(0)->bridge() == signals_ ? buses.at(0) : buses.at(1);
	TerminalBusItem *outer = inner == buses.at(0) ? buses.at(1) : buses.at(0);
	CHECK(inner->trunk().p1().x() == Approx(386 - 8));
	CHECK(outer->trunk().p1().x() == Approx(386 - 8 - 12));
		//The teeth of the commons, at 220 and 260, hop over the trunk of the
		//signals, which runs from 200 to 280; the one at 300 is past its end
	const QList<QPointF> hops = outer->toothHops();
	CHECK(hops.size() == 2);
	CHECK(hops.contains(QPointF(378, 220)));
	CHECK(hops.contains(QPointF(378, 260)));
	CHECK(inner->toothHops().isEmpty());

		//One undo: the bus bar of the commons alone, back next to the
		//terminals
	sheet->undoStack().undo();
	REQUIRE(TerminalBusItem::busesOn(sheet).size() == 1);
	CHECK(TerminalBusItem::busesOn(sheet).first()->trunk().p1().x() == Approx(386 - 8));
}

TEST_CASE("T87 - A second bus bar of a strip goes into it", "[T87][bus]")
{
	Setup setup;
		//The terminals 4 and 5, of no strip and with no comment, below the
		//strip X4 the first bus bar makes
	UiBench::ScratchProject project(fixture(QStringLiteral("X4"), true));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	REQUIRE(project->terminalStrip().size() == 1);
	TerminalStrip *strip = project->terminalStrip().first();
	REQUIRE(strip->physicalTerminalCount() == 3);
	Element *four = byLabel(sheet, QStringLiteral("4.2"));
	Element *five = byLabel(sheet, QStringLiteral("5.2"));
	REQUIRE(four);
	REQUIRE(five);

		//Named X4 when asked: into the strip X4, and not a second one
	REQUIRE(TerminalBusItem::needsStripName({four, five}));
	QString why;
	REQUIRE(TerminalBusItem::join(sheet, {four, five}, QStringLiteral("X4"), &why));
	CAPTURE(why);
	CHECK(project->terminalStrip().size() == 1);
	CHECK(strip->physicalTerminalCount() == 5);
	const QSharedPointer<TerminalStripBridge> second = TerminalBusItem::realTerminalOf(four)->bridge();
	REQUIRE(second);
	CHECK(second != TerminalBusItem::realTerminalOf(wired.x4.at(1))->bridge());
	CHECK(second->realTerminals().size() == 2);
		//One after the other on one level: both next to the terminals
	const QList<TerminalBusItem *> buses = TerminalBusItem::busesOn(sheet);
	REQUIRE(buses.size() == 2);
	CHECK(buses.at(0)->trunk().p1().x() == Approx(386 - 8));
	CHECK(buses.at(1)->trunk().p1().x() == Approx(386 - 8));
}

TEST_CASE("T87 - A bus bar more of a strip goes by the side of the others", "[T87][bus]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	const QSharedPointer<TerminalStripBridge> commons = TerminalBusItem::realTerminalOf(wired.x4.at(1))->bridge();
	REQUIRE(commons);
	REQUIRE(commons->side() == 0);
		//The signals with a wire by the right, where they have the fewest:
		//alone, their bus bar would be drawn there
	wire(sheet, wired.x4.at(0)->terminals().at(1), byLabel(sheet, QStringLiteral("Y1"))->terminals().at(0));
	REQUIRE(TerminalBusItem::join(sheet, {wired.x4.at(0), wired.x4.at(2), wired.x4.at(4)}, QString()));
	const QSharedPointer<TerminalStripBridge> signals_ = TerminalBusItem::realTerminalOf(wired.x4.at(0))->bridge();
	REQUIRE(signals_);
		//By the left, with the commons, in the lane inside
	CHECK(signals_->side() == 0);
	const QList<TerminalBusItem *> buses = TerminalBusItem::busesOn(sheet);
	REQUIRE(buses.size() == 2);
	for (const TerminalBusItem *bus : buses) {
		CHECK(bus->trunk().p1().x() == Approx(bus->bridge() == signals_ ? 386 - 8 : 386 - 8 - 12));
	}
}
