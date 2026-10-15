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
#ifndef SYNCSYMBOLTEXTSCOMMAND_H
#define SYNCSYMBOLTEXTSCOMMAND_H

#include "../qetgraphicsitem/symboltextsync.h"

#include <QPointer>
#include <QUndoCommand>

class DynamicElementTextItem;
class Element;

/**
	@brief The SyncSymbolTextsCommand class
	Brings the texts of placed elements in line with their symbols, as
	SymbolTextSync::plan() says, and takes them back.

	Each changed text is written back from what it was, so undoing puts it
	back exactly; a text added is kept by this command while it is not on
	its element, and so is a text removed. What each element remembers of
	its symbol (Element::symbolBase()) and of the texts removed from it
	follows undo and redo too.
*/
class SyncSymbolTextsCommand : public QUndoCommand
{
	public:
		SyncSymbolTextsCommand(const QList<SymbolTextSync::Plan> &plans,
							   QUndoCommand *parent = nullptr);
		~SyncSymbolTextsCommand() override;

		void undo() override;
		void redo() override;

		int addedTexts() const {return m_added_count;}
		int changedTexts() const {return m_changed_count;}
		int removedTexts() const {return m_removed_count;}
		int elements() const {return m_elements_count;}
		bool isEmpty() const {return m_applied.isEmpty();}

	private:
		struct Applied
		{
			QPointer<Element> element;
			QList<SymbolTextSync::Change> changes;
			QList<QPointer<DynamicElementTextItem>> changed;
			QList<QDomDocument> before;
			QList<QDomDocument> after;
			QList<QDomDocument> to_add;
			QList<DynamicElementTextItem *> added;
			QList<DynamicElementTextItem *> removed;
			QDomDocument base_before;
			QDomDocument base_after;
			QSet<QUuid> removed_before;
			QSet<QUuid> removed_after;
		};

		static void write(DynamicElementTextItem *text, const QDomDocument &xml);
		static void take(Element *element, DynamicElementTextItem *text);
		static void give(Element *element, DynamicElementTextItem *text);

		QList<Applied> m_applied;
		bool m_done = false;
		bool m_redone = false;
		int m_added_count = 0;
		int m_changed_count = 0;
		int m_removed_count = 0;
		int m_elements_count = 0;
};

#endif // SYNCSYMBOLTEXTSCOMMAND_H
