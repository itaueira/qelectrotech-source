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
#include "propertieseditordockwidget.h"
#include "ui_propertieseditordockwidget.h"
#include "propertieseditorwidget.h"

#include <QMainWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

/**
	@brief PropertiesEditorDockWidget::PropertiesEditorDockWidget
	Constructor
	@param parent : parent widget

	The editors are put in a scroll area. Without it, the minimum height
	of the dock is the one of its tallest editor, and when the docks
	stacked in the same area of the main window do not fit in the
	screen, the dock under this one is laid over the bottom of it, and
	what is there can be neither seen nor clicked. Scrolling, the dock
	takes the room it is given.
*/
PropertiesEditorDockWidget::PropertiesEditorDockWidget(QWidget *parent) :
	QDockWidget(parent),
	ui(new Ui::PropertiesEditorDockWidget)
{
	ui->setupUi(this);

	auto scroll_area = new QScrollArea(this);
	scroll_area->setFrameShape(QFrame::NoFrame);
	scroll_area->setWidgetResizable(true);
	scroll_area->setWidget(ui->dockWidgetContents);
	setWidget(scroll_area);
	fitToEditors();
}

/**
	@brief PropertiesEditorDockWidget::~PropertiesEditorDockWidget
	Destructor
*/
PropertiesEditorDockWidget::~PropertiesEditorDockWidget()
{
	clear();
	delete ui;
}

/**
	@brief PropertiesEditorDockWidget::clear
	Remove all editor present in this dock and delete it.
	They also disabled the button box at the bottom of this dock
*/
void PropertiesEditorDockWidget::clear()
{
	if (!m_editor_list.isEmpty())
		rememberHeight();

	foreach (PropertiesEditorWidget *editor, m_editor_list)
	{
		m_editor_list.removeOne(editor);
		ui->m_main_vlayout->removeWidget(editor);
		delete editor;
	}

	m_editor_list.clear();
	fitToEditors();
}

/**
	@brief PropertiesEditorDockWidget::apply
	Call the apply method for each editor present in this dock
*/
void PropertiesEditorDockWidget::apply()
{
	foreach(PropertiesEditorWidget *editor, m_editor_list)
		editor->apply();
}

/**
	@brief PropertiesEditorDockWidget::reset
	Call the reset method for each editor present in this widget
*/
void PropertiesEditorDockWidget::reset()
{
	foreach(PropertiesEditorWidget *editor, m_editor_list)
		editor->reset();
}

/**
	@brief PropertiesEditorDockWidget::addEditor
	Add an editor in this dock at index in the main vertical layout
	(note the button box are displayed at bottom of this layout by default)
	When an editor is added, we enable the button box
	@param editor : editor to add;
	@param index : index of editor in the layout
	@return true if was added (or already add)
	or false if can't be add (editor = nullptr)
*/
bool PropertiesEditorDockWidget::addEditor(PropertiesEditorWidget *editor,
					   int index)
{
	if (!editor) return false;
	if (m_editor_list.contains(editor)) return true;

	const bool was_empty = m_editor_list.isEmpty();
	ui -> m_main_vlayout -> insertWidget(index, editor);
	m_editor_list << editor;
	fitToEditors();
	if (was_empty)
		restoreHeight();
		//Once the editor is laid out, what it asks for is known; after the
		//heights given back, so that it only grows what is still too short
	QTimer::singleShot(0, this, &PropertiesEditorDockWidget::growToContent);
	return true;
}

/**
	@brief PropertiesEditorDockWidget::growToContent
	Ask the main window for the height the editors would like, when the
	dock is shorter than that. Scrolling, the dock no longer forces any
	height, and it was left with what it had when it was empty - a strip
	of a hundred pixels over a dock of automatic numbering half empty,
	seen on the screen on 2026-10-01. resizeDocks() keeps every other dock
	at least at its minimum, so this grows the dock without laying it over
	its neighbour, which its minimum height used to do. It never shrinks
	it: a dock someone made taller stays so.
*/
void PropertiesEditorDockWidget::growToContent()
{
	auto main_window = qobject_cast<QMainWindow *>(parentWidget());
	if (!main_window || isFloating() || !isVisible())
		return;

	auto scroll_area = qobject_cast<QScrollArea *>(widget());
	if (!scroll_area)
		return;
	const int frame = height() - scroll_area->viewport()->height();
		//A content wider than the dock brings the horizontal bar, and the
		//bar takes its height from the view: measured, 14 px short
	const bool too_wide = ui->dockWidgetContents->minimumSizeHint().width()
			      > scroll_area->viewport()->width();
	const int bar = too_wide && !scroll_area->horizontalScrollBar()->isVisible()
			? scroll_area->horizontalScrollBar()->sizeHint().height()
			: 0;
	const int wanted = ui->dockWidgetContents->sizeHint().height() + frame + bar;
	if (height() >= wanted || !main_window->tabifiedDockWidgets(this).isEmpty())
		return;

		//The docks stacked with this one, in the same column of the same
		//area, and what each can give: its height above its own minimum
	const Qt::DockWidgetArea area = main_window->dockWidgetArea(this);
	QList<QDockWidget *> column;
	QList<int> spare;
	int total_spare = 0;
	for (QDockWidget *other : main_window->findChildren<QDockWidget *>())
	{
		if (other == this || other->isFloating() || !other->isVisible()
		    || main_window->dockWidgetArea(other) != area
		    || other->geometry().right() < geometry().left()
		    || other->geometry().left() > geometry().right())
			continue;
		const int minimum = qMax(other->minimumHeight(),
					 other->minimumSizeHint().height());
		column << other;
		spare << qMax(0, other->height() - minimum);
		total_spare += spare.constLast();
	}
	const int given = qMin(wanted - height(), total_spare);
	if (given <= 0)
		return;

		//Every dock of the column gets its size, and the sizes add up to
		//what the column has. Asked for this one alone, resizeDocks() sets
		//it and then shrinks the whole column to fit, this one included,
		//and gives about half - measured, 300 to 427 px asking for 558.
	QList<QDockWidget *> docks({this});
	QList<int> sizes({height() + given});
	int taken = 0;
	for (int i = 0; i < column.size(); ++i)
	{
		const int take = (i == column.size() - 1)
				? given - taken
				: given * spare.at(i) / total_spare;
		taken += take;
		docks << column.at(i);
		sizes << column.at(i)->height() - take;
	}
	main_window->resizeDocks(docks, sizes, Qt::Vertical);
}

