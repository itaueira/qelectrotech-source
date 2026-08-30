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
#ifndef LOCATABLEELEMENT_H
#define LOCATABLEELEMENT_H

#include "../qetgraphicsitem/element.h"

/**
	@brief isLocatableElement
	@param element any item of a folio, or nullptr
	@return true when the element stands for something that occupies room
	in an enclosure, and false when it is a mark drawn on the paper.

	A location answers the question "where is this thing screwed down".
	A folio report has no answer: it is an arrow saying the wire carries
	on elsewhere, and asking which cabinet holds it is asking which
	cabinet holds a sentence. The same goes for a thumbnail and for the
	symbol that only exists to define a conductor.

	The test is written on the four kinds that do occupy room rather than
	on the ones that do not, and that direction is deliberate. A kind that
	appears later and is not listed here falls out of the count, which
	shows up as a report that is missing something; the other direction
	gives a report carrying an item nobody can ever assign, and the aim of
	CU-32.8 is to drive that report to zero. Missing beats unreachable.
*/
inline bool isLocatableElement(const Element *element)
{
	if (!element) {
		return false;
	}

	switch (element->linkType())
	{
		case Element::Simple:
		case Element::Master:
		case Element::Slave:
		case Element::Terminale:
			return true;
		default:
			return false;
	}
}

#endif // LOCATABLEELEMENT_H
