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

#include "../../../../sources/conductorautonumerotation.h"
#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/conductortextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/addgraphicsobjectcommand.h"

#include <catch2/catch.hpp>

#include <QSet>
#include <QString>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>

/*
	The number of a potential goes through a folio report.

	A wire numbered 24V on folio 1 ends on a report arrow; the arrow is
	linked to its pair on folio 2, where nothing is wired yet. A wire drawn
	there, from the arriving arrow to a component, is on the same electrical
	node as the 24V of folio 1, and its number has to say so.

	The walk that decides what a potential is - Conductor::related-
	PotentialConductors(), through relatedPotentialTerminal() of terminal.cpp
	- crosses a linked report when it is asked for every folio, and the
	automatic numbering of a new wire, ConductorAutoNumerotation, asks it
	that way. Nothing in conductorautonumerotation.cpp names a report, and
	it does not need to: the crossing is done one floor down, for every
	caller of the walk at once. That is what is measured here, from the
	two callers a draughtsman reaches - a wire drawn and numbered, and a
	number typed over a wire.

	The wire is drawn the way Terminal::mouseReleaseEvent() draws one when
	the potential does not hand it its properties outright: a conductor
	between the two terminals, its addition and its numbering in one undo
	step pushed on the stack of the project. A real drag would need the
	terminal to be the topmost item under a point of the scene, which is
	geometry this case is not about.

	Labelled T31 and not CU-31.4: the use case links three folios and asks
	every arrow to name the other two, which needs the arrows to form a
	group of more than two (step 7 of T31). The case of two folios is the
	one the walk already answers, and it is the one measured.
*/

namespace
{
	const char *leaving_part_uuid = "{31080000-0000-4000-8000-000000000001}";
	const char *going_uuid = "{31080000-0000-4000-8000-000000000002}";
	const char *coming_uuid = "{31080000-0000-4000-8000-000000000003}";
	const char *arriving_part_uuid = "{31080000-0000-4000-8000-000000000004}";

	/**
		An element of @a definition, its terminals written at their docking
		point - four units inside the point the symbol declares, which is
		what the reading path matches an instance terminal to a symbol
		terminal by. Written at the declared point, the project opens with
		no conductor at all.
	*/
	QString instance(const QString &definition, const char *uuid, int x, int y,
			 int top_id, int bottom_id, const char *linked_to)
	{
		QString terminals = QStringLiteral(
					    "<terminal x=\"0\" y=\"-6\" orientation=\"0\" id=\"%1\"/>")
				    .arg(top_id);
		if (bottom_id >= 0) {
			terminals += QStringLiteral(
					     "<terminal x=\"0\" y=\"6\" orientation=\"2\" id=\"%1\"/>")
				     .arg(bottom_id);
		}

		QString links;
		if (linked_to) {
			links = QStringLiteral("<link_uuid uuid=\"%1\"/>")
				.arg(QLatin1String(linked_to));
		}

		return QStringLiteral(
			       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/%3\" uuid=\"%4\">"
			       "<terminals>%5</terminals>"
			       "<inputs/>"
			       "<links_uuids>%6</links_uuids>"
			       "<elementInformations/>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(QString::number(x), QString::number(y), definition,
			     QString::fromLatin1(uuid), terminals, links);
	}

	QString symbol(const QString &name, const QString &link_type, bool two_terminals)
	{
		QString terminals = QStringLiteral(
					    "<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>");
		if (two_terminals) {
			terminals += QStringLiteral(
					     "<terminal x=\"0\" y=\"10\" orientation=\"s\"/>");
		}

		return QStringLiteral(
			       "<element name=\"%1\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"40\""
			       " hotspot_x=\"10\" hotspot_y=\"20\""
			       " orientation=\"dnnn\" link_type=\"%2\">"
			       "<names><name lang=\"en\">%1</name></names>"
			       "<description>"
			       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			       " end1=\"none\" end2=\"none\" length1=\"1.5\""
			       " length2=\"1.5\" antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "%3"
			       "</description>"
			       "</definition>"
			       "</element>")
			.arg(name, link_type, terminals);
	}

	/**
		Folio 1: a component, the 24V leaving it, and the arrow it leaves
		by. Folio 2: the arrow it arrives by, linked to the first one in the
		file, and a component nothing is wired to yet.
	*/
	QString fixtureXml()
	{
		const QString part = QStringLiteral("part.elmt");

		const QString leaving_elements =
				instance(part, leaving_part_uuid, 100, 100, 0, 1, nullptr)
				+ instance(QStringLiteral("going.elmt"), going_uuid, 100, 300,
					   2, -1, coming_uuid);
		const QString leaving_wires = QStringLiteral(
			"<conductor x=\"0\" y=\"0\" terminal1=\"1\" terminal2=\"2\""
			" num=\"24V\" type=\"multi\" freezeLabel=\"false\""
			" displaytext=\"1\" numsize=\"9\" condsize=\"1\"/>");

		const QString arriving_elements =
				instance(QStringLiteral("coming.elmt"), coming_uuid, 100, 100,
					 3, -1, going_uuid)
				+ instance(part, arriving_part_uuid, 100, 300, 4, 5, nullptr);

		const QString folio = QStringLiteral(
			"<diagram title=\"Folio %1\" order=\"%1\" height=\"600\""
			" cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements>"
			"<inputs/>"
			"<conductors>%3</conductors>"
			"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1</category></collection>"
			       "%2%3"
			       "</project>")
			.arg(symbol(part, QStringLiteral("simple"), true)
			     + symbol(QStringLiteral("going.elmt"), QStringLiteral("next_report"), false)
			     + symbol(QStringLiteral("coming.elmt"), QStringLiteral("previous_report"), false),
			     folio.arg(QStringLiteral("1"), leaving_elements, leaving_wires),
			     folio.arg(QStringLiteral("2"), arriving_elements, QString()));
	}

