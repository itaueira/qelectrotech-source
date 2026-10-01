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
#include "imageeditor.h"

#include "../../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../elementscene.h"
#include "../graphicspart/partimage.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QUndoStack>
#include <QVBoxLayout>

namespace
{
	QDoubleSpinBox *millimetreField(const QString &name, qreal minimum, QWidget *parent)
	{
		auto *field = new QDoubleSpinBox(parent);
		field->setObjectName(name);
		field->setDecimals(2);
		field->setRange(minimum, 100000.0);
		field->setSingleStep(0.5);
		return field;
	}

		/// The stack of the scene the image is in. The one a part gives,
		/// undoStack(), is that of the tab shown, and an image belongs to
		/// the 2D tab whichever tab is shown.
	void push(PartImage *image, QUndoCommand *command)
	{
		if (auto *scene = dynamic_cast<ElementScene *>(image->scene())) {
			scene->undoStack().push(command);
		}
		else {
			command->redo();
			delete command;
		}
	}
}

/**
	@brief ImageEditor::ImageEditor
	@param editor the element editor this belongs to
	@param parent parent widget
*/
ImageEditor::ImageEditor(QETElementEditor *editor, QWidget *parent) :
	ElementItemEditor(editor, parent)
{
	m_x_sb      = millimetreField(QStringLiteral("m_x_sb"), -100000.0, this);
	m_y_sb      = millimetreField(QStringLiteral("m_y_sb"), -100000.0, this);
	m_width_sb  = millimetreField(QStringLiteral("m_width_sb"), 0.01, this);
	m_height_sb = millimetreField(QStringLiteral("m_height_sb"), 0.01, this);

	m_proportion_button = new QPushButton(tr("Reprendre les proportions du fichier"), this);
	m_proportion_button->setToolTip(tr("Garde la largeur et recalcule la hauteur dans les proportions du fichier."));
	m_file_label = new QLabel(this);
	m_file_label->setWordWrap(true);

	auto *form = new QFormLayout();
	form->addRow(tr("X :"), m_x_sb);
	form->addRow(tr("Y :"), m_y_sb);
	form->addRow(tr("Largeur :"), m_width_sb);
	form->addRow(tr("Hauteur :"), m_height_sb);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(m_proportion_button);
	layout->addWidget(m_file_label);
	layout->addStretch();

	for (QDoubleSpinBox *field : {m_x_sb, m_y_sb, m_width_sb, m_height_sb}) {
		connect(field, &QDoubleSpinBox::editingFinished, this, &ImageEditor::apply);
	}
	connect(m_proportion_button, &QPushButton::clicked, this, &ImageEditor::restoreFileProportion);
}

/**
	@brief ImageEditor::~ImageEditor
*/
ImageEditor::~ImageEditor()
{
	disconnectChangeConnections();
}

bool ImageEditor::setPart(CustomElementPart *part)
{
	if (!part)
	{
		disconnectChangeConnections();
		m_part.clear();
		return true;
	}

	auto *image = dynamic_cast<PartImage *>(part);
	if (!image) {
		return false;
	}
	if (m_part == image) {
		return true;
	}

	disconnectChangeConnections();
	m_part = image;
	updateForm();
	m_change_connections << connect(image, &PartImage::rectChanged, this, &ImageEditor::updateForm);
	m_change_connections << connect(image, &PartImage::xChanged,    this, &ImageEditor::updateForm);
	m_change_connections << connect(image, &PartImage::yChanged,    this, &ImageEditor::updateForm);
	return true;
}

bool ImageEditor::setParts(QList<CustomElementPart *> parts)
{
		//One image at a time: their fields are positions and sizes, and
		//several images share neither
	if (parts.size() != 1) {
		return false;
	}
	return setPart(parts.first());
}

CustomElementPart *ImageEditor::currentPart() const
{
	return m_part.data();
}

QList<CustomElementPart *> ImageEditor::currentParts() const
{
	if (!m_part) {
		return {};
	}
	return {m_part.data()};
}

/**
	@brief ImageEditor::updateForm
	The fields read back from the image, without writing anything.
*/
void ImageEditor::updateForm()
{
	if (!m_part) {
		return;
	}
	m_locked = true;
	const QRectF rect = m_part->rect();
	const QPointF top_left = m_part->mapToScene(rect.topLeft());
	m_x_sb->setValue(top_left.x());
	m_y_sb->setValue(top_left.y());
	m_width_sb->setValue(rect.width());
	m_height_sb->setValue(rect.height());

	if (m_part->isNull())
	{
		m_file_label->setText(tr("Cette image ne peut pas être affichée ; "
					 "elle est gardée telle quelle dans le symbole."));
		m_proportion_button->setEnabled(false);
	}
	else
	{
		m_file_label->setText(tr("Image %1 incorporée au symbole : elle le suit "
					 "dans chaque projet, sans dépendre du fichier d'origine.")
				      .arg(m_part->fileFormat().toUpper()));
		m_proportion_button->setEnabled(true);
	}
	m_locked = false;
}

/**
	@brief ImageEditor::apply
	The four fields as one step of undo: a position or a size typed in is
	one change, whichever field it was typed in.
*/
void ImageEditor::apply()
{
	if (m_locked || !m_part) {
		return;
	}
	const QPointF top_left = m_part->mapFromScene(m_x_sb->value(), m_y_sb->value());
	const QRectF wanted(top_left, QSizeF(m_width_sb->value(), m_height_sb->value()));
	if (wanted == m_part->rect()) {
		return;
	}
	auto *undo = new QPropertyUndoCommand(m_part.data(), "rect", m_part->rect(), wanted);
	undo->setText(tr("Modifier une image"));
	push(m_part.data(), undo);
}

/**
	@brief ImageEditor::restoreFileProportion
	Keep the width, and give the height the proportion of the file.
*/
void ImageEditor::restoreFileProportion()
{
	if (!m_part) {
		return;
	}
	const QSizeF file = PartImage::fileSizeMm(m_part->fileData(), m_part->fileFormat());
	if (file.isEmpty()) {
		return;
	}
	QRectF wanted = m_part->rect();
	wanted.setHeight(wanted.width() * file.height() / file.width());
	if (wanted == m_part->rect()) {
		return;
	}
	auto *undo = new QPropertyUndoCommand(m_part.data(), "rect", m_part->rect(), wanted);
	undo->setText(tr("Modifier une image"));
	push(m_part.data(), undo);
}

void ImageEditor::disconnectChangeConnections()
{
	for (const QMetaObject::Connection &connection : std::as_const(m_change_connections)) {
		disconnect(connection);
	}
	m_change_connections.clear();
}
