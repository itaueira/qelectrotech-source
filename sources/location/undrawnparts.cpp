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
#include "undrawnparts.h"

#include "mountinglayout.h"

/**
	@brief UndrawnPart::of
	@param layout the layout of the project
	@param drawn_elements the uuid of the elements the folios hold
	@return the parts no folio draws, with the reason of each

	What decides between the two reasons is whether the part names an
	element at all, and not whether that name reads as a uuid. A link the
	file mangled still names something, and nothing on the folios answers
	to it: that is a symbol gone, and calling it "no symbol" would hide a
	broken link behind the ordinary state of a plate laid out first.
*/
QList<UndrawnPart> UndrawnPart::of(const MountingLayout &layout,
				   const QSet<QUuid> &drawn_elements)
{
	QList<UndrawnPart> undrawn;

	for (int index = 0 ; index < layout.count() ; ++index)
	{
		const MountingSurface &surface = layout.at(index);
		for (const MountedItem &item : surface.items)
		{
			UndrawnPart part;
			part.surface_uuid = surface.uuid;
			part.item = item;

			if (item.element.trimmed().isEmpty())
			{
				if (item.isCutToLength()) {
					continue;
				}
				part.reason = Reason::NoSymbol;
			}
			else
			{
				const QUuid named = elementOf(item);
				if (!named.isNull() && drawn_elements.contains(named)) {
					continue;
				}
				part.reason = Reason::SymbolGone;
			}

			undrawn.append(part);
		}
	}

	return undrawn;
}

/**
	@brief UndrawnPart::namedElements
	@param layout the layout of the project
	@return the uuid of every element a part names, each once
*/
QList<QUuid> UndrawnPart::namedElements(const MountingLayout &layout)
{
	QList<QUuid> named;

	for (int index = 0 ; index < layout.count() ; ++index)
	{
		for (const MountedItem &item : layout.at(index).items)
		{
			const QUuid uuid = elementOf(item);
			if (!uuid.isNull() && !named.contains(uuid)) {
				named.append(uuid);
			}
		}
	}

	return named;
}

/**
	@brief UndrawnPart::elementOf
	@param item a part of the layout
	@return the element it names, null when it names none or the name is
	not a uuid
*/
QUuid UndrawnPart::elementOf(const MountedItem &item)
{
	const QString text = item.element.trimmed();
	return text.isEmpty() ? QUuid() : QUuid(text);
}
