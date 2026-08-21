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
#include "createsymboldialog.h"

#include "symbolpreview.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include "../../catalog/catalog.h"
#include "../../catalog/ui/catalogbrowserdialog.h"
#include "../../catalog/catalogassignment.h"
#include "../../catalog/catalogclass.h"
#include "../../catalog/catalogproperty.h"
#include "../../qetapp.h"

namespace
{
	enum TerminalColumn
	{
		ColumnPosition = 0,
		ColumnOrientation,
		ColumnLabel,
		ColumnRole,
		ColumnPair,
		ColumnCount
	};
}

CreateSymbolDialog::CreateSymbolDialog(const SymbolDefinition &symbol,
				       Catalog *catalog,
				       QWidget *parent) :
	QDialog(parent),
	m_symbol(symbol),
	m_catalog(catalog)
{
	setWindowTitle(tr("Créer un symbole à partir du dessin"));
	setMinimumSize(760, 620);
	setUpWidget();

		//Push what the drawing gave onto the grid before showing it, and say
		//so: the projectist sees the numbers that will be saved, not the ones
		//they drew and would have to guess about.
	const SymbolSnapReport report = m_symbol.snapToGrid(m_grid);
	if (!report.isEmpty()) {
		m_snap_note->setText(
			tr("%n point(s) de raccordement ont été ramenés sur la grille "
			   "principale, de %1 unité(s) au plus. Vérifiez qu'ils sont "
			   "toujours au bout du trait qui les porte.",
			   "", report.moved)
				.arg(report.largest_move, 0, 'f', 1));
		m_snap_note->setVisible(true);
	} else {
		m_snap_note->setVisible(false);
	}

	fillTerminals();
	refreshProblems();
}

SymbolDefinition CreateSymbolDialog::symbol() const
{
	return m_symbol;
}

QString CreateSymbolDialog::savedPath() const
{
	return m_saved_path;
}

