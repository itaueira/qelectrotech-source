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

#include "../../../../sources/autoNum/assignvariables.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramview.h"
#include "../../../../sources/factory/elementfactory.h"
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/elementtextitemgroup.h"
#include "../../../../sources/qetgraphicsitem/reporttags.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/addgraphicsobjectcommand.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QTransform>
#include <QtMath>
#include <QUndoStack>

#include <algorithm>

/*
	T59 - the tag of the folio reports, and the references that name the
	other end by its folio and its tag instead of its row and column.

	affuteuse_250h.qet has folio reports linked in pairs and masters with
	slaves on other folios, and none of its arrows has a tag: it is the
	project saved before the tags that CU-59.5 is about, and a project to
	place new arrows into for the others.
*/

namespace
{
	const QString example = QStringLiteral("affuteuse_250h.qet");

	QList<DynamicElementTextItem *> textsOf(const Element *element)
	{
		QList<DynamicElementTextItem *> texts = element->dynamicTextItems();
		for (ElementTextItemGroup *group : element->textGroups()) {
			texts.append(group->texts());
		}
		return texts;
	}

	/// The text of @p report that shows the reference to the other end
	DynamicElementTextItem *referenceText(const Element *report)
	{
		for (DynamicElementTextItem *text : textsOf(report))
		{
			if ((text->textFrom() == DynamicElementTextItem::ElementInfo
				 && text->infoName() == QETInformation::ELMT_LABEL)
				|| (text->textFrom() == DynamicElementTextItem::CompositeText
					&& text->compositeText().contains(QLatin1String("%{label}")))) {
				return text;
			}
		}
		return nullptr;
	}

	/// The first folio report linked to another, and that other
	QPair<Element *, Element *> linkedPair(const QETProject *project)
	{
		for (Element *report : ReportTags::reports(project)) {
			if (!report->linkedElements().isEmpty() && referenceText(report)) {
				return qMakePair(report, report->linkedElements().first());
			}
		}
		return qMakePair(nullptr, nullptr);
	}

	/// The elements of @p diagram in the order the sheet is read, so that a
	/// case picks the same one every time: Diagram::elements() has no order
	/// The box of the tag of @p report and the arrow's, as the tag reads
	/// them: turned back by the tag's angle, in the arrow's coordinates.
	/// For an arrow and a tag that are not turned, the arrow's coordinates
	QPair<QRectF, QRectF> asTagReads(const Element *report)
	{
		const DynamicElementTextItem *text = ReportTags::tagText(report);
		QTransform back;
		back.rotate(-text->rotation());
		return qMakePair(back.mapRect(text->mapRectToParent(text->boundingRect())),
						 back.mapRect(report->boundingRect()));
	}

	/// The angle @p item is drawn at on the folio, in [0, 360)
	qreal sceneAngle(const QGraphicsItem *item)
	{
		const QTransform transform = item->sceneTransform();
		return QET::correctAngle(qRadiansToDegrees(qAtan2(transform.m12(), transform.m11())), true);
	}

	/// How far apart two angles are, the short way round
	qreal angleBetween(qreal a, qreal b)
	{
		const qreal d = QET::correctAngle(a - b, true);
		return qMin(d, 360.0 - d);
	}

	QList<Element *> inReadingOrder(const Diagram *diagram)
	{
		QList<Element *> elements = diagram->elements();
		std::stable_sort(elements.begin(), elements.end(),
						 [](const Element *a, const Element *b) {
			const QPointF pa = a->scenePos();
			const QPointF pb = b->scenePos();
			return pa.y() < pb.y() || (qFuzzyCompare(pa.y(), pb.y()) && pa.x() < pb.x());
		});
		return elements;
	}

