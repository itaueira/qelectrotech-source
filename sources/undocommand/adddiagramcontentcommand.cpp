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
#include "adddiagramcontentcommand.h"

#include "../diagram.h"
#include "../qgimanager.h"

#include <QGraphicsItem>
#include <QVector>

/**
	@brief itemsToVector
	QGIManager takes a QVector while DiagramContent hands out a QList.
	On Qt5 the QList overloads of manage() and release() are deprecated,
	and on Qt6 QVector is an alias for QList, so the conversion has to be
	written the one way that compiles both ways: an explicit loop.
	QVector::fromList() is gone in Qt6 and the iterator range constructor
	needs Qt 5.14.
	@param items the items to copy
	@return the same items in a QVector
*/
static QVector<QGraphicsItem *> itemsToVector(const QList<QGraphicsItem *> &items)
{
	QVector<QGraphicsItem *> vector;
	vector.reserve(items.size());
	for (QGraphicsItem *item : items) {
		vector.append(item);
	}
	return vector;
}

/**
	@brief AddDiagramContentCommand::AddDiagramContentCommand
	Constructor. The items of @a content are expected to be in @a diagram
	already, put there by Diagram::fromXml; this command takes ownership of
	them so that undo can remove them and redo can put them back.
	@param diagram the diagram they were added to
	@param content what was added
	@param text the undo caption; a default one is built when empty
	@param parent parent undo command
*/
AddDiagramContentCommand::AddDiagramContentCommand(Diagram *diagram,
						   const DiagramContent &content,
						   const QString &text,
						   QUndoCommand *parent) :
	QUndoCommand(parent),
	m_content(content),
	m_diagram(diagram),
	m_filter(DiagramContent::Elements
		 | DiagramContent::TextFields
		 | DiagramContent::Images
		 | DiagramContent::ConductorsToMove
		 | DiagramContent::Shapes
		 | DiagramContent::Tables
		 | DiagramContent::TerminalStrip),
	m_first_redo(true)
{
	if (text.isEmpty())
	{
		setText(QObject::tr("ajouter %1",
				    "undo caption - %1 is a sentence listing the added content")
			.arg(m_content.sentence(m_filter)));
	}
	else
	{
		setText(text);
	}

	m_diagram->qgiManager().manage(itemsToVector(m_content.items(m_filter)));
}

/**
	@brief AddDiagramContentCommand::~AddDiagramContentCommand
	Destructor
*/
AddDiagramContentCommand::~AddDiagramContentCommand()
{
	m_diagram->qgiManager().release(itemsToVector(m_content.items(m_filter)));
}

/**
	@brief AddDiagramContentCommand::undo
	Take the content out of the diagram again
*/
void AddDiagramContentCommand::undo()
{
	m_diagram->showMe();

	const QList<QGraphicsItem *> items = m_content.items(m_filter);
	for (QGraphicsItem *item : items) {
		m_diagram->removeItem(item);
	}
}

/**
	@brief AddDiagramContentCommand::redo
	Put the content back into the diagram. The first call adds nothing:
	whoever built the content has just read it in with Diagram::fromXml,
	which adds the items itself, and adding them twice would put a second
	copy of every item into the scene.
*/
void AddDiagramContentCommand::redo()
{
	m_diagram->showMe();

	if (m_first_redo)
	{
		m_first_redo = false;
	}
	else
	{
		const QList<QGraphicsItem *> items = m_content.items(m_filter);
		for (QGraphicsItem *item : items) {
			m_diagram->addItem(item);
		}
	}

	const QList<QGraphicsItem *> all = m_content.items();
	for (QGraphicsItem *item : all) {
		item->setSelected(true);
	}
}
