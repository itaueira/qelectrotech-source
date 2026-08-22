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
#ifndef IECSTRUCTUREDIALOG_H
#define IECSTRUCTUREDIALOG_H

#include <QDialog>

#include "../iecstructure.h"

class QCheckBox;
class QComboBox;
class QETProject;
class QLabel;

/**
	@brief Turns the IEC 81346 identification structure on for one project.

	Three controls and a preview, because there are only three decisions:
	whether the norm applies to this project at all, how much of the tag the
	drawing carries, and whether the `location` field of the components is
	the `+` of the norm or the free text it has always been.

	The preview is the point of the dialog. It shows a real component of the
	open project - its tag before, its tag after - so the answer to "what
	does this change on my drawing" is on screen before the button is
	pressed. And it says out loud that turning the switch back off puts
	everything back, because the composed tag is never written into the
	component: that is the promise this dialog has to make believable.
*/
class IecStructureDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit IecStructureDialog(QETProject *project,
					    QWidget *parent = nullptr);

		IecStructureSettings settings() const;

	private slots:
		void refreshPreview();
		void apply();

	private:
		void setUpWidget();

		QETProject *m_project = nullptr;
		IecStructureSettings m_settings;

		QCheckBox *m_enabled = nullptr;
		QCheckBox *m_element_location = nullptr;
		QComboBox *m_display = nullptr;
		QLabel *m_preview = nullptr;
		QLabel *m_folio_note = nullptr;
};

#endif // IECSTRUCTUREDIALOG_H
