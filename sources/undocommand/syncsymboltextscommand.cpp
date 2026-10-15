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
#include "syncsymboltextscommand.h"

#include "../ElementsCollection/elementorigin.h"
#include "../qetgraphicsitem/dynamicelementtextitem.h"
#include "../qetgraphicsitem/element.h"

#include <QGraphicsScene>
#include <QObject>

#include <utility>

namespace {

QDomDocument copyOf(const QDomElement &element)
{
	QDomDocument document;
	if (!element.isNull()) {
		document.appendChild(document.importNode(element, true));
	}
	return document;
}

QDomDocument xmlOf(const DynamicElementTextItem *text)
{
	QDomDocument document;
	document.appendChild(text->toXml(document));
	return document;
}

}

/**
	@brief SyncSymbolTextsCommand::SyncSymbolTextsCommand
	@param plans : what to do to each element, see SymbolTextSync::plan().
	An element whose plan changes no text is kept all the same when the
	symbol it is in line with changes: the next update compares with it.
	@param parent
*/
SyncSymbolTextsCommand::SyncSymbolTextsCommand(
		const QList<SymbolTextSync::Plan> &plans,
		QUndoCommand *parent) :
	QUndoCommand(parent)
{
	for (const SymbolTextSync::Plan &plan : plans)
	{
		Element *element = plan.element;
		if (!element) {
			continue;
		}

		Applied applied;
		applied.element = element;
		applied.base_before = copyOf(element->symbolBase());
		applied.base_after = Element::symbolOf(plan.new_base.documentElement());
		applied.removed_before = element->removedSymbolTexts();
		applied.removed_after = plan.removed_symbol_texts;

		const bool new_base = !plan.new_base.isNull()
				&& ElementOrigin::canonicalText(applied.base_before.documentElement(), false)
				   != ElementOrigin::canonicalText(applied.base_after.documentElement(), false);
		if (!new_base) {
			applied.base_after = copyOf(applied.base_before.documentElement());
		}
		if (plan.isEmpty() && !new_base && applied.removed_before == applied.removed_after) {
			continue;
		}

		applied.changes = plan.changed;
		applied.to_add = plan.added;
		applied.removed = plan.removed;

		int visible = 0;
		for (const SymbolTextSync::Change &change : plan.changed) {
			if (change.visible) {
				++visible;
			}
		}
		m_changed_count += visible;
		m_added_count += plan.added.size();
		m_removed_count += plan.removed.size();
		if (visible || !plan.added.isEmpty() || !plan.removed.isEmpty()) {
			++m_elements_count;
		}
		m_applied << applied;
	}

	setText(QObject::tr("Mettre à jour les textes de %n élément(s)", "", m_elements_count));
}

/**
	@brief SyncSymbolTextsCommand::~SyncSymbolTextsCommand
	Delete the texts this command holds: those it removed, once redone, and
	those it added, once undone. The others are on their element.
*/
SyncSymbolTextsCommand::~SyncSymbolTextsCommand()
{
	for (Applied &applied : m_applied)
	{
		if (m_redone) {
			qDeleteAll(applied.removed);
		}
		else if (m_done) {
			qDeleteAll(applied.added);
		}
	}
}

/**
	@brief SyncSymbolTextsCommand::undo
*/
void SyncSymbolTextsCommand::undo()
{
	for (int i = m_applied.size() - 1 ; i >= 0 ; --i)
	{
		Applied &applied = m_applied[i];
		Element *element = applied.element;
		if (!element) {
			continue;
		}
		for (DynamicElementTextItem *text : std::as_const(applied.removed)) {
			give(element, text);
		}
		for (DynamicElementTextItem *text : std::as_const(applied.added)) {
			take(element, text);
		}
		for (int j = 0 ; j < applied.changed.size() ; ++j) {
			if (applied.changed.at(j)) {
				write(applied.changed.at(j), applied.before.at(j));
			}
		}
			//Last: taking a text off its element records it as removed
		element->setSymbolBase(applied.base_before.documentElement());
		element->setRemovedSymbolTexts(applied.removed_before);
	}
	m_redone = false;
}

/**
	@brief SyncSymbolTextsCommand::redo
	The first time, each text is changed from the plan and written down as
	it ends up, and the texts to add are made; afterwards they are written
	back from what was written down.
*/
void SyncSymbolTextsCommand::redo()
{
	for (Applied &applied : m_applied)
	{
		Element *element = applied.element;
		if (!element) {
			continue;
		}

		if (!m_done)
		{
			for (const SymbolTextSync::Change &change : std::as_const(applied.changes))
			{
				DynamicElementTextItem *text = change.text;
				applied.changed << text;
				applied.before << xmlOf(text);
				write(text, change.target);
				if (change.move) {
					text->setPos(SymbolTextSync::posForAnchor(text, change.anchor));
				}
				applied.after << xmlOf(text);
			}
			for (const QDomDocument &xml : std::as_const(applied.to_add))
			{
				auto *text = new DynamicElementTextItem(element);
				write(text, xml);
				applied.added << text;
			}
		}
		else
		{
			for (int j = 0 ; j < applied.changed.size() ; ++j) {
				if (applied.changed.at(j)) {
					write(applied.changed.at(j), applied.after.at(j));
				}
			}
		}

		for (DynamicElementTextItem *text : std::as_const(applied.added)) {
			give(element, text);
		}
		for (DynamicElementTextItem *text : std::as_const(applied.removed)) {
			take(element, text);
		}
		element->setSymbolBase(applied.base_after.documentElement());
		element->setRemovedSymbolTexts(applied.removed_after);
	}
	m_done = true;
	m_redone = true;
}

/**
	@brief SyncSymbolTextsCommand::write
	Give @p text everything @p xml says, its place included, without the
	alignment moving it on the way: the place in @p xml is the final one
*/
void SyncSymbolTextsCommand::write(DynamicElementTextItem *text, const QDomDocument &xml)
{
	text->m_block_alignment = true;
	text->fromXml(xml.documentElement());
	text->m_block_alignment = false;
}

/**
	@brief SyncSymbolTextsCommand::take
	Take @p text off @p element and off the folio, as AddElementTextCommand
	does when it is undone
*/
void SyncSymbolTextsCommand::take(Element *element, DynamicElementTextItem *text)
{
	element->removeDynamicTextItem(text);
	if (text->scene()) {
		text->scene()->removeItem(text);
	}
}

/**
	@brief SyncSymbolTextsCommand::give
	Put @p text back on @p element
*/
void SyncSymbolTextsCommand::give(Element *element, DynamicElementTextItem *text)
{
	text->setParentItem(element);
	element->addDynamicTextItem(text);
}
