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
#include "locationmanagerdialog.h"

#include "../../catalog/catalog.h"
#include "../../catalog/catalogclass.h"
#include "../../catalog/catalogpart.h"
#include "../../catalog/ui/catalogbrowserdialog.h"
#include "../../diagram.h"
#include "../../diagramcontext.h"
#include "../locatableelement.h"
#include "../../qetapp.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetinformation.h"
#include "../../qetproject.h"
#include "../../undocommand/assignlocationcommand.h"
#include "../../undocommand/editlocationtreecommand.h"

#include <QDialogButtonBox>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
	enum Column
	{
		CodeColumn = 0,
		NameColumn,
		PathColumn,
		PartColumn,
		VirtualColumn,
		CountColumn,
		ColumnCount
	};

	const int UUID_ROLE = Qt::UserRole + 1;

	/// @return true when the person is allowed to type in @a column
	bool isEditableColumn(int column)
	{
		return column == CodeColumn || column == NameColumn;
	}

	/**
		@brief The ColumnDelegate class
		Read only columns that stay read only.

		QTreeWidgetItem carries its flags per row, not per column, so
		making the code editable would make the computed path editable
		too. Refusing to build an editor is what keeps the difference.
	*/
	class ColumnDelegate : public QStyledItemDelegate
	{
		public:
			explicit ColumnDelegate(QObject *parent = nullptr) :
				QStyledItemDelegate(parent)
			{}

			QWidget *createEditor(QWidget *parent,
					      const QStyleOptionViewItem &option,
					      const QModelIndex &index) const override
			{
				if (!isEditableColumn(index.column())) {
					return nullptr;
				}
				return QStyledItemDelegate::createEditor(parent, option,
									 index);
			}
	};
}

/**
	@brief LocationManagerDialog::LocationManagerDialog
	@param project the project whose locations are being edited
	@param parent parent widget
*/
LocationManagerDialog::LocationManagerDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	buildWidgets();

	if (m_project)
	{
			//no signal announces the tree, and adding one to QETProject
			//would be a divergence from upstream for nothing: every
			//change to it goes through the undo stack, so the index of
			//the stack is the same news.
		if (QUndoStack *stack = m_project->undoStack()) {
			connect(stack, &QUndoStack::indexChanged,
				this, &LocationManagerDialog::stackChanged);
		}
		connect(m_project.data(), &QObject::destroyed,
			this, &QWidget::close);
	}

	reload();
}

/**
	@brief LocationManagerDialog::project
	@return the project this window is editing
*/
QETProject *LocationManagerDialog::project() const
{
	return m_project.data();
}

