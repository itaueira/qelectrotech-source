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
#include "symbolgroupdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "../../environment/qetenvironment.h"

SymbolGroupDialog::SymbolGroupDialog(Mode mode,
				     const QDomDocument &fragment,
				     QWidget *parent) :
	QDialog(parent),
	m_mode(mode),
	m_fragment(fragment)
{
	setWindowTitle(mode == Save
		       ? tr("Enregistrer la sélection en groupement")
		       : tr("Insérer un groupement"));
	setMinimumSize(600, 480);
	setUpWidget();
	reload();
	selectionChanged();
	if (m_mode == Save) {
			//Nothing to save until it has a name, and the button says so from
			//the start rather than after the first keystroke.
		nameChanged();
	}
}

SymbolGroup SymbolGroupDialog::chosenGroup() const
{
	return m_chosen;
}

QString SymbolGroupDialog::savedPath() const
{
	return m_saved_path;
}

QString SymbolGroupDialog::folder()
{
	const QString path = QETEnvironment::groupingsDir();
	QDir().mkpath(path);
	return path;
}

void SymbolGroupDialog::setUpWidget()
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	if (m_mode == Save) {
		QLabel *explanation = new QLabel(
			tr("Un groupement est un morceau de schéma prêt : les "
			   "composants, les conducteurs, et les pièces du catalogue "
			   "déjà attribuées. Inséré ailleurs, il revient avec tout "
			   "cela. Le modifier sur la folio ne change pas le "
			   "groupement enregistré."), this);
		explanation->setWordWrap(true);
		layout->addWidget(explanation);

		QFormLayout *form = new QFormLayout();
		m_name = new QLineEdit(this);
		m_name->setPlaceholderText(tr("Commande marche-arrêt"));
		form->addRow(tr("Nom :"), m_name);
		m_description = new QTextEdit(this);
		m_description->setMaximumHeight(60);
		form->addRow(tr("Description :"), m_description);
		layout->addLayout(form);
	}

	QLabel *list_label = new QLabel(
				m_mode == Save
				? tr("Groupements déjà enregistrés :")
				: tr("Groupements enregistrés :"), this);
	layout->addWidget(list_label);

	m_list = new QListWidget(this);
	layout->addWidget(m_list, 1);

	m_content = new QLabel(this);
	m_content->setWordWrap(true);
	layout->addWidget(m_content);

	QDialogButtonBox *box = new QDialogButtonBox(this);
	m_remove = box->addButton(tr("Supprimer"), QDialogButtonBox::ResetRole);
	m_act = box->addButton(m_mode == Save ? tr("Enregistrer")
					      : tr("Insérer"),
			       QDialogButtonBox::AcceptRole);
	box->addButton(QDialogButtonBox::Cancel);
	layout->addWidget(box);

	connect(m_list, &QListWidget::currentRowChanged,
		this, &SymbolGroupDialog::selectionChanged);
	connect(m_list, &QListWidget::itemDoubleClicked,
		this, &SymbolGroupDialog::act);
	connect(m_remove, &QPushButton::clicked,
		this, &SymbolGroupDialog::removeSelected);
	connect(m_act, &QPushButton::clicked, this, &SymbolGroupDialog::act);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	if (m_name) {
		connect(m_name, &QLineEdit::textChanged,
			this, &SymbolGroupDialog::nameChanged);
	}
}

void SymbolGroupDialog::reload()
{
	m_groups.clear();
	m_list->clear();
	const QStringList paths = SymbolGroup::listFolder(folder());
	for (const QString &path : paths) {
		const SymbolGroup group = SymbolGroup::load(path);
		if (group.isNull()) {
			continue;
		}
		m_groups << group;
		QListWidgetItem *item = new QListWidgetItem(group.name, m_list);
		item->setData(Qt::UserRole, path);
	}
	if (m_list->count() && m_mode == Insert) {
		m_list->setCurrentRow(0);
	}
}

