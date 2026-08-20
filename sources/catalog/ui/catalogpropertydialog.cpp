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
#include "catalogpropertydialog.h"

#include "../catalog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

/**
	@brief CatalogPropertyDialog::CatalogPropertyDialog
	@param catalog : the catalog the property belongs to, for the lists
	@param property : the property to edit, a fresh one to create
	@param parent
*/
CatalogPropertyDialog::CatalogPropertyDialog(Catalog *catalog,
					     const CatalogProperty &property,
					     QWidget *parent) :
	QDialog(parent),
	m_catalog(catalog),
	m_property(property)
{
	// An existing property keeps its key: the values of the parts are stored
	// under it, and renaming the property must not orphan them.
	m_key_is_frozen = m_property.id > 0;

	setWindowTitle(m_property.id > 0 ? tr("Modifier une propriété")
					 : tr("Nouvelle propriété"));
	buildWidgets();
	fillFromProperty();
	listBehaviourChanged();
}

/**
	@brief CatalogPropertyDialog::buildWidgets
*/
void CatalogPropertyDialog::buildWidgets()
{
	m_name = new QLineEdit(this);
	m_key  = new QLineEdit(this);
	m_key->setToolTip(tr("Clé technique, utilisée dans le fichier projet et dans les listes. "
			     "Elle ne change plus une fois la propriété enregistrée."));

	m_type = new QComboBox(this);
	const QList<CatalogPropertyType> types = CatalogProperty::allTypes();
	for (const CatalogPropertyType type : types)
	{
		m_type->addItem(CatalogProperty::translatedTypeName(type),
				CatalogProperty::typeToString(type));
	}

	m_list_behaviour = new QComboBox(this);
	m_list_behaviour->addItem(CatalogProperty::translatedListBehaviourName(CatalogListBehaviour::None),
				  CatalogProperty::listBehaviourToString(CatalogListBehaviour::None));
	m_list_behaviour->addItem(CatalogProperty::translatedListBehaviourName(CatalogListBehaviour::Suggested),
				  CatalogProperty::listBehaviourToString(CatalogListBehaviour::Suggested));
	m_list_behaviour->addItem(CatalogProperty::translatedListBehaviourName(CatalogListBehaviour::Mandatory),
				  CatalogProperty::listBehaviourToString(CatalogListBehaviour::Mandatory));

	m_list_name = new QComboBox(this);
	m_list_name->addItem(tr("(valeurs saisies ici)"), QString());
	if (m_catalog)
	{
		const QStringList names = m_catalog->listNames();
		for (const QString &name : names) {
			m_list_name->addItem(name, name);
		}
	}
	m_list_name->setToolTip(tr("Une liste contrôlée est partagée par plusieurs propriétés : "
				   "les fabricants se tiennent à jour en un seul endroit."));

	m_inline_options = new QPlainTextEdit(this);
	m_inline_options->setPlaceholderText(tr("Une valeur par ligne"));
	m_inline_options->setMaximumHeight(90);
	m_inline_options_label = new QLabel(tr("Valeurs"), this);

	m_default_value = new QLineEdit(this);
	m_unit = new QLineEdit(this);
	m_description = new QLineEdit(this);

	m_apply_to_existing = new QCheckBox(tr("Appliquer la valeur initiale aux pièces existantes"), this);
	m_apply_to_existing->setToolTip(tr("Écrit la valeur initiale dans les pièces qui n'ont jamais "
					   "eu ce champ renseigné. Les valeurs déjà saisies ne sont pas touchées."));

	QFormLayout *form = new QFormLayout();
	form->addRow(tr("Nom"), m_name);
	form->addRow(tr("Clé"), m_key);
	form->addRow(tr("Type"), m_type);
	form->addRow(tr("Comportement de liste"), m_list_behaviour);
	form->addRow(tr("Liste contrôlée"), m_list_name);
	form->addRow(m_inline_options_label, m_inline_options);
	form->addRow(tr("Valeur initiale"), m_default_value);
	form->addRow(tr("Unité"), m_unit);
	form->addRow(tr("Description"), m_description);
	form->addRow(QString(), m_apply_to_existing);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
							 this);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(buttons);

	connect(m_name, &QLineEdit::textEdited, this, &CatalogPropertyDialog::nameEdited);
	// QOverload because Qt5 still carries the deprecated QString overload of
	// currentIndexChanged, which makes the plain member pointer ambiguous.
	connect(m_list_behaviour, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogPropertyDialog::listBehaviourChanged);
	connect(m_list_name, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogPropertyDialog::listBehaviourChanged);
	connect(buttons, &QDialogButtonBox::accepted,
		this, &CatalogPropertyDialog::validateAndAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief CatalogPropertyDialog::fillFromProperty
*/
void CatalogPropertyDialog::fillFromProperty()
{
	m_name->setText(m_property.name);
	m_key->setText(m_property.key);
	m_key->setReadOnly(m_key_is_frozen);

	const int type_index = m_type->findData(CatalogProperty::typeToString(m_property.type));
	if (type_index >= 0) {
		m_type->setCurrentIndex(type_index);
	}

	const int behaviour_index =
		m_list_behaviour->findData(CatalogProperty::listBehaviourToString(m_property.list_behaviour));
	if (behaviour_index >= 0) {
		m_list_behaviour->setCurrentIndex(behaviour_index);
	}

	const int list_index = m_list_name->findData(m_property.list_name);
	m_list_name->setCurrentIndex(list_index >= 0 ? list_index : 0);

	if (m_property.list_name.isEmpty()) {
		m_inline_options->setPlainText(m_property.options.join(QLatin1Char('\n')));
	}

	m_default_value->setText(m_property.default_value);
	m_unit->setText(m_property.unit);
	m_description->setText(m_property.description);
}

/**
	@brief CatalogPropertyDialog::nameEdited
	@param name
*/
void CatalogPropertyDialog::nameEdited(const QString &name)
{
	if (!m_key_is_frozen) {
		m_key->setText(CatalogProperty::keyFromName(name));
	}
}

/**
	@brief CatalogPropertyDialog::listBehaviourChanged
	Show the value editor only when there is a list to fill, and only when
	the values are not coming from a controlled list.
*/
void CatalogPropertyDialog::listBehaviourChanged()
{
	const CatalogListBehaviour behaviour =
		CatalogProperty::listBehaviourFromString(m_list_behaviour->currentData().toString());
	const bool has_list = behaviour != CatalogListBehaviour::None;
	const bool uses_controlled_list = !m_list_name->currentData().toString().isEmpty();

	m_list_name->setEnabled(has_list);
	m_inline_options->setVisible(has_list && !uses_controlled_list);
	m_inline_options_label->setVisible(has_list && !uses_controlled_list);
}

/**
	@brief CatalogPropertyDialog::validateAndAccept
*/
void CatalogPropertyDialog::validateAndAccept()
{
	CatalogProperty edited = m_property;
	edited.name           = m_name->text().trimmed();
	edited.key            = m_key->text().trimmed();
	edited.type           = CatalogProperty::typeFromString(m_type->currentData().toString());
	edited.list_behaviour = CatalogProperty::listBehaviourFromString(m_list_behaviour->currentData().toString());
	edited.list_name      = m_list_name->currentData().toString();
	edited.default_value  = m_default_value->text();
	edited.unit           = m_unit->text().trimmed();
	edited.description    = m_description->text();

	if (edited.list_behaviour == CatalogListBehaviour::None)
	{
		edited.list_name.clear();
		edited.options.clear();
	}
	else if (edited.list_name.isEmpty())
	{
		edited.options = m_inline_options->toPlainText()
				 .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
		for (QString &option : edited.options) {
			option = option.trimmed();
		}
		edited.options.removeAll(QString());
	}
	else if (m_catalog)
	{
		edited.options = m_catalog->listValues(edited.list_name);
	}

	QString error;
	if (!edited.isValid(&error))
	{
		QMessageBox::warning(this, tr("Propriété incomplète"), error);
		return;
	}

	m_property = edited;
	accept();
}

/**
	@brief CatalogPropertyDialog::property
	@return the property as the user left it
*/
CatalogProperty CatalogPropertyDialog::property() const
{
	return m_property;
}

/**
	@brief CatalogPropertyDialog::applyToExistingParts
	@return true when the user asked for the initial value to reach the parts
	that already exist
*/
bool CatalogPropertyDialog::applyToExistingParts() const
{
	return m_apply_to_existing->isChecked();
}
