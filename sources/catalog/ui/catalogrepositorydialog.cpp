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
#include "catalogrepositorydialog.h"

#include "../catalog.h"
#include "../catalogpackage.h"
#include "catalogbrowserdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

/**
	@brief CatalogRepositoryDialog::CatalogRepositoryDialog
	@param catalog
	@param parent
*/
CatalogRepositoryDialog::CatalogRepositoryDialog(Catalog *catalog, QWidget *parent) :
	QDialog(parent),
	m_catalog(catalog)
{
	setWindowTitle(tr("Répertoire de pièces partagé"));
	resize(960, 620);
	buildWidgets();
	reload();
}

/**
	@brief CatalogRepositoryDialog::buildWidgets
*/
void CatalogRepositoryDialog::buildWidgets()
{
	m_folder = new QLineEdit(CatalogRepository::path(), this);
	m_folder->setReadOnly(true);
	m_choose_folder = new QPushButton(tr("Changer de dossier…"), this);

	QHBoxLayout *folder_layout = new QHBoxLayout();
	folder_layout->addWidget(new QLabel(tr("Répertoire"), this));
	folder_layout->addWidget(m_folder, 1);
	folder_layout->addWidget(m_choose_folder);

	m_text = new QLineEdit(this);
	m_text->setPlaceholderText(tr("Référence, désignation, fabricant, classe…"));
	m_text->setClearButtonEnabled(true);
	m_class_filter = new QComboBox(this);
	m_manufacturer_filter = new QComboBox(this);
	m_clear_filters = new QPushButton(tr("Effacer les filtres"), this);

	QHBoxLayout *filters = new QHBoxLayout();
	filters->addWidget(new QLabel(tr("Rechercher"), this));
	filters->addWidget(m_text, 2);
	filters->addWidget(new QLabel(tr("Classe"), this));
	filters->addWidget(m_class_filter, 1);
	filters->addWidget(new QLabel(tr("Fabricant"), this));
	filters->addWidget(m_manufacturer_filter, 1);
	filters->addWidget(m_clear_filters);

	m_results_table = new QTableWidget(this);
	m_results_table->setColumnCount(4);
	m_results_table->setHorizontalHeaderLabels({ tr("Référence"), tr("Classe"),
						     tr("Désignation"), tr("Fabricant") });
	m_results_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_results_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_results_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_results_table->verticalHeader()->setVisible(false);
	m_results_table->horizontalHeader()->setStretchLastSection(true);

	m_image = new QLabel(this);
	m_image->setAlignment(Qt::AlignCenter);
	m_image->setMinimumHeight(130);
	m_image->setText(tr("(pas d'image)"));
	m_preview = new QTextBrowser(this);

	QWidget *right = new QWidget(this);
	QVBoxLayout *right_layout = new QVBoxLayout(right);
	right_layout->setContentsMargins(0, 0, 0, 0);
	right_layout->addWidget(m_image);
	right_layout->addWidget(m_preview);

	QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(m_results_table);
	splitter->addWidget(right);
	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 1);

	QLabel *hint = new QLabel(
		tr("Un paquet porte des <b>données</b>, pas un dessin : propriétés, brochage, vue "
		   "physique, image de référence et accessoires. Le symbole de schéma n'y est pas, "
		   "et c'est voulu — il est généré ici, dans l'orientation et le découpage qui "
		   "conviennent à la maison. Le prix n'y est pas non plus : un prix appartient à une "
		   "entreprise et à une date, pas à une pièce."), this);
	hint->setWordWrap(true);
	hint->setTextFormat(Qt::RichText);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	m_import = new QPushButton(tr("Télécharger et importer"), this);
	m_import->setDefault(true);
	m_contribute = new QPushButton(tr("Contribuer une pièce…"), this);
	m_contribute->setToolTip(tr("Écrit une pièce du catalogue local dans le répertoire. "
				    "Rien ne sort d'ici sans cette action : le catalogue porte des "
				    "décisions d'achat et d'ingénierie de l'entreprise."));

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->addButton(m_contribute, QDialogButtonBox::ActionRole);
	buttons->addButton(m_import, QDialogButtonBox::AcceptRole);
	buttons->addButton(QDialogButtonBox::Close);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(folder_layout);
	layout->addLayout(filters);
	layout->addWidget(splitter, 1);
	layout->addWidget(hint);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	connect(m_choose_folder, &QPushButton::clicked,
		this, &CatalogRepositoryDialog::chooseFolder);
	connect(m_text, &QLineEdit::textChanged, this, &CatalogRepositoryDialog::search);
	connect(m_class_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogRepositoryDialog::search);
	connect(m_manufacturer_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogRepositoryDialog::search);
	connect(m_clear_filters, &QPushButton::clicked,
		this, &CatalogRepositoryDialog::clearFilters);
	connect(m_results_table, &QTableWidget::itemSelectionChanged,
		this, &CatalogRepositoryDialog::selectionChanged);
	connect(m_results_table, &QTableWidget::doubleClicked,
		this, &CatalogRepositoryDialog::importSelected);
	connect(m_import, &QPushButton::clicked, this, &CatalogRepositoryDialog::importSelected);
	connect(m_contribute, &QPushButton::clicked, this, &CatalogRepositoryDialog::contribute);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief CatalogRepositoryDialog::reload
