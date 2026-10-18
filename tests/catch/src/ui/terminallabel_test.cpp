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

#include "../../../../sources/TerminalStrip/UndoCommand/sortterminalstripcommand.h"
#include "../../../../sources/TerminalStrip/physicalterminal.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/autoNum/iecstructure.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/label/componentlabelcollector.h"
#include "../../../../sources/label/terminallabelcollector.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/titleblockproperties.h"
#include "../../../../sources/undocommand/assignlocationcommand.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QPair>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QVector>

/*
	The terminal labels, collected from the live strips of a real project.

	A terminal label cannot come from the project data base the way a
	component label does - the base holds no strip, no position and no
	level - so the collector walks the objects, and the only way to check it
	is with a project open and strips built in it. That is why this file is
	in C_uitests, and why it builds its own folios rather than reading an
	example: no example of the collection ships a <terminal_strip>, and the
	order a case needs - one a sort would change - has to be chosen.

	What every case here holds against the collector is something it could
	get wrong while still giving the right number of labels. The order of
	the rail first: the strip below is mounted 2, 10, 1, which is neither the
	order the folio draws them in (1, 2, 10) nor any order a sort would give
	(1, 2, 10 by the editor's natural sort, 1, 10, 2 by plain text) - so a
	collector that sorts, by any rule, or that walks the drawing instead of
	the rail, fails the first case and not only a corner of it.

	Labelled T21 and not CU-21.x. CU-21.12 is the closest, and it ends at an
	exported file held against the rail, with the position column beside
	each label; no writer exists yet, so what is proved here is what that
	file will be written from. CU-21.3 and CU-21.15 end the same way.
*/

namespace
{
		//The folio number each sheet carries in its title block. Not 1 and
		//2 on purpose: those are the positions of the sheets in the project,
		//and an entry that took one for the other would pass if they agreed.
	const char *const sheet_one_folio = "11";
	const char *const sheet_two_folio = "12";

	/**
		One terminal symbol of the bench.

		@param index : unique over the whole project. Its docking points are
		2 * index (east) and 2 * index + 1 (west), and its uuid ends in it.
		@param x : where it sits on its folio
		@param label : the tag it carries
		@param information : more <elementInformation/> to write after the
		label, as XML

		The docking coordinates are written as the arithmetic, as
		terminalstripcable_test.cpp does and for the reason it gives: the
		coordinates of an instance are offset by Terminal::terminalSize, and
		the bench should not need editing the day that size changes.
	*/
	QString terminalXml(int index, int x, const QString &label,
			    const QString &information = QString())
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		return QStringLiteral(
			       "<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/terminal.elmt\""
			       " uuid=\"{c0ffee21-0002-4000-8000-0000000000%2}\">"
			       "<terminals>"
			       "<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
			       "<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
			       "</terminals>"
			       "<inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%7"
			       "</elementInformation>"
			       "%8"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(x)
			.arg(index, 2, 10, QLatin1Char('0'))
			.arg(east_dock)
			.arg(index * 2)
			.arg(west_dock)
			.arg(index * 2 + 1)
			.arg(label, information);
	}

	/// One more piece of information of a symbol, as XML.
	QString informationXml(const QString &key, const QString &value)
	{
		return QStringLiteral("<elementInformation show=\"0\" name=\"%1\">%2"
				      "</elementInformation>")
			.arg(key, value);
	}

