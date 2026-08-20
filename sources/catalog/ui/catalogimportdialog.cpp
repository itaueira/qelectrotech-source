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
#include "catalogimportdialog.h"

#include "../catalog.h"
#include "../catalogtablereader.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace
{
	/// How many rows of the file the preview shows.
	const int PREVIEW_ROWS = 12;
	/// Role holding the property key a mapping row is about.
	const int PROPERTY_KEY_ROLE = Qt::UserRole + 1;
}

/**
	@brief CatalogImportDialog::CatalogImportDialog
	@param catalog
	@param parent
*/
CatalogImportDialog::CatalogImportDialog(Catalog *catalog, QWidget *parent) :
	QDialog(parent),
	m_catalog(catalog)
{
	setWindowTitle(tr("Importer des pièces depuis une feuille de calcul"));
	resize(980, 700);
	buildWidgets();
	reloadProfileList();
	classChanged();
	updateEnabledState();
}

/**
	@brief CatalogImportDialog::buildWidgets
*/
void CatalogImportDialog::buildWidgets()
{
	m_file = new QLineEdit(this);
	m_file->setReadOnly(true);
	m_file->setPlaceholderText(tr("Aucun fichier choisi"));
	m_choose_file = new QPushButton(tr("Choisir un fichier…"), this);

	QHBoxLayout *file_layout = new QHBoxLayout();
	file_layout->addWidget(new QLabel(tr("Fichier"), this));
	file_layout->addWidget(m_file, 1);
	file_layout->addWidget(m_choose_file);

	QLabel *csv_hint = new QLabel(
		CatalogTableReader::xlsxSupported()
		? tr("Classeur xlsx ou fichier CSV. Pour un CSV, le séparateur est deviné : "
		     "point-virgule, virgule, tabulation ou barre verticale. D'un classeur, "
		     "la première feuille est lue.")
		: tr("Fichier CSV, tel qu'un tableur l'exporte. Le séparateur est deviné : "
		     "point-virgule, virgule, tabulation ou barre verticale. Cette version ne lit "
		     "pas les classeurs xlsx : les exporter en CSV depuis le tableur."), this);
	csv_hint->setWordWrap(true);

	// --- destination ----------------------------------------------------
	m_class = new QComboBox(this);
	if (m_catalog)
	{
		const QList<CatalogClass> classes = m_catalog->classes();
		for (const CatalogClass &catalog_class : classes)
		{
			const int depth = m_catalog->classAncestry(catalog_class.id).size() - 1;
			m_class->addItem(QString(depth * 4, QLatin1Char(' ')) + catalog_class.name,
					 catalog_class.key);
		}
		const int component = m_class->findData(QStringLiteral("component"));
		if (component >= 0) {
			m_class->setCurrentIndex(component);
		}
	}

	m_class_column = new QComboBox(this);
	m_class_column->setToolTip(tr("À utiliser quand la feuille porte elle-même la classe de "
				      "chaque ligne. Elle prend alors le pas sur la classe choisie "
				      "au-dessus."));
	m_code_column = new QComboBox(this);

	m_policy = new QComboBox(this);
	const QList<CatalogDuplicatePolicy> policies = { CatalogDuplicatePolicy::Ignore,
							 CatalogDuplicatePolicy::Update,
							 CatalogDuplicatePolicy::NewRevision };
	for (const CatalogDuplicatePolicy policy : policies)
	{
		m_policy->addItem(CatalogImportProfile::translatedPolicyName(policy),
				  CatalogImportProfile::policyToString(policy));
	}
	m_policy->setCurrentIndex(1);

	QFormLayout *destination = new QFormLayout();
	destination->addRow(tr("Classe de destination"), m_class);
	destination->addRow(tr("Colonne de la classe"), m_class_column);
	destination->addRow(tr("Colonne de la référence"), m_code_column);
	destination->addRow(tr("Pièce déjà au catalogue"), m_policy);

	// --- profiles -------------------------------------------------------
	m_profiles = new QComboBox(this);
	m_load_profile = new QPushButton(tr("Charger"), this);
	m_save_profile = new QPushButton(tr("Enregistrer sous…"), this);
	m_remove_profile = new QPushButton(tr("Supprimer"), this);

	QHBoxLayout *profile_layout = new QHBoxLayout();
	profile_layout->addWidget(new QLabel(tr("Profil"), this));
	profile_layout->addWidget(m_profiles, 1);
	profile_layout->addWidget(m_load_profile);
	profile_layout->addWidget(m_save_profile);
	profile_layout->addWidget(m_remove_profile);

	QLabel *profile_hint = new QLabel(
		tr("Chaque fournisseur envoie une disposition différente : le profil garde la "
		   "correspondance des colonnes pour le mois suivant. Les profils vivent dans le "
		   "catalogue, donc dans l'environnement partagé — la disposition de la liste d'un "
		   "fournisseur n'est pas une affaire de poste de travail."), this);
	profile_hint->setWordWrap(true);

	// --- preview and mapping --------------------------------------------
	m_preview = new QTableWidget(this);
	m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_preview->verticalHeader()->setVisible(false);

	m_mapping = new QTableWidget(this);
	m_mapping->setColumnCount(2);
	m_mapping->setHorizontalHeaderLabels({ tr("Propriété"), tr("Colonne du fichier") });
	m_mapping->horizontalHeader()->setStretchLastSection(true);
	m_mapping->verticalHeader()->setVisible(false);
	m_mapping->setEditTriggers(QAbstractItemView::NoEditTriggers);

	m_guess = new QPushButton(tr("Deviner la correspondance"), this);

	QWidget *mapping_page = new QWidget(this);
	QVBoxLayout *mapping_layout = new QVBoxLayout(mapping_page);
	mapping_layout->setContentsMargins(0, 0, 0, 0);
	mapping_layout->addWidget(m_mapping);
	mapping_layout->addWidget(m_guess);

	QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(m_preview);
	splitter->addWidget(mapping_page);
	splitter->setStretchFactor(0, 3);
	splitter->setStretchFactor(1, 2);

	m_report = new QTextBrowser(this);
	m_report->setMaximumHeight(150);
	m_report->setPlaceholderText(tr("Le rapport d'importation apparaît ici : accepté, mis à "
					"jour, ignoré, refusé — et pourquoi, ligne par ligne."));

	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	m_import = new QPushButton(tr("Importer"), this);
	m_import->setDefault(true);
	m_export = new QPushButton(tr("Exporter le catalogue en CSV…"), this);

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->addButton(m_export, QDialogButtonBox::ActionRole);
	buttons->addButton(m_import, QDialogButtonBox::ActionRole);
	buttons->addButton(QDialogButtonBox::Close);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(file_layout);
	layout->addWidget(csv_hint);
	layout->addLayout(destination);
	layout->addLayout(profile_layout);
	layout->addWidget(profile_hint);
	layout->addWidget(splitter, 1);
	layout->addWidget(m_report);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	connect(m_choose_file, &QPushButton::clicked, this, &CatalogImportDialog::chooseFile);
	connect(m_class, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogImportDialog::classChanged);
	connect(m_guess, &QPushButton::clicked, this, &CatalogImportDialog::guessMapping);
	connect(m_save_profile, &QPushButton::clicked, this, &CatalogImportDialog::saveProfile);
	connect(m_load_profile, &QPushButton::clicked, this, &CatalogImportDialog::loadProfile);
	connect(m_remove_profile, &QPushButton::clicked, this, &CatalogImportDialog::removeProfile);
	connect(m_import, &QPushButton::clicked, this, &CatalogImportDialog::runImport);
	connect(m_export, &QPushButton::clicked, this, &CatalogImportDialog::exportCatalog);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
}