*/
void CatalogRepositoryDialog::reload()
{
	const QString root = CatalogRepository::path();
	m_folder->setText(root);
	m_entries = CatalogRepository::entries(root);
	fillFilters();
	search();

	if (m_entries.isEmpty())
	{
		m_status->setText(
			QFileInfo::exists(root)
			? tr("Le répertoire %1 est vide. Il se remplit par la contribution "
			     "explicite de qui veut partager une pièce.").arg(root)
			: tr("Le dossier %1 n'existe pas encore. Il sera créé à la première "
			     "contribution.").arg(root));
	}
}

/**
	@brief CatalogRepositoryDialog::fillFilters
*/
void CatalogRepositoryDialog::fillFilters()
{
	m_class_filter->clear();
	m_class_filter->addItem(tr("Toutes"), QString());
	const QStringList class_keys = CatalogRepository::classKeysOf(m_entries);
	for (const QString &key : class_keys)
	{
		// Show the name this catalog uses for the class when it knows it, and
		// the raw key when it does not - a package from elsewhere may name a
		// class that does not exist here, and hiding that would be worse.
		const CatalogClass known = m_catalog ? m_catalog->classByKey(key) : CatalogClass();
		m_class_filter->addItem(known.isNull() ? key : known.name, key);
	}

	m_manufacturer_filter->clear();
	m_manufacturer_filter->addItem(tr("Tous"), QString());
	m_manufacturer_filter->addItems(CatalogRepository::manufacturersOf(m_entries));
}

/**
	@brief CatalogRepositoryDialog::search
*/
void CatalogRepositoryDialog::search()
{
	m_results = CatalogRepository::search(m_entries,
					      m_text->text(),
					      m_class_filter->currentData().toString(),
					      m_manufacturer_filter->currentIndex() <= 0
						      ? QString()
						      : m_manufacturer_filter->currentText());

	m_results_table->setRowCount(m_results.size());
	for (int row = 0 ; row < m_results.size() ; ++row)
	{
		const CatalogRepositoryEntry &entry = m_results.at(row);
		const CatalogClass known = m_catalog ? m_catalog->classByKey(entry.class_key)
						     : CatalogClass();
		const QStringList texts = { entry.code,
					    known.isNull() ? entry.class_name : known.name,
					    entry.designation,
					    entry.manufacturer };
		for (int column = 0 ; column < texts.size() ; ++column) {
			m_results_table->setItem(row, column, new QTableWidgetItem(texts.at(column)));
		}
	}
	m_results_table->resizeColumnsToContents();

	if (!m_entries.isEmpty())
	{
		m_status->setText(tr("%n pièce(s) trouvée(s) sur %1 dans le répertoire.",
				     "", m_results.size()).arg(m_entries.size()));
	}
	selectionChanged();
}

/**
	@brief CatalogRepositoryDialog::clearFilters
*/
void CatalogRepositoryDialog::clearFilters()
{
	m_text->clear();
	m_class_filter->setCurrentIndex(0);
	m_manufacturer_filter->setCurrentIndex(0);
	search();
}

/**
	@brief CatalogRepositoryDialog::selectionChanged
*/
void CatalogRepositoryDialog::selectionChanged()
{
	const int row = m_results_table->currentRow();
	m_selected = (row >= 0 && row < m_results.size()) ? m_results.at(row)
							  : CatalogRepositoryEntry();

	const bool writable = m_catalog && m_catalog->isWritable();
	m_import->setEnabled(writable && !m_selected.isNull());
	m_contribute->setEnabled(m_catalog && m_catalog->isOpen());
	showPreview(m_selected);
}

