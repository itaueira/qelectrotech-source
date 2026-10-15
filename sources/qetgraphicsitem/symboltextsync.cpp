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
#include "symboltextsync.h"

#include "../ElementsCollection/elementorigin.h"
#include "../diagramcontext.h"
#include "../qet.h"
#include "../textanchor.h"
#include "dynamicelementtextitem.h"
#include "element.h"

#include <QHash>
#include <QLineF>
#include <QScopedPointer>
#include <QStringList>
#include <QTransform>

#include <cmath>
#include <functional>

namespace {

	//Two anchor points closer than this are the same place
const qreal same_place = 0.05;
	//How far an anchor may be for a text saved before texts remembered
	//their symbol text to be taken as that text, when its content differs
const qreal near_place = 0.5;

	//The attributes of a text compared one by one. Its content (where the
	//text comes from and what it says) is compared as a whole, its colour
	//and its angle apart, and its place by its anchor point
const QStringList compared_attributes = {
	QStringLiteral("frame"),
	QStringLiteral("text_width"),
	QStringLiteral("font"),
	QStringLiteral("keep_visual_rotation"),
	QStringLiteral("rotation_point_center"),
	QStringLiteral("Halignment"),
	QStringLiteral("Valignment")
};

const QStringList content_children = {
	QStringLiteral("info_name"),
	QStringLiteral("composite_text"),
	QStringLiteral("text")
};

QDomDocument xmlOf(const DynamicElementTextItem *text)
{
	QDomDocument document;
	document.appendChild(text->toXml(document));
	return document;
}

QString childText(const QDomElement &text, const QString &tag)
{
	return text.firstChildElement(tag).text();
}

	//What the text shows, as a whole: where it comes from, and, for a fixed
	//text, what it says. The text of an information or a composite text is
	//the element's value, not a property of the text
QString content(const QDomElement &text)
{
	const QString from = text.attribute(QStringLiteral("text_from"));
	QString content = from + QLatin1Char('|') + childText(text, QStringLiteral("info_name"))
			+ QLatin1Char('|') + childText(text, QStringLiteral("composite_text"));
	if (from == QLatin1String("UserText")) {
		content += QLatin1Char('|') + childText(text, QStringLiteral("text"));
	}
	return content;
}

	//What links a text saved before texts remembered their symbol text to a
	//text of the symbol: its kind, and what it shows
QString key(const QDomElement &text)
{
	const QString from = text.attribute(QStringLiteral("text_from"));
	if (from == QLatin1String("ElementInfo")) {
		return from + QLatin1Char('|') + childText(text, QStringLiteral("info_name"));
	}
	if (from == QLatin1String("CompositeText")) {
		return from + QLatin1Char('|') + childText(text, QStringLiteral("composite_text"));
	}
	return from + QLatin1Char('|') + childText(text, QStringLiteral("text"));
}

	//The colour of a text, which toXml() writes only when it is not black
QString colorOf(const QDomElement &text)
{
	const QString color = childText(text, QStringLiteral("color"));
	return color.isEmpty() ? QStringLiteral("#000000") : color;
}

	//Put in @p target the child @p tag of @p source, or none if it has none
void copyChild(QDomElement target, const QDomElement &source, const QString &tag)
{
	const QDomElement old = target.firstChildElement(tag);
	if (!old.isNull()) {
		target.removeChild(old);
	}
	const QDomElement from = source.firstChildElement(tag);
	if (!from.isNull()) {
		target.appendChild(target.ownerDocument().importNode(from, true));
	}
}

bool samePlace(const QPointF &a, const QPointF &b, qreal tolerance)
{
	return QLineF(a, b).length() <= tolerance;
}

bool sameAngle(qreal a, qreal b)
{
	return qAbs(std::remainder(a - b, 360.0)) < 0.01;
}

	//How far the element of @p text is turned on its folio
qreal elementRotation(const DynamicElementTextItem *text)
{
	const Element *element = text->parentElement();
	return element ? element->rotation() : 0;
}

	//The angle @p text has in its symbol: a text that keeps its reading
	//when its element is turned (keepVisualRotation()) is turned back by
	//the element's angle, the other turns with it. An element turned after
	//it was placed and one built from its symbol then compare alike
qreal symbolRotation(const DynamicElementTextItem *text)
{
	return QET::correctAngle(text->rotation()
							 + (text->keepVisualRotation() ? elementRotation(text) : 0));
}

