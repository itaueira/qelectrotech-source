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

#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QEvent>
#include <QGraphicsSceneMouseEvent>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <algorithm>

/*
	A terminal drawn on a folio and a line of the terminal strip manager are
	the same object seen twice, and only one of the two ways between them
	existed: the manager sends the reader to the schematic on a double click
	of its cross reference column. From a terminal on a folio there was no
	path at all to the strip it belongs to - not a menu entry, not a shortcut,
	nothing.

	The double click is the gesture tried without reading anything, so it is
	the one that answers. What is proved here is the half of that answer a
	bench can prove, and the split is deliberate:

	  - the decision - which strip this terminal belongs to - is a question
	    about the project, and it is the half that can silently answer the
	    wrong strip. Terminal::parentTerminalStrip() is the only place that
	    answers it, and the cases below ask it directly;
	  - the window it opens is a question about the application, and no case
	    of this bench has a QETApp: TerminalStripEditorWindow::instance()
	    reaches QETApp::instance() to find the editor it takes as its parent.
	    Sending a double click to a terminal that has a strip would therefore
	    not test the navigation, it would kill the run.

	So the click itself is sent only where it must do nothing - a terminal no
	strip has taken - and what is measured there is that the base class still
	gets it. That is the regression this override could cause and the one that
	would be found by a person rather than by a number: a double click on any
	terminal of any element starts a conductor, and it has to keep doing it.

	Labelled T31 and not CU-31.3: the case of use is opening the strip window
	from the schematic and seeing the terminal highlighted in it, which is a
	window this bench does not open.
*/

namespace
{
		/// Four folios and fifty-two terminal elements, none of them on a
		/// strip: no example shipped with the program declares a
		/// <terminal_strip>, which is why the cases below make their own.
	const char *terminal_example = "perceuse.qet";

	/**
		The terminal elements of @a project, in an order that does not move
		between runs: folios in project order, components sorted by uuid
		inside each. QGraphicsScene::items() answers in an order of its own.
	*/
	QList<Element *> terminalElements(QETProject *project)
	{
		QList<Element *> found;

		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			QList<Element *> elements = folio->elements();
			std::sort(elements.begin(), elements.end(),
				  [](const Element *a, const Element *b)
			{
				return a->uuid().toString() < b->uuid().toString();
			});

			const QList<Element *> sorted = elements;
			for (Element *element : sorted)
			{
				if (element->linkType() & Element::Terminale) {
					found.append(element);
				}
			}
		}

		return found;
	}

	/**
		A component of @a project that is not a terminal and carries at least
		one free connection point, nullptr when the project has none.

		Free on purpose: a connection point that already holds a conductor
		has that conductor drawn over it, and a case about where a click
		lands has no business arguing with the drawing order.
	*/
	Terminal *freeTerminalOfOrdinaryElement(QETProject *project)
	{
		const QList<Diagram *> folios = project->diagrams();
		for (Diagram *folio : folios)
		{
			QList<Element *> elements = folio->elements();
			std::sort(elements.begin(), elements.end(),
				  [](const Element *a, const Element *b)
			{
				return a->uuid().toString() < b->uuid().toString();
			});

			const QList<Element *> sorted = elements;
			for (Element *element : sorted)
			{
				if (element->linkType() & Element::Terminale) {
					continue;
				}

				const QList<Terminal *> terminals = element->terminals();
				for (Terminal *terminal : terminals)
				{
					if (!terminal->conductorsCount()) {
						return terminal;
					}
				}
			}
		}

		return nullptr;
	}

	/**
		The double click the scene would hand to the item at @a scene_pos,
		sent the way QGraphicsView sends it.
	*/
	void doubleClickAt(Diagram *folio, const QPointF &scene_pos)
	{
		QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseDoubleClick);
		event.setButton(Qt::LeftButton);
		event.setButtons(Qt::LeftButton);
		event.setScenePos(scene_pos);
		event.setPos(scene_pos);
		event.setScreenPos(QPoint(0, 0));

		QCoreApplication::sendEvent(folio, &event);
	}
}

