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
#ifndef WIREARRANGEMENT_H
#define WIREARRANGEMENT_H

#include <QList>
#include <QPointF>

class Conductor;
class Diagram;
class Element;
class QUndoCommand;

/**
	@brief Puts the bridges of the conductors of a folio where WireLanes
	says, reading the folio for it: the symbols are in the way, the shapes
	are frames, and the conductors that are not being arranged stay where
	they are and are in the way too.

	A conductor between a terminal that points up or down and one that
	points to the side, towards it, gets its shape chosen too: with one
	bend when the straight way is free, else leaving the first terminal to
	the side, going up or down in a lane, and arriving at the second - never
	over the terminal of another component, nor along another wire or the
	side of a frame.

	A conductor placed by the application - drawn with the automatic path,
	or arranged here - may be moved again when a new conductor comes into
	its group. A conductor whose path the user dragged is not, unless the
	user selects it and asks.
*/
namespace WireArrangement
{
	enum class Scope
	{
			/// the conductors given, just drawn, and those placed by the
			/// application that share a group with one of them
		NewConductors,
			/// the conductors given, whoever placed them
		Selection,
			/// every conductor of the folio the user did not place by hand
		Folio,
			/// the conductors given, of components just moved, and the
			/// others of their groups, all but those placed by hand
		Moved
	};

	struct Change
	{
		Conductor *conductor = nullptr;
			/// The new path, in scene coordinates, from terminal1 to terminal2
		QList<QPointF> points;
	};

	QList<Change> plan(Diagram *diagram, const QList<Conductor *> &conductors, Scope scope);
	int arrange(Diagram *diagram, const QList<Conductor *> &conductors, Scope scope);

	bool arrangesAutomatically();
	void setArrangesAutomatically(bool arrange);

	QUndoCommand *arrangeNewConductors(Diagram *diagram, const QList<Conductor *> &conductors,
					   QUndoCommand *parent = nullptr);
	QUndoCommand *arrangeMovedConductors(Diagram *diagram, const QList<Element *> &elements,
					     QUndoCommand *parent = nullptr);
}

#endif // WIREARRANGEMENT_H