	/// A master with a slave on another folio, and that slave
	QPair<Element *, Element *> masterAndSlave(const QETProject *project)
	{
		for (Diagram *diagram : project->diagrams()) {
			for (Element *element : inReadingOrder(diagram))
			{
				if (element->linkType() != Element::Master
					|| element->actualLabel().isEmpty()) {
					continue;
				}
				for (Element *slave : element->linkedElements()) {
					if (slave->diagram() && slave->diagram() != diagram) {
						return qMakePair(element, slave);
					}
				}
			}
		}
		return qMakePair(nullptr, nullptr);
	}

	/// A new folio report like @p model, placed on @p diagram the way the
	/// elements panel places it
	Element *placeArrow(const Element *model, Diagram *diagram, const QPointF &pos)
	{
		int state = 0;
		Element *arrow = ElementFactory::Instance()->createElement(model->location(), nullptr, &state);
		if (state) {
			delete arrow;
			return nullptr;
		}
		arrow->setPos(pos);
		diagram->addItem(arrow);
		diagram->undoStack().push(new AddGraphicsObjectCommand(arrow, diagram, pos));
		return arrow;
	}

	QString folioOf(const Element *element) {
		return QString::number(element->diagram()->folioIndex() + 1);
	}

	/// What @p formula gives for @p referenced by its position, as before T59
	QString byPosition(const QString &formula, const Element *referenced)
	{
		autonum::sequentialNumbers seq;
		return autonum::AssignVariables::formulaToLabel(
					formula, seq, referenced->diagram(), referenced);
	}

	/// Every text @p element shows of its own: its texts, what they carry,
	/// and the cross reference a slave keeps in its information
	QStringList shownTexts(const Element *element)
	{
		QStringList texts;
		QList<QGraphicsItem *> items = element->childItems();
		while (!items.isEmpty())
		{
			QGraphicsItem *item = items.takeFirst();
			if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) {
				texts << text->toPlainText();
			}
			items.append(item->childItems());
		}
		texts << element->elementInformations().value(QStringLiteral("xref")).toString();
		return texts;
	}

	CrossRefItem *crossRefOf(const Element *master)
	{
		QList<QGraphicsItem *> items = master->childItems();
		while (!items.isEmpty())
		{
			QGraphicsItem *item = items.takeFirst();
			if (auto *xref = dynamic_cast<CrossRefItem *>(item)) {
				return xref;
			}
			items.append(item->childItems());
		}
		return nullptr;
	}

	void tagAll(QETProject *project)
	{
		if (QUndoCommand *command = ReportTags::tagAllCommand(project)) {
			project->undoStack()->push(command);
		}
	}

	void doubleClick(QGraphicsItem *item)
	{
		QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseDoubleClick);
		event.setButton(Qt::LeftButton);
		event.setButtons(Qt::LeftButton);
		event.setScenePos(item->sceneBoundingRect().center());
		event.setPos(item->boundingRect().center());
		item->scene()->sendEvent(item, &event);
	}
}

TEST_CASE("T59 — The position of a reference formula gives way to the tag",
		  "[T59][reporttags]")
{
	using autonum::AssignVariables;
	CHECK(AssignVariables::positionToTag(QStringLiteral("%f-%l%c")) == QStringLiteral("%f-%T"));
	CHECK(AssignVariables::positionToTag(QStringLiteral("(%f-%l%c)")) == QStringLiteral("(%f-%T)"));
	CHECK(AssignVariables::positionToTag(QStringLiteral("%F/%c.%l")) == QStringLiteral("%F/%T"));
	CHECK(AssignVariables::positionToTag(QStringLiteral("Fo %f")) == QStringLiteral("Fo %f"));
	CHECK(AssignVariables::positionToTag(QString()).isEmpty());
}

