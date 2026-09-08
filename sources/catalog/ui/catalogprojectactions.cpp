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
#include "catalogbrowserdialog.h"
#include "catalogpartdialog.h"
#include <algorithm>
#include <QPushButton>
#include "../catalogclass.h"
#include "../../qetapp.h"
#include "../../undocommand/changeelementinformationcommand.h"
#include <QMessageBox>
#include <QInputDialog>

#include "../../ElementsCollection/elementslocation.h"
#include "../../diagram.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetgraphicsitem/terminal.h"
#include "../../qetproject.h"
#include "../../undocommand/assigncatalogpartcommand.h"
#include "../catalog.h"
#include "../catalogassignment.h"
#include "../physicalview.h"

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

	/// What the catalogue holds about the body of one product code.
	struct PartBody
	{
			/// true when the catalogue holds that code at all
		bool known = false;
			/// what the catalogue calls the product
		QString description;
			/// how big it is, and where each number was written
		CatalogPhysicalView view;
	};

	/**
		Read one product code out of the catalogue.

		The same shape MountingPartReader::viewOfPart reads with, and that
		is the point: the report says a product is ready exactly when the
		layout would draw it. Two readings of the ten keys would be two
		answers, and the one that disagreed would be the one nobody ran.
	*/
	PartBody bodyOf(const Catalog &catalog, const QString &code)
	{
		PartBody body;
		const CatalogPart part = catalog.partByCode(code);
		body.known = !part.isNull();
		if (!body.known) {
			return body;
		}

			//Fetched once for the product and not once per key:
			//Catalog::effectiveProperty walks the ancestry of the class
			//on every call, and ten calls would walk it ten times.
		QHash<QString, CatalogProperty> properties;
		const QList<CatalogProperty> declared =
				catalog.effectiveProperties(part.class_id);
		for (const CatalogProperty &property : declared) {
			properties.insert(property.key, property);
		}

		const QHash<QString, QString> values = catalog.effectiveValues(part);
		body.description = values.value(QStringLiteral("designation")).trimmed();

			//With the origins, and not without: a width that came from
			//the initial value of a class is the same number and not the
			//same fact, and this report is the one place that difference
			//has a reader.
		body.view = CatalogPhysicalView::read(values, properties,
						      catalog.valueOrigins(part));
		return body;
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
	@return the part the components already point to, corrected with what they
	carry and with the terminals they draw; a new Component part when they point
	at nothing yet
*/
CatalogPart CatalogProjectActions::partFromElements(const Catalog &catalog,
						    const QList<Element *> &elements)
{
	CatalogPart part;
	if (elements.isEmpty()) {
		return part;
	}

	// The values the components already carry - the designer typed the
	// manufacturer and the description while drawing, and retyping them in the
	// part dialog is exactly the waste this flow removes. They also say which
	// part this is: registering from a component that already has one means
	// correcting that part, not replacing it with a bare generic one. Why that
	// matters is written on CatalogAssignment::partFromValues.
	part = CatalogAssignment::partFromValues(catalog, informationOf(elements.first()));

	QList<CatalogPin> drawn;
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
			drawn.append(pin);
		}
	}

	// The pin map of a part that already exists was made once, by hand or by
	// a package, and it says more than terminals the symbols never named.
	// Only a part with no pins at all takes what the drawing can give.
	if (part.pins.isEmpty()) {
		part.pins = drawn;
	}
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
		//Not const, and not copied into the lambdas below: the table shrinks as
		//rows are assigned, and a list that does not shrink with it makes every
		//row index below the removed one point at the wrong component. The
		//double click read that stale index and walked to a component the user
		//had not chosen.
	QList<Element *> missing = componentsWithoutPart(project);

	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Composants sans pièce"));
	dialog.resize(620, 460);

	QLabel *summary = new QLabel(&dialog);
	summary->setWordWrap(true);

		//The status line states one single fact - how many components are still
		//without a part - and it must read the same before and after an assignment.
		//Two strings would mean two translations and a wording that changes under
		//the user's eyes while nothing but the count did.
	auto setSummary = [summary, all](int remaining)
	{
		if (!remaining)
		{
			summary->setText(QObject::tr("Les %n composant(s) du projet ont une pièce attribuée. "
						     "La nomenclature est complète.", "", all.size()));
		}
		else
		{
			summary->setText(QObject::tr("%1 composant(s) sur %2 n'ont pas de pièce attribuée. "
						     "Double-cliquez une ligne pour aller au composant.")
					 .arg(remaining).arg(all.size()));
		}
	};
	setSummary(missing.size());

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

		//The double click was half the way: it took the sheet to the component and
		//left the modal window on top of it. Whoever clicked wanted to see the
		//component, so the window steps out of the way.
		//activated, not doubleClicked: it also fires on Enter, so the table
		//can be used without a mouse, and it follows the platform convention.
	QObject::connect(table, &QTableWidget::activated, table,
			 [table, &missing, &dialog](const QModelIndex &index)
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
		dialog.accept();
	});

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);

		//The report was a dead end: it said what was missing and offered no way to
		//settle it. The path now closes here - pick the rows, pick the part, assign,
		//and the list shortens on the spot.
	QPushButton *assign = new QPushButton(
				QObject::tr("Attribuer une pièce…"), &dialog);
	assign->setToolTip(QObject::tr(
		"Attribue une pièce du catalogue aux composants sélectionnés dans "
		"cette liste, sans avoir à les retrouver un par un sur les folios."));
	assign->setEnabled(false);
	buttons->addButton(assign, QDialogButtonBox::ActionRole);

	QObject::connect(table, &QTableWidget::itemSelectionChanged, assign,
			 [table, assign]()
	{
		assign->setEnabled(!table->selectedItems().isEmpty());
	});

	QObject::connect(assign, &QPushButton::clicked, &dialog,
			 [&dialog, table, &missing, setSummary]()
	{
		Catalog *catalog = QETApp::catalog();
		if (!catalog) {
			return;
		}

		QList<Element *> chosen;
		const QList<QTableWidgetSelectionRange> ranges = table->selectedRanges();
		for (const QTableWidgetSelectionRange &range : ranges) {
			for (int row = range.topRow() ; row <= range.bottomRow() ; ++row) {
				if (row >= 0 && row < missing.size()) {
					chosen << missing.at(row);
				}
			}
		}
		if (chosen.isEmpty()) {
			return;
		}

			//The browser knows nothing about the sheet: opened from here, "New part"
			//has to be born with one pin per terminal of the symbol and with what the
			//designer already typed, the way it is born when the part is created
			//from the sheet.
		const CatalogPart part = CatalogBrowserDialog::choosePart(
					catalog, &dialog,
					partFromElements(*catalog, chosen));
		if (part.isNull()) {
			return;
		}

		const int touched = assignPart(chosen, *catalog, part);
		if (!touched) {
			return;
		}

			//The assigned rows leave the list, because the list is "what is missing"
			//and they are not missing any more. A list that does not shorten as it is
			//worked through says nothing about how close the work is to done.
		QList<int> rows;
		for (Element *element : chosen) {
			const int row = missing.indexOf(element);
			if (row >= 0) {
				rows << row;
			}
		}
		std::sort(rows.begin(), rows.end(), std::greater<int>());
		for (int row : rows) {
			table->removeRow(row);
			missing.removeAt(row);
		}

		setSummary(table->rowCount());
	});

	QVBoxLayout *layout = new QVBoxLayout(&dialog);
	layout->addWidget(summary);
	layout->addWidget(table);
	layout->addWidget(buttons);

	dialog.exec();
}