/**
	@brief LocationManagerDialog::buildWidgets
*/
void LocationManagerDialog::buildWidgets()
{
	setWindowTitle(tr("Localisations du projet"));

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(ColumnCount);
	m_tree->setHeaderLabels(QStringList()
				<< tr("Code")
				<< tr("Nom")
				<< tr("Désignation")
				<< tr("Pièce")
				<< tr("Hors nomenclature")
				<< tr("Composants"));
	m_tree->setRootIsDecorated(true);
	m_tree->setAlternatingRowColors(true);
	m_tree->setEditTriggers(QAbstractItemView::DoubleClicked
				| QAbstractItemView::EditKeyPressed);
	m_tree->setItemDelegate(new ColumnDelegate(m_tree));
	m_tree->header()->setStretchLastSection(false);
	m_tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);

	connect(m_tree, &QTreeWidget::itemSelectionChanged,
		this, &LocationManagerDialog::selectionChanged);
	connect(m_tree, &QTreeWidget::itemChanged,
		this, &LocationManagerDialog::itemEdited);

	m_add        = new QPushButton(tr("Ajouter"), this);
	m_add_child  = new QPushButton(tr("Ajouter dedans"), this);
	m_remove     = new QPushButton(tr("Supprimer"), this);
	m_move       = new QPushButton(tr("Déplacer vers…"), this);
	m_link       = new QPushButton(tr("Lier une pièce…"), this);
	m_assign     = new QPushButton(tr("Affecter la sélection"), this);
	m_unassign   = new QPushButton(tr("Retirer la sélection"), this);

	m_add->setToolTip(tr("Créer une localisation au premier niveau."));
	m_add_child->setToolTip(tr(
		"Créer une localisation dans celle qui est sélectionnée."));
	m_remove->setToolTip(tr(
		"Supprimer la localisation sélectionnée et ce qu'elle contient. "
		"Les composants concernés redeviennent non localisés."));
	m_move->setToolTip(tr(
		"Déplacer la localisation sélectionnée, et ce qu'elle contient, "
		"dans une autre. Les composants suivent."));
	m_link->setToolTip(tr(
		"Choisir dans le catalogue la pièce que cette localisation a été "
		"achetée comme."));
	m_assign->setToolTip(tr(
		"Placer ce qui est sélectionné sur le folio dans la localisation "
		"sélectionnée ici."));
	m_unassign->setToolTip(tr(
		"Retirer de leur localisation les composants sélectionnés sur le "
		"folio."));

	connect(m_add, &QPushButton::clicked,
		this, &LocationManagerDialog::addLocation);
	connect(m_add_child, &QPushButton::clicked,
		this, &LocationManagerDialog::addChildLocation);
	connect(m_remove, &QPushButton::clicked,
		this, &LocationManagerDialog::removeLocation);
	connect(m_move, &QPushButton::clicked,
		this, &LocationManagerDialog::moveLocation);
	connect(m_link, &QPushButton::clicked,
		this, &LocationManagerDialog::linkPart);
	connect(m_assign, &QPushButton::clicked,
		this, &LocationManagerDialog::assignSelection);
	connect(m_unassign, &QPushButton::clicked,
		this, &LocationManagerDialog::unassignSelection);

	QHBoxLayout *tools = new QHBoxLayout();
	tools->addWidget(m_add);
	tools->addWidget(m_add_child);
	tools->addWidget(m_remove);
	tools->addWidget(m_move);
	tools->addWidget(m_link);
	tools->addStretch();
	tools->addWidget(m_assign);
	tools->addWidget(m_unassign);

	m_summary = new QLabel(this);
	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(tools);
	layout->addWidget(m_tree, 1);
	layout->addWidget(m_summary);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	resize(880, 520);
}

/**
	@brief LocationManagerDialog::reload
	Rebuild everything from the project.
*/
void LocationManagerDialog::reload()
{
	if (!m_project) {
		return;
	}

	m_loading = true;

	int unassigned = 0;
	m_counts = componentCounts(&unassigned);

	const LocationTree tree = m_project.data()->locationTree();
	fill(tree);

	int located = 0;
	const QStringList paths = tree.paths();
	for (const QString &path : paths) {
		located += m_counts.value(path);
	}

	int on_paths = 0;
	for (QHash<QString, int>::const_iterator it = m_counts.constBegin() ;
	     it != m_counts.constEnd() ; ++it)
	{
		on_paths += it.value();
	}
	const int stray = on_paths - located;

	QString summary = tr("%1 localisation(s) · %2 composant(s) localisé(s) · "
			     "%3 non localisé(s)")
				.arg(tree.count())
				.arg(located)
				.arg(unassigned);
		//a component naming a location the tree no longer has is neither
		//located nor unassigned, and would silently fall out of both
		//totals. It is said out loud instead.
	if (stray > 0)
	{
		summary = summary + QString(" · ")
			+ tr("%n composant(s) sur une localisation absente", "",
			     stray);
	}
	m_summary->setText(summary);

	m_loading = false;
	selectionChanged();
}

/**
	@brief LocationManagerDialog::fill
	@param tree the tree to show
*/
void LocationManagerDialog::fill(const LocationTree &tree)
{
	m_tree->clear();

	const QStringList roots = tree.rootUuids();
	for (const QString &uuid : roots) {
		addRow(nullptr, tree, uuid);
	}

	m_tree->expandAll();
	for (int column = 0 ; column < ColumnCount ; ++column) {
		m_tree->resizeColumnToContents(column);
	}
}

