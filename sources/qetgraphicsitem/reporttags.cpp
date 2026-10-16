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
#include "dynamicelementtextitem.h"
#include "element.h"
#include "elementtextitemgroup.h"

#include <QHash>
#include <QMap>
#include <QPointer>
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

	/// The font of the reference @p report shows, for its tag to look like it
	bool referenceFont(const Element *report, QFont *font)
	{
		for (DynamicElementTextItem *text : textsOf(report))
		{
			if ((text->textFrom() == DynamicElementTextItem::ElementInfo
				 && text->infoName() == QETInformation::ELMT_LABEL)
				|| text->textFrom() == DynamicElementTextItem::CompositeText)
			{
				*font = text->font();
				return true;
			}
		}
		return false;
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
		report->addDynamicTextItem(newTagText(report, tagOf(report)));
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
			element->addDynamicTextItem(newTagText(element, tagOf(element)));
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
		new AddElementTextCommand(report, newTagText(report, tags.value(report)), command);
	}
	return command;
}

/**
	@return true if @p text shows the tag of its folio report
*/
bool ReportTags::isTagText(const DynamicElementTextItem *text)
{
	return text
		&& text->textFrom() == DynamicElementTextItem::ElementInfo
		&& text->infoName() == QETInformation::ELMT_REPORT_TAG;
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
	@return a new text of @p report showing its tag, centred above the
	arrow, in the font of the reference the arrow shows. Not yet one of the
	texts of @p report: Element::addDynamicTextItem() or an
	AddElementTextCommand makes it one.
	@param tag : the tag it is about to show, to centre it on: the
	information may still be empty, when a command gives both
*/
DynamicElementTextItem *ReportTags::newTagText(Element *report, const QString &tag)
{
	auto *text = new DynamicElementTextItem(report);
	QFont font;
	if (referenceFont(report, &font)) {
		text->setFont(font);
	}
	text->setInfoName(QETInformation::ELMT_REPORT_TAG);
	text->setTextFrom(DynamicElementTextItem::ElementInfo);

	const QString shown = text->toPlainText();
	text->setPlainText(tag);
	const QRectF box = text->boundingRect();
	const QRectF body = report->boundingRect();
	text->setPos(body.center().x() - box.width() / 2.0, body.top() - box.height());
	text->setPlainText(shown);
	return text;
}