TEST_CASE("CU-59.1 — A folio report is born with its tag",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const QList<Element *> before = ReportTags::reports(project);
	REQUIRE_FALSE(before.isEmpty());
	for (const Element *report : before) {
		REQUIRE(ReportTags::tagOf(report).isEmpty());
	}
	Diagram *folio = before.first()->diagram();

	SECTION("tags not shown: the tag, and no text for it")
	{
		Element *first = placeArrow(before.first(), folio, QPointF(100, 100));
		Element *second = placeArrow(before.first(), folio, QPointF(200, 100));
		REQUIRE(first);
		REQUIRE(second);
		CHECK(ReportTags::tagOf(first) == QStringLiteral("S1"));
		CHECK(ReportTags::tagOf(second) == QStringLiteral("S2"));
		CHECK(ReportTags::tagText(first) == nullptr);
	}

	SECTION("tags shown: a text that shows it, moves, and is not typed into")
	{
		project->setShowReportTags(true);
		Element *arrow = placeArrow(before.first(), folio, QPointF(100, 100));
		REQUIRE(arrow);
		DynamicElementTextItem *text = ReportTags::tagText(arrow);
		REQUIRE(text);
		CHECK(text->toPlainText() == QStringLiteral("S1"));
		CHECK(text->isVisible());
		CHECK(bool(text->flags() & QGraphicsItem::ItemIsMovable));
			//Above the arrow
		CHECK(text->sceneBoundingRect().bottom() <= arrow->sceneBoundingRect().top() + 0.5);

		doubleClick(text);
		CHECK_FALSE(text->textInteractionFlags() & Qt::TextEditable);
		CHECK(text->toPlainText() == QStringLiteral("S1"));

		project->setShowReportTags(false);
		CHECK_FALSE(text->isVisible());
		project->setShowReportTags(true);
		CHECK(text->isVisible());
	}

	SECTION("the prefix of the project, for the arrows created after it")
	{
		project->setReportTagPrefix(QStringLiteral("R"));
		Element *arrow = placeArrow(before.first(), folio, QPointF(100, 100));
		REQUIRE(arrow);
		CHECK(ReportTags::tagOf(arrow) == QStringLiteral("R1"));
	}
}

TEST_CASE("CU-59.2 — A folio report names the other end by its folio and its tag",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	project->setDefaultReportProperties(QStringLiteral("%f-%l%c"));
	tagAll(project);

	const auto pair = linkedPair(project);
	REQUIRE(pair.first);
	REQUIRE(pair.second);
	Element *here = pair.first;
	Element *there = pair.second;
	DynamicElementTextItem *reference = referenceText(here);
	REQUIRE(reference);
	REQUIRE_FALSE(ReportTags::tagOf(there).isEmpty());

	const QString by_position = byPosition(QStringLiteral("%f-%l%c"), there);
	CHECK(reference->toPlainText() == by_position);

	project->setReferenceStyle(QETProject::ReferenceStyle::Tag);
	CHECK(reference->toPlainText() == folioOf(there) + QStringLiteral("-") + ReportTags::tagOf(there));
		//And the other way round
	DynamicElementTextItem *back = referenceText(there);
	REQUIRE(back);
	CHECK(back->toPlainText() == folioOf(here) + QStringLiteral("-") + ReportTags::tagOf(here));

	project->setReferenceStyle(QETProject::ReferenceStyle::Position);
	CHECK(reference->toPlainText() == by_position);
}

