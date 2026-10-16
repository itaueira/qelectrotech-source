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
#include "../qt_catch_tostring.h"
#include "uibench.h"

#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/ElementsCollection/embeddedelementrefresh.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/elementtextitemgroup.h"
#include "../../../../sources/qetgraphicsitem/reporttags.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QHash>
#include <QLineF>
#include <QUndoStack>
#include <QUuid>

#include <algorithm>

/*
	T62 - a symbol saved in the symbol editor reaches the folio and the
	file. The folio reports follow the texts of their symbol like any other
	element - where a text sits and how it is drawn, while the reference it
	shows stays the one the formula of the project gives - and a symbol
	saved into a project's copy changes the project.

	affuteuse_250h.qet has folio reports linked in pairs, whose texts carry
	no uuid of their symbol: they are linked by kind, content and place,
	which is also how the tag of T59 could be taken for a text of the
	symbol, and must not be.
*/

namespace
{
	const QString example = QStringLiteral("affuteuse_250h.qet");

	/// The path, in the project's collection, of the folio report placed
	/// the most often, and the reports placed from it
	QPair<QString, QList<Element *>> busiestReport(QETProject *project)
	{
		QHash<QString, QList<Element *>> placed;
		for (Element *report : ReportTags::reports(project)) {
			placed[report->location().collectionPath(false)] << report;
		}
			//The most placed, and of two placed as often the one whose path
			//sorts first: QHash has no order, and a case must pick the same
			//symbol every time
		QStringList paths = placed.keys();
		std::sort(paths.begin(), paths.end());
		QString best;
		for (const QString &path : paths) {
			if (best.isEmpty() || placed.value(path).size() > placed.value(best).size()) {
				best = path;
			}
		}
		return qMakePair(best, placed.value(best));
	}

	QDomDocument copyAt(QETProject *project, const QString &path)
	{
		QDomDocument document;
		document.appendChild(document.importNode(ElementsLocation(path, project).xml(), true));
		return document;
	}

	/// What the symbol editor does on saving: a new uuid for the symbol
	void newRevision(QDomDocument &definition)
	{
		QDomElement root = definition.documentElement();
		QDomElement uuid = root.firstChildElement(QStringLiteral("uuid"));
		if (uuid.isNull()) {
			uuid = definition.createElement(QStringLiteral("uuid"));
			root.insertBefore(uuid, root.firstChild());
		}
		uuid.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
	}

	QDomElement descriptionOf(const QDomDocument &definition) {
		return definition.documentElement().firstChildElement(QStringLiteral("description"));
	}

	/// The text of the symbol that shows the reference: the information "label"
	QDomElement referenceTextOf(const QDomDocument &definition)
	{
		for (QDomElement text = descriptionOf(definition).firstChildElement(QStringLiteral("dynamic_text"));
			 !text.isNull();
			 text = text.nextSiblingElement(QStringLiteral("dynamic_text")))
		{
			if (text.attribute(QStringLiteral("text_from")) == QLatin1String("ElementInfo")
					&& text.firstChildElement(QStringLiteral("info_name")).text() == QLatin1String("label")) {
				return text;
			}
		}
		return QDomElement();
	}

	/// The text of @p report that shows the reference to the other end
	DynamicElementTextItem *referenceText(const Element *report)
	{
		for (DynamicElementTextItem *text : report->allDynamicTextItems()) {
			if (text->textFrom() == DynamicElementTextItem::ElementInfo
					&& text->infoName() == QETInformation::ELMT_LABEL) {
				return text;
			}
		}
		return nullptr;
	}

	DynamicElementTextItem *textSaying(const Element *element, const QString &value)
	{
		for (DynamicElementTextItem *text : element->allDynamicTextItems()) {
			if (text->toPlainText() == value) {
				return text;
			}
		}
		return nullptr;
	}