/**
	@brief LocationManagerDialog::addRow
	@param parent the row of the location this one sits inside, null at the
	top level
	@param tree the tree being shown
	@param uuid the location to add, with everything it contains
*/
void LocationManagerDialog::addRow(QTreeWidgetItem *parent,
				   const LocationTree &tree,
				   const QString &uuid)
{
	const ProjectLocation location = tree.location(uuid);
	const QString path = tree.path(uuid);

	QTreeWidgetItem *item = parent ? new QTreeWidgetItem(parent)
				       : new QTreeWidgetItem(m_tree);

	item->setText(CodeColumn, location.code);
	item->setText(NameColumn, location.name);
	item->setText(PathColumn, LocationTree::iecTag(path));
	item->setText(PartColumn, location.part_code);
	item->setText(CountColumn, QString::number(m_counts.value(path)));
	item->setCheckState(VirtualColumn,
			    location.virtual_part ? Qt::Checked : Qt::Unchecked);
	item->setData(CodeColumn, UUID_ROLE, uuid);
	item->setFlags(item->flags() | Qt::ItemIsEditable);

	if (!location.description.isEmpty()) {
		item->setToolTip(NameColumn, location.description);
	}

	const QStringList children = tree.childUuids(uuid);
	for (const QString &child : children) {
		addRow(item, tree, child);
	}
}

/**
	@brief LocationManagerDialog::selectionChanged
*/
void LocationManagerDialog::selectionChanged()
{
	const bool picked = !selectedUuid().isEmpty();

	m_add_child->setEnabled(picked);
	m_remove->setEnabled(picked);
	m_move->setEnabled(picked);
	m_link->setEnabled(picked);
	m_assign->setEnabled(picked);
}

/**
	@brief LocationManagerDialog::itemEdited
	@param item the row that was typed into
	@param column the column that was typed into
*/
void LocationManagerDialog::itemEdited(QTreeWidgetItem *item, int column)
{
	if (m_loading || !item || !m_project) {
		return;
	}

	const QString uuid = item->data(CodeColumn, UUID_ROLE).toString();
	if (uuid.isEmpty()) {
		return;
	}

	LocationTree tree = m_project.data()->locationTree();
	ProjectLocation location = tree.location(uuid);
	if (location.isNull()) {
		return;
	}

	QString label;
	if (column == CodeColumn)
	{
		const QString typed =
			ProjectLocation::sanitizeCode(item->text(CodeColumn));
		if (typed == location.code) {
			return;
		}
		label = tr("Renommer « %1 » en « %2 »").arg(location.code, typed);
		location.code = typed;
	}
	else if (column == NameColumn)
	{
		if (item->text(NameColumn) == location.name) {
			return;
		}
		label = tr("Renommer la localisation « %1 »").arg(location.code);
		location.name = item->text(NameColumn);
	}
	else if (column == VirtualColumn)
	{
		const bool wanted =
			item->checkState(VirtualColumn) == Qt::Checked;
		if (wanted == location.virtual_part) {
			return;
		}
		label = wanted
			? tr("Retirer « %1 » de la nomenclature").arg(location.code)
			: tr("Compter « %1 » dans la nomenclature").arg(location.code);
		location.virtual_part = wanted;
	}
	else
	{
		return;
	}

	QMap<QString, QString> moved;
	QString error;
	if (!tree.update(location, &moved, &error))
	{
		say(error.isEmpty() ? tr("Modification refusée.") : error, true);
			//the row still shows what was typed, and the project does
			//not. The rebuild waits for the event loop: taking the rows
			//away from under a QTreeWidget still finishing its own
			//itemChanged is how a list like this crashes.
		QTimer::singleShot(0, this, &LocationManagerDialog::reload);
		return;
	}

	push(tree, moved, label);
}

/**
	@brief LocationManagerDialog::stackChanged
	Somebody undid or redid something, here or on the folio.
*/
void LocationManagerDialog::stackChanged()
{
	if (m_loading) {
		return;
	}
	reload();
}

/**
	@brief LocationManagerDialog::addLocation
*/
void LocationManagerDialog::addLocation()
{
	createUnder(QString());
}

/**
	@brief LocationManagerDialog::addChildLocation
*/
void LocationManagerDialog::addChildLocation()
{
	const QString uuid = selectedUuid();
	if (uuid.isEmpty()) {
		return;
	}
	createUnder(uuid);
}

