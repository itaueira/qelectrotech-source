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
#ifndef NOPRINTBOX_H
#define NOPRINTBOX_H

#include <QCheckBox>
#include <QList>
#include <QMetaObject>
#include <QPointer>

class QGraphicsObject;
class QUndoCommand;

/**
	@brief The "Do not print" box of the properties of a free shape and of
	a free text.

	The items it is given carry a "printable" property and a
	printableChanged() signal: a note of the drawing office - an arrow, a
	reminder - is shown on screen and left out of print, PDF and export.
	Only free shapes and free texts have it, on purpose: a component left
	out of the drawing would still be counted in the parts list and on the
	terminal strip, and the list and the drawing would disagree without
	anything saying so.

	The box shows the state of the items - checked when none of them
	prints, partly checked when they differ - and a click marks all of
	them in one step of the undo stack of their folio.
*/
class NoPrintBox : public QCheckBox
{
	Q_OBJECT

	public:
		explicit NoPrintBox(QWidget *parent = nullptr);

		void setItems(const QList<QGraphicsObject *> &items);

		/**
			The step that gives @a items the printable state @a printable,
			leaving out those already in it.
			@return nullptr when none of them changes
		*/
		static QUndoCommand *markCommand(const QList<QGraphicsObject *> &items,
						 bool printable);

	private slots:
		void sync();

	private:
		void mark(bool do_not_print);

		QList<QPointer<QGraphicsObject>> m_items;
		QList<QMetaObject::Connection> m_connections;
};

#endif // NOPRINTBOX_H
