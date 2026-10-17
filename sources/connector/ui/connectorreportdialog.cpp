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
#include "connectorreportdialog.h"

#include "../../catalog/catalog.h"
#include "../../catalog/catalogpart.h"
#include "../../catalog/ui/catalogbrowserdialog.h"
#include "../../catalog/ui/catalogprojectactions.h"
#include "../../diagram.h"
#include "../../qetapp.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetinformation.h"
#include "../../qetproject.h"
#include "../connectorcheck.h"
#include "../connectorswap.h"

#include <algorithm>

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>

namespace
{
	using DrawnConnector = ConnectorCheck::DrawnConnector;

	/// The tag of @a element, or a legible stand in for a component with none
	QString tagOf(Element *element)
	{
		const QString label = element
				      ? element->elementInformations()
					.value(QETInformation::ELMT_LABEL).toString()
				      : QString();
		return label.isEmpty() ? QObject::tr("(sans repère)") : label;
	}

	/// The 1-based folio @a element is drawn on, empty when it is on none
	QString folioOf(Element *element)
	{
		if (!element || !element->diagram() || !element->diagram()->project()) {
			return QString();
		}
		return QString::number(
				element->diagram()->project()
				->folioIndex(element->diagram()) + 1);
	}

	/**
		The way number of @a element, exactly as it is stored.

		Neither trimmed nor folded, like everywhere else the label of a pin
		is shown: a "9 " printed as if it were a "9" would hide the very
		disagreement with the part that ConnectorWays exists to name.
	*/
	QString wayOf(Element *element)
	{
		return element ? element->elementInformations()
				 .value(QETInformation::ELMT_LABEL).toString()
			       : QString();
	}

	/**
		Walk to @a element and step out of the way.

		The same move as the two reports next door: whoever double clicked
		wanted to look at the component, and a window left on top of it
		would be half an answer.
	*/
	void goTo(Element *element, QDialog *dialog)
	{
		if (!element || !element->diagram()) {
			return;
		}
		element->diagram()->showMe();
		element->diagram()->clearSelection();
		element->setSelected(true);
		element->ensureVisible();
		dialog->accept();
	}

	/// A table of read only rows, selected whole and several at a time
	QTableWidget *newTable(const QStringList &headers, QWidget *parent)
	{
		QTableWidget *table = new QTableWidget(parent);
		table->setColumnCount(headers.size());
		table->setHorizontalHeaderLabels(headers);
		table->setSelectionBehavior(QAbstractItemView::SelectRows);
		table->setSelectionMode(QAbstractItemView::ExtendedSelection);
		table->setEditTriggers(QAbstractItemView::NoEditTriggers);
		table->verticalHeader()->setVisible(false);
		table->horizontalHeader()->setStretchLastSection(true);
		return table;
	}

	/// The selected rows of @a table, each once, from top to bottom
	QList<int> selectedRows(const QTableWidget *table)
	{
		QList<int> rows;
		const QList<QTableWidgetSelectionRange> ranges = table->selectedRanges();
		for (const QTableWidgetSelectionRange &range : ranges)
		{
			for (int row = range.topRow() ; row <= range.bottomRow() ; ++row)
			{
				if (!rows.contains(row)) {
					rows << row;
				}
			}
		}
		std::sort(rows.begin(), rows.end());
		return rows;
	}

	/// Add the whole of @a row to what @a table has selected
	void selectRow(QTableWidget *table, int row)
	{
		table->setRangeSelected(QTableWidgetSelectionRange(
						row, 0, row, table->columnCount() - 1),
					true);
	}
}

/**
	@brief ConnectorCheck::showReport
	@param project
	@param parent
*/
void ConnectorCheck::showReport(QETProject *project, QWidget *parent)
{
	ReportDialog dialog(project, QETApp::catalog(), parent);
	dialog.exec();
}

