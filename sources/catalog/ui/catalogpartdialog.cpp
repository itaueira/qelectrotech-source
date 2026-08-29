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
#include "catalogpartdialog.h"

#include "../catalog.h"
#include "catalogbrowserdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

/**
	@brief CatalogPartDialog::CatalogPartDialog
	@param catalog
	@param part : a part with no code to create one
	@param parent
*/
CatalogPartDialog::CatalogPartDialog(Catalog *catalog,
				     const CatalogPart &part,
				     QWidget *parent) :
	QDialog(parent),
	m_catalog(catalog),
	m_part(part)
{
	setWindowTitle(m_part.id > 0 ? tr("Modifier une pièce du catalogue")
				     : tr("Nouvelle pièce du catalogue"));
	resize(700, 640);
	buildWidgets();
	buildValueEditors();
	fillPinTable();
	fillAccessoryTable();
}

/**
	@brief CatalogPartDialog::buildWidgets
*/
void CatalogPartDialog::buildWidgets()
{
	m_class = new QComboBox(this);
	if (m_catalog)
	{
		const QList<CatalogClass> classes = m_catalog->classes();
		for (const CatalogClass &catalog_class : classes)
		{
			const int depth = m_catalog->classAncestry(catalog_class.id).size() - 1;
			m_class->addItem(QString(depth * 4, QLatin1Char(' ')) + catalog_class.name,
					 catalog_class.id);
		}
		const int index = m_class->findData(m_part.class_id);
		if (index >= 0) {
			m_class->setCurrentIndex(index);
		}
	}

	m_code = new QLineEdit(m_part.code, this);
	m_code->setPlaceholderText(tr("Référence de la pièce — le seul champ obligatoire"));

	m_revision = new QLabel(m_part.id > 0
				? tr("révision %1").arg(m_part.revision)
				: tr("nouvelle pièce"), this);

	QFormLayout *head = new QFormLayout();
	head->addRow(tr("Classe"), m_class);
	head->addRow(tr("Référence"), m_code);
	head->addRow(tr("État"), m_revision);

	// --- values ---------------------------------------------------------
	m_value_container = new QWidget(this);
	m_value_form = new QFormLayout(m_value_container);

	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidget(m_value_container);
	scroll->setWidgetResizable(true);

	// --- pins -----------------------------------------------------------
	m_pins = new QTableWidget(this);
	m_pins->setColumnCount(7);
	m_pins->setHorizontalHeaderLabels({ tr("Borne"), tr("Rôle"), tr("Paire"), tr("Symbole"),
					    tr("Étiquette"), tr("Canal"), tr("Connecteur") });
	m_pins->horizontalHeader()->setStretchLastSection(true);
	m_pins->verticalHeader()->setVisible(false);
	m_pins->setSelectionBehavior(QAbstractItemView::SelectRows);

	QPushButton *add_pin = new QPushButton(tr("Ajouter"), this);
	QPushButton *remove_pin = new QPushButton(tr("Retirer"), this);
	QPushButton *pin_up = new QPushButton(tr("Monter"), this);
	QPushButton *pin_down = new QPushButton(tr("Descendre"), this);

	QHBoxLayout *pin_buttons = new QHBoxLayout();
	pin_buttons->addWidget(add_pin);
	pin_buttons->addWidget(remove_pin);
	pin_buttons->addStretch();
	pin_buttons->addWidget(pin_up);
	pin_buttons->addWidget(pin_down);

	QLabel *pin_hint = new QLabel(tr("L'ordre des bornes est celui des bornes du symbole : c'est "
					 "lui qui décide quel numéro remplace quelle étiquette "
					 "provisoire. La colonne « Symbole » sert aux pièces dessinées "
					 "en plusieurs symboles, comme une bobine et ses contacts. "
					 "La colonne « Canal » groupe les bornes d'un même point "
					 "d'entrée ou de sortie : une entrée à deux fils est un canal "
					 "de deux bornes, son entrée et son commun de retour."),
				      this);
	pin_hint->setWordWrap(true);

	QWidget *pin_tab = new QWidget(this);
	QVBoxLayout *pin_layout = new QVBoxLayout(pin_tab);
	pin_layout->addWidget(m_pins);
	pin_layout->addLayout(pin_buttons);
	pin_layout->addWidget(pin_hint);

	// --- accessories ----------------------------------------------------
	m_accessories = new QTableWidget(this);
	m_accessories->setColumnCount(2);
	m_accessories->setHorizontalHeaderLabels({ tr("Référence"), tr("Quantité") });
	m_accessories->horizontalHeader()->setStretchLastSection(true);
	m_accessories->verticalHeader()->setVisible(false);
	m_accessories->setSelectionBehavior(QAbstractItemView::SelectRows);

	QPushButton *add_accessory = new QPushButton(tr("Ajouter"), this);
	QPushButton *pick_accessory = new QPushButton(tr("Choisir dans le catalogue…"), this);
	QPushButton *remove_accessory = new QPushButton(tr("Retirer"), this);

	QHBoxLayout *accessory_buttons = new QHBoxLayout();
	accessory_buttons->addWidget(add_accessory);
	accessory_buttons->addWidget(pick_accessory);
	accessory_buttons->addWidget(remove_accessory);
	accessory_buttons->addStretch();

	QLabel *accessory_hint = new QLabel(tr("Un accessoire enregistré ici revient avec la pièce à "
					       "chaque attribution : c'est ainsi qu'un fusible reste dans "
					       "son porte-fusible. Pour qu'il ne revienne plus, le retirer "
					       "ici et enregistrer de nouveau."), this);
	accessory_hint->setWordWrap(true);

	QWidget *accessory_tab = new QWidget(this);
	QVBoxLayout *accessory_layout = new QVBoxLayout(accessory_tab);
	accessory_layout->addWidget(m_accessories);
	accessory_layout->addLayout(accessory_buttons);
	accessory_layout->addWidget(accessory_hint);

	QTabWidget *tabs = new QTabWidget(this);
	tabs->addTab(scroll, tr("Propriétés"));
	tabs->addTab(pin_tab, tr("Bornes"));
	tabs->addTab(accessory_tab, tr("Accessoires"));

	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	QPushButton *save = new QPushButton(tr("Enregistrer"), this);
	save->setDefault(true);
	QPushButton *save_revision = new QPushButton(tr("Enregistrer comme nouvelle révision"), this);
	save_revision->setToolTip(tr("À utiliser quand le fabricant a changé le produit : les projets "
				     "existants gardent la révision qu'ils avaient."));
	save_revision->setEnabled(m_part.id > 0);

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->addButton(save, QDialogButtonBox::AcceptRole);
	buttons->addButton(save_revision, QDialogButtonBox::ActionRole);
	buttons->addButton(QDialogButtonBox::Cancel);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(head);
	layout->addWidget(tabs);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	connect(m_class, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogPartDialog::classChanged);
	connect(add_pin, &QPushButton::clicked, this, &CatalogPartDialog::addPin);
	connect(remove_pin, &QPushButton::clicked, this, &CatalogPartDialog::removeSelectedPin);
	connect(pin_up, &QPushButton::clicked, this, &CatalogPartDialog::movePinUp);
	connect(pin_down, &QPushButton::clicked, this, &CatalogPartDialog::movePinDown);
	connect(add_accessory, &QPushButton::clicked, this, &CatalogPartDialog::addAccessory);
	connect(pick_accessory, &QPushButton::clicked,
		this, &CatalogPartDialog::pickAccessoryFromCatalog);
	connect(remove_accessory, &QPushButton::clicked,
		this, &CatalogPartDialog::removeSelectedAccessory);
	connect(save, &QPushButton::clicked, this, &CatalogPartDialog::saveInPlace);
	connect(save_revision, &QPushButton::clicked, this, &CatalogPartDialog::saveAsNewRevision);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief CatalogPartDialog::editorFor
	@param property
	@param value
	@return the widget that edits @a property, chosen from its type and from
	whether it offers a list
*/
QWidget *CatalogPartDialog::editorFor(const CatalogProperty &property, const QString &value)
{
	if (property.list_behaviour != CatalogListBehaviour::None)
	{
		QComboBox *combo = new QComboBox(m_value_container);
		combo->setEditable(property.list_behaviour == CatalogListBehaviour::Suggested);
		combo->addItem(QString());
		combo->addItems(property.options);
		const int index = combo->findText(value);
		if (index >= 0) {
			combo->setCurrentIndex(index);
		} else if (combo->isEditable()) {
			combo->setCurrentText(value);
		}
		return combo;
	}

	switch (property.type)
	{
		case CatalogPropertyType::Integer:
		{
			QSpinBox *spin = new QSpinBox(m_value_container);
			spin->setRange(-1000000, 1000000);
			spin->setSpecialValueText(QString());
			spin->setValue(value.toInt());
			return spin;
		}
		case CatalogPropertyType::Decimal:
		case CatalogPropertyType::Currency:
		case CatalogPropertyType::Measure:
		case CatalogPropertyType::Angle:
		{
			QDoubleSpinBox *spin = new QDoubleSpinBox(m_value_container);
			spin->setRange(-1000000.0, 1000000.0);
			spin->setDecimals(3);
			if (!property.unit.isEmpty()) {
				spin->setSuffix(QLatin1Char(' ') + property.unit);
			}
			spin->setValue(value.toDouble());
			return spin;
		}
		case CatalogPropertyType::Boolean:
		{
			QCheckBox *check = new QCheckBox(m_value_container);
			check->setChecked(value == QStringLiteral("1"));
			return check;
		}
		case CatalogPropertyType::Date:
		{
			QDateEdit *date = new QDateEdit(m_value_container);
			date->setCalendarPopup(true);
			date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
			const QDate parsed = QDate::fromString(value, Qt::ISODate);
			if (parsed.isValid()) {
				date->setDate(parsed);
			}
			return date;
		}
		default:
			break;
	}

	QLineEdit *line = new QLineEdit(value, m_value_container);
	if (property.type == CatalogPropertyType::Link) {
		line->setPlaceholderText(tr("https://…"));
	} else if (property.type == CatalogPropertyType::Image
		   || property.type == CatalogPropertyType::File
		   || property.type == CatalogPropertyType::Folder) {
		line->setPlaceholderText(tr("Chemin"));
	}
	return line;
}

/**
	@brief CatalogPartDialog::valueFrom
	@param property
	@param editor
	@return what the user left in @a editor, serialised the way the catalog
	stores it
*/
QString CatalogPartDialog::valueFrom(const CatalogProperty &property, QWidget *editor) const
{
	if (auto *combo = qobject_cast<QComboBox *>(editor)) {
		return combo->currentText();
	}
	if (auto *check = qobject_cast<QCheckBox *>(editor)) {
		return check->isChecked() ? QStringLiteral("1") : QStringLiteral("0");
	}
	if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
		return QString::number(spin->value());
	}
	if (auto *spin = qobject_cast<QDoubleSpinBox *>(editor))
	{
		// A measure left at zero means "not filled", not "zero millimetre":
		// writing a 0 would make the physical view believe it knows a width.
		if (qFuzzyIsNull(spin->value()) && !m_part.hasValue(property.key)) {
			return QString();
		}
		return QString::number(spin->value(), 'g', 10);
	}
	if (auto *date = qobject_cast<QDateEdit *>(editor)) {
		return date->date().toString(Qt::ISODate);
	}
	if (auto *line = qobject_cast<QLineEdit *>(editor)) {
		return line->text().trimmed();
	}
	return QString();
}

