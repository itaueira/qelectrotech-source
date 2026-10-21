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
#ifndef WIREREMOVAL_H
#define WIREREMOVAL_H

#include <QList>

class Diagram;
class QETProject;

/**
	@brief Removing the wires of a whole project or of some folios at once,
	to draw them again (T79).

	A wire is removed the way the Delete key removes it, by the
	DeleteQGraphicsItemCommand of its folio: what depends on a wire - its
	cable, the numbering of its point - goes on as it does when one wire is
	deleted by hand. The commands of every folio go under one, so that one
	undo puts every wire back.

	Two wires stay. One marked out of the automatic numbering (T77) was left
	out on purpose - the mark, and not a wire out of the numbering because
	of its component (T81) -, nearly always because its name is another one; one on a
	panel marked as built (T29) is in the panel already. The other wires of
	their points go.
*/
namespace WireRemoval
{
		/// What removing the wires of some folios does, or would do
	struct Count
	{
		int removed = 0;  ///< the wires that go
		int left_out = 0; ///< the wires that stay, left out of the numbering
		int frozen = 0;   ///< the wires that stay, on a panel marked as built
	};

		/**
			@return what removeWires() would do with @p folios, without
			doing it. A wire both left out and on a panel built is counted
			as on the panel.
		*/
	Count count(const QList<Diagram *> &folios);

		/**
			Remove the wires of @p folios, but the ones that stay (see the
			namespace), in one undo step of the undo stack of @p project;
			no step at all when there is nothing to remove. A folio of
			another project is skipped.
			@return what was done
		*/
	Count removeWires(QETProject *project, const QList<Diagram *> &folios);
}

#endif // WIREREMOVAL_H
