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
#include "catalogmanagerdialog.h"

#include "../catalog.h"
#include "../../autoNum/numberingformat.h"
#include "../catalogclasspackage.h"
#include "../catalogschema.h"
#include "catalogpropertydialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
	/// Role holding the identifier of the catalog object an item stands for.
	const int ID_ROLE = Qt::UserRole + 1;
}

/**
	@brief CatalogManagerDialog::CatalogManagerDialog
	@param catalog
	@param parent
*/
CatalogManagerDialog::CatalogManagerDialog(Catalog *catalog, QWidget *parent) :
	QDialog(parent),
	m_catalog(catalog)
{
	setWindowTitle(tr("Catalogue : classes et propriétés"));
	resize(900, 600);
	buildWidgets();
	reloadClassTree();
	reloadLists();
	updateEnabledState();
}

/**
	@brief CatalogManagerDialog::buildWidgets
*/
void CatalogManagerDialog::buildWidgets()
{
	// --- the class tree -------------------------------------------------
	m_class_tree = new QTreeWidget(this);
	m_class_tree->setColumnCount(3);
	m_class_tree->setHeaderLabels({ tr("Classe"), tr("Racine"), tr("Racine CEI") });
	m_class_tree->setUniformRowHeights(true);

	m_add_root_class = new QPushButton(tr("Nouvelle classe"), this);
	m_add_subclass   = new QPushButton(tr("Nouvelle sous-classe"), this);
	m_remove_class   = new QPushButton(tr("Supprimer"), this);
	m_export_class   = new QPushButton(tr("Exporter la branche…"), this);
	m_import_class   = new QPushButton(tr("Importer une branche…"), this);
	m_export_class->setToolTip(tr("Écrire la classe sélectionnée, ce qui est sous elle "
				      "et les propriétés déclarées dans un fichier."));
	m_import_class->setToolTip(tr("Lire un fichier de classes. Ce qui existe déjà ici "
				      "n'est pas modifié."));

	QHBoxLayout *class_buttons = new QHBoxLayout();
	class_buttons->addWidget(m_add_root_class);
	class_buttons->addWidget(m_add_subclass);
	class_buttons->addWidget(m_remove_class);

		//Second row on purpose: carrying a branch from one catalogue to
		//another is not the same kind of gesture as editing the tree here.
	QHBoxLayout *branch_buttons = new QHBoxLayout();
	branch_buttons->addWidget(m_export_class);
	branch_buttons->addWidget(m_import_class);
	branch_buttons->addStretch();

	QWidget *left = new QWidget(this);
	QVBoxLayout *left_layout = new QVBoxLayout(left);
	left_layout->setContentsMargins(0, 0, 0, 0);
	left_layout->addWidget(m_class_tree);
	left_layout->addLayout(class_buttons);
	left_layout->addLayout(branch_buttons);

	// --- the class tab --------------------------------------------------
	m_class_name        = new QLineEdit(this);
	m_class_key         = new QLineEdit(this);
	m_class_key->setReadOnly(true);
	m_class_description = new QLineEdit(this);
	m_class_root        = new QLineEdit(this);
	m_class_root->setToolTip(tr("Lettre du repère dans le standard de la maison, par exemple MTR "
				    "pour un moteur. La changer touche tous les objets de la classe."));
	m_class_root_iec    = new QLineEdit(this);
	m_class_root_iec->setToolTip(tr("Lettre du repère selon la CEI 81346, par exemple M pour un moteur."));
	m_class_has_symbol  = new QCheckBox(tr("Les objets de cette classe ont un symbole de schéma"), this);
	m_class_has_symbol->setToolTip(tr("À décocher pour une pièce qui n'a jamais de symbole : butée de "
					  "bornier, poignée de porte, étiquette de bouton, fusible."));
	m_class_numbering   = new QComboBox(this);
	m_class_numbering->addItem(tr("(le format proposé par la renumérotation)"), QString());
	{
		const QList<NumberingFormat> formats = NumberingFormat::builtinFormats();
		for (const NumberingFormat &format : formats) {
			m_class_numbering->addItem(format.name + QStringLiteral("  —  ") + format.pattern,
						   format.toXml());
		}
	}
	m_class_numbering->setToolTip(tr("La règle de numérotation des objets de cette classe. Elle "
					 "vit ici et non dans la commande, pour que deux personnes "
					 "renumérotant le même projet obtiennent la même chose. Une "
					 "sous-classe qui ne dit rien suit sa classe mère, comme pour "
					 "la racine du repère."));
	m_apply_class       = new QPushButton(tr("Appliquer"), this);

	QWidget *class_tab = new QWidget(this);
	QFormLayout *class_form = new QFormLayout(class_tab);
	class_form->addRow(tr("Nom"), m_class_name);
	class_form->addRow(tr("Clé"), m_class_key);
	class_form->addRow(tr("Description"), m_class_description);
	class_form->addRow(tr("Racine du repère"), m_class_root);
	class_form->addRow(tr("Racine CEI 81346"), m_class_root_iec);
	class_form->addRow(QString(), m_class_has_symbol);
	class_form->addRow(tr("Format de numérotation"), m_class_numbering);
	class_form->addRow(QString(), m_apply_class);

	// --- the properties tab ---------------------------------------------
	m_property_table = new QTableWidget(this);
	m_property_table->setColumnCount(6);
	m_property_table->setHorizontalHeaderLabels({ tr("Nom"), tr("Clé"), tr("Type"),
						      tr("Liste"), tr("Valeur initiale"),
						      tr("Déclarée sur") });
	m_property_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_property_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_property_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_property_table->verticalHeader()->setVisible(false);
	m_property_table->horizontalHeader()->setStretchLastSection(true);

	m_add_property       = new QPushButton(tr("Nouvelle propriété"), this);
	m_edit_property      = new QPushButton(tr("Modifier"), this);
	m_remove_property    = new QPushButton(tr("Supprimer"), this);
	m_move_property_up   = new QPushButton(tr("Monter"), this);
	m_move_property_down = new QPushButton(tr("Descendre"), this);

	QHBoxLayout *property_buttons = new QHBoxLayout();
	property_buttons->addWidget(m_add_property);
	property_buttons->addWidget(m_edit_property);
	property_buttons->addWidget(m_remove_property);
	property_buttons->addStretch();
	property_buttons->addWidget(m_move_property_up);
	property_buttons->addWidget(m_move_property_down);

	QLabel *property_hint = new QLabel(tr("L'ordre des propriétés est l'ordre des colonnes dans les "
					      "listes de pièces. Une propriété déclarée sur une classe mère "
					      "apparaît ici en lecture seule : elle se modifie sur sa classe."),
					  this);
	property_hint->setWordWrap(true);

	QWidget *property_tab = new QWidget(this);
	QVBoxLayout *property_layout = new QVBoxLayout(property_tab);
	property_layout->addWidget(m_property_table);
	property_layout->addLayout(property_buttons);
	property_layout->addWidget(property_hint);

	// --- the controlled lists tab ---------------------------------------
	m_list_names  = new QListWidget(this);
	m_list_values = new QPlainTextEdit(this);
	m_list_values->setPlaceholderText(tr("Une valeur par ligne"));
	m_add_list    = new QPushButton(tr("Nouvelle liste"), this);
	m_remove_list = new QPushButton(tr("Supprimer la liste"), this);
	m_save_list   = new QPushButton(tr("Enregistrer les valeurs"), this);

	QVBoxLayout *list_left = new QVBoxLayout();
	list_left->addWidget(m_list_names);
	list_left->addWidget(m_add_list);
	list_left->addWidget(m_remove_list);

	QVBoxLayout *list_right = new QVBoxLayout();
	list_right->addWidget(m_list_values);
	list_right->addWidget(m_save_list);

	QWidget *list_tab = new QWidget(this);
	QHBoxLayout *list_layout = new QHBoxLayout(list_tab);
	list_layout->addLayout(list_left, 1);
	list_layout->addLayout(list_right, 2);

	QTabWidget *tabs = new QTabWidget(this);
	tabs->addTab(class_tab, tr("Classe"));
	tabs->addTab(property_tab, tr("Propriétés"));
	tabs->addTab(list_tab, tr("Listes contrôlées"));

	QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(left);
	splitter->addWidget(tabs);
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 2);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	if (m_catalog)
	{
		const QString where = m_catalog->filePath().isEmpty()
				      ? tr("en mémoire")
				      : m_catalog->filePath();
		m_status->setText(tr("Catalogue : %1 — schéma v%2 — %3")
				  .arg(where)
				  .arg(m_catalog->schemaVersion())
				  .arg(m_catalog->isWritable() ? tr("écriture autorisée")
							       : tr("lecture seule")));
	}
	else
	{
		m_status->setText(tr("Aucun catalogue ouvert."));
	}

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(splitter);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	connect(m_class_tree, &QTreeWidget::itemSelectionChanged,
		this, &CatalogManagerDialog::classSelected);
	connect(m_add_root_class, &QPushButton::clicked, this, &CatalogManagerDialog::addRootClass);
	connect(m_add_subclass, &QPushButton::clicked, this, &CatalogManagerDialog::addSubclass);
	connect(m_remove_class, &QPushButton::clicked, this, &CatalogManagerDialog::removeSelectedClass);
	connect(m_export_class, &QPushButton::clicked, this, &CatalogManagerDialog::exportClassBranch);
	connect(m_import_class, &QPushButton::clicked, this, &CatalogManagerDialog::importClassBranch);
	connect(m_apply_class, &QPushButton::clicked, this, &CatalogManagerDialog::applyClassChanges);

	connect(m_add_property, &QPushButton::clicked, this, &CatalogManagerDialog::addProperty);
	connect(m_edit_property, &QPushButton::clicked, this, &CatalogManagerDialog::editSelectedProperty);
	connect(m_property_table, &QTableWidget::doubleClicked,
		this, &CatalogManagerDialog::editSelectedProperty);
	connect(m_remove_property, &QPushButton::clicked, this, &CatalogManagerDialog::removeSelectedProperty);
	connect(m_move_property_up, &QPushButton::clicked, this, &CatalogManagerDialog::movePropertyUp);
	connect(m_move_property_down, &QPushButton::clicked, this, &CatalogManagerDialog::movePropertyDown);
	connect(m_property_table, &QTableWidget::itemSelectionChanged,
		this, &CatalogManagerDialog::updateEnabledState);

	connect(m_list_names, &QListWidget::itemSelectionChanged,
		this, &CatalogManagerDialog::listSelected);
	connect(m_add_list, &QPushButton::clicked, this, &CatalogManagerDialog::addList);
	connect(m_remove_list, &QPushButton::clicked, this, &CatalogManagerDialog::removeSelectedList);
	connect(m_save_list, &QPushButton::clicked, this, &CatalogManagerDialog::saveListValues);

	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
}