/**
	@brief ConnectorCheck::ReportDialog::ReportDialog
	@param project
	@param catalog
	@param parent
*/
ConnectorCheck::ReportDialog::ReportDialog(QETProject *project,
					   Catalog *catalog,
					   QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_catalog(catalog)
{
	setWindowTitle(QObject::tr("Contrôle des connecteurs"));
		//Taller than the two reports next door, because it holds a
		//third table that is worked in rather than only read.
	resize(780, 720);
	buildWidgets();
	refill();
}

/**
	@brief ConnectorCheck::ReportDialog::buildWidgets
*/
void ConnectorCheck::ReportDialog::buildWidgets()
{
	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);

		//Two tables and not one for the check, and it is the whole point
		//of the window: a connector nobody catalogued and a pin belonging
		//to no connector are two problems with two different answers. One
		//table over both would need a column meaning "what kind of trouble
		//is this row", which is a heading admitting it holds two lists.
	QLabel *heading_connectors = new QLabel(
				QObject::tr("Connecteurs dont les voies ne peuvent "
					    "pas être comptées"), this);
	heading_connectors->setWordWrap(true);
	m_connectors = newTable({ QObject::tr("Connecteur"),
				  QObject::tr("Folio"),
				  QObject::tr("Broches dessinées"),
				  QObject::tr("Pièce"),
				  QObject::tr("Problème") }, this);

	QLabel *heading_pins = new QLabel(
				QObject::tr("Broches n'appartenant à aucun connecteur"),
				this);
	heading_pins->setWordWrap(true);
	m_pins = newTable({ QObject::tr("Repère"),
			    QObject::tr("Folio"),
			    QObject::tr("Symbole") }, this);

		//The third table answers no question of the check: it is where a
		//connector is worked on. Every connector the folios draw is
		//offered, counted or not, because swapping two way numbers needs
		//no part - only two pins that say they are of one connector.
	QLabel *heading_ways = new QLabel(QObject::tr("Ways of connector"), this);
	m_way_choice = new QComboBox(this);
	m_way_choice->setObjectName(QStringLiteral("connector_way_choice"));
	m_way_choice->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	m_ways = newTable({ QObject::tr("Way"),
			    QObject::tr("Folio"),
			    QObject::tr("Symbole") }, this);
	m_ways->setObjectName(QStringLiteral("connector_ways"));

		//A refused swap is said here, in the window, and not in a message
		//box: the refusal is news about the two rows still selected right
		//above it, and a box would have to be closed before reading them.
	m_swap_status = new QLabel(this);
	m_swap_status->setObjectName(QStringLiteral("connector_swap_status"));
	m_swap_status->setWordWrap(true);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

		//Both queues close here, like the two reports next door: a report
		//that only says what is wrong sends the person to another window
		//to find again what this one already names.
	m_assign_part = new QPushButton(QObject::tr("Attribuer une pièce…"), this);
	m_assign_part->setToolTip(QObject::tr(
		"Attribue une pièce du catalogue à toutes les broches des "
		"connecteurs sélectionnés : c'est elle qui dit combien de voies "
		"le connecteur a."));
	m_assign_part->setEnabled(false);
	buttons->addButton(m_assign_part, QDialogButtonBox::ActionRole);

	m_assign_connector = new QPushButton(
				QObject::tr("Affecter à un connecteur…"), this);
	m_assign_connector->setToolTip(QObject::tr(
		"Écrit le nom du connecteur sur les broches sélectionnées, en une "
		"seule action annulable."));
	m_assign_connector->setEnabled(false);
	buttons->addButton(m_assign_connector, QDialogButtonBox::ActionRole);

	m_swap = new QPushButton(QObject::tr("Swap ways"), this);
	m_swap->setObjectName(QStringLiteral("connector_swap"));
	m_swap->setToolTip(QObject::tr(
		"Exchanges the way numbers of the two selected pins, or asks for "
		"the second one; Ctrl+Z undoes it."));
	m_swap->setEnabled(false);
	buttons->addButton(m_swap, QDialogButtonBox::ActionRole);

		//activated, not doubleClicked: it fires on Enter too, so every
		//table can be walked without a mouse.
	connect(m_connectors, &QTableWidget::activated, this,
		[this](const QModelIndex &index)
	{
		const int row = index.row();
		if (row < 0 || row >= m_uncountable.size()) {
			return;
		}
			//To the first pin of it: a connector is not drawn
			//anywhere as a thing of its own, so the nearest place to
			//stand is the first way of it in reading order.
		goTo(m_uncountable.at(row).pins.value(0), this);
	});
	connect(m_pins, &QTableWidget::activated, this,
		[this](const QModelIndex &index)
	{
		goTo(m_loose.value(index.row()), this);
	});
	connect(m_ways, &QTableWidget::activated, this,
		[this](const QModelIndex &index)
	{
		goTo(m_way_pins.value(index.row()), this);
	});

	connect(m_connectors, &QTableWidget::itemSelectionChanged,
		this, [this]() { updateButtons(); });
	connect(m_pins, &QTableWidget::itemSelectionChanged,
		this, [this]() { updateButtons(); });
	connect(m_ways, &QTableWidget::itemSelectionChanged,
		this, [this]() { updateButtons(); });
	connect(m_way_choice, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this]()
	{
		m_swap_status->clear();
		fillWays();
	});

	connect(m_assign_part, &QPushButton::clicked,
		this, [this]() { assignPartToSelection(); });
	connect(m_assign_connector, &QPushButton::clicked,
		this, [this]() { assignConnectorToSelection(); });
	connect(m_swap, &QPushButton::clicked,
		this, [this]() { swapSelectedWays(); });

		//The window is modal, so the Undo of the editor behind it cannot
		//be reached while it is open; and a swap made by mistake is undone
		//right away or not at all. The same project stack, so a step
		//undone here is the step the editor would have undone.
	QAction *undo = new QAction(this);
	undo->setObjectName(QStringLiteral("connector_report_undo"));
	undo->setShortcuts(QKeySequence::Undo);
	addAction(undo);
	connect(undo, &QAction::triggered, this, [this]() { undoLast(); });

	QAction *redo = new QAction(this);
	redo->setObjectName(QStringLiteral("connector_report_redo"));
	redo->setShortcuts(QKeySequence::Redo);
	addAction(redo);
	connect(redo, &QAction::triggered, this, [this]() { redoNext(); });

	QHBoxLayout *ways_line = new QHBoxLayout();
	ways_line->addWidget(heading_ways);
	ways_line->addWidget(m_way_choice);
	ways_line->addStretch();

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(m_summary);
	layout->addWidget(heading_connectors);
	layout->addWidget(m_connectors);
	layout->addWidget(heading_pins);
	layout->addWidget(m_pins);
	layout->addLayout(ways_line);
	layout->addWidget(m_ways);
	layout->addWidget(m_swap_status);
	layout->addWidget(buttons);
}