/**
	@brief CatalogPartDialog::buildValueEditors
	Build one editor per property the class of the part declares, inherited
	ones included, in the order the class gives them.
*/
void CatalogPartDialog::buildValueEditors()
{
	// Clear whatever the previous class had put here.
	while (m_value_form->rowCount() > 0) {
		m_value_form->removeRow(0);
	}
	m_editors.clear();

	if (!m_catalog) {
		return;
	}

	const int class_id = m_class->currentData().toInt();
	const QHash<QString, QString> values = m_catalog->effectiveValues(m_part);
	const QList<CatalogProperty> properties = m_catalog->effectiveProperties(class_id);

	for (const CatalogProperty &property : properties)
	{
		QWidget *editor = editorFor(property, values.value(property.key));
		if (!property.description.isEmpty()) {
			editor->setToolTip(property.description);
		}
		m_editors.insert(property.key, editor);
		m_value_form->addRow(property.name, editor);
	}

	if (properties.isEmpty())
	{
		m_value_form->addRow(new QLabel(tr("Cette classe ne déclare aucune propriété. "
						   "Ajoutez-en dans « Classes et propriétés du catalogue »."),
						m_value_container));
	}
}

/**
	@brief CatalogPartDialog::classChanged
*/
void CatalogPartDialog::classChanged()
{
	// Keep what the user already typed before rebuilding, so that changing
	// the class does not silently empty the form.
	CatalogPart current = m_part;
	collect(current);
	m_part.values = current.values;
	buildValueEditors();
}