/**
	@brief CatalogManagerDialog::appendClassItem
	@param parent_item : nullptr for a class at the top of the tree
	@param class_id
*/
void CatalogManagerDialog::appendClassItem(QTreeWidgetItem *parent_item, int class_id)
{
	const CatalogClass catalog_class = m_catalog->classById(class_id);
	QTreeWidgetItem *item = parent_item ? new QTreeWidgetItem(parent_item)
					    : new QTreeWidgetItem(m_class_tree);
	item->setText(0, catalog_class.name);
	item->setText(1, catalog_class.root);
	item->setText(2, catalog_class.root_iec);
	item->setData(0, ID_ROLE, class_id);

	const QList<CatalogClass> children = m_catalog->childClasses(class_id);
	for (const CatalogClass &child : children) {
		appendClassItem(item, child.id);
	}
}

/**
	@brief CatalogManagerDialog::reloadClassTree
*/
void CatalogManagerDialog::reloadClassTree()
{
	if (!m_catalog) {
		return;
	}

	const int previous = selectedClassId();

	m_class_tree->clear();
	const QList<CatalogClass> roots = m_catalog->childClasses(0);
	for (const CatalogClass &root : roots) {
		appendClassItem(nullptr, root.id);
	}
	m_class_tree->expandAll();
	m_class_tree->resizeColumnToContents(0);

	if (previous > 0)
	{
		// Put the cursor back where it was, so that editing a class does
		// not send the user back to the top of the tree.
		const QList<QTreeWidgetItem *> items =
			m_class_tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive, 0);
		for (QTreeWidgetItem *item : items)
		{
			if (item->data(0, ID_ROLE).toInt() == previous)
			{
				m_class_tree->setCurrentItem(item);
				break;
			}
		}
	}
}

