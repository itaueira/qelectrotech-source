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
#include "reporttags.h"

#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "../undocommand/addelementtextcommand.h"
#include "../undocommand/changeelementinformationcommand.h"
#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "dynamicelementtextitem.h"
#include "element.h"
#include "elementtextitemgroup.h"

#include <QFontMetricsF>
#include <QHash>
#include <QLineF>
#include <QMap>
#include <QPointer>
#include <QTextDocument>
#include <QTransform>
#include <QUndoCommand>

#include <algorithm>

namespace
{
	/// Every text of @p element, its own and those of its groups
	QList<DynamicElementTextItem *> textsOf(const Element *element)
	{
		QList<DynamicElementTextItem *> texts = element->dynamicTextItems();
		for (ElementTextItemGroup *group : element->textGroups()) {
			texts.append(group->texts());
		}
		return texts;
	}

	/// The text of @p report that shows its reference, nullptr if none
	const DynamicElementTextItem *referenceText(const Element *report)
	{
		for (DynamicElementTextItem *text : textsOf(report))
		{
			if ((text->textFrom() == DynamicElementTextItem::ElementInfo
				 && text->infoName() == QETInformation::ELMT_LABEL)
				|| text->textFrom() == DynamicElementTextItem::CompositeText) {
				return text;
			}
		}
		return nullptr;
	}

	/// The font of the reference @p report shows, for its tag to look like it
	bool referenceFont(const Element *report, QFont *font)
	{
		if (const DynamicElementTextItem *reference = referenceText(report)) {
			*font = reference->font();
			return true;
		}
		return false;
	}

	/// The angle the tag of @p report is turned by, in the coordinates of
	/// the arrow: the reference's, so that both read the same way up. A text
	/// keeps the angle it had on the folio when its arrow is turned
	/// (DynamicElementTextItem::keepVisualRotation()), and a tag created
	/// unturned on an arrow turned 180° was read upside down - measured on
	/// a real project on 16/10/2026. Without a reference, upright on the folio.
	qreal tagRotation(const Element *report)
	{
		if (const DynamicElementTextItem *reference = referenceText(report)) {
			qreal rotation = reference->rotation();
			if (const ElementTextItemGroup *group = reference->parentGroup()) {
				rotation += group->rotation();
			}
			return QET::correctAngle(rotation, true);
		}
		return QET::correctAngle(-report->rotation(), true);
	}

	/// The size of a tag when the project chooses no font: small, as the
	/// arrow itself is, and not the size of its reference
	constexpr qreal default_tag_size = 4;
	/// Between the letters of a tag and the drawing of its arrow
	constexpr qreal tag_gap = 1;

	/// The font of the tag of @p report: the one chosen for the project,
	/// or the family of the reference the arrow shows, at
	/// default_tag_size. False when neither says, and the text keeps the
	/// default font of a text
	bool tagFont(const Element *report, const QETProject *project, QFont *font)
	{
		if (project && !project->reportTagFont().isEmpty() && font->fromString(project->reportTagFont())) {
			return true;
		}
		if (!referenceFont(report, font)) {
			return false;
		}
		font->setPointSizeF(default_tag_size);
		return true;
	}

	/// Where @p text goes on @p report once turned by @p rotation: centred
	/// above or below the drawing of the arrow as the tag reads, its
	/// letters tag_gap from it - above is above on the folio for a tag
	/// upright on the folio, whichever way the arrow is turned. In the
	/// coordinates of the arrow, as the text's pos(). The drawing, and not
	/// the box the symbol declares, which can leave the tag far above the
	/// arrow; and the letters, and not the text's box, which has a margin
	/// around them.
	/// @param box : the text's bounding rect, in the font and with the text it
	/// is about to have
	/// @param font : that font
	QPointF tagPlace(const Element *report, const DynamicElementTextItem *text,
					 const QRectF &box, const QFont &font, qreal rotation,
					 QETProject::ReportTagPosition position)
	{
		QTransform turn;
		turn.rotate(rotation);
			//The arrow as the tag reads it
		const QRectF body = turn.inverted().mapRect(report->drawnRect());
		const qreal x = body.center().x() - box.width() / 2.0;
			//The letters in the box: from the top of a capital to the line
			//they stand on
		const QFontMetricsF metrics(font);
		const qreal baseline = box.top() + text->document()->documentMargin() + metrics.ascent();
		const qreal cap_top = baseline - metrics.capHeight();
		const QPointF corner = position == QETProject::ReportTagPosition::Below
				? QPointF(x, body.bottom() + tag_gap - (cap_top - box.top()))
				: QPointF(x, body.top() - tag_gap - (baseline - box.top()));
			//A text is turned about its transform origin: its box's corner
			//lands on pos + origin + turn(box corner - origin)
		const QPointF origin = text->transformOriginPoint();
		return turn.map(corner) - origin - turn.map(box.topLeft() - origin);
	}

	void setTag(Element *report, const QString &tag)
	{
		DiagramContext info = report->elementInformations();
		info.addValue(QETInformation::ELMT_REPORT_TAG, tag);
		report->setElementInformations(info);
	}