/**
	@brief CatalogImportDialog::chooseFile
*/
void CatalogImportDialog::chooseFile()
{
	const QString filter = CatalogTableReader::xlsxSupported()
			       ? tr("Feuilles de calcul (*.xlsx *.csv *.txt);;Tous les fichiers (*)")
			       : tr("Feuilles de calcul (*.csv *.txt);;Tous les fichiers (*)");
	const QString chosen = QFileDialog::getOpenFileName(
		this, tr("Choisir la feuille de calcul"), m_file->text(), filter);
	if (chosen.isEmpty()) {
		return;
	}
	m_file_path = chosen;
	m_file->setText(chosen);
	reloadFile();
}

/**
	@brief CatalogImportDialog::reloadFile
*/
void CatalogImportDialog::reloadFile()
{
	QString error;
	m_table = CatalogTableReader::read(m_file_path, QChar(), &error);

	if (!error.isEmpty())
	{
		QMessageBox::warning(this, tr("Fichier non lu"), error);
		m_status->setText(error);
		return;
	}
	if (m_table.isEmpty())
	{
		m_status->setText(tr("Le fichier ne contient aucune ligne lisible."));
		return;
	}

	m_status->setText(tr("%1 colonne(s) et %2 ligne(s) lues. Rien n'est encore écrit dans le "
			     "catalogue.").arg(m_table.columnCount()).arg(m_table.rowCount()));

	// The column pickers, then the preview, then a first guess: the user sees
	// the file before being asked anything about it.
	const QStringList headers = m_table.headers;
	m_class_column->clear();
	m_class_column->addItem(tr("(la classe choisie ci-dessus)"), QString());
	m_code_column->clear();
	for (const QString &header : headers)
	{
		m_class_column->addItem(header, header);
		m_code_column->addItem(header, header);
	}

	reloadPreview();
	reloadMappingTable();
	guessMapping();
	updateEnabledState();
}

