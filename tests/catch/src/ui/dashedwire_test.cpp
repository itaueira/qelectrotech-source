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
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/assignlocationcommand.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QGraphicsScene>
#include <QList>
#include <QRectF>
#include <QUndoStack>

#include <utility>

/*
	The dashed external wire, and the day it stopped being repainted.

	A wire whose two ends stand in different locations is drawn dashed, and
	the choice is made in Conductor::paint() out of the location its two
	components carry - stored nowhere, so that the style the draughtsman set
	by hand survives. The price of deciding it there is that somebody has to
	say when a location moved, and that was the defect: the folio option
	said it (Diagram::setDashExternalWires walks the conductors), the
	component did not. Changing the location of one end left the wire on the
	screen exactly as it was until something else happened to dirty it - a
	scroll, in the session where it was found.

	@par How "was asked to redraw" is checked without a screen

	Not by looking at the drawing. A rendering repaints everything it is
	asked to render, so an image - or an SVG export, which is the same path
	- comes out right whether or not anything was ever invalidated: it would
	have passed on the broken code, which makes it the wrong instrument for
	this defect.

	What is watched instead is QGraphicsScene::changed, the signal the scene
	emits with the regions it wants back on screen. That is the actual thing
	that was missing. It arrives through the event queue, hence the
	processEvents() in RepaintWatch::drain(), and it works with no view
	attached: with an empty view list QGraphicsScene falls back to
	QGraphicsScene::update() over the item's scene rectangle, which is
	exactly the rectangle to be looked for.

	An instrument nobody calibrated proves nothing, so two controls come
	first, in their own sections: an update() asked for by hand has to be
	seen, and a quiet folio has to show nothing. Only then do the location
	cases mean anything.

	The drawing is checked too, once, and by ink rather than by shape - a
	dashed stroke leaves less of it on the same path than a solid one. That
	is what ties the invalidation being measured here to something a person
	would see.
*/

namespace {

	/// The two enclosures of the fixture. Codes, as the components store them.
	const QString panel_a = QStringLiteral("QCM1");
	const QString panel_b = QStringLiteral("QCM2");

	/**
		The project the cases work on, as text.

		Four boxes and two wires: one pair whose location the cases move
		about, and one pair standing well away from it, untouched, so that
		"only the wires that had to be repainted were" is a measurement and
		not a hope.

		The symbol carries a terminal on each side and nothing else - no
		dynamic text, no label drawn. The band of the sheet the ink is
		counted over then holds the wire and nothing but the wire, which is
		what lets a difference of a few dozen pixels mean something.

		The two ends of a wire are named by the id of their terminal, and the
		coordinates of an instance terminal are the docking point and not the
		point of the definition: Terminal::init() shifts it by terminalSize
		towards the body. That shift is computed here rather than written out
		as a number, so the fixture cannot rot the day the constant moves.
	*/
	QString fixtureXml()
	{
		struct Box
		{
			int x;
			int y;
			const char *label;
			const char *location;
		};

		// Two pairs. The first is the one under test, the second is the
		// witness: far away, in one location, and never touched.
		const Box boxes[] = {
			{200, 200, "K1", "QCM1"},
			{320, 200, "K2", "QCM2"},
			{200, 380, "K3", "QCM1"},
			{320, 380, "K4", "QCM1"}};

		// The docking point of a terminal, which is what the instance stores.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString instances;
		int index = 0;
		for (const Box &box : boxes)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/box.elmt\""
					     " uuid=\"{decaf000-0000-4000-8000-00000000000%3}\">"
					     "<terminals>"
					     "<terminal x=\"%4\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
					     "<terminal x=\"%6\" y=\"0\" orientation=\"3\" id=\"%7\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%8"
					     "</elementInformation>"
					     "<elementInformation show=\"1\" name=\"location_path\">%9"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(box.x)
				     .arg(box.y)
				     .arg(index)
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(QLatin1String(box.label),
					  QLatin1String(box.location));
			++index;
		}