	//The angle to give a text of @p element that has @p symbol_angle in its
	//symbol: the other way round from symbolRotation()
qreal placedRotation(qreal symbol_angle, bool keep_visual_rotation, const Element *element)
{
	return QET::correctAngle(symbol_angle
							 - (keep_visual_rotation && element ? element->rotation() : 0));
}

	//An element built from @p definition and given the informations of
	//@p placed, so that its texts show what the placed element's show. The
	//label is given as the placed element shows it: a label made from a
	//formula needs a folio to be computed
Element *detached(const QDomElement &definition, Element *placed)
{
	Element *element = Element::buildDetached(definition);
	DiagramContext informations = placed->elementInformations();
	informations.remove(QStringLiteral("formula"));
	informations.addValue(QStringLiteral("label"), placed->displayedLabel());
	element->setElementInformations(informations);
	return element;
}

	//The texts of @p element made from a text of its symbol, in the order
	//of the symbol
QList<QPair<QUuid, DynamicElementTextItem *>> symbolTexts(const Element *element)
{
	QList<QPair<QUuid, DynamicElementTextItem *>> texts;
	if (!element) {
		return texts;
	}
	for (DynamicElementTextItem *text : element->allDynamicTextItems()) {
		if (!text->symbolTextUuid().isNull()) {
			texts << qMakePair(text->symbolTextUuid(), text);
		}
	}
	return texts;
}

}

/**
	@brief SymbolTextSync::syncs
	@return whether the texts of @p element follow its symbol. A folio
	report's text is its cross reference, not a text of the symbol, so a
	report is left alone.
*/
bool SymbolTextSync::syncs(const Element *element)
{
	return element && !(element->linkType() & Element::AllReport);
}

/**
	@brief SymbolTextSync::anchorOf
	@return the anchor point of @p text in the coordinates of its element,
	with the angle it has in its symbol and without the mirror an element
	may give its texts: the same point for a text of a turned or mirrored
	element and for the same text of an element built from its symbol. A
	text is turned around its own origin when its element is, and its place
	does not change.
*/
QPointF SymbolTextSync::anchorOf(const DynamicElementTextItem *text)
{
	const QPointF origin = text->transformOriginPoint();
	QTransform rotation;
	rotation.translate(origin.x(), origin.y());
	rotation.rotate(symbolRotation(text));
	rotation.translate(-origin.x(), -origin.y());
	return text->pos()
			+ rotation.map(TextAnchor::localPoint(text->boundingRect(), text->alignment()))
			- rotation.map(QPointF(0, 0));
}

/**
	@brief SymbolTextSync::posForAnchor
	@return the position to give @p text so that its anchor point (see
	anchorOf()) is at @p anchor
*/
QPointF SymbolTextSync::posForAnchor(const DynamicElementTextItem *text, const QPointF &anchor)
{
	return text->pos() + anchor - anchorOf(text);
}

namespace {

	//What is compared between a text of an element and the same text of its
	//symbol, as text, and the anchor point apart
struct Values
{
	QHash<QString, QString> fields;
	QPointF anchor;
};

Values valuesOf(const DynamicElementTextItem *text)
{
	Values values;
	const QDomDocument document = xmlOf(text);
	const QDomElement xml = document.documentElement();
	for (const QString &attribute : compared_attributes) {
		values.fields.insert(attribute, xml.attribute(attribute));
	}
	values.fields.insert(QStringLiteral("content"), content(xml));
		//Where the text comes from: an information, a composite text, or a
		//fixed text - what it is for, whatever it says
	values.fields.insert(QStringLiteral("role"),
						 xml.attribute(QStringLiteral("text_from")) + QLatin1Char('|')
						 + childText(xml, QStringLiteral("info_name")) + QLatin1Char('|')
						 + childText(xml, QStringLiteral("composite_text")));
	values.fields.insert(QStringLiteral("color"), colorOf(xml));
	values.fields.insert(QStringLiteral("angle"),
						 QString::number(symbolRotation(text), 'f', 2));
	values.anchor = SymbolTextSync::anchorOf(text);
	return values;
}

	//Which text of the symbol each text of an element comes from
struct Links
{
	QHash<DynamicElementTextItem *, QUuid> link;
	QSet<QUuid> linked;
		///Linked by kind, content and place, the element having been saved
		///before its texts remembered their symbol text
	QSet<DynamicElementTextItem *> guessed;
};

