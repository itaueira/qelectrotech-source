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
#include "../../../../sources/wirearrangement.h"

#include <catch2/catch.hpp>

#include <cmath>

/*
	The handles of a wire (T67): one on each segment but the first and the
	last, which only extend their terminal - and one on the first or the
	last too when it leaves its terminal to the side. The arrangement draws
	a wire from a terminal that points down to one that points to the side
	leaving the first at the height of its end, and that run could not be
	moved (asked by the Renan on 23/10/2026).

	The bench: a component at (100, 100) with a terminal pointing down, and
	one at (300, 200) with a terminal pointing left.
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
			"<rect x=\"-6\" y=\"-6\" width=\"12\" height=\"12\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), terminal);
	}

	QString instance(const char *def, int x, int y, const char *label, int n)
	{
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/%3\""
			" uuid=\"{67a00000-0000-4000-8000-00000000000%4}\">"
			"<terminals/><inputs/>"
			"<elementInformations>"
			"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
			"</elementInformations>"
			"<dynamic_texts/><texts_groups/>"
			"</element>")
			.arg(x).arg(y).arg(QLatin1String(def)).arg(n).arg(QLatin1String(label));
	}

	QString fixture()
	{
		const QString definitions =
			definition("drop.elmt", QStringLiteral("<terminal x=\"0\" y=\"14\" orientation=\"s\"/>"))
			+ definition("side.elmt", QStringLiteral("<terminal x=\"-14\" y=\"0\" orientation=\"w\"/>"));
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2%3</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definitions,
			     instance("drop.elmt", 100, 100, "K1", 1),
			     instance("side.elmt", 300, 200, "X1", 2));
	}

		/// No arranging of the wire the bench draws
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

	bool same(const QList<QPointF> &a, const QList<QPointF> &b)
	{
		if (a.size() != b.size()) return false;
		for (int i = 0 ; i < a.size() ; ++i) {
			if (std::abs(a.at(i).x() - b.at(i).x()) > 0.01 || std::abs(a.at(i).y() - b.at(i).y()) > 0.01) return false;
		}
		return true;
	}
}

TEST_CASE("T67 - A run that leaves its terminal to the side has a handle", "[T67][handles]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture());
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Element *k1 = byLabel(sheet, QStringLiteral("K1"));
	Element *x1 = byLabel(sheet, QStringLiteral("X1"));
	REQUIRE(k1);
	REQUIRE(x1);
	Terminal *down = k1->terminals().value(0);
	Terminal *left = x1->terminals().value(0);
	REQUIRE(down);
	REQUIRE(left);
	REQUIRE(down->orientation() == Qet::South);
	REQUIRE(left->orientation() == Qet::West);

	auto *wire = new Conductor(down, left);
	sheet->addItem(wire);
	const QPointF d1 = down->dockConductor();
	const QPointF d2 = left->dockConductor();
		//Out of the terminal pointing down to the side, at its end, as the
		//arrangement leaves it; down at 200; into the terminal pointing left
	const QList<QPointF> sideways{d1, QPointF(200, d1.y()), QPointF(200, d2.y()), d2};
	REQUIRE(wire->setPathPoints(sideways));
	REQUIRE(same(wire->scenePathPoints(), sideways));

		//The run has its handle, the segment into the terminal pointing left
		//none, as ever
	CHECK(wire->handlerSegments() == QList<int>{0, 1});
	REQUIRE(wire->handlerPoints().size() == 2);
	CHECK(same({wire->mapToScene(wire->handlerPoints().at(0))}, {(d1 + QPointF(200, d1.y())) / 2}));

	SECTION("dragged away from the terminal, the run gets the segment of it")
	{
		REQUIRE(wire->moveSegment(0, 0, 20));
		const QList<QPointF> moved{d1, d1 + QPointF(0, 20), QPointF(200, d1.y() + 20),
					   QPointF(200, d2.y()), d2};
		CHECK(same(wire->scenePathPoints(), moved));
			//The new segment of the terminal goes on from it: no handle
		CHECK(wire->handlerSegments() == QList<int>{1, 2});
	}
	SECTION("dragged into the terminal, it stays at its end")
	{
		REQUIRE(wire->moveSegment(0, 0, -20));
		CHECK(same(wire->scenePathPoints(), sideways));
	}
}