		// Both wires are born with the stock style, solid. The section that
		// checks the hand-set style sets it there, the way the properties
		// dialogue does, rather than here: a wire that is already dashed by
		// hand cannot be used to measure the dash the location rule lays
		// over it, since both are holes in the same line.
		const QString conductors = QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"3\" num=\"\""
			" displaytext=\"0\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"4\" terminal2=\"7\" num=\"\""
			" displaytext=\"0\" type=\"multi\" condsize=\"1\"/>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"30\" height=\"20\""
			       " hotspot_x=\"15\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\""
			       " dashExternalWires=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors>%2</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(instances, conductors);
	}

	/// The box carrying @a label, or nullptr when the sheet has none.
	Element *box(Diagram *sheet, const QString &label)
	{
		if (!sheet) {
			return nullptr;
		}

		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->elementInformations()
			    .value(QETInformation::ELMT_LABEL).toString() == label) {
				return element;
			}
		}
		return nullptr;
	}

	/// The wire joining @a first and @a second, nullptr when there is none.
	Conductor *wireBetween(Diagram *sheet, Element *first, Element *second)
	{
		if (!sheet || !first || !second) {
			return nullptr;
		}

		const QList<Conductor *> wires = sheet->conductors();
		for (Conductor *wire : wires)
		{
			Element *a = wire->terminal1->parentElement();
			Element *b = wire->terminal2->parentElement();
			if ((a == first && b == second) || (a == second && b == first)) {
				return wire;
			}
		}
		return nullptr;
	}

	/**
		The regions the folio asked to have back on screen.

		Cleared by hand between steps rather than at every read, because a
		case wants to say "from here on" and then ask twice about the same
		stretch - once about the wire that had to move, once about the wire
		that had to stay still.
	*/
	class RepaintWatch
	{
		public:
			explicit RepaintWatch(Diagram *sheet)
			{
				if (!sheet) {
					return;
				}
				QObject::connect(sheet, &QGraphicsScene::changed,
						 &m_context,
						 [this](const QList<QRectF> &regions)
						 {m_regions.append(regions);});
			}

			/// Forget everything asked for so far, queue included.
			void clear()
			{
				drain();
				m_regions.clear();
			}

			/**
				Whether the folio asked for @a item itself.

				A region has to hold the whole rectangle of the item, and
				intersecting it is not enough. That is not fussiness, it is
				what makes the answer mean anything here: with no view
				attached, a scene that has anything at all to redraw walks
				its top level items and hands back the four components as
				well, the same four rectangles whatever was touched -
				measured, by asking for a bare update() of one wire and
				reading what came back. A
				conductor's rectangle spans the two components it joins, so
				it intersects theirs by construction, and "intersects" would
				call every wire on the folio repainted, always.

				What the fixed rectangles cost is that the component that
				moved cannot be told apart from the noise. Nothing here
				claims anything about it: the question asked is about the
				wire.
			*/
			bool covers(QGraphicsItem *item)
			{
				if (!item) {
					return false;
				}
				drain();

				// Half a unit of slack, because a region is built by mapping
				// the item rectangle through its scene transform and the
				// two are compared as floating point.
				const QRectF wanted = item->sceneBoundingRect();
				for (const QRectF &region : std::as_const(m_regions))
				{
					if (region.adjusted(-.5, -.5, .5, .5).contains(wanted)) {
						return true;
					}
				}
				return false;
			}

			/// How many regions were asked for; a count, for the quiet case.
			int count()
			{
				drain();
				return m_regions.count();
			}

		private:
			/*
				QGraphicsScene does not emit changed() on the spot. An
				update() marks the item and posts _q_processDirtyItems; that
				one, in turn, posts _q_emitUpdated. Two hops through the
				queue, so the queue is run more than once - three times, for
				the margin, since an empty queue costs nothing.
			*/
			void drain()
			{
				for (int pass = 0 ; pass < 3 ; ++pass) {
					QCoreApplication::processEvents();
				}
			}

			QObject m_context;
			QList<QRectF> m_regions;
	};

	/// The strip of the sheet holding the wire between @a first and @a second
	/// and nothing else: the gap between the two boxes, a few units tall.
	QRectF wireBand(Element *first, Element *second)
	{
		const QRectF a = first->sceneBoundingRect();
		const QRectF b = second->sceneBoundingRect();
		const qreal left = qMin(a.right(), b.right()) + 5.;
		const qreal right = qMax(a.left(), b.left()) - 5.;
		const qreal middle = a.center().y();
		return QRectF(QPointF(left, middle - 4.),
			      QPointF(right, middle + 4.));
	}

	/// Write @a path onto @a element the way the location manager does.
	void assignLocation(QETProject *project, Element *element,
			    const QString &path)
	{
		REQUIRE(project != nullptr);
		REQUIRE(element != nullptr);
		auto *command = new AssignLocationCommand(
					QList<Element *>{element}, path);
		project->undoStack()->push(command);
	}
}