TEST_CASE("CU-59.3 — A contact names its coil by the folio and the label, and the coil its contact",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();

	const auto pair = masterAndSlave(project);
	REQUIRE(pair.first);
	REQUIRE(pair.second);
	Element *coil = pair.first;
	Element *contact = pair.second;
	const QString type = coil->kindInformations()[QStringLiteral("type")].toString();
	XRefProperties xrp = project->defaultXRefProperties(type);
	xrp.setMasterLabel(QStringLiteral("%f-%l%c"));
	xrp.setSlaveLabel(QStringLiteral("(%f-%l%c)"));
	project->setDefaultXRefProperties(type, xrp);
	const QString label = coil->actualLabel();
	CAPTURE(label.toStdString(), type.toStdString());
	CrossRefItem *xref = crossRefOf(coil);
	REQUIRE(xref);
	const QRectF by_position_rect = xref->boundingRect();

	project->setReferenceStyle(QETProject::ReferenceStyle::Tag);
	const QString contact_shows = QStringLiteral("(") + folioOf(coil) + QStringLiteral("-") + label + QStringLiteral(")");
	CHECK(shownTexts(contact).contains(contact_shows));
	CHECK(xref->elementPositionText(contact) == folioOf(contact) + QStringLiteral("-") + label);
		//The list of the coil was laid out again for the new texts, and not
		//only asked for them: its rectangle is the one a fresh layout gives
	const QRectF by_tag_rect = xref->boundingRect();
	CHECK(by_tag_rect != by_position_rect);
	xref->updateLabel();
	CHECK(xref->boundingRect() == by_tag_rect);

	SECTION("the coil renamed: both references follow")
	{
		DiagramContext info = coil->elementInformations();
		info.addValue(QETInformation::ELMT_FORMULA, QString());
		info.addValue(QETInformation::ELMT_LABEL, QStringLiteral("K99"));
		coil->setElementInformations(info);
		QCoreApplication::processEvents();

		CHECK(shownTexts(contact).contains(QStringLiteral("(") + folioOf(coil) + QStringLiteral("-K99)")));
		CHECK(xref->elementPositionText(contact) == folioOf(contact) + QStringLiteral("-K99"));
	}

	SECTION("by position again: the row and the column")
	{
		project->setReferenceStyle(QETProject::ReferenceStyle::Position);
		CHECK(shownTexts(contact).contains(QStringLiteral("(") + byPosition(QStringLiteral("%f-%l%c"), coil)
										   + QStringLiteral(")")));
		CHECK_FALSE(shownTexts(contact).contains(contact_shows));
		CHECK(xref->elementPositionText(contact) == byPosition(QStringLiteral("%f-%l%c"), contact));
	}
}

TEST_CASE("CU-59.4 — A copied arrow gets a tag of its own, a cut one keeps its own",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	tagAll(project);
	const QList<Element *> reports = ReportTags::reports(project);
	REQUIRE(reports.size() >= 2);
	const int count = reports.size();
	Element *original = reports.first();
	Diagram *folio = original->diagram();
	const QString tag = ReportTags::tagOf(original);
	CAPTURE(tag.toStdString(), count);

	SECTION("duplicated, Ctrl+D: the next free tag")
	{
		folio->clearSelection();
		original->setSelected(true);
		DiagramView view(folio);
		view.duplicate(QPoint(0, 20));
		QCoreApplication::processEvents();

		Element *copy = nullptr;
		for (QGraphicsItem *item : folio->selectedItems()) {
			auto *element = dynamic_cast<Element *>(item);
			if (element && element != original) {
				copy = element;
			}
		}
		REQUIRE(copy);
		CHECK(ReportTags::tagOf(original) == tag);
		CHECK(ReportTags::tagOf(copy) == QStringLiteral("S") + QString::number(count + 1));
	}

	SECTION("cut and pasted: the same tag")
	{
		folio->clearSelection();
		original->setSelected(true);
		DiagramView view(folio);
		view.cut();
		QCoreApplication::processEvents();
		REQUIRE_FALSE(ReportTags::reports(project).contains(original));
		view.paste(QPointF(300, 300));
		QCoreApplication::processEvents();

		QStringList tags;
		for (const Element *report : ReportTags::reports(project)) {
			tags << ReportTags::tagOf(report);
		}
		CHECK(tags.size() == count);
		CHECK(tags.count(tag) == 1);
		CHECK_FALSE(tags.contains(QStringLiteral("S") + QString::number(count + 1)));
	}
}