/**
	@brief ConnectorCheck::ReportDialog::refill
	Read the project again and write every table from that one reading.
*/
void ConnectorCheck::ReportDialog::refill()
{
		//No catalogue reads as a closed one, and not as an empty project:
		//which pins say they are of which connector is written on the
		//folio, and the third table needs nothing else.
	const Catalog none;
	const Catalog *asked = m_catalog ? m_catalog : &none;
	m_current = ConnectorCheck::report(m_project, *asked);
	m_uncountable = m_current.uncountable();
	m_loose = m_current.pins_without_connector;

		//Four states, because they are four different pieces of news, and
		//the wording must not change under the reader's eyes while nothing
		//but a count did.
	QStringList sentences;
	if (!m_current.catalog_read)
	{
		sentences << QObject::tr(
			"Le catalogue n'a pas répondu : impossible de dire "
			"quels connecteurs ont une pièce, ni quelles broches "
			"en sont.");
	}
	else if (m_current.connectors.isEmpty() && m_loose.isEmpty())
	{
		sentences << QObject::tr(
			"Ce projet ne dessine aucun connecteur : il n'y a rien "
			"à contrôler ici.");
	}
	else if (m_uncountable.isEmpty() && m_loose.isEmpty())
	{
		sentences << QObject::tr(
			"Les %n connecteur(s) du projet ont une pièce avec "
			"son brochage, et aucune broche n'est hors connecteur.",
			"", m_current.connectors.size());
			//Said only here, where every connector is counted: a
			//total of voies libres taken while some connector has no
			//pinout would be a sum with a hole in it.
		sentences << QObject::tr(
			"%n voie(s) de réserve au total.",
			"", m_current.reserveWays());
	}
	else
	{
		sentences << QObject::tr(
			"%1 connecteur(s) sur %2 n'ont pas de pièce exploitable, "
			"et %3 broche(s) n'appartiennent à aucun connecteur. "
			"Double-cliquez une ligne pour aller au composant.")
			     .arg(m_uncountable.size())
			     .arg(m_current.connectors.size())
			     .arg(m_loose.size());
	}

		//Counted apart and only when there is something to count: those
		//connectors do have a pinout, so they are not rows, but a voie
		//drawn that the product does not have is a mistake on one side or
		//the other.
	if (m_current.mismatchedConnectors())
	{
		sentences << QObject::tr(
			"%n connecteur(s) dessinent une voie que leur pièce "
			"ne déclare pas.",
			"", m_current.mismatchedConnectors());
	}

	m_summary->setText(sentences.join(QLatin1Char(' ')));

		//A selection kept over a rewrite would point at whatever row now
		//sits where the chosen one was - the very coming apart of a row
		//and its element this window is built to avoid.
	m_connectors->clearSelection();
	m_connectors->setRowCount(m_uncountable.size());
	for (int row = 0 ; row < m_uncountable.size() ; ++row)
	{
		const DrawnConnector &entry = m_uncountable.at(row);

			//The count of what is drawn, and never the count of voies:
			//on every row of this table the pinout is unknown, and
			//printing what a count reads then would put "-1" on the
			//screen.
		m_connectors->setItem(row, 0, new QTableWidgetItem(entry.name));
		m_connectors->setItem(row, 1, new QTableWidgetItem(
					      entry.folio ? QString::number(entry.folio)
							  : QString()));
		m_connectors->setItem(row, 2, new QTableWidgetItem(
					      QString::number(entry.drawn_count)));
		m_connectors->setItem(row, 3, new QTableWidgetItem(entry.part_code));
		m_connectors->setItem(row, 4, new QTableWidgetItem(entry.describe()));
	}
	m_connectors->resizeColumnsToContents();

	m_pins->clearSelection();
	m_pins->setRowCount(m_loose.size());
	for (int row = 0 ; row < m_loose.size() ; ++row)
	{
		Element *element = m_loose.at(row);
		m_pins->setItem(row, 0, new QTableWidgetItem(tagOf(element)));
		m_pins->setItem(row, 1, new QTableWidgetItem(folioOf(element)));
		m_pins->setItem(row, 2, new QTableWidgetItem(
					element ? element->name() : QString()));
	}
	m_pins->resizeColumnsToContents();

		//The third table stays on the connector it was showing, found
		//again by key: after a swap the person is looking at those two
		//rows, and a list jumping back to the first connector would make
		//them look for what they just did.
	const QString shown = m_way_choice->currentData().toString();
	{
		const QSignalBlocker blocker(m_way_choice);
		m_way_choice->clear();
		const QList<DrawnConnector> &connectors = m_current.connectors;
		for (const DrawnConnector &connector : connectors) {
			m_way_choice->addItem(connector.name, connector.key);
		}
		const int kept = m_way_choice->findData(shown);
		m_way_choice->setCurrentIndex(kept >= 0 ? kept : 0);
	}
	m_way_choice->setEnabled(m_way_choice->count() > 0);

	m_swap_status->clear();
	fillWays();
}

