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
#include "renumberdialog.h"

#include "../../catalog/catalog.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetproject.h"
#include "../projectrenumberer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
	const char *ORIENTATION_KEY = "renumbering/columns-first";
}

/**
	@brief RenumberDialog::RenumberDialog
	@param project
	@param catalog
	@param selection : the components selected on the folio, may be empty
	@param parent
*/
RenumberDialog::RenumberDialog(QETProject *project,
			       Catalog *catalog,
			       const QList<Element *> &selection,
			       QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_catalog(catalog),
	m_selection(selection)
{
	setWindowTitle(tr("Renuméroter les composants"));
	resize(720, 620);
	buildWidgets();
	recompute();
}

/**
	@brief RenumberDialog::buildWidgets
*/
void RenumberDialog::buildWidgets()
{
	m_whole_project = new QRadioButton(tr("Tout le projet"), this);
	m_only_selection = new QRadioButton(
		tr("Seulement la sélection (%n composant(s))", "", m_selection.size()), this);

	// The selection is the default when there is one: selecting a control
	// circuit that was just inserted and renumbering only that is the most
	// frequent use of the day.
	if (m_selection.isEmpty())
	{
		m_whole_project->setChecked(true);
		m_only_selection->setEnabled(false);
	}
	else
	{
		m_only_selection->setChecked(true);
	}

	m_format = new QComboBox(this);
	const QList<NumberingFormat> formats = NumberingFormat::builtinFormats();
	for (const NumberingFormat &format : formats)
	{
		m_format->addItem(format.name + QStringLiteral("  —  ") + format.pattern,
				  format.toXml());
	}

	m_orientation = new QComboBox(this);
	m_orientation->addItem(tr("De haut en bas, puis de gauche à droite"), false);
	m_orientation->addItem(tr("De gauche à droite, puis de haut en bas"), true);
	{
		QSettings settings;
		const bool columns_first =
			settings.value(QLatin1String(ORIENTATION_KEY), false).toBool();
		m_orientation->setCurrentIndex(columns_first ? 1 : 0);
	}

		//The request was to be able to index from 0: DJ0, DJ1, DJ2. The format
		//already stored the starting number; what was missing was a place to say it.
	m_start = new QSpinBox(this);
	m_start->setRange(0, 9999);
	m_start->setValue(1);
	m_start->setToolTip(tr(
			   "Le premier numéro de chaque groupe. Mettre 0 donne DJ0, "
			   "DJ1, DJ2. S'applique au format par défaut, pas à celui "
			   "qu'une classe déclare."));

		//The case at hand: terminal symbols authored as "simple", which the
		//program has no way to tell apart from a component. The field it decides
		//on is the type declared in the .elmt, and there it reads "simple".
		//Until the library declares them, skipping by prefix is the way out.
	m_skip_prefixes = new QLineEdit(this);
	m_skip_prefixes->setPlaceholderText(tr("X, XB, TB"));
	m_skip_prefixes->setToolTip(tr(
			   "Repères commençant par ces lettres : laissés tels quels. "
			   "Sert aux bornes dont le symbole n'est pas déclaré comme "
			   "borne dans la bibliothèque, et que le programme ne peut "
			   "donc pas distinguer d'un composant. Séparés par des "
			   "virgules."));

	m_only_changes = new QCheckBox(tr("N'afficher que ce qui change"), this);
	m_only_changes->setChecked(true);

	QLabel *format_hint = new QLabel(
		tr("Le format choisi ici ne s'applique qu'aux classes qui n'en déclarent pas. "
		   "Le format d'une classe se règle dans « Classes et propriétés du catalogue » : "
		   "c'est là qu'il vit, pour que deux personnes renumérotant le même projet "
		   "obtiennent la même chose."), this);
	format_hint->setWordWrap(true);

	QFormLayout *form = new QFormLayout();
	form->addRow(tr("Portée"), m_whole_project);
	form->addRow(QString(), m_only_selection);
	form->addRow(tr("Format par défaut"), m_format);
	form->addRow(tr("Ordre de lecture"), m_orientation);
	form->addRow(tr("Commencer à"), m_start);
	form->addRow(tr("Ne pas toucher aux repères commençant par"), m_skip_prefixes);
	form->addRow(QString(), m_only_changes);

	m_preview = new QTableWidget(this);
	m_preview->setColumnCount(3);
	m_preview->setHorizontalHeaderLabels({ tr("De"), tr("Vers"), tr("Note") });
	m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_preview->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_preview->verticalHeader()->setVisible(false);
	m_preview->horizontalHeader()->setStretchLastSection(true);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	m_warning = new QLabel(this);
	m_warning->setWordWrap(true);

	QPushButton *apply_button = new QPushButton(tr("Appliquer"), this);
	apply_button->setDefault(true);

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->addButton(apply_button, QDialogButtonBox::AcceptRole);
	buttons->addButton(QDialogButtonBox::Cancel);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(format_hint);
	layout->addWidget(m_preview, 1);
	layout->addWidget(m_summary);
	layout->addWidget(m_warning);
	layout->addWidget(buttons);

	connect(m_whole_project, &QRadioButton::toggled, this, &RenumberDialog::recompute);
	connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &RenumberDialog::recompute);
	connect(m_orientation, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &RenumberDialog::recompute);
	connect(m_start, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &RenumberDialog::recompute);
	connect(m_skip_prefixes, &QLineEdit::textChanged,
		this, &RenumberDialog::recompute);
	connect(m_only_changes, &QCheckBox::toggled, this, &RenumberDialog::recompute);
	connect(apply_button, &QPushButton::clicked, this, &RenumberDialog::apply);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief RenumberDialog::currentScope
	@return the components the chosen scope covers
*/
QList<Element *> RenumberDialog::currentScope() const
{
	return m_whole_project->isChecked()
	       ? ProjectRenumberer::components(m_project)
	       : ProjectRenumberer::components(m_project, m_selection);
}

/**
	@brief RenumberDialog::recompute
*/
void RenumberDialog::recompute()
{
	m_scope = currentScope();

	NumberingFormat format =
		NumberingFormat::fromXml(m_format->currentData().toString());
	format.start = m_start->value();
	const bool columns_first = m_orientation->currentData().toBool();

		//The prefixes to spare, read once. Empty spares nothing, which is the
		//previous behaviour.
	QStringList skip;
	const QStringList typed = m_skip_prefixes->text().split(QLatin1Char(','));
	for (const QString &prefix : typed) {
		const QString clean = prefix.trimmed();
		if (!clean.isEmpty()) {
			skip << clean;
		}
	}

	// The format chosen here is the fallback: a class that declares its own
	// wins, which is the registered decision of T07. So a project whose
	// classes are set up renumbers the same way whoever runs the command.
	QList<RenumberInput> inputs =
		m_catalog ? ProjectRenumberer::inputsFor(*m_catalog, m_scope, format)
			  : QList<RenumberInput>();

		//Sparing means marking as frozen, not dropping from the list: the object
		//keeps showing in the preview, flagged as skipped. Vanishing from the
		//table would leave the doubt of whether it was renumbered unseen.
	if (!skip.isEmpty())
	{
		for (RenumberInput &input : inputs)
		{
			if (input.frozen) {
				continue;
			}
			for (const QString &prefix : skip)
			{
				if (input.current.startsWith(prefix, Qt::CaseInsensitive)) {
					input.frozen = true;
					break;
				}
			}
		}
	}

	m_plan = Renumberer::plan(inputs, columns_first);

	const bool only_changes = m_only_changes->isChecked();
	m_preview->setRowCount(0);
	for (const RenumberEntry &entry : m_plan.entries)
	{
		if (only_changes && !entry.changed && !entry.frozen) {
			continue;
		}

		const int row = m_preview->rowCount();
		m_preview->insertRow(row);
		m_preview->setItem(row, 0, new QTableWidgetItem(entry.from));
		m_preview->setItem(row, 1, new QTableWidgetItem(entry.to));

		QString note;
		if (entry.frozen) {
			note = tr("numéroté à la main : laissé tel quel");
		} else if (!entry.changed) {
			note = tr("inchangé");
		}
		QTableWidgetItem *note_item = new QTableWidgetItem(note);
		if (entry.frozen)
		{
			QFont font = note_item->font();
			font.setItalic(true);
			note_item->setFont(font);
		}
		m_preview->setItem(row, 2, note_item);
	}
	m_preview->resizeColumnsToContents();

	m_summary->setText(tr("%1 composant(s) dans la portée, %2 changement(s), "
			      "%3 numéroté(s) à la main et laissé(s) tel(s) quel(s).")
			   .arg(m_plan.entries.size())
			   .arg(m_plan.changeCount())
			   .arg(m_plan.frozenCount()));

	// A duplicate is the one outcome renumbering must never produce, so it is
	// said here, before the button is pressed, and not discovered in the
	// workshop.
	if (m_plan.hasDuplicates())
	{
		m_warning->setText(tr("<b>Attention :</b> ce format produirait des repères en double : "
				      "%1. Changez de format ou de portée avant d'appliquer.")
				   .arg(m_plan.duplicates().join(QStringLiteral(", "))));
	}
	else
	{
		m_warning->clear();
	}
}

/**
	@brief RenumberDialog::apply
*/
void RenumberDialog::apply()
{
	if (m_plan.hasDuplicates())
	{
		if (QMessageBox::question(this, tr("Repères en double"),
					  tr("Ce format produirait des repères en double : %1.\n\n"
					     "Appliquer quand même ?")
						  .arg(m_plan.duplicates().join(QStringLiteral(", "))))
		    != QMessageBox::Yes)
		{
			return;
		}
	}

	// Remember the reading order: it is a setting of the station, and two
	// people renumbering the same project have to get the same answer.
	QSettings settings;
	settings.setValue(QLatin1String(ORIENTATION_KEY),
			  m_orientation->currentData().toBool());

	m_applied = ProjectRenumberer::applyPlan(m_scope, m_plan);
	accept();
}

/**
	@brief RenumberDialog::appliedCount
	@return how many components were renumbered
*/
int RenumberDialog::appliedCount() const
{
	return m_applied;
}
