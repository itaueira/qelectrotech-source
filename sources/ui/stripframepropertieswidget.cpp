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
#include "stripframepropertieswidget.h"

#include "../diagram.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/stripframeitem.h"

#include <QFormLayout>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QVBoxLayout>

StripFramePropertiesWidget::StripFramePropertiesWidget(StripFrameItem *frame, QWidget *parent) :
	PropertiesEditorWidget(parent)
{
	m_name = new QLineEdit(this);
	m_spacing = new QSpinBox(this);
	m_spacing->setRange(1, 10000);
	m_spacing->setToolTip(tr("From one terminal to the next, along the strip"));
	m_list = new QListWidget(this);
	m_list->setToolTip(tr("Click a terminal to select it on the folio"));

	auto *form = new QFormLayout;
	form->addRow(tr("Name:"), m_name);
	form->addRow(tr("Spacing:"), m_spacing);
	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(new QLabel(tr("Terminals:"), this));
	layout->addWidget(m_list, 1);

	connect(m_name, &QLineEdit::editingFinished, this, &StripFramePropertiesWidget::applyName);
	connect(m_spacing, &QSpinBox::editingFinished, this, &StripFramePropertiesWidget::applySpacing);
	connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
		pick(m_list->row(item));
	});
	setFrame(frame);
}

void StripFramePropertiesWidget::setFrame(StripFrameItem *frame)
{
	if (m_changed) {
		disconnect(m_changed);
	}
	m_frame = frame;
	if (m_frame) {
		m_changed = connect(m_frame.data(), &StripFrameItem::frameChanged,
				    this, &StripFramePropertiesWidget::updateUi);
	}
	updateUi();
}

QString StripFramePropertiesWidget::title() const
{
	return tr("Drawn terminal strip");
}

/**
	@brief StripFramePropertiesWidget::updateUi
	The fields as the frame is now, the one being typed in left alone
*/
void StripFramePropertiesWidget::updateUi()
{
	const bool editable = m_frame && m_frame->diagram() && !m_frame->diagram()->isReadOnly();
	m_name->setEnabled(editable);
	m_spacing->setEnabled(editable);
	if (!m_frame)
	{
		m_name->clear();
		m_list->clear();
		return;
	}
	if (!m_name->hasFocus()) {
		m_name->setText(m_frame->stripName());
	}
	const QList<Element *> list = m_frame->orderedTerminals();
	if (!m_spacing->hasFocus())
	{
		const QSignalBlocker blocker(m_spacing);
		m_spacing->setEnabled(editable && list.size() > 1);
		m_spacing->setValue(qRound(m_frame->spacing()));
	}
	m_list->clear();
	for (Element *terminal : list)
	{
		const QString label = terminal->actualLabel().trimmed();
		m_list->addItem(label.isEmpty() ? tr("(no label)") : label);
	}
}

/**
	@brief StripFramePropertiesWidget::pick
	The terminal at @a row of the list, the only thing selected on the folio
*/
void StripFramePropertiesWidget::pick(int row)
{
	if (!m_frame || !m_frame->diagram()) {
		return;
	}
	const QList<Element *> list = m_frame->orderedTerminals();
	if (row < 0 || row >= list.size()) {
		return;
	}
	Element *terminal = list.at(row);
	Diagram *folio = m_frame->diagram();
	folio->clearSelection();
	terminal->setSelected(true);
	for (QGraphicsView *view : folio->views()) {
		view->ensureVisible(terminal);
	}
}

void StripFramePropertiesWidget::applyName()
{
	if (m_frame && m_name->text().trimmed() != m_frame->stripName()) {
		m_frame->rename(m_name->text());
	}
}

void StripFramePropertiesWidget::applySpacing()
{
	if (m_frame && m_spacing->value() != qRound(m_frame->spacing())) {
		m_frame->setSpacing(m_spacing->value());
	}
}
