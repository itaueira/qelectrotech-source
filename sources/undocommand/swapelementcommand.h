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
#ifndef SWAPELEMENTCOMMAND_H
#define SWAPELEMENTCOMMAND_H

#include <QDomDocument>
#include <QHash>
#include <QList>
#include <QString>
#include <QUndoCommand>

#include "../diagramcontext.h"

class Conductor;
class Diagram;
class Element;
class ElementsLocation;
class Terminal;

/**
	@brief Puts another element in the place of one on a folio, and hands
	its wires over.

	What stays is everything that belongs to the folio and not to the
	symbol: the uuid, the position, the rotation and mirror, the frozen
	label and the numbering, the texts as they were arranged, the
	information, the cross-reference links. What comes from the new symbol
	is its drawing and its terminals, and the information the old element
	left empty.

	Each wire keeps its own identity - uuid, number, properties, cable -
	and only changes the terminal it is docked at, chosen by
	TerminalMatch::match(). A wire on a terminal with no match cannot be
	docked anywhere and leaves the folio with the old element; undo brings
	both back.

	The uuid is kept because the mounting plate, the I/O list, the terminal
	strips and the cross references saved in the file find a component by
	it: with the same uuid none of them needs to know a replacement
	happened. The two elements are never on the folio at the same time.
*/
class SwapElementCommand : public QUndoCommand
{
	public:
		/**
			@brief What replacing one element would do, worked out
			before anything on the folio changes, so that it can be
			shown, and refused, first.
		*/
		struct Plan
		{
			Element *old_element = nullptr;
				/// built and placed where the old one is, not on a
				/// folio yet
			Element *new_element = nullptr;
				/// the old element as saved, without its terminals and
				/// links, and with the information completed: read
				/// into the new element once it is on the folio
			QDomDocument state;
				/// each matched terminal of the old element, to its match
			QHash<Terminal *, Terminal *> terminals;
				/// wires whose every end on the old element has a match
			QList<Conductor *> moved_conductors;
				/// wires with an end on a terminal without a match
			QList<Conductor *> lost_conductors;
				/// elements linked to the old one the new one cannot take
			QList<Element *> lost_links;
				/// why nothing can be done; empty when the plan is good
			QString error;
		};

		static bool isSwappable(Element *element);
		static Plan plan(Element *old_element,
				 const ElementsLocation &project_location);
		static void discard(Plan &plan);

		SwapElementCommand(const Plan &plan, QUndoCommand *parent = nullptr);
		~SwapElementCommand() override;

		void undo() override;
		void redo() override;

	private:
		void swap(Element *from, Element *to, bool forward);

		Diagram *m_diagram = nullptr;
		Element *m_old = nullptr;
		Element *m_new = nullptr;
		QDomDocument m_state;
		bool m_state_read = false;
		QHash<Terminal *, Terminal *> m_terminals;
		QList<Conductor *> m_moved;
		QList<Conductor *> m_lost;
			/// the wires of m_lost that redo took off the folio
		QList<Conductor *> m_lost_off;
			/// the links of the old element, and what each linked
			/// element said and was told before the replacement
		QList<Element *> m_links;
		QHash<Element *, DiagramContext> m_link_informations;
		QHash<Element *, int> m_group_index_here;
		QHash<Element *, int> m_group_index_there;
};

#endif // SWAPELEMENTCOMMAND_H
