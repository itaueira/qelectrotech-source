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
#include "folionavigationbuttons.h"

#include <QAction>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QWidget>

/**
	@brief FolioNavigationButtons::FolioNavigationButtons
	@param window : the window whose widgets answer the buttons, which also
	owns this object
	@param back : triggered by the Back button of the mouse
	@param forward : triggered by the Forward button of the mouse
*/
FolioNavigationButtons::FolioNavigationButtons(QWidget *window, QAction *back, QAction *forward) :
	QObject(window),
	m_window(window),
	m_back(back),
	m_forward(forward)
{
	if (QCoreApplication *application = QCoreApplication::instance()) {
		application->installEventFilter(this);
	}
}

/**
	@brief FolioNavigationButtons::eventFilter
	Trigger the action of the Back or Forward button pressed on a widget of
	the window, and keep the press, the double click and the release of
	those two buttons from the widget: it never asked for them, and a widget
	that took the press would be left waiting for a release.
	A disabled action is not triggered, but its button is kept all the same.
	@return true for the events this object kept
*/
bool FolioNavigationButtons::eventFilter(QObject *watched, QEvent *event)
{
	const QEvent::Type type = event->type();
	if (type != QEvent::MouseButtonPress
	    && type != QEvent::MouseButtonDblClick
	    && type != QEvent::MouseButtonRelease) {
		return false;
	}

	const auto mouse_event = static_cast<QMouseEvent *>(event);
	QAction *action = nullptr;
	if (mouse_event->button() == Qt::BackButton) {
		action = m_back;
	} else if (mouse_event->button() == Qt::ForwardButton) {
		action = m_forward;
	} else {
		return false;
	}

	const auto widget = qobject_cast<QWidget *>(watched);
	if (!widget || !m_window || widget->window() != m_window) {
		return false;
	}

	if (type != QEvent::MouseButtonRelease && action && action->isEnabled()) {
		action->trigger();
	}
	event->accept();
	return true;
}