/**
	@brief CatalogImportDialog::reloadPreview
*/
void CatalogImportDialog::reloadPreview()
{
	m_preview->clear();
	m_preview->setColumnCount(m_table.columnCount());
	m_preview->setHorizontalHeaderLabels(m_table.headers);

	const int rows = qMin(PREVIEW_ROWS, m_table.rowCount());
	m_preview->setRowCount(rows);
	for (int row = 0 ; row < rows ; ++row)
	{
		for (int column = 0 ; column < m_table.columnCount() ; ++column) {
			m_preview->setItem(row, column,
					   new QTableWidgetItem(m_table.value(row, column)));
		}
	}
	m_preview->resizeColumnsToContents();
}

/**
	@brief CatalogImportDialog::reloadMappingTable
	One row per property of the destination class, so that what can be mapped
	is what the class actually declares - and adding a property to a class
	makes it appear here without anybody touching this file.
*/
void CatalogImportDialog::reloadMappingTable()
{
	m_mapping->setRowCount(0);
	if (!m_catalog) {
		return;
	}

	const CatalogClass destination = m_catalog->classByKey(m_class->currentData().toString());
	const QList<CatalogProperty> properties =
		m_catalog->effectiveProperties(destination.id);
	m_mapping->setRowCount(properties.size());

	for (int row = 0 ; row < properties.size() ; ++row)
	{
		const CatalogProperty &property = properties.at(row);
		QTableWidgetItem *name = new QTableWidgetItem(property.name);
		name->setData(PROPERTY_KEY_ROLE, property.key);
		name->setToolTip(property.key);
		m_mapping->setItem(row, 0, name);

		QComboBox *column = new QComboBox(m_mapping);
		column->addItem(tr("(ne pas importer)"), QString());
		for (const QString &header : m_table.headers) {
			column->addItem(header, header);
		}
		m_mapping->setCellWidget(row, 1, column);
	}
	m_mapping->resizeColumnsToContents();
}

/**
	@brief CatalogImportDialog::classChanged
*/
void CatalogImportDialog::classChanged()
{
	reloadMappingTable();
	if (!m_table.isEmpty()) {
		guessMapping();
	}
	updateEnabledState();
}

/**
	@brief CatalogImportDialog::guessMapping
*/
void CatalogImportDialog::guessMapping()
{
	if (!m_catalog || m_table.isEmpty()) {
		return;
	}

	const CatalogClass destination = m_catalog->classByKey(m_class->currentData().toString());
	const CatalogImportProfile guessed =
		CatalogImportProfile::guess(*m_catalog, destination.id, m_table);
	applyProfile(guessed);
}