TEST_CASE("T31 — um borne do esquema sabe a régua de bornes a que pertence",
	  "[navigation][terminalstrip]")
{
	UiBench::Project bench(terminal_example);
	REQUIRE(bench.isOpen());

	const QList<Element *> terminal_elements = terminalElements(bench.project());
	REQUIRE(terminal_elements.count() > 0);

	Element *terminal_element = terminal_elements.first();
	const QList<Terminal *> terminals = terminal_element->terminals();
	REQUIRE(terminals.count() > 0);

	SECTION("um borne que nenhuma régua tomou não aponta para régua nenhuma")
	{
			//The example ships no <terminal_strip>, so every terminal of it
			//is a free one. A wrong answer here would open a window on
			//someone else's strip.
		for (Terminal *terminal : terminals) {
			CHECK(terminal->parentTerminalStrip() == nullptr);
		}
	}

	SECTION("posto numa régua, todo ponto de ligação dele aponta para ela")
	{
		TerminalStrip *strip = bench->newTerminalStrip(QString(),
							       QString(),
							       QStringLiteral("X1"));
		REQUIRE(strip != nullptr);
		REQUIRE(strip->addTerminal(terminal_element));

			//Every connection point of the element, and not the first one:
			//a terminal block is drawn with one point on each side, and
			//the reader clicks whichever is nearer the wire.
		for (Terminal *terminal : terminals) {
			CHECK(terminal->parentTerminalStrip() == strip);
		}

			//And no other terminal element was dragged along with it.
		if (terminal_elements.count() > 1)
		{
			const QList<Terminal *> other =
					terminal_elements.at(1)->terminals();
			for (Terminal *terminal : other) {
				CHECK(terminal->parentTerminalStrip() == nullptr);
			}
		}
	}

	SECTION("um ponto de ligação de outro componente nunca aponta para régua")
	{
		TerminalStrip *strip = bench->newTerminalStrip(QString(),
							       QString(),
							       QStringLiteral("X1"));
		REQUIRE(strip != nullptr);
		REQUIRE(strip->addTerminal(terminal_element));

		Terminal *ordinary = freeTerminalOfOrdinaryElement(bench.project());
		REQUIRE(ordinary != nullptr);

			//A coil, a contact, a motor: the strip of the folio has
			//nothing to do with them, and answering one would send the
			//reader to a window about someone else.
		CHECK(ordinary->parentTerminalStrip() == nullptr);
	}
}

TEST_CASE("T31 — o duplo clique num ponto de ligação comum continua começando um condutor",
	  "[navigation][terminalstrip]")
{
	UiBench::Project bench(terminal_example);
	REQUIRE(bench.isOpen());

	Terminal *terminal = freeTerminalOfOrdinaryElement(bench.project());
	REQUIRE(terminal != nullptr);

	Diagram *folio = terminal->diagram();
	REQUIRE(folio != nullptr);
	REQUIRE(terminal->parentTerminalStrip() == nullptr);

		/* The conductor setter is the dashed stub drawn between two
		 * terminals while a conductor is being made. Diagram::setConductor()
		 * puts it in the scene and takes it back out, and the item itself is
		 * private - so what is counted is the scene growing by exactly one
		 * item, which is the same thing said from outside.
		 *
		 * Terminal::mousePressEvent is what asks for it, and the base class
		 * of mouseDoubleClickEvent is what hands the double click to
		 * mousePressEvent. Cut that hand-over and this case goes red while
		 * every other case of this file stays green: the override would
		 * swallow the double click of every terminal of the program, and
		 * drawing a conductor by double clicking would stop working without
		 * a single error message. */
	const int items_before = folio->items().count();

	doubleClickAt(folio, terminal->dockConductor());

	CHECK(folio->items().count() == items_before + 1);

	folio->setConductor(false);
	CHECK(folio->items().count() == items_before);
}