void CreateSymbolDialog::setUpWidget()
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	QFormLayout *form = new QFormLayout();
	m_name = new QLineEdit(m_symbol.name, this);
	m_name->setPlaceholderText(tr("Contacteur 3 pôles, bobine…"));
	form->addRow(tr("Nom :"), m_name);

	m_class = new QComboBox(this);
	m_class->addItem(tr("— choisir une classe —"), QString());
	if (m_catalog) {
		const QList<CatalogClass> classes = m_catalog->classes();
		for (const CatalogClass &catalog_class : classes) {
			m_class->addItem(catalog_class.name, catalog_class.key);
		}
	}
	const int class_index = m_class->findData(m_symbol.class_key);
	if (class_index >= 0) {
		m_class->setCurrentIndex(class_index);
	}
	form->addRow(tr("Classe :"), m_class);

	m_link_type = new QComboBox(this);
	for (SymbolLinkType type : {SymbolLinkType::Simple,
				    SymbolLinkType::Master,
				    SymbolLinkType::Slave}) {
		m_link_type->addItem(SymbolDefinition::translatedLinkType(type),
				     int(type));
	}
	form->addRow(tr("Rôle dans le renvoi de folio :"), m_link_type);

		//Optional, and normally left empty: one contactor symbol serves twenty
		//contactors. Worth having for the ones the shop always buys, where
		//filling the part in again on every insertion is typing the same
		//answer forever.
	QHBoxLayout *part_layout = new QHBoxLayout();
	m_default_part = new QLabel(tr("aucune — symbole générique"), this);
	m_choose_part = new QPushButton(tr("Choisir…"), this);
	m_clear_part = new QPushButton(tr("Retirer"), this);
	part_layout->addWidget(m_default_part, 1);
	part_layout->addWidget(m_choose_part);
	part_layout->addWidget(m_clear_part);
	form->addRow(tr("Pièce par défaut :"), part_layout);

	QHBoxLayout *folder_layout = new QHBoxLayout();
	m_folder = new QLineEdit(QETApp::companyElementsDir(), this);
	m_folder_button = new QPushButton(tr("Parcourir…"), this);
	folder_layout->addWidget(m_folder);
	folder_layout->addWidget(m_folder_button);
	form->addRow(tr("Dossier :"), folder_layout);
	layout->addLayout(form);

	m_snap_note = new QLabel(this);
	m_snap_note->setWordWrap(true);
	m_snap_note->setStyleSheet(QStringLiteral("QLabel { color : #8a6d00; }"));
	layout->addWidget(m_snap_note);

	QGroupBox *terminals_box = new QGroupBox(
				tr("Points de raccordement et contacts"), this);
	QVBoxLayout *terminals_layout = new QVBoxLayout(terminals_box);

	QLabel *explanation = new QLabel(
		tr("Le repère est provisoire : quand une pièce du catalogue sera "
		   "attribuée au composant, ses numéros réels prendront la place. "
		   "Le type de contact, lui, appartient au symbole — c'est lui qui "
		   "permettra de compter les contacts libres."), terminals_box);
	explanation->setWordWrap(true);
	terminals_layout->addWidget(explanation);

	QHBoxLayout *table_and_preview = new QHBoxLayout();
	m_terminals = new QTableWidget(0, ColumnCount, terminals_box);
	m_terminals->setHorizontalHeaderLabels(
				QStringList{tr("Position"),
					    tr("Côté"),
					    tr("Repère provisoire"),
					    tr("Type de contact"),
					    tr("Paire")});
	m_terminals->horizontalHeader()->setStretchLastSection(true);
	m_terminals->verticalHeader()->setVisible(false);
	m_terminals->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_and_preview->addWidget(m_terminals, 3);

		//O desenho ao lado da tabela. A tabela diz o que o ponto é; o desenho
		//diz qual ponto é. Sem o segundo, declarar contato é adivinhação — foi
		//exatamente o que o teste E.3 mostrou.
	m_preview = new SymbolPreview(terminals_box);
	table_and_preview->addWidget(m_preview, 2);
	terminals_layout->addLayout(table_and_preview);

	QHBoxLayout *buttons = new QHBoxLayout();
	m_add_terminal = new QPushButton(tr("Ajouter un point"), terminals_box);
	m_remove_terminal = new QPushButton(tr("Retirer"), terminals_box);
	m_pair = new QPushButton(tr("Former une paire"), terminals_box);
	m_pair->setToolTip(tr("Sélectionnez les deux points d'un même contact, "
			      "puis déclarez ce qu'il est."));
	m_unpair = new QPushButton(tr("Défaire la paire"), terminals_box);
	buttons->addWidget(m_add_terminal);
	buttons->addWidget(m_remove_terminal);
	buttons->addStretch();
	buttons->addWidget(m_pair);
	buttons->addWidget(m_unpair);
	terminals_layout->addLayout(buttons);
	layout->addWidget(terminals_box, 1);

	QGroupBox *properties_box = new QGroupBox(
				tr("Valeurs à afficher à côté du symbole"), this);
	QVBoxLayout *properties_layout = new QVBoxLayout(properties_box);
	QLabel *properties_note = new QLabel(
		tr("Le repère du composant est toujours affiché. Cochez ici ce qui "
		   "doit apparaître en plus : le calibre du fusible, la puissance du "
		   "moteur, le code de la pièce."), properties_box);
	properties_note->setWordWrap(true);
	properties_layout->addWidget(properties_note);
	m_properties = new QListWidget(properties_box);
	m_properties->setMaximumHeight(110);
	properties_layout->addWidget(m_properties);
	layout->addWidget(properties_box);

	m_problems = new QLabel(this);
	m_problems->setWordWrap(true);
	m_problems->setStyleSheet(QStringLiteral("QLabel { color : #a00000; }"));
	layout->addWidget(m_problems);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	QDialogButtonBox *box = new QDialogButtonBox(this);
	m_save = box->addButton(tr("Enregistrer le symbole"),
				QDialogButtonBox::AcceptRole);
	box->addButton(QDialogButtonBox::Cancel);
	layout->addWidget(box);

	connect(m_save, &QPushButton::clicked, this, &CreateSymbolDialog::save);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_folder_button, &QPushButton::clicked,
		this, &CreateSymbolDialog::chooseFolder);
	connect(m_choose_part, &QPushButton::clicked,
		this, &CreateSymbolDialog::chooseDefaultPart);
	connect(m_clear_part, &QPushButton::clicked,
		this, &CreateSymbolDialog::clearDefaultPart);
	connect(m_add_terminal, &QPushButton::clicked,
		this, &CreateSymbolDialog::addTerminal);
	connect(m_remove_terminal, &QPushButton::clicked,
		this, &CreateSymbolDialog::removeTerminal);
	connect(m_pair, &QPushButton::clicked,
		this, &CreateSymbolDialog::pairSelected);
	connect(m_unpair, &QPushButton::clicked,
		this, &CreateSymbolDialog::unpairSelected);
	connect(m_terminals, &QTableWidget::itemChanged,
		this, &CreateSymbolDialog::terminalsChanged);
	connect(m_terminals, &QTableWidget::currentCellChanged,
		this, &CreateSymbolDialog::terminalRowChanged);
	connect(m_preview, &SymbolPreview::terminalPicked,
		this, &CreateSymbolDialog::terminalPickedInPreview);
	connect(m_name, &QLineEdit::textChanged,
		this, &CreateSymbolDialog::terminalsChanged);
	connect(m_class,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &CreateSymbolDialog::terminalsChanged);
}