	/// A user text in the symbol, as the symbol editor writes one
	void addUserText(QDomDocument &definition, const QString &value, qreal x, qreal y)
	{
		QDomElement text = definition.createElement(QStringLiteral("dynamic_text"));
		text.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
		text.setAttribute(QStringLiteral("x"), x);
		text.setAttribute(QStringLiteral("y"), y);
		text.setAttribute(QStringLiteral("z"), 5);
		text.setAttribute(QStringLiteral("rotation"), 0);
		text.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
		text.setAttribute(QStringLiteral("text_width"), -1);
		text.setAttribute(QStringLiteral("font"), QStringLiteral("Sans Serif,9,-1,5,50,0,0,0,0,0"));
		text.setAttribute(QStringLiteral("keep_visual_rotation"), QStringLiteral("true"));
		text.setAttribute(QStringLiteral("rotation_point_center"), QStringLiteral("false"));
		text.setAttribute(QStringLiteral("text_from"), QStringLiteral("UserText"));
		text.setAttribute(QStringLiteral("Halignment"), QStringLiteral("AlignLeft"));
		text.setAttribute(QStringLiteral("Valignment"), QStringLiteral("AlignTop"));
		QDomElement content = definition.createElement(QStringLiteral("text"));
		content.appendChild(definition.createTextNode(value));
		text.appendChild(content);
		descriptionOf(definition).appendChild(text);
	}

	/// Save over the project's copy, as the symbol editor does, then the
	/// reload of the drawings, which brings the texts in line: one Ctrl+Z
	void saveAndReload(QETProject *project, const QString &path, const QDomDocument &definition)
	{
		REQUIRE(ElementsLocation(path, project).setXml(definition));
		EmbeddedElementRefresh::Report report;
		if (QUndoCommand *command = EmbeddedElementRefresh::updateCommand(
					project, QStringList(), QSet<QString>(), &report)) {
			project->undoStack()->push(command);
		}
	}

	bool samePlace(const QPointF &a, const QPointF &b) {
		return QLineF(a, b).length() < 0.05;
	}

	/// The reports of @p reports whose reference sits where most of them
	/// have it: where the symbol puts it. The others were placed with
	/// another version of the symbol, or moved by hand, and keep their place
	QList<Element *> inLine(const QList<Element *> &reports)
	{
		QList<QPointF> places;
		QList<int> counts;
		for (const Element *report : reports)
		{
			const QPointF place = referenceText(report)->pos();
			int found = -1;
			for (int i = 0 ; i < places.size() ; ++i) {
				if (samePlace(places.at(i), place)) {
					found = i;
				}
			}
			if (found < 0) {
				places << place;
				counts << 1;
			} else {
				++counts[found];
			}
		}
		int best = 0;
		for (int i = 1 ; i < counts.size() ; ++i) {
			if (counts.at(i) > counts.at(best)) {
				best = i;
			}
		}
		QList<Element *> in_line;
		for (Element *report : reports) {
			if (!places.isEmpty() && samePlace(referenceText(report)->pos(), places.at(best))) {
				in_line << report;
			}
		}
		return in_line;
	}

	/// What the symbol editor writes for the label fields of a symbol saved
	/// before the dynamic texts: each <input> with a tag becomes a
	/// dynamic text showing that information, where the field was
	void modernize(QDomDocument &definition)
	{
		QDomElement description = descriptionOf(definition);
		QList<QDomElement> inputs;
		for (QDomElement input = description.firstChildElement(QStringLiteral("input"));
			 !input.isNull();
			 input = input.nextSiblingElement(QStringLiteral("input"))) {
			if (input.attribute(QStringLiteral("tagg"), QStringLiteral("none")) != QLatin1String("none")) {
				inputs << input;
			}
		}
		for (QDomElement input : inputs)
		{
			QDomElement text = definition.createElement(QStringLiteral("dynamic_text"));
			text.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
			text.setAttribute(QStringLiteral("x"), input.attribute(QStringLiteral("x")).toDouble());
			text.setAttribute(QStringLiteral("y"), input.attribute(QStringLiteral("y")).toDouble() - 7);
			text.setAttribute(QStringLiteral("z"), 5);
			text.setAttribute(QStringLiteral("rotation"), input.attribute(QStringLiteral("rotation"), QStringLiteral("0")));
			text.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
			text.setAttribute(QStringLiteral("text_width"), -1);
			text.setAttribute(QStringLiteral("font"), QStringLiteral("Sans Serif,%1,-1,5,50,0,0,0,0,0")
							  .arg(input.attribute(QStringLiteral("size"), QStringLiteral("9"))));
			text.setAttribute(QStringLiteral("keep_visual_rotation"), QStringLiteral("true"));
			text.setAttribute(QStringLiteral("rotation_point_center"), QStringLiteral("false"));
			text.setAttribute(QStringLiteral("text_from"), QStringLiteral("ElementInfo"));
			text.setAttribute(QStringLiteral("Halignment"), QStringLiteral("AlignLeft"));
			text.setAttribute(QStringLiteral("Valignment"), QStringLiteral("AlignTop"));
			text.appendChild(definition.createElement(QStringLiteral("text")));
			QDomElement info = definition.createElement(QStringLiteral("info_name"));
			info.appendChild(definition.createTextNode(input.attribute(QStringLiteral("tagg"))));
			text.appendChild(info);
			description.replaceChild(text, input);
		}
	}

