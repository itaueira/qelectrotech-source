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
#ifndef STRIPFRAMEPROPERTIESWIDGET_H
#define STRIPFRAMEPROPERTIESWIDGET_H

#include "../PropertiesEditor/propertieseditorwidget.h"

#include <QPointer>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QSpinBox;
class StripFrameItem;

/**
	@brief The StripFramePropertiesWidget class
	The frame of a strip drawn round its terminals (T85), in the properties
	dock: its name, the spacing of its terminals, and the list of them. A
	terminal clicked in the list is the one selected on the folio - the
	terminals of a strip stand too close to pick one there by hand.
*/
class StripFramePropertiesWidget : public PropertiesEditorWidget
{
	Q_OBJECT

	public:
		explicit StripFramePropertiesWidget(StripFrameItem *frame, QWidget *parent = nullptr);

		void setFrame(StripFrameItem *frame);
		StripFrameItem *frame() const {return m_frame.data();}
		QString title() const override;
		void updateUi() override;
		void pick(int row);

	private:
		void applyName();
		void applySpacing();

		QPointer<StripFrameItem> m_frame;
		QMetaObject::Connection m_changed;
		QLineEdit *m_name = nullptr;
		QSpinBox *m_spacing = nullptr;
		QListWidget *m_list = nullptr;
};

#endif // STRIPFRAMEPROPERTIESWIDGET_H