void CreateSymbolDialog::fillTerminals()
{
	const bool blocked = m_terminals->blockSignals(true);
	m_terminals->setRowCount(m_symbol.terminals.size());

	for (int row = 0 ; row < m_symbol.terminals.size() ; ++row) {
		const SymbolTerminal &terminal = m_symbol.terminals.at(row);

			//Editable, and this is the escape hatch of the whole dialog: the
			//connection points are deduced from the free ends of the drawing,
			//which covers the drawing anybody actually makes. When it does not
			//- a point wanted where no line ends - the position is typed here
			//and snapped to the main grid on the way in.
		QTableWidgetItem *position = new QTableWidgetItem(
					QStringLiteral("%1 ; %2")
					.arg(terminal.position.x())
					.arg(terminal.position.y()));
		position->setToolTip(tr("x ; y — sera ramené sur la grille principale"));
		m_terminals->setItem(row, ColumnPosition, position);

		QComboBox *orientation = new QComboBox(m_terminals);
		for (Qet::Orientation value : SymbolTerminal::allOrientations()) {
			orientation->addItem(
						SymbolTerminal::translatedOrientation(value),
						int(value));
		}
		orientation->setCurrentIndex(
					orientation->findData(int(terminal.orientation)));
		connect(orientation, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &CreateSymbolDialog::terminalsChanged);
		m_terminals->setCellWidget(row, ColumnOrientation, orientation);

		m_terminals->setItem(row, ColumnLabel,
				     new QTableWidgetItem(terminal.label));

		QComboBox *role = new QComboBox(m_terminals);
		const QList<CatalogPinRole> roles = CatalogPin::allRoles();
		for (CatalogPinRole value : roles) {
			role->addItem(CatalogPin::translatedRoleName(value), int(value));
		}
		role->setCurrentIndex(role->findData(int(terminal.role)));
		connect(role, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &CreateSymbolDialog::terminalsChanged);
		m_terminals->setCellWidget(row, ColumnRole, role);

		m_terminals->setItem(row, ColumnPair,
				     new QTableWidgetItem(terminal.pair));
	}

	m_terminals->resizeColumnsToContents();
	m_terminals->blockSignals(blocked);

	if (m_preview) {
		m_preview->setSymbol(m_symbol);
		m_preview->setHighlighted(m_terminals->currentRow());
	}

		//The properties of the chosen class, so a value can be shown next to
		//the symbol without anybody having to remember its key.
	m_properties->clear();
	if (m_catalog) {
		const QString key = m_class->currentData().toString();
		const CatalogClass catalog_class = m_catalog->classByKey(key);
		if (catalog_class.id > 0) {
			const QList<CatalogProperty> properties =
					m_catalog->effectiveProperties(catalog_class.id);
			for (const CatalogProperty &property : properties) {
				QListWidgetItem *item =
						new QListWidgetItem(property.name, m_properties);
				item->setData(Qt::UserRole, property.key);
				item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
				bool shown = false;
				for (const SymbolText &text : m_symbol.texts) {
					if (text.info_key == property.key) {
						shown = true;
					}
				}
				item->setCheckState(shown ? Qt::Checked : Qt::Unchecked);
			}
		}
	}
}