/**
	@brief CatalogRepositoryDialog::showPreview
	@param entry
*/
void CatalogRepositoryDialog::showPreview(const CatalogRepositoryEntry &entry)
{
	if (entry.isNull())
	{
		m_preview->clear();
		m_image->setPixmap(QPixmap());
		m_image->setText(tr("(pas d'image)"));
		return;
	}

	// Read the whole package now: the user asked to look at one, and this is
	// what "pré-visualisation avant de télécharger" means.
	QString error;
	const CatalogPart part = m_catalog
				 ? CatalogPackage::read(entry.file_path, *m_catalog, &error)
				 : CatalogPart();

	QString html = QStringLiteral("<h3>%1</h3>").arg(entry.code.toHtmlEscaped());
	if (!error.isEmpty()) {
		html += QStringLiteral("<p><b>%1</b></p>").arg(error.toHtmlEscaped());
	}

	html += QStringLiteral("<table cellspacing='4'>");
	const QStringList keys = part.values.keys();
	for (const QString &key : keys)
	{
		const QString value = part.values.value(key);
		if (value.isEmpty()) {
			continue;
		}
		QString label = key;
		if (m_catalog && part.class_id > 0)
		{
			const CatalogProperty property =
				m_catalog->effectiveProperty(part.class_id, key);
			if (!property.isNull()) {
				label = property.name;
			}
		}
		html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>")
			.arg(label.toHtmlEscaped(), value.toHtmlEscaped());
	}
	html += QStringLiteral("</table>");

	if (!part.pins.isEmpty())
	{
		QStringList pin_texts;
		for (const CatalogPin &pin : part.pins)
		{
			pin_texts.append(QStringLiteral("%1 (%2)")
					 .arg(pin.label.toHtmlEscaped(),
					      CatalogPin::translatedRoleName(pin.role).toHtmlEscaped()));
		}
		html += QStringLiteral("<p><b>%1</b><br/>%2</p>")
			.arg(tr("Brochage").toHtmlEscaped(), pin_texts.join(QStringLiteral(", ")));
	}

	if (!part.accessories.isEmpty())
	{
		QStringList accessory_texts;
		for (const CatalogAccessory &accessory : part.accessories)
		{
			accessory_texts.append(QStringLiteral("%1 &times; %2")
					       .arg(QString::number(accessory.quantity),
						    accessory.code.toHtmlEscaped()));
		}
		html += QStringLiteral("<p><b>%1</b><br/>%2</p>")
			.arg(tr("Accessoires").toHtmlEscaped(),
			     accessory_texts.join(QStringLiteral("<br/>")));
	}

	m_preview->setHtml(html);

	QPixmap pixmap;
	if (!entry.image.isEmpty() && QFileInfo::exists(entry.image)) {
		pixmap.load(entry.image);
	}
	if (pixmap.isNull())
	{
		m_image->setPixmap(QPixmap());
		m_image->setText(tr("(pas d'image)"));
	}
	else
	{
		m_image->setPixmap(pixmap.scaled(m_image->width(), 130,
						 Qt::KeepAspectRatio, Qt::SmoothTransformation));
		m_image->setText(QString());
	}
}