	/**
		A project of two folios and six terminal symbols.

		Sheet one draws 1, 2 and 10, in that order from left to right. 2
		and 10 share one wire, W7, so that each of them has something
		docked; 1 has no wire at all, and instead the cable it belongs to
		and the wire of that cable, as the strip editor stores them - and
		it is kept out of the parts list. Sheet two draws 3, 4 and 5, with
		nothing docked and nothing more said.

		The file carries no strip: every case mounts the rail it needs, so
		that the order of the rail is the case's own choice and is stated
		where it is checked.
	*/
	QString fixtureXml()
	{
		const QString one_information =
				informationXml(RealTerminal::cableInfoKey(),
					       QStringLiteral("W100"))
				+ informationXml(RealTerminal::cableWireInfoKey(),
						 QStringLiteral("BK"))
				+ informationXml(QStringLiteral("exclude_from_bom"),
						 QStringLiteral("true"));

		const QString sheet_one =
				terminalXml(0, 240, QStringLiteral("1"), one_information)
				+ terminalXml(1, 300, QStringLiteral("2"))
				+ terminalXml(2, 360, QStringLiteral("10"));

			//From the east side of 2 (id 2) to the west side of 10 (id 5).
		const QString sheet_one_wires = QStringLiteral(
				"<conductor terminal1=\"2\" terminal2=\"5\""
				" num=\"W7\" displaytext=\"1\" type=\"multi\""
				" condsize=\"1\"/>");

		const QString sheet_two =
				terminalXml(3, 240, QStringLiteral("3"))
				+ terminalXml(4, 300, QStringLiteral("4"))
				+ terminalXml(5, 360, QStringLiteral("5"));

		const QString diagram = QStringLiteral(
				"<diagram title=\"%1\" order=\"%2\" height=\"600\""
				" folio=\"%3\""
				" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
				" displaycols=\"true\" displayrows=\"true\">"
				"<elements>%4</elements>"
				"<inputs/>"
				"<conductors>%5</conductors>"
				"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"terminal.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"terminal\">"
			       "<names><name lang=\"en\">Terminal</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>")
		       + diagram.arg(QStringLiteral("Sheet one"), QStringLiteral("1"),
				     QString::fromLatin1(sheet_one_folio), sheet_one,
				     sheet_one_wires)
		       + diagram.arg(QStringLiteral("Sheet two"), QStringLiteral("2"),
				     QString::fromLatin1(sheet_two_folio), sheet_two,
				     QString())
		       + QStringLiteral("</project>");
	}

	/// The terminal symbol of @a diagram tagged @a label, nullptr when none.
	Element *terminalLabelled(Diagram *diagram, const QString &label)
	{
		if (!diagram) {
			return nullptr;
		}

		const auto elements = diagram->elements();
		for (Element *element : elements)
		{
			if (element->elementData().m_type == ElementData::Terminal
			    && element->actualLabel() == label) {
				return element;
			}
		}
		return nullptr;
	}

	/// The real terminal of the terminal symbol @a element.
	QSharedPointer<RealTerminal> realOf(Element *element)
	{
		if (!element) {
			return QSharedPointer<RealTerminal>();
		}
		return static_cast<TerminalElement *>(element)->realTerminal();
	}

	/**
		A new strip named @a name, with the terminals of @a diagram tagged
		@a labels mounted one after the other, in that order.

		Null when one of them is not found or not taken: the cases REQUIRE
		it, so a bench that could not mount its rail stops there instead of
		measuring a collector against a rail it does not have.
	*/
	TerminalStrip *mountStrip(QETProject *project, const QString &name,
				  const QList<QPair<Diagram *, QString>> &terminals)
	{
		TerminalStrip *strip =
				project->newTerminalStrip(QString(), QString(), name);
		if (!strip) {
			return nullptr;
		}

		for (const auto &terminal : terminals)
		{
			Element *element = terminalLabelled(terminal.first,
							    terminal.second);
			if (!element || !strip->addTerminal(element)) {
				return nullptr;
			}
		}
		return strip;
	}

	/// The tags of the terminals of @a strip, in the order of its rail.
	QStringList labelsOnRail(TerminalStrip *strip)
	{
		QStringList labels;
		const auto real_terminals = strip->realTerminals();
		for (const auto &real_terminal : real_terminals) {
			labels << real_terminal->label();
		}
		return labels;
	}