void CreateSymbolDialog::readTerminals()
{
	for (int row = 0 ; row < m_terminals->rowCount() &&
	     row < m_symbol.terminals.size() ; ++row) {
		SymbolTerminal &terminal = m_symbol.terminals[row];

		if (QComboBox *orientation = qobject_cast<QComboBox *>(
					m_terminals->cellWidget(row, ColumnOrientation))) {
			terminal.orientation = Qet::Orientation(
						orientation->currentData().toInt());
		}
		if (QTableWidgetItem *position = m_terminals->item(row, ColumnPosition))
		{
				//"12 ; 30", and anything else is ignored rather than reset to
				//zero: a half typed coordinate must not throw the point to the
				//origin while it is being typed.
			const QStringList parts =
					position->text().split(QLatin1Char(';'));
			if (parts.size() == 2)
			{
				bool x_ok = false, y_ok = false;
				const qreal x = parts.at(0).trimmed().toDouble(&x_ok);
				const qreal y = parts.at(1).trimmed().toDouble(&y_ok);
				if (x_ok && y_ok) {
					terminal.position = m_grid.snapToMain(QPointF(x, y));
				}
			}
		}
		if (QTableWidgetItem *label = m_terminals->item(row, ColumnLabel)) {
			terminal.label = label->text().trimmed();
		}
		if (QComboBox *role = qobject_cast<QComboBox *>(
					m_terminals->cellWidget(row, ColumnRole))) {
			terminal.role = CatalogPinRole(role->currentData().toInt());
		}
		if (QTableWidgetItem *pair = m_terminals->item(row, ColumnPair)) {
			terminal.pair = pair->text().trimmed();
		}
	}

	m_symbol.name = m_name->text().trimmed();
	m_symbol.class_key = m_class->currentData().toString();
	m_symbol.link_type = SymbolLinkType(m_link_type->currentData().toInt());

		//Rebuild the text fields: the tag stays first, the ticked properties
		//follow it down the right hand side of the drawing.
	const QRectF box = m_symbol.bounds();
	QList<SymbolText> texts;
	texts << SymbolText::tagField(QPointF(box.left(), box.top() - 12.0));
	for (const SymbolText &text : m_symbol.texts) {
		if (text.info_key.isEmpty() && !text.text.isEmpty()) {
				//Free text drawn on the sheet: kept as it was.
			texts << text;
		}
	}
	qreal y = box.top();
	for (int row = 0 ; row < m_properties->count() ; ++row) {
		QListWidgetItem *item = m_properties->item(row);
		if (item->checkState() != Qt::Checked) {
			continue;
		}
		SymbolText text(QPointF(box.right() + 4.0, y),
				item->data(Qt::UserRole).toString());
		texts << text;
		y += 10.0;
	}
	m_symbol.texts = texts;
}