	/// The project's copy at @p path in today's form: the folio reports of
	/// the examples are older than the dynamic texts, and the symbol
	/// editor saves them converted - which is the first update their
	/// arrows go through
	QDomDocument modernCopy(QETProject *project, const QString &path)
	{
		QDomDocument definition = copyAt(project, path);
		if (referenceTextOf(definition).isNull())
		{
			modernize(definition);
			newRevision(definition);
			saveAndReload(project, path, definition);
			definition = copyAt(project, path);
		}
		return definition;
	}
}

TEST_CASE("CU-62.1 — A folio report follows the text box of its symbol",
		  "[T62][reportsync]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const auto busiest = busiestReport(project);
	REQUIRE(busiest.second.size() >= 2);
	const QString path = busiest.first;
	CAPTURE(path.toStdString(), busiest.second.size());

	QDomDocument definition = modernCopy(project, path);
	QDomElement symbol_reference = referenceTextOf(definition);
	REQUIRE_FALSE(symbol_reference.isNull());

		//Its old fields saved as texts of their own, by the symbol editor:
		//each arrow keeps one reference, taken for the converted text, and
		//not a second one next to it
	for (Element *report : busiest.second)
	{
		int references = 0;
		for (DynamicElementTextItem *text : report->allDynamicTextItems()) {
			if (text->textFrom() == DynamicElementTextItem::ElementInfo
					&& text->infoName() == QETInformation::ELMT_LABEL) {
				++references;
			}
		}
		CAPTURE(report->uuid().toString().toStdString());
		CHECK(references == 1);
		CHECK_FALSE(referenceText(report)->symbolTextUuid().isNull());
	}

		//Where each arrow's reference sits, and what it shows; one of them
		//moved by hand on its folio
	QHash<Element *, QPointF> before_pos;
	QHash<Element *, QString> before_text;
	for (Element *report : busiest.second)
	{
		DynamicElementTextItem *text = referenceText(report);
		REQUIRE(text);
		before_pos.insert(report, text->pos());
		before_text.insert(report, text->toPlainText());
	}
	const QList<Element *> in_line = inLine(busiest.second);
	REQUIRE(in_line.size() >= 2);
	Element *moved_by_hand = in_line.last();
	referenceText(moved_by_hand)->setPos(before_pos.value(moved_by_hand) + QPointF(40, 40));
	before_pos.insert(moved_by_hand, referenceText(moved_by_hand)->pos());

		//The symbol's reference text moved by (6, -4), and a bigger font
	symbol_reference.setAttribute(QStringLiteral("x"), symbol_reference.attribute(QStringLiteral("x")).toDouble() + 6);
	symbol_reference.setAttribute(QStringLiteral("y"), symbol_reference.attribute(QStringLiteral("y")).toDouble() - 4);
	QFont bigger = referenceText(busiest.second.first())->font();
	bigger.setPointSizeF(bigger.pointSizeF() + 3);
	symbol_reference.setAttribute(QStringLiteral("font"), bigger.toString());
	newRevision(definition);
	saveAndReload(project, path, definition);

	for (Element *report : busiest.second)
	{
		DynamicElementTextItem *text = referenceText(report);
		REQUIRE(text);
		CAPTURE(report->uuid().toString().toStdString());
			//What it shows is still the reference, from the formula
		CHECK(text->toPlainText() == before_text.value(report));
		CHECK(text->font().pointSizeF() == Approx(bigger.pointSizeF()));
			//Where the symbol had it, it follows; elsewhere - placed with
			//another version, or moved by hand - it stays
		const bool follows = in_line.contains(report) && report != moved_by_hand;
		CHECK(samePlace(text->pos(), before_pos.value(report)) == !follows);
	}
}

