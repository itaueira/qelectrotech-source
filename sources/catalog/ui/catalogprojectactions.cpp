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
#include "catalogprojectactions.h"

#include "../../ElementsCollection/elementslocation.h"
#include "../../diagram.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetgraphicsitem/terminal.h"
#include "../../qetproject.h"
#include "../../undocommand/assigncatalogpartcommand.h"
#include "../catalog.h"
#include "../catalogassignment.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>

namespace
{
	/**
		The element types a bill of material cares about. A folio report or a
		thumbnail is drawing, not something that gets bought, and counting it
		as "component without a part" would make the report useless on the
		very first run.
	*/
	bool isBuyableComponent(const Element *element)
	{
		if (!element) {
			return false;
		}
		const ElementData::Type type = element->elementData().m_type;
		return type == ElementData::Simple
		       || type == ElementData::Master
		       || type == ElementData::Slave;
	}

	QHash<QString, QString> informationOf(const Element *element)
	{
		QHash<QString, QString> values;
		const DiagramContext context = element->elementInformations();
		const QList<QString> keys = context.keys();
		for (const QString &key : keys) {
			values.insert(key, context.value(key).toString());
		}
		return values;
	}
}

/**
	@brief CatalogProjectActions::components
	@param project
	@return every component of @a project
*/
QList<Element *> CatalogProjectActions::components(QETProject *project)
{
	QList<Element *> found;
	if (!project) {
		return found;
	}

	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
		const QList<QGraphicsItem *> items = diagram->items();
		for (QGraphicsItem *item : items)
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
			if (isBuyableComponent(element)) {
				found.append(element);
			}
		}
	}
	return found;
}

/**
	@brief CatalogProjectActions::componentsWithoutPart
	@param project
	@return every component of @a project with no catalog part assigned
*/
QList<Element *> CatalogProjectActions::componentsWithoutPart(QETProject *project)
{
	QList<Element *> found;
	const QList<Element *> all = components(project);
	for (Element *element : all)
	{
		if (CatalogAssignment::isWithoutPart(informationOf(element))) {
			found.append(element);
		}
	}
	return found;
}

/**
	@brief CatalogProjectActions::partFromElements
	@param catalog
	@param elements
	@return a part carrying the pins of @a elements
*/
CatalogPart CatalogProjectActions::partFromElements(const Catalog &catalog,
						    const QList<Element *> &elements)
{
	CatalogPart part;
	if (elements.isEmpty()) {
		return part;
	}

	// The class: whatever the components already say, otherwise Component.
	const CatalogClass component_class = catalog.classByKey(QStringLiteral("component"));
	part.class_id = component_class.isNull() ? 0 : component_class.id;

	// Take the values the components already carry - the draughtsman typed
	// the manufacturer and the description while drawing, and retyping them
	// in the part dialog is exactly the waste this flow removes.
	const QHash<QString, QString> first_values = informationOf(elements.first());
	const QList<CatalogProperty> properties = catalog.effectiveProperties(part.class_id);
	for (const CatalogProperty &property : properties)
	{
		const QString value = first_values.value(property.key);
		if (!value.isEmpty()) {
			part.setValue(property.key, value);
		}
	}
	part.code = first_values.value(CatalogAssignment::partCodeKey());

	int order = 1;
	for (Element *element : elements)
	{
		if (!element) {
			continue;
		}
		const QString group = element->location().path();
		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *terminal : terminals)
		{
			CatalogPin pin(terminal->name(), CatalogPinRole::Unknown);

			// The symbol may already say what the pair is for: the element
			// editor knows normally open, normally closed and common.
			switch (terminal->terminalType())
			{
				case TerminalData::No:
					pin.role = CatalogPinRole::ContactNo;
					break;
				case TerminalData::Nc:
					pin.role = CatalogPinRole::ContactNc;
					break;
				default:
					pin.role = CatalogPinRole::Terminal;
					break;
			}

			pin.group = group;
			pin.order_index = order++;
			part.pins.append(pin);
		}
	}

	part.origin = QStringLiteral("project");
	return part;
}

/**
	@brief CatalogProjectActions::assignPart
	@param elements
	@param catalog
	@param part
	@return how many components were touched
*/
int CatalogProjectActions::assignPart(const QList<Element *> &elements,
				      const Catalog &catalog,
				      const CatalogPart &part)
{
	if (elements.isEmpty() || part.isNull()) {
		return 0;
	}

	Diagram *diagram = nullptr;
	for (Element *element : elements)
	{
		if (element && element->diagram())
		{
			diagram = element->diagram();
			break;
		}
	}
	if (!diagram) {
		return 0;
	}

	auto *command = new AssignCatalogPartCommand(elements, catalog, part);
	const int count = command->componentCount();
	if (count == 0)
	{
		delete command;
		return 0;
	}
	diagram->undoStack().push(command);
	return count;
}

/**
	@brief CatalogProjectActions::showMissingPartReport
	@param project
	@param parent
*/
void CatalogProjectActions::showMissingPartReport(QETProject *project, QWidget *parent)
{
	const QList<Element *> all = components(project);
	const QList<Element *> missing = componentsWithoutPart(project);

	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Composants sans pièce"));
	dialog.resize(620, 460);

	QLabel *summary = new QLabel(&dialog);
	summary->setWordWrap(true);
	if (missing.isEmpty())
	{
		summary->setText(QObject::tr("Les %n composant(s) du projet ont une pièce attribuée. "
					     "La nomenclature est complète.", "", all.size()));
	}
	else
	{
		summary->setText(QObject::tr("%1 composant(s) sur %2 n'ont pas de pièce attribuée. "
					     "Double-cliquez une ligne pour aller au composant.")
				 .arg(missing.size()).arg(all.size()));
	}

	QTableWidget *table = new QTableWidget(&dialog);
	table->setColumnCount(3);
	table->setHorizontalHeaderLabels({ QObject::tr("Repère"),
					   QObject::tr("Folio"),
					   QObject::tr("Symbole") });
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->verticalHeader()->setVisible(false);
	table->horizontalHeader()->setStretchLastSection(true);
	table->setRowCount(missing.size());

	for (int row = 0 ; row < missing.size() ; ++row)
	{
		Element *element = missing.at(row);
		const QHash<QString, QString> values = informationOf(element);

		QString folio;
		if (element->diagram() && element->diagram()->project())
		{
			folio = QString::number(
					element->diagram()->project()->folioIndex(element->diagram()) + 1);
		}

		QString label = values.value(QStringLiteral("label"));
		if (label.isEmpty()) {
			label = QObject::tr("(sans repère)");
		}

		table->setItem(row, 0, new QTableWidgetItem(label));
		table->setItem(row, 1, new QTableWidgetItem(folio));
		table->setItem(row, 2, new QTableWidgetItem(element->name()));
	}
	table->resizeColumnsToContents();

	QObject::connect(table, &QTableWidget::doubleClicked, table,
			 [table, missing](const QModelIndex &index)
	{
		const int row = index.row();
		if (row < 0 || row >= missing.size()) {
			return;
		}
		Element *element = missing.at(row);
		if (!element || !element->diagram()) {
			return;
		}
		element->diagram()->showMe();
		element->diagram()->clearSelection();
		element->setSelected(true);
		element->ensureVisible();
	});

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);

	QVBoxLayout *layout = new QVBoxLayout(&dialog);
	layout->addWidget(summary);
	layout->addWidget(table);
	layout->addWidget(buttons);

	dialog.exec();
}