/**
	@brief LocationManagerDialog::createUnder
	@param parent_uuid the location to create inside, empty for the top
	level
*/
void LocationManagerDialog::createUnder(const QString &parent_uuid)
{
	if (!m_project) {
		return;
	}

	LocationTree tree = m_project.data()->locationTree();

	bool accepted = false;
	const QString question = parent_uuid.isEmpty()
		? tr("Code de la nouvelle localisation :")
		: tr("Code de la localisation à créer dans « %1 » :")
			.arg(tree.path(parent_uuid));
	const QString typed = QInputDialog::getText(
			this, tr("Nouvelle localisation"), question,
			QLineEdit::Normal, freeCode(tree, parent_uuid),
			&accepted);
	if (!accepted) {
		return;
	}

	ProjectLocation location(ProjectLocation::sanitizeCode(typed));
	location.parent_uuid = parent_uuid;

	QString error;
	if (tree.append(location, &error).isEmpty())
	{
		say(error.isEmpty() ? tr("Localisation refusée.") : error, true);
		return;
	}

		//creating a location moves nobody: nothing was written on a
		//component that has to follow.
	push(tree, QMap<QString, QString>(),
	     tr("Créer la localisation « %1 »")
		.arg(ProjectLocation::sanitizeCode(typed)));
}

/**
	@brief LocationManagerDialog::removeLocation
*/
void LocationManagerDialog::removeLocation()
{
	if (!m_project) {
		return;
	}

	const QString uuid = selectedUuid();
	if (uuid.isEmpty()) {
		return;
	}

	LocationTree tree = m_project.data()->locationTree();
	const ProjectLocation location = tree.location(uuid);

	QStringList removed;
	if (!tree.remove(uuid, &removed))
	{
		say(tr("Cette localisation n'existe plus."), true);
		return;
	}

	int concerned = 0;
	for (const QString &path : removed) {
		concerned += m_counts.value(path);
	}

		//asking is worth it here and nowhere else in this window: every
		//other button is undone by one Ctrl+Z that puts the same thing
		//back, and this one is the only one that can quietly empty the
		//location of eighty components at once.
	if (removed.size() > 1 || concerned > 0)
	{
		QString warning =
			tr("« %1 » et ce qu'elle contient font %n localisation(s).",
			   "", removed.size()).arg(location.code);
		if (concerned > 0)
		{
			warning = warning + QString("\n")
				+ tr("%n composant(s) redeviendront non localisé(s).",
				     "", concerned);
		}

		if (QMessageBox::question(this, tr("Supprimer la localisation"),
					  warning,
					  QMessageBox::Yes | QMessageBox::No,
					  QMessageBox::No) != QMessageBox::Yes)
		{
			return;
		}
	}

	push(tree, LocationTree::lostPaths(removed),
	     tr("Supprimer la localisation « %1 »").arg(location.code));
}

/**
	@brief LocationManagerDialog::moveLocation
*/
void LocationManagerDialog::moveLocation()
{
	if (!m_project) {
		return;
	}

	const QString uuid = selectedUuid();
	if (uuid.isEmpty()) {
		return;
	}

	LocationTree tree = m_project.data()->locationTree();
	const ProjectLocation location = tree.location(uuid);

		//a location cannot be moved into itself nor into what it
		//contains, so those are not offered rather than refused later.
	const QStringList forbidden = tree.descendantUuids(uuid);
	QStringList labels;
	QStringList uuids;
	labels.append(tr("(premier niveau)"));
	uuids.append(QString());

	for (int index = 0 ; index < tree.count() ; ++index)
	{
		const ProjectLocation candidate = tree.at(index);
		if (candidate.uuid == uuid
		    || forbidden.contains(candidate.uuid)) {
			continue;
		}

			//the label is built on the path, which is unique in the
			//tree, so that two enclosures sharing a name cannot make
			//the answer ambiguous.
		QString label = LocationTree::iecTag(tree.path(candidate.uuid));
		if (!candidate.name.isEmpty()) {
			label = label + QString(" — ") + candidate.name;
		}
		labels.append(label);
		uuids.append(candidate.uuid);
	}

	bool accepted = false;
	const QString chosen = QInputDialog::getItem(
			this, tr("Déplacer la localisation"),
			tr("Placer « %1 » dans :").arg(location.code),
			labels, 0, false, &accepted);
	if (!accepted) {
		return;
	}

	const QString parent_uuid = uuids.value(labels.indexOf(chosen));

	QMap<QString, QString> moved;
	QString error;
	if (!tree.move(uuid, parent_uuid, &moved, &error))
	{
		say(error.isEmpty() ? tr("Déplacement refusé.") : error, true);
		return;
	}

	push(tree, moved,
	     tr("Déplacer la localisation « %1 »").arg(location.code));
}

