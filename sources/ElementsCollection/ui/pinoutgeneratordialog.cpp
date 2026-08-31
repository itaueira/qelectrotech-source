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
#include "pinoutgeneratordialog.h"

#include "symbolpreview.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QUuid>
#include <QVBoxLayout>

#include "../../catalog/catalog.h"
#include "../../catalog/catalogassignment.h"
#include "../../catalog/ui/catalogbrowserdialog.h"
#include "../../properties/elementdata.h"
#include "../../qetapp.h"
#include "../../qetinformation.h"
#include "../pinoutgenerator.h"
#include "../pinoutusagescanner.h"

namespace
{
	enum PinColumn
	{
		ColumnLabel = 0,
		ColumnSecondary,
		ColumnRole,
		ColumnChannel,
		ColumnPair,
		ColumnConnector,
		ColumnCount
	};

		/**
			@return which kind of point of a PLC a block made of @a pins
			is, in the words the component itself uses, or an empty
			string when the block is of no one kind. Two vocabularies
			meet here and nowhere else: the catalogue says Input, the
			list of points says the translated name of a digital input,
			and neither of the two has any business knowing the other.
		*/
	QString plcTypeOf(const QList<CatalogPin> &pins)
	{
		switch (PinoutGenerator::ioRoleOf(pins))
		{
			case CatalogPinRole::Input:
				return ElementData::translatedPlcIOType(
						ElementData::EntreeDigitale);
			case CatalogPinRole::Output:
			case CatalogPinRole::OutputRelay:
				return ElementData::translatedPlcIOType(
						ElementData::SortieDigitale);
			case CatalogPinRole::InputAnalog:
				return ElementData::translatedPlcIOType(
						ElementData::EntreeAnalogique);
			case CatalogPinRole::OutputAnalog:
				return ElementData::translatedPlcIOType(
						ElementData::SortieAnalogique);
			default:
				return QString();
		}
	}

		/**
			@brief Put into @a block what the catalogue already knows.

			The values of the part, so the component comes out of the
			panel already filled; and the two fields the list of points
			reads. The address and the comment are deliberately not
			written: those belong to the point on the sheet and not to
			the model of the card, and filling them here would be
			filling them wrong for every card but the first.
		*/
	void addCatalogValues(const Catalog *catalog, const CatalogPart &part,
			      const CatalogClass &catalog_class,
			      const QList<CatalogPin> &pins,
			      SymbolDefinition &block)
	{
		if (!catalog) {
			return;
		}

		const QHash<QString, QString> values =
				CatalogAssignment::valuesForElement(*catalog, part);
		const QStringList keys = values.keys();
		for (const QString &key : keys) {
			block.default_part_values.insert(key, values.value(key));
		}

		const QString type = plcTypeOf(pins);
		if (!type.isEmpty()) {
			block.default_part_values.insert(
					QETInformation::ELMT_PLC_TYPE, type);
		}

			//What the card does, which is a sentence the catalogue has
			//in the designation of the part. Failing that, the name of
			//the class is the truest short thing there is to say.
		QString function = values.value(
				QETInformation::ELMT_DESIGNATION).trimmed();
		if (function.isEmpty()) {
			function = catalog_class.name.trimmed();
		}
		if (!function.isEmpty()) {
			block.default_part_values.insert(
					QETInformation::ELMT_PLC_FUNCTION, function);
		}
	}
}

PinoutGeneratorDialog::PinoutGeneratorDialog(Catalog *catalog,
					     QETProject *project,
					     QWidget *parent) :
	QDialog(parent),
	m_catalog(catalog),
	m_project(project),
	m_convention(PinoutConvention::current())
{
	setUpWidget();
	showTemplate();
	regenerate();
}

QList<SymbolDefinition> PinoutGeneratorDialog::blocks() const
{
	return m_blocks;
}

QStringList PinoutGeneratorDialog::savedPaths() const
{
	return m_saved_paths;
}

