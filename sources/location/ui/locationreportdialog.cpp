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
#include "locationreportdialog.h"

#include "../../diagram.h"
#include "../../diagramcontext.h"
#include "../../diagramposition.h"
#include "../locatableelement.h"
#include "../locationtree.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetinformation.h"
#include "../../qetproject.h"
#include "../../undocommand/assignlocationcommand.h"

#include <QDialogButtonBox>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QStringList>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QVariant>
#include <QPointer>

#include <algorithm>

namespace
{
	enum Column
	{
		NameColumn = 0,
		PositionColumn,
		DesignationColumn,
		TypeColumn,
		PartColumn,
		ColumnCount
	};

		/// index of the component in m_elements, on a component row
	const int INDEX_ROLE = Qt::UserRole + 1;
		/// text the filter matches against, on any row
	const int SEARCH_ROLE = Qt::UserRole + 2;

	/**
		@brief typeLabel
		@param element a locatable component
		@return what kind of thing it is, said in the words of the folio
	*/
	QString typeLabel(const Element *element)
	{
		switch (element->linkType())
		{
			case Element::Terminale:
				return QObject::tr("Borne");
			case Element::Master:
				return QObject::tr("Maître");
			case Element::Slave:
				return QObject::tr("Esclave");
			default:
				return QObject::tr("Composant");
		}
	}

	/**
		@brief positionOf
		@param element a component standing on a folio
		@return where it stands, in the letters and numbers printed on the
		border of the sheet
	*/
	QString positionOf(Element *element)
	{
		Diagram *diagram = element ? element->diagram() : nullptr;
		if (!diagram) {
			return QString();
		}
		return diagram->convertPosition(element->scenePos()).toString();
	}

	/**
		@brief nameOf
		@param element a component
		@return its label when it has one, the name of its symbol when it
		has not - which is the case of the hundred and eighteen terminals
		of a real project before numbering.
	*/
	QString nameOf(Element *element)
	{
		const QString label = element->elementInformations()
				.value(QETInformation::ELMT_LABEL).toString();
		return label.isEmpty() ? element->name() : label;
	}
}

/**
	@brief LocationReportDialog::LocationReportDialog
	@param project the project being reported on
	@param parent parent widget
*/
LocationReportDialog::LocationReportDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	buildWidgets();

	if (m_project)
	{
			//same reasoning as the manager: no signal announces an
			//assignment, but every one of them goes through the undo
			//stack, so the index of the stack is the same news.
		if (QUndoStack *stack = m_project->undoStack()) {
			connect(stack, &QUndoStack::indexChanged,
				this, &LocationReportDialog::stackChanged);
		}
		connect(m_project.data(), &QObject::destroyed,
			this, &QWidget::close);
	}

	reload();
}

/**
	@brief LocationReportDialog::project
	@return the project this window is reporting on
*/
QETProject *LocationReportDialog::project() const
{
	return m_project.data();
}

/**
	@brief LocationReportDialog::buildWidgets
*/
void LocationReportDialog::buildWidgets()
{
	setWindowTitle(tr("Composants non localisés"));

	m_filter = new QLineEdit(this);
	m_filter->setClearButtonEnabled(true);
	m_filter->setPlaceholderText(tr("Filtrer par étiquette, désignation ou "
					"pièce…"));
	connect(m_filter, &QLineEdit::textChanged,
		this, &LocationReportDialog::filterChanged);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(ColumnCount);
	m_tree->setHeaderLabels(QStringList()
				<< tr("Folio / composant")
				<< tr("Position")
				<< tr("Désignation")
				<< tr("Type")
				<< tr("Pièce"));
	m_tree->setRootIsDecorated(true);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tree->header()->setStretchLastSection(false);
	m_tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);

	connect(m_tree, &QTreeWidget::itemSelectionChanged,
		this, &LocationReportDialog::selectionChanged);
	connect(m_tree, &QTreeWidget::itemDoubleClicked,
		this, &LocationReportDialog::itemActivated);

	m_assign = new QPushButton(tr("Affecter à…"), this);
	m_select = new QPushButton(tr("Sélectionner sur le folio"), this);

	m_assign->setToolTip(tr(
		"Mettre les composants choisis ici dans une localisation du "
		"projet. Un folio choisi vaut pour tout ce qu'il contient."));
	m_select->setToolTip(tr(
		"Sélectionner sur les folios les composants choisis ici, pour "
		"les voir avant d'en décider."));

	connect(m_assign, &QPushButton::clicked,
		this, &LocationReportDialog::assignSelection);
	connect(m_select, &QPushButton::clicked,
		this, &LocationReportDialog::selectOnFolio);

	QHBoxLayout *tools = new QHBoxLayout();
	tools->addWidget(new QLabel(tr("Filtre :"), this));
	tools->addWidget(m_filter, 1);
	tools->addWidget(m_select);
	tools->addWidget(m_assign);

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

	resize(880, 560);
}