/**
	@brief LocationManagerDialog::linkPart
	Say which catalogue part the enclosure was bought as - which is what
	gives the bill of material a line and the enclosure swap something to
	compare.
*/
void LocationManagerDialog::linkPart()
{
	if (!m_project) {
		return;
	}

	const QString uuid = selectedUuid();
	if (uuid.isEmpty()) {
		return;
	}

	Catalog *catalog = QETApp::catalog();
	if (!catalog)
	{
		say(tr("Le catalogue n'est pas disponible."), true);
		return;
	}

	LocationTree tree = m_project.data()->locationTree();
	ProjectLocation location = tree.location(uuid);
	if (location.isNull()) {
		return;
	}

	CatalogPart wanted;
	wanted.class_id = catalog->classByKey(QString("location")).id;

		//the class is both what a new enclosure part is born as and the
		//only class worth offering: a location is not an inverter.
	const CatalogPart part =
		CatalogBrowserDialog::choosePart(catalog, this, wanted,
						 wanted.class_id);
	if (part.code.isEmpty()) {
		return;
	}

	location.part_code = part.code;
	location.part_revision = part.revision;

	QString error;
	if (!tree.update(location, nullptr, &error))
	{
		say(error.isEmpty() ? tr("Modification refusée.") : error, true);
		return;
	}

	push(tree, QMap<QString, QString>(),
	     tr("Lier « %1 » à la pièce %2").arg(location.code, part.code));
}

/**
	@brief LocationManagerDialog::assignSelection
	CU-32.1: what is selected on the folio goes into the selected
	enclosure, in one gesture and one undo.
*/
void LocationManagerDialog::assignSelection()
{
	if (!m_project) {
		return;
	}

	const QString uuid = selectedUuid();
	if (uuid.isEmpty()) {
		return;
	}

	assignTo(m_project.data()->locationTree().path(uuid));
}

/**
	@brief LocationManagerDialog::unassignSelection
*/
void LocationManagerDialog::unassignSelection()
{
	assignTo(QString());
}

/**
	@brief LocationManagerDialog::assignTo
	@param path where the folio selection goes, empty to take it out of
	wherever it is
*/
void LocationManagerDialog::assignTo(const QString &path)
{
	if (!m_project) {
		return;
	}

	const QList<Element *> elements = selectedElements();
	if (elements.isEmpty())
	{
		say(tr("Rien n'est sélectionné sur le folio."), true);
		return;
	}

	QUndoStack *stack = m_project.data()->undoStack();
	if (!stack)
	{
		say(tr("Ce projet n'a pas de pile d'annulation."), true);
		return;
	}

	AssignLocationCommand *command =
		new AssignLocationCommand(elements, path);
	const int changed = command->componentCount();
	if (changed == 0)
	{
			//nothing to undo, so nothing is stacked: an undo that puts
			//back exactly what was already there is a step the person
			//has to walk over for no reason.
		delete command;
		say(path.isEmpty()
		    ? tr("Ces composants n'ont déjà aucune localisation.")
		    : tr("Ces composants sont déjà dans « %1 ».").arg(path));
		return;
	}

	m_loading = true;
	stack->push(command);
	m_loading = false;

	say(path.isEmpty()
	    ? tr("%n composant(s) retiré(s) de leur localisation.", "", changed)
	    : tr("%n composant(s) affecté(s) à « %1 ».", "", changed).arg(path));

	QTimer::singleShot(0, this, &LocationManagerDialog::reload);
}

