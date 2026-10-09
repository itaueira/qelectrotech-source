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
#ifndef UNDRAWNPARTS_H
#define UNDRAWNPARTS_H

#include "enclosuretransfer.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QUuid>

class MountingLayout;

/**
	@brief A part mounted on a face of the layout that no folio of the
	project draws, and why.

	The other half of "off the plate". That list is what the folios draw
	and no face holds; this one is what a face holds and no folio draws.
	Two different things put a part here, and they ask two different
	things of whoever reads the list, which is why the reason is carried
	and not left to be guessed:

	- NoSymbol: the part was laid with no element behind it - picked from
	  the catalogue, the plate laid out before the schematic. Nothing is
	  broken: the symbol is still to be drawn.
	- SymbolGone: the part names an element that no folio holds any more -
	  the symbol was deleted after the part was mounted. The link is kept,
	  and not cleared here: an undo on the folio brings the symbol back,
	  and an unlink done here could not be undone there.

	A piece of rail or duct cut to length never has a symbol, so it is not
	listed as NoSymbol: a plate with four rails would open with four lines
	nobody can act on. It is listed when it names an element that is gone,
	which is a broken link like any other.

	Tying a symbol to a part that is already on the plate is not offered
	by anything here, and not by omission: that command does not exist yet
	(CU-19.12), and this list only says which parts would need it.

	Arithmetic over a layout and a set of identifiers. No project, no
	element, no widget: the folios are read by whoever calls, and handed in
	as the set of what they draw.
*/
class UndrawnPart
{
	public:
		enum class Reason
		{
			NoSymbol,   ///< laid with no element behind it
			SymbolGone  ///< names an element no folio holds any more
		};

			/// the face it is mounted on
		QString surface_uuid;
			/// the part as the layout holds it, its link included
		MountedItem item;
			/// why no folio draws it
		Reason reason = Reason::NoSymbol;

		/**
			@brief The parts of @a layout that no folio draws.
			@param layout the layout of the project
			@param drawn_elements the uuid of the elements the folios
			hold; only the ones namedElements() returns are looked
			up, so the set may be limited to those
			@return one entry per part, face by face in the order of
			the layout, and on one face in the order the parts are
			kept
		*/
		static QList<UndrawnPart> of(const MountingLayout &layout,
					     const QSet<QUuid> &drawn_elements);

		/**
			@return the uuid of every element a part of @a layout
			names, in the order of the layout, each once. The
			question the caller has to ask the folios, and nothing
			more: a project of fourteen folios is not walked to
			answer about three parts.
		*/
		static QList<QUuid> namedElements(const MountingLayout &layout);

		/**
			@return the element @a item names, a null uuid when it
			names none. Read leniently: the file may hold the uuid
			with or without its braces.
		*/
		static QUuid elementOf(const MountedItem &item);
};

#endif // UNDRAWNPARTS_H
