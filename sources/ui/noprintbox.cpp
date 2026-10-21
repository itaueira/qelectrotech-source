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
#include "noprintbox.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../diagram.h"

#include <QGraphicsObject>
#include <QUndoCommand>

namespace
{
	const char *const property_name = "printable";
}

/**
	@brief NoPrintBox::NoPrintBox
	@param parent : the parent widget
*/
NoPrintBox::NoPrintBox(QWidget *parent) :
	QCheckBox(tr("Do not print"), parent)
{
	setToolTip(tr("On screen only: left out of print, PDF and export."));
	connect(this, &QCheckBox::clicked, this, &NoPrintBox::mark);
	setEnabled(false);
}

/**
	@brief NoPrintBox::setItems
	@param items : the free shapes or free texts the box shows and marks
*/
void NoPrintBox::setItems(const QList<QGraphicsObject *> &items)
{
	for (const QMetaObject::Connection &connection : std::as_const(m_connections)) {
		disconnect(connection);
	}
	m_connections.clear();
	m_items.clear();

	for (QGraphicsObject *item : items)
	{
		if (!item || item->property(property_name).isNull()) {
			continue;
		}
		m_items << QPointer<QGraphicsObject>(item);
			//Undone or done again from elsewhere, the box follows
		m_connections << connect(item, SIGNAL(printableChanged()),
					 this, SLOT(sync()));
	}
	sync();
}

/**
	@brief NoPrintBox::markCommand
	@param items : the items to mark
	@param printable : the state to give them
	@return one step for all of them, or nullptr when none changes
*/
QUndoCommand *NoPrintBox::markCommand(const QList<QGraphicsObject *> &items,
				      bool printable)
{
	QUndoCommand *step = new QUndoCommand(printable ? tr("Print")
							: tr("Do not print"));
	for (QGraphicsObject *item : items)
	{
		if (!item) {
			continue;
		}
		const QVariant now = item->property(property_name);
		if (now.isNull() || now.toBool() == printable) {
			continue;
		}
		new QPropertyUndoCommand(item, property_name, now, printable, step);
	}

	if (!step->childCount()) {
		delete step;
		return nullptr;
	}
	return step;
}

/**
	@brief NoPrintBox::sync
	Checked when no item prints, unchecked when all do, partly checked when
	they differ; disabled with no item.
*/
void NoPrintBox::sync()
{
	int not_printed = 0;
	int count = 0;
	for (const QPointer<QGraphicsObject> &item : std::as_const(m_items))
	{
		if (!item) {
			continue;
		}
		++count;
		if (!item->property(property_name).toBool()) {
			++not_printed;
		}
	}

	const QSignalBlocker blocker(this);
	setEnabled(count > 0);
	if (count == 0 || not_printed == 0) {
		setCheckState(Qt::Unchecked);
	} else if (not_printed == count) {
		setCheckState(Qt::Checked);
	} else {
		setCheckState(Qt::PartiallyChecked);
	}
}

/**
	@brief NoPrintBox::mark
	@param do_not_print : the state the box was clicked to
*/
void NoPrintBox::mark(bool do_not_print)
{
	QList<QGraphicsObject *> items;
	Diagram *diagram = nullptr;
	for (const QPointer<QGraphicsObject> &item : std::as_const(m_items))
	{
		if (!item) {
			continue;
		}
		items << item.data();
		if (!diagram) {
			diagram = qobject_cast<Diagram *>(item->scene());
		}
	}

	QUndoCommand *step = markCommand(items, !do_not_print);
	if (!step) {
		sync();
		return;
	}
	if (diagram) {
		diagram->undoStack().push(step);
	} else {
		step->redo();
		delete step;
	}
	sync();
}