/**
	@brief CatalogPartDialog::fillPinTable
*/
void CatalogPartDialog::fillPinTable()
{
	m_pins->setRowCount(m_part.pins.size());
	for (int row = 0 ; row < m_part.pins.size() ; ++row)
	{
		const CatalogPin &pin = m_part.pins.at(row);
		m_pins->setItem(row, 0, new QTableWidgetItem(pin.label));

		QComboBox *role = new QComboBox(m_pins);
		const QList<CatalogPinRole> roles = CatalogPin::allRoles();
		for (const CatalogPinRole candidate : roles) {
			role->addItem(CatalogPin::translatedRoleName(candidate),
				      CatalogPin::roleToString(candidate));
		}
		const int index = role->findData(CatalogPin::roleToString(pin.role));
		if (index >= 0) {
			role->setCurrentIndex(index);
		}
		m_pins->setCellWidget(row, 1, role);

		m_pins->setItem(row, 2, new QTableWidgetItem(pin.pair));
		m_pins->setItem(row, 3, new QTableWidgetItem(pin.group));
		m_pins->setItem(row, 4, new QTableWidgetItem(pin.secondary_label));
		m_pins->setItem(row, 5, new QTableWidgetItem(pin.channel));
		m_pins->setItem(row, 6, new QTableWidgetItem(pin.connector));
	}
	m_pins->resizeColumnsToContents();
}