	/// The tags of a run, **in the order of the run**: here the order is the case.
	QStringList primaryTexts(const LabelEntryList &entries)
	{
		QStringList texts;
		for (const LabelEntry &entry : entries) {
			texts << entry.primary_text;
		}
		return texts;
	}

	QVector<int> stripPositions(const LabelEntryList &entries)
	{
		QVector<int> positions;
		for (const LabelEntry &entry : entries) {
			positions << entry.strip_position;
		}
		return positions;
	}

	QVector<int> levels(const LabelEntryList &entries)
	{
		QVector<int> list;
		for (const LabelEntry &entry : entries) {
			list << entry.level;
		}
		return list;
	}

	/// The entry tagged @a text, or a default one when the run has none.
	LabelEntry entryTagged(const LabelEntryList &entries, const QString &text)
	{
		for (const LabelEntry &entry : entries) {
			if (entry.primary_text == text) {
				return entry;
			}
		}
		return LabelEntry();
	}

	/// Give a sheet a title block, as the title block dialog would.
	void setTitleBlock(Diagram *sheet, const QString &revision,
			   const QString &plant = QString(),
			   const QString &location = QString())
	{
		TitleBlockProperties properties =
				sheet->border_and_titleblock.exportTitleBlock();
		properties.indexrev = revision;
		properties.plant = plant;
		properties.locmach = location;
		sheet->border_and_titleblock.importTitleBlock(properties);
	}
}

TEST_CASE("T21 — the terminals come out in the order of the rail, and nothing sorts them",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-labels.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	TerminalStrip *strip = mountStrip(
			bench.project(), QStringLiteral("X1"),
			{{sheet, QStringLiteral("2")},
			 {sheet, QStringLiteral("10")},
			 {sheet, QStringLiteral("1")}});
	REQUIRE(strip != nullptr);

		//The rail, as the strip itself holds it: what the run is compared
		//with, measured and not assumed.
	REQUIRE(labelsOnRail(strip)
		== QStringList({QStringLiteral("2"), QStringLiteral("10"),
				QStringLiteral("1")}));

	TerminalLabelCollector collector(bench.project());

	SECTION("the run is the rail, with the position the editor shows beside each label")
	{
		const LabelEntryList entries = collector.collect();
		INFO("collector said: " << collector.error().toStdString());
		REQUIRE(collector.error().isEmpty());

			//The case: neither the drawing (1, 2, 10) nor a sort.
		CHECK(primaryTexts(entries)
		      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
				      QStringLiteral("1")}));
		CHECK(stripPositions(entries) == QVector<int>({0, 1, 2}));

			//And the position is the very number of the editor's Position
			//column, which is PhysicalTerminal::pos(): the two are meant
			//to be held side by side, so they are compared here one by
			//one rather than trusted to agree.
		REQUIRE(entries.count() == strip->physicalTerminalCount());
		for (int i = 0 ; i < entries.count() ; ++i)
		{
			INFO("entry " << i);
			CHECK(entries.at(i).strip_position
			      == strip->physicalTerminal(i)->pos());
		}

		for (const LabelEntry &entry : entries)
		{
			INFO("label " << entry.primary_text.toStdString());
			CHECK(entry.source == LabelSource::Terminal);
			CHECK(entry.strip == QStringLiteral("X1"));
			CHECK(entry.level == 0);
		}
	}

	SECTION("sorted by the user, the run follows the new order, and each position follows its label")
	{
			//What the Sort button of the strip editor pushes.
		bench->undoStack()->push(new SortTerminalStripCommand(strip));

			//The precondition: the command did reorder the rail. Without
			//it, the check below would pass for a collector that ignored
			//the strip altogether and printed the drawing.
		REQUIRE(labelsOnRail(strip)
			== QStringList({QStringLiteral("1"), QStringLiteral("2"),
					QStringLiteral("10")}));

		const LabelEntryList entries = collector.collect();
		REQUIRE(collector.error().isEmpty());

			//Not because the collector sorted: because the user did. The
			//section above is what tells the two apart.
		CHECK(primaryTexts(entries)
		      == QStringList({QStringLiteral("1"), QStringLiteral("2"),
				      QStringLiteral("10")}));
		CHECK(stripPositions(entries) == QVector<int>({0, 1, 2}));

			//10 was at 1 on the rail and is at 2 now: the position travels
			//with the label, so the bench sees the move before the rail does.
		CHECK(entryTagged(entries, QStringLiteral("10")).strip_position == 2);
	}

	SECTION("undone, the run is the rail as it was mounted")
	{
		bench->undoStack()->push(new SortTerminalStripCommand(strip));
		bench->undoStack()->undo();

		REQUIRE(labelsOnRail(strip)
			== QStringList({QStringLiteral("2"), QStringLiteral("10"),
					QStringLiteral("1")}));

		const LabelEntryList entries = collector.collect();
		REQUIRE(collector.error().isEmpty());

		CHECK(primaryTexts(entries)
		      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
				      QStringLiteral("1")}));
		CHECK(entryTagged(entries, QStringLiteral("10")).strip_position == 1);
	}
}