/**
	@brief LocationManagerDialog::push
	@param tree the tree as the edit left it
	@param changed the paths that moved, empty when none did
	@param label how the undo stack calls what happened
*/
void LocationManagerDialog::push(const LocationTree &tree,
				 const QMap<QString, QString> &changed,
				 const QString &label)
{
	if (!m_project) {
		return;
	}

	QUndoStack *stack = m_project.data()->undoStack();
	if (!stack)
	{
		say(tr("Ce projet n'a pas de pile d'annulation."), true);
		return;
	}

	EditLocationTreeCommand *command =
		new EditLocationTreeCommand(m_project.data(), tree, changed,
					    label);
	const int followed = command->componentCount();

	m_loading = true;
	stack->push(command);
	m_loading = false;

	say(followed > 0
	    ? label + QString(" — ")
		+ tr("%n composant(s) ont suivi.", "", followed)
	    : label);

	QTimer::singleShot(0, this, &LocationManagerDialog::reload);
}

/**
	@brief LocationManagerDialog::componentCounts
	@param unassigned filled with how many components name no location
	@return how many components stand on each path of the project
*/
QHash<QString, int> LocationManagerDialog::componentCounts(int *unassigned) const
{
	QHash<QString, int> counts;
	int without = 0;

	if (!m_project)
	{
		if (unassigned) {
			*unassigned = 0;
		}
		return counts;
	}

	const QList<Diagram *> diagrams = m_project.data()->diagrams();
	for (Diagram *diagram : diagrams)
	{
		const QList<QGraphicsItem *> items = diagram->items();
		for (QGraphicsItem *item : items)
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
			//A folio report is an arrow, not a thing in a
			//cabinet: counting it would leave the report of
			//CU-32.8 with items nobody can ever assign.
			if (!isLocatableElement(element)) {
				continue;
			}

			const QString path =
				element->elementInformations()
					.value(QETInformation::ELMT_LOCATION_PATH)
					.toString();
			if (path.isEmpty()) {
				++without;
			} else {
				counts.insert(path, counts.value(path) + 1);
			}
		}
	}

	if (unassigned) {
		*unassigned = without;
	}
	return counts;
}

/**
	@brief LocationManagerDialog::selectedElements
	@return the components the person has selected, on whichever folio

	Every folio is asked, and not only the one in front: a selection made
	on a folio does not go away when another one is brought up, and the
	person who selected on two of them meant both.
*/
QList<Element *> LocationManagerDialog::selectedElements() const
{
	QList<Element *> elements;
	if (!m_project) {
		return elements;
	}

	const QList<Diagram *> diagrams = m_project.data()->diagrams();
	for (Diagram *diagram : diagrams)
	{
		const QList<QGraphicsItem *> items = diagram->selectedItems();
		for (QGraphicsItem *item : items)
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
			if (isLocatableElement(element)) {
				elements.append(element);
			}
		}
	}

	return elements;
}

/**
	@brief LocationManagerDialog::selectedUuid
	@return the uuid of the selected row, empty when nothing is selected
*/
QString LocationManagerDialog::selectedUuid() const
{
	if (!m_tree) {
		return QString();
	}

	const QList<QTreeWidgetItem *> selected = m_tree->selectedItems();
	if (selected.isEmpty()) {
		return QString();
	}

	return selected.first()->data(CodeColumn, UUID_ROLE).toString();
}

/**
	@brief LocationManagerDialog::freeCode
	@param tree the tree the location is going into
	@param parent_uuid the location it is going inside, empty for the top
	level
	@return a code no sibling answers to yet, so that the usual case is
	Enter and nothing else
*/
QString LocationManagerDialog::freeCode(const LocationTree &tree,
					const QString &parent_uuid) const
{
	const QStringList siblings = parent_uuid.isEmpty()
		? tree.rootUuids()
		: tree.childUuids(parent_uuid);

	QStringList taken;
	for (const QString &uuid : siblings) {
		taken.append(tree.location(uuid).code);
	}

	for (int number = 1 ; number < 1000 ; ++number)
	{
		const QString code = QString("Q%1").arg(number);
		if (!taken.contains(code)) {
			return code;
		}
	}

	return QString();
}

/**
	@brief LocationManagerDialog::say
	@param message what to write on the status line
	@param problem true to write it in red
*/
void LocationManagerDialog::say(const QString &message, bool problem)
{
	if (!m_status) {
		return;
	}

	m_status->setText(message);

	QPalette status_palette = m_status->palette();
	status_palette.setColor(QPalette::WindowText,
				problem ? QColor(Qt::red)
					: palette().color(QPalette::WindowText));
	m_status->setPalette(status_palette);
}