/**
	@brief CatalogPartDialog::fillAccessoryTable
*/
void CatalogPartDialog::fillAccessoryTable()
{
	m_accessories->setRowCount(m_part.accessories.size());
	for (int row = 0 ; row < m_part.accessories.size() ; ++row)
	{
		const CatalogAccessory &accessory = m_part.accessories.at(row);
		m_accessories->setItem(row, 0, new QTableWidgetItem(accessory.code));
		m_accessories->setItem(row, 1,
				       new QTableWidgetItem(QString::number(accessory.quantity)));
	}
	m_accessories->resizeColumnsToContents();
}

/**
	@brief CatalogPartDialog::addPin
*/
void CatalogPartDialog::addPin()
{
	const int row = m_pins->rowCount();
	m_pins->insertRow(row);
	m_pins->setItem(row, 0, new QTableWidgetItem(QString()));

	QComboBox *role = new QComboBox(m_pins);
	const QList<CatalogPinRole> roles = CatalogPin::allRoles();
	for (const CatalogPinRole candidate : roles) {
		role->addItem(CatalogPin::translatedRoleName(candidate),
			      CatalogPin::roleToString(candidate));
	}
	m_pins->setCellWidget(row, 1, role);

	m_pins->setItem(row, 2, new QTableWidgetItem(QString()));
	m_pins->setItem(row, 3, new QTableWidgetItem(QString()));
	m_pins->setItem(row, 4, new QTableWidgetItem(QString()));
	m_pins->setItem(row, 5, new QTableWidgetItem(QString()));
	m_pins->setItem(row, 6, new QTableWidgetItem(QString()));
	m_pins->setCurrentCell(row, 0);
}