/**
	@brief CatalogProjectActions::MissingPhysicalView::describe
	@return what is missing, in one sentence
*/
QString CatalogProjectActions::MissingPhysicalView::describe() const
{
	switch (state)
	{
		case UnknownPart:
			return tr("pièce absente du catalogue");
		case NoMeasure:
			return tr("ni largeur ni hauteur");
		case WidthOnly:
				//The clause is added only when it says something: a
				//width typed on the product needs no comment, while
				//a width taken from the class is a number nobody
				//measured on this product.
			return measured_origin.isFromClass()
			       ? tr("hauteur manquante, largeur héritée de la classe %1")
				 .arg(measured_origin.className())
			       : tr("hauteur manquante");
		case HeightOnly:
			return measured_origin.isFromClass()
			       ? tr("largeur manquante, hauteur héritée de la classe %1")
				 .arg(measured_origin.className())
			       : tr("largeur manquante");
	}
	return QString();
}

/**
	@brief CatalogProjectActions::PhysicalViewReport::codesToMeasure
	@return the distinct product codes to measure, sorted

	The number the cataloguing queue is actually as long as. Sixty
	components missing a body are not sixty records to fill in - the same
	contactor is drawn twelve times - and a report that counted components
	alone would make the work look twelve times bigger than it is.
*/
QStringList CatalogProjectActions::PhysicalViewReport::codesToMeasure() const
{
	QStringList codes;
	for (const MissingPhysicalView &entry : missing)
	{
		if (!codes.contains(entry.part_code)) {
			codes << entry.part_code;
		}
	}
	codes.sort();
	return codes;
}

