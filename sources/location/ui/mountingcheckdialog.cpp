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
#include "mountingcheckdialog.h"

#include "../../catalog/catalog.h"
#include "../../qetproject.h"
#include "../bommeasure.h"
#include "../mountingpartview.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
	enum Column
	{
		KindColumn = 0,
		PartsColumn,
		DistanceColumn,
		ColumnCount
	};

		/// the part a row points at, on a complaint row
	const int ITEM_ROLE = Qt::UserRole + 1;

	/**
		@brief millimetres
		@param value a length in millimetre
		@return the number as this program writes a millimetre

		Borrowed from BomMeasure and not written again, for the reason
		the drilling table borrows it: this window and the material
		list are read in the same afternoon, and two hands writing
		millimetres disagree about the decimal separator sooner or
		later.
	*/
	QString millimetres(qreal value)
	{
		return BomMeasure::formatQuantityWithUnit(value,
							  QStringLiteral("mm"));
	}
}

/**
	@brief MountingCheckDialog::MountingCheckDialog
	@param project the project the faces are read from
	@param catalog where a clearance is looked up, none when none
	@param parent parent widget
*/
MountingCheckDialog::MountingCheckDialog(QETProject *project,
					 Catalog *catalog,
					 QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_catalog(catalog)
{
	buildWidgets();

	if (m_project)
	{
			//The layout editor writes the plate back into the
			//project after every step of its stack, so this is the
			//news that something moved: the answer on the screen
			//has to follow the drag that caused it, or a person
			//fixes an overlap and is still told about it.
		connect(m_project.data(), &QETProject::mountingLayoutChanged,
			this, &MountingCheckDialog::layoutChanged);
		connect(m_project.data(), &QObject::destroyed,
			this, &QWidget::close);
	}

	refreshSurfaceList();

	const MountingLayout layout = m_project ? m_project->mountingLayout()
						: MountingLayout();
	const QStringList faces = layout.surfaceUuids();
	if (!faces.isEmpty()) {
		showSurface(faces.first());
	}
	else
	{
		fill();
		say(tr("This project has no plate yet, so there is nothing to "
		       "check."));
	}
}

/**
	@brief MountingCheckDialog::project
	@return the project this check reads from
*/
QETProject *MountingCheckDialog::project() const
{
	return m_project.data();
}

/**
	@brief MountingCheckDialog::shownSurface
	@return the face being checked, empty when none
*/
QString MountingCheckDialog::shownSurface() const
{
	return m_shown;
}

/**
	@brief MountingCheckDialog::showSurface
	@param surface_uuid which face, empty for none
	@return true when there was such a face in the project
*/
bool MountingCheckDialog::showSurface(const QString &surface_uuid)
{
	const MountingLayout layout = m_project ? m_project->mountingLayout()
						: MountingLayout();
	const int index = layout.indexOfSurface(surface_uuid);

	m_shown = index >= 0 ? surface_uuid : QString();

	const int row = m_surface_box->findData(m_shown);
	if (row >= 0 && row != m_surface_box->currentIndex())
	{
		m_filling = true;
		m_surface_box->setCurrentIndex(row);
		m_filling = false;
	}

	fill();
	return index >= 0;
}

/**
	@brief MountingCheckDialog::report
	@return the whole report of the face being checked
*/
MountingSurfaceReport MountingCheckDialog::report() const
{
	return m_report;
}

/**
	@brief MountingCheckDialog::issueRowCount
	@return how many complaint rows are listed
*/
int MountingCheckDialog::issueRowCount() const
{
	int rows = 0;
	for (int group = 0 ; group < m_tree->topLevelItemCount() ; ++group) {
		rows += m_tree->topLevelItem(group)->childCount();
	}
	return rows;
}

/**
	@brief MountingCheckDialog::complainedAbout
	@return the identity of every part named in a complaint
*/
QStringList MountingCheckDialog::complainedAbout() const
{
	return m_report.complainedAbout();
}

/**
	@brief MountingCheckDialog::summaryText
	@return the sentence under the list, as it is shown
*/
QString MountingCheckDialog::summaryText() const
{
	return m_summary->text();
}

/**
	@brief MountingCheckDialog::fitMessage
	@param fit the answer
	@return the sentence for the person who has to fix it

	Four sentences for four answers, and the switch is exhaustive on
	purpose: a fifth value added to the enum has to break the build here
	rather than draw a row with nothing written on it.
*/
QString MountingCheckDialog::fitMessage(MountingFit fit)
{
	switch (fit)
	{
		case MountingFit::Fits:
			return tr("Where it can be.");
		case MountingFit::OutsideArea:
			return tr("Off the plate where it stands — drag it "
				  "back on.");
		case MountingFit::LargerThanArea:
			return tr("Bigger than the whole plate — no position "
				  "on this face holds it.");
		case MountingFit::NoArea:
			return tr("This plate has no usable dimensions, so "
				  "nothing can be said about where this part "
				  "sits.");
	}
	return QString();
}

/**
	@brief MountingCheckDialog::shownSurfaceData
	@return the face being checked, a default one when none
*/
MountingSurface MountingCheckDialog::shownSurfaceData() const
{
	if (!m_project || m_shown.isEmpty()) {
		return MountingSurface();
	}
	return m_project->mountingLayout().surface(m_shown);
}

/**
	@brief MountingCheckDialog::designationOf
	@param item_uuid which part
	@return how that part is called on this face

	Through MountedItem::designation, which never answers empty: a
	complaint that names nothing points at nothing.
*/
QString MountingCheckDialog::designationOf(const QString &item_uuid) const
{
	return shownSurfaceData().item(item_uuid).designation();
}

/**
	@brief MountingCheckDialog::buildWidgets
*/
void MountingCheckDialog::buildWidgets()
{
	setWindowTitle(tr("Plate check"));

	m_surface_box = new QComboBox(this);
	m_surface_box->setMinimumWidth(240);
	m_surface_box->setToolTip(tr("The face being checked."));
	connect(m_surface_box,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &MountingCheckDialog::surfaceChosen);

	m_recheck = new QPushButton(tr("Check again"), this);
	m_recheck->setToolTip(tr("Ask the rule again. It is asked by itself "
				 "after every step of the layout; this is for "
				 "when the catalogue changed underneath."));
	connect(m_recheck, &QPushButton::clicked,
		this, &MountingCheckDialog::recheck);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(ColumnCount);
	m_tree->setHeaderLabels(QStringList()
				<< tr("What")
				<< tr("Parts")
				<< tr("How far"));
	m_tree->setRootIsDecorated(true);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tree->header()->setStretchLastSection(false);
	m_tree->header()->setSectionResizeMode(PartsColumn,
					       QHeaderView::Stretch);

	connect(m_tree, &QTreeWidget::itemDoubleClicked,
		this, &MountingCheckDialog::rowActivated);
	connect(m_tree, &QTreeWidget::itemSelectionChanged,
		this, &MountingCheckDialog::selectionChanged);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	QFont summary_font = m_summary->font();
	summary_font.setBold(true);
	m_summary->setFont(summary_font);

		//The second sentence, and it is never optional: a clean plate
		//whose parts nobody measured is clean for a reason that has
		//nothing to do with the plate being right.
	m_trust = new QLabel(this);
	m_trust->setWordWrap(true);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QHBoxLayout *top = new QHBoxLayout();
	top->addWidget(new QLabel(tr("Plate:"), this));
	top->addWidget(m_surface_box, 1);
	top->addWidget(m_recheck);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(top);
	layout->addWidget(m_tree, 1);
	layout->addWidget(m_summary);
	layout->addWidget(m_trust);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	resize(820, 560);
}

/**
	@brief MountingCheckDialog::refreshSurfaceList
*/
void MountingCheckDialog::refreshSurfaceList()
{
	m_filling = true;
	m_surface_box->clear();

	const MountingLayout layout = m_project ? m_project->mountingLayout()
						: MountingLayout();
	const QStringList faces = layout.surfaceUuids();

	if (faces.isEmpty()) {
		m_surface_box->addItem(tr("(no plate)"), QString());
	}

	for (const QString &uuid : faces)
	{
		const MountingSurface surface = layout.surface(uuid);
		m_surface_box->addItem(surface.designation(), uuid);
	}

	const int index = m_surface_box->findData(m_shown);
	if (index >= 0) {
		m_surface_box->setCurrentIndex(index);
	}

	m_filling = false;
}

/**
	@brief MountingCheckDialog::fill
	Ask the rule, then draw what it answered.

	A group with nothing in it is left out rather than shown empty, the way
	the unlocated components report leaves out a folio where everything is
	located: the counts are in the sentence underneath, and a branch that
	opens onto nothing reads as a failure to load.
*/
void MountingCheckDialog::fill()
{
	m_tree->clear();

	const MountingSurface surface = shownSurfaceData();

	if (m_catalog) {
		m_report = MountingPartReader::checkSurface(*m_catalog.data(),
							 surface);
	}
	else
	{
			//The honest fallback: the rule still runs, every part
			//is checked against no requirement, and isConclusive
			//says so. Answering "clean" without the second sentence
			//is what this whole class refuses to do.
		m_report = MountingCheck::surfaceReport(surface);
	}

	QTreeWidgetItem *overlap_group = nullptr;
	for (const MountingOverlap &overlap : m_report.overlaps)
	{
		if (!overlap_group)
		{
			overlap_group = new QTreeWidgetItem(m_tree);
			overlap_group->setText(KindColumn,
					       tr("Two parts in the same "
						  "room"));
		}

		QTreeWidgetItem *row = new QTreeWidgetItem(overlap_group);
		row->setText(KindColumn, tr("Overlap"));
		row->setText(PartsColumn, tr("%1 and %2")
			     .arg(designationOf(overlap.first_uuid),
				  designationOf(overlap.second_uuid)));
		row->setText(DistanceColumn, millimetres(overlap.way_out));
		row->setToolTip(DistanceColumn,
				tr("The shorter way out: how far either of the "
				   "two has to be pushed, along one axis, for "
				   "the room to be free."));
			//Metal on metal is not a complaint one of the two
			//owns, so the row points at the first of the pair; the
			//other one is a millimetre away on the drawing.
		row->setData(KindColumn, ITEM_ROLE, overlap.first_uuid);
	}

	QTreeWidgetItem *air_group = nullptr;
	for (const MountingEncroachment &entry : m_report.encroachments)
	{
		if (!air_group)
		{
			air_group = new QTreeWidgetItem(m_tree);
			air_group->setText(KindColumn,
					   tr("Air a part asked for and does "
					      "not have"));
		}

		QTreeWidgetItem *row = new QTreeWidgetItem(air_group);
		row->setText(KindColumn, tr("Clearance"));
		row->setText(PartsColumn, tr("%1 stands in the air %2 asked "
					     "for")
			     .arg(designationOf(entry.intruder_uuid),
				  designationOf(entry.claimant_uuid)));
		row->setText(DistanceColumn, millimetres(entry.missing));
		row->setToolTip(DistanceColumn,
				tr("How much air is missing: how far the "
				   "intruder has to be pushed for the "
				   "requirement to be met."));
			//The air belongs to whoever asked for it, so the row
			//points at the part standing in it - that is the one a
			//person moves.
		row->setData(KindColumn, ITEM_ROLE, entry.intruder_uuid);
	}

	QTreeWidgetItem *fit_group = nullptr;
	const QList<MountingItemFit> misfits = m_report.misfits();
	for (const MountingItemFit &entry : misfits)
	{
		if (!fit_group)
		{
			fit_group = new QTreeWidgetItem(m_tree);
			fit_group->setText(KindColumn,
					   tr("Parts that are not on the "
					      "plate"));
		}

		QTreeWidgetItem *row = new QTreeWidgetItem(fit_group);
		row->setText(KindColumn, fitMessage(entry.fit));
		row->setText(PartsColumn, designationOf(entry.uuid));
		row->setData(KindColumn, ITEM_ROLE, entry.uuid);

		if (entry.size_unknown)
		{
			row->setToolTip(PartsColumn,
					tr("Nobody typed this part's "
					   "dimensions, so it was judged on "
					   "its corner alone."));
			row->setForeground(PartsColumn,
					   QBrush(QColor(Qt::darkYellow)));
		}
	}

	m_tree->expandAll();
	for (int column = 0 ; column < ColumnCount ; ++column) {
		m_tree->resizeColumnToContents(column);
	}

	if (m_shown.isEmpty())
	{
		m_summary->setText(tr("No plate is checked."));
		m_trust->clear();
		return;
	}

	if (m_report.isClean())
	{
		m_summary->setText(tr("Nothing found against this plate: "
				      "%n part(s) checked.", "",
				      m_report.itemCount()));
	}
	else
	{
			//Two numbers and not one. How many complaints there are
			//is what a person acts on; how they split between the
			//three questions is what says which afternoon it is -
			//an overlap is dragged away in a minute, a part bigger
			//than the plate is another enclosure.
		m_summary->setText(tr("%1 problem(s) on this plate: %2 "
				      "overlap(s), %3 clearance(s), %4 part(s) "
				      "off the plate — out of %5 part(s) "
				      "checked.")
				   .arg(QString::number(m_report.issueCount()),
					QString::number(m_report.overlaps
							.count()),
					QString::number(m_report.encroachments
							.count()),
					QString::number(m_report.misfitCount()),
					QString::number(m_report.itemCount())));
	}

	if (m_report.isConclusive())
	{
		m_trust->setText(tr("Every part of this answer rests on a "
				    "measurement somebody took."));
	}
	else
	{
		QStringList holes;
		if (!m_report.area.isValid()) {
			holes << tr("the plate itself is not measured");
		}
		if (m_report.unknown_size_count > 0)
		{
			holes << tr("%n part(s) have no dimensions", "",
				    m_report.unknown_size_count);
		}
		if (m_report.unknown_clearance_count > 0)
		{
			holes << tr("%n part(s) were checked against no "
				    "clearance at all", "",
				    m_report.unknown_clearance_count);
		}

		m_trust->setText(tr("Read this beside what it does not know: "
				    "%1. A part with no dimensions cannot be "
				    "shown to collide with anything.")
				 .arg(holes.join(QStringLiteral(", "))));
	}
}

/**
	@brief MountingCheckDialog::say
	@param message what to tell the person
	@param problem true when it is a refusal
*/
void MountingCheckDialog::say(const QString &message, bool problem)
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
	@brief MountingCheckDialog::surfaceChosen
	@param index the row of the list that was chosen
*/
void MountingCheckDialog::surfaceChosen(int index)
{
	if (m_filling || index < 0) {
		return;
	}

	const QString uuid = m_surface_box->itemData(index).toString();
	if (uuid == m_shown) {
		return;
	}

	showSurface(uuid);
}

/**
	@brief MountingCheckDialog::rowActivated
	@param item the row that was double clicked
	@param column unused
*/
void MountingCheckDialog::rowActivated(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column)

	if (!item) {
		return;
	}

	const QString uuid = item->data(KindColumn, ITEM_ROLE).toString();
	if (!uuid.isEmpty()) {
		emit goToItem(uuid);
	}
}