/**
	@brief CatalogPartDialog::removeSelectedPin
*/
void CatalogPartDialog::removeSelectedPin()
{
	const int row = m_pins->currentRow();
	if (row >= 0) {
		m_pins->removeRow(row);
	}
}

/**
	@brief CatalogPartDialog::movePinUp
*/
void CatalogPartDialog::movePinUp()
{
	movePin(-1);
}

/**
	@brief CatalogPartDialog::movePinDown
*/
void CatalogPartDialog::movePinDown()
{
	movePin(1);
}

/**
	@brief CatalogPartDialog::movePin
	@param offset : -1 to move up, 1 to move down
*/
void CatalogPartDialog::movePin(int offset)
{
	const int row = m_pins->currentRow();
	const int target = row + offset;
	if (row < 0 || target < 0 || target >= m_pins->rowCount()) {
		return;
	}

	// Read both rows, swap them, and rebuild through the model so that the
	// role combo boxes follow their row.
	CatalogPart temporary;
	temporary.pins = m_part.pins;
	CatalogPart collected = m_part;
	if (!collect(collected)) {
		return;
	}
	if (row >= collected.pins.size() || target >= collected.pins.size()) {
		return;
	}
	collected.pins.move(row, target);
	m_part.pins = collected.pins;
	fillPinTable();
	m_pins->setCurrentCell(target, 0);
}

/**
	@brief CatalogPartDialog::addAccessory
*/
void CatalogPartDialog::addAccessory()
{
	bool ok = false;
	const QString code = QInputDialog::getText(this, tr("Ajouter un accessoire"),
						   tr("Référence de l'accessoire"),
						   QLineEdit::Normal, QString(), &ok);
	if (!ok || code.trimmed().isEmpty()) {
		return;
	}

	const int row = m_accessories->rowCount();
	m_accessories->insertRow(row);
	m_accessories->setItem(row, 0, new QTableWidgetItem(code.trimmed()));
	m_accessories->setItem(row, 1, new QTableWidgetItem(QStringLiteral("1")));
}

/**
	@brief CatalogPartDialog::pickAccessoryFromCatalog
*/
void CatalogPartDialog::pickAccessoryFromCatalog()
{
	const CatalogPart chosen = CatalogBrowserDialog::choosePart(m_catalog, this);
	if (chosen.isNull()) {
		return;
	}

	const int row = m_accessories->rowCount();
	m_accessories->insertRow(row);
	m_accessories->setItem(row, 0, new QTableWidgetItem(chosen.code));
	m_accessories->setItem(row, 1, new QTableWidgetItem(QStringLiteral("1")));
}

/**
	@brief CatalogPartDialog::removeSelectedAccessory
*/
void CatalogPartDialog::removeSelectedAccessory()
{
	const int row = m_accessories->currentRow();
	if (row >= 0) {
		m_accessories->removeRow(row);
	}
}

