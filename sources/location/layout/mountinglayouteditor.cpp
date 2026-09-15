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
#include "mountinglayouteditor.h"

#include "../../catalog/catalog.h"
#include "../../catalog/catalogpart.h"
#include "../../catalog/ui/catalogbrowserdialog.h"
#include "../../qetapp.h"
#include "../../qeticons.h"
#include "../../qetproject.h"
#include "../mountinglayout.h"
#include "../mountingpartview.h"
#include "mountingscene.h"
#include "mountingview.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>

namespace
{
	/// millimetre: where the first part dropped on a face lands
	const qreal FIRST_DROP = 10.0;

	/// millimetre: how far apart parts dropped one after another land, so
	/// that two of them never hide each other exactly
	const qreal DROP_STEP = 15.0;

	/// how many parts are dropped before the cascade starts over
	const int DROP_WRAP = 12;

	/// how long an ordinary message stays in the status bar, milliseconds
	const int MESSAGE_LIFE = 6000;
}

/**
	@brief MountingLayoutEditor::MountingLayoutEditor
	@param project the project whose panel is being laid out
	@param parent parent widget, normally none

	No parent by default, and that is what makes this a window of the
	application rather than a dialogue of the folio editor: QETApp finds its
	editors among the top level widgets, so a window with a parent would be
	a window the program cannot list, cannot raise and cannot count.
*/
MountingLayoutEditor::MountingLayoutEditor(QETProject *project,
					   QWidget *parent) :
	QMainWindow(parent),
	m_project(project)
{
	m_scene = new MountingScene(this);
	m_view  = new MountingView(m_scene, this);

	buildActions();
	buildWidgets();

	connect(&m_scene->undoStack(), &QUndoStack::indexChanged,
		this, &MountingLayoutEditor::stepApplied);
	connect(m_view, &MountingView::pointed,
		this, &MountingLayoutEditor::pointedAt);
	connect(m_view, &MountingView::zoomChanged,
		this, &MountingLayoutEditor::zoomTold);

		//The project can be closed while this window is open - it is a
		//window of its own, and nothing keeps the two alive together.
		//Going with it is the only honest answer: a layout editor over
		//a project that no longer exists has nothing to write into.
	if (m_project)
	{
		connect(m_project, &QObject::destroyed, this, [this]()
		{
			close();
		});
	}

	refreshSurfaceList();

		//Opened on the first face there is, because a window that opens
		//empty over a project that has a plate looks like a window that
		//has lost it.
	const MountingLayout layout = m_project ? m_project->mountingLayout()
						: MountingLayout();
	const QStringList faces = layout.surfaceUuids();
	if (!faces.isEmpty()) {
		showSurface(faces.first());
	}
	else
	{
		updateTitle();
		updateActions();
		say(tr("Ce projet n'a pas encore de platine. « Nouvelle "
		       "platine… » en déclare une pour une localisation "
		       "existante."));
	}

	if (m_project && m_project->isReadOnly())
	{
			//Nothing is dragged and nothing is written: the project
			//is open read only, and a drawing that let itself be
			//rearranged would be promising a save that cannot
			//happen.
		m_view->setInteractive(false);
		say(tr("Projet en lecture seule : le calepinage se regarde, il "
		       "ne se modifie pas."), true);
	}

	readSettings();
}

MountingLayoutEditor::~MountingLayoutEditor()
{}

/**
	@brief MountingLayoutEditor::project
	@return the project this window lays out
*/
QETProject *MountingLayoutEditor::project() const
{
	return m_project.data();
}

/**
	@brief MountingLayoutEditor::scene
	@return the scene the face is drawn on
*/
MountingScene *MountingLayoutEditor::scene() const
{
	return m_scene;
}

/**
	@brief MountingLayoutEditor::view
	@return the view the face is looked at through
*/
MountingView *MountingLayoutEditor::view() const
{
	return m_view;
}

/**
	@brief MountingLayoutEditor::shownSurface
	@return the identifier of the face being drawn, empty when none
*/
QString MountingLayoutEditor::shownSurface() const
{
	return m_shown;
}