/**
	@brief CatalogManagerDialog::selectedClassId
	@return the class the user selected, 0 when none
*/
int CatalogManagerDialog::selectedClassId() const
{
	const QTreeWidgetItem *item = m_class_tree->currentItem();
	return item ? item->data(0, ID_ROLE).toInt() : 0;
}

/**
	@brief CatalogManagerDialog::selectedPropertyId
	@return the property the user selected, 0 when none
*/
int CatalogManagerDialog::selectedPropertyId() const
{
	const int row = m_property_table->currentRow();
	if (row < 0) {
		return 0;
	}
	const QTableWidgetItem *item = m_property_table->item(row, 0);
	return item ? item->data(ID_ROLE).toInt() : 0;
}

/**
	@brief CatalogManagerDialog::classSelected
*/
void CatalogManagerDialog::classSelected()
{
	const int class_id = selectedClassId();
	const CatalogClass catalog_class = m_catalog ? m_catalog->classById(class_id)
						     : CatalogClass();

	m_class_name->setText(catalog_class.name);
	m_class_key->setText(catalog_class.key);
	m_class_description->setText(catalog_class.description);
	m_class_root->setText(catalog_class.root);
	m_class_root_iec->setText(catalog_class.root_iec);
	m_class_has_symbol->setChecked(catalog_class.has_symbol);

	// The stored format is matched by name, not by the whole document: a
	// format whose pattern was edited elsewhere still selects its own row.
	int numbering_index = 0;
	if (!catalog_class.numbering_format.isEmpty())
	{
		const QString stored_name =
			NumberingFormat::fromXml(catalog_class.numbering_format).name;
		for (int index = 1 ; index < m_class_numbering->count() ; ++index)
		{
			const QString candidate =
				NumberingFormat::fromXml(m_class_numbering->itemData(index).toString()).name;
			if (candidate == stored_name)
			{
				numbering_index = index;
				break;
			}
		}
	}
	m_class_numbering->setCurrentIndex(numbering_index);

	reloadPropertyTable();
	updateEnabledState();
}

