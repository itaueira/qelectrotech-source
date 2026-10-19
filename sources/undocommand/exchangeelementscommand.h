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
#ifndef EXCHANGEELEMENTSCOMMAND_H
#define EXCHANGEELEMENTSCOMMAND_H

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QUndoCommand>

class Conductor;
class Diagram;
class Element;
class Terminal;

/**
	@brief Puts two elements of a folio each in the place of the other:
	position, rotation and mirrors. The wires stay where they are, and each
	element takes the wires of the place it comes to.

	Each element keeps all that is its own - uuid, label, texts, information,
	links, terminal strip - and only changes place, as if it had been dragged
	and turned there. A wire end goes to the terminal of the element coming
	that lies where it ends, or else to the one of the same name
	(TerminalMatch::matchByPlace()). A wire with an end on no terminal cannot
	be docked anywhere and leaves the folio; undo brings it back.
*/
class ExchangeElementsCommand : public QUndoCommand
{
	public:
			/// Where an element is on its folio
		struct Place
		{
			QPointF pos;
			qreal rotation = 0;
			bool horizontal_mirror = false;
			bool vertical_mirror = false;
		};

			/**
				@brief What exchanging two elements would do, worked out
				before anything on the folio changes, so that it can be
				shown, and refused, first.
			*/
		struct Plan
		{
			Element *first = nullptr;
			Element *second = nullptr;
				/// each terminal of the two elements that has wires to
				/// hand over, to the terminal of the other that takes them
			QHash<Terminal *, Terminal *> terminals;
				/// wires whose every end on the two elements has a match
			QList<Conductor *> moved_conductors;
				/// wires with an end on a terminal without a match
			QList<Conductor *> lost_conductors;
				/// why nothing can be done; empty when the plan is good
			QString error;
		};

		static Plan plan(Element *first, Element *second);
		static Place placeOf(const Element *element);

		ExchangeElementsCommand(const Plan &plan, QUndoCommand *parent = nullptr);
		~ExchangeElementsCommand() override;

		void undo() override;
		void redo() override;

	private:
		void exchange(bool forward);
		static void setPlace(Element *element, const Place &place);

		Diagram *m_diagram = nullptr;
		Element *m_first = nullptr;
		Element *m_second = nullptr;
		Place m_first_place;
		Place m_second_place;
		QHash<Terminal *, Terminal *> m_forward;
		QHash<Terminal *, Terminal *> m_backward;
		QList<Conductor *> m_moved;
		QList<Conductor *> m_lost;
			/// the wires of m_lost that redo took off the folio
		QList<Conductor *> m_lost_off;
};

#endif // EXCHANGEELEMENTSCOMMAND_H