/**
	@brief MountingLayoutEditor::showSurface
	@param surface_uuid which face, empty for none
	@return true when the project holds such a face

	The face being left is written back first. Every step already wrote
	itself, so in the ordinary case this finds nothing to do; it is here for
	the case where it does, because the alternative is losing a drag by
	changing a combo box.
*/
bool MountingLayoutEditor::showSurface(const QString &surface_uuid)
{
	commitToProject();

	if (!m_project)
	{
		m_shown.clear();
		m_scene->setSurface(MountingSurface());
		updateTitle();
		updateActions();
		return false;
	}

	const MountingLayout layout = m_project->mountingLayout();
	const int index = layout.indexOfSurface(surface_uuid);

	m_shown = index >= 0 ? surface_uuid : QString();
	m_scene->setSurface(index >= 0 ? layout.at(index) : MountingSurface());

	m_view->zoomFit();
	updateTitle();
	updateActions();

	if (index >= 0 && !m_scene->isAreaMeasured())
	{
		say(tr("Cette platine n'est pas mesurée : ses composants sont "
		       "dessinés, la tôle sous eux ne l'est pas."));
	}

	return index >= 0;
}

/**
	@brief MountingLayoutEditor::commitToProject
	@return true when the project was given something new
*/
bool MountingLayoutEditor::commitToProject()
{
	if (!m_project || m_shown.isEmpty()) {
		return false;
	}
	if (m_project->isReadOnly()) {
		return false;
	}

	MountingLayout layout = m_project->mountingLayout();
	QString error;

	if (!layout.updateSurface(m_scene->surface(), &error))
	{
			//An empty reason is the ordinary answer: what is drawn
			//is already what the project holds, so there is nothing
			//to write. A reason means the face went away underneath
			//this window, and saying so is better than writing it
			//back and inventing it again.
		if (!error.isEmpty()) {
			say(error, true);
		}
		return false;
	}

	m_project->setMountingLayout(layout);
	return true;
}

/**
	@brief MountingLayoutEditor::addSurface
	@param location_path the location the face belongs to
	@param kind which face of it
	@param name what a person calls it
	@param area how much room it has, an unusable one meaning unmeasured
	@param error filled with why nothing was added
	@return the identifier of the face, empty when refused
*/
QString MountingLayoutEditor::addSurface(const QString &location_path,
					 const QString &kind,
					 const QString &name,
					 const QSizeF &area,
					 QString *error)
{
	if (error) {
		error->clear();
	}

	if (!isEditable())
	{
		if (error) {
			*error = tr("Ce projet ne se modifie pas.");
		}
		return QString();
	}

	MountingSurface surface(location_path, kind, MountingArea(area));
	surface.name = name;

	MountingLayout layout = m_project->mountingLayout();
	const QString added = layout.appendSurface(surface, error);
	if (added.isEmpty()) {
		return QString();
	}

	m_project->setMountingLayout(layout);
	refreshSurfaceList();
	showSurface(added);

	return added;
}

/**
	@brief MountingLayoutEditor::closeEvent
	@param event

	Written back on the way out as well, and not only after every step. The
	step by step write is what a backup needs; this one covers the moment
	between the last step and the close - and it costs nothing when there is
	nothing to write, because the project compares before it stores.
*/
void MountingLayoutEditor::closeEvent(QCloseEvent *event)
{
	commitToProject();
	writeSettings();
	QMainWindow::closeEvent(event);
}

/**
	@brief MountingLayoutEditor::stepApplied
	A step of the undo stack has just been pushed, undone or redone.
*/
void MountingLayoutEditor::stepApplied()
{
	commitToProject();
	updateActions();
}

/**
	@brief MountingLayoutEditor::surfaceChosen
	@param index the row of the list that was chosen
*/
void MountingLayoutEditor::surfaceChosen(int index)
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
	@brief MountingLayoutEditor::newSurface
	Ask for a face and add it.

	The two numbers are typed, and they start at nothing. A default of 600
	by 800 would be this window guessing how big somebody's cabinet is, and
	a guessed measurement is the one nobody checks; zero reads back as "not
	measured", which is a state the whole module already knows how to
	report.