/**
	@brief CatalogManagerDialog::reloadPropertyTable
	Show the properties of the selected class, the inherited ones included
	and marked as such: the user has to see what a class already has before
	adding a field that duplicates it.
*/
void CatalogManagerDialog::reloadPropertyTable()
{
	m_property_table->setRowCount(0);
	const int class_id = selectedClassId();
	if (!m_catalog || class_id <= 0) {
		return;
	}

	const QList<CatalogProperty> properties = m_catalog->effectiveProperties(class_id);
	m_property_table->setRowCount(properties.size());

	for (int row = 0 ; row < properties.size() ; ++row)
	{
		const CatalogProperty &property = properties.at(row);
		const bool inherited = property.class_id != class_id;
		const CatalogClass owner = m_catalog->classById(property.class_id);

		QString list_description;
		if (property.list_behaviour != CatalogListBehaviour::None)
		{
			list_description = CatalogProperty::translatedListBehaviourName(property.list_behaviour);
			if (!property.list_name.isEmpty()) {
				list_description += QStringLiteral(" — ") + property.list_name;
			}
		}

		const QStringList texts = { property.name,
					    property.key,
					    CatalogProperty::translatedTypeName(property.type),
					    list_description,
					    property.default_value,
					    inherited ? owner.name : QString() };

		for (int column = 0 ; column < texts.size() ; ++column)
		{
			QTableWidgetItem *item = new QTableWidgetItem(texts.at(column));
			if (inherited)
			{
				QFont font = item->font();
				font.setItalic(true);
				item->setFont(font);
			}
			if (column == 0)
			{
				item->setData(ID_ROLE, property.id);
				item->setData(Qt::UserRole + 2, inherited);
			}
			m_property_table->setItem(row, column, item);
		}
	}

	m_property_table->resizeColumnsToContents();
}