/**
	@brief CatalogProjectActions::physicalViewReport
	@param project
	@param catalog
	@return which components point at a product the catalogue cannot draw
*/
CatalogProjectActions::PhysicalViewReport
CatalogProjectActions::physicalViewReport(QETProject *project, const Catalog &catalog)
{
	PhysicalViewReport report;

	const QList<Element *> all = components(project);
	report.components   = all.size();
	report.catalog_read = catalog.isOpen();

		//One reading per product code and not per component: a plate
		//with twelve of the same contactor would otherwise ask the data
		//base the same three questions twelve times.
	QHash<QString, PartBody> read;

	for (Element *element : all)
	{
		const QHash<QString, QString> values = informationOf(element);

			//A component nobody bought a product for is the subject
			//of the report next door, and the two lists mixed would
			//be one heading over two problems. It is not counted as
			//missing here and it is not counted in with_part either,
			//which is what the sentence divides by.
		if (CatalogAssignment::isWithoutPart(values)) {
			continue;
		}
		++report.with_part;

		if (!report.catalog_read) {
			continue;
		}

		const QString code =
				values.value(CatalogAssignment::partCodeKey()).trimmed();
		if (!read.contains(code)) {
			read.insert(code, bodyOf(catalog, code));
		}
		const PartBody body = read.value(code);

		if (body.known && body.view.hasPhysicalView())
		{
				//Drawable, so out of the list - and counted, so the
				//summary can say how much of what is drawable is
				//drawable on a number that belongs to a class.
			if (body.view.width.origin.isFromClass()
					|| body.view.height.origin.isFromClass()) {
				++report.inherited_size;
			}
			continue;
		}

		MissingPhysicalView entry;
		entry.element     = element;
		entry.part_code   = code;
		entry.description = body.description;

		if (!body.known) {
			entry.state = MissingPhysicalView::UnknownPart;
		}
		else if (body.view.hasWidth())
		{
			entry.state           = MissingPhysicalView::WidthOnly;
			entry.measured_origin = body.view.width.origin;
		}
		else if (body.view.hasHeight())
		{
			entry.state           = MissingPhysicalView::HeightOnly;
			entry.measured_origin = body.view.height.origin;
		}
		else {
			entry.state = MissingPhysicalView::NoMeasure;
		}

		report.missing << entry;
	}

		//By product code first, because the work is done product by
		//product: the twelve rows of the same contactor are one visit to
		//the part dialogue, and a list in folio order would scatter them
		//over the whole report. The tag breaks the tie, so two runs over
		//the same project answer in the same order.
	std::sort(report.missing.begin(), report.missing.end(),
		  [](const MissingPhysicalView &left, const MissingPhysicalView &right)
	{
		if (left.part_code != right.part_code) {
			return left.part_code < right.part_code;
		}
		const QString left_tag  = left.element  ? left.element->actualLabel()  : QString();
		const QString right_tag = right.element ? right.element->actualLabel() : QString();
		return left_tag < right_tag;
	});

	return report;
}

