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
#include "ioassigndialog.h"

#include "../../diagram.h"
#include "../../elementprovider.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetproject.h"
#include "../../undocommand/assigniopointscommand.h"
#include "../ioassignment.h"
#include "../iolist.h"
#include "../iopoint.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUndoStack>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
	/// Role holding the id of the point a row of the left table is about.
	const int ID_ROLE = Qt::UserRole + 1;
	/// Role holding the row of the card table a row of the right table is about.
	const int INDEX_ROLE = Qt::UserRole + 2;
}

/**
	@brief IoAssignDialog::IoAssignDialog
	@param project the project whose points and cards are shown
	@param parent
*/
IoAssignDialog::IoAssignDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Affecter des entrées / sorties à une carte"));
	buildWidgets();
	reloadCards();
	cardChanged();
}

/**
	@brief IoAssignDialog::report
	@return what the last button press did
*/
QString IoAssignDialog::report() const
{
	return m_report;
}

/**
	@brief IoAssignDialog::buildWidgets
*/
void IoAssignDialog::buildWidgets()
{
	m_card = new QComboBox(this);
	connect(m_card, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &IoAssignDialog::cardChanged);

	QHBoxLayout *card_line = new QHBoxLayout();
	card_line->addWidget(new QLabel(tr("Carte :"), this));
	card_line->addWidget(m_card, 1);

	m_points = new QTableWidget(this);
	m_points->setColumnCount(4);
	m_points->setHorizontalHeaderLabels(QStringList()
					    << tr("Point") << tr("Description")
					    << tr("Type") << tr("Adresse"));
	m_points->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_points->setSelectionMode(QAbstractItemView::NoSelection);
	m_points->setAlternatingRowColors(true);
	m_points->verticalHeader()->hide();
	m_points->horizontalHeader()->setStretchLastSection(true);
	connect(m_points, &QTableWidget::itemChanged,
		this, &IoAssignDialog::selectionChanged);

	m_channels = new QTableWidget(this);
	m_channels->setColumnCount(4);
	m_channels->setHorizontalHeaderLabels(QStringList()
					      << tr("Voie") << tr("Type")
					      << tr("Fonction") << tr("Point"));
	m_channels->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_channels->setSelectionMode(QAbstractItemView::NoSelection);
	m_channels->setAlternatingRowColors(true);
	m_channels->verticalHeader()->hide();
	m_channels->horizontalHeader()->setStretchLastSection(true);
	connect(m_channels, &QTableWidget::itemChanged,
		this, &IoAssignDialog::selectionChanged);

	QGroupBox *points_box = new QGroupBox(tr("Points d'E/S encore libres"), this);
	QVBoxLayout *points_layout = new QVBoxLayout(points_box);
	points_layout->addWidget(m_points);

	QGroupBox *channels_box = new QGroupBox(tr("Voies de la carte"), this);
	QVBoxLayout *channels_layout = new QVBoxLayout(channels_box);
	channels_layout->addWidget(m_channels);

	QHBoxLayout *tables = new QHBoxLayout();
	tables->addWidget(points_box, 1);
	tables->addWidget(channels_box, 1);

		//What the buttons would do, worked out against a copy of the list
		//and thrown away. It is the whole point of the dialogue: an
		//affectation that runs first and explains afterwards is one nobody
		//trusts a second time.
	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	m_summary->setTextFormat(Qt::PlainText);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	m_status->setTextFormat(Qt::PlainText);

	m_assign = new QPushButton(tr("Affecter"), this);
	m_assign->setToolTip(tr("Écrit les points cochés dans les voies libres "
				"de la carte."));
	connect(m_assign, &QPushButton::clicked, this, &IoAssignDialog::assign);

	m_release = new QPushButton(tr("Libérer"), this);
	m_release->setToolTip(tr("Rend les voies cochées, et remet leurs points "
				 "dans la liste."));
	connect(m_release, &QPushButton::clicked, this, &IoAssignDialog::release);

		//ActionRole and not AcceptRole: the window stays open, because
		//placing the points of a project means going through several cards
		//one after the other, and closing after each one would make that
		//four windows instead of one.
	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	buttons->addButton(m_assign, QDialogButtonBox::ActionRole);
	buttons->addButton(m_release, QDialogButtonBox::ActionRole);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(card_line);
	layout->addLayout(tables, 1);
	layout->addWidget(m_summary);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	resize(880, 520);
}