/**
	@brief CatalogImportDialog::applyProfile
	@param profile
*/
void CatalogImportDialog::applyProfile(const CatalogImportProfile &profile)
{
	if (!profile.class_key.isEmpty())
	{
		const int index = m_class->findData(profile.class_key);
		if (index >= 0) {
			m_class->setCurrentIndex(index);
		}
	}

	const int class_column = m_class_column->findData(profile.class_column);
	m_class_column->setCurrentIndex(class_column >= 0 ? class_column : 0);

	const int code_column = m_code_column->findData(profile.code_column);
	if (code_column >= 0) {
		m_code_column->setCurrentIndex(code_column);
	}

	const int policy = m_policy->findData(
		CatalogImportProfile::policyToString(profile.policy));
	if (policy >= 0) {
		m_policy->setCurrentIndex(policy);
	}

	for (int row = 0 ; row < m_mapping->rowCount() ; ++row)
	{
		const QTableWidgetItem *name = m_mapping->item(row, 0);
		auto *column = qobject_cast<QComboBox *>(m_mapping->cellWidget(row, 1));
		if (!name || !column) {
			continue;
		}
		const QString key = name->data(PROPERTY_KEY_ROLE).toString();
		const int index = column->findData(profile.value_columns.value(key));
		column->setCurrentIndex(index >= 0 ? index : 0);
	}
}

/**
	@brief CatalogImportDialog::currentProfile
	@return the profile the dialog holds right now
*/
CatalogImportProfile CatalogImportDialog::currentProfile() const
{
	CatalogImportProfile profile;
	profile.class_key = m_class->currentData().toString();
	profile.class_column = m_class_column->currentData().toString();
	profile.code_column = m_code_column->currentData().toString();
	profile.policy = CatalogImportProfile::policyFromString(m_policy->currentData().toString());

	for (int row = 0 ; row < m_mapping->rowCount() ; ++row)
	{
		const QTableWidgetItem *name = m_mapping->item(row, 0);
		auto *column = qobject_cast<QComboBox *>(m_mapping->cellWidget(row, 1));
		if (!name || !column) {
			continue;
		}
		const QString header = column->currentData().toString();
		if (!header.isEmpty()) {
			profile.value_columns.insert(name->data(PROPERTY_KEY_ROLE).toString(),
						     header);
		}
	}
	return profile;
}

/**
	@brief CatalogImportDialog::reloadProfileList
*/
void CatalogImportDialog::reloadProfileList()
{
	m_profiles->clear();
	if (!m_catalog) {
		return;
	}
	m_profiles->addItems(m_catalog->importProfileNames());
	updateEnabledState();
}

/**
	@brief CatalogImportDialog::saveProfile
*/
void CatalogImportDialog::saveProfile()
{
	bool ok = false;
	const QString name = QInputDialog::getText(
		this, tr("Enregistrer le profil"),
		tr("Nom du profil — celui du fournisseur, en général"),
		QLineEdit::Normal, m_profiles->currentText(), &ok);
	if (!ok || name.trimmed().isEmpty()) {
		return;
	}

	CatalogImportProfile profile = currentProfile();
	profile.name = name.trimmed();

	QString error;
	if (!profile.isValid(&error))
	{
		QMessageBox::warning(this, tr("Profil incomplet"), error);
		return;
	}
	if (!m_catalog->saveImportProfile(profile.name, profile.toXml(), &error))
	{
		QMessageBox::warning(this, tr("Profil non enregistré"), error);
		return;
	}

	reloadProfileList();
	const int index = m_profiles->findText(profile.name);
	if (index >= 0) {
		m_profiles->setCurrentIndex(index);
	}
	m_status->setText(tr("Profil « %1 » enregistré dans le catalogue.").arg(profile.name));
}