/**
	@brief CatalogManagerDialog::updateEnabledState
*/
void CatalogManagerDialog::updateEnabledState()
{
	const bool writable = m_catalog && m_catalog->isWritable();
	const int class_id = selectedClassId();
	const bool has_class = class_id > 0;

	const int row = m_property_table->currentRow();
	bool property_is_own = false;
	if (row >= 0)
	{
		const QTableWidgetItem *item = m_property_table->item(row, 0);
		property_is_own = item && !item->data(Qt::UserRole + 2).toBool();
	}

	m_add_root_class->setEnabled(writable);
	m_add_subclass->setEnabled(writable && has_class);
	m_remove_class->setEnabled(writable && has_class);
		//Exporting only reads: a read-only catalogue exports fine.
	m_export_class->setEnabled(has_class);
	m_import_class->setEnabled(writable);
	m_apply_class->setEnabled(writable && has_class);

	m_add_property->setEnabled(writable && has_class);
	m_edit_property->setEnabled(writable && property_is_own);
	m_remove_property->setEnabled(writable && property_is_own);
	m_move_property_up->setEnabled(writable && property_is_own);
	m_move_property_down->setEnabled(writable && property_is_own);

	m_add_list->setEnabled(writable);
	m_remove_list->setEnabled(writable && m_list_names->currentItem() != nullptr);
	m_save_list->setEnabled(writable && m_list_names->currentItem() != nullptr);
}

/**
	@brief CatalogManagerDialog::addRootClass
*/
void CatalogManagerDialog::addRootClass()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Nouvelle classe"),
						   tr("Nom de la classe"),
						   QLineEdit::Normal, QString(), &ok);
	if (!ok || name.trimmed().isEmpty()) {
		return;
	}

	CatalogClass catalog_class(QString(), name.trimmed());
	QString error;
	if (m_catalog->addClass(catalog_class, &error) == 0)
	{
		QMessageBox::warning(this, tr("Classe non créée"), error);
		return;
	}
	reloadClassTree();
}

/**
	@brief CatalogManagerDialog::addSubclass
*/
void CatalogManagerDialog::addSubclass()
{
	const int parent_id = selectedClassId();
	if (parent_id <= 0) {
		return;
	}

	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Nouvelle sous-classe"),
						   tr("Nom de la sous-classe de « %1 »")
						   .arg(m_catalog->classById(parent_id).name),
						   QLineEdit::Normal, QString(), &ok);
	if (!ok || name.trimmed().isEmpty()) {
		return;
	}

	CatalogClass catalog_class(QString(), name.trimmed());
	catalog_class.parent_id = parent_id;
	QString error;
	if (m_catalog->addClass(catalog_class, &error) == 0)
	{
		QMessageBox::warning(this, tr("Classe non créée"), error);
		return;
	}
	reloadClassTree();
}

/**
	@brief CatalogManagerDialog::removeSelectedClass
*/
void CatalogManagerDialog::removeSelectedClass()
{
	const int class_id = selectedClassId();
	if (class_id <= 0) {
		return;
	}

	const CatalogClass catalog_class = m_catalog->classById(class_id);
	if (QMessageBox::question(this, tr("Supprimer la classe"),
				  tr("Supprimer la classe « %1 » ?").arg(catalog_class.name))
	    != QMessageBox::Yes)
	{
		return;
	}

	QString error;
	if (!m_catalog->removeClass(class_id, &error))
	{
		QMessageBox::warning(this, tr("Classe conservée"), error);
		return;
	}
	reloadClassTree();
	classSelected();
}