TEST_CASE("T21 — a terminal of several levels gives one label per level, at one position",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-levels.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	TerminalStrip *strip = mountStrip(
			bench.project(), QStringLiteral("X1"),
			{{sheet, QStringLiteral("2")},
			 {sheet, QStringLiteral("10")},
			 {sheet, QStringLiteral("1")}});
	REQUIRE(strip != nullptr);

		//1 becomes the second level of the terminal 10 is on, as grouping
		//in the strip editor does.
	const auto ten = realOf(terminalLabelled(sheet, QStringLiteral("10")));
	const auto one = realOf(terminalLabelled(sheet, QStringLiteral("1")));
	REQUIRE_FALSE(ten.isNull());
	REQUIRE_FALSE(one.isNull());
	REQUIRE(strip->groupTerminals(ten->physicalTerminal(), {one}));
	REQUIRE(strip->physicalTerminalCount() == 2);

	TerminalLabelCollector collector(bench.project());
	const LabelEntryList entries = collector.collect();
	REQUIRE(collector.error().isEmpty());

		//Three labels for two places on the rail: one per level, because
		//each level is a terminal of its own with a tag of its own. The
		//two levels share the position of their terminal, back one first.
	CHECK(primaryTexts(entries)
	      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
			      QStringLiteral("1")}));
	CHECK(stripPositions(entries) == QVector<int>({0, 1, 1}));
	CHECK(levels(entries) == QVector<int>({0, 0, 1}));
}

TEST_CASE("T21 — every strip of the project, and only the terminals on a rail",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-strips.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet_one = bench.diagram(0);
	Diagram *sheet_two = bench.diagram(1);
	REQUIRE(sheet_one != nullptr);
	REQUIRE(sheet_two != nullptr);

	TerminalStrip *x1 = mountStrip(
			bench.project(), QStringLiteral("X1"),
			{{sheet_one, QStringLiteral("2")},
			 {sheet_one, QStringLiteral("10")},
			 {sheet_one, QStringLiteral("1")}});
	TerminalStrip *x2 = mountStrip(
			bench.project(), QStringLiteral("X2"),
			{{sheet_two, QStringLiteral("4")},
			 {sheet_two, QStringLiteral("3")}});
	REQUIRE(x1 != nullptr);
	REQUIRE(x2 != nullptr);
		//5 is drawn and mounted nowhere.
	REQUIRE(terminalLabelled(sheet_two, QStringLiteral("5")) != nullptr);

	TerminalLabelCollector collector(bench.project());
	const LabelEntryList entries = collector.collect();
	REQUIRE(collector.error().isEmpty());

		//Strip after strip, each in the order of its own rail, and each
		//counted from 0 again: a position is a place on one rail.
	CHECK(primaryTexts(entries)
	      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
			      QStringLiteral("1"), QStringLiteral("4"),
			      QStringLiteral("3")}));
	CHECK(stripPositions(entries) == QVector<int>({0, 1, 2, 0, 1}));

	QStringList strips;
	for (const LabelEntry &entry : entries) {
		strips << entry.strip;
	}
	CHECK(strips == QStringList({QStringLiteral("X1"), QStringLiteral("X1"),
				     QStringLiteral("X1"), QStringLiteral("X2"),
				     QStringLiteral("X2")}));

		//The count of the run is the count of the rails, and 5 is not in
		//it: a terminal no strip holds is on no rail and gets no label from
		//here. A collector walking the drawing would print six.
	CHECK(entries.count()
	      == x1->realTerminals().count() + x2->realTerminals().count());
	CHECK_FALSE(primaryTexts(entries).contains(QStringLiteral("5")));

	SECTION("one strip alone gives that strip and nothing else")
	{
		const LabelEntryList x2_only = TerminalLabelCollector::collectStrip(x2);
		CHECK(primaryTexts(x2_only)
		      == QStringList({QStringLiteral("4"), QStringLiteral("3")}));
		CHECK(stripPositions(x2_only) == QVector<int>({0, 1}));
	}
}

