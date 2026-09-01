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
#ifndef DIAGRAMEVENTADDLOCATIONAREA_H
#define DIAGRAMEVENTADDLOCATIONAREA_H

#include "diagrameventinterface.h"

#include <QPointF>

class LocationAreaItem;
class QGraphicsLineItem;

/**
	@brief The DiagramEventAddLocationArea class
	This event manages the drawing of a location area on a folio.

	It is a separate event from the one that draws the shapes, even though
	the rectangle on screen is dragged out the same way, and the reason is
	the ending: a shape is finished when the second corner is placed, while
	an area still has to be told which location it stands for, and has to
	hand the components it has just enclosed to the assignment command. None
	of that belongs in the shape tool, which would have to grow a flag and
	two branches to carry it.

	The gesture accepts both hands. Press, drag and release draws the
	rectangle in one movement, which is what a hand used to drawing boxes
	does; press, move and press again also works, which is what the shape
	tool taught the same hand. A right click throws away a rectangle still
	being drawn, and a right click with nothing in progress puts the tool
	away.

	The tool stays armed after each area, because an enclosure is rarely
	alone: a folio that has one cabinet on it usually has the door and two
	sub-plates as well.
*/
class DiagramEventAddLocationArea : public DiagramEventInterface
{
		Q_OBJECT

	public:
		explicit DiagramEventAddLocationArea(Diagram *diagram);
		~DiagramEventAddLocationArea() override;

		void mousePressEvent   (QGraphicsSceneMouseEvent *event) override;
		void mouseMoveEvent    (QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent (QGraphicsSceneMouseEvent *event) override;
		void init() override;

	private:
		void updateHelpCross (const QPointF &p);
		void discardArea();
		bool commitArea();

			///ATTRIBUTES
	private:
		LocationAreaItem  *m_area{nullptr};
		QGraphicsLineItem *m_help_horiz{nullptr},
				  *m_help_verti{nullptr};
};

#endif // DIAGRAMEVENTADDLOCATIONAREA_H
