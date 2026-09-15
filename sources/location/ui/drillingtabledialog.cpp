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
#include "drillingtabledialog.h"

#include "../../qetproject.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
		/// index of the hole in the list of the shown face, on a row
	const int INDEX_ROLE = Qt::UserRole + 1;
		/// text the filter matches against, on a row
	const int SEARCH_ROLE = Qt::UserRole + 2;

	/**
		@brief isMisplaced
		@param fit what DrillingTable::fitOf answered
		@return true when the hole is not in the metal

		NoArea is not misplacement and is deliberately excluded: a hole
		on a plate nobody has measured has not been judged at all, and
		counting it here would send a person looking for a mistake that
		has not been shown to exist.
	*/
	bool isMisplaced(MountingFit fit)
	{
		return fit == MountingFit::OutsideArea
		       || fit == MountingFit::LargerThanArea;
	}
}

/**
	@brief DrillingTableDialog::DrillingTableDialog
	@param project the project the faces are read from
	@param parent parent widget
*/
DrillingTableDialog::DrillingTableDialog(QETProject *project,
					 QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	buildWidgets();

	if (m_project)
	{
			//The layout is written back into the project after
			//every step of the layout editor, so this is the news
			//that a plate has changed under this window - a face
			//added, a part moved, a face gone.
		connect(m_project.data(), &QETProject::mountingLayoutChanged,
			this, &DrillingTableDialog::layoutChanged);
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
		say(tr("This project has no plate yet. A drilling table is the "
		       "worklist of one face, and there is no face to list."));
	}
}

/**
	@brief DrillingTableDialog::project
	@return the project this table is read from
*/
QETProject *DrillingTableDialog::project() const
{
	return m_project.data();
}

/**
	@brief DrillingTableDialog::shownSurface
	@return the face being listed, empty when none
*/
QString DrillingTableDialog::shownSurface() const
{
	return m_shown;
}

/**
	@brief DrillingTableDialog::showSurface
	@param surface_uuid which face, empty for none
	@return true when there was such a face in the project
*/
bool DrillingTableDialog::showSurface(const QString &surface_uuid)
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
	@brief DrillingTableDialog::setHoles
	@param surface_uuid which face the holes belong to
	@param surface_holes the holes, in the order they are to be drilled
*/
void DrillingTableDialog::setHoles(const QString &surface_uuid,
				   const QList<DrillingHole> &surface_holes)
{
	if (surface_uuid.isEmpty()) {
		return;
	}

	if (surface_holes.isEmpty()) {
		m_holes.remove(surface_uuid);
	}
	else {
		m_holes.insert(surface_uuid, surface_holes);
	}

	if (surface_uuid == m_shown) {
		fill();
	}
}

/**
	@brief DrillingTableDialog::setHoles
	@param surface_holes the holes of the face being listed
*/
void DrillingTableDialog::setHoles(const QList<DrillingHole> &surface_holes)
{
	setHoles(m_shown, surface_holes);
}

/**
	@brief DrillingTableDialog::holes
	@return the holes of the face being listed
*/
QList<DrillingHole> DrillingTableDialog::holes() const
{
	return m_holes.value(m_shown);
}

/**
	@brief DrillingTableDialog::holeCount
	@return how many holes that face has
*/
int DrillingTableDialog::holeCount() const
{
	return static_cast<int>(holes().count());
}

/**
	@brief DrillingTableDialog::visibleRowCount
	@return how many rows the filter leaves visible
*/
int DrillingTableDialog::visibleRowCount() const
{
	int shown = 0;
	for (int row = 0 ; row < m_tree->topLevelItemCount() ; ++row)
	{
		if (!m_tree->topLevelItem(row)->isHidden()) {
			++shown;
		}
	}
	return shown;
}

/**
	@brief DrillingTableDialog::setFilter
	@param needle what to match, empty to show everything
*/
void DrillingTableDialog::setFilter(const QString &needle)
{
	m_filter->setText(needle);
}

/**
	@brief DrillingTableDialog::filter
	@return what is being filtered on
*/
QString DrillingTableDialog::filter() const
{
	return m_filter->text();
}

/**
	@brief DrillingTableDialog::columnNames
	@return the column names, in order
*/
QStringList DrillingTableDialog::columnNames() const
{
	QStringList names;
	for (int column = 0 ; column < m_tree->columnCount() ; ++column) {
		names << m_tree->headerItem()->text(column);
	}
	return names;
}

/**
	@brief DrillingTableDialog::referenceFrameText
	@return the frame sentence shown at the head
*/
QString DrillingTableDialog::referenceFrameText() const
{
	return DrillingTable::referenceFrameText(frame());
}

