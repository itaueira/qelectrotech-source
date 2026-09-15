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
#include "../mountingmeasure.h"
#include "../mountingpartview.h"
#include "../ui/drillingtabledialog.h"
#include "../ui/mountingcheckdialog.h"
#include "mountedpartitem.h"
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

		/*
			Laid on the drawing and not written into the project
			here. The scene takes one part in without rebuilding
			itself, so the undo stack survives adding something -
			and the step it pushes is what carries the part into the
			project, down the same wire every drag already goes.

			It used to be the other way round: the project was
			written first and the whole face redrawn from it, which
			dropped the stack every time somebody added a part.
		*/
	QString error;
	const QString mounted = mountOnShownSurface(item, &error);

	if (mounted.isEmpty())
	{
		say(error, true);
		return;
	}

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
	@brief MountingLayoutEditor::newProfile
	Ask for a piece of rail or of duct and lay it on the plate.

	The box opens where the last one was left, profile and length included,
	because nobody lays a panel out with four different rails: the second
	one is almost always the first one again. The very first one starts on
	the standard rail of every panel and as long as the plate is wide, which
	are the two numbers somebody would have typed anyway - and both are in
	front of them, in a box, to be changed.
*/
void MountingLayoutEditor::newProfile()
{
	if (!isEditable() || m_shown.isEmpty()) {
		return;
	}

	const QList<MountingProfile> rails = MountingProfile::standardRails();

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Poser un rail ou une goulotte"));

	QComboBox *standard_box = new QComboBox(&dialog);
	for (int index = 0 ; index < rails.count() ; ++ index) {
		standard_box->addItem(rails.at(index).designation(), index);
	}
	standard_box->addItem(tr("Autre (à préciser ci-dessous)"), -1);

	QComboBox *kind_box = new QComboBox(&dialog);
	kind_box->addItem(tr("Rail"), MountingProfile::railKind());
	kind_box->addItem(tr("Goulotte"), MountingProfile::ductKind());

	QDoubleSpinBox *section_box = new QDoubleSpinBox(&dialog);
	section_box->setRange(0.0, 1000.0);
	section_box->setDecimals(1);
	section_box->setSuffix(tr(" mm"));
	section_box->setToolTip(tr("La largeur que le profilé occupe sur la "
				   "platine, en travers de sa longueur."));

	QDoubleSpinBox *depth_box = new QDoubleSpinBox(&dialog);
	depth_box->setRange(0.0, 1000.0);
	depth_box->setDecimals(1);
	depth_box->setSuffix(tr(" mm"));
	depth_box->setSpecialValueText(tr("non mesurée"));
	depth_box->setToolTip(tr("La hauteur du profilé au-dessus de la "
				 "platine. Elle ne se dessine pas ici, elle "
				 "compte pour la porte."));

	QDoubleSpinBox *length_box = new QDoubleSpinBox(&dialog);
	length_box->setRange(0.0, 10000.0);
	length_box->setDecimals(1);
	length_box->setSuffix(tr(" mm"));

	QComboBox *run_box = new QComboBox(&dialog);
	run_box->addItem(tr("Horizontal"), 0);
	run_box->addItem(tr("Vertical"), 1);

		//Where the box opens. The last piece if there was one, the
		//standard rail of every panel if there was not.
	const MountingProfile opening = m_last_profile.isNull()
					? (rails.isEmpty() ? MountingProfile()
							   : rails.first())
					: m_last_profile;

	kind_box->setCurrentIndex(opening.isDuct() ? 1 : 0);
	section_box->setValue(opening.section);
	depth_box->setValue(opening.depth);
	run_box->setCurrentIndex(m_last_run == MountingRun::Down ? 1 : 0);

	const int known = standard_box->findText(opening.designation());
	standard_box->setCurrentIndex(known >= 0 ? known
						 : standard_box->count() - 1);

		//As long as the plate is wide, less the margin the first part
		//is dropped at on each side. It is a number in front of
		//somebody, in a box, and not a measurement invented behind
		//their back - and a plate nobody has measured proposes nothing
		//of the sort.
	const qreal proposed = m_last_length > 0.0
			       ? m_last_length
			       : (m_scene->isAreaMeasured()
				  ? qMax(MountingProfile::minimumLength(),
					 m_scene->area().width - 2.0 * FIRST_DROP)
				  : 0.0);
	length_box->setValue(proposed);

	connect(standard_box, QOverload<int>::of(&QComboBox::currentIndexChanged),
		&dialog, [&](int index)
	{
		const int chosen = standard_box->itemData(index).toInt();
		if (chosen < 0 || chosen >= rails.count()) {
			return;
		}

		kind_box->setCurrentIndex(0);
		section_box->setValue(rails.at(chosen).section);
		depth_box->setValue(rails.at(chosen).depth);
	});

	QDialogButtonBox *buttons = new QDialogButtonBox(
				QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
				&dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	QFormLayout *form = new QFormLayout(&dialog);
	form->addRow(tr("Profilé courant :"), standard_box);
	form->addRow(tr("Type :"), kind_box);
	form->addRow(tr("Largeur :"), section_box);
	form->addRow(tr("Hauteur :"), depth_box);
	form->addRow(tr("Longueur coupée :"), length_box);
	form->addRow(tr("Sens :"), run_box);
	form->addRow(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	const MountingProfile profile(kind_box->currentData().toString(),
				      section_box->value(),
				      depth_box->value());
	const MountingRun run = run_box->currentData().toInt() == 1
				? MountingRun::Down
				: MountingRun::Across;

	QString error;
	const QString laid = addProfile(profile, length_box->value(), run,
					&error);

	if (laid.isEmpty()) {
		say(error, true);
	}
	else {
		say(tr("%1 posé sur la platine.").arg(profile.designation()));
	}
}

/**
	@brief MountingLayoutEditor::addProfile
	@param profile the bar the piece is cut from
	@param length_mm how long the piece is, millimetre
	@param run which way it runs
	@param error filled with why nothing was laid
	@return the identifier of the piece, empty when refused
*/
QString MountingLayoutEditor::addProfile(const MountingProfile &profile,
					 qreal length_mm,
					 MountingRun run,
					 QString *error)
{
	if (error) {
		error->clear();
	}

	if (profile.isNull())
	{
		if (error) {
			*error = tr("Dites si c'est un rail ou une goulotte : "
				    "un profilé sans type ne se pose pas.");
		}
		return QString();
	}

	if (!profile.hasSection())
	{
			//Refused, and it is one of the few places this family
			//refuses a missing measurement rather than reporting it.
			//The reason is that this one has nothing to report with:
			//a piece of no width is a piece nothing is drawn of, and
			//an invisible piece on a plate is worse than no piece.
		if (error) {
			*error = tr("Un profilé a besoin de sa largeur : sans "
				    "elle, rien ne se dessine sur la platine.");
		}
		return QString();
	}

	if (!MountingMeasure::isLength(length_mm)
	    || MountingMeasure::isSameLength(length_mm, 0.0))
	{
		if (error) {
			*error = tr("Une longueur de coupe est un nombre de "
				    "millimètres.");
		}
		return QString();
	}

	MountedItem item;
	item.profile  = profile;
	item.run      = run;
	item.position = freePosition();
	item.size     = profile.sizeFor(length_mm, run);

	const QString laid = mountOnShownSurface(item, error);

	if (!laid.isEmpty())
	{
		m_last_profile = profile;
		m_last_length  = length_mm;
		m_last_run     = run;
	}

	return laid;
}

/**
	@brief MountingLayoutEditor::mountOnShownSurface
	@param item what is mounted, identity optional
	@param error filled with why nothing was mounted
	@return the identifier of what was mounted, empty when refused

	The identity is handed out here, by the layout and not by the drawing:
	MountingLayout::newId is what the whole project uses, and a window that
	made its own would be a second source of identity for the same kind of
	thing.

	Nothing is written into the project from here. The step pushed on the
	stack is what does that, through stepApplied - which is the same wire a
	drag goes down, and having one wire is what makes a part that was laid
	reach the file exactly as surely as a part that was moved.
*/
QString MountingLayoutEditor::mountOnShownSurface(MountedItem item,
						  QString *error)
{
	if (error) {
		error->clear();
	}

	if (!isEditable() || m_shown.isEmpty())
	{
		if (error) {
			*error = tr("Ce projet ne se modifie pas.");
		}
		return QString();
	}

	if (item.uuid.isEmpty()) {
		item.uuid = MountingLayout::newId();
	}

	if (!m_scene->mountItem(item, error)) {
		return QString();
	}

	refreshSurfaceList();
	updateTitle();
	updateActions();

	return item.uuid;
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
	@brief MountingLayoutEditor::openDrillingTable
	@return the drilling table of the face being laid out

	Not modal, kept in a guarded pointer and asked for again rather than
	built again: a second copy of the same worklist on top of the first is
	two documents that disagree the moment a part moves.

	The plate is written into the project before the window is given
	anything, because that is where the window reads from. Everything this
	editor does reaches the project already - after every step of the
	stack - so in the ordinary case this finds nothing to write; it is here
	for the case where it does.
*/
DrillingTableDialog *MountingLayoutEditor::openDrillingTable()
{
	commitToProject();

	if (!m_drilling_dialog)
	{
		m_drilling_dialog = new DrillingTableDialog(m_project.data(),
							   this);
		m_drilling_dialog->setAttribute(Qt::WA_DeleteOnClose);
		connect(m_drilling_dialog.data(),
			&DrillingTableDialog::goToComponent,
			this, &MountingLayoutEditor::pointAtItem);
	}

	m_drilling_dialog->showSurface(m_shown);
	m_drilling_dialog->show();
	m_drilling_dialog->raise();
	m_drilling_dialog->activateWindow();

	return m_drilling_dialog.data();
}

/**
	@brief MountingLayoutEditor::openPlateCheck
	@return the check of the face being laid out
*/
MountingCheckDialog *MountingLayoutEditor::openPlateCheck()
{
	commitToProject();

	if (!m_check_dialog)
	{
		m_check_dialog = new MountingCheckDialog(m_project.data(),
							 QETApp::catalog(),
							 this);
		m_check_dialog->setAttribute(Qt::WA_DeleteOnClose);
		connect(m_check_dialog.data(), &MountingCheckDialog::goToItem,
			this, &MountingLayoutEditor::pointAtItem);
	}

	m_check_dialog->showSurface(m_shown);
	m_check_dialog->show();
	m_check_dialog->raise();
	m_check_dialog->activateWindow();

	return m_check_dialog.data();
}

/**
	@brief MountingLayoutEditor::pointAtItem
	@param item_uuid which part

	The face is changed first when the part is on another one. A report
	that named a part and left the drawing showing a different plate would
	be sending a person to look for something that is not on the screen,
	which is worse than saying nothing: the part is there, on the plate
	being shown, and it is not.
*/
void MountingLayoutEditor::pointAtItem(const QString &item_uuid)
{
	if (item_uuid.isEmpty()) {
		return;
	}

	if (m_project)
	{
		const QString face = m_project->mountingLayout()
				     .surfaceOfItem(item_uuid);
		if (!face.isEmpty() && face != m_shown) {
			showSurface(face);
		}
	}

	MountedPartItem *part = m_scene->partItem(item_uuid);
	if (!part)
	{
			//Said rather than passed over. The identity came from a
			//list built out of this very project, so a part that
			//cannot be found is news - about a hole whose component
			//was never mounted, most likely - and swallowing it
			//would leave a double click that does nothing at all.
		say(tr("That part is not mounted on any plate of this "
		       "project."), true);
		return;
	}

	m_scene->clearSelection();
	part->setSelected(true);
	m_view->centerOn(part);
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

	m_add_profile = new QAction(tr("Ajouter un rail ou une goulotte…"), this);
	m_add_profile->setStatusTip(tr("Pose un profilé coupé à la longueur "
				       "voulue. Une fois posé, il se rallonge "
				       "et se raccourcit par ses extrémités."));
	connect(m_add_profile, &QAction::triggered,
		this, &MountingLayoutEditor::newProfile);

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

	m_check_plate = new QAction(tr("Check the plate…"), this);
	m_check_plate->setStatusTip(tr("What is wrong with this face before a "
				       "hole is drilled in it: two parts in "
				       "the same room, air a part asked for, "
				       "parts off the plate."));
	connect(m_check_plate, &QAction::triggered,
		this, &MountingLayoutEditor::openPlateCheck);

	m_drilling_table = new QAction(tr("Drilling table…"), this);
	m_drilling_table->setStatusTip(tr("The holes of this face as the "
					  "worklist the bench drills from, "
					  "with the corner they are measured "
					  "from written at the head of it."));
	connect(m_drilling_table, &QAction::triggered,
		this, &MountingLayoutEditor::openDrillingTable);

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
	layout_menu->addAction(m_add_profile);
	layout_menu->addSeparator();
	layout_menu->addAction(m_check_plate);
	layout_menu->addAction(m_drilling_table);
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
	bar->addAction(m_add_profile);
	bar->addSeparator();
	bar->addAction(m_check_plate);
	bar->addAction(m_drilling_table);
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
	m_add_profile->setEnabled(editable && !m_shown.isEmpty());

		//Reading, not writing: a project open read only is exactly the
		//project somebody opens to check a plate against, and refusing
		//to say what is wrong with it because it cannot be edited
		//would be refusing the only thing that can still be done with
		//it.
	m_check_plate->setEnabled(!m_shown.isEmpty());
	m_drilling_table->setEnabled(!m_shown.isEmpty());

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