/**
	@brief IoAssignDialog::reloadCards
	Fill the combo with the PLC masters of the project.

	A master whose table is empty is left out: it has no channel to give, and
	offering it would only produce a refusal. A full one is kept - full means
	its slave limit is reached, which is about linking, and writing a
	function text into a free row is not linking.
*/
void IoAssignDialog::reloadCards()
{
	m_loading = true;
	m_card->clear();
	m_cards.clear();

	if (m_project)
	{
		ElementProvider ep(m_project);
		const QVector<QPointer<Element>> masters = ep.find(ElementData::Master);

		for (Element *elmt : masters)
		{
			if (!elmt) {
				continue;
			}
			if (elmt->elementData().m_master_type != ElementData::PLC) {
				continue;
			}
			if (elmt->elementData().plcMasterData().ios.isEmpty()) {
				continue;
			}

			QString label = elmt->actualLabel();
			if (label.isEmpty()) {
				label = elmt->name();
			}

			QString folio;
			if (elmt->diagram()) {
				folio = QString::number(elmt->diagram()->folioIndex() + 1);
			}

			m_cards << QPointer<Element>(elmt);
			m_card->addItem(folio.isEmpty()
					? label
					: tr("%1 (folio %2)").arg(label, folio));
		}
	}

	m_loading = false;
}

/**
	@brief IoAssignDialog::reloadPoints
	Fill the left table with the points no card has taken.
*/
void IoAssignDialog::reloadPoints()
{
	m_loading = true;
	m_points->clearContents();
	m_points->setRowCount(0);

	if (m_project)
	{
		const IoList list = m_project->ioList();
		const QList<int> free_points = list.unassigned();
		m_points->setRowCount(int(free_points.count()));

		for (int row = 0 ; row < free_points.count() ; ++row)
		{
			const IoPoint &point = list.at(free_points.at(row));

			QTableWidgetItem *name = new QTableWidgetItem(
					IoAssignment::pointLabel(point));
			name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
			name->setCheckState(Qt::Unchecked);
			name->setData(ID_ROLE, point.id);
			m_points->setItem(row, 0, name);

			m_points->setItem(row, 1,
					  new QTableWidgetItem(point.description));
			m_points->setItem(row, 2, new QTableWidgetItem(
					ElementData::translatedPlcIOType(point.type)));
			m_points->setItem(row, 3, new QTableWidgetItem(point.address));
		}
		m_points->resizeColumnsToContents();
	}

	m_loading = false;
}

/**
	@brief IoAssignDialog::reloadChannels
	Fill the right table with the card itself, row by row.

	A row can be given back only when a point of the list holds it and
	nothing is wired to it. The wired ones are said to be wired, in the
	column that says who holds the row, so that a tick that is not there has
	a reason a person can read.
*/
void IoAssignDialog::reloadChannels()
{
	m_loading = true;
	m_channels->clearContents();
	m_channels->setRowCount(0);

	Element *master = currentCard();
	if (!master || !m_project)
	{
		m_loading = false;
		return;
	}

	const ElementData::PlcMasterData plc =
			master->elementData().plcMasterData();
	const QString uuid = master->uuid().toString();
	const QList<int> wired = wiredChannels(master);
	const IoList list = m_project->ioList();

	m_channels->setRowCount(int(plc.ios.count()));

	for (int row = 0 ; row < plc.ios.count() ; ++row)
	{
		const ElementData::PlcIO &io = plc.ios.at(row);

		QTableWidgetItem *name = new QTableWidgetItem(
				IoAssignment::channelName(plc.ios, row));
		name->setFlags(Qt::ItemIsEnabled);
		name->setData(INDEX_ROLE, row);
		m_channels->setItem(row, 0, name);

		m_channels->setItem(row, 1, new QTableWidgetItem(
				ElementData::translatedPlcIOType(io.type)));
		m_channels->setItem(row, 2, new QTableWidgetItem(io.functionText));

		QString holder;
		for (int i = 0 ; i < list.count() ; ++i)
		{
			const IoPoint &point = list.at(i);
			if (point.master_uuid == uuid && point.io_index == row)
			{
				holder = IoAssignment::pointLabel(point);
				break;
			}
		}

		const bool is_wired = wired.contains(row);
		if (is_wired) {
			holder = holder.isEmpty()
					? tr("câblée")
					: tr("%1 — câblée").arg(holder);
		}
		m_channels->setItem(row, 3, new QTableWidgetItem(holder));

		if (!holder.isEmpty() && !is_wired)
		{
			name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
			name->setCheckState(Qt::Unchecked);
		}

		if (is_wired)
		{
			const QBrush grey(Qt::gray);
			for (int column = 0 ; column < 4 ; ++column)
			{
				if (QTableWidgetItem *item = m_channels->item(row, column)) {
					item->setForeground(grey);
				}
			}
		}
	}

	m_channels->resizeColumnsToContents();
	m_loading = false;
}