/**
	@brief DrillingTableDialog::asText
	@param separator the delimiter between cells
	@return the whole table of the face being listed
*/
QString DrillingTableDialog::asText(const QString &separator) const
{
	return DrillingTable::toDelimitedText(holes(), separator,
					      QLocale(), frame());
}

/**
	@brief DrillingTableDialog::toolTotals
	@return how many holes each tool makes
*/
QList<DrillingToolTotal> DrillingTableDialog::toolTotals() const
{
	return DrillingTable::toolTotals(holes());
}

/**
	@brief DrillingTableDialog::offSurfaceCount
	@return how many holes are not in the metal
*/
int DrillingTableDialog::offSurfaceCount() const
{
	const MountingArea area = shownSurfaceData().area;

	int misplaced = 0;
	const QList<DrillingHole> listed = holes();
	for (const DrillingHole &hole : listed)
	{
		if (isMisplaced(DrillingTable::fitOf(hole, area))) {
			++misplaced;
		}
	}
	return misplaced;
}

/**
	@brief DrillingTableDialog::summaryText
	@return the sentence under the table, as it is shown
*/
QString DrillingTableDialog::summaryText() const
{
	return m_summary->text();
}

/**
	@brief DrillingTableDialog::shownSurfaceData
	@return the face being listed, a default one when none
*/
MountingSurface DrillingTableDialog::shownSurfaceData() const
{
	if (!m_project || m_shown.isEmpty()) {
		return MountingSurface();
	}
	return m_project->mountingLayout().surface(m_shown);
}

/**
	@brief DrillingTableDialog::frame
	@return the origin of the face bound to its dimensions

	Built here and never stored, which is the rule DrillingFrame states: a
	plate resized to another pair of numbers changes every coordinate
	measured from its far edges, and a frame kept in a member would go on
	answering about the plate of an hour ago.
*/
DrillingFrame DrillingTableDialog::frame() const
{
	return shownSurfaceData().drillingFrame();
}

/**
	@brief DrillingTableDialog::buildWidgets
*/
void DrillingTableDialog::buildWidgets()
{
	setWindowTitle(tr("Drilling table"));

	m_surface_box = new QComboBox(this);
	m_surface_box->setMinimumWidth(240);
	m_surface_box->setToolTip(tr("The face whose holes are listed. One "
				     "list is one plate, because one plate is "
				     "what is taken to the drill."));
	connect(m_surface_box,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &DrillingTableDialog::surfaceChosen);

	m_filter = new QLineEdit(this);
	m_filter->setClearButtonEnabled(true);
	m_filter->setPlaceholderText(tr("Filter by mark, size or purpose…"));
	connect(m_filter, &QLineEdit::textChanged,
		this, &DrillingTableDialog::filterChanged);

	m_frame_label = new QLabel(this);
	m_frame_label->setWordWrap(true);
	m_frame_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		//Said in bold because it is the one sentence on the sheet that
		//tells the bench which of the four corners the numbers were
		//measured from, and a workshop that guesses wrong drills the
		//mirror image of the plate that was drawn.
	QFont frame_font = m_frame_label->font();
	frame_font.setBold(true);
	m_frame_label->setFont(frame_font);

	m_tree = new QTreeWidget(this);
		//The columns of the table are the columns of the file, from
		//the one list of them: two spellings of the same column read
		//as two columns to anybody holding both documents.
	const QStringList columns = DrillingTable::header();
	m_tree->setColumnCount(static_cast<int>(columns.count()));
	m_tree->setHeaderLabels(columns);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
		//A coordinate is not typed here - see the class comment.
	m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tree->header()->setStretchLastSection(true);

	connect(m_tree, &QTreeWidget::itemDoubleClicked,
		this, &DrillingTableDialog::rowActivated);

	m_tools = new QTreeWidget(this);
	m_tools->setColumnCount(3);
	m_tools->setHeaderLabels(QStringList()
				 << tr("Tool")
				 << tr("Size")
				 << tr("Holes"));
	m_tools->setRootIsDecorated(false);
	m_tools->setAlternatingRowColors(true);
	m_tools->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tools->setSelectionMode(QAbstractItemView::NoSelection);
	m_tools->setMaximumHeight(140);
	m_tools->setToolTip(tr("Which bits to fit, and how many times each. "
			       "The counts add up to the number of holes: a "
			       "hole lost between two groups leaves the number "
			       "of groups exactly as it was."));

	m_copy = new QPushButton(tr("Copy"), this);
	m_copy->setToolTip(tr("Copy the whole list, tab separated, whatever "
			      "the filter shows."));
	m_export = new QPushButton(tr("Export…"), this);
	m_export->setToolTip(tr("Write the whole list to a delimited file, "
				"whatever the filter shows."));

	connect(m_copy, &QPushButton::clicked,
		this, &DrillingTableDialog::copyToClipboard);
	connect(m_export, &QPushButton::clicked,
		this, &DrillingTableDialog::exportCsv);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QHBoxLayout *top = new QHBoxLayout();
	top->addWidget(new QLabel(tr("Plate:"), this));
	top->addWidget(m_surface_box, 1);
	top->addWidget(m_copy);
	top->addWidget(m_export);

	QHBoxLayout *tools = new QHBoxLayout();
	tools->addWidget(new QLabel(tr("Filter:"), this));
	tools->addWidget(m_filter, 1);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(top);
	layout->addWidget(m_frame_label);
	layout->addLayout(tools);
	layout->addWidget(m_tree, 1);
	layout->addWidget(m_tools);
	layout->addWidget(m_summary);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	resize(900, 620);
}