TEST_CASE("T21 — each terminal carries the folio and the revision of its own sheet",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-revisions.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet_one = bench.diagram(0);
	Diagram *sheet_two = bench.diagram(1);
	REQUIRE(sheet_one != nullptr);
	REQUIRE(sheet_two != nullptr);

	setTitleBlock(sheet_one, QStringLiteral("A"));
	setTitleBlock(sheet_two, QStringLiteral("B"));

		//The folio numbers of the two sheets, as they draw them. Required
		//to differ: the check below could not tell one sheet from the other
		//otherwise.
	const QString folio_one = sheet_one->border_and_titleblock.finalfolio();
	const QString folio_two = sheet_two->border_and_titleblock.finalfolio();
	REQUIRE_FALSE(folio_one.isEmpty());
	REQUIRE(folio_one != folio_two);

	const int position_one = bench->folioIndex(sheet_one) + 1;
	const int position_two = bench->folioIndex(sheet_two) + 1;
	REQUIRE(position_one != position_two);

		//One strip crossing the two sheets: the case a single revision for
		//the strip would get wrong.
	TerminalStrip *strip = mountStrip(
			bench.project(), QStringLiteral("X1"),
			{{sheet_one, QStringLiteral("1")},
			 {sheet_two, QStringLiteral("3")}});
	REQUIRE(strip != nullptr);

	TerminalLabelCollector collector(bench.project());
	const LabelEntryList entries = collector.collect();
	REQUIRE(collector.error().isEmpty());
	REQUIRE(entries.count() == 2);

	const LabelEntry one = entryTagged(entries, QStringLiteral("1"));
	const LabelEntry three = entryTagged(entries, QStringLiteral("3"));

	CHECK(one.folio == folio_one);
	CHECK(one.folio_position == position_one);
	CHECK(one.revision == QStringLiteral("A"));

	CHECK(three.folio == folio_two);
	CHECK(three.folio_position == position_two);
	CHECK(three.revision == QStringLiteral("B"));
}

