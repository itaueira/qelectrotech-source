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
#include "catalogbrowserdialog.h"

#include "../catalog.h"
#include "catalogpartdialog.h"
#include "catalogrepositorydialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
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

namespace
{
	const int PART_ID_ROLE = Qt::UserRole + 1;
}

/**
	@brief CatalogBrowserDialog::CatalogBrowserDialog
	@param catalog
	@param parent
*/
CatalogBrowserDialog::CatalogBrowserDialog(Catalog *catalog, QWidget *parent) :
	QDialog(parent),
	m_catalog(catalog)
{
	setWindowTitle(tr("Catalogue de pièces"));
	resize(1000, 600);
	buildWidgets();
	fillClassFilter();
	fillManufacturerFilter();
	search();
}

/**
	@brief CatalogBrowserDialog::buildWidgets
*/
void CatalogBrowserDialog::buildWidgets()
{
	m_text = new QLineEdit(this);
	m_text->setPlaceholderText(tr("Référence, désignation, fabricant…"));
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
	m_results_table->setColumnCount(5);
	m_results_table->setHorizontalHeaderLabels({ tr("Référence"), tr("Classe"),
						     tr("Désignation"), tr("Fabricant"),
						     tr("Rév.") });
	m_results_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_results_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_results_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_results_table->verticalHeader()->setVisible(false);
	m_results_table->horizontalHeader()->setStretchLastSection(true);

	m_image = new QLabel(this);
	m_image->setAlignment(Qt::AlignCenter);
	m_image->setMinimumHeight(140);
	m_image->setText(tr("(pas d'image)"));

	m_preview = new QTextBrowser(this);
	m_preview->setOpenExternalLinks(true);

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

	m_repository  = new QPushButton(tr("Chercher dans le répertoire…"), this);
	m_repository->setToolTip(tr("Quand la pièce n'est pas encore au catalogue local : la "
				    "chercher dans le répertoire partagé, l'importer, et revenir "
				    "ici pour l'attribuer."));
	m_new_part    = new QPushButton(tr("Nouvelle pièce…"), this);
	m_edit_part   = new QPushButton(tr("Modifier…"), this);
	m_duplicate_part = new QPushButton(tr("Dupliquer…"), this);
	m_duplicate_part->setToolTip(tr(
			   "Ouvre une pièce neuve avec les valeurs de celle-ci et le code "
			   "vide. Cataloguer une famille — même produit, un champ qui "
			   "change — devient remplir un champ au lieu de tout retaper."));
	m_remove_part = new QPushButton(tr("Supprimer"), this);
	m_choose      = new QPushButton(tr("Attribuer"), this);
	m_choose->setDefault(true);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->addButton(m_repository, QDialogButtonBox::ActionRole);
	buttons->addButton(m_new_part, QDialogButtonBox::ActionRole);
	buttons->addButton(m_edit_part, QDialogButtonBox::ActionRole);
	buttons->addButton(m_duplicate_part, QDialogButtonBox::ActionRole);
	buttons->addButton(m_remove_part, QDialogButtonBox::ActionRole);
	buttons->addButton(m_choose, QDialogButtonBox::AcceptRole);
	buttons->addButton(QDialogButtonBox::Close);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(filters);
		//A horizontal QSplitter carries a vertical size policy of Preferred, not
		//Expanding, so it does not claim the height a taller window offers: the
		//table stayed seven rows deep with a third of the dialog left blank
		//below it.  The stretch is what makes room; the two other catalogue
		//dialogs already pass it.
	layout->addWidget(splitter, 1);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	connect(m_text, &QLineEdit::textChanged, this, &CatalogBrowserDialog::search);
	connect(m_class_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogBrowserDialog::search);
	connect(m_manufacturer_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CatalogBrowserDialog::search);
	connect(m_clear_filters, &QPushButton::clicked, this, &CatalogBrowserDialog::clearFilters);
	connect(m_results_table, &QTableWidget::itemSelectionChanged,
		this, &CatalogBrowserDialog::selectionChanged);
		//activated, not doubleClicked: it also fires on Enter, so the table
		//can be used without a mouse, and it follows the platform convention.
	connect(m_results_table, &QTableWidget::activated,
		this, &CatalogBrowserDialog::acceptSelection);
	connect(m_repository, &QPushButton::clicked,
		this, &CatalogBrowserDialog::searchRepository);
	connect(m_new_part, &QPushButton::clicked, this, &CatalogBrowserDialog::createPart);
	connect(m_edit_part, &QPushButton::clicked, this, &CatalogBrowserDialog::editSelectedPart);
	connect(m_duplicate_part, &QPushButton::clicked, this, &CatalogBrowserDialog::duplicateSelectedPart);
	connect(m_remove_part, &QPushButton::clicked, this, &CatalogBrowserDialog::removeSelectedPart);
	connect(m_choose, &QPushButton::clicked, this, &CatalogBrowserDialog::acceptSelection);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief CatalogBrowserDialog::fillClassFilter
	List the classes with their depth shown by indentation, so that filtering
	by "Composant" and filtering by "Contacteur" are visibly different things.
*/
void CatalogBrowserDialog::fillClassFilter()
{
	m_class_filter->clear();
	m_class_filter->addItem(tr("Toutes"), 0);
	if (!m_catalog) {
		return;
	}

	const QList<CatalogClass> classes = m_catalog->classes();
	for (const CatalogClass &catalog_class : classes)
	{
		const int depth = m_catalog->classAncestry(catalog_class.id).size() - 1;
		m_class_filter->addItem(QString(depth * 4, QLatin1Char(' ')) + catalog_class.name,
					catalog_class.id);
	}
}

/**
	@brief CatalogBrowserDialog::fillManufacturerFilter
*/
void CatalogBrowserDialog::fillManufacturerFilter()
{
	m_manufacturer_filter->clear();
	m_manufacturer_filter->addItem(tr("Tous"), QString());
	if (!m_catalog) {
		return;
	}

	// The values that are actually in use, not only the controlled list: a
	// catalog filled by import carries manufacturers nobody typed into a list.
	QStringList manufacturers;
	const QList<CatalogPart> parts = m_catalog->parts();
	for (const CatalogPart &part : parts)
	{
		const QString manufacturer = part.value(QStringLiteral("manufacturer"));
		if (!manufacturer.isEmpty() && !manufacturers.contains(manufacturer)) {
			manufacturers.append(manufacturer);
		}
	}
	manufacturers.sort();
	for (const QString &manufacturer : manufacturers) {
		m_manufacturer_filter->addItem(manufacturer, manufacturer);
	}
}

/**
	@brief CatalogBrowserDialog::search
*/
void CatalogBrowserDialog::search()
{
	m_results.clear();
	m_results_table->setRowCount(0);
	if (!m_catalog) {
		return;
	}

	m_results = m_catalog->searchParts(m_text->text().trimmed(),
					   m_class_filter->currentData().toInt(),
					   m_manufacturer_filter->currentData().toString());

	m_results_table->setRowCount(m_results.size());
	for (int row = 0 ; row < m_results.size() ; ++row)
	{
		const CatalogPart &part = m_results.at(row);
		const CatalogClass owner = m_catalog->classById(part.class_id);
		const QHash<QString, QString> values = m_catalog->effectiveValues(part);

		const QStringList texts = { part.code,
					    owner.name,
					    values.value(QStringLiteral("designation")),
					    values.value(QStringLiteral("manufacturer")),
					    QString::number(part.revision) };
		for (int column = 0 ; column < texts.size() ; ++column)
		{
			QTableWidgetItem *item = new QTableWidgetItem(texts.at(column));
			if (column == 0) {
				item->setData(PART_ID_ROLE, part.id);
			}
			m_results_table->setItem(row, column, item);
		}
	}
	m_results_table->resizeColumnsToContents();

	m_status->setText(tr("%n pièce(s) trouvée(s) sur %1 dans le catalogue.", "", m_results.size())
			  .arg(m_catalog->partCount()));
	selectionChanged();
}

/**
	@brief CatalogBrowserDialog::clearFilters
*/
void CatalogBrowserDialog::clearFilters()
{
	m_text->clear();
		//the class the caller pinned is not a filter the user typed,
		//so clearing gives it back instead of dropping it.
	const int forced = m_class_filter->findData(m_forced_class);
	m_class_filter->setCurrentIndex(forced >= 0 ? forced : 0);
	m_manufacturer_filter->setCurrentIndex(0);
	search();
}

/**
	@brief CatalogBrowserDialog::selectionChanged
*/
void CatalogBrowserDialog::selectionChanged()
{
	const int row = m_results_table->currentRow();
	m_selected = (row >= 0 && row < m_results.size()) ? m_results.at(row) : CatalogPart();

	const bool has_selection = !m_selected.isNull();
	const bool writable = m_catalog && m_catalog->isWritable();
	m_choose->setEnabled(has_selection);
	m_edit_part->setEnabled(has_selection && writable);
	m_duplicate_part->setEnabled(has_selection && writable);
	m_remove_part->setEnabled(has_selection && writable);
	m_new_part->setEnabled(writable);
	m_repository->setEnabled(writable);

	showPreview(m_selected);
}

/**
	@brief CatalogBrowserDialog::showPreview
	@param part
*/
void CatalogBrowserDialog::showPreview(const CatalogPart &part)
{
	if (part.isNull())
	{
		m_preview->clear();
		m_image->setPixmap(QPixmap());
		m_image->setText(tr("(pas d'image)"));
		return;
	}

	const QHash<QString, QString> values = m_catalog->effectiveValues(part);
	const QList<CatalogProperty> properties = m_catalog->effectiveProperties(part.class_id);

	QString html = QStringLiteral("<h3>%1</h3>").arg(part.code.toHtmlEscaped());
	html += QStringLiteral("<table cellspacing='4'>");

	for (const CatalogProperty &property : properties)
	{
		const QString value = values.value(property.key);
		if (value.isEmpty()) {
			continue;
		}
		QString shown = value.toHtmlEscaped();
		if (property.type == CatalogPropertyType::Link) {
			shown = QStringLiteral("<a href='%1'>%1</a>").arg(shown);
		} else if (!property.unit.isEmpty()) {
			shown += QLatin1Char(' ') + property.unit.toHtmlEscaped();
		}
		html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>")
			.arg(property.name.toHtmlEscaped(), shown);
	}
	html += QStringLiteral("</table>");

	if (!part.pins.isEmpty())
	{
		html += QStringLiteral("<p><b>%1</b><br/>").arg(tr("Bornes").toHtmlEscaped());
		QStringList pin_texts;
		for (const CatalogPin &pin : part.pins)
		{
			pin_texts.append(QStringLiteral("%1 (%2)")
					 .arg(pin.label.toHtmlEscaped(),
					      CatalogPin::translatedRoleName(pin.role).toHtmlEscaped()));
		}
		html += pin_texts.join(QStringLiteral(", ")) + QStringLiteral("</p>");
	}

	if (!part.accessories.isEmpty())
	{
		html += QStringLiteral("<p><b>%1</b><br/>").arg(tr("Accessoires").toHtmlEscaped());
		QStringList accessory_texts;
		for (const CatalogAccessory &accessory : part.accessories)
		{
			accessory_texts.append(QStringLiteral("%1 &times; %2")
					       .arg(QString::number(accessory.quantity),
						    accessory.code.toHtmlEscaped()));
		}
		html += accessory_texts.join(QStringLiteral("<br/>")) + QStringLiteral("</p>");
	}

	if (!part.origin.isEmpty())
	{
		html += QStringLiteral("<p><i>%1</i></p>")
			.arg(tr("Provenance : %1 (%2)")
			     .arg(part.origin, part.origin_date).toHtmlEscaped());
	}

	m_preview->setHtml(html);

	const QString image_path = values.value(QStringLiteral("image"));
	QPixmap pixmap;
	if (!image_path.isEmpty() && QFileInfo::exists(image_path)) {
		pixmap.load(image_path);
	}
	if (pixmap.isNull())
	{
		m_image->setPixmap(QPixmap());
		m_image->setText(tr("(pas d'image)"));
	}
	else
	{
		m_image->setPixmap(pixmap.scaled(m_image->width(), 140,
						 Qt::KeepAspectRatio, Qt::SmoothTransformation));
		m_image->setText(QString());
	}
}

/**
	@brief CatalogBrowserDialog::searchRepository
	Go to the shared repository, bring a part back, and select it here so that
	the assignment the user was in the middle of simply continues.
*/
void CatalogBrowserDialog::searchRepository()
{
	const CatalogPart imported = CatalogRepositoryDialog::findAndImport(m_catalog, this);
	if (imported.isNull()) {
		return;
	}

	// Show what was just brought in, whatever the filters were: the user asked
	// for that part, and hiding it behind a stale filter would look like the
	// import failed.
	clearFilters();
	m_text->setText(imported.code);
	search();

	for (int row = 0 ; row < m_results.size() ; ++row)
	{
		if (m_results.at(row).code == imported.code)
		{
			m_results_table->setCurrentCell(row, 0);
			break;
		}
	}
	m_status->setText(tr("« %1 » importée du répertoire. Elle est sélectionnée : "
			     "« Attribuer » termine ce que vous étiez en train de faire.")
			  .arg(imported.code));
}

/**
	@brief CatalogBrowserDialog::createPart
*/
void CatalogBrowserDialog::createPart()
{
		//Coming from "Components without a part", the caller already knows which
		//component is being settled: the manufacturer the designer typed while
		//drawing, and one pin per terminal of the symbol. Typing that again is
		//exactly the waste this path exists to remove.
	CatalogPart part = m_has_template ? m_template : CatalogPart();

		//The button is "New part": even when the template points at a part that
		//already exists, what comes out of here is a new part.
	part.id = 0;

	if (part.class_id == 0) {
		part.class_id = m_class_filter->currentData().toInt();
	}
	if (part.class_id == 0)
	{
		const CatalogClass component = m_catalog->classByKey(QStringLiteral("component"));
		part.class_id = component.isNull() ? 0 : component.id;
	}

	CatalogPartDialog dialog(m_catalog, part, this);
	if (dialog.exec() == QDialog::Accepted)
	{
		fillManufacturerFilter();
		search();
	}
}

/**
	@brief CatalogBrowserDialog::editSelectedPart
*/
void CatalogBrowserDialog::editSelectedPart()
{
	if (m_selected.isNull()) {
		return;
	}
	CatalogPartDialog dialog(m_catalog, m_selected, this);
	if (dialog.exec() == QDialog::Accepted)
	{
		fillManufacturerFilter();
		search();
	}
}

/**
	@brief CatalogBrowserDialog::duplicateSelectedPart
	Opens a new part with this one's values and an empty code.

	Registering a family of parts — the same product with a different range,
	voltage or rating — meant retyping everything because of one field. What
	is **not** copied is what identifies the product: the code, and the
	revision. Everything else comes along, pins and accessories included,
	because in a family it is precisely the rest that repeats.
*/
void CatalogBrowserDialog::duplicateSelectedPart()
{
	if (m_selected.isNull()) {
		return;
	}

	CatalogPart copy = m_selected;
	copy.id = 0;
	copy.code.clear();
	copy.revision = 1;
	copy.is_current = true;
	copy.origin.clear();
	copy.origin_date.clear();

	CatalogPartDialog dialog(m_catalog, copy, this);
	if (dialog.exec() == QDialog::Accepted)
	{
		fillManufacturerFilter();
		search();
	}
}
/**
	@brief CatalogBrowserDialog::removeSelectedPart
*/
void CatalogBrowserDialog::removeSelectedPart()
{
	if (m_selected.isNull()) {
		return;
	}

	if (QMessageBox::question(this, tr("Supprimer la pièce"),
				  tr("Supprimer « %1 » du catalogue ? Les projets qui la référencent "
				     "gardent ce qu'ils ont enregistré, mais ne trouveront plus la "
				     "pièce.").arg(m_selected.code))
	    != QMessageBox::Yes)
	{
		return;
	}

	QString error;
	if (!m_catalog->removePart(m_selected.id, &error))
	{
		QMessageBox::warning(this, tr("Pièce conservée"), error);
		return;
	}
	search();
}

/**
	@brief CatalogBrowserDialog::acceptSelection
*/
void CatalogBrowserDialog::acceptSelection()
{
	if (m_selected.isNull()) {
		return;
	}
	accept();
}

/**
	@brief CatalogBrowserDialog::selectedPart
	@return the part the user picked
*/
CatalogPart CatalogBrowserDialog::selectedPart() const
{
	return m_selected;
}

/**
	@brief CatalogBrowserDialog::setPartTemplate
	@param part : what « New part » starts from
*/
void CatalogBrowserDialog::setPartTemplate(const CatalogPart &part)
{
		//A template part has neither code nor id, so isNull() would call a
		//perfectly useful template "empty". The caller is the one who knows.
	m_template = part;
	m_has_template = true;
}

/**
	@brief CatalogBrowserDialog::setClassFilter
	@param class_id : the only class the browser offers, 0 for all

	The restriction belongs to whoever opened the browser, not to the
	search, so « Effacer les filtres » comes back to it rather than to
	« Toutes ».
*/
void CatalogBrowserDialog::setClassFilter(int class_id)
{
	m_forced_class = class_id;

	const int index = m_class_filter->findData(class_id);
	if (index < 0) {
		return;
	}

	m_class_filter->setCurrentIndex(index);
	search();
}

/**
	@brief CatalogBrowserDialog::choosePart
	@param catalog
	@param parent
	@param part_template : what « New part » starts from, an empty part to
	start from nothing
	@return the part the user picked, a null part when cancelled
*/
CatalogPart CatalogBrowserDialog::choosePart(Catalog *catalog,
					     QWidget *parent,
					     const CatalogPart &part_template,
					     int class_filter)
{
	CatalogBrowserDialog dialog(catalog, parent);
	dialog.setPartTemplate(part_template);
	if (class_filter > 0) {
		dialog.setClassFilter(class_filter);
	}
	if (dialog.exec() != QDialog::Accepted) {
		return CatalogPart();
	}
	return dialog.selectedPart();
}