void CreateSymbolDialog::refreshProblems()
{
	const QStringList messages = m_symbol.problemMessages(m_grid);
	if (messages.isEmpty()) {
		m_problems->clear();
		m_problems->setVisible(false);
	} else {
		m_problems->setText(QStringLiteral("• ") +
				    messages.join(QStringLiteral("\n• ")));
		m_problems->setVisible(true);
	}
	m_save->setEnabled(messages.isEmpty());

	QStringList summary;
	summary << tr("%n forme(s) dessinée(s)", "", m_symbol.shapes.size());
	summary << tr("%n point(s) de raccordement", "",
		      m_symbol.terminals.size());
	const int no = m_symbol.contactCount(CatalogPinRole::ContactNo);
	const int nc = m_symbol.contactCount(CatalogPinRole::ContactNc);
	const int power = m_symbol.contactCount(CatalogPinRole::PowerContactNo);
	if (no || nc || power) {
		summary << tr("%1 NO, %2 NF, %3 de puissance")
			   .arg(no).arg(nc).arg(power);
	}
	if (!m_symbol.default_part_code.isEmpty()) {
		summary << tr("pièce %1 incluse").arg(m_symbol.default_part_code);
		m_default_part->setText(m_symbol.default_part_code);
	} else {
		m_default_part->setText(tr("aucune — symbole générique"));
	}
	m_clear_part->setEnabled(!m_symbol.default_part_code.isEmpty());
	m_summary->setText(summary.join(QStringLiteral(" · ")));
}

/**
	@brief CreateSymbolDialog::terminalRowChanged
	A linha escolhida na tabela passa a ser o ponto destacado no desenho.
*/
void CreateSymbolDialog::terminalRowChanged()
{
	if (m_preview) {
		m_preview->setHighlighted(m_terminals->currentRow());
	}
}

/**
	@brief CreateSymbolDialog::terminalPickedInPreview
	@param index
	O caminho de volta: clicar o ponto no desenho seleciona a linha dele.
	É a direção que o projetista precisa mais — ele está olhando o desenho e
	quer dizer o que aquele ponto é.
*/
void CreateSymbolDialog::terminalPickedInPreview(int index)
{
	if (index < 0 || index >= m_terminals->rowCount()) {
		return;
	}
	m_terminals->setCurrentCell(index, ColumnLabel);
	m_terminals->setFocus();
}

void CreateSymbolDialog::terminalsChanged()
{
	readTerminals();
		//Rebuilt rather than left as typed: the position was snapped on the
		//way in, and a table that goes on showing 23 while the point sits at
		//20 is a table that lies about what will be saved.
	fillTerminals();
	refreshProblems();
}

/**
	@brief CreateSymbolDialog::chooseDefaultPart
	Pick the catalog part this symbol is for.

	Two things happen when a part is chosen, and both are the point: the values
	of the part are written into the symbol, so a component inserted from it
	arrives with manufacturer and code already filled; and the provisional
	labels of the connection points are replaced by the real pin numbers of the
	product, because for a symbol that is for one product there is nothing
	provisional about them.
*/
void CreateSymbolDialog::chooseDefaultPart()
{
	if (!m_catalog) {
		QMessageBox::information(this, tr("Pièce par défaut"),
			tr("Le catalogue n'est pas disponible."));
		return;
	}

	const CatalogPart part = CatalogBrowserDialog::choosePart(m_catalog, this);
	if (part.isNull()) {
		return;
	}

	readTerminals();
	m_symbol.default_part_code = part.code;
	m_symbol.default_part_values =
			CatalogAssignment::valuesForElement(*m_catalog, part);

		//The real pin numbers, in the reading order of the connection points -
		//the same order and the same rule the assignment uses on the sheet, so
		//that a symbol made this way and a part assigned later agree.
	const QStringList names = CatalogAssignment::terminalNames(
				part, QString(), m_symbol.terminals.size());
	for (int i = 0 ; i < m_symbol.terminals.size() && i < names.size() ; ++i) {
		if (!names.at(i).isEmpty()) {
			m_symbol.terminals[i].label = names.at(i);
		}
	}

	if (m_symbol.class_key.isEmpty() && part.class_id > 0) {
		const CatalogClass part_class = m_catalog->classById(part.class_id);
		if (!part_class.key.isEmpty()) {
			const int index = m_class->findData(part_class.key);
			if (index >= 0) {
				m_class->setCurrentIndex(index);
			}
		}
	}

	fillTerminals();
	refreshProblems();
}