	Element *elementWithUuid(Diagram *folio, const char *uuid)
	{
		if (!folio) {
			return nullptr;
		}
		const QList<Element *> elements = folio->elements();
		for (Element *element : elements)
		{
			if (element->uuid() == QUuid(QLatin1String(uuid))) {
				return element;
			}
		}
		return nullptr;
	}

	/**
		A wire between @a from and @a to, added and numbered in one step,
		as Terminal::mouseReleaseEvent() does when it hands the numbering
		to ConductorAutoNumerotation.
	*/
	Conductor *drawNumberedWire(Diagram *folio, Terminal *from, Terminal *to)
	{
		auto *wire = new Conductor(from, to);
		auto *step = new QUndoCommand();
		auto *added = new AddGraphicsObjectCommand(wire, folio, QPointF(), step);
		step->setText(added->text());

		ConductorAutoNumerotation numbering(wire, folio, step);
		numbering.numerate();

		folio->undoStack().push(step);
		return wire;
	}

	/// A number typed over @a wire on the folio, through the slot its text calls.
	void typeOver(Conductor *wire, const QString &text)
	{
		wire->textItem()->setPlainText(text);
		wire->displayedTextChanged();
	}
}

TEST_CASE("T31 — o número do potencial atravessa a seta de folha: o fio da folha de chegada leva o número da folha de partida",
	  "[uibench][t31][potential]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("reportpotential.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 2);

	Diagram *leaving = bench.diagram(0);
	Diagram *arriving = bench.diagram(1);

	Element *going = elementWithUuid(leaving, going_uuid);
	Element *coming = elementWithUuid(arriving, coming_uuid);
	Element *arriving_part = elementWithUuid(arriving, arriving_part_uuid);
	REQUIRE(going != nullptr);
	REQUIRE(coming != nullptr);
	REQUIRE(arriving_part != nullptr);

		//The fixture is what it says: the arrows linked as the file links
		//them, the 24V on folio 1 and nothing wired on folio 2.
	REQUIRE(going->linkedElements().size() == 1);
	REQUIRE(going->linkedElements().first() == coming);
	REQUIRE(leaving->conductors().size() == 1);
	Conductor *wire_24 = leaving->conductors().first();
	REQUIRE(wire_24->properties().text == QStringLiteral("24V"));
	REQUIRE(wire_24->textItem() != nullptr);
	REQUIRE(arriving->conductors().isEmpty());
	REQUIRE(coming->terminals().size() == 1);
	REQUIRE(arriving_part->terminals().size() == 2);

	Terminal *arrow_end = coming->terminals().first();
	Terminal *part_end = arriving_part->terminals().first();

	SECTION("a numeração automática de um fio novo na folha de chegada dá a ele o 24V da folha de partida")
	{
		Conductor *arrived = drawNumberedWire(arriving, arrow_end, part_end);
		REQUIRE(arriving->conductors().contains(arrived));

		CHECK(arrived->properties().text == QStringLiteral("24V"));

			//One potential over two folios, seen from both ends...
		const QSet<Conductor *> from_arrival = arrived->relatedPotentialConductors();
		CHECK(from_arrival.size() == 1);
		CHECK(from_arrival.contains(wire_24));
		CHECK(wire_24->relatedPotentialConductors().contains(arrived));
			//...and only through the arrow: asked for its own folio alone,
			//the wire is a potential of one.
		CHECK(arrived->relatedPotentialConductors(false).isEmpty());
	}

	SECTION("o número digitado num lado da seta chega ao outro, nos dois sentidos, e volta junto no desfazer")
	{
		Conductor *arrived = drawNumberedWire(arriving, arrow_end, part_end);
		REQUIRE(arrived->properties().text == QStringLiteral("24V"));
		REQUIRE(arrived->textItem() != nullptr);
		QUndoStack &stack = leaving->undoStack();

		typeOver(wire_24, QStringLiteral("31"));
		CHECK(wire_24->properties().text == QStringLiteral("31"));
		CHECK(arrived->properties().text == QStringLiteral("31"));

		stack.undo();
		CHECK(wire_24->properties().text == QStringLiteral("24V"));
		CHECK(arrived->properties().text == QStringLiteral("24V"));

			//Typed on folio 2, against the direction of the arrow.
		typeOver(arrived, QStringLiteral("32"));
		CHECK(arrived->properties().text == QStringLiteral("32"));
		CHECK(wire_24->properties().text == QStringLiteral("32"));

		stack.undo();
		CHECK(wire_24->properties().text == QStringLiteral("24V"));
		CHECK(arrived->properties().text == QStringLiteral("24V"));
	}

	SECTION("controle negativo — com as setas desligadas, o potencial para na borda da folha")
	{
		going->unlinkAllElements();
		REQUIRE(going->linkedElements().isEmpty());
		REQUIRE(coming->linkedElements().isEmpty());

		Conductor *arrived = drawNumberedWire(arriving, arrow_end, part_end);
		REQUIRE(arriving->conductors().contains(arrived));

			//The same wire, drawn the same way between the same two
			//terminals: without the link it is a potential of one, and the
			//24V of folio 1 is not its number.
		CHECK(arrived->relatedPotentialConductors().isEmpty());
		CHECK(arrived->properties().text != QStringLiteral("24V"));

		const QString arrived_before = arrived->properties().text;
		REQUIRE(arrived_before != QStringLiteral("31"));
		typeOver(wire_24, QStringLiteral("31"));
		CHECK(wire_24->properties().text == QStringLiteral("31"));
		CHECK(arrived->properties().text == arrived_before);
	}
}