/**
	@brief MountingCheckDialog::pointAtRow
	@param index the complaint, in the order they are listed
	@return true when a part was pointed at

	Walked group by group rather than read off a flat list, because the
	list is the tree: an index that counted rows the tree does not have
	would point at the wrong part the day a group is added, and pointing at
	the wrong part is worse than pointing at none.
*/
bool MountingCheckDialog::pointAtRow(int index)
{
	if (index < 0) {
		return false;
	}

	int seen = 0;
	for (int group = 0 ; group < m_tree->topLevelItemCount() ; ++group)
	{
		QTreeWidgetItem *branch = m_tree->topLevelItem(group);
		for (int row = 0 ; row < branch->childCount() ; ++row)
		{
			if (seen == index)
			{
				const QString uuid = branch->child(row)
						     ->data(KindColumn,
							    ITEM_ROLE)
						     .toString();
				if (uuid.isEmpty()) {
					return false;
				}
				emit goToItem(uuid);
				return true;
			}
			++seen;
		}
	}

	return false;
}

/**
	@brief MountingCheckDialog::selectionChanged

	Selecting is enough, and a double click is not required: a check panel
	is walked down with the arrow keys, and having to click twice on every
	line to see where it is would make the walk not worth taking.
*/
void MountingCheckDialog::selectionChanged()
{
	const QList<QTreeWidgetItem *> chosen = m_tree->selectedItems();
	if (chosen.count() != 1) {
		return;
	}

	const QString uuid = chosen.first()->data(KindColumn, ITEM_ROLE)
			     .toString();
	if (!uuid.isEmpty()) {
		emit goToItem(uuid);
	}
}

/**
	@brief MountingCheckDialog::layoutChanged
	A plate changed under this window.
*/
void MountingCheckDialog::layoutChanged()
{
	const QString was_shown = m_shown;

	refreshSurfaceList();

	if (!showSurface(was_shown) && !was_shown.isEmpty())
	{
		say(tr("The plate that was checked is no longer in the "
		       "project."), true);
	}
}

/**
	@brief MountingCheckDialog::recheck
*/
void MountingCheckDialog::recheck()
{
	fill();
	say(tr("Checked again."));
}
