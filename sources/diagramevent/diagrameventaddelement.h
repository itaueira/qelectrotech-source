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
#ifndef DIAGRAMEVENTADDELEMENT_H
#define DIAGRAMEVENTADDELEMENT_H

#include "../ElementsCollection/elementslocation.h"
#include "diagrameventinterface.h"

#include <QStringList>

class Element;
class QStatusBar;

/**
	@brief The DiagramEventAddElement class
	This diagram event add a new element, for each left click button at the position of click.
	Space key rotate current element by 90°, right click button finish this event.
	When the symbol cannot become an element, nothing is placed, the
	project is left as it was, and the reason is written to the log and
	to the status bar of the editor.
*/
class DiagramEventAddElement : public DiagramEventInterface
{
		Q_OBJECT

	public:
			/// The state failureReason() is given when the symbol could not
			/// be copied into the project: not one of the states of
			/// Element::buildFromXml()
		static constexpr int ImportFailed = -1;

		DiagramEventAddElement(ElementsLocation &location, Diagram *diagram, QPointF pos = QPointF(0,0));
		~DiagramEventAddElement() override;

		void mouseMoveEvent        (QGraphicsSceneMouseEvent *event) override;
		void mousePressEvent       (QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent     (QGraphicsSceneMouseEvent *event) override;
		void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event) override;
		void keyPressEvent (QKeyEvent *event) override;
		void init() override;

		static QString failureReason(int state);

	private:
		bool buildElement();
		void addElement();
		bool endIfNothingToPlace();
		void undoImport(const QStringList &added_elements,
				const QStringList &new_directories);
		void reportFailure(int state) const;

	private:
		ElementsLocation m_location;
		Element *m_element;
		QString m_integrate_path;
		QPointer<QStatusBar> m_status_bar;
};

#endif // DIAGRAMEVENTADDELEMENT_H
