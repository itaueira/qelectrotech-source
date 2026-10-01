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

#include <QScrollArea>

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
	foreach (PropertiesEditorWidget *editor, m_editor_list)
	{
		m_editor_list.removeOne(editor);
		ui->m_main_vlayout->removeWidget(editor);
		delete editor;
	}

	m_editor_list.clear();
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

	ui -> m_main_vlayout -> insertWidget(index, editor);
	m_editor_list << editor;
	return true;
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
	bool result = m_editor_list.removeOne(editor);
	if (result)
		ui -> m_main_vlayout -> removeWidget(editor);

	return result;
}