/**
	@brief LocationReportDialog::reload
	Rebuild the list from the project.
*/
void LocationReportDialog::reload()
{
	if (!m_project) {
		return;
	}

	m_loading = true;
	fill();
	m_loading = false;

	filterChanged();
	selectionChanged();
}

/**
	@brief LocationReportDialog::fill
	One top level row per folio that still has something on it, and the
	components inside. A folio where everything is located does not appear
	at all: an empty branch would read as a problem, and it is the
	opposite.
*/
void LocationReportDialog::fill()
{
	m_tree->clear();

	const QList<Element *> unlocated = unlocatedElements();

	QTreeWidgetItem *folio_item = nullptr;
	Diagram *folio = nullptr;
	int on_folio = 0;

	for (int index = 0 ; index < unlocated.count() ; ++index)
	{
		Element *element = unlocated.at(index);
		Diagram *diagram = element->diagram();

		if (diagram != folio)
		{
			if (folio_item) {
				folio_item->setText(PositionColumn,
						    QString::number(on_folio));
			}

			folio = diagram;
			on_folio = 0;

			const int number = diagram ? diagram->folioIndex() + 1 : 0;
			const QString title = diagram ? diagram->title() : QString();
			folio_item = new QTreeWidgetItem(m_tree);
			folio_item->setText(NameColumn,
					    title.isEmpty()
					    ? tr("Folio %1").arg(number)
					    : tr("Folio %1 — %2").arg(number)
							.arg(title));
			folio_item->setFirstColumnSpanned(false);
			folio_item->setData(NameColumn, SEARCH_ROLE, QString());
		}

		const QString name = nameOf(element);
		const QString designation = element->elementInformations()
				.value(QETInformation::ELMT_DESIGNATION).toString();
		const QString part = element->elementInformations()
				.value(QETInformation::ELMT_PART_CODE).toString();

		QTreeWidgetItem *item = new QTreeWidgetItem(folio_item);
		item->setText(NameColumn, name);
		item->setText(PositionColumn, positionOf(element));
		item->setText(DesignationColumn, designation);
		item->setText(TypeColumn, typeLabel(element));
		item->setText(PartColumn, part);
		item->setData(NameColumn, INDEX_ROLE, index);
		item->setData(NameColumn, SEARCH_ROLE,
			      (QStringList() << name << designation << part
					     << element->name()).join(QLatin1Char(' '))
					.toLower());
		++on_folio;
	}

	if (folio_item) {
		folio_item->setText(PositionColumn, QString::number(on_folio));
	}

	m_tree->expandAll();
	for (int column = 0 ; column < ColumnCount ; ++column) {
		m_tree->resizeColumnToContents(column);
	}

	m_elements.clear();
	for (Element *element : unlocated) {
		m_elements.append(QPointer<Element>(element));
	}

	if (unlocated.isEmpty())
	{
			//CU-32.8 closes here, and the sentence says so rather than
			//leaving an empty table to be read as a failure to load.
		m_summary->setText(tr("Tous les composants du projet sont "
				      "localisés."));
	}
	else
	{
		m_summary->setText(tr("%n composant(s) non localisé(s)", "",
				      unlocated.count()));
	}
}