/**
	@brief CatalogManagerDialog::applyClassChanges
*/
void CatalogManagerDialog::applyClassChanges()
{
	const int class_id = selectedClassId();
	if (class_id <= 0) {
		return;
	}

	CatalogClass catalog_class = m_catalog->classById(class_id);
	const QString previous_root = catalog_class.root;
	const QString previous_root_iec = catalog_class.root_iec;

	catalog_class.name        = m_class_name->text().trimmed();
	catalog_class.description = m_class_description->text();
	catalog_class.root        = m_class_root->text().trimmed();
	catalog_class.root_iec    = m_class_root_iec->text().trimmed();
	catalog_class.has_symbol  = m_class_has_symbol->isChecked();
	catalog_class.numbering_format = m_class_numbering->currentData().toString();

	if (catalog_class.root != previous_root || catalog_class.root_iec != previous_root_iec)
	{
		// Say it out loud: this reaches the objects that already exist.
		if (QMessageBox::question(this, tr("Changer la racine du repère"),
					  tr("Changer la racine du repère de « %1 » touche tous les "
					     "objets de cette classe et de ses sous-classes, y compris "
					     "ceux des projets déjà enregistrés. Continuer ?")
					  .arg(catalog_class.name))
		    != QMessageBox::Yes)
		{
			return;
		}
	}

	QString error;
	if (!m_catalog->updateClass(catalog_class, &error))
	{
		QMessageBox::warning(this, tr("Classe inchangée"), error);
		return;
	}
	reloadClassTree();
}

/**
	@brief CatalogManagerDialog::exportClassBranch
	Writes the selected class, what is under it and the declared
	properties to a file, so that the branch does not have to be typed
	again on the next workstation.
*/
void CatalogManagerDialog::exportClassBranch()
{
	const int class_id = selectedClassId();
	if (class_id <= 0) {
		return;
	}

	const CatalogClass catalog_class = m_catalog->classById(class_id);
	const QString suggested =
			CatalogClassPackage::suggestedFileName(catalog_class.name);
	const QString file_path = QFileDialog::getSaveFileName(
				this, tr("Exporter la branche de classes"), suggested,
				CatalogClassPackage::fileFilter());
	if (file_path.isEmpty()) {
		return;
	}

	QString error;
	if (!CatalogClassPackage::write(file_path, *m_catalog, class_id, &error))
	{
		QMessageBox::warning(this, tr("Branche non exportée"), error);
		return;
	}
	m_status->setText(tr("Branche « %1 » écrite dans %2.")
			  .arg(catalog_class.name, QDir::toNativeSeparators(file_path)));
}

/**
	@brief CatalogManagerDialog::importClassBranch
	Reads a branch of classes into this catalogue. What the file asks for
	is shown before anything is written, and the same code path answers
	both questions: a dialog that announces two classes and then creates
	something else is worse than no dialog at all.
*/
void CatalogManagerDialog::importClassBranch()
{
	const QString file_path = QFileDialog::getOpenFileName(
				this, tr("Importer une branche de classes"), QString(),
				CatalogClassPackage::fileFilter());
	if (file_path.isEmpty()) {
		return;
	}

	QString error;
	const CatalogClassPackage::Report plan =
			CatalogClassPackage::summary(file_path, *m_catalog, &error);
	if (!error.isEmpty())
	{
		QMessageBox::warning(this, tr("Fichier non lu"), error);
		return;
	}

	if (plan.changesNothing())
	{
		QMessageBox::information(this, tr("Rien à créer"),
					 tr("Tout ce que ce fichier décrit existe déjà "
					    "dans ce catalogue.\n\n%1").arg(plan.toText()));
		return;
	}

	if (QMessageBox::question(this, tr("Importer une branche de classes"),
				  tr("Ce fichier va créer :\n\n%1\n\nCe qui existe déjà "
				     "ici n'est pas modifié. Continuer ?").arg(plan.toText()))
	    != QMessageBox::Yes)
	{
		return;
	}

	CatalogClassPackage::Report done;
	if (!CatalogClassPackage::read(file_path, *m_catalog, &done, &error))
	{
		QMessageBox::warning(this, tr("Branche non importée"), error);
		return;
	}

	reloadClassTree();
	reloadLists();
	classSelected();
	m_status->setText(done.toText());
	if (!done.refused.isEmpty())
	{
			//Refusals are the part the user has to see: they are what the
			//file asked for and did not get.
		QMessageBox::information(this, tr("Branche importée en partie"), done.toText());
	}
}