	//The texts of @p element linked to @p reference, the texts of the symbol
	//it is compared with; a uuid already remembered counts when it is in
	//@p reference or in @p also
Links linksOf(Element *element,
			  const QList<QPair<QUuid, DynamicElementTextItem *>> &reference,
			  const QHash<QUuid, DynamicElementTextItem *> &also)
{
	Links links;
	QSet<QUuid> known;
	for (const auto &text : reference) {
		known.insert(text.first);
	}
	const QList<DynamicElementTextItem *> placed_texts = element->allDynamicTextItems();
	for (DynamicElementTextItem *text : placed_texts)
	{
		const QUuid uuid = text->symbolTextUuid();
		if (!uuid.isNull() && !links.linked.contains(uuid)
				&& (known.contains(uuid) || also.contains(uuid))) {
			links.link.insert(text, uuid);
			links.linked.insert(uuid);
		}
	}

		//A text saved before texts remembered it, or made from another
		//symbol: by kind and content at the same place, then by kind and
		//content, then by kind at the same place - a fixed text rewritten on
		//the folio is still that text, and must not get its old content back
		//as a text of its own
	const QList<std::function<bool(const QDomElement &, const QDomElement &,
								   const QPointF &, const QPointF &)>> passes = {
		[](const QDomElement &a, const QDomElement &b, const QPointF &pa, const QPointF &pb) {
			return key(a) == key(b) && samePlace(pa, pb, near_place);},
		[](const QDomElement &a, const QDomElement &b, const QPointF &, const QPointF &) {
			return key(a) == key(b);},
		[](const QDomElement &a, const QDomElement &b, const QPointF &pa, const QPointF &pb) {
			return a.attribute(QStringLiteral("text_from")) == b.attribute(QStringLiteral("text_from"))
					&& samePlace(pa, pb, near_place);}
	};
	for (const auto &pass : passes)
	{
		for (DynamicElementTextItem *text : placed_texts)
		{
			if (links.link.contains(text)) {
				continue;
			}
			const QDomDocument text_document = xmlOf(text);
			const QPointF text_anchor = SymbolTextSync::anchorOf(text);
			for (const auto &candidate : reference)
			{
				if (links.linked.contains(candidate.first)) {
					continue;
				}
				const QDomDocument candidate_document = xmlOf(candidate.second);
				if (pass(text_document.documentElement(), candidate_document.documentElement(),
						 text_anchor, SymbolTextSync::anchorOf(candidate.second)))
				{
					links.link.insert(text, candidate.first);
					links.linked.insert(candidate.first);
					links.guessed.insert(text);
					break;
				}
			}
		}
	}
	return links;
}

	//The definition @p element is in line with, or @p fallback
QDomElement baseOf(const Element *element, const QDomElement &fallback)
{
	const QDomElement base = element->symbolBase();
	return base.isNull() ? fallback : base;
}

}

/**
	@brief SymbolTextSync::consensus
	What most of @p elements - the elements placed from one copy of a symbol
	- have in common, text by text and value by value, among their texts
	saved before texts remembered their symbol text.

	Those elements do not say what symbol they were placed from, and the
	project's copy may have changed since without their texts following.
	Compared with the copy, a value they all have looks changed on a folio
	when it is the value of the older symbol they were placed from: the same
	label place on 87 elements is no work of a hand. plan() takes such a
	value for what the symbol had. A value held by more than half of the
	elements that have the text, and by two at least, is common; what a few
	were given on the folio stays theirs.
	@param elements : elements of one copy
	@return for each text of the symbol, its common values
*/
SymbolTextSync::Consensus SymbolTextSync::consensus(const QList<Element *> &elements)
{
	Consensus common;
	QHash<QUuid, int> texts;
	QHash<QUuid, QHash<QString, QHash<QString, int>>> counts;
	QHash<QUuid, QList<QPointF>> anchors;

	for (Element *element : elements)
	{
		if (!syncs(element) || element->symbolBase().isNull()) {
			continue;
		}
		QScopedPointer<Element> symbol(detached(element->symbolBase(), element));
		const QList<QPair<QUuid, DynamicElementTextItem *>> symbol_texts = symbolTexts(symbol.data());
		const Links links = linksOf(element, symbol_texts, {});
		for (DynamicElementTextItem *text : links.guessed)
		{
			const QUuid uuid = links.link.value(text);
			const Values values = valuesOf(text);
			++texts[uuid];
			for (auto it = values.fields.cbegin() ; it != values.fields.cend() ; ++it) {
				++counts[uuid][it.key()][it.value()];
			}
			anchors[uuid] << values.anchor;
		}
	}

	for (auto it = counts.cbegin() ; it != counts.cend() ; ++it)
	{
		const int total = texts.value(it.key());
		if (total < 2) {
			continue;
		}
		Shared shared;
		for (auto field = it.value().cbegin() ; field != it.value().cend() ; ++field) {
			for (auto value = field.value().cbegin() ; value != field.value().cend() ; ++value) {
				if (value.value() >= 2 && value.value() * 2 > total) {
					shared.fields.insert(field.key(), value.key());
				}
			}
		}
		const QList<QPointF> &places = anchors.value(it.key());
		for (const QPointF &place : places)
		{
			int same = 0;
			for (const QPointF &other : places) {
				if (samePlace(place, other, same_place)) {
					++same;
				}
			}
			if (same >= 2 && same * 2 > total)
			{
				shared.has_anchor = true;
				shared.anchor = place;
				break;
			}
		}
		common.insert(it.key(), shared);
	}
	return common;
}