TEST_CASE("CU-62.2 — A text added to the symbol of a folio report appears on its arrows",
		  "[T62][reportsync]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const auto busiest = busiestReport(project);
	REQUIRE_FALSE(busiest.second.isEmpty());

	QDomDocument definition = modernCopy(project, busiest.first);
	addUserText(definition, QStringLiteral("NOVO"), 0, 14);
	newRevision(definition);
	saveAndReload(project, busiest.first, definition);

	for (Element *report : busiest.second) {
		CAPTURE(report->uuid().toString().toStdString());
		CHECK(textSaying(report, QStringLiteral("NOVO")) != nullptr);
	}
}

TEST_CASE("CU-62.3 — The update leaves the tag of the arrows alone",
		  "[T62][reportsync]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	project->setShowReportTags(true);
	if (QUndoCommand *tag_all = ReportTags::tagAllCommand(project)) {
		project->undoStack()->push(tag_all);
	}
	const auto busiest = busiestReport(project);
	REQUIRE_FALSE(busiest.second.isEmpty());
	QDomDocument definition = modernCopy(project, busiest.first);

	const QList<Element *> in_line = inLine(busiest.second);
	REQUIRE_FALSE(in_line.isEmpty());
	QHash<Element *, QPointF> tag_pos;
	QHash<Element *, QPointF> reference_pos;
	for (Element *report : busiest.second)
	{
		DynamicElementTextItem *tag = ReportTags::tagText(report);
		REQUIRE(tag);
		tag_pos.insert(report, tag->pos());
		reference_pos.insert(report, referenceText(report)->pos());
	}

		//The symbol's reference text moved
	QDomElement symbol_reference = referenceTextOf(definition);
	REQUIRE_FALSE(symbol_reference.isNull());
	symbol_reference.setAttribute(QStringLiteral("x"), symbol_reference.attribute(QStringLiteral("x")).toDouble() + 6);
	newRevision(definition);
	saveAndReload(project, busiest.first, definition);

	for (Element *report : busiest.second)
	{
		CAPTURE(report->uuid().toString().toStdString());
		DynamicElementTextItem *tag = ReportTags::tagText(report);
		REQUIRE(tag);
		CHECK(samePlace(tag->pos(), tag_pos.value(report)));
		CHECK(tag->toPlainText() == ReportTags::tagOf(report));
		CHECK(tag->symbolTextUuid().isNull());
			//And it is the reference that followed the symbol, where it was
			//where the symbol had it
		CHECK(samePlace(referenceText(report)->pos(), reference_pos.value(report))
			  == !in_line.contains(report));
	}
}

TEST_CASE("CU-62.4 — A symbol saved into the project's copy changes the project",
		  "[T62][reportsync]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const auto busiest = busiestReport(project);
	REQUIRE_FALSE(busiest.second.isEmpty());
		//Opening an example can already change it (texts given their
		//symbol, sizes adopted): what is checked is the change of saving.
		//The mark the title shows is the one that counts: what
		//projectWasModified() answers also counts any title block template
		//the project carries, and the example carries some
	project->undoStack()->setClean();
	project->setModified(false);
	REQUIRE_FALSE(project->projectOptionsWereModified());
	REQUIRE_FALSE(project->pathNameTitle().contains(QStringLiteral("modifi")));

	QDomDocument definition = copyAt(project, busiest.first);
	newRevision(definition);
	REQUIRE(ElementsLocation(busiest.first, project).setXml(definition));

	CHECK(project->projectOptionsWereModified());
	CHECK(project->pathNameTitle().contains(QStringLiteral("modifi")));
}