/**
	@brief DrillingTableDialog::refreshSurfaceList
	Fill the list of faces from the project.

	Filling it must choose nothing, for the reason the layout editor states:
	setting the rows of a combo box makes it report that its current row
	changed, and answering that report would list another face than the one
	being looked at.
*/
void DrillingTableDialog::refreshSurfaceList()
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
	@brief DrillingTableDialog::fill
	One row per hole, in the order they were handed over.
*/
void DrillingTableDialog::fill()
{
	m_tree->clear();
	m_tools->clear();

	m_frame_label->setText(referenceFrameText());

	const MountingArea area = shownSurfaceData().area;
	const DrillingFrame read_frame = frame();
	const QLocale locale;

	const QList<DrillingHole> listed = holes();
	const int listed_count = static_cast<int>(listed.count());

	for (int index = 0 ; index < listed_count ; ++index)
	{
		const DrillingHole &hole = listed.at(index);
		const QStringList cells = DrillingTable::row(hole, locale,
							    read_frame);
		const int cell_count = static_cast<int>(cells.count());

		QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
		for (int column = 0 ; column < cell_count ; ++column) {
			item->setText(column, cells.at(column));
		}
		item->setData(0, INDEX_ROLE, index);
		item->setData(0, SEARCH_ROLE, cells.join(QLatin1Char(' '))
					      .toLower());

			//Said on the row rather than in a column of its own:
			//the columns here are the columns of the file, and the
			//file is the workshop's. A hole off the metal is a
			//thing to fix before the plate is drilled, not a cell
			//to hand over.
		const MountingFit fit = DrillingTable::fitOf(hole, area);
		if (isMisplaced(fit))
		{
			const QString reason =
				(fit == MountingFit::LargerThanArea)
				? tr("This hole is bigger than the whole "
				     "plate.")
				: tr("This hole is off the plate where it "
				     "stands.");

			for (int column = 0 ; column < cell_count ; ++column)
			{
				item->setToolTip(column, reason);
				item->setForeground(column,
						    QBrush(QColor(Qt::red)));
			}
		}
	}

	const QList<DrillingToolTotal> totals = toolTotals();
	for (const DrillingToolTotal &total : totals)
	{
		QTreeWidgetItem *item = new QTreeWidgetItem(m_tools);
		item->setText(0, (total.shape == DrillingShape::Round)
				 ? tr("Drilled") : tr("Cut out"));
		item->setText(1, total.sizeText(locale));
		item->setText(2, locale.toString(total.count));
	}

	for (int column = 0 ; column < m_tree->columnCount() ; ++column) {
		m_tree->resizeColumnToContents(column);
	}
	for (int column = 0 ; column < m_tools->columnCount() ; ++column) {
		m_tools->resizeColumnToContents(column);
	}

	filterChanged();
	updateSummary();

	const bool something = !listed.isEmpty();
	m_copy->setEnabled(something);
	m_export->setEnabled(something);
}