/**
	@brief SymbolTextSync::plan
	What bringing the texts of @p element in line with @p definition does.
	The symbol the element was in line with before is Element::symbolBase().
	@param element : a placed element
	@param definition : its symbol now (a <definition>)
	@param mode : Merge or Reset, see the namespace
	@param common : what the elements of the same copy have in common, see
	consensus(); used in Merge for the texts saved before they remembered
	their symbol text
	@return the plan; empty when there is nothing to do or when the texts of
	@p element do not follow its symbol (syncs()), but for the symbol it is
	in line with afterwards
*/
SymbolTextSync::Plan SymbolTextSync::plan(Element *element,
										  const QDomElement &definition,
										  Mode mode,
										  const Consensus &common)
{
	Plan plan;
	plan.element = element;
	if (!element || definition.isNull()) {
		return plan;
	}

		//Even an element whose texts do not follow its symbol is in line
		//with it afterwards: the next update compares with it
	plan.new_base.appendChild(plan.new_base.importNode(definition, true));
	plan.removed_symbol_texts = element->removedSymbolTexts();
	if (!syncs(element)) {
		return plan;
	}
	if (mode == Mode::Reset) {
		plan.removed_symbol_texts.clear();
	}

		//The symbol now and the symbol before. They are the same symbol for
		//every element whose copy did not change since it was built, and one
		//element built from it serves both
	QScopedPointer<Element> now(detached(definition, element));
	QScopedPointer<Element> before;
	const QDomElement base = baseOf(element, definition);
	if (ElementOrigin::canonicalText(base, false)
			!= ElementOrigin::canonicalText(Element::symbolOf(definition).documentElement(), false)) {
		before.reset(detached(base, element));
	}

	const QList<QPair<QUuid, DynamicElementTextItem *>> now_texts = symbolTexts(now.data());
	const QList<QPair<QUuid, DynamicElementTextItem *>> before_texts =
			before ? symbolTexts(before.data()) : now_texts;
	QHash<QUuid, DynamicElementTextItem *> now_by_uuid, before_by_uuid;
	for (const auto &text : now_texts) {
		now_by_uuid.insert(text.first, text.second);
	}
	for (const auto &text : before_texts) {
		before_by_uuid.insert(text.first, text.second);
	}

	const Links links = linksOf(element,
								mode == Mode::Reset ? now_texts : before_texts,
								mode == Mode::Reset ? before_by_uuid : now_by_uuid);

	for (DynamicElementTextItem *text : element->allDynamicTextItems())
	{
		if (!links.link.contains(text)) {
			continue;	//added on a folio
		}
		const QUuid uuid = links.link.value(text);
		DynamicElementTextItem *now_text = now_by_uuid.value(uuid);

		if (!now_text)
		{
				//Gone from the symbol. In Merge, only a text the symbol had
				//before: one linked to another symbol says nothing about this
				//one
			if (mode == Mode::Reset || before_by_uuid.contains(uuid)) {
				plan.removed << text;
			}
			continue;
		}

		const QDomDocument placed_document = xmlOf(text);
		const QDomElement placed_xml = placed_document.documentElement();
		const QDomDocument now_document = xmlOf(now_text);
		const QDomElement now_xml = now_document.documentElement();

		const Values placed = valuesOf(text);
		const Values now_values = valuesOf(now_text);
		Values before_values = valuesOf(before_by_uuid.value(uuid, now_text));
			//An element saved before it remembered its symbol text: what most
			//elements of its copy share is what the symbol had
		if (links.guessed.contains(text) && common.contains(uuid))
		{
			const Shared &shared = common[uuid];
			for (auto it = shared.fields.cbegin() ; it != shared.fields.cend() ; ++it) {
				before_values.fields.insert(it.key(), it.value());
			}
			if (shared.has_anchor) {
				before_values.anchor = shared.anchor;
			}
		}

		Change change;
		change.text = text;
		change.move = !text->parentGroup();
		change.target.appendChild(change.target.importNode(placed_xml, true));
		QDomElement target = change.target.documentElement();
		const bool changed = text->symbolTextUuid() != uuid;
		target.setAttribute(QStringLiteral("symbol_text_uuid"), uuid.toString());

			//In Merge, a value follows the symbol where the element still has
			//the value the symbol had, and the symbol changed it; in Reset,
			//it always does
		auto changedOnlyInSymbol = [&](const QString &field) {
			const QString placed_value = placed.fields.value(field);
			const QString now_value = now_values.fields.value(field);
			const QString before_value = before_values.fields.value(field);
			return before_value != now_value && placed_value == before_value;
		};
			//And a text the symbol made another text - a label become the
			//text of the plant, the symbol giving one of its texts another
			//role - takes everything from the symbol: where a label was put
			//by hand says nothing of where the plant goes. A fixed text
			//reworded keeps its role
		const bool becomes_another = mode == Mode::Merge
				&& placed.fields.value(QStringLiteral("role"))
				   != now_values.fields.value(QStringLiteral("role"))
				&& changedOnlyInSymbol(QStringLiteral("role"));
		auto follows = [&](const QString &field) {
			return placed.fields.value(field) != now_values.fields.value(field)
					&& (mode == Mode::Reset || becomes_another || changedOnlyInSymbol(field));
		};

		for (const QString &attribute : compared_attributes)
		{
			if (follows(attribute))
			{
				target.setAttribute(attribute, now_xml.attribute(attribute));
				change.visible = true;
			}
		}

		if (follows(QStringLiteral("content")))
		{
			target.setAttribute(QStringLiteral("text_from"),
								now_xml.attribute(QStringLiteral("text_from")));
			for (const QString &tag : content_children) {
				copyChild(target, now_xml, tag);
			}
			change.visible = true;
		}

		if (follows(QStringLiteral("color")))
		{
			copyChild(target, now_xml, QStringLiteral("color"));
			change.visible = true;
		}

			//The angle, in the symbol's terms: turning the element on its
			//folio is no change of the text
		qreal target_angle = symbolRotation(text);
		if (follows(QStringLiteral("angle")))
		{
			target_angle = symbolRotation(now_text);
			change.visible = true;
		}
		const bool keep = target.attribute(QStringLiteral("keep_visual_rotation"))
				== QLatin1String("true");
		target.setAttribute(QStringLiteral("rotation"),
							QString::number(placedRotation(target_angle, keep, element)));

		change.anchor = placed.anchor;
		if (change.move)
		{
			const bool symbol_moved = !samePlace(before_values.anchor, now_values.anchor, same_place);
			const bool element_moved = !samePlace(placed.anchor, before_values.anchor, same_place);
			if (!samePlace(placed.anchor, now_values.anchor, same_place)
					&& (mode == Mode::Reset || becomes_another
						|| (symbol_moved && !element_moved)))
			{
				change.anchor = now_values.anchor;
				change.visible = true;
			}
		}

		if (changed || change.visible) {
			plan.changed << change;
		}
	}

		//New in the symbol, or never given to this element
	for (const auto &now_text : now_texts)
	{
		if (links.linked.contains(now_text.first)) {
			continue;
		}
		if (mode == Mode::Merge && plan.removed_symbol_texts.contains(now_text.first)) {
			continue;
		}
			//Given the element's angle the way a text placed with it and
			//then turned has it
		QDomDocument added = xmlOf(now_text.second);
		QDomElement added_text = added.documentElement();
		added_text.setAttribute(QStringLiteral("rotation"), QString::number(
				placedRotation(now_text.second->rotation(),
							   now_text.second->keepVisualRotation(), element)));
		plan.added << added;
	}

	return plan;
}