/**
	@brief CatalogProjectActions::showMissingPhysicalViewReport
	@param project
	@param parent
*/
void CatalogProjectActions::showMissingPhysicalViewReport(QETProject *project,
							  QWidget *parent)
{
	Catalog *catalog = QETApp::catalog();
	const PhysicalViewReport report = catalog
					  ? physicalViewReport(project, *catalog)
					  : PhysicalViewReport();

		//The rows are held beside the table and shortened with it. Held,
		//and not read back from the table, because the double click
		//needs the element behind the row: an index into a list that no
		//longer matches the rows walks the folio to the wrong component,
		//which looks like navigation and is not.
	QList<MissingPhysicalView> rows = report.missing;

	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Pièces sans vue physique"));
	dialog.resize(760, 480);

	QLabel *summary = new QLabel(&dialog);
	summary->setWordWrap(true);

		//Four states and not two, because they are four different pieces
		//of news: the catalogue did not answer, nothing in the project
		//points at a product, everything that does is measured, and the
		//working state. One sentence per state, for the reason written
		//on the report next door - what must never happen is the wording
		//changing while nothing but a count did.
	auto setSummary = [summary, report, &rows]()
	{
		QStringList sentences;

		if (!report.catalog_read)
		{
			sentences << QObject::tr(
				"Le catalogue n'a pas répondu : impossible de dire "
				"quelles pièces sont mesurées.");
		}
		else if (!report.with_part)
		{
			sentences << QObject::tr(
				"Aucun des %n composant(s) du projet n'a de pièce "
				"attribuée : c'est « Composants sans pièce » qui "
				"répond à cela.",
				"", report.components);
		}
		else if (rows.isEmpty())
		{
			sentences << QObject::tr(
				"Les %n composant(s) du projet qui ont une pièce ont "
				"une vue physique. Le plan peut être dessiné.",
				"", report.with_part);
		}
		else
		{
			QStringList codes;
			for (const MissingPhysicalView &entry : rows)
			{
				if (!codes.contains(entry.part_code)) {
					codes << entry.part_code;
				}
			}
			sentences << QObject::tr(
				"%1 composant(s) sur %2 utilisent une pièce sans vue "
				"physique : %3 code(s) à mesurer. Double-cliquez une "
				"ligne pour aller au composant.")
				     .arg(rows.size())
				     .arg(report.with_part)
				     .arg(codes.size());
		}

			//Said apart, and only when there is something to say: an
			//inherited size is not a missing size, so it cannot be a
			//row, and a report that never mentioned it would let
			//inheritance pass for cataloguing.
		if (report.catalog_read && report.inherited_size)
		{
			sentences << QObject::tr(
				"%n composant(s) tiennent leur taille de leur classe "
				"et non de leur pièce.",
				"", report.inherited_size);
		}

		summary->setText(sentences.join(QLatin1Char(' ')));
	};
	setSummary();

	QTableWidget *table = new QTableWidget(&dialog);
	table->setColumnCount(5);
	table->setHorizontalHeaderLabels({ QObject::tr("Repère"),
					   QObject::tr("Folio"),
					   QObject::tr("Code"),
					   QObject::tr("Désignation"),
					   QObject::tr("Manque") });
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->verticalHeader()->setVisible(false);
	table->horizontalHeader()->setStretchLastSection(true);
	table->setRowCount(rows.size());

	for (int row = 0 ; row < rows.size() ; ++row)
	{
		const MissingPhysicalView &entry = rows.at(row);

		QString folio;
		if (entry.element && entry.element->diagram()
				&& entry.element->diagram()->project())
		{
			folio = QString::number(
					entry.element->diagram()->project()
					->folioIndex(entry.element->diagram()) + 1);
		}

		QString label = entry.element
				? informationOf(entry.element).value(QStringLiteral("label"))
				: QString();
		if (label.isEmpty()) {
			label = QObject::tr("(sans repère)");
		}

		table->setItem(row, 0, new QTableWidgetItem(label));
		table->setItem(row, 1, new QTableWidgetItem(folio));
		table->setItem(row, 2, new QTableWidgetItem(entry.part_code));
		table->setItem(row, 3, new QTableWidgetItem(entry.description));
		table->setItem(row, 4, new QTableWidgetItem(entry.describe()));
	}
	table->resizeColumnsToContents();

		//activated, not doubleClicked: it also fires on Enter, so the
		//table can be used without a mouse. Same move as the report next
		//door - whoever clicked wanted to see the component, so the
		//window steps out of the way.
	QObject::connect(table, &QTableWidget::activated, table,
			 [&rows, &dialog](const QModelIndex &index)
	{
		const int row = index.row();
		if (row < 0 || row >= rows.size()) {
			return;
		}
		Element *element = rows.at(row).element;
		if (!element || !element->diagram()) {
			return;
		}
		element->diagram()->showMe();
		element->diagram()->clearSelection();
		element->setSelected(true);
		element->ensureVisible();
		dialog.accept();
	});

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);

		//The queue closes here, like the one next door: pick the rows,
		//fill in the millimetres, and the rows leave. A report that only
		//states what is missing sends the person to another window to
		//find again what this one already names.
	QPushButton *measure = new QPushButton(
				QObject::tr("Mesurer la pièce…"), &dialog);
	measure->setToolTip(QObject::tr(
		"Ouvre la fiche de la pièce des lignes sélectionnées pour y saisir "
		"les millimètres : une fiche par code, et les lignes mesurées "
		"quittent la liste."));
	measure->setEnabled(false);
	buttons->addButton(measure, QDialogButtonBox::ActionRole);

	QObject::connect(table, &QTableWidget::itemSelectionChanged, measure,
			 [table, measure]()
	{
		measure->setEnabled(!table->selectedItems().isEmpty());
	});

	QObject::connect(measure, &QPushButton::clicked, &dialog,
			 [&dialog, table, &rows, setSummary]()
	{
		Catalog *catalog = QETApp::catalog();
		if (!catalog || !catalog->isOpen()) {
			return;
		}

			//One visit per code and not per row: twelve rows of the
			//same contactor are one record to fill in.
		QStringList codes;
		const QList<QTableWidgetSelectionRange> ranges = table->selectedRanges();
		for (const QTableWidgetSelectionRange &range : ranges)
		{
			for (int row = range.topRow() ; row <= range.bottomRow() ; ++row)
			{
				if (row >= 0 && row < rows.size()
						&& !codes.contains(rows.at(row).part_code)) {
					codes << rows.at(row).part_code;
				}
			}
		}
		if (codes.isEmpty()) {
			return;
		}

		for (const QString &code : codes)
		{
			CatalogPart part = catalog->partByCode(code);
			if (part.isNull())
			{
					//The code is drawn on a folio and the
					//catalogue does not hold it. The record is
					//born here carrying that code, rather than
					//sending the person to the browser to type
					//again what this window already names.
				const CatalogClass component =
						catalog->classByKey(QStringLiteral("component"));
				part = CatalogPart(code, component.isNull() ? 0 : component.id);
			}

			CatalogPartDialog part_dialog(catalog, part, &dialog);
			if (part_dialog.exec() != QDialog::Accepted)
			{
					//Backing out of one record stops the walk:
					//somebody who cancels the second of five
					//dialogues did not ask for the third.
				break;
			}
		}

			//Read back rather than assumed: the record may have been
			//saved with the width alone, and a row leaving the list on
			//the strength of the dialogue having been accepted would be
			//a product reported as ready that the layout cannot draw.
		QHash<QString, PartBody> read;
		for (int row = rows.size() - 1 ; row >= 0 ; --row)
		{
			const QString code = rows.at(row).part_code;
			if (!read.contains(code)) {
				read.insert(code, bodyOf(*catalog, code));
			}
			const PartBody body = read.value(code);
			if (body.known && body.view.hasPhysicalView())
			{
				rows.removeAt(row);
				table->removeRow(row);
			}
		}

		setSummary();
	});

	QVBoxLayout *layout = new QVBoxLayout(&dialog);
	layout->addWidget(summary);
	layout->addWidget(table);
	layout->addWidget(buttons);

	dialog.exec();
}