/**
	@brief CatalogImportDialog::loadProfile
*/
void CatalogImportDialog::loadProfile()
{
	const QString name = m_profiles->currentText();
	if (name.isEmpty()) {
		return;
	}
	const QString xml = m_catalog->importProfile(name);
	if (xml.isEmpty())
	{
		m_status->setText(tr("Le profil « %1 » est vide.").arg(name));
		return;
	}

	const CatalogImportProfile profile = CatalogImportProfile::fromXml(xml);
	reloadMappingTable();
	applyProfile(profile);
	m_status->setText(tr("Profil « %1 » chargé. Aucun remappage à faire.").arg(name));
}

/**
	@brief CatalogImportDialog::removeProfile
*/
void CatalogImportDialog::removeProfile()
{
	const QString name = m_profiles->currentText();
	if (name.isEmpty()) {
		return;
	}
	if (QMessageBox::question(this, tr("Supprimer le profil"),
				  tr("Supprimer le profil « %1 » ?").arg(name))
	    != QMessageBox::Yes)
	{
		return;
	}

	QString error;
	if (!m_catalog->removeImportProfile(name, &error))
	{
		QMessageBox::warning(this, tr("Profil conservé"), error);
		return;
	}
	reloadProfileList();
}

/**
	@brief CatalogImportDialog::runImport
*/
void CatalogImportDialog::runImport()
{
	if (!m_catalog || m_table.isEmpty()) {
		return;
	}

	const CatalogImportProfile profile = currentProfile();
	QString error;
	if (!profile.isValid(&error))
	{
		QMessageBox::warning(this, tr("Importation impossible"), error);
		return;
	}

	const QString origin = QStringLiteral("planilha:") + QFileInfo(m_file_path).fileName();

	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	const CatalogImportReport report =
		CatalogImporter::import(*m_catalog, m_table, profile, origin);
	QGuiApplication::restoreOverrideCursor();

	m_report->setPlainText(report.toText());
	m_status->setText(tr("%1 créée(s), %2 mise(s) à jour, %3 révision(s), %4 ignorée(s), "
			     "%5 refusée(s).")
			  .arg(report.created).arg(report.updated).arg(report.revised)
			  .arg(report.ignored).arg(report.rejected()));

	// Refusals are said out loud, not left in a panel the user may not look
	// at: a silent import is what corrupts a catalog.
	if (report.rejected() > 0)
	{
		QMessageBox::warning(this, tr("Importation terminée avec des refus"),
				     tr("%n ligne(s) ont été refusée(s). Le rapport en bas de la "
					"fenêtre dit laquelle et pourquoi.", "", report.rejected()));
	}
}

/**
	@brief CatalogImportDialog::exportCatalog
*/
void CatalogImportDialog::exportCatalog()
{
	if (!m_catalog) {
		return;
	}

	const QString chosen = QFileDialog::getSaveFileName(
		this, tr("Exporter le catalogue"), QStringLiteral("catalogo.csv"),
		tr("Feuilles de calcul (*.csv)"));
	if (chosen.isEmpty()) {
		return;
	}

	const CatalogClass destination = m_catalog->classByKey(m_class->currentData().toString());
	const CatalogTable table = CatalogImporter::exportToTable(*m_catalog, destination.id);

	QString error;
	if (!CatalogTableReader::writeCsv(chosen, table, QLatin1Char(';'), &error))
	{
		QMessageBox::warning(this, tr("Export non effectué"), error);
		return;
	}
	m_status->setText(tr("%n pièce(s) exportée(s) vers %1.", "", table.rowCount()).arg(chosen));
}

/**
	@brief CatalogImportDialog::updateEnabledState
*/
void CatalogImportDialog::updateEnabledState()
{
	const bool writable = m_catalog && m_catalog->isWritable();
	const bool has_file = !m_table.isEmpty();
	const bool has_profile = m_profiles->count() > 0;

	m_import->setEnabled(writable && has_file);
	m_guess->setEnabled(has_file);
	m_save_profile->setEnabled(writable && has_file);
	m_load_profile->setEnabled(has_profile);
	m_remove_profile->setEnabled(writable && has_profile);
	m_export->setEnabled(m_catalog && m_catalog->isOpen());
}