/**
	@brief CatalogPartDialog::collect
	@param part : receives what the dialog holds
	@return true when the dialog holds something saveable
*/
bool CatalogPartDialog::collect(CatalogPart &part)
{
	part.class_id = m_class->currentData().toInt();
	part.code = m_code->text().trimmed();

	const QList<CatalogProperty> properties = m_catalog
						  ? m_catalog->effectiveProperties(part.class_id)
						  : QList<CatalogProperty>();
	for (const CatalogProperty &property : properties)
	{
		QWidget *editor = m_editors.value(property.key);
		if (!editor) {
			continue;
		}
		const QString value = valueFrom(property, editor);
		if (value.isEmpty()) {
			part.clearValue(property.key);
		} else {
			part.setValue(property.key, value);
		}
	}

	part.pins.clear();
	for (int row = 0 ; row < m_pins->rowCount() ; ++row)
	{
		CatalogPin pin;
		const QTableWidgetItem *label = m_pins->item(row, 0);
		pin.label = label ? label->text().trimmed() : QString();
		if (pin.label.isEmpty()) {
			continue;    // an empty line is a line the user abandoned
		}
		if (auto *role = qobject_cast<QComboBox *>(m_pins->cellWidget(row, 1))) {
			pin.role = CatalogPin::roleFromString(role->currentData().toString());
		}
		const QTableWidgetItem *pair = m_pins->item(row, 2);
		pin.pair = pair ? pair->text().trimmed() : QString();
		const QTableWidgetItem *group = m_pins->item(row, 3);
		pin.group = group ? group->text().trimmed() : QString();
		const QTableWidgetItem *secondary = m_pins->item(row, 4);
		pin.secondary_label = secondary ? secondary->text().trimmed() : QString();
		const QTableWidgetItem *channel = m_pins->item(row, 5);
		pin.channel = channel ? channel->text().trimmed() : QString();
		const QTableWidgetItem *connector = m_pins->item(row, 6);
		pin.connector = connector ? connector->text().trimmed() : QString();
		pin.order_index = part.pins.size() + 1;
		part.pins.append(pin);
	}

	part.accessories.clear();
	for (int row = 0 ; row < m_accessories->rowCount() ; ++row)
	{
		const QTableWidgetItem *code = m_accessories->item(row, 0);
		if (!code || code->text().trimmed().isEmpty()) {
			continue;
		}
		const QTableWidgetItem *quantity = m_accessories->item(row, 1);
		bool ok = false;
		const double amount = quantity ? quantity->text().toDouble(&ok) : 0.0;
		part.accessories.append(CatalogAccessory(code->text().trimmed(),
							 ok && amount > 0.0 ? amount : 1.0));
	}

	return true;
}

/**
	@brief CatalogPartDialog::save
	@param as_new_revision
	@return true when the part was saved
*/
bool CatalogPartDialog::save(bool as_new_revision)
{
	if (!m_catalog) {
		return false;
	}

	CatalogPart to_save = m_part;
	if (!collect(to_save)) {
		return false;
	}

	QString error;
	if (!to_save.isValid(&error))
	{
		QMessageBox::warning(this, tr("Pièce incomplète"), error);
		return false;
	}

	// Say which of the two ways is being taken, because they differ in what
	// happens to the projects already delivered.
	if (as_new_revision)
	{
		if (QMessageBox::question(this, tr("Nouvelle révision"),
					  tr("Créer une nouvelle révision de « %1 » ? Les projets "
					     "existants continuent sur la révision qu'ils ont ; "
					     "les nouveaux prendront celle-ci.").arg(to_save.code))
		    != QMessageBox::Yes)
		{
			return false;
		}
		if (!m_catalog->savePartAsNewRevision(to_save, &error))
		{
			QMessageBox::warning(this, tr("Pièce non enregistrée"), error);
			return false;
		}
	}
	else
	{
		if (m_part.id > 0
		    && QMessageBox::question(this, tr("Modifier la pièce"),
					     tr("Enregistrer « %1 » en place ? Tous les projets qui "
						"utilisent cette pièce verront la correction à leur "
						"prochaine ouverture. Pour un produit qui a changé, "
						"utilisez plutôt une nouvelle révision.")
					     .arg(to_save.code))
		       != QMessageBox::Yes)
		{
			return false;
		}
		if (!m_catalog->savePart(to_save, &error))
		{
			QMessageBox::warning(this, tr("Pièce non enregistrée"), error);
			return false;
		}
	}

	m_part = to_save;
	return true;
}

/**
	@brief CatalogPartDialog::saveInPlace
*/
void CatalogPartDialog::saveInPlace()
{
	if (save(false)) {
		accept();
	}
}

/**
	@brief CatalogPartDialog::saveAsNewRevision
*/
void CatalogPartDialog::saveAsNewRevision()
{
	if (save(true)) {
		accept();
	}
}

/**
	@brief CatalogPartDialog::part
	@return the part as it was saved
*/
CatalogPart CatalogPartDialog::part() const
{
	return m_part;
}
