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
#include "catalogreplacedialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "../../diagram.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetproject.h"
#include "../catalog.h"
#include "../catalogassignment.h"
#include "catalogbrowserdialog.h"
#include "catalogprojectactions.h"

namespace
{
	enum UsageColumn
	{
		ColumnCode = 0,
		ColumnCount,
		ColumnFolios,
		ColumnTotal
	};
}

CatalogReplaceDialog::CatalogReplaceDialog(QETProject *project,
					   Catalog *catalog,
					   QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_catalog(catalog)
{
	setWindowTitle(tr("Remplacer une pièce dans tout le projet"));
	setMinimumSize(720, 520);
	setUpWidget();
	reload();
	selectionChanged();
}

int CatalogReplaceDialog::replacedCount() const
{
	return m_replaced;
}

void CatalogReplaceDialog::setUpWidget()
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	QLabel *explanation = new QLabel(
		tr("Les pièces utilisées dans ce projet, et combien de composants "
		   "utilisent chacune. Choisissez celle à remplacer, puis la pièce "
		   "qui prend sa place : les informations et les numéros de bornes "
		   "des composants concernés suivent, en une seule fois, et un seul "
		   "Ctrl+Z revient en arrière."), this);
	explanation->setWordWrap(true);
	layout->addWidget(explanation);

	m_table = new QTableWidget(0, ColumnTotal, this);
	m_table->setHorizontalHeaderLabels(
				QStringList{tr("Pièce"),
					    tr("Composants"),
					    tr("Folios")});
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->verticalHeader()->setVisible(false);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	layout->addWidget(m_table, 1);

	QHBoxLayout *replacement_layout = new QHBoxLayout();
	replacement_layout->addWidget(new QLabel(tr("Remplacer par :"), this));
	m_replacement_label = new QLabel(tr("— aucune choisie —"), this);
	m_choose = new QPushButton(tr("Choisir la pièce…"), this);
	replacement_layout->addWidget(m_replacement_label, 1);
	replacement_layout->addWidget(m_choose);
	layout->addLayout(replacement_layout);

	m_preview = new QLabel(this);
	m_preview->setWordWrap(true);
	m_preview->setTextFormat(Qt::PlainText);
	m_preview->setStyleSheet(QStringLiteral(
		"QLabel { background-color : palette(base); "
		"border : 1px solid palette(mid); padding : 8px; }"));
	layout->addWidget(m_preview);

	QDialogButtonBox *box = new QDialogButtonBox(this);
	m_replace = box->addButton(tr("Remplacer"), QDialogButtonBox::AcceptRole);
	box->addButton(QDialogButtonBox::Close);
	layout->addWidget(box);

	connect(m_table, &QTableWidget::itemSelectionChanged,
		this, &CatalogReplaceDialog::selectionChanged);
	connect(m_choose, &QPushButton::clicked,
		this, &CatalogReplaceDialog::chooseReplacement);
	connect(m_replace, &QPushButton::clicked,
		this, &CatalogReplaceDialog::replace);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QList<Element *> CatalogReplaceDialog::componentsUsing(const QString &code) const
{
	return m_usage.value(code);
}

QStringList CatalogReplaceDialog::foliosOf(const QList<Element *> &elements) const
{
	QStringList folios;
	for (Element *element : elements)
	{
		Diagram *diagram = element->diagram();
		if (!diagram) {
			continue;
		}
		const QString name = diagram->title().isEmpty()
				? tr("folio %1").arg(diagram->folioIndex() + 1)
				: diagram->title();
		if (!folios.contains(name)) {
			folios << name;
		}
	}
	return folios;
}

void CatalogReplaceDialog::reload()
{
	m_usage.clear();
	m_table->setRowCount(0);
	if (!m_project) {
		return;
	}

	const QList<Element *> components =
			CatalogProjectActions::components(m_project);
	for (Element *element : components)
	{
		const QString code = element->elementInformations()
				.value(CatalogAssignment::partCodeKey()).toString().trimmed();
		if (code.isEmpty()) {
			continue;
		}
		m_usage[code].append(element);
	}

	QStringList codes = m_usage.keys();
	codes.sort();
	m_table->setRowCount(codes.size());
	for (int row = 0 ; row < codes.size() ; ++row)
	{
		const QString code = codes.at(row);
		const QList<Element *> elements = m_usage.value(code);

		QTableWidgetItem *code_item = new QTableWidgetItem(code);
		code_item->setData(Qt::UserRole, code);
		m_table->setItem(row, ColumnCode, code_item);

		QTableWidgetItem *count_item = new QTableWidgetItem();
		count_item->setData(Qt::DisplayRole, elements.size());
		m_table->setItem(row, ColumnCount, count_item);

		m_table->setItem(row, ColumnFolios,
				 new QTableWidgetItem(
					 foliosOf(elements).join(QStringLiteral(", "))));
	}
	m_table->resizeColumnsToContents();
}

void CatalogReplaceDialog::selectionChanged()
{
	const QList<QTableWidgetItem *> selected = m_table->selectedItems();
	const bool has_selection = !selected.isEmpty();

	if (!has_selection) {
		m_preview->setText(m_usage.isEmpty()
			? tr("Aucun composant de ce projet n'a de pièce attribuée : il "
			     "n'y a rien à remplacer. Attribuez d'abord des pièces.")
			: tr("Choisissez la pièce à remplacer dans la liste."));
		m_replace->setEnabled(false);
		return;
	}

	const QString code = m_table->item(m_table->currentRow(),
					   ColumnCode)->data(Qt::UserRole).toString();
	const QList<Element *> elements = componentsUsing(code);

	if (m_replacement.isNull()) {
		m_preview->setText(
			tr("« %1 » est sur %n composant(s). Choisissez maintenant la "
			   "pièce qui prend sa place.", "", elements.size()).arg(code));
		m_replace->setEnabled(false);
		return;
	}

	if (m_replacement.code == code) {
		m_preview->setText(
			tr("La pièce choisie est celle qui est déjà là. Rien à faire."));
		m_replace->setEnabled(false);
		return;
	}

	m_preview->setText(
		tr("%n composant(s) passent de « %1 » à « %2 ».\n"
		   "Folios touchés : %3\n\n"
		   "Les repères ne changent pas ; les numéros de bornes prennent "
		   "ceux de la nouvelle pièce.", "", elements.size())
			.arg(code,
			     m_replacement.code,
			     foliosOf(elements).join(QStringLiteral(", "))));
	m_replace->setEnabled(true);
}

void CatalogReplaceDialog::chooseReplacement()
{
	if (!m_catalog) {
		return;
	}
	const CatalogPart part = CatalogBrowserDialog::choosePart(m_catalog, this);
	if (part.isNull()) {
		return;
	}
	m_replacement = part;
	m_replacement_label->setText(part.code);
	selectionChanged();
}

void CatalogReplaceDialog::replace()
{
	if (!m_project || !m_catalog || m_replacement.isNull()) {
		return;
	}
	const QList<QTableWidgetItem *> selected = m_table->selectedItems();
	if (selected.isEmpty()) {
		return;
	}

	const QString code = m_table->item(m_table->currentRow(),
					   ColumnCode)->data(Qt::UserRole).toString();
	const QList<Element *> elements = componentsUsing(code);
	if (elements.isEmpty()) {
		return;
	}

	m_replaced = CatalogProjectActions::assignPart(elements, *m_catalog,
						      m_replacement);
	if (m_replaced == 0) {
		QMessageBox::warning(this, tr("Remplacer une pièce"),
			tr("Le remplacement n'a touché aucun composant."));
		return;
	}

		//Reloaded rather than closed: swapping two parts in a row is the
		//normal case when a supplier changes, and reopening the dialog to do
		//the second one is a click for nothing.
	m_replacement = CatalogPart();
	m_replacement_label->setText(tr("— aucune choisie —"));
	reload();
	selectionChanged();
}