/**
	@brief IoAssignDialog::reloadSummary
	Say what pressing Affecter would do, before it is pressed.
*/
void IoAssignDialog::reloadSummary()
{
	if (!m_summary) {
		return;
	}

	Element *master = currentCard();
	if (!master || !m_project)
	{
		m_summary->setText(tr("Aucune carte d'automate dans ce projet. "
				      "Une carte est un élément maître de type "
				      "automate dont le tableau de voies n'est "
				      "pas vide."));
		return;
	}

	const QStringList ids = checkedPoints();
	const ElementData::PlcMasterData plc =
			master->elementData().plcMasterData();
	const QString uuid = master->uuid().toString();
	const QList<int> wired = wiredChannels(master);

	if (ids.isEmpty())
	{
		const int free_count = int(IoAssignment::freeChannels(
				plc.ios, m_project->ioList(), uuid, wired).count());

		if (m_points->rowCount() == 0) {
			m_summary->setText(tr("Tous les points d'E/S du projet sont "
					      "déjà affectés. Cette carte a %1 "
					      "voie(s) libre(s).").arg(free_count));
		} else {
			m_summary->setText(tr("%1 voie(s) libre(s) sur cette carte. "
					      "Cocher à gauche les points à y "
					      "placer.").arg(free_count));
		}
		return;
	}

	const IoAssignment::Plan plan = IoAssignment::plan(m_project->ioList(), ids,
							   plc.ios, uuid, wired);
	m_summary->setText(plan.text());
}

/**
	@brief IoAssignDialog::updateEnabledState
*/
void IoAssignDialog::updateEnabledState()
{
	const bool has_card = currentCard() != nullptr;

	if (m_assign) {
		m_assign->setEnabled(has_card && !checkedPoints().isEmpty());
	}
	if (m_release) {
		m_release->setEnabled(has_card && !checkedChannels().isEmpty());
	}
}

/**
	@brief IoAssignDialog::say
	@param message
	@param problem true to say it in red
*/
void IoAssignDialog::say(const QString &message, bool problem)
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

/**
	@brief IoAssignDialog::currentCard
	@return the card chosen in the combo
*/
Element *IoAssignDialog::currentCard() const
{
	const int index = m_card ? m_card->currentIndex() : -1;
	if (index < 0 || index >= m_cards.count()) {
		return nullptr;
	}
	return m_cards.at(index).data();
}

/**
	@brief IoAssignDialog::wiredChannels
	@param master
	@return the rows of that card a drawn element is already reading

	Nothing in the card table records the link: LinkElementCommand reads the
	row and pushes its values onto the slave. So this is the one thing the
	pure model cannot work out for itself, and it is handed to it.
*/
QList<int> IoAssignDialog::wiredChannels(Element *master) const
{
	QList<int> wired;
	if (!master) {
		return wired;
	}

	for (Element *linked : master->linkedElements())
	{
		if (!linked) {
			continue;
		}
		const int index = master->groupIndexForElement(linked);
		if (index >= 0 && !wired.contains(index)) {
			wired << index;
		}
	}
	return wired;
}

