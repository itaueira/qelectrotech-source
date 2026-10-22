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
#include "scaleelementscommand.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../qetgraphicsitem/element.h"

/**
	@brief ScaleElementsCommand::ScaleElementsCommand
	Each element of @p elements drawn at @p scale, about its hotspot: the
	hotspot stays where it is, on the grid. An element already at that
	scale is left out.
	@param elements : the elements to scale
	@param scale : 1 for the symbol as it is drawn
	@param parent : parent undo command
*/
ScaleElementsCommand::ScaleElementsCommand(const QList<Element *> &elements,
					   qreal scale,
					   QUndoCommand *parent) :
	QUndoCommand(parent)
{
	setText(QObject::tr("Scale %n element(s) to %1 %", "undo caption", elements.size())
		.arg(qRound(scale * 100)));
	for (Element *element : elements)
	{
		if (!element || qFuzzyCompare(element->symbolScale(), scale))
			continue;
		new QPropertyUndoCommand(element, "symbolScale", element->symbolScale(), scale, this);
	}
}

/**
	@brief ScaleElementsCommand::isValid
	@return whether an element changes scale
*/
bool ScaleElementsCommand::isValid() const
{
	return childCount() > 0;
}