TEST_CASE("T21 — a terminal label carries what the strip editor shows beside it",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-texts.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet_one = bench.diagram(0);
	Diagram *sheet_two = bench.diagram(1);
	REQUIRE(sheet_one != nullptr);
	REQUIRE(sheet_two != nullptr);
	REQUIRE(sheet_one->conductors().size() == 1);

	TerminalStrip *strip = mountStrip(
			bench.project(), QStringLiteral("X1"),
			{{sheet_one, QStringLiteral("2")},
			 {sheet_one, QStringLiteral("10")},
			 {sheet_one, QStringLiteral("1")},
			 {sheet_two, QStringLiteral("3")}});
	REQUIRE(strip != nullptr);

	TerminalLabelCollector collector(bench.project());
	const LabelEntryList entries = collector.collect();
	REQUIRE(collector.error().isEmpty());
	REQUIRE(entries.count() == 4);

	SECTION("the wire docked, on both ends of it")
	{
		CHECK(entryTagged(entries, QStringLiteral("2")).secondary_texts
		      == QStringList{QStringLiteral("W7")});
		CHECK(entryTagged(entries, QStringLiteral("10")).secondary_texts
		      == QStringList{QStringLiteral("W7")});
	}

	SECTION("the cable and its wire, with no blank line for the wire that is not there")
	{
			//1 has no wire docked. Its first line is the cable, not an
			//empty line standing for the missing wire number.
		CHECK(entryTagged(entries, QStringLiteral("1")).secondary_texts
		      == QStringList({QStringLiteral("W100"), QStringLiteral("BK")}));
	}

	SECTION("a terminal with nothing to add says nothing more")
	{
			//The legitimate empty, and the negative control of the two
			//sections above: filling every entry with the same constant
			//would pass them and fail this.
		CHECK(entryTagged(entries, QStringLiteral("3")).secondary_texts
		      .isEmpty());
	}

	SECTION("kept out of the parts list, and labelled all the same")
	{
			//Carried, not obeyed: 1 is on the rail whatever the purchase
			//list says, so it is in the run - the count above has it -
			//and says it was kept out.
		CHECK(entryTagged(entries, QStringLiteral("1")).excluded_from_bom);
		CHECK_FALSE(entryTagged(entries, QStringLiteral("2")).excluded_from_bom);
		CHECK_FALSE(entryTagged(entries, QStringLiteral("10")).excluded_from_bom);
		CHECK_FALSE(entryTagged(entries, QStringLiteral("3")).excluded_from_bom);
	}
}

TEST_CASE("T21 — with the structure on, a terminal label carries the tag the sheet draws",
	  "[t21][label][terminalstrip][iec]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-iec.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

		//The folio hands down the function and the location.
	setTitleBlock(sheet, QString(), QStringLiteral("CT1"),
		      QStringLiteral("QCM"));

	TerminalStrip *strip = mountStrip(
			bench.project(), QStringLiteral("X1"),
			{{sheet, QStringLiteral("2")},
			 {sheet, QStringLiteral("10")},
			 {sheet, QStringLiteral("1")}});
	REQUIRE(strip != nullptr);

	const auto real_terminals = strip->realTerminals();
	REQUIRE(real_terminals.count() == 3);

	TerminalLabelCollector collector(bench.project());

	SECTION("off, the run carries the stored tags, as the strip editor lists them")
	{
		REQUIRE(bench->iecSettings().enabled == false);

		const LabelEntryList entries = collector.collect();
		REQUIRE(collector.error().isEmpty());
		REQUIRE(entries.count() == real_terminals.count());

		for (int i = 0 ; i < entries.count() ; ++i)
		{
			INFO("entry " << i);
			CHECK(entries.at(i).primary_text == real_terminals.at(i)->label());
		}
	}

	SECTION("on, the run carries the tag of the norm, not the stored field")
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Full;
		bench->setIecSettings(settings);

		const LabelEntryList entries = collector.collect();
		REQUIRE(collector.error().isEmpty());
		REQUIRE(entries.count() == real_terminals.count());

		for (int i = 0 ; i < entries.count() ; ++i)
		{
			INFO("entry " << i);
			CHECK(entries.at(i).primary_text != real_terminals.at(i)->label());
		}

			//A bare number is a connection and not a product, so the full
			//form reads plant, location and the colon of the norm, with no
			//dash. The expectation is the one CU-21.7 measured for a
			//component tagged 7 on the same folio - =CT1+QCM:7 - since the
			//composition is the same function for both. 1 is the third
			//terminal of the rail.
		CHECK(entries.at(2).primary_text == QStringLiteral("=CT1+QCM:1"));
	}

	SECTION("the tape and the sheet say the same thing, in the three modes")
	{
			//The assertion that keeps this collector on the composition
			//the sheet draws with: Element::displayedLabel() of the very
			//symbol each entry was read from, in the order of the rail.
		IecStructureSettings settings;
		settings.enabled = true;

		for (IecTagDisplay display : {IecTagDisplay::Short,
					      IecTagDisplay::Context,
					      IecTagDisplay::Full})
		{
			settings.display = display;
			bench->setIecSettings(settings);

			const LabelEntryList entries = collector.collect();
			REQUIRE(collector.error().isEmpty());
			REQUIRE(entries.count() == real_terminals.count());

			for (int i = 0 ; i < entries.count() ; ++i)
			{
				INFO("display mode "
				     << IecStructureSettings::displayToString(display)
						.toStdString()
				     << ", entry " << i);
				Element *element = real_terminals.at(i)->element();
				REQUIRE(element != nullptr);
				CHECK(entries.at(i).primary_text
				      == element->displayedLabel());
			}
		}
	}
}