/**
	@brief DrillingTableDialog::updateSummary

	Two numbers and not one. How many groups there are catches a hole that
	became a line of its own; how many holes they add up to catches the one
	that was folded into a group it does not belong to - and that second
	failure leaves the number of groups exactly as it was.
*/
void DrillingTableDialog::updateSummary()
{
	const QList<DrillingHole> listed = holes();
	const QList<DrillingToolTotal> totals = toolTotals();

	if (m_shown.isEmpty())
	{
		m_summary->setText(tr("No plate is listed."));
		return;
	}

	if (listed.isEmpty())
	{
			//The honest sentence of today: the producer of holes is
			//the drilling view, which does not exist yet, so an
			//empty table here is the state of the program and not a
			//plate that was forgotten.
		m_summary->setText(tr("No hole on this plate. The form, the "
				      "reference frame and the export are "
				      "ready; nothing produces holes yet."));
		return;
	}

	int summed = 0;
	for (const DrillingToolTotal &total : totals) {
		summed += total.count;
	}

	QStringList sentences;
	sentences << tr("%1 hole(s), %2 tool(s).")
		     .arg(QString::number(summed),
			  QString::number(totals.count()));

	const MountingArea area = shownSurfaceData().area;
	if (!area.isValid())
	{
		sentences << tr("This plate is not measured: no hole can be "
				"said to be in the metal or off it.");
	}
	else
	{
		const int misplaced = offSurfaceCount();
		if (misplaced > 0)
		{
			sentences << tr("%n of them is not in the metal.",
					"", misplaced);
		}
	}

	m_summary->setText(sentences.join(QLatin1Char(' ')));
}

/**
	@brief DrillingTableDialog::say
	@param message what to tell the person
	@param problem true when it is a refusal
*/
void DrillingTableDialog::say(const QString &message, bool problem)
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
	@brief DrillingTableDialog::surfaceChosen
	@param index the row of the list that was chosen
*/
void DrillingTableDialog::surfaceChosen(int index)
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
	@brief DrillingTableDialog::filterChanged
*/
void DrillingTableDialog::filterChanged()
{
	const QString needle = m_filter->text().trimmed().toLower();

	for (int row = 0 ; row < m_tree->topLevelItemCount() ; ++row)
	{
		QTreeWidgetItem *item = m_tree->topLevelItem(row);
		const QString haystack = item->data(0, SEARCH_ROLE).toString();
		item->setHidden(!needle.isEmpty()
				&& !haystack.contains(needle));
	}
}

/**
	@brief DrillingTableDialog::rowActivated
	@param item the row that was double clicked
	@param column unused
*/
void DrillingTableDialog::rowActivated(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column)

	if (!item) {
		return;
	}

	const QVariant index = item->data(0, INDEX_ROLE);
	if (index.isValid()) {
		pointAtRow(index.toInt());
	}
}

/**
	@brief DrillingTableDialog::pointAtRow
	@param row the hole, in the order they were handed over
	@return true when a component was pointed at
*/
bool DrillingTableDialog::pointAtRow(int row)
{
	const QList<DrillingHole> listed = holes();
	if (row < 0 || row >= static_cast<int>(listed.count())) {
		return false;
	}

	const QString component = listed.at(row).component_uuid;
	if (component.isEmpty())
	{
		say(tr("This hole belongs to no component: a gland, or a "
		       "fixing hole for something cut on the bench."));
		return false;
	}

	emit goToComponent(component);
	return true;
}

/**
	@brief DrillingTableDialog::copyToClipboard
*/
void DrillingTableDialog::copyToClipboard()
{
	QApplication::clipboard()->setText(asText(QStringLiteral("\t")));
	say(tr("The whole list of this plate was copied, the filter included."));
}

/**
	@brief DrillingTableDialog::exportCsv

	The whole plate, never the filter. A drilling list handed over with
	holes missing produces a plate with holes missing, and nothing in the
	document that lacks them says they were ever there.
*/
void DrillingTableDialog::exportCsv()
{
	const QString path = QFileDialog::getSaveFileName(
				this,
				tr("Export the drilling table"),
				QString(),
				tr("CSV file (*.csv)"));
	if (path.isEmpty()) {
		return;
	}

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		say(tr("The file \"%1\" could not be written.").arg(path),
		    true);
		return;
	}

	QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	stream.setCodec("UTF-8");
#endif
	stream << asText(QStringLiteral(";")) << "\n";
	file.close();

	say(tr("The whole list of this plate was written to \"%1\", the "
	       "filter included.").arg(path));
}

/**
	@brief DrillingTableDialog::layoutChanged
	A plate changed under this window.

	The holes handed over are kept: they belong to the face, and a part
	moved on another plate is no reason to drop them. What is read again is
	the list of faces and the frame, because a plate that was resized
	changes every coordinate measured from its far edges.
*/
void DrillingTableDialog::layoutChanged()
{
	const QString was_shown = m_shown;

	refreshSurfaceList();

	if (!showSurface(was_shown) && !was_shown.isEmpty())
	{
		say(tr("The plate that was listed is no longer in the "
		       "project."), true);
	}
}