	int highestNumber(const QList<Element *> &reports, const QString &prefix)
	{
		int highest = 0;
		for (const Element *report : reports) {
			highest = qMax(highest, ReportTags::numberOf(ReportTags::tagOf(report), prefix));
		}
		return highest;
	}
}

/**
	@return true if @p element is a folio report, coming or going
*/
bool ReportTags::isReport(const Element *element) {
	return element && (element->linkType() & Element::AllReport);
}

/**
	@return the tag of @p report, empty if it has none: an arrow of a
	project saved before the tags, until the project asks for them
*/
QString ReportTags::tagOf(const Element *report)
{
	if (!report) {
		return QString();
	}
	return report->elementInformations()
			.value(QETInformation::ELMT_REPORT_TAG).toString();
}

/**
	@return the number of @p tag, 12 for "S12" with the prefix "S", or -1
	when the tag is not a number after @p prefix
*/
int ReportTags::numberOf(const QString &tag, const QString &prefix)
{
	if (!tag.startsWith(prefix)) {
		return -1;
	}
	bool ok = false;
	const int number = tag.mid(prefix.size()).toInt(&ok);
	return ok && number > 0 ? number : -1;
}

/**
	@return the folio reports of @p project, folio by folio, and on each
	folio in the order the sheet is read: top to bottom, then left to right
*/
QList<Element *> ReportTags::reports(const QETProject *project)
{
	QList<Element *> found;
	if (!project) {
		return found;
	}
	for (Diagram *diagram : project->diagrams())
	{
		QList<Element *> on_folio;
		for (Element *element : diagram->elements()) {
			if (isReport(element)) {
				on_folio.append(element);
			}
		}
		std::stable_sort(on_folio.begin(), on_folio.end(),
						 [](const Element *a, const Element *b) {
			const QPointF pa = a->scenePos();
			const QPointF pb = b->scenePos();
			return pa.y() < pb.y() || (qFuzzyCompare(pa.y(), pb.y()) && pa.x() < pb.x());
		});
		found.append(on_folio);
	}
	return found;
}

/**
	@return the tag for the next folio report of @p project: the prefix of
	the project and one more than the highest number given so far. The
	number is taken, see QETProject::nextReportTagNumber().
*/
QString ReportTags::nextTag(QETProject *project)
{
	const QString prefix = project->reportTagPrefix();
	const int number = project->nextReportTagNumber(highestNumber(reports(project), prefix));
	return prefix + QString::number(number);
}

/**
	@brief ReportTags::give
	Give @p report its tag, if it has none yet, and the text that shows it
	when @p project shows the tags. For an arrow being created: no undo of
	its own, the command that adds the arrow takes the tag away with it.
*/
void ReportTags::give(Element *report, QETProject *project)
{
	if (!isReport(report) || !project) {
		return;
	}
	if (tagOf(report).isEmpty()) {
		setTag(report, nextTag(project));
	}
	if (project->showReportTags() && !tagText(report)) {
		report->addDynamicTextItem(newTagText(report, tagOf(report), project));
	}
}

/**
	@brief ReportTags::renewRepeated
	The folio reports just pasted into @p project, given a tag of their own:
	a copy carries the tag of the arrow it was copied from, and gets the
	next one; an arrow cut and pasted back is the only one with its tag,
	and keeps it.
	@param pasted : the elements pasted, already on their folio
*/
void ReportTags::renewRepeated(const QList<Element *> &pasted, QETProject *project)
{
	if (!project) {
		return;
	}
	QHash<QString, int> uses;
	for (const Element *report : reports(project))
	{
		const QString tag = tagOf(report);
		if (!tag.isEmpty()) {
			++uses[tag];
		}
	}
	for (Element *element : pasted)
	{
		if (!isReport(element)) {
			continue;
		}
		const QString tag = tagOf(element);
		if (tag.isEmpty() || uses.value(tag) > 1)
		{
			if (!tag.isEmpty()) {
				--uses[tag];
			}
			setTag(element, nextTag(project));
		}
		if (project->showReportTags() && !tagText(element)) {
			element->addDynamicTextItem(newTagText(element, tagOf(element), project));
		}
	}
}

/**
	@brief ReportTags::tagAllCommand
	@return the command that gives a tag to every folio report of
	@p project without one, in the order of reports(), and a text to every
	one without it when the project shows the tags; one undo for all.
	nullptr when there is nothing to do.
*/
QUndoCommand *ReportTags::tagAllCommand(QETProject *project)
{
	if (!project) {
		return nullptr;
	}
	const QString prefix = project->reportTagPrefix();
	const QList<Element *> all = reports(project);
	const int highest = highestNumber(all, prefix);

	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> infos;
	QHash<Element *, QString> tags;
	for (Element *report : all)
	{
		QString tag = tagOf(report);
		if (tag.isEmpty())
		{
			tag = prefix + QString::number(project->nextReportTagNumber(highest));
			DiagramContext tagged = report->elementInformations();
			tagged.addValue(QETInformation::ELMT_REPORT_TAG, tag);
			infos.insert(report, qMakePair(report->elementInformations(), tagged));
		}
		tags.insert(report, tag);
	}

	QList<Element *> without_text;
	if (project->showReportTags()) {
		for (Element *report : all) {
			if (!tagText(report)) {
				without_text.append(report);
			}
		}
	}

	if (infos.isEmpty() && without_text.isEmpty()) {
		return nullptr;
	}
	auto *command = new QUndoCommand(QObject::tr("Donner un repère aux renvois de folio"));
	if (!infos.isEmpty()) {
		new ChangeElementInformationCommand(infos, command);
	}
	for (Element *report : without_text) {
		new AddElementTextCommand(report, newTagText(report, tags.value(report), project), command);
	}
	return command;
}