/**
	@brief CatalogProjectActions::possibleOwners
	@param project
	@param accessory
	@return the components an accessory could belong to
*/
QList<Element *> CatalogProjectActions::possibleOwners(QETProject *project,
						       Element *accessory)
{
	QList<Element *> owners;
	const QList<Element *> all = components(project);
	for (Element *element : all)
	{
			//An accessory does not own itself, and it does not own another
			//accessory either: nesting them would make the bill of material
			//a tree nobody asked for.
		if (element == accessory) {
			continue;
		}
		if (!element->elementInformations()
				.value(CatalogAssignment::accessoryOwnerKey())
				.toString().isEmpty()) {
			continue;
		}
		owners << element;
	}
	return owners;
}

/**
	@brief CatalogProjectActions::linkAccessory
	@param accessory
	@param parent
	@return true when a link was made
*/
bool CatalogProjectActions::linkAccessory(Element *accessory, QWidget *parent)
{
	if (!accessory || !accessory->diagram() ||
			!accessory->diagram()->project()) {
		return false;
	}
	QETProject *project = accessory->diagram()->project();

	Catalog *catalog = QETApp::catalog();
	const QString own_class = accessory->elementInformations()
			.value(QStringLiteral("catalog_class")).toString();
	if (catalog && !own_class.isEmpty())
	{
			//A warning and not a refusal: the shop knows what it drew better
			//than the class tree does, and refusing on a class somebody
			//forgot to set would be refusing the right thing for a
			//bookkeeping reason.
		const CatalogClass symbol_class = catalog->classByKey(own_class);
		if (symbol_class.id > 0 &&
				!catalog->isDescendantOf(symbol_class.id,
							 QStringLiteral("accessory")))
		{
			if (QMessageBox::question(parent,
					QObject::tr("Lier un accessoire"),
					QObject::tr("« %1 » n'est pas de la classe Accessoire, "
						    "mais de « %2 ». Le lier quand même ?")
						.arg(accessory->actualLabel(), symbol_class.name))
					!= QMessageBox::Yes) {
				return false;
			}
		}
	}

	const QList<Element *> owners = possibleOwners(project, accessory);
	if (owners.isEmpty())
	{
		QMessageBox::information(parent, QObject::tr("Lier un accessoire"),
			QObject::tr("Ce projet n'a aucun composant auquel rattacher "
				    "l'accessoire."));
		return false;
	}

		//Named by tag and folio, because a tag alone repeats across folios and
		//the designer is choosing between things they can see.
	QStringList labels;
	for (Element *owner : owners)
	{
		const QString tag = owner->actualLabel().isEmpty()
				? QObject::tr("(sans repère)")
				: owner->actualLabel();
		const QString folio = owner->diagram() && !owner->diagram()->title().isEmpty()
				? owner->diagram()->title()
				: QObject::tr("folio %1").arg(
					  owner->diagram()
					  ? owner->diagram()->folioIndex() + 1 : 0);
		labels << QStringLiteral("%1 — %2").arg(tag, folio);
	}

	const QString current = accessory->elementInformations()
			.value(CatalogAssignment::accessoryOwnerKey()).toString();
	int start = 0;
	for (int i = 0 ; i < owners.size() ; ++i)
	{
		if (owners.at(i)->uuid().toString() == current) {
			start = i;
			break;
		}
	}

	bool chosen = false;
	const QString picked = QInputDialog::getItem(parent,
			QObject::tr("Lier un accessoire"),
			QObject::tr("À quel composant « %1 » appartient-il ?")
				.arg(accessory->actualLabel()),
			labels, start, false, &chosen);
	if (!chosen) {
		return false;
	}

	Element *owner = owners.value(labels.indexOf(picked));
	if (!owner) {
		return false;
	}

	DiagramContext old_information = accessory->elementInformations();
	DiagramContext new_information = old_information;
	new_information.addValue(CatalogAssignment::accessoryOwnerKey(),
				 owner->uuid().toString());
	if (old_information == new_information) {
		return false;
	}

	accessory->diagram()->undoStack().push(
				new ChangeElementInformationCommand(accessory,
								   old_information,
								   new_information));
	return true;
}
