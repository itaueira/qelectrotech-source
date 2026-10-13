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
#ifndef FOLIONAVIGATIONBUTTONS_H
#define FOLIONAVIGATIONBUTTONS_H

#include <QObject>
#include <QPointer>

class QAction;
class QWidget;

/**
	@brief The FolioNavigationButtons class
	Gives the Back and Forward buttons of a mouse to two actions, over the
	whole of one window: the folio, the tabs, the panels and the tool bars,
	as a web browser does over the whole of its page.

	It has to watch the application and not the window. A press is given to
	the widget under the cursor first, and many of them take every button
	they are pressed with - a graphics item, a tree - so a press on the
	folio would never come up to the window. Another window of the program,
	such as the element editor or a dialog, is left alone.

	The second of two quick presses comes as a double click, and counts like
	a press: two quick presses on Back go back twice, as in a browser.
*/
class FolioNavigationButtons : public QObject
{
	Q_OBJECT

	public:
		FolioNavigationButtons(QWidget *window, QAction *back, QAction *forward);

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;

	private:
		QPointer<QWidget> m_window;
		QPointer<QAction> m_back;
		QPointer<QAction> m_forward;
};

#endif // FOLIONAVIGATIONBUTTONS_H