/**
	@brief CatalogManagerDialog::addProperty
*/
void CatalogManagerDialog::addProperty()
{
	const int class_id = selectedClassId();
	if (class_id <= 0) {
		return;
	}

	CatalogProperty property;
	property.class_id = class_id;

	CatalogPropertyDialog dialog(m_catalog, property, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	CatalogProperty edited = dialog.property();
	edited.class_id = class_id;

	QString error;
	const int property_id = m_catalog->addProperty(edited, &error);
	if (property_id == 0)
	{
		QMessageBox::warning(this, tr("Propriété non créée"), error);
		return;
	}

	if (dialog.applyToExistingParts())
	{
		const int touched = m_catalog->applyDefaultToExistingParts(property_id, &error);
		if (touched < 0) {
			QMessageBox::warning(this, tr("Valeur initiale non appliquée"), error);
		} else {
			m_status->setText(tr("Valeur initiale écrite dans %n pièce(s).", "", touched));
		}
	}

	reloadPropertyTable();
	updateEnabledState();
}

/**
	@brief CatalogManagerDialog::editSelectedProperty
*/
void CatalogManagerDialog::editSelectedProperty()
{
	const int class_id = selectedClassId();
	const int property_id = selectedPropertyId();
	if (class_id <= 0 || property_id <= 0) {
		return;
	}

	const QList<CatalogProperty> own = m_catalog->ownProperties(class_id);
	CatalogProperty property;
	for (const CatalogProperty &candidate : own)
	{
		if (candidate.id == property_id) {
			property = candidate;
		}
	}
	if (property.isNull())
	{
		// An inherited property is edited on the class that declares it.
		QMessageBox::information(this, tr("Propriété héritée"),
					 tr("Cette propriété est déclarée sur une classe mère. "
					    "Sélectionnez cette classe pour la modifier."));
		return;
	}

	CatalogPropertyDialog dialog(m_catalog, property, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	QString error;
	if (!m_catalog->updateProperty(dialog.property(), &error))
	{
		QMessageBox::warning(this, tr("Propriété inchangée"), error);
		return;
	}

	if (dialog.applyToExistingParts())
	{
		const int touched = m_catalog->applyDefaultToExistingParts(property_id, &error);
		if (touched >= 0) {
			m_status->setText(tr("Valeur initiale écrite dans %n pièce(s).", "", touched));
		}
	}

	reloadPropertyTable();
}

/**
	@brief CatalogManagerDialog::removeSelectedProperty
*/
void CatalogManagerDialog::removeSelectedProperty()
{
	const int property_id = selectedPropertyId();
	if (property_id <= 0) {
		return;
	}

	if (QMessageBox::question(this, tr("Supprimer la propriété"),
				  tr("Supprimer cette propriété ? Les valeurs déjà saisies dans les "
				     "pièces sont conservées et redeviendront visibles si la "
				     "propriété est recréée avec la même clé."))
	    != QMessageBox::Yes)
	{
		return;
	}

	QString error;
	if (!m_catalog->removeProperty(property_id, &error))
	{
		QMessageBox::warning(this, tr("Propriété conservée"), error);
		return;
	}
	reloadPropertyTable();
	updateEnabledState();
}

/**
	@brief CatalogManagerDialog::movePropertyUp
*/
void CatalogManagerDialog::movePropertyUp()
{
	moveProperty(-1);
}

/**
	@brief CatalogManagerDialog::movePropertyDown
*/
void CatalogManagerDialog::movePropertyDown()
{
	moveProperty(1);
}

/**
	@brief CatalogManagerDialog::moveProperty
	@param offset : -1 to move up, 1 to move down
*/
void CatalogManagerDialog::moveProperty(int offset)
{
	const int class_id = selectedClassId();
	const int property_id = selectedPropertyId();
	if (class_id <= 0 || property_id <= 0) {
		return;
	}

	QList<int> order;
	const QList<CatalogProperty> own = m_catalog->ownProperties(class_id);
	for (const CatalogProperty &property : own) {
		order.append(property.id);
	}

	const int from = order.indexOf(property_id);
	const int to = from + offset;
	if (from < 0 || to < 0 || to >= order.size()) {
		return;
	}
	order.move(from, to);

	QString error;
	if (!m_catalog->setPropertyOrder(class_id, order, &error))
	{
		QMessageBox::warning(this, tr("Ordre inchangé"), error);
		return;
	}
	reloadPropertyTable();

	// Follow the property the user just moved.
	for (int row = 0 ; row < m_property_table->rowCount() ; ++row)
	{
		const QTableWidgetItem *item = m_property_table->item(row, 0);
		if (item && item->data(ID_ROLE).toInt() == property_id)
		{
			m_property_table->setCurrentCell(row, 0);
			break;
		}
	}
}

/**
	@brief CatalogManagerDialog::reloadLists
*/
void CatalogManagerDialog::reloadLists()
{
	if (!m_catalog) {
		return;
	}
	const QString previous = m_list_names->currentItem()
				 ? m_list_names->currentItem()->text()
				 : QString();

	m_list_names->clear();
	m_list_names->addItems(m_catalog->listNames());

	if (!previous.isEmpty())
	{
		const QList<QListWidgetItem *> items = m_list_names->findItems(previous, Qt::MatchExactly);
		if (!items.isEmpty()) {
			m_list_names->setCurrentItem(items.first());
		}
	}
	listSelected();
}

/**
	@brief CatalogManagerDialog::listSelected
*/
void CatalogManagerDialog::listSelected()
{
	const QListWidgetItem *item = m_list_names->currentItem();
	m_list_values->setPlainText(item ? m_catalog->listValues(item->text()).join(QLatin1Char('\n'))
					 : QString());
	updateEnabledState();
}

/**
	@brief CatalogManagerDialog::addList
*/
void CatalogManagerDialog::addList()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Nouvelle liste contrôlée"),
						   tr("Nom de la liste"),
						   QLineEdit::Normal, QString(), &ok);
	if (!ok || name.trimmed().isEmpty()) {
		return;
	}

	QString error;
	if (!m_catalog->setListValues(name.trimmed(), QStringList(), &error))
	{
		QMessageBox::warning(this, tr("Liste non créée"), error);
		return;
	}
	reloadLists();
}