/**
	@brief LocationReportDialog::unlocatedElements
	@return every component of the project that names no location, folio by
	folio and, inside a folio, in the order the sheet is read.
*/
QList<Element *> LocationReportDialog::unlocatedElements() const
{
	QList<Element *> unlocated;
	if (!m_project) {
		return unlocated;
	}

	const QList<Diagram *> diagrams = m_project.data()->diagrams();
	for (Diagram *diagram : diagrams)
	{
		QList<Element *> on_folio;

		const QList<QGraphicsItem *> items = diagram->items();
		for (QGraphicsItem *item : items)
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
				//the very same test the manager counts with: a
				//folio report is an arrow, and an arrow is
				//nowhere on purpose.
			if (!isLocatableElement(element)) {
				continue;
			}

			const QString path = element->elementInformations()
					.value(QETInformation::ELMT_LOCATION_PATH)
					.toString();
			if (!path.isEmpty()) {
				continue;
			}

			on_folio.append(element);
		}

			//items() hands back the folio in drawing order, which is
			//the order things happened to be added in. The person
			//works down the sheet, so the report is sorted the way
			//the sheet is printed.
		std::sort(on_folio.begin(), on_folio.end(),
			  [](Element *a, Element *b)
		{
			const QString pa = positionOf(a);
			const QString pb = positionOf(b);
			if (pa != pb) {
				return pa < pb;
			}
			return nameOf(a).localeAwareCompare(nameOf(b)) < 0;
		});

		unlocated.append(on_folio);
	}

	return unlocated;
}

/**
	@brief LocationReportDialog::chosenElements
	@return the components the person picked, a folio row counting for
	everything it contains
*/
QList<Element *> LocationReportDialog::chosenElements() const
{
	QList<Element *> chosen;
	if (!m_tree) {
		return chosen;
	}

	const QList<QTreeWidgetItem *> selected = m_tree->selectedItems();
	for (QTreeWidgetItem *item : selected)
	{
		QList<QTreeWidgetItem *> rows;
		if (item->childCount() > 0)
		{
				//a folio row stands for what it contains, and only
				//for what is visible: the person who filtered to
				//"XB" and picked the folio meant the sixteen rows
				//in front of them, not the hundred behind.
			for (int i = 0 ; i < item->childCount() ; ++i)
			{
				QTreeWidgetItem *child = item->child(i);
				if (!child->isHidden()) {
					rows.append(child);
				}
			}
		}
		else
		{
			rows.append(item);
		}

		for (QTreeWidgetItem *row : rows)
		{
			const QVariant index = row->data(NameColumn, INDEX_ROLE);
			if (!index.isValid()) {
				continue;
			}
			Element *element = m_elements.value(index.toInt()).data();
			if (element && !chosen.contains(element)) {
				chosen.append(element);
			}
		}
	}

	return chosen;
}

/**
	@brief LocationReportDialog::selectionChanged
*/
void LocationReportDialog::selectionChanged()
{
	if (m_loading) {
		return;
	}

	const int picked = chosenElements().count();
	m_assign->setEnabled(picked > 0);
	m_select->setEnabled(picked > 0);

	if (picked > 0) {
		say(tr("%n composant(s) choisi(s).", "", picked));
	} else {
		say(QString());
	}
}

/**
	@brief LocationReportDialog::filterChanged
	Hide what does not match, and hide a folio whose components have all
	been hidden - a folio heading with nothing under it says the folio is
	clean, which would be a lie.
*/
void LocationReportDialog::filterChanged()
{
	if (!m_tree || !m_filter) {
		return;
	}

	const QString needle = m_filter->text().trimmed().toLower();

	for (int f = 0 ; f < m_tree->topLevelItemCount() ; ++f)
	{
		QTreeWidgetItem *folio_item = m_tree->topLevelItem(f);
		int shown = 0;

		for (int c = 0 ; c < folio_item->childCount() ; ++c)
		{
			QTreeWidgetItem *child = folio_item->child(c);
			const bool matches = needle.isEmpty()
				|| child->data(NameColumn, SEARCH_ROLE)
					.toString().contains(needle)
				|| folio_item->text(NameColumn).toLower()
					.contains(needle);
			child->setHidden(!matches);
			if (matches) {
				++shown;
			}
		}

		folio_item->setHidden(shown == 0);
	}

	selectionChanged();
}