/**
	@brief PropertiesEditorDockWidget::editors
	@return all editor used in this dock
*/
QList<PropertiesEditorWidget *> PropertiesEditorDockWidget::editors() const
{
	return m_editor_list;
}

/**
	@brief PropertiesEditorDockWidget::removeEditor
	Remove editor from this dock.
	The editor wasn't delete a the end of this method
	If the editor was the last on this widget, we disabled the button box
	@param editor : editor to remove
	@return true on success, else false
*/
bool PropertiesEditorDockWidget::removeEditor(PropertiesEditorWidget *editor)
{
	if (m_editor_list.size() == 1 && m_editor_list.first() == editor)
		rememberHeight();

	bool result = m_editor_list.removeOne(editor);
	if (result)
		ui -> m_main_vlayout -> removeWidget(editor);
	fitToEditors();

	return result;
}

/**
	@brief PropertiesEditorDockWidget::fitToEditors
	Let the dock shrink to its title bar while it has no editor, so that
	the other docks of its area take the room (#1566). The upstream does it
	with the maximum size of the layout of the editors; here they are in a
	scroll area, which has a height of its own whatever it holds, so the
	scroll area is the one given no height while it is empty - and any
	height again when an editor comes, before restoreHeight() gives the
	docks of the area the heights they had.
*/
void PropertiesEditorDockWidget::fitToEditors()
{
	if (QWidget *area = widget()) {
		area->setMaximumHeight(m_editor_list.isEmpty() ? 0 : QWIDGETSIZE_MAX);
	}
}

/**
	@brief PropertiesEditorDockWidget::rememberHeight
	The dock shrinks to its title bar while it has no editor, and the
	other docks of its area take the room. Keep the heights the docks
	of the area have now, the heights the user gave them, to give them
	back when an editor comes back.
*/
void PropertiesEditorDockWidget::rememberHeight()
{
		// Still the shrunk height: the last editor came and went
		// before the heights were given back
	if (m_restore_pending || isFloating() || !isVisible())
		return;
	auto main_window = qobject_cast<QMainWindow *>(parentWidget());
	if (!main_window)
		return;

	m_area_docks.clear();
	m_area_heights.clear();
	const Qt::DockWidgetArea area = main_window->dockWidgetArea(this);
	for (QDockWidget *dock : main_window->findChildren<QDockWidget *>())
	{
		if (dock->parentWidget() == main_window && !dock->isFloating()
			&& dock->isVisible()
			&& main_window->dockWidgetArea(dock) == area)
		{
			m_area_docks << dock;
			m_area_heights << dock->height();
		}
	}
}

/**
	@brief PropertiesEditorDockWidget::restoreHeight
	Give the docks of the area back the heights they had before this
	dock shrank. Without this the area is split again from the docks'
	size hints, and giving this dock alone its height back takes the
	room only from its neighbours.
*/
void PropertiesEditorDockWidget::restoreHeight()
{
	if (m_area_docks.isEmpty() || m_restore_pending)
		return;

	m_restore_pending = true;
		// Once the layout has given the dock its maximum height back
	QTimer::singleShot(0, this, [this]() {
		m_restore_pending = false;
		auto main_window = qobject_cast<QMainWindow *>(parentWidget());
		if (!main_window || isFloating() || !isVisible()
			|| m_editor_list.isEmpty())
			return;

		QList<QDockWidget *> docks;
		QList<int> heights;
		const Qt::DockWidgetArea area = main_window->dockWidgetArea(this);
		for (int i = 0 ; i < m_area_docks.size() ; ++i)
		{
			QDockWidget *dock = m_area_docks.at(i);
				// Closed, moved or deleted since
			if (dock && !dock->isFloating() && dock->isVisible()
				&& main_window->dockWidgetArea(dock) == area)
			{
				docks << dock;
				heights << m_area_heights.at(i);
			}
		}
		if (docks.contains(this))
			main_window->resizeDocks(docks, heights, Qt::Vertical);
	});
}