TEST_CASE("T62 — The notice reloads the projects that have the saved symbol placed",
		  "[T62][reportsync]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const auto busiest = busiestReport(project);
	REQUIRE_FALSE(busiest.second.isEmpty());

	const QList<QETProject *> placing = EmbeddedElementRefresh::projectsPlacing(
				ElementsLocation(busiest.first, project), {project});
	CHECK(placing == QList<QETProject *>{project});

		//A copy placed nowhere: nothing to reload
	const QList<QETProject *> nowhere = EmbeddedElementRefresh::projectsPlacing(
				ElementsLocation(QStringLiteral("import/nowhere/none.elmt"), project), {project});
	CHECK(nowhere.isEmpty());
}


TEST_CASE("CU-62.3 — The tag is not taken for a text of the symbol put where it sits",
		  "[T62][reportsync]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	project->setShowReportTags(true);
	if (QUndoCommand *tag_all = ReportTags::tagAllCommand(project)) {
		project->undoStack()->push(tag_all);
	}
	const auto busiest = busiestReport(project);
	REQUIRE_FALSE(busiest.second.isEmpty());
	QDomDocument definition = modernCopy(project, busiest.first);

		//A text of the symbol showing an information, exactly where the tag
		//of one arrow sits, in its font
	Element *arrow = busiest.second.first();
	DynamicElementTextItem *tag = ReportTags::tagText(arrow);
	REQUIRE(tag);
	const QPointF tag_place = tag->pos();
	const QString tag_font = tag->font().toString();
	const QUuid comment_uuid = QUuid::createUuid();
	{
		QDomElement text = definition.createElement(QStringLiteral("dynamic_text"));
		text.setAttribute(QStringLiteral("uuid"), comment_uuid.toString());
		text.setAttribute(QStringLiteral("x"), tag_place.x());
		text.setAttribute(QStringLiteral("y"), tag_place.y());
		text.setAttribute(QStringLiteral("z"), 5);
		text.setAttribute(QStringLiteral("rotation"), 0);
		text.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
		text.setAttribute(QStringLiteral("text_width"), -1);
		text.setAttribute(QStringLiteral("font"), tag_font);
		text.setAttribute(QStringLiteral("keep_visual_rotation"), QStringLiteral("true"));
		text.setAttribute(QStringLiteral("rotation_point_center"), QStringLiteral("false"));
		text.setAttribute(QStringLiteral("text_from"), QStringLiteral("ElementInfo"));
		text.setAttribute(QStringLiteral("Halignment"), QStringLiteral("AlignLeft"));
		text.setAttribute(QStringLiteral("Valignment"), QStringLiteral("AlignTop"));
		text.appendChild(definition.createElement(QStringLiteral("text")));
		QDomElement info = definition.createElement(QStringLiteral("info_name"));
		info.appendChild(definition.createTextNode(QStringLiteral("comment")));
		text.appendChild(info);
		descriptionOf(definition).appendChild(text);
	}
	newRevision(definition);
	saveAndReload(project, busiest.first, definition);

		//That arrow had the new text deleted on its folio
	DynamicElementTextItem *comment = nullptr;
	for (DynamicElementTextItem *text : arrow->allDynamicTextItems()) {
		if (text->symbolTextUuid() == comment_uuid) {
			comment = text;
		}
	}
	REQUIRE(comment);
	arrow->removeDynamicTextItem(comment);
	delete comment;

		//And the symbol changes that text again
	definition = copyAt(project, busiest.first);
	for (QDomElement text = descriptionOf(definition).firstChildElement(QStringLiteral("dynamic_text"));
		 !text.isNull();
		 text = text.nextSiblingElement(QStringLiteral("dynamic_text"))) {
		if (QUuid(text.attribute(QStringLiteral("uuid"))) == comment_uuid) {
			QFont bigger = tag->font();
			bigger.setPointSizeF(bigger.pointSizeF() + 4);
			text.setAttribute(QStringLiteral("font"), bigger.toString());
		}
	}
	newRevision(definition);
	saveAndReload(project, busiest.first, definition);

	tag = ReportTags::tagText(arrow);
	REQUIRE(tag);
	CHECK(tag->symbolTextUuid().isNull());
	CHECK(tag->font().toString() == tag_font);
	CHECK(tag->toPlainText() == ReportTags::tagOf(arrow));
}