/**
	@brief CreateSymbolDialog::clearDefaultPart
	Back to a generic symbol. The provisional labels are not put back: the
	person who chose the part may well have wanted those numbers anyway, and
	guessing which of them to undo would be guessing.
*/
void CreateSymbolDialog::clearDefaultPart()
{
	readTerminals();
	m_symbol.default_part_code.clear();
	m_symbol.default_part_values.clear();
	refreshProblems();
}

void CreateSymbolDialog::addTerminal()
{
		//A new point lands on the grid at the top left of the drawing, where
		//it is visible and wrong rather than invisible and wrong.
	const QRectF box = m_symbol.bounds();
	const QPointF position = m_grid.snapToMain(box.topLeft());
	m_symbol.terminals << SymbolTerminal(position, Qet::North);
	fillTerminals();
	refreshProblems();
}

void CreateSymbolDialog::removeTerminal()
{
	const QList<QTableWidgetSelectionRange> ranges =
			m_terminals->selectedRanges();
	QList<int> rows;
	for (const QTableWidgetSelectionRange &range : ranges) {
		for (int row = range.topRow() ; row <= range.bottomRow() ; ++row) {
			if (!rows.contains(row)) {
				rows << row;
			}
		}
	}
	std::sort(rows.begin(), rows.end(), std::greater<int>());
	for (int row : rows) {
		if (row >= 0 && row < m_symbol.terminals.size()) {
			m_symbol.terminals.removeAt(row);
		}
	}
	fillTerminals();
	refreshProblems();
}

void CreateSymbolDialog::pairSelected()
{
	readTerminals();
	QList<int> rows;
	const QList<QTableWidgetSelectionRange> ranges =
			m_terminals->selectedRanges();
	for (const QTableWidgetSelectionRange &range : ranges) {
		for (int row = range.topRow() ; row <= range.bottomRow() ; ++row) {
			if (!rows.contains(row)) {
				rows << row;
			}
		}
	}
	if (rows.size() != 2) {
		QMessageBox::information(this, tr("Former une paire"),
			tr("Un contact a deux points. Sélectionnez exactement les "
			   "deux lignes qui le forment."));
		return;
	}

		//The pair name is generated rather than asked for: nobody needs to
		//name a pair, they need to say what it is. The name only has to be
		//unique inside this symbol.
	int index = 1;
	QString pair_name;
	forever {
		pair_name = QStringLiteral("p%1").arg(index);
		if (!m_symbol.pairNames().contains(pair_name)) {
			break;
		}
		index++;
	}

	for (int row : rows) {
		if (row >= 0 && row < m_symbol.terminals.size()) {
			m_symbol.terminals[row].pair = pair_name;
			if (m_symbol.terminals[row].role == CatalogPinRole::Unknown) {
				m_symbol.terminals[row].role = CatalogPinRole::ContactNo;
			}
		}
	}
		//Both halves get the role of the first one, so the pair is never born
		//contradicting itself.
	const CatalogPinRole role = m_symbol.terminals.at(rows.first()).role;
	for (int row : rows) {
		m_symbol.terminals[row].role = role;
	}

	fillTerminals();
	refreshProblems();
}