/*
	The whole of the defect, in the order it was found.

	Sections rather than separate cases: every one of them starts from the
	same four boxes and two wires, and Catch2 rebuilds that fixture for each
	section, so no section can be made to pass by what another one did.
*/
TEST_CASE("T32 - a wire is repainted when the location of one of its ends moves",
	  "[location][conductor][dashedwire]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("dashedwire.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 1);

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->dashExternalWires());

	Element *k1 = box(sheet, QStringLiteral("K1"));
	Element *k2 = box(sheet, QStringLiteral("K2"));
	Element *k3 = box(sheet, QStringLiteral("K3"));
	Element *k4 = box(sheet, QStringLiteral("K4"));
	REQUIRE(k1 != nullptr);
	REQUIRE(k2 != nullptr);
	REQUIRE(k3 != nullptr);
	REQUIRE(k4 != nullptr);

	Conductor *wire = wireBetween(sheet, k1, k2);
	Conductor *witness = wireBetween(sheet, k3, k4);
	REQUIRE(wire != nullptr);
	REQUIRE(witness != nullptr);

	// The starting state: the two ends of the wire under test stand apart,
	// the two ends of the witness stand together.
	REQUIRE(k1->elementInformations()
		.value(QETInformation::ELMT_LOCATION_PATH).toString() == panel_a);
	REQUIRE(k2->elementInformations()
		.value(QETInformation::ELMT_LOCATION_PATH).toString() == panel_b);

	SECTION("the instrument sees a repaint asked for by hand")
	{
		RepaintWatch watch(sheet);
		watch.clear();

		wire->update();

		CHECK(watch.covers(wire));
		// The other half of the calibration: asking for one wire must not
		// answer for the other. Without this, every check below would pass
		// on an instrument that says yes to everything.
		CHECK_FALSE(watch.covers(witness));
	}

	SECTION("the instrument sees nothing on a folio nobody touched")
	{
		RepaintWatch watch(sheet);
		watch.clear();

		CHECK(watch.count() == 0);
		CHECK_FALSE(watch.covers(wire));
	}

	SECTION("the two ends come together, and the wire is asked to redraw")
	{
		RepaintWatch watch(sheet);
		watch.clear();

		assignLocation(bench.project(), k2, panel_a);

		CHECK(k2->elementInformations()
		      .value(QETInformation::ELMT_LOCATION_PATH).toString() == panel_a);
		CHECK(watch.covers(wire));
	}

	SECTION("the two ends come apart, and the wire is asked to redraw")
	{
		// Put them together first, out of the watch, so that what is
		// measured below is only the second move.
		assignLocation(bench.project(), k2, panel_a);

		RepaintWatch watch(sheet);
		watch.clear();

		assignLocation(bench.project(), k2, panel_b);

		CHECK(watch.covers(wire));
	}

	SECTION("an undone move asks for the wire again")
	{
		assignLocation(bench.project(), k2, panel_a);

		RepaintWatch watch(sheet);
		watch.clear();

		bench.project()->undoStack()->undo();

		CHECK(k2->elementInformations()
		      .value(QETInformation::ELMT_LOCATION_PATH).toString() == panel_b);
		CHECK(watch.covers(wire));
	}

	SECTION("the far wire is left alone when a location moves")
	{
		RepaintWatch watch(sheet);
		watch.clear();

		assignLocation(bench.project(), k2, panel_a);

		CHECK(watch.covers(wire));
		CHECK_FALSE(watch.covers(witness));
	}

	/*
		The choice made in Conductor::elementInformationChanged(), stated as
		a test so that changing it has to be deliberate: a change of
		information that is not the location asks for nothing. Renaming a
		component or stamping a manufacturer's code onto a selection runs
		through the same signal, in a loop over the whole folio, and none of
		it changes where the wire's ends stand.

		Only the wire is checked, not the folio: the component itself is of
		course repainted, since the label it draws is the one that changed.
	*/
	SECTION("a change that is not the location asks for nothing")
	{
		RepaintWatch watch(sheet);
		watch.clear();

		const DiagramContext old_info = k2->elementInformations();
		DiagramContext new_info = old_info;
		new_info.addValue(QETInformation::ELMT_LABEL,
				  QStringLiteral("K22"));
		bench.project()->undoStack()->push(
			new ChangeElementInformationCommand(k2, old_info, new_info));

		REQUIRE(k2->elementInformations()
			.value(QETInformation::ELMT_LABEL).toString()
			== QStringLiteral("K22"));
		CHECK_FALSE(watch.covers(wire));
	}

	/*
		The drawing itself, once: the ink over the gap between the two boxes,
		which holds the wire and nothing else. Counted and not compared shape
		by shape, for the reason given in uibench.h - and here a count is all
		that is wanted, because a dashed stroke is the same path with holes
		in it.
	*/
	SECTION("the stroke really is dashed while the ends stand apart")
	{
		const QRectF band = wireBand(k1, k2);

		const UiBench::Rendering apart(sheet);
		REQUIRE_FALSE(apart.isNull());
		const int ink_apart = apart.ink(band);

		assignLocation(bench.project(), k2, panel_a);

		const UiBench::Rendering together(sheet);
		REQUIRE_FALSE(together.isNull());
		const int ink_together = together.ink(band);

		CHECK(ink_apart > 0);
		CHECK(ink_apart < ink_together);
	}

	/*
		The non regression, and it is the one that matters most: the stroke
		the draughtsman chose is read at paint time and stored nowhere, so
		nothing above may have written a style onto the wire. Checked after
		the location has been moved both ways, which is the sequence that
		would have overwritten it.
	*/
	SECTION("the style set by hand survives the whole sequence")
	{
		// Set here and not in the fixture, the way the properties dialogue
		// sets it: the wire is born solid, the draughtsman makes it
		// dash-dot, and from then on nothing but him may change it.
		ConductorProperties chosen = wire->properties();
		chosen.style = Qt::DashDotLine;
		wire->setProperties(chosen);
		REQUIRE(wire->properties().style == Qt::DashDotLine);

		assignLocation(bench.project(), k2, panel_a);
		CHECK(wire->properties().style == Qt::DashDotLine);

		assignLocation(bench.project(), k2, panel_b);
		CHECK(wire->properties().style == Qt::DashDotLine);

		bench.project()->undoStack()->undo();
		CHECK(wire->properties().style == Qt::DashDotLine);

		// Where the undo left the ends, said out loud: back on the first
		// location, which is where the other end stands - so the folio
		// draws this wire whole again while what it stores is still the
		// dash-dot the draughtsman chose.
		CHECK(k2->elementInformations()
		      .value(QETInformation::ELMT_LOCATION_PATH).toString() == panel_a);
	}
}