/**
	@brief IoAssignDialog::checkedPoints
	@return the ids of the points ticked on the left, in the order shown
*/
QStringList IoAssignDialog::checkedPoints() const
{
	QStringList ids;
	if (!m_points) {
		return ids;
	}

	for (int row = 0 ; row < m_points->rowCount() ; ++row)
	{
		QTableWidgetItem *item = m_points->item(row, 0);
		if (item && item->checkState() == Qt::Checked) {
			ids << item->data(ID_ROLE).toString();
		}
	}
	return ids;
}

/**
	@brief IoAssignDialog::checkedChannels
	@return the rows ticked on the right
*/
QList<int> IoAssignDialog::checkedChannels() const
{
	QList<int> rows;
	if (!m_channels) {
		return rows;
	}

	for (int row = 0 ; row < m_channels->rowCount() ; ++row)
	{
		QTableWidgetItem *item = m_channels->item(row, 0);
		if (item
		    && (item->flags() & Qt::ItemIsUserCheckable)
		    && item->checkState() == Qt::Checked) {
			rows << item->data(INDEX_ROLE).toInt();
		}
	}
	return rows;
}

/**
	@brief IoAssignDialog::cardChanged
*/
void IoAssignDialog::cardChanged()
{
	if (m_loading) {
		return;
	}

	reloadPoints();
	reloadChannels();
	reloadSummary();
	updateEnabledState();
}

/**
	@brief IoAssignDialog::selectionChanged
*/
void IoAssignDialog::selectionChanged()
{
	if (m_loading) {
		return;
	}

	reloadSummary();
	updateEnabledState();
}

/**
	@brief IoAssignDialog::assign
	Write the ticked points into the free voies of the chosen card.
*/
void IoAssignDialog::assign()
{
	Element *master = currentCard();
	if (!master || !m_project) {
		return;
	}

	const QStringList ids = checkedPoints();
	if (ids.isEmpty()) {
		return;
	}

	ElementData data = master->elementData();
	ElementData::PlcMasterData plc = data.plcMasterData();
	const QString uuid = master->uuid().toString();

	const IoAssignment::Plan plan = IoAssignment::plan(m_project->ioList(), ids,
							   plc.ios, uuid,
							   wiredChannels(master));

	IoList list = m_project->ioList();
	const int done = IoAssignment::apply(plan, list, plc.ios, uuid);

		//Nothing went in, so nothing is pushed: an undo step that undoes
		//nothing is worse than no step at all. The paragraph already says
		//why, one point at a time.
	if (done == 0)
	{
		say(plan.text(), true);
		return;
	}

	data.setPlcMasterData(plc);

	if (QUndoStack *stack = m_project->undoStack()) {
		stack->push(new AssignIoPointsCommand(m_project, master,
						      list, data, done));
	}

	m_report = plan.text();
	cardChanged();
	say(m_report, !plan.isClean());
}

/**
	@brief IoAssignDialog::release
	Give back the ticked voies, and put their points back in the list.
*/
void IoAssignDialog::release()
{
	Element *master = currentCard();
	if (!master || !m_project) {
		return;
	}

	const QList<int> rows = checkedChannels();
	if (rows.isEmpty()) {
		return;
	}

	ElementData data = master->elementData();
	ElementData::PlcMasterData plc = data.plcMasterData();
	const QString uuid = master->uuid().toString();

	IoList list = m_project->ioList();
	const int freed = IoAssignment::release(list, plc.ios, uuid, rows);

	if (freed == 0)
	{
		say(tr("Aucune voie n'a été libérée."), true);
		return;
	}

	data.setPlcMasterData(plc);

	if (QUndoStack *stack = m_project->undoStack()) {
		stack->push(new AssignIoPointsCommand(m_project, master,
						      list, data, freed, true));
	}

	m_report = tr("%n voie(s) libérée(s), et autant de points revenus dans "
		      "la liste.", "", freed);
	cardChanged();
	say(m_report);
}