*/
void MountingLayoutEditor::newSurface()
{
	if (!isEditable()) {
		return;
	}

	const QStringList paths = m_project->locationTree().paths();
	if (paths.isEmpty())
	{
		QMessageBox::information(
				this,
				tr("Nouvelle platine"),
				tr("Une platine appartient à une localisation "
				   "du projet, et ce projet n'en a aucune. "
				   "Faites l'armoire dans « Armoires et "
				   "localisations… », puis revenez ici."));
		return;
	}

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Nouvelle platine"));

	QComboBox *location_box = new QComboBox(&dialog);
	location_box->addItems(paths);

		//Tokens, and the wording stays here: the file keeps « plate »
		//whatever language the person who reads it works in.
	QComboBox *kind_box = new QComboBox(&dialog);
	kind_box->addItem(tr("Platine (fond d'armoire)"),
			  MountingSurface::defaultKind());
	kind_box->addItem(tr("Porte"), QStringLiteral("door"));
	kind_box->addItem(tr("Joue (flanc)"), QStringLiteral("side"));

	QLineEdit *name_edit = new QLineEdit(&dialog);
	name_edit->setPlaceholderText(tr("Platine principale"));

	QDoubleSpinBox *width_box = new QDoubleSpinBox(&dialog);
	width_box->setRange(0.0, 10000.0);
	width_box->setDecimals(1);
	width_box->setSuffix(tr(" mm"));
	width_box->setSpecialValueText(tr("non mesurée"));

	QDoubleSpinBox *height_box = new QDoubleSpinBox(&dialog);
	height_box->setRange(0.0, 10000.0);
	height_box->setDecimals(1);
	height_box->setSuffix(tr(" mm"));
	height_box->setSpecialValueText(tr("non mesurée"));

	QDialogButtonBox *buttons = new QDialogButtonBox(
				QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
				&dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	QFormLayout *form = new QFormLayout(&dialog);
	form->addRow(tr("Localisation :"), location_box);
	form->addRow(tr("Face :"), kind_box);
	form->addRow(tr("Nom :"), name_edit);
	form->addRow(tr("Largeur utile :"), width_box);
	form->addRow(tr("Hauteur utile :"), height_box);
	form->addRow(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	QString error;
	const QString added = addSurface(location_box->currentText(),
					 kind_box->currentData().toString(),
					 name_edit->text().trimmed(),
					 QSizeF(width_box->value(),
						height_box->value()),
					 &error);

	if (added.isEmpty()) {
		say(error, true);
	}
	else {
		say(tr("Platine ajoutée."));
	}
}

/**
	@brief MountingLayoutEditor::addPart
	Screw a part of the catalogue onto the face being shown.

	The size comes from the catalogue and from nowhere else - nobody types a
	width here - and a product the catalogue holds no measurement for
	arrives as a part reported unmeasured rather than as a box of an
	invented size.

	What this does not do is tie the part to a component of a folio, and
	what it does not do either is count it in the bill of material. Both are
	steps of their own, and doing half of either here would make a part that
	is drawn twice or bought twice.
*/
void MountingLayoutEditor::addPart()
{
	if (!isEditable() || m_shown.isEmpty()) {
		return;
	}

	Catalog *catalog = QETApp::catalog();
	if (!catalog) {
		return;
	}

	const CatalogPart part = CatalogBrowserDialog::choosePart(catalog, this);
	if (part.code.isEmpty()) {
		return;
	}

	MountedItem item = MountingPartReader::mountedItemFor(*catalog,
							      part.code,
							      freePosition());

	MountingLayout layout = m_project->mountingLayout();
	QString error;
	const QString mounted = layout.mountItem(m_shown, item, &error);

	if (mounted.isEmpty())
	{
		say(error, true);
		return;
	}

	m_project->setMountingLayout(layout);

		/*
			Drawn again from the project, and deliberately not
			through showSurface: that one writes the face it is
			leaving back first, and the face it would be leaving is
			this one *without* the part that has just been mounted -
			so it would unmount it again on the way out.

			The scene has no way of taking one part in, which is why
			the whole face is rebuilt here; and rebuilding drops the
			undo stack, which is why mounting a part is not a step
			that can be undone. Both are the same missing thing, and
			it is the first thing the next step has to mend.
		*/
	m_scene->setSurface(layout.surface(m_shown));
	refreshSurfaceList();
	updateTitle();
	updateActions();

	if (!m_scene->mountedItem(mounted).hasDeclaredSize())
	{
		say(tr("« %1 » est posé, mais le catalogue ne dit pas ce qu'il "
		       "mesure : il est dessiné comme non mesuré.")
		    .arg(part.code), true);
	}
	else {
		say(tr("« %1 » est posé sur la platine.").arg(part.code));
	}
}

/**
	@brief MountingLayoutEditor::pointedAt
	@param position_mm where the pointer is, millimetre
*/
void MountingLayoutEditor::pointedAt(const QPointF &position_mm)
{
	m_position_label->setText(tr("X : %1 mm    Y : %2 mm")
				  .arg(position_mm.x(), 0, 'f', 1)
				  .arg(position_mm.y(), 0, 'f', 1));
}

/**
	@brief MountingLayoutEditor::zoomTold
	@param pixels_per_millimetre how big a millimetre is drawn right now
*/
void MountingLayoutEditor::zoomTold(qreal pixels_per_millimetre)
{
	m_zoom_label->setText(tr("1 mm = %1 px")
			      .arg(pixels_per_millimetre, 0, 'f', 2));
}

/**
	@brief MountingLayoutEditor::buildActions
*/
void MountingLayoutEditor::buildActions()
{
	m_new_surface = new QAction(QET::Icons::Add,
				    tr("Nouvelle platine…"), this);
	m_new_surface->setStatusTip(tr("Déclare une face à implanter pour une "
				       "localisation du projet."));
	connect(m_new_surface, &QAction::triggered,
		this, &MountingLayoutEditor::newSurface);

	m_add_part = new QAction(tr("Ajouter un composant…"), this);
	m_add_part->setStatusTip(tr("Pose un article du catalogue sur la "
				    "platine, à la mesure du catalogue."));
	connect(m_add_part, &QAction::triggered,
		this, &MountingLayoutEditor::addPart);

	m_undo = m_scene->undoStack().createUndoAction(this, tr("Annuler"));
	m_undo->setIcon(QET::Icons::EditUndo);
	m_undo->setShortcuts(QKeySequence::Undo);

	m_redo = m_scene->undoStack().createRedoAction(this, tr("Rétablir"));
	m_redo->setIcon(QET::Icons::EditRedo);
	m_redo->setShortcuts(QKeySequence::Redo);

	m_zoom_in = new QAction(QET::Icons::ZoomIn, tr("Zoom avant"), this);
	m_zoom_in->setShortcuts(QKeySequence::ZoomIn);
	connect(m_zoom_in, &QAction::triggered, m_view, &MountingView::zoomIn);

	m_zoom_out = new QAction(QET::Icons::ZoomOut, tr("Zoom arrière"), this);
	m_zoom_out->setShortcuts(QKeySequence::ZoomOut);
	connect(m_zoom_out, &QAction::triggered, m_view, &MountingView::zoomOut);

	m_zoom_fit = new QAction(QET::Icons::ZoomFitBest,
				 tr("Ajuster à la platine"), this);
	m_zoom_fit->setShortcut(QKeySequence(QStringLiteral("Ctrl+9")));
	connect(m_zoom_fit, &QAction::triggered, m_view, &MountingView::zoomFit);

	m_zoom_actual = new QAction(QET::Icons::ZoomOriginal,
				    tr("Échelle 1:1 (1 mm = 1 px)"), this);
	m_zoom_actual->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
	connect(m_zoom_actual, &QAction::triggered,
		m_view, &MountingView::zoomActualSize);

	m_close = new QAction(tr("Fermer"), this);
	m_close->setShortcuts(QKeySequence::Close);
	connect(m_close, &QAction::triggered, this, &QWidget::close);
}

/**
	@brief MountingLayoutEditor::buildWidgets
*/
void MountingLayoutEditor::buildWidgets()
{
	setObjectName(QStringLiteral("mounting_layout_editor"));
	setCentralWidget(m_view);

	m_surface_box = new QComboBox(this);
	m_surface_box->setMinimumWidth(220);
	m_surface_box->setToolTip(tr("La face implantée"));
	connect(m_surface_box,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &MountingLayoutEditor::surfaceChosen);

	QMenu *layout_menu = menuBar()->addMenu(tr("&Calepinage"));
	layout_menu->addAction(m_new_surface);
	layout_menu->addAction(m_add_part);
	layout_menu->addSeparator();
	layout_menu->addAction(m_close);

	QMenu *edit_menu = menuBar()->addMenu(tr("&Édition"));
	edit_menu->addAction(m_undo);
	edit_menu->addAction(m_redo);

	QMenu *view_menu = menuBar()->addMenu(tr("&Affichage"));
	view_menu->addAction(m_zoom_in);
	view_menu->addAction(m_zoom_out);
	view_menu->addAction(m_zoom_fit);
	view_menu->addAction(m_zoom_actual);

	QToolBar *bar = addToolBar(tr("Calepinage"));
	bar->setObjectName(QStringLiteral("mounting_layout_toolbar"));
	bar->addWidget(new QLabel(tr("Platine : "), bar));
	bar->addWidget(m_surface_box);
	bar->addSeparator();
	bar->addAction(m_new_surface);
	bar->addAction(m_add_part);
	bar->addSeparator();
	bar->addAction(m_undo);
	bar->addAction(m_redo);
	bar->addSeparator();
	bar->addAction(m_zoom_in);
	bar->addAction(m_zoom_out);
	bar->addAction(m_zoom_fit);
	bar->addAction(m_zoom_actual);

	m_position_label = new QLabel(this);
	m_zoom_label = new QLabel(this);
	statusBar()->addPermanentWidget(m_position_label);
	statusBar()->addPermanentWidget(m_zoom_label);

	pointedAt(QPointF(0.0, 0.0));
	zoomTold(m_view->pixelsPerMillimetre());

	resize(900, 700);
}

/**
	@brief MountingLayoutEditor::refreshSurfaceList
	Fill the list of faces from the project.

	Filling it must choose nothing: setting the rows of a combo box makes it
	report that its current row changed, and answering that report would
	draw another face than the one being looked at.
*/
void MountingLayoutEditor::refreshSurfaceList()
{
	m_filling = true;
	m_surface_box->clear();

	const MountingLayout layout = m_project ? m_project->mountingLayout()
						: MountingLayout();
	const QStringList faces = layout.surfaceUuids();

	if (faces.isEmpty()) {
		m_surface_box->addItem(tr("(aucune platine)"), QString());
	}

	for (const QString &uuid : faces)
	{
		const MountingSurface surface = layout.surface(uuid);
		m_surface_box->addItem(tr("%1 — %2 composant(s)")
				       .arg(surface.designation())
				       .arg(surface.itemCount()),
				       uuid);
	}

	const int index = m_surface_box->findData(m_shown);
	if (index >= 0) {
		m_surface_box->setCurrentIndex(index);
	}

	m_filling = false;
}

/**
	@brief MountingLayoutEditor::updateTitle
*/
void MountingLayoutEditor::updateTitle()
{
	const QString project_title = m_project ? m_project->title() : QString();
	const QString named = project_title.isEmpty() ? tr("projet sans titre")
						      : project_title;

	if (m_shown.isEmpty())
	{
		setWindowTitle(tr("Calepinage — %1").arg(named));
		return;
	}

	setWindowTitle(tr("Calepinage : %1 — %2")
		       .arg(m_scene->surface().designation(), named));
}

/**
	@brief MountingLayoutEditor::updateActions
*/
void MountingLayoutEditor::updateActions()
{
	const bool editable = isEditable();

	m_new_surface->setEnabled(editable);
	m_add_part->setEnabled(editable && !m_shown.isEmpty());

	const int index = m_surface_box->findData(m_shown);
	if (index >= 0 && index != m_surface_box->currentIndex())
	{
		m_filling = true;
		m_surface_box->setCurrentIndex(index);
		m_filling = false;
	}
}

/**
	@brief MountingLayoutEditor::say
	@param message what to tell the person
	@param problem true when it is a refusal rather than a report

	A refusal stays until something replaces it. A report fades, because a
	status bar still holding this morning's good news is a status bar nobody
	reads.
*/
void MountingLayoutEditor::say(const QString &message, bool problem)
{
	if (message.isEmpty()) {
		return;
	}

	statusBar()->showMessage(message, problem ? 0 : MESSAGE_LIFE);
}

/**
	@brief MountingLayoutEditor::freePosition
	@return where the next part dropped on this face lands, millimetre

	A cascade, and not a search for room: finding a free rectangle is the
	business of the check that already knows what overlaps what, and
	answering it here would be a second opinion on the same question. What
	this promises is only that two parts dropped one after the other do not
	land exactly on top of each other, which is what makes the second one
	possible to grab.
*/
QPointF MountingLayoutEditor::freePosition() const
{
	const int step = m_scene->partCount() % DROP_WRAP;
	const qreal offset = FIRST_DROP + DROP_STEP * step;

	return QPointF(offset, offset);
}

/**
	@brief MountingLayoutEditor::isEditable
	@return true when there is a project and it accepts being written to
*/
bool MountingLayoutEditor::isEditable() const
{
	return m_project && !m_project->isReadOnly();
}

/**
	@brief MountingLayoutEditor::readSettings
*/
void MountingLayoutEditor::readSettings()
{
	QSettings settings;

	const QVariant geometry = settings.value("mountinglayouteditor/geometry");
	if (geometry.isValid()) {
		restoreGeometry(geometry.toByteArray());
	}

	const QVariant state = settings.value("mountinglayouteditor/state");
	if (state.isValid() && !restoreState(state.toByteArray())) {
		settings.remove("mountinglayouteditor/state");
	}
}

/**
	@brief MountingLayoutEditor::writeSettings
*/
void MountingLayoutEditor::writeSettings() const
{
	QSettings settings;
	settings.setValue("mountinglayouteditor/geometry", saveGeometry());
	settings.setValue("mountinglayouteditor/state", saveState());
}