/**
	@brief LocationReportDialog::itemActivated
	@param item the row that was double clicked
	@param column unused
	CU-32.3: a terminal whose label says nothing is answered by looking at
	it, not by guessing.
*/
void LocationReportDialog::itemActivated(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column)

	if (!item) {
		return;
	}

	const QVariant index = item->data(NameColumn, INDEX_ROLE);
	if (!index.isValid()) {
		return;
	}

	if (Element *element = m_elements.value(index.toInt()).data()) {
		emit goToElement(element);
	}
}

/**
	@brief LocationReportDialog::stackChanged
	Somebody assigned, undid or redid, here or elsewhere.
*/
void LocationReportDialog::stackChanged()
{
	if (m_loading) {
		return;
	}
	reload();
}

/**
	@brief LocationReportDialog::selectOnFolio
	Put the chosen components into the selection of their folios, so that
	the manager, the properties panel and everything else that works on a
	selection can be used on them.
*/
void LocationReportDialog::selectOnFolio()
{
	if (!m_project) {
		return;
	}

	const QList<Element *> chosen = chosenElements();
	if (chosen.isEmpty()) {
		return;
	}

	const QList<Diagram *> diagrams = m_project.data()->diagrams();
	for (Diagram *diagram : diagrams) {
		diagram->clearSelection();
	}

	for (Element *element : chosen) {
		element->setSelected(true);
	}

	say(tr("%n composant(s) sélectionné(s) sur les folios.", "",
	       chosen.count()));
}

/**
	@brief LocationReportDialog::assignSelection
	CU-32.8: the list is walked down to zero from inside itself, which is
	the whole reason it groups by folio.
*/
void LocationReportDialog::assignSelection()
{
	if (!m_project) {
		return;
	}

	const QList<Element *> chosen = chosenElements();
	if (chosen.isEmpty()) {
		return;
	}

	const LocationTree tree = m_project.data()->locationTree();
	if (tree.isEmpty())
	{
		say(tr("Le projet n'a encore aucune localisation. Elles se font "
		       "dans « Armoires et localisations… »."), true);
		return;
	}

	QStringList labels;
	QStringList paths;
	for (int index = 0 ; index < tree.count() ; ++index)
	{
		const ProjectLocation location = tree.at(index);
		const QString path = tree.path(location.uuid);

		QString label = LocationTree::iecTag(path);
		if (!location.name.isEmpty()) {
			label = label + QString(" — ") + location.name;
		}
		labels.append(label);
		paths.append(path);
	}

	bool accepted = false;
	const QString chosen_label = QInputDialog::getItem(
			this, tr("Affecter une localisation"),
			tr("Mettre %n composant(s) dans :", "", chosen.count()),
			labels, 0, false, &accepted);
	if (!accepted) {
		return;
	}

	const QString path = paths.value(labels.indexOf(chosen_label));
	if (path.isEmpty()) {
		return;
	}

	QUndoStack *stack = m_project.data()->undoStack();
	if (!stack)
	{
		say(tr("Ce projet n'a pas de pile d'annulation."), true);
		return;
	}

	AssignLocationCommand *command =
			new AssignLocationCommand(chosen, path);
	if (command->componentCount() == 0)
	{
			//cannot happen from this window - everything in it is
			//unlocated by construction - but the command says so and
			//pushing an empty one would put a caption on the stack
			//with nothing under it.
		delete command;
		say(tr("Rien à affecter."));
		return;
	}

	stack->push(command);
	say(tr("%n composant(s) affecté(s) à « %1 ».", "", chosen.count())
	    .arg(LocationTree::iecTag(path)));
}

/**
	@brief LocationReportDialog::say
	@param message what to write on the status line
	@param problem true to write it in red
*/
void LocationReportDialog::say(const QString &message, bool problem)
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