TEST_CASE("CU-59.5 — The arrows of a project saved before the tags get theirs, in one undo",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const QList<Element *> reports = ReportTags::reports(project);
	REQUIRE(reports.size() >= 2);

	project->setShowReportTags(true);
	tagAll(project);

		//In the order of the folios, and on each one top to bottom, then
		//left to right: worked out here from the positions, and not taken
		//from ReportTags::reports(), which is what gave the tags
	QList<Element *> reading;
	for (Diagram *diagram : project->diagrams()) {
		for (Element *element : inReadingOrder(diagram)) {
			if (ReportTags::isReport(element)) {
				reading.append(element);
			}
		}
	}
	REQUIRE(reading.size() == reports.size());
	for (int i = 0 ; i < reading.size() ; ++i)
	{
		CAPTURE(i, folioOf(reading.at(i)).toStdString(), reading.at(i)->scenePos());
		CHECK(ReportTags::tagOf(reading.at(i)) == QStringLiteral("S") + QString::number(i + 1));
		DynamicElementTextItem *text = ReportTags::tagText(reading.at(i));
		REQUIRE(text);
		CHECK(text->toPlainText() == ReportTags::tagOf(reading.at(i)));
	}
		//Nothing left to do: a second time gives no command
	CHECK(ReportTags::tagAllCommand(project) == nullptr);

	project->undoStack()->undo();
	for (const Element *report : reports)
	{
		CHECK(ReportTags::tagOf(report).isEmpty());
		CHECK(ReportTags::tagText(report) == nullptr);
	}
}

TEST_CASE("CU-59.6 — Without a tag at the other end, the reference shows the position",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	project->setDefaultReportProperties(QStringLiteral("%f-%l%c"));

	const auto pair = linkedPair(project);
	REQUIRE(pair.first);
	DynamicElementTextItem *reference = referenceText(pair.first);
	REQUIRE(reference);
	REQUIRE(ReportTags::tagOf(pair.second).isEmpty());

		//The style alone, the arrows left untagged
	project->setReferenceStyle(QETProject::ReferenceStyle::Tag);
	CHECK(reference->toPlainText() == byPosition(QStringLiteral("%f-%l%c"), pair.second));
	CHECK_FALSE(reference->toPlainText().isEmpty());

		//Tagged afterwards, the reference follows
	tagAll(project);
	CHECK(reference->toPlainText() == folioOf(pair.second) + QStringLiteral("-")
		  + ReportTags::tagOf(pair.second));
}

TEST_CASE("CU-59.7 — The options, the tags and a moved tag text come back when the project is reopened",
		  "[T59][reporttags]")
{
	UiBench::ScratchProject bench(UiBench::fileContent(UiBench::examplePath(example)));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

		//Nothing asked, nothing written
	CHECK(bench->toXml().documentElement().firstChildElement(QStringLiteral("folio_references")).isNull());

	bench->setReferenceStyle(QETProject::ReferenceStyle::Tag);
	bench->setShowReportTags(true);
	bench->setReportTagPrefix(QStringLiteral("R"));
	tagAll(bench.project());
	const QList<Element *> reports = ReportTags::reports(bench.project());
	REQUIRE_FALSE(reports.isEmpty());
	const QUuid moved_uuid = reports.first()->uuid();
	DynamicElementTextItem *text = ReportTags::tagText(reports.first());
	REQUIRE(text);
	text->setPos(text->pos() + QPointF(17, 9));
	const QPointF moved = text->pos();
	QStringList tags;
	for (const Element *report : reports) {
		tags << ReportTags::tagOf(report);
	}

	REQUIRE(bench.saveAndReopen());
	INFO(bench.error().toStdString());

	CHECK(bench->referenceStyle() == QETProject::ReferenceStyle::Tag);
	CHECK(bench->showReportTags());
	CHECK(bench->reportTagPrefix() == QStringLiteral("R"));
	QStringList reopened_tags;
	Element *reopened_first = nullptr;
	for (Element *report : ReportTags::reports(bench.project()))
	{
		reopened_tags << ReportTags::tagOf(report);
		if (report->uuid() == moved_uuid) {
			reopened_first = report;
		}
	}
	CHECK(reopened_tags == tags);
	REQUIRE(reopened_first);
	DynamicElementTextItem *reopened_text = ReportTags::tagText(reopened_first);
	REQUIRE(reopened_text);
	CHECK(reopened_text->pos() == moved);
	CHECK(reopened_text->isVisible());
	CHECK(reopened_text->toPlainText() == tags.first());
}

