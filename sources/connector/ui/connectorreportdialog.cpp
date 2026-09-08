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

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
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
}

/**
	@brief ConnectorCheck::showReport
	@param project
	@param parent
*/
void ConnectorCheck::showReport(QETProject *project, QWidget *parent)
{
	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Contrôle des connecteurs"));
	dialog.resize(780, 560);

	QLabel *summary = new QLabel(&dialog);
	summary->setWordWrap(true);

		//Two tables and not one, and it is the whole point of the window:
		//a connector nobody catalogued and a pin belonging to no
		//connector are two problems with two different answers. One table
		//over both would need a column meaning "what kind of trouble is
		//this row", which is a heading admitting it holds two lists.
	QLabel *heading_connectors = new QLabel(
				QObject::tr("Connecteurs dont les voies ne peuvent "
					    "pas être comptées"), &dialog);
	heading_connectors->setWordWrap(true);
	QTableWidget *connectors = new QTableWidget(&dialog);
	connectors->setColumnCount(5);
	connectors->setHorizontalHeaderLabels({ QObject::tr("Connecteur"),
						QObject::tr("Folio"),
						QObject::tr("Broches dessinées"),
						QObject::tr("Pièce"),
						QObject::tr("Problème") });
	connectors->setSelectionBehavior(QAbstractItemView::SelectRows);
	connectors->setEditTriggers(QAbstractItemView::NoEditTriggers);
	connectors->verticalHeader()->setVisible(false);
	connectors->horizontalHeader()->setStretchLastSection(true);

	QLabel *heading_pins = new QLabel(
				QObject::tr("Broches n'appartenant à aucun connecteur"),
				&dialog);
	heading_pins->setWordWrap(true);
	QTableWidget *pins = new QTableWidget(&dialog);
	pins->setColumnCount(3);
	pins->setHorizontalHeaderLabels({ QObject::tr("Repère"),
					  QObject::tr("Folio"),
					  QObject::tr("Symbole") });
	pins->setSelectionBehavior(QAbstractItemView::SelectRows);
	pins->setEditTriggers(QAbstractItemView::NoEditTriggers);
	pins->verticalHeader()->setVisible(false);
	pins->horizontalHeader()->setStretchLastSection(true);

		//What the two tables are showing, kept beside them and rewritten
		//whole on every reading. Nothing is ever removed from either of
		//them one row at a time: that is what let a table and its list
		//come apart in the report next door, and it cannot happen where
		//both are written from the same read.
	ConnectorCheck::Report current;
	QList<DrawnConnector> uncountable;
	QList<Element *> loose;

	auto refill = [&]()
	{
		Catalog *catalog = QETApp::catalog();
		current = catalog ? ConnectorCheck::report(project, *catalog)
				  : ConnectorCheck::Report();
		uncountable = current.uncountable();
		loose = current.pins_without_connector;

			//Four states, because they are four different pieces of
			//news, and the wording must not change under the reader's
			//eyes while nothing but a count did.
		QStringList sentences;
		if (!current.catalog_read)
		{
			sentences << QObject::tr(
				"Le catalogue n'a pas répondu : impossible de dire "
				"quels connecteurs ont une pièce, ni quelles broches "
				"en sont.");
		}
		else if (current.connectors.isEmpty() && loose.isEmpty())
		{
			sentences << QObject::tr(
				"Ce projet ne dessine aucun connecteur : il n'y a rien "
				"à contrôler ici.");
		}
		else if (uncountable.isEmpty() && loose.isEmpty())
		{
			sentences << QObject::tr(
				"Les %n connecteur(s) du projet ont une pièce avec "
				"son brochage, et aucune broche n'est hors connecteur.",
				"", current.connectors.size());
				//Said only here, where every connector is counted:
				//a total of voies libres taken while some connector
				//has no pinout would be a sum with a hole in it.
			sentences << QObject::tr(
				"%n voie(s) de réserve au total.",
				"", current.reserveWays());
		}
		else
		{
			sentences << QObject::tr(
				"%1 connecteur(s) sur %2 n'ont pas de pièce exploitable, "
				"et %3 broche(s) n'appartiennent à aucun connecteur. "
				"Double-cliquez une ligne pour aller au composant.")
				     .arg(uncountable.size())
				     .arg(current.connectors.size())
				     .arg(loose.size());
		}

			//Counted apart and only when there is something to
			//count: those connectors do have a pinout, so they are
			//not rows, but a voie drawn that the product does not
			//have is a mistake on one side or the other.
		if (current.mismatchedConnectors())
		{
			sentences << QObject::tr(
				"%n connecteur(s) dessinent une voie que leur pièce "
				"ne déclare pas.",
				"", current.mismatchedConnectors());
		}

		summary->setText(sentences.join(QLatin1Char(' ')));

		connectors->setRowCount(uncountable.size());
		for (int row = 0 ; row < uncountable.size() ; ++row)
		{
			const DrawnConnector &entry = uncountable.at(row);

				//The count of what is drawn, and never the count of
				//voies: on every row of this table the pinout is
				//unknown, and printing what a count reads then would
				//put "-1" on the screen.
			connectors->setItem(row, 0, new QTableWidgetItem(entry.name));
			connectors->setItem(row, 1, new QTableWidgetItem(
						    entry.folio ? QString::number(entry.folio)
								: QString()));
			connectors->setItem(row, 2, new QTableWidgetItem(
						    QString::number(entry.drawn_count)));
			connectors->setItem(row, 3, new QTableWidgetItem(entry.part_code));
			connectors->setItem(row, 4, new QTableWidgetItem(entry.describe()));
		}
		connectors->resizeColumnsToContents();

		pins->setRowCount(loose.size());
		for (int row = 0 ; row < loose.size() ; ++row)
		{
			Element *element = loose.at(row);
			pins->setItem(row, 0, new QTableWidgetItem(tagOf(element)));
			pins->setItem(row, 1, new QTableWidgetItem(folioOf(element)));
			pins->setItem(row, 2, new QTableWidgetItem(
					      element ? element->name() : QString()));
		}
		pins->resizeColumnsToContents();
	};
	refill();

		//activated, not doubleClicked: it fires on Enter too, so either
		//table can be walked without a mouse.
	QObject::connect(connectors, &QTableWidget::activated, connectors,
			 [&uncountable, &dialog](const QModelIndex &index)
	{
		const int row = index.row();
		if (row < 0 || row >= uncountable.size()) {
			return;
		}
			//To the first pin of it: a connector is not drawn
			//anywhere as a thing of its own, so the nearest place to
			//stand is the first way of it in reading order.
		goTo(uncountable.at(row).pins.value(0), &dialog);
	});

	QObject::connect(pins, &QTableWidget::activated, pins,
			 [&loose, &dialog](const QModelIndex &index)
	{
		const int row = index.row();
		if (row < 0 || row >= loose.size()) {
			return;
		}
		goTo(loose.at(row), &dialog);
	});

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);

		//Both queues close here, like the two reports next door: a report
		//that only says what is wrong sends the person to another window
		//to find again what this one already names.
	QPushButton *assign_part = new QPushButton(
				QObject::tr("Attribuer une pièce…"), &dialog);
	assign_part->setToolTip(QObject::tr(
		"Attribue une pièce du catalogue à toutes les broches des "
		"connecteurs sélectionnés : c'est elle qui dit combien de voies "
		"le connecteur a."));
	assign_part->setEnabled(false);
	buttons->addButton(assign_part, QDialogButtonBox::ActionRole);

	QPushButton *assign_connector = new QPushButton(
				QObject::tr("Affecter à un connecteur…"), &dialog);
	assign_connector->setToolTip(QObject::tr(
		"Écrit le nom du connecteur sur les broches sélectionnées, en une "
		"seule action annulable."));
	assign_connector->setEnabled(false);
	buttons->addButton(assign_connector, QDialogButtonBox::ActionRole);

	QObject::connect(connectors, &QTableWidget::itemSelectionChanged, assign_part,
			 [connectors, assign_part]()
	{
		assign_part->setEnabled(!connectors->selectedItems().isEmpty());
	});
	QObject::connect(pins, &QTableWidget::itemSelectionChanged, assign_connector,
			 [pins, assign_connector]()
	{
		assign_connector->setEnabled(!pins->selectedItems().isEmpty());
	});

	QObject::connect(assign_part, &QPushButton::clicked, &dialog,
			 [&dialog, connectors, &uncountable, refill]()
	{
		Catalog *catalog = QETApp::catalog();
		if (!catalog || !catalog->isOpen()) {
			return;
		}

			//Every pin of every connector picked, because the pinout
			//belongs to the connector and not to one of its ways: a
			//part written on three pins out of nine would make the
			//other six a second connector as far as the count goes.
		QList<Element *> chosen;
		const QList<QTableWidgetSelectionRange> ranges = connectors->selectedRanges();
		for (const QTableWidgetSelectionRange &range : ranges)
		{
			for (int row = range.topRow() ; row <= range.bottomRow() ; ++row)
			{
				if (row >= 0 && row < uncountable.size()) {
					chosen << uncountable.at(row).pins;
				}
			}
		}
		if (chosen.isEmpty()) {
			return;
		}

		const CatalogPart part = CatalogBrowserDialog::choosePart(
					catalog, &dialog,
					CatalogProjectActions::partFromElements(*catalog, chosen));
		if (part.isNull()) {
			return;
		}
		if (!CatalogProjectActions::assignPart(chosen, *catalog, part)) {
			return;
		}
		refill();
	});

	QObject::connect(assign_connector, &QPushButton::clicked, &dialog,
			 [&dialog, pins, &loose, &current, refill]()
	{
		QList<Element *> chosen;
		const QList<QTableWidgetSelectionRange> ranges = pins->selectedRanges();
		for (const QTableWidgetSelectionRange &range : ranges)
		{
			for (int row = range.topRow() ; row <= range.bottomRow() ; ++row)
			{
				if (row >= 0 && row < loose.size()) {
					chosen << loose.at(row);
				}
			}
		}
		if (chosen.isEmpty()) {
			return;
		}

			//Offered as a list of the connectors the project already
			//draws, and typable all the same: putting a way into XS1
			//has to be a pick and not a spelling exercise, and the
			//first connector of a project has to be creatable without
			//one existing yet.
		QStringList known;
		for (const DrawnConnector &connector : current.connectors) {
			known << connector.name;
		}

		bool picked = false;
		const QString question = QObject::tr(
			"À quel connecteur ces %n broche(s) appartiennent-elles ?",
			"", chosen.size());
		const QString title = QObject::tr("Affecter à un connecteur");
		const QString name = known.isEmpty()
				     ? QInputDialog::getText(&dialog, title, question,
							     QLineEdit::Normal,
							     QString(), &picked)
				     : QInputDialog::getItem(&dialog, title, question,
							     known, 0, true, &picked);
		if (!picked) {
			return;
		}
		if (!ConnectorCheck::assignConnector(chosen, name)) {
			return;
		}
		refill();
	});

	QVBoxLayout *layout = new QVBoxLayout(&dialog);
	layout->addWidget(summary);
	layout->addWidget(heading_connectors);
	layout->addWidget(connectors);
	layout->addWidget(heading_pins);
	layout->addWidget(pins);
	layout->addWidget(buttons);

	dialog.exec();
}