/**
	@brief ReportTags::formatCommand
	@return the command that gives every tag of @p project the font, the
	colour and the place the project sets for them, one undo for all;
	nullptr when every tag already has them. A tag moved by hand is put
	back in its place: changing the place is what this is for.
*/
QUndoCommand *ReportTags::formatCommand(QETProject *project)
{
	if (!project) {
		return nullptr;
	}
	auto *command = new QUndoCommand(QObject::tr("Formater les repères des renvois de folio"));
	for (Element *report : reports(project))
	{
		DynamicElementTextItem *text = tagText(report);
		if (!text) {
			continue;
		}
		const QFont old_font = text->font();
		QFont font = old_font;
		tagFont(report, project, &font);
		const QColor color = project->reportTagColor();
		const qreal old_rotation = text->rotation();
		const qreal rotation = tagRotation(report);

			//The box it will have in its new font
		const QPointF old_pos = text->pos();
		text->setFont(font);
		const QRectF box = text->boundingRect();
		text->setFont(old_font);
		text->setPos(old_pos);
		const QPointF pos = tagPlace(report, text, box, font, rotation, project->reportTagPosition());

		if (font != old_font) {
			new QPropertyUndoCommand(text, "font", old_font, font, command);
		}
		if (color != text->color()) {
			new QPropertyUndoCommand(text, "color", text->color(), color, command);
		}
			//A tag read upside down, from before it took its reference's angle
		if (!qFuzzyCompare(QET::correctAngle(old_rotation, true) + 1.0, rotation + 1.0)) {
			new QPropertyUndoCommand(text, "rotation", old_rotation, rotation, command);
		}
		if (QLineF(pos, old_pos).length() > 0.01) {
			new QPropertyUndoCommand(text, "pos", old_pos, pos, command);
		}
	}
	if (command->childCount() == 0) {
		delete command;
		return nullptr;
	}
	return command;
}

/**
	@return true if @p text is the text the program gave its folio report
	to show its tag. A text of the symbol may show the tag too - a symbol
	drawn to show it, or its reference text made to - and that one is the
	symbol's: it follows the symbol (T62), whatever the project says of
	showing the tags. What tells them apart is that a text of the symbol is
	linked to it, and the program's never is.
*/
bool ReportTags::isTagText(const DynamicElementTextItem *text)
{
	return text
		&& text->textFrom() == DynamicElementTextItem::ElementInfo
		&& text->infoName() == QETInformation::ELMT_REPORT_TAG
		&& text->symbolTextUuid().isNull();
}

/**
	@return the text that shows the tag of @p report, nullptr if none
*/
DynamicElementTextItem *ReportTags::tagText(const Element *report)
{
	if (!report) {
		return nullptr;
	}
	for (DynamicElementTextItem *text : textsOf(report)) {
		if (isTagText(text)) {
			return text;
		}
	}
	return nullptr;
}

/**
	@brief ReportTags::newTagText
	@return a new text of @p report showing its tag, drawn as @p project
	sets the tags: its font - the reference's unless one was chosen -, its
	colour, centred above or below the arrow, and turned the way the
	reference is, so that both read the same way up. Not yet one of the
	texts of @p report: Element::addDynamicTextItem() or an
	AddElementTextCommand makes it one.
	@param tag : the tag it is about to show, to centre it on: the
	information may still be empty, when a command gives both
*/
DynamicElementTextItem *ReportTags::newTagText(Element *report, const QString &tag,
											   QETProject *project)
{
	auto *text = new DynamicElementTextItem(report);
	QFont font;
	if (tagFont(report, project, &font)) {
		text->setFont(font);
	}
	if (project) {
		text->setColor(project->reportTagColor());
	}
	text->setInfoName(QETInformation::ELMT_REPORT_TAG);
	text->setTextFrom(DynamicElementTextItem::ElementInfo);

	const qreal rotation = tagRotation(report);
	text->setRotation(rotation);

	const QString shown = text->toPlainText();
	text->setPlainText(tag);
	const QRectF box = text->boundingRect();
	text->setPos(tagPlace(report, text, box, text->font(), rotation,
						  project ? project->reportTagPosition()
								  : QETProject::ReportTagPosition::Above));
	text->setPlainText(shown);
	return text;
}