/*
	The location of a terminal, written the way the norm writes it.

	The rule is ComponentLabelQuery::writtenLocation(), checked answer by
	answer and setting by setting in tests/catch/src/componentlabel_test.cpp,
	where it passes whatever this collector does with it. What only a project
	can show is that the collector calls it, and calls it with the settings
	of the project its symbol is drawn in: the case reads the location with
	the structure off and on, and holds every terminal against the row the
	data base gives the same symbol as a component. A collector with a rule
	of its own - the stored path, the field with a `+` always - or with
	settings of its own comes out red here.

	Labelled T21: CU-21.14 ends at an exported file, and that is
	labelexport_test.cpp.
*/
TEST_CASE("T21 — a terminal label writes its location as the norm writes it, and as the component label does",
	  "[t21][label][terminalstrip][iec]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-location.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	Element *two = terminalLabelled(sheet, QStringLiteral("2"));
	Element *ten = terminalLabelled(sheet, QStringLiteral("10"));
	REQUIRE(two != nullptr);
	REQUIRE(ten != nullptr);

		//2 is given a place of the tree, by the command the location
		//manager pushes; 10 gets the free text field and nothing more; 1
		//gets nothing at all.
	const QString path = QStringLiteral("QCM1/PLACA");
	bench->undoStack()->push(new AssignLocationCommand(
			QList<Element *>({two}), path));
	REQUIRE(two->elementInformations()
		.value(IecStructure::locationPathKey()).toString() == path);

	DiagramContext information = ten->elementInformations();
	information.addValue(IecStructure::locationKey(), QStringLiteral("X9"));
	bench->undoStack()->push(new ChangeElementInformationCommand(
			ten, ten->elementInformations(), information));
	REQUIRE(ten->elementInformations()
		.value(IecStructure::locationKey()).toString()
		== QStringLiteral("X9"));

	REQUIRE(mountStrip(bench.project(), QStringLiteral("X1"),
			   {{sheet, QStringLiteral("2")},
			    {sheet, QStringLiteral("10")},
			    {sheet, QStringLiteral("1")}})
		!= nullptr);

	TerminalLabelCollector collector(bench.project());

	SECTION("off: the place as the norm writes it, the field as typed, nothing as nothing")
	{
		REQUIRE(bench->iecSettings().enabled == false);

		const LabelEntryList entries = collector.collect();
		REQUIRE(collector.error().isEmpty());
		REQUIRE(primaryTexts(entries)
			== QStringList({QStringLiteral("2"), QStringLiteral("10"),
					QStringLiteral("1")}));

			//Not the QCM1/PLACA the symbol stores.
		CHECK(entries.at(0).location == QStringLiteral("+QCM1+PLACA"));

			//As typed, with no `+`: this project never said the field is
			//a place, and the field is where projects write the strip the
			//wiring lands on.
		CHECK(entries.at(1).location == QStringLiteral("X9"));

			//And nothing is nothing - not a lone `+`.
		CHECK(entries.at(2).location.isEmpty());
	}

	SECTION("on, with the field declared a place: the field behind the + of the norm")
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Full;
		settings.location_from_element = true;
		bench->setIecSettings(settings);

		const LabelEntryList entries = collector.collect();
		REQUIRE(collector.error().isEmpty());
		REQUIRE(entries.count() == 3);

		CHECK(entries.at(0).location == QStringLiteral("+QCM1+PLACA"));
		CHECK(entries.at(1).location == QStringLiteral("+X9"));
		CHECK(entries.at(2).location.isEmpty());

			//The very `+` the full tag of each terminal opens with: no
			//folio of this bench hands down a function or a place, so the
			//tag the sheet draws is the place, then the connection of the
			//norm - the number, after the colon.
		CHECK(entries.at(0).primary_text == QStringLiteral("+QCM1+PLACA:2"));
		CHECK(entries.at(1).primary_text == QStringLiteral("+X9:10"));
	}

	SECTION("a terminal and the component row of its symbol read the same location")
	{
		ComponentLabelCollector components(bench.project());

			//The short form leaves a terminal number as it is, so every
			//terminal can be found among the component rows by its tag in
			//both settings.
		for (bool declared : {false, true})
		{
			IecStructureSettings settings;
			settings.enabled = declared;
			settings.display = IecTagDisplay::Short;
			settings.location_from_element = declared;
			bench->setIecSettings(settings);

			const LabelEntryList terminals = collector.collect();
			REQUIRE(collector.error().isEmpty());
			const LabelEntryList rows = components.collect();
			INFO("component collector said: "
			     << components.error().toStdString());
			REQUIRE(components.error().isEmpty());
			REQUIRE(terminals.count() == 3);

			for (const LabelEntry &terminal : terminals)
			{
				const LabelEntry row =
						entryTagged(rows, terminal.primary_text);
				INFO("field declared a place: " << declared
				     << ", terminal " << terminal.primary_text.toStdString());
				REQUIRE(row.primary_text == terminal.primary_text);
				CHECK(row.location == terminal.location);
			}

				//And the agreement is not two empty strings: the field of
				//10 is read, and read the way the project says.
			CHECK(entryTagged(terminals, QStringLiteral("10")).location
			      == (declared ? QStringLiteral("+X9")
					   : QStringLiteral("X9")));
			CHECK(entryTagged(terminals, QStringLiteral("2")).location
			      == QStringLiteral("+QCM1+PLACA"));
		}
	}
}

