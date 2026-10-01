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
#ifndef IMAGEEDITOR_H
#define IMAGEEDITOR_H

#include "../elementitemeditor.h"

#include <QPointer>

class PartImage;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

/**
	@brief The ImageEditor class
	The fields of an image of the 2D tab: where it is and how large, in
	millimetre, each one apart. Nothing holds the proportion of the file -
	the size that counts is the real one of the part - and a button puts it
	back for whoever wants it.
*/
class ImageEditor : public ElementItemEditor
{
		Q_OBJECT

	public:
		explicit ImageEditor(QETElementEditor *editor, QWidget *parent = nullptr);
		~ImageEditor() override;

		bool setPart(CustomElementPart *part) override;
		bool setParts(QList<CustomElementPart *> parts) override;
		CustomElementPart *currentPart() const override;
		QList<CustomElementPart *> currentParts() const override;

	public slots:
		void updateForm() override;

	private:
		void apply();
		void restoreFileProportion();
		void disconnectChangeConnections();

		QDoubleSpinBox
			*m_x_sb      = nullptr,
			*m_y_sb      = nullptr,
			*m_width_sb  = nullptr,
			*m_height_sb = nullptr;
		QPushButton *m_proportion_button = nullptr;
		QLabel *m_file_label = nullptr;
		QPointer<PartImage> m_part;
		QList<QMetaObject::Connection> m_change_connections;
		bool m_locked = false;
};

#endif // IMAGEEDITOR_H