/**
	@brief ConnectorCheck::ReportDialog::fillWays
	Write the third table from the connector the choice above it names.

	The rows selected are kept when the connector is the same one, and only
	then. The rows are the pins in reading order, which no way number takes
	part in, so after a swap or its undo the same row still holds the same
	pin, and keeping the two selected is what lets the person see what
	moved - and swap them back by pressing the button again.
*/
void ConnectorCheck::ReportDialog::fillWays()
{
	const QString key = m_way_choice->currentData().toString();
	const QList<int> kept = key == m_ways_key ? selectedRows(m_ways)
						  : QList<int>();
	m_ways_key = key;

	m_way_pins.clear();
	const QList<DrawnConnector> &connectors = m_current.connectors;
	for (const DrawnConnector &connector : connectors)
	{
		if (!key.isEmpty() && connector.key == key)
		{
			m_way_pins = connector.pins;
			break;
		}
	}

	{
		const QSignalBlocker blocker(m_ways);
		m_ways->clearSelection();
		m_ways->setRowCount(m_way_pins.size());
		for (int row = 0 ; row < m_way_pins.size() ; ++row)
		{
			Element *element = m_way_pins.at(row);
			m_ways->setItem(row, 0, new QTableWidgetItem(wayOf(element)));
			m_ways->setItem(row, 1, new QTableWidgetItem(folioOf(element)));
			m_ways->setItem(row, 2, new QTableWidgetItem(
						element ? element->name() : QString()));
		}
		for (int row : kept)
		{
			if (row < m_way_pins.size()) {
				selectRow(m_ways, row);
			}
		}
	}
	m_ways->resizeColumnsToContents();
	updateButtons();
}