TEST_CASE("CU-59.9 — The font and the colour of the tags, set in the project",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	project->setShowReportTags(true);
	tagAll(project);
	const QList<Element *> reports = ReportTags::reports(project);
	REQUIRE_FALSE(reports.isEmpty());

	QHash<Element *, QString> font_before;
	for (Element *report : reports) {
		REQUIRE(ReportTags::tagText(report));
		font_before.insert(report, ReportTags::tagText(report)->font().toString());
	}

	QFont chosen(QStringLiteral("Courier New"), 14);
	chosen.setBold(true);
	project->setReportTagFont(chosen.toString());
	project->setReportTagColor(QColor(Qt::red));
	QUndoCommand *format = ReportTags::formatCommand(project);
	REQUIRE(format);
	project->undoStack()->push(format);

	for (Element *report : reports)
	{
		CAPTURE(ReportTags::tagOf(report).toStdString());
		DynamicElementTextItem *text = ReportTags::tagText(report);
		CHECK(text->font().family() == chosen.family());
		CHECK(text->font().pointSizeF() == Approx(14));
		CHECK(text->font().bold());
		CHECK(text->color() == QColor(Qt::red));
			//Centred above the arrow, as the tag reads
		const auto [tag_box, arrow_box] = asTagReads(report);
		CHECK(tag_box.bottom() == Approx(arrow_box.top()).margin(0.5));
		CHECK(tag_box.center().x() == Approx(arrow_box.center().x()).margin(0.5));
	}

		//A new arrow is born with them
	Element *arrow = placeArrow(reports.first(), reports.first()->diagram(), QPointF(120, 120));
	REQUIRE(arrow);
	DynamicElementTextItem *born = ReportTags::tagText(arrow);
	REQUIRE(born);
	CHECK(born->font().family() == chosen.family());
	CHECK(born->font().bold());
	CHECK(born->color() == QColor(Qt::red));

		//One Ctrl+Z for every tag that was there: the arrow added comes off
		//first, then the formatting
	project->undoStack()->undo();
	project->undoStack()->undo();
	for (Element *report : reports)
	{
		CAPTURE(ReportTags::tagOf(report).toStdString());
		CHECK(ReportTags::tagText(report)->font().toString() == font_before.value(report));
		CHECK(ReportTags::tagText(report)->color() == QColor(Qt::black));
	}
}

TEST_CASE("CU-59.10 — The tags below the arrows",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	project->setShowReportTags(true);
	tagAll(project);
	const QList<Element *> reports = ReportTags::reports(project);
	REQUIRE_FALSE(reports.isEmpty());

	project->setReportTagPosition(QETProject::ReportTagPosition::Below);
	QUndoCommand *format = ReportTags::formatCommand(project);
	REQUIRE(format);
	project->undoStack()->push(format);

	auto below = [](const Element *report) {
		const auto [tag_box, arrow_box] = asTagReads(report);
		return qAbs(tag_box.top() - arrow_box.bottom()) < 0.5
				&& qAbs(tag_box.center().x() - arrow_box.center().x()) < 0.5;
	};
	for (Element *report : reports) {
		CAPTURE(ReportTags::tagOf(report).toStdString());
		CHECK(below(report));
	}
	Element *arrow = placeArrow(reports.first(), reports.first()->diagram(), QPointF(120, 120));
	REQUIRE(arrow);
	REQUIRE(ReportTags::tagText(arrow));
	CHECK(below(arrow));

		//Nothing left to do: a second time gives no command
	CHECK(ReportTags::formatCommand(project) == nullptr);
}

