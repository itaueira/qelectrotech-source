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
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/reportelement.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QString>
#include <algorithm>

/*
	The arrow of a folio reference says where the wire goes and says nothing
	about what is waiting there. A wire that simply continues, one core of a
	cable, a terminal block and a component are four different things to find
	on the next folio, and from this side they all looked the same: the label
	of the arrow carries the folio, the line and the column, and that is all.

	ReportElement::referenced() is what answers, and it is asked here
	directly. What it feeds is the tooltip of the arrow, and the tooltip was
	chosen over a drawn mark on purpose - the arrow is about ten units wide
	and its own drawing fills it, so a glyph put inside is a glyph to be
	squinted at. That choice is about legibility, and legibility is not
	settled by a number: it is for whoever opens the program.

	What a number does settle is the answer itself, and the cases below take
	the two ends of it: a reference nobody tied answers "nothing", and a real
	reference of a real project reaches across the folio and names what it
	found. The cable case is built by the case rather than looked for,
	because no example ships a cable - and it is the branch that decides
	before all the others, so leaving it unexercised would leave the order of
	the four answers unguarded.

	Labelled T31 and not a case of use: what the person sees is a tooltip
	under a mouse pointer, and no case of this bench has either.
*/

namespace
{
		/// Twelve folios and thirty-four folio references, every one of
		/// them tied to its pair.
	const char *report_example = "affuteuse_250h.qet";

	/**
		Every folio reference of @a project, in an order that does not move
		between runs: folios in project order, components sorted by uuid
		inside each.
	*/
	QList<ReportElement *> reports(QETProject *project)
	{
		QList<ReportElement *> found;

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
				if (auto *report = dynamic_cast<ReportElement *>(element)) {
					found.append(report);
				}
			}
		}

		return found;
	}

	/// The conductors docked to every terminal of @a element, in dock order.
	QList<Conductor *> conductorsOf(Element *element)
	{
		QList<Conductor *> found;

		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *terminal : terminals) {
			found.append(terminal->conductors());
		}

		return found;
	}

	/**
		A folio reference of @a project whose pair carries at least one
		conductor: the only shape of reference this file can say anything
		about, since a reference whose other side is not wired has nothing
		on the other side to name.
	*/
	ReportElement *wiredReport(QETProject *project)
	{
		const QList<ReportElement *> all = reports(project);
		for (ReportElement *report : all)
		{
			if (report->isFree()) {
				continue;
			}

			Element *other = report->linkedElements().first();
			if (other && !conductorsOf(other).isEmpty()) {
				return report;
			}
		}

		return nullptr;
	}
}

TEST_CASE("T31 — a seta de referência entre folhas sabe dizer o que há do outro lado",
	  "[navigation][report]")
{
	UiBench::Project bench(report_example);
	REQUIRE(bench.isOpen());

	const QList<ReportElement *> all = reports(bench.project());
	REQUIRE(all.count() >= 2);

	SECTION("uma seta ligada nunca responde que não está ligada")
	{
		int linked = 0;

		for (ReportElement *report : all)
		{
			if (report->isFree()) {
				CHECK(report->referenced() == ReportElement::NotLinked);
				continue;
			}

			++linked;
			CHECK(report->referenced() != ReportElement::NotLinked);
		}

			//The example ships thirty-four of them, all tied: a run that
			//finds none would be measuring an empty list and saying yes.
		CHECK(linked > 0);
	}

	SECTION("desligada, ela volta a não ter o que dizer")
	{
		ReportElement *report = wiredReport(bench.project());
		REQUIRE(report != nullptr);
		REQUIRE(report->referenced() != ReportElement::NotLinked);

		report->unlinkAllElements();

		QString name = QStringLiteral("not emptied");
		CHECK(report->referenced(&name) == ReportElement::NotLinked);
		CHECK(name.isEmpty());
		CHECK(report->referenceToolTip() == QStringLiteral("Report non relié"));
	}

	SECTION("do outro lado do desenho, ela nomeia o que achou")
	{
		ReportElement *report = wiredReport(bench.project());
		REQUIRE(report != nullptr);

		Element *other = report->linkedElements().first();
		REQUIRE(other != nullptr);

		QString name;
		const ReportElement::Referenced answer = report->referenced(&name);

			/* Whatever the drawing of the example puts there, the two
			 * halves of the answer have to agree: a name without a thing
			 * named, or a thing named without a name, is the shape of a
			 * walk that stopped halfway. */
		if (answer == ReportElement::TerminalBlock
				|| answer == ReportElement::Component)
		{
			CHECK(name.isEmpty() == false);
			CHECK(report->referenceToolTip().startsWith(
					      QStringLiteral("Report vers")));
		}
		else
		{
			CHECK(answer == ReportElement::Wire);
			CHECK(name.isEmpty());
		}
	}

	SECTION("um fio de cabo do outro lado é o que ela diz primeiro")
	{
		ReportElement *report = wiredReport(bench.project());
		REQUIRE(report != nullptr);

		Element *other = report->linkedElements().first();
		REQUIRE(other != nullptr);

		const QList<Conductor *> conductors = conductorsOf(other);
		REQUIRE(conductors.count() >= 1);

			/* Built here and not looked for: no example of the collection
			 * ships a cable, and the cable is the answer that is decided
			 * before the other three. Left unexercised, the order of the
			 * four could be inverted without a single case going red. */
		ConductorProperties properties = conductors.first()->properties();
		properties.m_cable = QStringLiteral("W12");
		conductors.first()->setProperties(properties);

		QString name;
		CHECK(report->referenced(&name) == ReportElement::Cable);
		CHECK(name == QStringLiteral("W12"));
		CHECK(report->referenceToolTip()
		      == QStringLiteral("Report vers le câble W12"));
	}
}