/**
	@brief ConnectorCheck::ReportDialog::updateButtons
	Light each button from what its own table has selected.
*/
void ConnectorCheck::ReportDialog::updateButtons()
{
	m_assign_part->setEnabled(!m_connectors->selectedItems().isEmpty());
	m_assign_connector->setEnabled(!m_pins->selectedItems().isEmpty());

		//One way or two, and nothing else. Three selected would leave the
		//button to guess which two were meant, and a guess that writes
		//way numbers is a crimping error waiting on the bench.
	const int selected = selectedRows(m_ways).size();
	m_swap->setEnabled(m_way_pins.size() >= 2
			   && (selected == 1 || selected == 2));
}

/**
	@brief ConnectorCheck::ReportDialog::assignPartToSelection
	Assign a catalogue part to every pin of the connectors selected.
*/
void ConnectorCheck::ReportDialog::assignPartToSelection()
{
	if (!m_catalog || !m_catalog->isOpen()) {
		return;
	}

		//Every pin of every connector picked, because the pinout belongs
		//to the connector and not to one of its ways: a part written on
		//three pins out of nine would make the other six a second
		//connector as far as the count goes.
	QList<Element *> chosen;
	const QList<int> rows = selectedRows(m_connectors);
	for (int row : rows)
	{
		if (row >= 0 && row < m_uncountable.size()) {
			chosen << m_uncountable.at(row).pins;
		}
	}
	if (chosen.isEmpty()) {
		return;
	}

	const CatalogPart part = CatalogBrowserDialog::choosePart(
				m_catalog, this,
				CatalogProjectActions::partFromElements(*m_catalog, chosen));
	if (part.isNull()) {
		return;
	}
	if (!CatalogProjectActions::assignPart(chosen, *m_catalog, part)) {
		return;
	}
	refill();
}

/**
	@brief ConnectorCheck::ReportDialog::assignConnectorToSelection
	Write a connector name on the loose pins selected.
*/
void ConnectorCheck::ReportDialog::assignConnectorToSelection()
{
	QList<Element *> chosen;
	const QList<int> rows = selectedRows(m_pins);
	for (int row : rows)
	{
		if (row >= 0 && row < m_loose.size()) {
			chosen << m_loose.at(row);
		}
	}
	if (chosen.isEmpty()) {
		return;
	}

		//Offered as a list of the connectors the project already draws,
		//and typable all the same: putting a way into XS1 has to be a pick
		//and not a spelling exercise, and the first connector of a project
		//has to be creatable without one existing yet.
	QStringList known;
	const QList<DrawnConnector> &connectors = m_current.connectors;
	for (const DrawnConnector &connector : connectors) {
		known << connector.name;
	}

	bool picked = false;
	const QString question = QObject::tr(
		"À quel connecteur ces %n broche(s) appartiennent-elles ?",
		"", chosen.size());
	const QString title = QObject::tr("Affecter à un connecteur");
	const QString name = known.isEmpty()
			     ? QInputDialog::getText(this, title, question,
						     QLineEdit::Normal,
						     QString(), &picked)
			     : QInputDialog::getItem(this, title, question,
						     known, 0, true, &picked);
	if (!picked) {
		return;
	}
	if (!ConnectorCheck::assignConnector(chosen, name)) {
		return;
	}
	refill();
}