void PinoutGeneratorDialog::setUpWidget()
{
	setWindowTitle(tr("Générer des blocs à partir du brochage"));

	QVBoxLayout *layout = new QVBoxLayout(this);
	QHBoxLayout *columns = new QHBoxLayout();
	QVBoxLayout *left = new QVBoxLayout();
	QVBoxLayout *right = new QVBoxLayout();

		//The three things nobody but a person can know.
	QFormLayout *form = new QFormLayout();
	QHBoxLayout *part_layout = new QHBoxLayout();
	m_part_label = new QLabel(tr("aucune"), this);
	m_part_button = new QPushButton(tr("Choisir…"), this);
	part_layout->addWidget(m_part_label, 1);
	part_layout->addWidget(m_part_button);
	form->addRow(tr("Pièce :"), part_layout);

	m_class_label = new QLabel(this);
	form->addRow(tr("Classe :"), m_class_label);

	m_component = new QLineEdit(this);
	m_component->setPlaceholderText(tr("-U1"));
	form->addRow(tr("Composant :"), m_component);
	left->addLayout(form);

	QGroupBox *pins_group = new QGroupBox(tr("Points à dessiner"), this);
	QVBoxLayout *pins_layout = new QVBoxLayout(pins_group);
	m_pins = new QTableWidget(0, ColumnCount, pins_group);
	m_pins->setHorizontalHeaderLabels(QStringList()
			<< tr("Borne") << tr("Repère") << tr("Type")
			<< tr("Voie") << tr("Paire") << tr("Connecteur"));
	m_pins->verticalHeader()->setVisible(false);
	m_pins->horizontalHeader()->setStretchLastSection(true);
	m_pins->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pins->setEditTriggers(QAbstractItemView::NoEditTriggers);
	pins_layout->addWidget(m_pins);

	QHBoxLayout *check_layout = new QHBoxLayout();
	m_check_all = new QPushButton(tr("Tout cocher"), pins_group);
	m_check_none = new QPushButton(tr("Tout décocher"), pins_group);
	check_layout->addWidget(m_check_all);
	check_layout->addWidget(m_check_none);
	check_layout->addStretch();
	pins_layout->addLayout(check_layout);
	left->addWidget(pins_group, 1);

	QGroupBox *template_group = new QGroupBox(tr("Modèle de la classe"), this);
	QFormLayout *template_form = new QFormLayout(template_group);
	m_width = new QSpinBox(template_group);
	m_width->setRange(2, 100);
	m_width->setSuffix(tr(" pas"));
	template_form->addRow(tr("Largeur :"), m_width);

	m_pitch = new QSpinBox(template_group);
	m_pitch->setRange(1, 20);
	m_pitch->setSuffix(tr(" pas"));
	template_form->addRow(tr("Espacement :"), m_pitch);

	m_margin = new QSpinBox(template_group);
	m_margin->setRange(0, 20);
	m_margin->setSuffix(tr(" pas"));
	template_form->addRow(tr("Marge :"), m_margin);

	m_max_terminals = new QSpinBox(template_group);
	m_max_terminals->setRange(0, 999);
	m_max_terminals->setSpecialValueText(tr("sans limite"));
	template_form->addRow(tr("Bornes par bloc :"), m_max_terminals);

		//Decision D, said where the change is made. The behaviour is the
		//one asked for, and a right behaviour nobody was told about
		//reads as a bug the first time somebody notices it.
	m_template_note = new QLabel(template_group);
	m_template_note->setWordWrap(true);
	m_template_note->setStyleSheet(QStringLiteral("QLabel { color : #8a6d00; }"));
	m_template_note->setText(tr("Modifier le modèle change les blocs générés "
				    "à partir de maintenant. Les cartes déjà "
				    "insérées dans un projet gardent leur dessin : "
				    "ce sont des fichiers, et non des vues du "
				    "modèle."));
	template_form->addRow(m_template_note);

	m_save_template = new QPushButton(tr("Enregistrer dans la classe"),
					  template_group);
	template_form->addRow(m_save_template);

	m_convention_note = new QLabel(template_group);
	m_convention_note->setWordWrap(true);
	template_form->addRow(m_convention_note);
	left->addWidget(template_group);

	m_block = new QComboBox(this);
	right->addWidget(m_block);
	m_preview = new SymbolPreview(this);
	right->addWidget(m_preview, 1);

	QHBoxLayout *folder_layout = new QHBoxLayout();
	folder_layout->addWidget(new QLabel(tr("Dossier :"), this));
	m_folder = new QLineEdit(QETApp::companyElementsDir(), this);
	m_folder_button = new QPushButton(tr("Parcourir…"), this);
	folder_layout->addWidget(m_folder, 1);
	folder_layout->addWidget(m_folder_button);
	right->addLayout(folder_layout);

	columns->addLayout(left, 3);
	columns->addLayout(right, 2);
	layout->addLayout(columns, 1);

	m_refusal = new QLabel(this);
	m_refusal->setWordWrap(true);
	m_refusal->setStyleSheet(QStringLiteral("QLabel { color : #a00000; }"));
	layout->addWidget(m_refusal);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	QDialogButtonBox *box = new QDialogButtonBox(this);
	m_save = box->addButton(tr("Enregistrer les blocs"),
				QDialogButtonBox::AcceptRole);
	box->addButton(QDialogButtonBox::Cancel);
	layout->addWidget(box);

	connect(m_part_button, &QPushButton::clicked,
		this, &PinoutGeneratorDialog::choosePart);
	connect(m_check_all, &QPushButton::clicked,
		this, &PinoutGeneratorDialog::checkEveryPin);
	connect(m_check_none, &QPushButton::clicked,
		this, &PinoutGeneratorDialog::checkNoPin);
	connect(m_pins, &QTableWidget::itemChanged,
		this, &PinoutGeneratorDialog::pinsChanged);
	connect(m_component, &QLineEdit::textChanged,
		this, &PinoutGeneratorDialog::refreshRefusal);
	connect(m_width, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &PinoutGeneratorDialog::templateChanged);
	connect(m_pitch, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &PinoutGeneratorDialog::templateChanged);
	connect(m_margin, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &PinoutGeneratorDialog::templateChanged);
	connect(m_max_terminals, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &PinoutGeneratorDialog::templateChanged);
	connect(m_save_template, &QPushButton::clicked,
		this, &PinoutGeneratorDialog::saveTemplateIntoClass);
	connect(m_block, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &PinoutGeneratorDialog::blockSelected);
	connect(m_folder_button, &QPushButton::clicked,
		this, &PinoutGeneratorDialog::chooseFolder);
	connect(m_save, &QPushButton::clicked,
		this, &PinoutGeneratorDialog::save);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PinoutGeneratorDialog::choosePart()
{
	if (!m_catalog)
	{
		QMessageBox::information(this, tr("Générer des blocs"),
				tr("Le catalogue n'est pas disponible."));
		return;
	}

	const CatalogPart part = CatalogBrowserDialog::choosePart(m_catalog, this);
	if (part.isNull()) {
		return;
	}

	m_part = part;
	m_class = m_catalog->classById(part.class_id);
	m_part_label->setText(m_part.code);
	m_class_label->setText(m_class.name.isEmpty()
			       ? tr("hors classe") : m_class.name);

		//The template belongs to the class, so choosing a part of another
		//class brings another template with it.
	const PinoutBlockTemplate stored =
			PinoutBlockTemplate::fromXml(m_class.block_template);
	m_template = stored.isNull() ? PinoutBlockTemplate() : stored;

	showTemplate();
	fillPins();
	regenerate();
}

void PinoutGeneratorDialog::fillPins()
{
	m_filling = true;
	m_pins->clearContents();
	m_pins->setRowCount(m_part.pins.size());

	for (int row = 0 ; row < m_part.pins.size() ; ++row)
	{
		const CatalogPin &pin = m_part.pins.at(row);

		QTableWidgetItem *label = new QTableWidgetItem(pin.label);
		label->setFlags((label->flags() | Qt::ItemIsUserCheckable)
				& ~Qt::ItemIsEditable);
		label->setCheckState(Qt::Checked);
		m_pins->setItem(row, ColumnLabel, label);

		m_pins->setItem(row, ColumnSecondary,
				new QTableWidgetItem(pin.secondary_label));
		m_pins->setItem(row, ColumnRole,
				new QTableWidgetItem(
					CatalogPin::translatedRoleName(pin.role)));
		m_pins->setItem(row, ColumnChannel,
				new QTableWidgetItem(pin.channel));
		m_pins->setItem(row, ColumnPair,
				new QTableWidgetItem(pin.pair));
		m_pins->setItem(row, ColumnConnector,
				new QTableWidgetItem(pin.connector));
	}
	m_pins->resizeColumnsToContents();
	m_filling = false;
}

QStringList PinoutGeneratorDialog::selectedPinLabels() const
{
	QStringList labels;
	int checked = 0;

	for (int row = 0 ; row < m_pins->rowCount() ; ++row)
	{
		const QTableWidgetItem *item = m_pins->item(row, ColumnLabel);
		if (!item || item->checkState() != Qt::Checked) {
			continue;
		}
		checked++;
		if (!item->text().isEmpty()) {
			labels << item->text();
		}
	}

		//Every point ticked is said by naming none of them, because that
		//is what the generator reads as the whole part: thirty two names
		//and no list at all draw the same block today, and the empty one
		//keeps drawing the whole card if the part gains a pin tomorrow.
	if (checked == m_pins->rowCount()) {
		return QStringList();
	}
	return labels;
}

void PinoutGeneratorDialog::checkEveryPin()
{
	m_filling = true;
	for (int row = 0 ; row < m_pins->rowCount() ; ++row)
	{
		if (QTableWidgetItem *item = m_pins->item(row, ColumnLabel)) {
			item->setCheckState(Qt::Checked);
		}
	}
	m_filling = false;
	regenerate();
}

void PinoutGeneratorDialog::checkNoPin()
{
	m_filling = true;
	for (int row = 0 ; row < m_pins->rowCount() ; ++row)
	{
		if (QTableWidgetItem *item = m_pins->item(row, ColumnLabel)) {
			item->setCheckState(Qt::Unchecked);
		}
	}
	m_filling = false;
	regenerate();
}

void PinoutGeneratorDialog::pinsChanged()
{
	if (m_filling) {
		return;
	}
	regenerate();
}

void PinoutGeneratorDialog::templateChanged()
{
	if (m_filling) {
		return;
	}
	regenerate();
}

void PinoutGeneratorDialog::readTemplate()
{
	m_template.width_steps = m_width->value();
	m_template.pitch_steps = m_pitch->value();
	m_template.margin_steps = m_margin->value();
	m_template.max_terminals = m_max_terminals->value();
}

void PinoutGeneratorDialog::showTemplate()
{
	m_filling = true;
	m_width->setValue(m_template.width_steps);
	m_pitch->setValue(m_template.pitch_steps);
	m_margin->setValue(m_template.margin_steps);
	m_max_terminals->setValue(m_template.max_terminals);
	m_filling = false;

		//Decision E: the convention is the workshop's, not the dialog's.
		//What is said here is only which one is in force, so that a block
		//coming out with its inputs on top is not read as a choice this
		//dialog made on its own.
	m_convention_note->setText(PinoutConvention::isConfigured()
			? tr("Convention de l'atelier : %1.").arg(
				PinoutConvention::translatedName(m_convention.key))
			: tr("Aucune convention n'a été choisie pour l'atelier : "
			     "les blocs suivent la CEI."));
}

void PinoutGeneratorDialog::saveTemplateIntoClass()
{
	if (!m_catalog || m_class.id <= 0)
	{
		QMessageBox::information(this, tr("Modèle de la classe"),
				tr("Choisir d'abord une pièce : le modèle "
				   "s'enregistre sur la classe de la pièce."));
		return;
	}
	if (!m_catalog->isWritable())
	{
		QMessageBox::information(this, tr("Modèle de la classe"),
				tr("Le catalogue est en lecture seule."));
		return;
	}

	readTemplate();
	QString error;
	if (!m_template.isValid(&error))
	{
		QMessageBox::warning(this, tr("Modèle de la classe"), error);
		return;
	}

	CatalogClass edited = m_class;
	edited.block_template = m_template.toXml();
	if (!m_catalog->updateClass(edited, &error))
	{
		QMessageBox::warning(this, tr("Modèle de la classe"),
				error.isEmpty()
				? tr("Le modèle n'a pas pu être enregistré.")
				: error);
		return;
	}
	m_class = edited;

		//Said a second time, and after the fact, because this is the
		//moment the change stops being this dialog's and becomes
		//everyone's.
	QMessageBox::information(this, tr("Modèle de la classe"),
			tr("Le modèle de la classe « %1 » est enregistré. Les "
			   "blocs générés à partir de maintenant le suivent ; "
			   "les cartes déjà insérées gardent leur dessin.")
			.arg(m_class.name));
}

void PinoutGeneratorDialog::regenerate()
{
	m_blocks.clear();
	readTemplate();

	if (!m_part.isNull())
	{
		const PinoutGenerator generator(m_template, m_convention,
						m_grid, m_class.key);
		if (generator.isValid())
		{
			const QStringList labels = selectedPinLabels();
			const QList<QList<CatalogPin> > pins =
					generator.split(m_part, labels);
			m_blocks = generator.generate(m_part, labels);

				//The same arguments give the same partition in
				//the same order, so the block and the pins it
				//was made of are found by the same index.
			for (int index = 0 ; index < m_blocks.size() ; ++index)
			{
				addCatalogValues(m_catalog, m_part, m_class,
						 index < pins.size()
						 ? pins.at(index)
						 : QList<CatalogPin>(),
						 m_blocks[index]);
			}
		}
	}

	describeBlocks();
	refreshRefusal();
}

void PinoutGeneratorDialog::describeBlocks()
{
	m_filling = true;
	m_block->clear();
	for (const SymbolDefinition &block : m_blocks) {
		m_block->addItem(block.name);
	}
	m_filling = false;
	m_block->setEnabled(m_blocks.size() > 1);

	m_preview->setSymbol(m_blocks.isEmpty() ? SymbolDefinition()
						: m_blocks.first());
}

void PinoutGeneratorDialog::refreshRefusal()
{
		//The count first, because it is what is looked at while the
		//points are being ticked. Written here and not in describeBlocks
		//so that typing the name of the component makes the note about
		//the missing name go away as it is typed.
	QStringList said;
	if (m_part.isNull())
	{
		said << tr("Choisir une pièce pour voir les blocs qu'elle donne.");
	}
	else
	{
		int terminals = 0;
		for (const SymbolDefinition &block : m_blocks) {
			terminals += block.terminals.size();
		}
		said << tr("%n bloc(s) à dessiner, ", "", m_blocks.size())
				+ tr("%n borne(s) au total.", "", terminals);

		if (m_component->text().trimmed().isEmpty())
		{
				//Not a refusal: a block that has not been named
				//yet is not attributable to any component, and
				//refusing it would refuse most drawings halfway
				//through being made. But the person is owed the
				//reason the check is silent.
			said << tr("Sans nom de composant, les bornes déjà "
				   "dessinées dans le projet ne peuvent pas "
				   "être vérifiées.");
		}
	}
	m_summary->setText(said.join(QStringLiteral("\n")));

	QStringList lines;

		//What the drawing cannot be wherever it is put: a block with no
		//class, with a contact whose other half is missing, with a
		//terminal off the grid a conductor can reach.
	for (const SymbolDefinition &block : m_blocks)
	{
		const QStringList problems = block.problemMessages(m_grid);
		for (const QString &problem : problems)
		{
			const QString line = m_blocks.size() > 1
					? tr("%1 : %2").arg(block.name, problem)
					: problem;
			if (!lines.contains(line)) {
				lines << line;
			}
		}
	}

		//And what this project will not let it be. Read once, then asked
		//of every block in turn against the same usage, so that two
		//blocks of the same insertion colliding with each other is found
		//too and not only a collision with what was already on a folio.
	PinoutUsage usage = PinoutUsageScanner::scan(m_project);
	QList<PinoutUsageConflict> conflicts;

	PinoutUsageEntry place;
	place.component = m_component->text().trimmed();
	place.part_code = m_part.code;

	for (const SymbolDefinition &block : m_blocks)
	{
		conflicts << PinoutUsage::selfConflicts(block);
		usage.addSymbol(place, block, &conflicts);
	}

	const QString message = PinoutUsage::messageFor(conflicts);
	if (!message.isEmpty()) {
		lines << message;
	}

	m_refusal->setText(lines.join(QStringLiteral("\n")));
		//Barred at the insertion, and not discovered in the terminal
		//list: by the time the list comes out with two lines for the
		//same screw, one of the two has been wired.
	m_save->setEnabled(!m_blocks.isEmpty() && lines.isEmpty());
}

void PinoutGeneratorDialog::blockSelected(int index)
{
	if (m_filling || index < 0 || index >= m_blocks.size()) {
		return;
	}
	m_preview->setSymbol(m_blocks.at(index));
}

void PinoutGeneratorDialog::chooseFolder()
{
	const QString folder = QFileDialog::getExistingDirectory(
				this, tr("Dossier de la collection"),
				m_folder->text());
	if (!folder.isEmpty()) {
		m_folder->setText(folder);
	}
}

QString PinoutGeneratorDialog::targetFolder() const
{
	return m_folder->text().trimmed();
}

void PinoutGeneratorDialog::save()
{
	regenerate();
	if (m_blocks.isEmpty() || !m_refusal->text().isEmpty()) {
		return;
	}

	const QString folder = targetFolder();
	QDir dir(folder);
	if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
	{
		QMessageBox::warning(this, tr("Enregistrer les blocs"),
			tr("Le dossier « %1 » n'existe pas et n'a pas pu être "
			   "créé.").arg(folder));
		return;
	}

	QStringList bases;
	int existing = 0;
	for (const SymbolDefinition &block : m_blocks)
	{
		QString base = SymbolDefinition::fileNameFor(block.name);
		if (base.isEmpty()) {
			base = tr("bloc");
		}
			//Two blocks of one part whose names come down to the same
			//file name would write one over the other, and the second
			//would be the only one left.
		QString unique = base;
		int suffix = 2;
		while (bases.contains(unique))
		{
			unique = QStringLiteral("%1_%2").arg(base).arg(suffix);
			suffix++;
		}
		bases << unique;

		if (QFile::exists(dir.absoluteFilePath(
					  unique + QStringLiteral(".elmt")))) {
			existing++;
		}
	}

	bool as_revision = false;
	if (existing > 0)
	{
			//The question of T12, asked once for the whole set: a
			//symbol already used is either changed everywhere or left
			//alone and superseded. Guessing on the designer's
			//behalf is how a delivered project quietly changes
			//drawing.
		QMessageBox question(this);
		question.setWindowTitle(tr("Des blocs existent déjà"));
		question.setIcon(QMessageBox::Question);
		question.setText(tr("%n bloc(s) de cette pièce sont déjà dans "
				    "ce dossier.", "", existing));
		question.setInformativeText(
			tr("Remplacer change le dessin dans tous les projets qui "
			   "les utilisent, y compris ceux déjà livrés.\n\n"
			   "Enregistrer une révision laisse ces projets comme "
			   "ils sont et ne sert que pour les suivants."));
		QPushButton *replace = question.addButton(
					tr("Remplacer partout"),
					QMessageBox::DestructiveRole);
		QPushButton *revision = question.addButton(
					tr("Enregistrer une révision"),
					QMessageBox::AcceptRole);
		question.addButton(QMessageBox::Cancel);
		question.setDefaultButton(revision);
		question.exec();

		if (question.clickedButton() == revision) {
			as_revision = true;
		} else if (question.clickedButton() != replace) {
			return;
		}
	}

	m_saved_paths.clear();
	for (int index = 0 ; index < m_blocks.size() ; ++index)
	{
		SymbolDefinition &block = m_blocks[index];
		const QString base = bases.at(index);
		QString path = dir.absoluteFilePath(base + QStringLiteral(".elmt"));

		if (as_revision && QFile::exists(path))
		{
			int number = 2;
			while (QFile::exists(dir.absoluteFilePath(
					QStringLiteral("%1_r%2.elmt")
					.arg(base).arg(number)))) {
				number++;
			}
			path = dir.absoluteFilePath(QStringLiteral("%1_r%2.elmt")
						    .arg(base).arg(number));
			block.revision = number;
				//A new revision is a new symbol as far as
				//anything comparing symbols is concerned, so it
				//gets its own identity.
			block.uuid = QUuid::createUuid();
		}

		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			QMessageBox::warning(this, tr("Enregistrer les blocs"),
				tr("Impossible d'écrire « %1 » : %2")
					.arg(path, file.errorString()));
			return;
		}
		QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		stream.setCodec("UTF-8");
#endif
		stream << block.toXml().toString(4);
		file.close();

		m_saved_paths << path;
	}
	accept();
}
