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
#ifndef PINOUTGENERATORDIALOG_H
#define PINOUTGENERATORDIALOG_H

#include <QDialog>

#include "../../catalog/catalogclass.h"
#include "../../catalog/catalogpart.h"
#include "../pinoutblocktemplate.h"
#include "../symbolbuilder.h"

class Catalog;
class QETProject;
class SymbolPreview;

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

/**
	@brief Draws the symbol of a part from the pins the catalogue knows,
	instead of it being drawn by hand.

	Everything the drawing needs is already written down somewhere: the pins
	are in the catalogue, the side each kind of pin goes to is the convention
	of the workshop, and how wide and how far apart is the template of the
	class. What is left for a person to decide is the three things nobody
	else can know — which part, what the component is called, and which of
	its points this block carries — and those three are what this dialog
	asks.

	It refuses rather than reports. A terminal already drawn somewhere in the
	project cannot be drawn a second time, and the moment to say so is before
	the block exists, not when the terminal list comes out with two lines for
	the same screw: by then one of the two has been wired and there is no way
	to tell which.

	The template of the class can be changed from here, and doing so changes
	the blocks generated from here on. The cards already inserted keep the
	drawing they were born with, because they are files and not views of the
	template. That is the behaviour asked for, and the dialog says it out
	loud - a right behaviour nobody was told about reads as a bug.
*/
class PinoutGeneratorDialog : public QDialog
{
	Q_OBJECT

	public:
		/**
			@param catalog the parts and the classes, may be nullptr
			@param project what is already drawn, and therefore what
			a new block may not draw again; may be nullptr, and then
			only what a block repeats inside itself is refused
		*/
		PinoutGeneratorDialog(Catalog *catalog,
				      QETProject *project = nullptr,
				      QWidget *parent = nullptr);

		/// the blocks as the dialog left them
		QList<SymbolDefinition> blocks() const;
		/// the files that were written, empty when nothing was
		QStringList savedPaths() const;

	private slots:
		void choosePart();
		void checkEveryPin();
		void checkNoPin();
		void pinsChanged();
		void templateChanged();
		void saveTemplateIntoClass();
		void blockSelected(int index);
		void chooseFolder();
		void save();

	private:
		void setUpWidget();
		void fillPins();
		/// the labels that are ticked, empty when every one of them is
		QStringList selectedPinLabels() const;
		void readTemplate();
		void showTemplate();
		void regenerate();
		void describeBlocks();
		void refreshRefusal();
		/// the folder the blocks go in, created if it does not exist yet
		QString targetFolder() const;

		Catalog *m_catalog = nullptr;
		QETProject *m_project = nullptr;
		CatalogPart m_part;
		CatalogClass m_class;
		SymbolGrid m_grid;
		PinoutBlockTemplate m_template;
		PinoutConvention m_convention;
		QList<SymbolDefinition> m_blocks;
		QStringList m_saved_paths;
		bool m_filling = false;

		QLabel *m_part_label = nullptr;
		QPushButton *m_part_button = nullptr;
		QLabel *m_class_label = nullptr;
		QLineEdit *m_component = nullptr;
		QTableWidget *m_pins = nullptr;
		QPushButton *m_check_all = nullptr;
		QPushButton *m_check_none = nullptr;
		QSpinBox *m_width = nullptr;
		QSpinBox *m_pitch = nullptr;
		QSpinBox *m_margin = nullptr;
		QSpinBox *m_max_terminals = nullptr;
		QLabel *m_template_note = nullptr;
		QPushButton *m_save_template = nullptr;
		QLabel *m_convention_note = nullptr;
		QComboBox *m_block = nullptr;
		SymbolPreview *m_preview = nullptr;
		QLineEdit *m_folder = nullptr;
		QPushButton *m_folder_button = nullptr;
		QLabel *m_refusal = nullptr;
		QLabel *m_summary = nullptr;
		QPushButton *m_save = nullptr;
};

#endif // PINOUTGENERATORDIALOG_H