/**
	@brief ConnectorCheck::ReportDialog::swapSelectedWays
	Swap the two ways selected, or ask for the second when one only is.
*/
void ConnectorCheck::ReportDialog::swapSelectedWays()
{
	const QList<int> rows = selectedRows(m_ways);
	if (rows.size() == 2)
	{
		swapWays(rows.at(0), rows.at(1));
		return;
	}
	if (rows.size() != 1) {
		return;
	}

		//One way chosen: the other is picked among the rest of the same
		//connector, which is what keeps the pair inside it - the swap
		//across two connectors is refused anyway, and offering it would
		//be offering a refusal.
	const int first = rows.first();
	QStringList choices;
	QList<int> rows_of_choices;
	for (int row = 0 ; row < m_way_pins.size() ; ++row)
	{
		if (row == first) {
			continue;
		}
		Element *element = m_way_pins.at(row);
		choices << QObject::tr("%1 (folio %2)")
			   .arg(tagOf(element), folioOf(element));
		rows_of_choices << row;
	}
	if (choices.isEmpty()) {
		return;
	}

	bool picked = false;
	const QString choice = QInputDialog::getItem(
				       this, QObject::tr("Swap ways"),
				       QObject::tr("Swap way %1 with which way?")
				       .arg(tagOf(m_way_pins.value(first))),
				       choices, 0, false, &picked);
	if (!picked) {
		return;
	}
	const int index = choices.indexOf(choice);
	if (index < 0) {
		return;
	}
	swapWays(first, rows_of_choices.at(index));
}

/**
	@brief ConnectorCheck::ReportDialog::swapWays
	@param first_row
	@param second_row
	@return why nothing was written, ConnectorSwap::Refusal::None otherwise

	ConnectorCheck::swapPins does the swap - the rule, the one step on the
	stack, the folio following - and nothing of it is repeated here. What
	this adds is the window: the sentence when it refuses, and the tables
	read again when it does not.
*/
ConnectorSwap::Refusal ConnectorCheck::ReportDialog::swapWays(int first_row,
							      int second_row)
{
	const ConnectorSwap::Refusal answer = ConnectorCheck::swapPins(
						      m_way_pins.value(first_row),
						      m_way_pins.value(second_row));
	if (answer != ConnectorSwap::Refusal::None)
	{
			//Nothing was written, so nothing is read again: the two
			//rows stay selected under the sentence that says why.
		m_swap_status->setText(QObject::tr("Not swapped: %1.")
				       .arg(ConnectorSwap::Plan::describe(answer)));
		return answer;
	}
	refill();
	return answer;
}

/**
	@brief ConnectorCheck::ReportDialog::undoLast
	Undo the last step of the project, and read it again.
*/
void ConnectorCheck::ReportDialog::undoLast()
{
	QUndoStack *stack = m_project ? m_project->undoStack() : nullptr;
	if (!stack || !stack->canUndo()) {
		return;
	}
	stack->undo();
	refill();
}

/**
	@brief ConnectorCheck::ReportDialog::redoNext
	Redo the step of the project undone last, and read it again.
*/
void ConnectorCheck::ReportDialog::redoNext()
{
	QUndoStack *stack = m_project ? m_project->undoStack() : nullptr;
	if (!stack || !stack->canRedo()) {
		return;
	}
	stack->redo();
	refill();
}