/**
	@brief CatalogManagerDialog::removeSelectedList
*/
void CatalogManagerDialog::removeSelectedList()
{
	const QListWidgetItem *item = m_list_names->currentItem();
	if (!item) {
		return;
	}
	const QString name = item->text();

	if (QMessageBox::question(this, tr("Supprimer la liste"),
				  tr("Supprimer la liste « %1 » ? Les propriétés qui s'y référaient "
				     "redeviendront des champs libres.").arg(name))
	    != QMessageBox::Yes)
	{
		return;
	}

	QString error;
	if (!m_catalog->removeList(name, &error))
	{
		QMessageBox::warning(this, tr("Liste conservée"), error);
		return;
	}
	reloadLists();
	reloadPropertyTable();
}

/**
	@brief CatalogManagerDialog::saveListValues
*/
void CatalogManagerDialog::saveListValues()
{
	const QListWidgetItem *item = m_list_names->currentItem();
	if (!item) {
		return;
	}

	QStringList values = m_list_values->toPlainText().split(QLatin1Char('\n'),
								Qt::SkipEmptyParts);
	for (QString &value : values) {
		value = value.trimmed();
	}
	values.removeAll(QString());

	QString error;
	if (!m_catalog->setListValues(item->text(), values, &error))
	{
		QMessageBox::warning(this, tr("Valeurs non enregistrées"), error);
		return;
	}
	reloadPropertyTable();
	m_status->setText(tr("Liste « %1 » enregistrée : %n valeur(s).", "", values.size())
			  .arg(item->text()));
}