TEST_CASE("CU-59.11 — The formatting of the tags saved and reopened",
		  "[T59][reporttags]")
{
	UiBench::ScratchProject bench(UiBench::fileContent(UiBench::examplePath(example)));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	bench->setShowReportTags(true);
	QDomElement written = bench->toXml().documentElement().firstChildElement(QStringLiteral("folio_references"));
	CHECK_FALSE(written.hasAttribute(QStringLiteral("report_tag_font")));
	CHECK_FALSE(written.hasAttribute(QStringLiteral("report_tag_color")));
	CHECK_FALSE(written.hasAttribute(QStringLiteral("report_tag_position")));

	QFont chosen(QStringLiteral("Courier New"), 11);
	chosen.setItalic(true);
	bench->setReportTagFont(chosen.toString());
	bench->setReportTagColor(QColor(QStringLiteral("#1e64c8")));
	bench->setReportTagPosition(QETProject::ReportTagPosition::Below);
	tagAll(bench.project());
	REQUIRE_FALSE(ReportTags::reports(bench.project()).isEmpty());

	REQUIRE(bench.saveAndReopen());
	INFO(bench.error().toStdString());
	CHECK(bench->reportTagFont() == chosen.toString());
	CHECK(bench->reportTagColor() == QColor(QStringLiteral("#1e64c8")));
	CHECK(bench->reportTagPosition() == QETProject::ReportTagPosition::Below);

		//And the tags drawn with them come back drawn so
	const QList<Element *> reopened = ReportTags::reports(bench.project());
	REQUIRE_FALSE(reopened.isEmpty());
	for (Element *report : reopened)
	{
		CAPTURE(ReportTags::tagOf(report).toStdString());
		const DynamicElementTextItem *text = ReportTags::tagText(report);
		REQUIRE(text);
		CHECK(text->font().italic());
		CHECK(text->color() == QColor(QStringLiteral("#1e64c8")));
		const auto [tag_box, arrow_box] = asTagReads(report);
		CHECK(tag_box.top() == Approx(arrow_box.bottom()).margin(0.5));
	}
}

TEST_CASE("T59 — On an arrow turned half a turn, the tag reads the way its reference does",
		  "[T59][reporttags]")
{
	UiBench::Project bench(example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	project->setShowReportTags(true);

		//Turned before it has a tag, as the arrows of a project saved before
		//the tags were (CU-59.5); its reference keeps reading upright
	Element *arrow = nullptr;
	for (Element *report : ReportTags::reports(project)) {
		if (referenceText(report) && !ReportTags::tagText(report)) {
			arrow = report;
			break;
		}
	}
	REQUIRE(arrow);
	const DynamicElementTextItem *reference = referenceText(arrow);
	REQUIRE(angleBetween(sceneAngle(reference), 0) < 0.01);
	arrow->setRotation(QET::correctAngle(arrow->rotation() + 180, true));
	REQUIRE(angleBetween(sceneAngle(reference), 0) < 0.01);

	tagAll(project);
	DynamicElementTextItem *tag = ReportTags::tagText(arrow);
	REQUIRE(tag);
	CAPTURE(arrow->rotation(), tag->rotation(), sceneAngle(tag));
	CHECK(angleBetween(sceneAngle(tag), sceneAngle(reference)) < 0.01);
		//Above on the folio, not above the arrow as it is turned
	CHECK(tag->sceneBoundingRect().bottom() == Approx(arrow->sceneBoundingRect().top()).margin(0.5));
	CHECK(tag->sceneBoundingRect().center().x()
		  == Approx(arrow->sceneBoundingRect().center().x()).margin(0.5));

	SECTION("a tag read upside down is turned back by the formatting, in its one undo")
	{
		const qreal right = tag->rotation();
		const qreal wrong = QET::correctAngle(right + 180, true);
		tag->setRotation(wrong);
		QUndoCommand *format = ReportTags::formatCommand(project);
		REQUIRE(format);
		project->undoStack()->push(format);
		CHECK(angleBetween(tag->rotation(), right) < 0.01);
		CHECK(tag->sceneBoundingRect().bottom() == Approx(arrow->sceneBoundingRect().top()).margin(0.5));

		project->undoStack()->undo();
		CHECK(angleBetween(tag->rotation(), wrong) < 0.01);
	}
}
