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
#ifndef EXPLODEELEMENTCOMMAND_H
#define EXPLODEELEMENTCOMMAND_H

#include <QList>
#include <QPointer>
#include <QUndoCommand>

class Diagram;
class Element;
class QGraphicsItem;

/**
	@brief Turns a placed component back into the drawing it was made of.

	The other half of creating a symbol on the sheet: draw, make a block, and
	when the block turns out to need a line moved, explode it, move the line,
	make the block again. Without this the only way to fix a symbol is to draw
	it from nothing a second time, which is how a library ends up with three
	nearly identical symbols.

	What comes back is the drawing: lines, rectangles, ellipses, polygons and
	the fixed texts, placed where they were on the sheet. What does not come
	back is what a drawing cannot hold — the connection points, the class, the
	fields bound to component information. Those are said again in the dialog
	when the block is made, and saying so out loud is better than pretending
	an exploded symbol is lossless.

	A component with conductors attached is refused before this command is
	built: exploding it would leave conductors hanging on nothing.
*/
class ExplodeElementCommand : public QUndoCommand
{
	public:
		ExplodeElementCommand(const QList<Element *> &elements,
				      QUndoCommand *parent = nullptr);
		~ExplodeElementCommand() override;

		void undo() override;
		void redo() override;

		/// how many components the command turned back into drawing
		int elementCount() const;
		/// how many shapes and texts came out
		int pieceCount() const;
		/// whether anything at all could be exploded
		bool isEmpty() const;

		/**
			@brief Why @a element cannot be exploded, empty when it can.
			Said before the command exists, so a refusal is a sentence and
			not a command that does nothing.
		*/
		static QString refusal(Element *element);

	private:
		/// One component, and the pieces it becomes.
		struct Explosion
		{
			QPointer<Element> element;
			QList<QGraphicsItem *> pieces;
			Diagram *diagram = nullptr;
		};

		QList<Explosion> m_explosions;
		/// who owns the items right now, which decides what the destructor frees
		bool m_exploded = false;
};

#endif // EXPLODEELEMENTCOMMAND_H