/**
	@brief CatalogRepositoryDialog::importSelected
*/
void CatalogRepositoryDialog::importSelected()
{
	if (m_selected.isNull() || !m_catalog) {
		return;
	}

	QString error;
	CatalogPart part = CatalogPackage::read(m_selected.file_path, *m_catalog, &error);
	if (part.isNull())
	{
		QMessageBox::warning(this, tr("Paquet non lu"), error);
		return;
	}

	if (part.class_id == 0)
	{
		// The class of the package does not exist here. Offering to create it
		// beats refusing the part, and beats guessing which class it should go
		// into.
		const QString class_key = m_selected.class_key.isEmpty()
					  ? m_selected.class_name
					  : m_selected.class_key;
		if (QMessageBox::question(this, tr("Classe absente"),
					  tr("La classe « %1 » n'existe pas dans ce catalogue.\n\n"
					     "La créer maintenant, sous « Composant » ?")
						  .arg(class_key))
		    != QMessageBox::Yes)
		{
			return;
		}

		CatalogClass created(m_selected.class_key,
				     m_selected.class_name.isEmpty() ? m_selected.class_key
								     : m_selected.class_name);
		created.parent_id = m_catalog->classByKey(QStringLiteral("component")).id;
		const int class_id = m_catalog->addClass(created, &error);
		if (class_id == 0)
		{
			QMessageBox::warning(this, tr("Classe non créée"), error);
			return;
		}
		part.class_id = class_id;
	}

	// A part already here is not silently replaced: same three answers as the
	// spreadsheet import, asked the same way.
	const CatalogPart existing = m_catalog->partByCode(part.code);
	if (!existing.isNull())
	{
		QMessageBox box(this);
		box.setWindowTitle(tr("La pièce existe déjà"));
		box.setText(tr("« %1 » est déjà dans le catalogue local.").arg(part.code));
		box.setInformativeText(tr("Que faire du paquet ?"));
		QPushButton *ignore = box.addButton(tr("Ignorer"), QMessageBox::RejectRole);
		QPushButton *update = box.addButton(tr("Mettre à jour"), QMessageBox::AcceptRole);
		QPushButton *revision = box.addButton(tr("Nouvelle révision"),
						      QMessageBox::AcceptRole);
		box.setDefaultButton(update);
		box.exec();

		if (box.clickedButton() == ignore) {
			return;
		}
		if (box.clickedButton() == revision)
		{
			if (!m_catalog->savePartAsNewRevision(part, &error))
			{
				QMessageBox::warning(this, tr("Pièce non importée"), error);
				return;
			}
			m_imported = part;
			m_status->setText(tr("« %1 » importée comme révision %2.")
					  .arg(part.code).arg(part.revision));
			accept();
			return;
		}
		part.id = existing.id;
		part.revision = existing.revision;
	}

	if (!m_catalog->savePart(part, &error))
	{
		QMessageBox::warning(this, tr("Pièce non importée"), error);
		return;
	}

	m_imported = part;
	m_status->setText(tr("« %1 » importée dans le catalogue local.").arg(part.code));
	accept();
}

/**
	@brief CatalogRepositoryDialog::contribute
*/
void CatalogRepositoryDialog::contribute()
{
	const CatalogPart part = CatalogBrowserDialog::choosePart(m_catalog, this);
	if (part.isNull()) {
		return;
	}

	if (QMessageBox::question(this, tr("Contribuer une pièce"),
				  tr("Écrire « %1 » dans le répertoire partagé ?\n\n"
				     "Les données de la pièce y seront lisibles par qui a accès au "
				     "dossier. Le prix et les conditions commerciales ne partent pas.")
					  .arg(part.code))
	    != QMessageBox::Yes)
	{
		return;
	}

	QString error;
	if (!CatalogRepository::contribute(CatalogRepository::path(), *m_catalog, part, &error))
	{
		QMessageBox::warning(this, tr("Pièce non contribuée"), error);
		return;
	}

	reload();
	m_status->setText(tr("« %1 » écrite dans le répertoire.").arg(part.code));
}

/**
	@brief CatalogRepositoryDialog::chooseFolder
*/
void CatalogRepositoryDialog::chooseFolder()
{
	const QString chosen = QFileDialog::getExistingDirectory(
		this, tr("Choisir le dossier du répertoire de pièces"), m_folder->text());
	if (chosen.isEmpty()) {
		return;
	}

	QString error;
	if (!CatalogRepository::setPath(chosen, &error))
	{
		QMessageBox::warning(this, tr("Dossier inchangé"), error);
		return;
	}
	reload();
}

/**
	@brief CatalogRepositoryDialog::importedPart
	@return the part that was brought into the catalog
*/
CatalogPart CatalogRepositoryDialog::importedPart() const
{
	return m_imported;
}

/**
	@brief CatalogRepositoryDialog::findAndImport
	@param catalog
	@param parent
	@return the part the user downloaded and imported
*/
CatalogPart CatalogRepositoryDialog::findAndImport(Catalog *catalog, QWidget *parent)
{
	CatalogRepositoryDialog dialog(catalog, parent);
	if (dialog.exec() != QDialog::Accepted) {
		return CatalogPart();
	}
	return dialog.importedPart();
}