void CreateSymbolDialog::unpairSelected()
{
	readTerminals();
	const QList<QTableWidgetSelectionRange> ranges =
			m_terminals->selectedRanges();
	for (const QTableWidgetSelectionRange &range : ranges) {
		for (int row = range.topRow() ; row <= range.bottomRow() ; ++row) {
			if (row < 0 || row >= m_symbol.terminals.size()) {
				continue;
			}
			const QString pair = m_symbol.terminals.at(row).pair;
			if (pair.isEmpty()) {
				continue;
			}
				//Undoing one half undoes the other: half a pair is a state
				//the symbol is not allowed to be saved in anyway.
			for (SymbolTerminal &terminal : m_symbol.terminals) {
				if (terminal.pair == pair) {
					terminal.pair.clear();
				}
			}
		}
	}
	fillTerminals();
	refreshProblems();
}

void CreateSymbolDialog::chooseFolder()
{
	const QString folder = QFileDialog::getExistingDirectory(
				this, tr("Dossier de la collection"), m_folder->text());
	if (!folder.isEmpty()) {
		m_folder->setText(folder);
	}
}

QString CreateSymbolDialog::targetFolder() const
{
	return m_folder->text().trimmed();
}

void CreateSymbolDialog::save()
{
	readTerminals();
	refreshProblems();
	if (!m_symbol.canBeSaved(m_grid)) {
		return;
	}

	const QString folder = targetFolder();
	QDir dir(folder);
	if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
		QMessageBox::warning(this, tr("Enregistrer le symbole"),
			tr("Le dossier « %1 » n'existe pas et n'a pas pu être créé.")
					.arg(folder));
		return;
	}

	QString base = SymbolDefinition::fileNameFor(m_symbol.name);
	if (base.isEmpty()) {
		base = QStringLiteral("symbole");
	}
	QString path = dir.absoluteFilePath(base + QStringLiteral(".elmt"));

	if (QFile::exists(path)) {
			//The question of T12, asked in the same words: a symbol already
			//used is either changed everywhere or left alone and superseded.
			//Guessing on the projectist's behalf is how a delivered project
			//quietly changes drawing.
		QMessageBox question(this);
		question.setWindowTitle(tr("Le symbole existe déjà"));
		question.setIcon(QMessageBox::Question);
		question.setText(tr("« %1 » est déjà dans ce dossier.")
				 .arg(QFileInfo(path).fileName()));
		question.setInformativeText(
			tr("Remplacer le symbole change le dessin dans tous les projets "
			   "qui l'utilisent, y compris ceux déjà livrés.\n\n"
			   "Enregistrer une révision laisse ces projets comme ils sont "
			   "et ne sert que pour les suivants."));
		QPushButton *replace = question.addButton(
					tr("Remplacer partout"), QMessageBox::DestructiveRole);
		QPushButton *revision = question.addButton(
					tr("Enregistrer une révision"), QMessageBox::AcceptRole);
		question.addButton(QMessageBox::Cancel);
		question.setDefaultButton(revision);
		question.exec();

		if (question.clickedButton() == revision) {
			int number = 2;
			while (QFile::exists(dir.absoluteFilePath(
						     QStringLiteral("%1_r%2.elmt")
						     .arg(base).arg(number)))) {
				number++;
			}
			path = dir.absoluteFilePath(QStringLiteral("%1_r%2.elmt")
						    .arg(base).arg(number));
			m_symbol.revision = number;
				//A new revision is a new symbol as far as anything comparing
				//symbols is concerned, so it gets its own identity.
			m_symbol.uuid = QUuid::createUuid();
		} else if (question.clickedButton() != replace) {
			return;
		}
	}

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::warning(this, tr("Enregistrer le symbole"),
			tr("Impossible d'écrire « %1 » : %2")
					.arg(path, file.errorString()));
		return;
	}
	QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	stream.setCodec("UTF-8");
#endif
	stream << m_symbol.toXml().toString(4);
	file.close();

	m_saved_path = path;
	accept();
}
