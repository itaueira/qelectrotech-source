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
#ifndef SCALEELEMENTSCOMMAND_H
#define SCALEELEMENTSCOMMAND_H

#include <QList>
#include <QUndoCommand>

class Element;

/**
	@brief The ScaleElementsCommand class
	Draw elements at another scale on the folio, about their hotspot: these
	placed elements only, not their symbol (T83). See
	Element::setSymbolScale()
*/
class ScaleElementsCommand : public QUndoCommand
{
	public:
		ScaleElementsCommand(const QList<Element *> &elements, qreal scale,
				     QUndoCommand *parent = nullptr);

		bool isValid() const;
};

#endif // SCALEELEMENTSCOMMAND_H