void SymbolGroupDialog::selectionChanged()
{
	const int row = m_list->currentRow();
	const bool has_selection = row >= 0 && row < m_groups.size();
	m_remove->setEnabled(has_selection);

	if (m_mode == Insert) {
		m_act->setEnabled(has_selection);
	}

		//In Save mode with nothing picked in the list, what is described is
		//the selection about to be filed - that is the thing the designer
		//is looking at. Otherwise it is the grouping picked in the list.
	const SymbolGroup shown = (m_mode == Save && !has_selection)
			? SymbolGroup::fromFragment(QString(), m_fragment)
			: (has_selection ? m_groups.at(row) : SymbolGroup());

	if (shown.isNull()) {
		m_content->clear();
		return;
	}

	QStringList lines;
	lines << tr("%n composant(s)", "", shown.elementCount());
	lines << tr("%n conducteur(s)", "", shown.conductorCount());
	const QStringList codes = shown.partCodes();
	QString text = lines.join(QStringLiteral(" · "));
	if (codes.isEmpty()) {
		text += QStringLiteral("\n") +
				tr("Aucune pièce du catalogue n'est attribuée : le "
				   "groupement apportera le dessin, pas le matériel.");
	} else {
		text += QStringLiteral("\n") +
				tr("Pièces apportées : %1").arg(codes.join(QStringLiteral(", ")));
	}
	if (!shown.description.isEmpty()) {
		text += QStringLiteral("\n") + shown.description;
	}
	m_content->setText(text);
}

void SymbolGroupDialog::nameChanged()
{
	if (m_mode != Save) {
		return;
	}
	m_act->setEnabled(!m_name->text().trimmed().isEmpty());
}

void SymbolGroupDialog::removeSelected()
{
	const int row = m_list->currentRow();
	if (row < 0 || row >= m_groups.size()) {
		return;
	}
	const QString path = m_list->item(row)->data(Qt::UserRole).toString();
	if (QMessageBox::question(this, tr("Supprimer le groupement"),
			tr("Supprimer « %1 » de la bibliothèque ? Les schémas déjà "
			   "dessinés avec lui ne changent pas.")
					.arg(m_groups.at(row).name))
			!= QMessageBox::Yes) {
		return;
	}
	if (!QFile::remove(path)) {
		QMessageBox::warning(this, tr("Supprimer le groupement"),
			tr("Impossible de supprimer « %1 ».").arg(path));
		return;
	}
	reload();
	selectionChanged();
}

void SymbolGroupDialog::act()
{
	if (m_mode == Insert) {
		const int row = m_list->currentRow();
		if (row < 0 || row >= m_groups.size()) {
			return;
		}
		m_chosen = m_groups.at(row);
		accept();
		return;
	}

	const QString name = m_name->text().trimmed();
	if (name.isEmpty()) {
		return;
	}

	SymbolGroup group = SymbolGroup::fromFragment(name, m_fragment);
	group.description = m_description->toPlainText().trimmed();
	if (group.isNull()) {
		QMessageBox::warning(this, tr("Enregistrer le groupement"),
			tr("La sélection ne contient rien à enregistrer."));
		return;
	}

	QString base = SymbolGroup::fileNameFor(name);
	if (base.isEmpty()) {
		base = QStringLiteral("groupement");
	}
	const QString path = QDir(folder()).absoluteFilePath(
				base + QStringLiteral(".") + SymbolGroup::extension());

	if (QFile::exists(path)) {
			//Saving over the same name is how a grouping is changed, so it is
			//offered rather than refused - but it is said plainly, because
			//what is inserted already stays as it is and only the next
			//insertion changes.
		if (QMessageBox::question(this, tr("Enregistrer le groupement"),
				tr("« %1 » existe déjà. L'écraser ?\n\n"
				   "Les schémas où il a déjà été inséré ne changent pas ; "
				   "seules les insertions suivantes prendront la nouvelle "
				   "version.").arg(name))
				!= QMessageBox::Yes) {
			return;
		}
	}

	QString error;
	if (!group.save(path, &error)) {
		QMessageBox::warning(this, tr("Enregistrer le groupement"), error);
		return;
	}
	m_saved_path = path;
	accept();
}

QString SymbolGroupDialog::saveSelection(const QDomDocument &fragment,
					 QWidget *parent)
{
	SymbolGroupDialog dialog(Save, fragment, parent);
	if (dialog.exec() == QDialog::Accepted) {
		return dialog.savedPath();
	}
	return QString();
}

SymbolGroup SymbolGroupDialog::chooseGroup(QWidget *parent)
{
	SymbolGroupDialog dialog(Insert, QDomDocument(), parent);
	if (dialog.exec() == QDialog::Accepted) {
		return dialog.chosenGroup();
	}
	return SymbolGroup();
}