TEST_CASE("T21 — the order of the rail survives saving and opening again",
	  "[t21][label][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("terminal-reopen.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(mountStrip(bench.project(), QStringLiteral("X1"),
			   {{sheet, QStringLiteral("2")},
			    {sheet, QStringLiteral("10")},
			    {sheet, QStringLiteral("1")}})
		!= nullptr);

		//Read back from the file: the strip is rebuilt from its XML, which
		//is the path an export without the window open will take.
	const bool reopened = bench.saveAndReopen();
	INFO("reopening said: " << bench.error().toStdString());
	REQUIRE(reopened);
	REQUIRE(bench->terminalStrip().count() == 1);

	TerminalLabelCollector collector(bench.project());
	const LabelEntryList entries = collector.collect();
	REQUIRE(collector.error().isEmpty());

	CHECK(primaryTexts(entries)
	      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
			      QStringLiteral("1")}));
	CHECK(stripPositions(entries) == QVector<int>({0, 1, 2}));
}

TEST_CASE("T21 — the terminal collector says so when there is no project",
	  "[t21][label][terminalstrip]")
{
	TerminalLabelCollector collector(nullptr);
	CHECK(collector.collect().isEmpty());
	CHECK_FALSE(collector.error().isEmpty());

	CHECK(TerminalLabelCollector::collectStrip(nullptr).isEmpty());
}
