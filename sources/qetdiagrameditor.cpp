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
#include "qetdiagrameditor.h"

#include "ElementsCollection/sheetsymbolextractor.h"
#include "ElementsCollection/ui/createsymboldialog.h"
#include "ElementsCollection/ui/symbolgroupdialog.h"

#include "autoNum/ui/iecstructuredialog.h"
#include "autoNum/ui/renumberdialog.h"
#include "catalog/ui/catalogbrowserdialog.h"
#include "undocommand/explodeelementcommand.h"
#include "undocommand/conductortextcommand.h"
#include "catalog/ui/catalogreplacedialog.h"
#include "catalog/ui/catalogimportdialog.h"
#include "catalog/ui/catalogrepositorydialog.h"
#include "environment/projectlock.h"
#include "environment/ui/environmentdialog.h"
#include "catalog/ui/catalogmanagerdialog.h"
#include "catalog/ui/catalogpartdialog.h"
#include "catalog/ui/catalogprojectactions.h"
#include <QCoreApplication>
#include "ElementsCollection/elementscollectionwidget.h"
#include "QWidgetAnimation/qwidgetanimation.h"
#include "autoNum/ui/autonumberingdockwidget.h"
#include "conductornumexport.h"
#include "diagramcommands.h"
#include "diagramevent/diagrameventaddimage.h"
#include "diagramevent/diagrameventaddshape.h"
#include "diagramevent/diagrameventaddtext.h"
#include "diagramview.h"
#include "elementspanelwidget.h"
#include "factory/qetgraphicstablefactory.h"
#include "print/projectprintwindow.h"
#include "project/projectpropertieshandler.h"
#include "projectview.h"
#include "qetproject.h"
#include "qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qeticons.h"
#include "qetmessagebox.h"
#include "recentfiles.h"
#include "shortcutmanager.h"
#include "ui/bomexportdialog.h"
#include "ui/jumptoelementdialog.h"
#include "ui/diagrampropertieseditordockwidget.h"
#include "ui/backupdialog.h"
#include "ui/dialogwaiting.h"
#include "undocommand/addelementtextcommand.h"
#include "utils/qetutils.h"
#include "undocommand/rotateselectioncommand.h"
#include "undocommand/rotatetextscommand.h"
#include "diagram.h"
#include "TerminalStrip/ui/terminalstripeditorwindow.h"
#include "ui/diagrameditorhandlersizewidget.h"
#include "TerminalStrip/ui/addterminalstripitemdialog.h"
#include "wiringlistexport.h"
#include "ui/terminalnumberingdialog.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#ifdef BUILD_WITHOUT_KF
#	include "ui/nokde/kautosavefile.h"
#else
#	include <KAutoSaveFile>
#endif

/**
	@brief QETDiagramEditor::QETDiagramEditor
	Constructor
	@param files : list of files to open
	@param parent : parent widget
*/
QETDiagramEditor::QETDiagramEditor(const QStringList &files, QWidget *parent) :
	QETMainWindow(parent),
	m_row_column_actions_group (this),
	m_selection_actions_group  (this),
	m_add_item_actions_group   (this),
	m_zoom_actions_group       (this),
	m_select_actions_group     (this),
	m_file_actions_group       (this),
	open_dialog_dir            (QETApp::documentDir())
{
		//Trivial property use to set the graphics handler size
	setProperty("graphics_handler_size", 10);

	activeSubWindowIndex = 0;

	QSplitter *splitter_ = new QSplitter(this);
	splitter_->setChildrenCollapsible(false);
	splitter_->setOrientation(Qt::Vertical);
	splitter_->addWidget(&m_workspace);
	splitter_->addWidget(&m_search_and_replace_widget);
	setCentralWidget(splitter_);
	m_search_and_replace_widget.setEditor(this);

	QList<int> s;
	s << m_workspace.maximumHeight() << m_search_and_replace_widget.minimumSizeHint().height();
	splitter_->setSizes(s); //Force the size of the search and replace widget, force have a good animation the first time he is showed

	auto anim = new QWidgetAnimation(&m_search_and_replace_widget, Qt::Vertical, QWidgetAnimation::lastSize, 250);
	anim->setObjectName("search and replace animator");
	m_search_and_replace_widget.setHidden(true);
	anim->setLastShowSize(m_search_and_replace_widget.minimumSizeHint().height());

		//Set object name to be retrieved by the stylesheets
	m_workspace.setBackground(QBrush(Qt::NoBrush));
	m_workspace.setObjectName("mdiarea");
	m_workspace.setTabsClosable(true);

		//Set the signal mapper
	connect(&windowMapper, &QSignalMapper::mappedObject, this, [this](QObject *object) { activateWidget(qobject_cast<QWidget *>(object)); });

	setWindowTitle(tr("QElectroTech", "window title"));
	setWindowIcon(QET::Icons::QETLogo);
	statusBar() -> showMessage(tr("QElectroTech", "status bar message"));

	setUpElementsPanel();
	setUpElementsCollectionWidget();
	setUpUndoStack();
	setUpSelectionPropertiesEditor();
	setUpAutonumberingWidget();

	setUpActions();
	setUpToolBar();
	setUpMenu();

	tabifyDockWidget(qdw_undo, qdw_pa);

		//By default the windows is maximised
	setMinimumSize(QSize(500, 350));
	setWindowState(Qt::WindowMaximized);

	connect(&m_workspace, &QMdiArea::subWindowActivated, this, &QETDiagramEditor::subWindowActivated);
	connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &QETDiagramEditor::slot_updatePasteAction);

	readSettings();
	show();

		//If valid file path is given as arguments
	uint opened_projects = 0;
	if (files.count())
	{
			//So we open this files
		foreach(QString file, files)
			if (openAndAddProject(file))
				++ opened_projects;
	}

	slot_updateActions();
}

/**
	Destructeur
*/
QETDiagramEditor::~QETDiagramEditor()
{
}

/**
	@brief QETDiagramEditor::setUpElementsPanel
	Setup the element panel and element panel widget
*/
void QETDiagramEditor::setUpElementsPanel()
{
	//Add the element panel as a QDockWidget
	qdw_pa = new QDockWidget(tr("Projets", "dock title"), this);

	qdw_pa -> setObjectName   ("projects panel");
	qdw_pa -> setAllowedAreas (Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	qdw_pa -> setFeatures     (
				QDockWidget::DockWidgetClosable
				|QDockWidget::DockWidgetMovable
				|QDockWidget::DockWidgetFloatable);
	qdw_pa -> setMinimumWidth (160);
	qdw_pa -> setWidget       (pa = new ElementsPanelWidget(qdw_pa));

	addDockWidget(Qt::LeftDockWidgetArea, qdw_pa);

	connect(pa, &ElementsPanelWidget::requestForProject, this, qOverload<QETProject*>(&QETDiagramEditor::activateProject));
	connect(pa, &ElementsPanelWidget::requestForProjectClosing, this, qOverload<QETProject*>(&QETDiagramEditor::closeProject));
	connect(pa, &ElementsPanelWidget::requestForProjectPropertiesEdition, this, qOverload<QETProject*>(&QETDiagramEditor::editProjectProperties));
	connect(pa, &ElementsPanelWidget::requestForNewDiagram, this, &QETDiagramEditor::addDiagramToProject);
	connect(pa, &ElementsPanelWidget::requestForNewDiagramAt, this, &QETDiagramEditor::addDiagramToProjectAt);
	connect(pa, &ElementsPanelWidget::requestForDiagramPropertiesEdition, this, qOverload<Diagram*>(&QETDiagramEditor::editDiagramProperties));
	connect(pa, &ElementsPanelWidget::requestForDiagramsDeletion, this, &QETDiagramEditor::removeDiagrams);
	connect(pa, &ElementsPanelWidget::requestForDiagramMoveUp, this, &QETDiagramEditor::moveDiagramUp);
	connect(pa, &ElementsPanelWidget::requestForDiagramMoveDown, this, &QETDiagramEditor::moveDiagramDown);
	connect(pa, &ElementsPanelWidget::requestForDiagramMoveUpTop, this, &QETDiagramEditor::moveDiagramUpTop);
	connect(pa, &ElementsPanelWidget::requestForDiagramMoveUpx10, this, &QETDiagramEditor::moveDiagramUpx10);
	connect(pa, &ElementsPanelWidget::requestForDiagramMoveDownx10, this, &QETDiagramEditor::moveDiagramDownx10);
	connect(pa, &ElementsPanelWidget::requestForDiagramMoveUpx100, this, &QETDiagramEditor::moveDiagramUpx100);
	connect(pa, &ElementsPanelWidget::requestForDiagramMoveDownx100, this, &QETDiagramEditor::moveDiagramDownx100);
}

/**
	@brief QETDiagramEditor::setUpElementsCollectionWidget
	Set up the dock widget of element collection
*/
void QETDiagramEditor::setUpElementsCollectionWidget()
{
	m_qdw_elmt_collection = new QDockWidget(tr("Collections"), this);
	m_qdw_elmt_collection->setObjectName("elements_collection_widget");
	m_qdw_elmt_collection->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_qdw_elmt_collection->setFeatures(
				QDockWidget::DockWidgetClosable
				|QDockWidget::DockWidgetMovable
				|QDockWidget::DockWidgetFloatable);

	m_element_collection_widget = new ElementsCollectionWidget(m_qdw_elmt_collection);
	m_qdw_elmt_collection->setWidget(m_element_collection_widget);
	m_element_collection_widget->expandFirstItems();

	addDockWidget(Qt::RightDockWidgetArea, m_qdw_elmt_collection);
}

/**
	@brief QETDiagramEditor::setUpUndoStack
	Setup the undostack and undo stack widget
*/
void QETDiagramEditor::setUpUndoStack()
{

	QUndoView *undo_view = new QUndoView(&undo_group, this);

	undo_view -> setEmptyLabel (tr("Aucune modification"));
	undo_view -> setStatusTip  (tr("Cliquez sur une action pour revenir en arrière dans l'édition de votre schéma", "Status tip"));
	undo_view -> setWhatsThis  (tr("Ce panneau liste les différentes actions effectuées sur le folio courant. Cliquer sur une action permet de revenir à l'état du schéma juste après son application.", "\"What's this\" tip"));

	qdw_undo  = new QDockWidget(tr("Annulations", "dock title"), this);
	qdw_undo -> setObjectName("diagram_undo");

	qdw_undo -> setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	qdw_undo -> setFeatures(
				QDockWidget::DockWidgetClosable
				|QDockWidget::DockWidgetMovable
				|QDockWidget::DockWidgetFloatable);
	qdw_undo -> setMinimumWidth(160);
	qdw_undo -> setWidget(undo_view);

	addDockWidget(Qt::LeftDockWidgetArea, qdw_undo);
}

/**
	@brief QETDiagramEditor::setUpSelectionPropertiesEditor
	Setup the dock for edit the current selection
*/
void QETDiagramEditor::setUpSelectionPropertiesEditor()
{
	m_selection_properties_editor = new DiagramPropertiesEditorDockWidget(this);
	m_selection_properties_editor -> setObjectName("diagram_properties_editor_dock_widget");
	addDockWidget(Qt::RightDockWidgetArea, m_selection_properties_editor);
}

/**
	@brief QETDiagramEditor::setUpAutonumberingWidget
	Setup the dock for AutoNumbering Selection
*/
void QETDiagramEditor::setUpAutonumberingWidget()
{
	m_autonumbering_dock = new AutoNumberingDockWidget(this);
	m_autonumbering_dock -> setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_autonumbering_dock -> setFeatures(
				QDockWidget::DockWidgetClosable
				|QDockWidget::DockWidgetMovable
				|QDockWidget::DockWidgetFloatable);
	addDockWidget(Qt::RightDockWidgetArea, m_autonumbering_dock);
}

/**
	@brief QETDiagramEditor::setUpActions
	Set up all Qaction
*/
void QETDiagramEditor::setUpActions()
{
		//Export to another file type (jpeg, dxf etc...)
	m_export_to_images = new QAction(QET::Icons::DocumentExport,  tr("E&xporter"), this);
	ShortcutManager::instance().registerAction(m_export_to_images, "diagrameditor.export_to_images", tr("Éditeur de schémas"), Qt::CTRL | Qt::SHIFT | Qt::Key_X);
	m_export_to_images->setStatusTip(tr("Exporte le folio courant dans un autre format", "status bar tip"));
	connect(m_export_to_images, &QAction::triggered, [this]() {
		ProjectView *current_project = currentProjectView();
		if (current_project) {
			current_project -> exportProject();
		}
	});

		//Print
	m_print = new QAction(QET::Icons::DocumentPrint,   tr("Imprimer"),  this);
	ShortcutManager::instance().registerAction(m_print, "diagrameditor.print", tr("Éditeur de schémas"), QKeySequence::Print);
	m_print->setStatusTip(tr("Imprime un ou plusieurs folios du projet courant", "status bar tip"));
	connect(m_print, &QAction::triggered, [this]() {
		auto project = currentProject();
		if (project) {
			ProjectPrintWindow::launchDialog(project, QPrinter::NativeFormat ,this);
		}
	});

		//export to pdf
	m_export_to_pdf = new QAction(QET::Icons::PDF, tr("Exporter en pdf"), this);
	m_export_to_pdf->setStatusTip(tr("Exporte un ou plusieurs folios du projet courant", "status bar tip"));
	connect(m_export_to_pdf, &QAction::triggered, [this] () {
		auto project = currentProject();
		if (project) {
			ProjectPrintWindow::launchDialog(project, QPrinter::PdfFormat, this);
		}
	});

		//Quit editor
	m_quit_editor = new QAction(QET::Icons::ApplicationExit, tr("&Quitter"),  this);
	ShortcutManager::instance().registerAction(m_quit_editor, "diagrameditor.quit", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_Q);
	m_quit_editor->setStatusTip(tr("Ferme l'application QElectroTech", "status bar tip"));
	connect(m_quit_editor, &QAction::triggered, this, &QETDiagramEditor::close);

		//Undo
	undo = undo_group.createUndoAction(this, tr("Annuler"));
	undo->setIcon(QET::Icons::EditUndo);
	ShortcutManager::instance().registerAction(undo, "diagrameditor.undo", tr("Éditeur de schémas"), QKeySequence::Undo);
	undo->setStatusTip(tr("Annule l'action précédente", "status bar tip"));
		//Redo
	redo = undo_group.createRedoAction(this, tr("Refaire"));
	redo->setIcon(QET::Icons::EditRedo);
	ShortcutManager::instance().registerAction(redo, "diagrameditor.redo", tr("Éditeur de schémas"), QKeySequence::Redo);
	redo->setStatusTip(tr("Restaure l'action annulée", "status bar tip"));

		//cut copy past
	m_cut   = new QAction(QET::Icons::EditCut,   tr("Co&uper"), this);
	m_copy  = new QAction(QET::Icons::EditCopy,  tr("Cop&ier"), this);
	m_paste = new QAction(QET::Icons::EditPaste, tr("C&oller"), this);

	ShortcutManager::instance().registerAction(m_cut, "diagrameditor.cut", tr("Éditeur de schémas"), QKeySequence::Cut);
	ShortcutManager::instance().registerAction(m_copy, "diagrameditor.copy", tr("Éditeur de schémas"), QKeySequence::Copy);
	ShortcutManager::instance().registerAction(m_paste, "diagrameditor.paste", tr("Éditeur de schémas"), QKeySequence::Paste);

	m_cut   -> setStatusTip(tr("Transfère les éléments sélectionnés dans le presse-papier", "status bar tip"));
	m_copy  -> setStatusTip(tr("Copie les éléments sélectionnés dans le presse-papier", "status bar tip"));
	m_paste -> setStatusTip(tr("Place les éléments du presse-papier sur le folio", "status bar tip"));

	connect(m_cut, &QAction::triggered, [this]() {
		if (currentDiagramView())
			currentDiagramView()->cut();
	});
	connect(m_copy, &QAction::triggered, [this]() {
		if (currentDiagramView())
			currentDiagramView()->copy();
	});
	connect(m_paste, &QAction::triggered, [this]() {
		if(currentDiagramView())
			currentDiagramView()->paste();
	});

		//Reset conductor path
	m_conductor_reset = new QAction(QET::Icons::ConductorSettings,     tr("Réinitialiser les conducteurs"),        this);
	ShortcutManager::instance().registerAction(m_conductor_reset, "diagrameditor.conductor_reset", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_K);
	m_conductor_reset->setStatusTip(tr("Recalcule les chemins des conducteurs sans tenir compte des modifications", "status bar tip"));
	connect(m_conductor_reset, &QAction::triggered, [this]() {
		if (DiagramView *dv = currentDiagramView())
			dv->resetConductors();
	});

		//AutoConductor
	m_auto_conductor = new QAction   (QET::Icons::Autoconnect, tr("Création automatique de conducteur(s)","Tool tip of auto conductor"), this);
	m_auto_conductor->setStatusTip (tr("Utiliser la création automatique de conducteur(s) quand cela est possible", "Status tip of auto conductor"));
	m_auto_conductor->setCheckable (true);
	connect(m_auto_conductor, &QAction::triggered, [this](bool ac) {
		if (ProjectView *pv = currentProjectView())
			pv->project()->setAutoConductor(ac);
	});

		//AutoBreakConductor
	m_auto_break_conductor = new QAction   (QET::Icons::Conductor, tr("Coupure automatique de conducteur(s)","Tool tip of auto break conductor"), this);
	m_auto_break_conductor->setStatusTip (tr("Couper automatiquement les conducteurs existants lors du placement d'un élément", "Status tip of auto break conductor"));
	m_auto_break_conductor->setCheckable (true);
	{
		QSettings settings;
		m_auto_break_conductor->setChecked(settings.value("diagrameditor/auto_break_conductor", false).toBool());
	}
	connect(m_auto_break_conductor, &QAction::triggered, [this](bool abc) {
		QSettings settings;
		settings.setValue("diagrameditor/auto_break_conductor", abc);
		if (ProjectView *pv = currentProjectView())
			pv->project()->setAutoBreakConductor(abc);
	});

		//Switch background color
	m_grey_background = new QAction   (QET::Icons::DiagramBg, tr("Couleur de fond blanc/gris","Tool tip of white/grey background button"), this);
	m_grey_background -> setStatusTip (tr("Affiche la couleur de fond du folio en blanc ou en gris", "Status tip of white/grey background button"));
	m_grey_background -> setCheckable (true);
	connect (m_grey_background, &QAction::triggered, [this](bool checked) {
		Diagram::background_color = checked ? Qt::darkGray : Qt::white;
		if (this->currentDiagramView() &&  this->currentDiagramView()->diagram())
			this->currentDiagramView()->diagram()->update();
	});

		//Draw or not the background grid
	m_draw_grid = new QAction ( QET::Icons::Grid, tr("Afficher la grille"), this);
	m_draw_grid->setStatusTip(tr("Affiche ou masque la grille des folios"));
	QSettings settings;
	m_draw_grid->setCheckable(true);
	m_draw_grid->setChecked(settings.value("diagrameditor/grid_display_startup", true).toBool());
	connect(m_draw_grid, &QAction::triggered, [this](bool checked) {
		foreach (ProjectView *prjv, this->openedProjects())
			foreach (Diagram *d, prjv->project()->diagrams()) {
				d->setDisplayGrid(checked);
				d->update();
			}
	});

	// Draw or not the custom guides
	m_draw_guides = new QAction ( QIcon(":/ico/22x22/guides.png"), tr("Afficher les guides"), this);
	m_draw_guides->setStatusTip(tr("Affiche ou masque les guides"));
	m_draw_guides->setCheckable(true);
	m_draw_guides->setChecked(settings.value("diagrameditor/guides_display_startup", false).toBool());
	connect(m_draw_guides, &QAction::triggered, [this](bool checked) {
		foreach (ProjectView *prjv, this->openedProjects())
			foreach (Diagram *d, prjv->project()->diagrams()) {
				d->setDisplayGuides(checked);
			}
	});

		//Edit current diagram properties
	m_edit_diagram_properties = new QAction(QET::Icons::DialogInformation, tr("Propriétés du folio"), this);
	ShortcutManager::instance().registerAction(m_edit_diagram_properties, "diagrameditor.edit_diagram_properties", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_L);
	m_edit_diagram_properties     -> setStatusTip(tr("Édite les propriétés du folio (dimensions, informations du cartouche, propriétés des conducteurs...)", "status bar tip"));
	connect(m_edit_diagram_properties, &QAction::triggered, [this]() {
		if (ProjectView *project_view = currentProjectView())
		{
			activateProject(project_view);
			project_view->editCurrentDiagramProperties();
		}
	});

		//Edit current project properties
	m_project_edit_properties = new QAction(QET::Icons::ProjectProperties, tr("Propriétés du projet"), this);
	connect(m_project_edit_properties, &QAction::triggered, [this]() {
		editProjectProperties(currentProjectView());
	});

		//Add new folio to current project
	m_project_add_diagram = new QAction(QET::Icons::DiagramAdd, tr("Ajouter un folio"), this);
	ShortcutManager::instance().registerAction(m_project_add_diagram, "diagrameditor.project_add_diagram", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_T);
	connect(m_project_add_diagram, &QAction::triggered, [this]() {
		if (ProjectView *current_project = currentProjectView()) {
			current_project->project()->addNewDiagram();
		}
	});

		//Remove current folio from current project
	m_remove_diagram_from_project = new QAction(QET::Icons::DiagramDelete, tr("Supprimer le folio"), this);
	connect(m_remove_diagram_from_project, &QAction::triggered, this, &QETDiagramEditor::removeDiagramFromProject);

		//Clean the current project
	m_clean_project         = new QAction(QET::Icons::EditClear,             tr("Nettoyer le projet"),                   this);
	connect(m_clean_project, &QAction::triggered, [this]() {
		if (ProjectView *current_project = currentProjectView()) {
			if (current_project->cleanProject()) {
				pa -> reloadAndFilter();
			}
		}
	});

		//Export nomenclature to CSV
	m_csv_export = new QAction(QET::Icons::DocumentSpreadsheet, tr("Exporter au format CSV"), this);
	connect(m_csv_export, &QAction::triggered, [this]() {
		BOMExportDialog bom(currentProjectView()->project(), this);
		bom.exec();
	});

		//Add a nomenclature item
	m_add_nomenclature = new QAction(QET::Icons::TableOfContent, tr("Ajouter une nomenclature"), this);
	connect(m_add_nomenclature, &QAction::triggered, this, [=]() {
		if(this->currentDiagramView()) {
			QetGraphicsTableFactory::createAndAddNomenclature(this->currentDiagramView()->diagram());
		}
	});

		//Add a summary item
	m_add_summary = new QAction(QET::Icons::TableOfContent, tr("Ajouter un sommaire"), this);
	connect(m_add_summary, &QAction::triggered, this, [=]() {
		if(this->currentDiagramView()) {
			QetGraphicsTableFactory::createAndAddSummary(this->currentDiagramView()->diagram());
		}
	});

	m_terminal_strip_dialog = new QAction(QET::Icons::TerminalStrip, tr("Gestionnaire de borniers (DEV)"), this);
	connect(m_terminal_strip_dialog, &QAction::triggered, this, [=]()
	{
		if (auto project = this->currentProject())
		{
			TerminalStripEditorWindow::instance(project, this)->show();
		}
	});

		//Manage the classes and the properties of the shared catalog. The
		//catalog is shared by the whole office and does not belong to one
		//project, so this action stays enabled even with no project open.
	m_catalog_manager = new QAction(QET::Icons::TableOfContent, tr("Classes et propriétés du catalogue"), this);
	connect(m_catalog_manager, &QAction::triggered, this, [this]()
	{
		CatalogManagerDialog dialog(QETApp::catalog(), this);
		dialog.exec();
	});

	m_catalog_browse = new QAction(QET::Icons::TableOfContent, tr("Parcourir le catalogue"), this);
	connect(m_catalog_browse, &QAction::triggered, this, [this]()
	{
		CatalogBrowserDialog dialog(QETApp::catalog(), this);
		dialog.exec();
	});

		//The most repeated action of the day, so it gets a shortcut - and it
		//goes through the shortcut manager, because what is repeated all day is
		//what people want to rebind.
	m_catalog_assign = new QAction(QET::Icons::TableOfContent, tr("Attribuer une pièce aux composants sélectionnés"), this);
	ShortcutManager::instance().registerAction(m_catalog_assign, "diagrameditor.catalog_assign",
						   tr("Éditeur de schémas"),
						   Qt::CTRL | Qt::SHIFT | Qt::Key_P);
	connect(m_catalog_assign, &QAction::triggered, this, &QETDiagramEditor::assignCatalogPart);

	m_catalog_register = new QAction(QET::Icons::TableOfContent, tr("Enregistrer les composants sélectionnés comme pièce"), this);
	connect(m_catalog_register, &QAction::triggered, this, &QETDiagramEditor::registerCatalogPart);

	m_catalog_missing = new QAction(QET::Icons::TableOfContent, tr("Composants sans pièce"), this);
	connect(m_catalog_missing, &QAction::triggered, this, [this]()
	{
		CatalogProjectActions::showMissingPartReport(this->currentProject(), this);
	});

	m_environment = new QAction(QET::Icons::Configure, tr("Environnement de travail"), this);
	connect(m_environment, &QAction::triggered, this, [this]()
	{
		EnvironmentDialog dialog(this);
		dialog.exec();
	});

	m_catalog_import = new QAction(QET::Icons::DocumentSpreadsheet, tr("Importer des pièces depuis une feuille de calcul"), this);
	connect(m_catalog_import, &QAction::triggered, this, [this]()
	{
		CatalogImportDialog dialog(QETApp::catalog(), this);
		dialog.exec();
	});

	m_catalog_repository = new QAction(QET::Icons::FolderShowAll, tr("Répertoire de pièces partagé"), this);
	connect(m_catalog_repository, &QAction::triggered, this, [this]()
	{
		CatalogRepositoryDialog dialog(QETApp::catalog(), this);
		dialog.exec();
	});

	m_renumber_components = new QAction(tr("Renuméroter les composants"), this);
	connect(m_renumber_components, &QAction::triggered,
		this, &QETDiagramEditor::renumberComponents);

		//Making a symbol on the sheet, with the drawing tools everybody
		//already uses, instead of in a separate program. The separate program
		//is why nobody makes symbols: a similar one gets used and disguised,
		//and the library stops representing what is actually built.
	m_create_symbol = new QAction(QET::Icons::ElementNew,
				      tr("Créer un symbole à partir du dessin sélectionné"),
				      this);
	connect(m_create_symbol, &QAction::triggered,
		this, &QETDiagramEditor::createSymbolFromSelection);

	m_save_group = new QAction(tr("Enregistrer la sélection en groupement"),
				   this);
	connect(m_save_group, &QAction::triggered,
		this, &QETDiagramEditor::saveSelectionAsGroup);

	m_insert_group = new QAction(tr("Insérer un groupement…"), this);
	connect(m_insert_group, &QAction::triggered,
		this, &QETDiagramEditor::insertGroup);

		//The norm is a property of the project, not of the program: two
		//customers, two requirements, the same workstation.
		//The other half of creating a symbol: draw, make a block, and when the
		//block turns out to need a line moved, explode it and make it again.
	m_explode_element = new QAction(
				tr("Éclater le symbole en dessin"), this);
	connect(m_explode_element, &QAction::triggered,
		this, &QETDiagramEditor::explodeSelection);

	m_replace_part = new QAction(
				tr("Remplacer une pièce dans tout le projet…"), this);
	connect(m_replace_part, &QAction::triggered, this, [this]()
	{
		if (QETProject *project = this->currentProject())
		{
			CatalogReplaceDialog dialog(project, QETApp::catalog(), this);
			dialog.exec();
			if (dialog.replacedCount()) {
				statusBar()->showMessage(
							tr("%n composant(s) ont changé de pièce. "
							   "Ctrl+Z annule tout d'un coup.",
							   "", dialog.replacedCount()), 8000);
			}
		}
	});

		//The three switches of T35: on while a symbol is being drawn, off the
		//rest of the time. Checkable, because the answer to "is this on?" has
		//to be visible in the menu without trying it.
	m_show_fine_grid = new QAction(tr("Afficher la grille fine"), this);
	m_show_fine_grid->setCheckable(true);
	m_show_fine_grid->setChecked(Diagram::displayFineGrid);
	m_show_fine_grid->setStatusTip(
				tr("La grille fine sert au dessin ; les points de "
				   "raccordement, eux, doivent rester sur la grille "
				   "principale."));
	connect(m_show_fine_grid, &QAction::toggled, this, [this](bool on)
	{
		Diagram::displayFineGrid = on;
		QSettings settings;
		settings.setValue(QStringLiteral("diagrameditor/display-fine-grid"), on);
		for (ProjectView *view : this->openedProjects()) {
			for (Diagram *diagram : view->project()->diagrams()) {
				diagram->update();
			}
		}
	});

	m_show_terminals = new QAction(
				tr("Afficher les points de raccordement"), this);
	m_show_terminals->setCheckable(true);
	m_show_terminals->setChecked(Diagram::displayTerminals);
	m_show_terminals->setStatusTip(
				tr("À laisser désactivé pour le travail courant : un point "
				   "visible est un point qu'on déplace par accident."));
	connect(m_show_terminals, &QAction::toggled, this, [this](bool on)
	{
		Diagram::displayTerminals = on;
		QSettings settings;
		settings.setValue(QStringLiteral("diagrameditor/display-terminals"), on);
		for (ProjectView *view : this->openedProjects()) {
			for (Diagram *diagram : view->project()->diagrams()) {
				diagram->update();
			}
		}
	});

	m_show_empty_fields = new QAction(
				tr("Afficher les attributs vides"), this);
	m_show_empty_fields->setCheckable(true);
	m_show_empty_fields->setChecked(Diagram::displayEmptyTextFields);
	connect(m_show_empty_fields, &QAction::toggled, this, [this](bool on)
	{
		Diagram::displayEmptyTextFields = on;
		QSettings settings;
		settings.setValue(
					QStringLiteral("diagrameditor/display-empty-fields"), on);
		for (ProjectView *view : this->openedProjects()) {
			for (Diagram *diagram : view->project()->diagrams()) {
				diagram->update();
			}
		}
	});

		//Renumbering fills a folio with text. These two put it back in order,
		//and neither touches a number: the drawing stops repeating it, the
		//wiring list keeps it.
	m_show_conductor_text = new QAction(
				tr("Afficher le numéro des conducteurs sélectionnés"), this);
	connect(m_show_conductor_text, &QAction::triggered,
		this, [this]() { this->setConductorTextVisible(true); });

	m_hide_conductor_text = new QAction(
				tr("Masquer le numéro des conducteurs sélectionnés"), this);
	connect(m_hide_conductor_text, &QAction::triggered,
		this, [this]() { this->setConductorTextVisible(false); });

	m_align_conductor_text = new QAction(
				tr("Aligner le numéro des conducteurs sélectionnés"), this);
	m_align_conductor_text->setStatusTip(
				tr("Remplace le fait de déplacer chaque texte à la main."));
	connect(m_align_conductor_text, &QAction::triggered,
		this, &QETDiagramEditor::alignConductorTexts);

		//An accessory drawn on the folio, in its own location, saying whose it
		//is. Reached from the menu on the selected element rather than asked
		//when the symbol is inserted - see the T13 task file for why.
	m_link_accessory = new QAction(
				tr("Lier l'accessoire sélectionné à un composant…"), this);
	connect(m_link_accessory, &QAction::triggered, this, [this]()
	{
		DiagramView *view = this->currentDiagramView();
		if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
			return;
		}
		Element *accessory = nullptr;
		const QList<QGraphicsItem *> items = view->diagram()->selectedItems();
		for (QGraphicsItem *item : items)
		{
			if (Element *element = qgraphicsitem_cast<Element *>(item))
			{
				if (accessory) {
					QMessageBox::information(this,
						tr("Lier un accessoire"),
						tr("Sélectionnez un seul accessoire."));
					return;
				}
				accessory = element;
			}
		}
		if (!accessory) {
			QMessageBox::information(this, tr("Lier un accessoire"),
				tr("Sélectionnez l'accessoire à rattacher."));
			return;
		}
		if (CatalogProjectActions::linkAccessory(accessory, this)) {
			statusBar()->showMessage(
						tr("Accessoire rattaché. Ctrl+Z annule."), 6000);
		}
	});

	m_iec_structure = new QAction(
				tr("Structure d'identification (CEI 81346)…"), this);
	connect(m_iec_structure, &QAction::triggered, this, [this]()
	{
		if (QETProject *project = this->currentProject())
		{
			IecStructureDialog dialog(project, this);
			dialog.exec();
		}
	});

		//Launch the plugin of terminal generator
	m_project_terminalBloc = new QAction(QET::Icons::TerminalStrip, tr("Lancer le plugin de création de borniers"), this);
	connect(m_project_terminalBloc, &QAction::triggered, this, &QETDiagramEditor::generateTerminalBlock);

	//Export conductor num to csv
	m_project_export_conductor_num = new QAction(QET::Icons::DocumentSpreadsheet, tr("Exporter la liste des noms de conducteurs"), this);
	connect(m_project_export_conductor_num, &QAction::triggered, [this]() {
		QETProject *project = this->currentProject();
		if (project)
		{
			ConductorNumExport wne(project, this);
			wne.toCsv();
		}
	});
	// Export wiring list to CSV
	m_project_export_wiring_list = new QAction(QET::Icons::DocumentSpreadsheet, tr("Exporter le plan de câblage"), this);
	connect(m_project_export_wiring_list, &QAction::triggered, [this]() {
		QETProject *project = this->currentProject();
		if (project)
		{
			WiringListExport wle(project, this);
			wle.toCsv();
		}
	});

	// Terminal Numbering
	m_terminal_numbering = new QAction(QET::Icons::TerminalStrip, tr("Numérotation automatique des bornes"), this);
	connect(m_terminal_numbering, &QAction::triggered, this, &QETDiagramEditor::slot_terminalNumbering);

	#ifdef QET_EXPORT_PROJECT_DB
		m_export_project_db = new QAction(QET::Icons::DocumentSpreadsheet, tr("Exporter la base de donnée interne du projet"), this);
		connect(m_export_project_db, &QAction::triggered, [this]() {
			projectDataBase::exportDb(this->currentProject()->dataBase(), this);
		});
	#endif

		//MDI view style
	m_tabbed_view_mode = new QAction(tr("en utilisant des onglets"), this);
	m_tabbed_view_mode->setStatusTip(tr("Présente les différents projets ouverts des onglets", "status bar tip"));
	m_tabbed_view_mode->setCheckable(true);
	connect(m_tabbed_view_mode, &QAction::triggered, this, &QETDiagramEditor::setTabbedMode);

	m_windowed_view_mode = new QAction(tr("en utilisant des fenêtres"), this);
	m_windowed_view_mode->setStatusTip(tr("Présente les différents projets ouverts dans des sous-fenêtres", "status bar tip"));
	m_windowed_view_mode->setCheckable(true);
	connect(m_windowed_view_mode, &QAction::triggered, this, &QETDiagramEditor::setWindowedMode);

	m_group_view_mode = new QActionGroup(this);
	m_group_view_mode -> addAction(m_windowed_view_mode);
	m_group_view_mode -> addAction(m_tabbed_view_mode);
	m_group_view_mode -> setExclusive(true);

	m_tile_window = new QAction(tr("&Mosaïque"), this);
	m_tile_window->setStatusTip(tr("Dispose les fenêtres en mosaïque", "status bar tip"));
	connect(m_tile_window, &QAction::triggered, &m_workspace, &QMdiArea::tileSubWindows);

	m_cascade_window = new QAction(tr("&Cascade"), this);
	m_cascade_window->setStatusTip(tr("Dispose les fenêtres en cascade", "status bar tip"));
	connect(m_cascade_window, &QAction::triggered, &m_workspace, &QMdiArea::cascadeSubWindows);

		//Switch selection/view mode
	m_mode_selection = new QAction(QET::Icons::PartSelect, tr("Mode Selection"), this);
	m_mode_selection->setStatusTip(tr("Permet de sélectionner les éléments", "status bar tip"));
	m_mode_selection->setCheckable(true);
	m_mode_selection->setChecked(true);
	connect(m_mode_selection, &QAction::triggered, [this]() {
		if (ProjectView *pv = currentProjectView()) {
			for (DiagramView *dv : pv->diagram_views()) {
				dv->setSelectionMode();
			}
		}
	});

	m_mode_visualise = new QAction(QET::Icons::ViewMove, tr("Mode Visualisation"), this);
	m_mode_visualise->setStatusTip(tr("Permet de visualiser le folio sans pouvoir le modifier", "status bar tip"));
	m_mode_visualise->setCheckable(true);
	connect(m_mode_visualise, &QAction::triggered, [this]() {
		if (ProjectView *pv = currentProjectView()) {
			for(DiagramView *dv : pv->diagram_views()) {
				dv->setVisualisationMode();
			}
		}
	});

	grp_visu_sel = new QActionGroup(this);
	grp_visu_sel->addAction(m_mode_selection);
	grp_visu_sel->addAction(m_mode_visualise);
	grp_visu_sel->setExclusive(true);

		//Navigate next/previous project
	m_next_window = new QAction(tr("Projet suivant"), this);
	ShortcutManager::instance().registerAction(m_next_window, "diagrameditor.next_window", tr("Éditeur de schémas"), QKeySequence::NextChild);
	m_next_window->setStatusTip(tr("Active le projet suivant", "status bar tip"));
	connect(m_next_window, &QAction::triggered, &m_workspace, &QMdiArea::activateNextSubWindow);

	m_previous_window = new QAction(tr("Projet précédent"), this);
	ShortcutManager::instance().registerAction(m_previous_window, "diagrameditor.previous_window", tr("Éditeur de schémas"), QKeySequence::PreviousChild);
	m_previous_window->setStatusTip(tr("Active le projet précédent", "status bar tip"));
	connect(m_previous_window, &QAction::triggered, &m_workspace, &QMdiArea::activatePreviousSubWindow);

		//Files action
	QAction *new_file  = m_file_actions_group.addAction(QET::Icons::ProjectNew,     tr("&Nouveau"));
	QAction *open_file = m_file_actions_group.addAction(QET::Icons::DocumentOpen,   tr("&Ouvrir"));
	m_save_file        = m_file_actions_group.addAction(QET::Icons::DocumentSave,   tr("&Enregistrer"));
	m_save_file_as     = m_file_actions_group.addAction(QET::Icons::DocumentSaveAs, tr("Enregistrer sous"));
	m_close_file       = m_file_actions_group.addAction(QET::Icons::ProjectClose,   tr("&Fermer"));

	ShortcutManager::instance().registerAction(new_file, "diagrameditor.new_file", tr("Éditeur de schémas"), QKeySequence::New);
	ShortcutManager::instance().registerAction(open_file, "diagrameditor.open_file", tr("Éditeur de schémas"), QKeySequence::Open);
	ShortcutManager::instance().registerAction(m_close_file, "diagrameditor.close_file", tr("Éditeur de schémas"), QKeySequence::Close);
	ShortcutManager::instance().registerAction(m_save_file, "diagrameditor.save_file", tr("Éditeur de schémas"), QKeySequence::Save);
	ShortcutManager::instance().registerAction(m_save_file_as, "diagrameditor.save_file_as", tr("Éditeur de schémas"), Qt::CTRL | Qt::SHIFT | Qt::Key_S);

	new_file     ->setStatusTip( tr("Crée un nouveau projet", "status bar tip") );
	open_file    ->setStatusTip( tr("Ouvre un projet existant", "status bar tip") );
	m_close_file ->setStatusTip( tr("Ferme le projet courant", "status bar tip") );
	m_save_file    ->setStatusTip( tr("Enregistre le projet courant et tous ses folios", "status bar tip") );
	m_save_file_as ->setStatusTip( tr("Enregistre le projet courant avec un autre nom de fichier", "status bar tip") );

	connect(m_save_file_as, &QAction::triggered, this, &QETDiagramEditor::saveAs);
	connect(m_save_file,    &QAction::triggered, this, &QETDiagramEditor::save);
	connect(new_file,       &QAction::triggered, this, &QETDiagramEditor::newProject);
	connect(open_file,      &QAction::triggered, this, &QETDiagramEditor::openProject);
	connect(m_close_file,   &QAction::triggered, [this]() {
		if (ProjectView *project_view = currentProjectView()) {
			closeProject(project_view);
		}
	});

		//Rows and Columns
	QAction *add_column    = m_row_column_actions_group.addAction( QET::Icons::EditTableInsertColumnRight, tr("Ajouter une colonne") );
	QAction *remove_column = m_row_column_actions_group.addAction( QET::Icons::EditTableDeleteColumn,      tr("Enlever une colonne") );
	QAction *add_row       = m_row_column_actions_group.addAction( QET::Icons::EditTableInsertRowUnder,    tr("Ajouter une ligne", "Add row") );
	QAction *remove_row    = m_row_column_actions_group.addAction( QET::Icons::EditTableDeleteRow,         tr("Enlever une ligne","Remove row") );

	add_column    -> setStatusTip( tr("Ajoute une colonne au folio", "status bar tip"));
	remove_column -> setStatusTip( tr("Enlève une colonne au folio", "status bar tip"));
	add_row       -> setStatusTip( tr("Agrandit le folio en hauteur", "status bar tip"));
	remove_row    -> setStatusTip( tr("Rétrécit le folio en hauteur", "status bar tip"));

	add_column   ->setData("add_column");
	remove_column->setData("remove_column");
	add_row      ->setData("add_row");
	remove_row   ->setData("remove_row");

	connect(&m_row_column_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::rowColumnGroupTriggered);

		//Selections Actions (related to a selected item)
	m_delete_selection     = m_selection_actions_group.addAction( QET::Icons::EditDelete,        tr("Supprimer")                 );
	m_rotate_selection     = m_selection_actions_group.addAction( QET::Icons::TransformRotate,   tr("Pivoter")                   );
	m_rotate_group_selection = m_selection_actions_group.addAction( QET::Icons::TransformRotate, tr("Pivoter le groupe")         );
	m_rotate_texts         = m_selection_actions_group.addAction( QET::Icons::ObjectRotateRight, tr("Orienter les textes")       );
	m_find_element         = m_selection_actions_group.addAction( QET::Icons::ZoomDraw,          tr("Retrouver dans le panel")   );
	m_edit_selection       = m_selection_actions_group.addAction( QET::Icons::ElementEdit,       tr("Éditer l'item sélectionné") );
	m_group_selected_texts = m_selection_actions_group.addAction( QET::Icons::textGroup,         tr("Grouper les textes sélectionnés"));

	ShortcutManager::instance().registerAction(m_delete_selection, "diagrameditor.delete_selection", tr("Éditeur de schémas"), Qt::Key_Delete);
	ShortcutManager::instance().registerAction(m_rotate_selection, "diagrameditor.rotate_selection", tr("Éditeur de schémas"), Qt::Key_Space);
	ShortcutManager::instance().registerAction(m_rotate_group_selection, "diagrameditor.rotate_group_selection", tr("Éditeur de schémas"), Qt::SHIFT | Qt::Key_Space);
	ShortcutManager::instance().registerAction(m_rotate_texts, "diagrameditor.rotate_texts", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_Space);
	ShortcutManager::instance().registerAction(m_edit_selection, "diagrameditor.edit_selection", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_E);

	m_delete_selection->setStatusTip( tr("Enlève les éléments sélectionnés du folio", "status bar tip"));
	m_rotate_selection->setStatusTip( tr("Pivote les éléments et textes sélectionnés", "status bar tip"));
	m_rotate_group_selection->setStatusTip( tr("Pivote la sélection comme un groupe autour de son centre, au lieu de chaque élément sur place", "status bar tip"));
	m_rotate_texts    ->setStatusTip( tr("Pivote les textes sélectionnés à un angle précis", "status bar tip"));
	m_find_element    ->setStatusTip( tr("Retrouve l'élément sélectionné dans le panel", "status bar tip"));

	m_delete_selection    ->setData("delete_selection");
	m_rotate_selection    ->setData("rotate_selection");
	m_rotate_group_selection->setData("rotate_group_selection");
	m_rotate_texts        ->setData("rotate_selected_text");
	m_find_element        ->setData("find_selected_element");
	m_edit_selection      ->setData("edit_selected_element");
	m_group_selected_texts->setData("group_selected_texts");

	connect(&m_selection_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::selectionGroupTriggered);

		//Select Action
	QAction *select_all     = m_select_actions_group.addAction( QET::Icons::EditSelectAll,      tr("Tout sélectionner") );
	QAction *select_nothing = m_select_actions_group.addAction( QET::Icons::EditSelectNone,     tr("Désélectionner tout") );
	QAction *select_invert  = m_select_actions_group.addAction( QET::Icons::EditSelectInvert,   tr("Inverser la sélection") );

	ShortcutManager::instance().registerAction(select_all, "diagrameditor.select_all", tr("Éditeur de schémas"), QKeySequence::SelectAll);
	ShortcutManager::instance().registerAction(select_nothing, "diagrameditor.select_nothing", tr("Éditeur de schémas"), QKeySequence::Deselect);
	ShortcutManager::instance().registerAction(select_invert, "diagrameditor.select_invert", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_I);

	select_all    ->setStatusTip( tr("Sélectionne tous les éléments du folio", "status bar tip") );
	select_nothing->setStatusTip( tr("Désélectionne tous les éléments du folio", "status bar tip") );
	select_invert ->setStatusTip( tr("Désélectionne les éléments sélectionnés et sélectionne les éléments non sélectionnés", "status bar tip") );

	select_all    ->setData("select_all");
	select_nothing->setData("deselect");
	select_invert ->setData("invert_selection");

	connect(&m_select_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::selectGroupTriggered);

		//Zoom actions
	QAction *zoom_in      = m_zoom_actions_group.addAction( QET::Icons::ZoomIn,       tr("Zoom avant"));
	QAction *zoom_out     = m_zoom_actions_group.addAction( QET::Icons::ZoomOut,      tr("Zoom arrière"));
	QAction *zoom_content = m_zoom_actions_group.addAction( QET::Icons::ZoomDraw,     tr("Zoom sur le contenu"));
	QAction *zoom_fit     = m_zoom_actions_group.addAction( QET::Icons::ZoomFitBest,  tr("Zoom adapté"));
	QAction *zoom_reset   = m_zoom_actions_group.addAction( QET::Icons::ZoomOriginal, tr("Pas de zoom"));
	m_zoom_action_toolBar << zoom_content << zoom_fit << zoom_reset;

	ShortcutManager::instance().registerAction(zoom_in, "diagrameditor.zoom_in", tr("Éditeur de schémas"), QKeySequence::ZoomIn);
	ShortcutManager::instance().registerAction(zoom_out, "diagrameditor.zoom_out", tr("Éditeur de schémas"), QKeySequence::ZoomOut);
	ShortcutManager::instance().registerAction(zoom_content, "diagrameditor.zoom_content", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_8);
	ShortcutManager::instance().registerAction(zoom_fit, "diagrameditor.zoom_fit", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_9);
	ShortcutManager::instance().registerAction(zoom_reset, "diagrameditor.zoom_reset", tr("Éditeur de schémas"), Qt::CTRL | Qt::Key_0);

	zoom_in     ->setStatusTip(tr("Agrandit le folio", "status bar tip"));
	zoom_out    ->setStatusTip(tr("Rétrécit le folio", "status bar tip"));
	zoom_content->setStatusTip(tr("Adapte le zoom de façon à afficher tout le contenu du folio indépendamment du cadre"));
	zoom_fit    ->setStatusTip(tr("Adapte le zoom exactement sur le cadre du folio", "status bar tip"));
	zoom_reset  ->setStatusTip(tr("Restaure le zoom par défaut", "status bar tip"));

	zoom_in     ->setData("zoom_in");
	zoom_out    ->setData("zoom_out");
	zoom_content->setData("zoom_content");
	zoom_fit    ->setData("zoom_fit");
	zoom_reset  ->setData("zoom_reset");

	connect(&m_zoom_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::zoomGroupTriggered);

		//Adding action (add text, image, shape...)
	QAction *add_text      = m_add_item_actions_group.addAction(QET::Icons::PartTextField, tr("Ajouter un champ de texte"));
	QAction *add_image	   = m_add_item_actions_group.addAction(QET::Icons::adding_image,  tr("Ajouter une image"));
	QAction *add_line	   = m_add_item_actions_group.addAction(QET::Icons::PartLine,      tr("Ajouter une ligne", "Draw line"));
	QAction *add_rectangle = m_add_item_actions_group.addAction(QET::Icons::PartRectangle, tr("Ajouter un rectangle"));
	QAction *add_ellipse   = m_add_item_actions_group.addAction(QET::Icons::PartEllipse,   tr("Ajouter une ellipse"));
	QAction *add_polyline  = m_add_item_actions_group.addAction(QET::Icons::PartPolygon,   tr("Ajouter une polyligne"));
	QAction *add_terminal_strip = m_add_item_actions_group.addAction(QET::Icons::TerminalStrip, tr("Ajouter un plan de bornes"));

	add_text     ->setStatusTip(tr("Ajoute un champ de texte sur le folio actuel"));
	add_image    ->setStatusTip(tr("Ajoute une image sur le folio actuel"));
	add_line     ->setStatusTip(tr("Ajoute une ligne sur le folio actuel"));
	add_rectangle->setStatusTip(tr("Ajoute un rectangle sur le folio actuel"));
	add_ellipse  ->setStatusTip(tr("Ajoute une ellipse sur le folio actuel"));
	add_polyline ->setStatusTip(tr("Ajoute une polyligne sur le folio actuel"));
	add_terminal_strip->setStatusTip(tr("Ajoute un plan de bornier sur le folio actuel"));

	add_text     ->setData(QStringLiteral("text"));
	add_image    ->setData(QStringLiteral("image"));
	add_line     ->setData(QStringLiteral("line"));
	add_rectangle->setData(QStringLiteral("rectangle"));
	add_ellipse  ->setData(QStringLiteral("ellipse"));
	add_polyline ->setData(QStringLiteral("polyline"));
	add_terminal_strip->setData(QStringLiteral("terminal_strip"));

	add_text->setCheckable(true);
	add_line->setCheckable(true);
	add_rectangle->setCheckable(true);
	add_ellipse->setCheckable(true);
	add_polyline->setCheckable(true);

	connect(&m_add_item_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::addItemGroupTriggered);

		//Depth action
	m_depth_action_group = QET::depthActionGroup(this);
	m_depth_action_group->setDisabled(true);

	connect(m_depth_action_group, &QActionGroup::triggered, [this](QAction *action) {
		this->currentDiagramView()->diagram()->changeZValue(action->data().value<QET::DepthOption>());
	});

	m_find = new QAction(tr("Chercher/remplacer"), this);
	ShortcutManager::instance().registerAction(m_find, "diagrameditor.find", tr("Éditeur de schémas"), QKeySequence::Find);
	connect(m_find, &QAction::triggered, [this]()
	{
		if (auto animator = m_search_and_replace_widget.findChild<QWidgetAnimation *>("search and replace animator")) {
			animator->setHidden(!m_search_and_replace_widget.isHidden());
		} else {
			this->m_search_and_replace_widget.setHidden(!m_search_and_replace_widget.isHidden());
		}
	});

	m_jump_to_element = new QAction(tr("Atteindre un élément"), this);
	m_jump_to_element->setShortcut(Qt::CTRL | Qt::Key_G);
	m_jump_to_element->setStatusTip(tr("Recherche et sélectionne rapidement un élément du folio", "status bar tip"));
	connect(m_jump_to_element, &QAction::triggered, [this]()
	{
		DiagramView *diagram_view = this->currentDiagramView();
		if (!diagram_view || !diagram_view->diagram()) {
			return;
		}
		JumpToElementDialog dialog(diagram_view->diagram(), this);
		dialog.exec();
	});
}

/**
	@brief QETDiagramEditor::setUpToolBar
*/
void QETDiagramEditor::setUpToolBar()
{
	main_tool_bar = new QToolBar(tr("Outils"), this);
	main_tool_bar -> setObjectName("toolbar");

	view_tool_bar = new QToolBar(tr("Affichage"), this);
	view_tool_bar -> setObjectName("display");

	diagram_tool_bar = new QToolBar(tr("Schéma"), this);
	diagram_tool_bar -> setObjectName("diagram");

	main_tool_bar -> addActions(m_file_actions_group.actions());
	main_tool_bar -> addAction(m_print);
	main_tool_bar -> addAction(m_export_to_pdf);
	main_tool_bar -> addSeparator();
	main_tool_bar -> addAction(undo);
	main_tool_bar -> addAction(redo);
	main_tool_bar -> addSeparator();
	main_tool_bar -> addAction(m_cut);
	main_tool_bar -> addAction(m_copy);
	main_tool_bar -> addAction(m_paste);
	main_tool_bar -> addSeparator();
	main_tool_bar -> addAction(m_delete_selection);
	main_tool_bar -> addAction(m_rotate_selection);

	// Modes selection / visualisation et zoom
	view_tool_bar -> addAction(m_mode_selection);
	view_tool_bar -> addAction(m_mode_visualise);
	view_tool_bar -> addSeparator();
	view_tool_bar -> addWidget(new DiagramEditorHandlerSizeWidget(this));
	view_tool_bar -> addSeparator();
	view_tool_bar -> addAction(m_draw_grid);
	view_tool_bar -> addAction(m_draw_guides);
	view_tool_bar -> addAction (m_grey_background);
	view_tool_bar -> addSeparator();
	view_tool_bar -> addActions(m_zoom_action_toolBar);

	diagram_tool_bar -> addAction (m_edit_diagram_properties);
	diagram_tool_bar -> addAction (m_conductor_reset);
	diagram_tool_bar -> addAction (m_auto_conductor);
	diagram_tool_bar -> addAction (m_auto_break_conductor);

	m_add_item_tool_bar = new QToolBar(tr("Ajouter"), this);
	m_add_item_tool_bar->setObjectName("adding");
	m_add_item_tool_bar->addActions(m_add_item_actions_group.actions());

	m_depth_tool_bar = new QToolBar(tr("Profondeur", "toolbar title"));
	m_depth_tool_bar->setObjectName("diagram_depth_toolbar");
	m_depth_tool_bar->addActions(m_depth_action_group->actions());

	addToolBar(Qt::TopToolBarArea, main_tool_bar);
	addToolBar(Qt::TopToolBarArea, view_tool_bar);
	addToolBar(Qt::TopToolBarArea, diagram_tool_bar);
	addToolBar(Qt::TopToolBarArea, m_add_item_tool_bar);
	addToolBar(Qt::TopToolBarArea, m_depth_tool_bar);

		//The selected tool has to be recognisable, which means the icon on it
		//has to stay visible. See QETApp::toolBarStyleSheet().
	QETApp::styleToolBars(this);
}

/**
	@brief QETDiagramEditor::assignCatalogPart
	Assign one catalog part to every component selected on the current folio.

	The most repeated action of the day, which is why it is one dialog and one
	confirmation and nothing else. Assigning a part removes the accessories
	the components had, so the user is told before it happens rather than
	after.
*/
void QETDiagramEditor::assignCatalogPart()
{
	DiagramView *diagram_view = currentDiagramView();
	if (!diagram_view || !diagram_view->diagram()) {
		return;
	}

	QList<Element *> selected;
	const QList<QGraphicsItem *> items = diagram_view->diagram()->selectedItems();
	for (QGraphicsItem *item : items)
	{
		if (Element *element = qgraphicsitem_cast<Element *>(item)) {
			selected.append(element);
		}
	}

	if (selected.isEmpty())
	{
		QET::QetMessageBox::information(this, tr("Aucun composant sélectionné"),
						tr("Sélectionnez le ou les composants qui doivent "
						   "recevoir la pièce."));
		return;
	}

	Catalog *catalog = QETApp::catalog();
	const CatalogPart part = CatalogBrowserDialog::choosePart(catalog, this);
	if (part.isNull()) {
		return;
	}

	const int count = CatalogProjectActions::assignPart(selected, *catalog, part);
	statusBar()->showMessage(tr("Pièce %1 attribuée à %n composant(s).", "", count)
				 .arg(part.code), 4000);
}

/**
	@brief QETDiagramEditor::registerCatalogPart
	Save the components selected on the folio as a catalog part.

	This is the registration flow that makes the catalog grow at the speed of
	the projects: draw the circuit, fix the terminal numbers, fill in the
	code, save. Select the coil and all its contacts together and the part
	keeps the numbers of each symbol apart.
*/
void QETDiagramEditor::registerCatalogPart()
{
	DiagramView *diagram_view = currentDiagramView();
	if (!diagram_view || !diagram_view->diagram()) {
		return;
	}

	QList<Element *> selected;
	const QList<QGraphicsItem *> items = diagram_view->diagram()->selectedItems();
	for (QGraphicsItem *item : items)
	{
		if (Element *element = qgraphicsitem_cast<Element *>(item)) {
			selected.append(element);
		}
	}

	if (selected.isEmpty())
	{
		QET::QetMessageBox::information(this, tr("Aucun composant sélectionné"),
						tr("Sélectionnez les symboles du composant — la bobine et "
						   "tous ses contacts — avant d'enregistrer la pièce."));
		return;
	}

	Catalog *catalog = QETApp::catalog();
	CatalogPart part = CatalogProjectActions::partFromElements(*catalog, selected);

	CatalogPartDialog dialog(catalog, part, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	// Registering a part from a circuit and not assigning it back to that
	// circuit would leave the drawing without the code that was just typed.
	const CatalogPart saved = dialog.part();
	const int count = CatalogProjectActions::assignPart(selected, *catalog, saved);
	statusBar()->showMessage(tr("Pièce %1 enregistrée et attribuée à %n composant(s).", "", count)
				 .arg(saved.code), 4000);
}

/**
	@brief QETDiagramEditor::renumberComponents
	Renumber the components, showing what would change before changing it.

	Half of the number one pain of the office is renumbering: inserting a folio
	in the middle, or duplicating a circuit, and correcting the tags by hand
	afterwards. This is the command that stops that being by hand - and it
	shows the "from → to" table first, because renumbering blind is worse than
	not renumbering.
*/
void QETDiagramEditor::renumberComponents()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

	QList<Element *> selected;
	if (DiagramView *diagram_view = currentDiagramView())
	{
		if (diagram_view->diagram())
		{
			const QList<QGraphicsItem *> items = diagram_view->diagram()->selectedItems();
			for (QGraphicsItem *item : items)
			{
				if (Element *element = qgraphicsitem_cast<Element *>(item)) {
					selected.append(element);
				}
			}
		}
	}

	RenumberDialog dialog(project, QETApp::catalog(), selected, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	statusBar()->showMessage(tr("%n composant(s) renuméroté(s). Ctrl+Z annule tout d'un coup.",
				    "", dialog.appliedCount()), 5000);
}


/**
	@brief QETDiagramEditor::createSymbolFromSelection
	Turn what is drawn and selected on the sheet into a symbol of the library.

	The step that was missing: the drawing tools of the sheet are the tools
	everybody knows, so a symbol should be made with them. What the drawing
	cannot say - the class, the provisional label of each connection point,
	which pair of points is a NO contact - is asked for in the dialog, and
	only there.
*/
void QETDiagramEditor::createSymbolFromSelection()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}

	DiagramContent content(view->diagram(), true);
	const QString refusal = SheetSymbolExtractor::refusal(content);
	if (!refusal.isEmpty()) {
		QMessageBox::information(this, tr("Créer un symbole"), refusal);
		return;
	}

	const SymbolGrid grid;
	const SymbolDefinition symbol =
			SheetSymbolExtractor::fromSelection(content, grid);

	CreateSymbolDialog dialog(symbol, QETApp::catalog(), this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

		//The collection is reloaded so the new symbol is in the panel at
		//once: making a symbol and then not finding it is the same as not
		//having made it.
	if (m_element_collection_widget) {
		m_element_collection_widget->reload();
	}
	statusBar()->showMessage(
				tr("Symbole enregistré : %1").arg(dialog.savedPath()),
				8000);
}

/**
	@brief QETDiagramEditor::saveSelectionAsGroup
	File the selected piece of schematic in the library, catalog parts and all.
*/
void QETDiagramEditor::saveSelectionAsGroup()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram()) {
		return;
	}
	if (view->diagram()->selectedItems().isEmpty()) {
		QMessageBox::information(this, tr("Enregistrer un groupement"),
			tr("Rien n'est sélectionné. Sélectionnez le morceau de schéma "
			   "à enregistrer."));
		return;
	}

		//The very fragment the copy command produces. Nothing is translated
		//into another format on the way in, so nothing can be lost on the
		//way out - the assigned parts included.
	const QDomDocument fragment = view->diagram()->toXml(false);
	const QString path = SymbolGroupDialog::saveSelection(fragment, this);
	if (path.isEmpty()) {
		return;
	}
	statusBar()->showMessage(tr("Groupement enregistré : %1").arg(path), 8000);
}

/**
	@brief QETDiagramEditor::insertGroup
	Insert a filed piece of schematic into the sheet, in one undoable step.
*/
void QETDiagramEditor::insertGroup()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}

	const SymbolGroup group = SymbolGroupDialog::chooseGroup(this);
	if (group.isNull()) {
		return;
	}

	QDomDocument fragment;
	fragment.appendChild(fragment.importNode(
				     group.fragment.documentElement()
				     .firstChildElement(QStringLiteral("diagram")), true));
	if (fragment.documentElement().isNull()) {
			//A file written before the wrapper existed, or by hand: the
			//fragment is the whole document.
		fragment = group.fragment;
	}

		//Read in and pushed as one command, the same way a paste is: undoing
		//an insertion has to be one Ctrl+Z, not thirty.
	Diagram *diagram = view->diagram();
	DiagramContent inserted;
		//Where the projectist is looking, not at the corner of the sheet: a
		//grouping that lands off screen reads as one that did not arrive.
	const QPointF where =
			view->mapToScene(view->viewport()->rect().center());
	diagram->fromXml(fragment, where, false, &inserted);
	if (!inserted.count()) {
		QMessageBox::warning(this, tr("Insérer un groupement"),
			tr("« %1 » n'a rien apporté sur la folio.").arg(group.name));
		return;
	}

	diagram->clearSelection();
	diagram->undoStack().push(new PasteDiagramCommand(diagram, inserted));
	view->adjustSceneRect();

	const QStringList codes = group.partCodes();
	if (codes.isEmpty()) {
		statusBar()->showMessage(
					tr("« %1 » inséré. Ctrl+Z annule tout d'un coup.")
					.arg(group.name), 8000);
	} else {
		statusBar()->showMessage(
					tr("« %1 » inséré avec %n pièce(s) déjà attribuée(s). "
					   "Ctrl+Z annule tout d'un coup.", "", codes.size())
					.arg(group.name), 8000);
	}
}


/**
	@brief QETDiagramEditor::explodeSelection
	Turn the selected components back into the drawing they were made of.
*/
void QETDiagramEditor::explodeSelection()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}

	QList<Element *> selected;
	const QList<QGraphicsItem *> items = view->diagram()->selectedItems();
	for (QGraphicsItem *item : items)
	{
		if (Element *element = qgraphicsitem_cast<Element *>(item)) {
			selected.append(element);
		}
	}

	if (selected.isEmpty()) {
		QMessageBox::information(this, tr("Éclater le symbole"),
			tr("Sélectionnez le ou les symboles à éclater."));
		return;
	}

		//Refusals said one by one and before anything happens: a command that
		//silently does nothing to three of five selected symbols is worse
		//than a command that explains itself.
	QStringList refusals;
	for (Element *element : selected)
	{
		const QString refusal = ExplodeElementCommand::refusal(element);
		if (!refusal.isEmpty()) {
			refusals << refusal;
		}
	}

	ExplodeElementCommand *command = new ExplodeElementCommand(selected);
	if (command->isEmpty())
	{
		delete command;
		QMessageBox::information(this, tr("Éclater le symbole"),
			refusals.isEmpty()
			? tr("Aucun des symboles sélectionnés n'a pu être éclaté.")
			: refusals.join(QStringLiteral("\n\n")));
		return;
	}

	const int exploded = command->elementCount();
	const int pieces = command->pieceCount();
	view->diagram()->clearSelection();
	view->diagram()->undoStack().push(command);

	QString message = tr("%n symbole(s) éclaté(s) en %1 formes et textes. "
			     "Ctrl+Z annule tout d'un coup.", "", exploded)
			.arg(pieces);
	if (!refusals.isEmpty()) {
		message += QStringLiteral(" ") +
				tr("%n symbole(s) refusé(s).", "", refusals.size());
	}
	statusBar()->showMessage(message, 10000);

	if (!refusals.isEmpty()) {
		QMessageBox::information(this, tr("Éclater le symbole"),
					 refusals.join(QStringLiteral("\n\n")));
	}
}


/**
	@brief QETDiagramEditor::setConductorTextVisible
	@param visible
	Show or hide the number of the selected conductors, without touching the
	number itself.
*/
void QETDiagramEditor::setConductorTextVisible(bool visible)
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}

	const DiagramContent content(view->diagram(), true);
	const QList<Conductor *> conductors = content.conductors();
	if (conductors.isEmpty()) {
		QMessageBox::information(this, tr("Numéro des conducteurs"),
			tr("Sélectionnez les conducteurs dont le numéro doit être "
			   "affiché ou masqué."));
		return;
	}

	ConductorTextCommand *command =
			new ConductorTextCommand(conductors, visible);
	if (command->isEmpty()) {
		delete command;
		statusBar()->showMessage(
					tr("Les conducteurs sélectionnés sont déjà comme ça."),
					5000);
		return;
	}

	const int count = command->conductorCount();
	view->diagram()->undoStack().push(command);
	statusBar()->showMessage(
				visible
				? tr("Numéro affiché sur %n conducteur(s). Le numéro "
				     "lui-même n'a pas changé.", "", count)
				: tr("Numéro masqué sur %n conducteur(s). Le numéro "
				     "lui-même n'a pas changé.", "", count),
				8000);
}

/**
	@brief QETDiagramEditor::alignConductorTexts
	Line the numbers of the selected conductors up on one axis.

	The specification asks for two clicks defining an axis. This does it from
	the selection instead: the conductors chosen already say where the axis is
	- they are parallel, and the axis is perpendicular to them - so asking the
	user to draw it would be asking them to say twice what they already said.

	Which coordinate is shared is decided by the conductors, not guessed: a
	column of horizontal wires gets the same x, a row of vertical ones the
	same y.
*/
void QETDiagramEditor::alignConductorTexts()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}

	const DiagramContent content(view->diagram(), true);
	QList<Conductor *> conductors;
	for (Conductor *conductor : content.conductors())
	{
		if (conductor && conductor->textItem() &&
				conductor->properties().m_show_text) {
			conductors << conductor;
		}
	}
	if (conductors.size() < 2) {
		QMessageBox::information(this, tr("Aligner les numéros"),
			tr("Sélectionnez au moins deux conducteurs dont le numéro est "
			   "affiché."));
		return;
	}

		//Which way the wires run, decided by counting rather than by looking
		//at the first one: one stray diagonal must not choose the axis for
		//the whole selection.
	int horizontal = 0;
	for (Conductor *conductor : conductors)
	{
		const QRectF box = conductor->boundingRect();
		if (box.width() >= box.height()) {
			horizontal++;
		}
	}
	const bool wires_are_horizontal = horizontal * 2 >= conductors.size();

		//The axis is the average of where the texts already are, so the
		//alignment moves everything the least it can: the projectist who put
		//three of them roughly right does not see all three jump.
	qreal sum = 0.0;
	for (Conductor *conductor : conductors)
	{
		const QPointF position = conductor->textItem()->pos();
		sum += wires_are_horizontal ? position.y() : position.x();
	}
	const qreal axis = sum / conductors.size();

	MoveConductorsTextsCommand *command =
			new MoveConductorsTextsCommand(view->diagram());
	int moved = 0;
	for (Conductor *conductor : conductors)
	{
		ConductorTextItem *text = conductor->textItem();
		const QPointF old_position = text->pos();
		const QPointF new_position = wires_are_horizontal
				? QPointF(old_position.x(), axis)
				: QPointF(axis, old_position.y());
		if (QLineF(old_position, new_position).length() < 0.01) {
			continue;
		}
			//Marked as moved by the user, because it was: from here on the
			//automatic placement must leave these texts where they were put.
		command->addTextMovement(text, old_position, new_position, true);
		moved++;
	}

	if (!moved) {
		delete command;
		statusBar()->showMessage(
					tr("Les numéros sont déjà alignés."), 5000);
		return;
	}
	view->diagram()->undoStack().push(command);
	statusBar()->showMessage(
				tr("%n numéro(s) alignés sur un même axe.", "", moved), 8000);
}

/**
	@brief QETDiagramEditor::setUpMenu
*/
void QETDiagramEditor::setUpMenu()
{

	QMenu* menu_fichier	  = new QMenu(tr("&Fichier"), this);
	QMenu* menu_edition	  = new QMenu(tr("&Édition"), this);
	QMenu* menu_project	  = new QMenu(tr("&Projet"), this);
	QMenu* menu_catalogue = new QMenu(tr("&Catalogue"), this);
	QMenu* menu_affichage = new QMenu(tr("Afficha&ge"), this);
	// QMenu *menu_outils    = new QMenu(tr("O&utils"), this);
	windows_menu = new QMenu(tr("Fe&nêtres"), this);

	insertMenu(settings_menu_, menu_fichier);
	insertMenu(settings_menu_, menu_edition);
	insertMenu(settings_menu_, menu_project);
	insertMenu(settings_menu_, menu_catalogue);
	insertMenu(settings_menu_, menu_affichage);
	insertMenu(help_menu_, windows_menu);

	// File menu
	QMenu *recentfile = menu_fichier -> addMenu(QET::Icons::DocumentOpenRecent, tr("&Récemment ouverts"));
	recentfile->addActions(QETApp::projectsRecentFiles()->menu()->actions());
	connect(QETApp::projectsRecentFiles(), &RecentFiles::fileOpeningRequested, this, &QETDiagramEditor::openRecentFile);
	menu_fichier -> addActions(m_file_actions_group.actions());
	menu_fichier -> addSeparator();
	//menu_fichier -> addAction(import_diagram);
	menu_fichier -> addAction(m_export_to_images);
	menu_fichier -> addAction(m_export_to_pdf);
	menu_fichier -> addAction(m_print);
	menu_fichier -> addSeparator();
	menu_fichier -> addAction(m_quit_editor);

	// menu Edition
	menu_edition -> addAction(undo);
	menu_edition -> addAction(redo);
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_cut);
	menu_edition -> addAction(m_copy);
	menu_edition -> addAction(m_paste);
	menu_edition -> addSeparator();
	menu_edition -> addActions(m_select_actions_group.actions());
	menu_edition -> addSeparator();
	menu_edition -> addActions(m_selection_actions_group.actions());
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_conductor_reset);
	menu_edition -> addAction(m_show_conductor_text);
	menu_edition -> addAction(m_hide_conductor_text);
	menu_edition -> addAction(m_align_conductor_text);
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_edit_diagram_properties);
	menu_edition -> addActions(m_row_column_actions_group.actions());
	menu_edition -> addSeparator();
	menu_edition -> addActions(m_depth_action_group->actions());
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_create_symbol);
	menu_edition -> addAction(m_explode_element);
	menu_edition -> addAction(m_save_group);
	menu_edition -> addAction(m_insert_group);
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_find);
	menu_edition -> addAction(m_jump_to_element);

	// menu Projet
	menu_project -> addAction(m_project_edit_properties);
	menu_project -> addAction(m_auto_conductor);
	menu_project -> addSeparator();
	menu_project -> addAction(m_project_add_diagram);
	menu_project -> addAction(m_remove_diagram_from_project);
	menu_project -> addAction(m_clean_project);
	menu_project -> addSeparator();
	menu_project -> addAction(m_add_summary);
	menu_project -> addAction(m_add_nomenclature);
	menu_project -> addAction(m_csv_export);
	menu_project -> addAction(m_project_export_conductor_num);
	menu_project -> addAction(m_terminal_strip_dialog);
	menu_project -> addAction(m_project_terminalBloc);
	menu_project -> addAction(m_project_export_wiring_list);
	menu_project -> addAction(m_terminal_numbering);
	menu_project -> addAction(m_renumber_components);
	menu_project -> addAction(m_iec_structure);
	menu_project -> addAction(m_replace_part);
#ifdef QET_EXPORT_PROJECT_DB
	menu_project -> addSeparator();
	menu_project -> addAction(m_export_project_db);
#endif

	// menu Catalogue. The catalog is shared by the office and does not
	// belong to a project, which is why it has a menu of its own.
	menu_catalogue -> addAction(m_catalog_browse);
	menu_catalogue -> addAction(m_catalog_assign);
	menu_catalogue -> addAction(m_catalog_register);
	menu_catalogue -> addSeparator();
	menu_catalogue -> addAction(m_link_accessory);
	menu_catalogue -> addAction(m_catalog_missing);
	menu_catalogue -> addSeparator();
	menu_catalogue -> addAction(m_catalog_import);
	menu_catalogue -> addAction(m_catalog_repository);
	menu_catalogue -> addSeparator();
	menu_catalogue -> addAction(m_catalog_manager);
	menu_catalogue -> addSeparator();
	menu_catalogue -> addAction(m_environment);

	main_tool_bar         -> toggleViewAction() -> setStatusTip(tr("Affiche ou non la barre d'outils principale"));
	view_tool_bar         -> toggleViewAction() -> setStatusTip(tr("Affiche ou non la barre d'outils Affichage"));
	diagram_tool_bar      -> toggleViewAction() -> setStatusTip(tr("Affiche ou non la barre d'outils Schéma"));
	qdw_pa           -> toggleViewAction() -> setStatusTip(tr("Affiche ou non le panel d'appareils"));
	qdw_undo         -> toggleViewAction() -> setStatusTip(tr("Affiche ou non la liste des modifications"));


	// menu Affichage
	QMenu *projects_view_mode = menu_affichage -> addMenu(QET::Icons::ConfigureToolbars, tr("Afficher les projets"));
	projects_view_mode -> setTearOffEnabled(true);
	projects_view_mode -> addAction(m_windowed_view_mode);
	projects_view_mode -> addAction(m_tabbed_view_mode);

	menu_affichage -> addSeparator();
	menu_affichage -> addAction(m_mode_selection);
	menu_affichage -> addAction(m_mode_visualise);
	menu_affichage -> addSeparator();
	menu_affichage -> addAction(m_draw_grid);
	menu_affichage -> addAction(m_show_fine_grid);
	menu_affichage -> addAction(m_show_terminals);
	menu_affichage -> addAction(m_show_empty_fields);
	menu_affichage -> addAction(m_draw_guides);
	menu_affichage -> addAction(m_grey_background);
	menu_affichage -> addSeparator();
	menu_affichage -> addActions(m_zoom_actions_group.actions());

	// menu Fenetres
	slot_updateWindowsMenu();
}

/**
	Permet de quitter l'application lors de la fermeture de la fenetre principale
	@param qce Le QCloseEvent correspondant a l'evenement de fermeture
*/
void QETDiagramEditor::closeEvent(QCloseEvent *qce)
{
	// quitte directement s'il n'y a aucun projet ouvert
	bool can_quit = true;
	if (openedProjects().count()) {
		// s'assure que la fenetre soit visible s'il y a des projets a fermer
		if (!isVisible() || isMinimized()) {
			if (isMaximized()) showMaximized();
			else showNormal();
		}
		// sinon demande la permission de fermer chaque projet
		foreach(ProjectView *project, openedProjects()) {
			if (!closeProject(project)) {
				can_quit = false;
				qce -> ignore();
				break;
			}
		}
	}
	if (can_quit) {
		writeSettings();
		setAttribute(Qt::WA_DeleteOnClose);
		qce -> accept();
	}
}

/**
	@brief QETDiagramEditor::event
	Reimplemented to :
	-Load elements collection when WindowActivate.
	@param e
	@return
*/
bool QETDiagramEditor::event(QEvent *e)
{
	if (m_first_show && e->type() == QEvent::WindowActivate)
	{
		m_first_show = false;
		QTimer::singleShot(250, m_element_collection_widget, &ElementsCollectionWidget::reload);
	}
	return(QETMainWindow::event(e));
}

/**
	@brief QETDiagramEditor::save
	Ask the current active project to save
*/
void QETDiagramEditor::save()
{
	if (ProjectView *project_view = currentProjectView()) {
			//Somebody else wrote to this file since we read it. Overwriting
			//would throw their work away without a word, which is the one
			//thing a shared environment must never do: refuse, and offer the
			//copy instead.
		QETProject *project = project_view -> project();
		if (project && project -> changedOnDiskByOthers())
		{
			const QMessageBox::StandardButton answer = QET::QetMessageBox::warning(
				this,
				tr("Le fichier a changé entre-temps", "message box title"),
				tr("%1 a été modifié par quelqu'un d'autre depuis que vous l'avez "
				   "ouvert.\n\nL'enregistrer par-dessus effacerait ce travail. "
				   "Enregistrez plutôt une copie, puis comparez les deux.")
					.arg(project -> filePath()),
				QMessageBox::Save | QMessageBox::Cancel,
				QMessageBox::Save);
			if (answer != QMessageBox::Save) {
				return;
			}
			saveAs();
			return;
		}

		QETResult saved = project_view -> save();

		if (saved.isOk()) {
			//save_file -> setDisabled(true);
			QETApp::projectsRecentFiles() -> fileWasOpened(project_view -> project() -> filePath());

			QString title = (project_view -> project() -> title ());
			if (title.isEmpty()) title = "QElectroTech ";
			QString filePath = (project_view -> project() -> filePath ());
			statusBar()-> showMessage(tr("Projet %1 enregistré dans le repertoire: %2.").arg(title).arg (filePath), 2000);
			m_element_collection_widget->highlightUnusedElement();
		}
		else {
			showError(saved);
		}
	}
}

/**
	@brief QETDiagramEditor::saveAs
	Ask the current active project to save as
*/
void QETDiagramEditor::saveAs()
{
	if (ProjectView *project_view = currentProjectView()) {
		QETResult save_file = project_view -> saveAs();
		if (save_file.isOk()) {
			QETApp::projectsRecentFiles() -> fileWasOpened(project_view -> project() -> filePath());

			QString title = (project_view -> project() -> title ());
			if (title.isEmpty()) title = "QElectroTech ";
			QString filePath = (project_view -> project() -> filePath ());
			statusBar()->showMessage(tr("Projet %1 enregistré dans le repertoire: %2.").arg(title).arg (filePath), 2000);
			m_element_collection_widget->highlightUnusedElement();
		}
		else {
			showError(save_file);
		}
	}
}

/**
	@brief QETDiagramEditor::newProject
	Create a new project with an empty diagram
	@return
*/
bool QETDiagramEditor::newProject()
{
	auto new_project = new QETProject(this);

	// add new diagram
	new_project -> addNewDiagram();

	return addProject(new_project);
}

/**
	Slot utilise pour ouvrir un fichier recent.
	Transfere filepath au slot openAndAddDiagram seulement si cet editeur est
	actif
	@param filepath Fichier a ouvrir
	@see openAndAddDiagram
*/
bool QETDiagramEditor::openRecentFile(const QString &filepath)
{
	// small hack to prevent all diagram editors from trying to topen the required
	// recent file at the same time
	if (qApp -> activeWindow() != this) return(false);
	return(openAndAddProject(filepath));
}

/**
	Cette fonction demande un nom de fichier a ouvrir a l'utilisateur
	@return true si l'ouverture a reussi, false sinon
*/
bool QETDiagramEditor::openProject()
{
	// demande un chemin de fichier a ouvrir a l'utilisateur
	QString filepath = QFileDialog::getOpenFileName(
		this,
		tr("Ouvrir un fichier"),
		open_dialog_dir.absolutePath(),
		tr("Projets QElectroTech (*.qet);;Fichiers XML (*.xml);;Tous les fichiers (*)")
	);
	if (filepath.isEmpty()) return(false);

	// retient le dossier contenant le dernier projet ouvert
	open_dialog_dir = QDir(filepath);

	// ouvre le fichier
	return(openAndAddProject(filepath));
}

/**
	Ferme un projet
	@param project_view Projet a fermer
	@return true si la fermeture du projet a reussi, false sinon
	Note : cette methode renvoie true si project est nul
*/
bool QETDiagramEditor::closeProject(ProjectView *project_view)
{
	if (project_view) {
		activateProject(project_view);
		if (QMdiSubWindow *sub_window = subWindowForWidget(project_view)){
			return(sub_window -> close());
		}
	}
	return(true);
}

/**
	Ferme un projet
	@param project projet a fermer
	@return true si la fermeture du fichier a reussi, false sinon
	Note : cette methode renvoie true si project est nul
*/
bool QETDiagramEditor::closeProject(QETProject *project)
{
	if (ProjectView *project_view = findProject(project)) {
		return(closeProject(project_view));
	}
	return(true);
}

/**
	Ouvre un projet depuis un fichier et l'ajoute a cet editeur
	@param filepath Chemin du projet a ouvrir
	@param interactive true pour afficher des messages a l'utilisateur, false sinon
	@return true si l'ouverture a reussi, false sinon
*/
bool QETDiagramEditor::openAndAddProject(
		const QString &filepath,
		bool interactive)
{
	if (filepath.isEmpty()) return(false);

	QFileInfo filepath_info(filepath);

	//Check if project is not open in another editor
	if (QETDiagramEditor *diagram_editor = QETApp::diagramEditorForFile(filepath))
	{
		if (diagram_editor == this)
		{
			if (ProjectView *project_view = viewForFile(filepath))
			{
				activateWidget(project_view);
				show();
				activateWindow();
			}
			return(false);
		}
		else
		{
				//Ask to the other editor to display the file
			return(diagram_editor -> openAndAddProject(filepath));
		}
	}

	// check the file exists
	if (!filepath_info.exists())
	{
		if (interactive)
		{
			QET::QetMessageBox::critical(
				this,
				tr("Impossible d'ouvrir le fichier", "message box title"),
				QString(
					tr("Il semblerait que le fichier %1 que vous essayez d'ouvrir"
					" n'existe pas ou plus.")
				).arg(filepath)
			);
		}
		return(false);
	}

	//Check if file readable
	if (!filepath_info.isReadable())
	{
		if (interactive) {
			QET::QetMessageBox::critical(
				this,
				tr("Impossible d'ouvrir le fichier", "message box title"),
				tr("Il semblerait que le fichier que vous essayez d'ouvrir ne "
				"soit pas accessible en lecture. Il est donc impossible de "
				"l'ouvrir. Veuillez vérifier les permissions du fichier.")
			);
		}
		return(false);
	}

	//Check if file is read only
	if (!filepath_info.isWritable())
	{
		if (interactive) {
			QET::QetMessageBox::warning(
				this,
				tr("Ouverture du projet en lecture seule", "message box title"),
				tr("Il semblerait que le projet que vous essayez d'ouvrir ne "
				"soit pas accessible en écriture. Il sera donc ouvert en "
				"lecture seule.")
			);
		}
	}

		//Is somebody else working on this project right now? On a shared
		//environment this is the case that costs an afternoon, so it is said
		//before the file is loaded, with a name and an hour, and the project
		//opens read-only rather than becoming a second copy that will
		//overwrite the first.
	ProjectLock open_lock(filepath);
	bool held_by_somebody_else = open_lock.exists() && !open_lock.isStale();
	if (held_by_somebody_else && interactive)
	{
		const QMessageBox::StandardButton answer = QET::QetMessageBox::warning(
			this,
			tr("Projet déjà ouvert ailleurs", "message box title"),
			tr("Ce projet est ouvert par %1.\n\n"
			   "Ouvrir en lecture seule est le choix sûr : vous verrez le dessin, "
			   "sans risque d'écraser son travail. Forcer l'ouverture en écriture "
			   "n'a de sens que si vous savez que cette session n'existe plus.")
				.arg(open_lock.holder().description()),
			QMessageBox::Ok | QMessageBox::Ignore,
			QMessageBox::Ok);
		if (answer == QMessageBox::Ignore)
		{
			open_lock.forceRelease();
			held_by_somebody_else = false;
		}
	}

	//Create the project
	DialogWaiting::instance(this);

		//Per-project window for the font counters reported below; the folios
		//(and with them the stored font descriptions) are built between here
		//and the end of addProject(). RAII, because DialogWaiting pumps the
		//event loop during the load: a nested openAndAddProject() gets its
		//own window and this one resumes unharmed.
	QETUtils::FontRestorationScope font_scope;

	QETProject *project = new QETProject(filepath);
	if (project -> state() != QETProject::Ok)
	{
		if (interactive && project -> state() != QETProject::FileOpenDiscard)
		{
			QET::QetMessageBox::warning(
				this,
				tr("Échec de l'ouverture du projet", "message box title"),
				QString(
					tr(
						"Il semblerait que le fichier %1 ne soit pas un fichier"
						" projet QElectroTech. Il ne peut donc être ouvert.",
						"message box content"
					)
				).arg(filepath)
			);
		}
		delete project;
		DialogWaiting::dropInstance();
		return(false);
	}

	if (held_by_somebody_else) {
		project -> setReadOnly(true);
	} else {
		project -> acquireOpenLock();
	}

	QETApp::projectsRecentFiles() -> fileWasOpened(filepath);
	addProject(project);
	DialogWaiting::dropInstance();

		//Report font descriptions which could not be read as-is (written by
		//an incompatible Qt version or corrupted), so the user learns about
		//it from somewhere else than the console. See issue #553.
	const int salvaged_fonts = font_scope.salvaged();
	const int unreadable_fonts = font_scope.unreadable();
	if (salvaged_fonts || unreadable_fonts)
	{
		qInfo().nospace() << "Project font descriptions: "
				  << salvaged_fonts << " salvaged from a foreign format, "
				  << unreadable_fonts << " unreadable (default font applies)";
	}
	if (interactive && (salvaged_fonts || unreadable_fonts))
	{
		QStringList details;
		if (salvaged_fonts) {
			details << tr("%n description(s) de police écrite(s) dans un "
					  "format étranger ou corrompu ont été restaurée(s). "
					  "Elles seront réécrites dans un format stable au "
					  "prochain enregistrement du projet.",
					  "message box content",
					  salvaged_fonts);
		}
		if (unreadable_fonts) {
			details << tr("%n description(s) de police n'ont pas pu être "
					  "lue(s) ; la police par défaut sera utilisée pour "
					  "ces textes.",
					  "message box content",
					  unreadable_fonts);
		}
		QET::QetMessageBox::information(
			this,
			tr("Polices du projet", "message box title"),
			details.join("\n\n")
		);
	}

	BackupDialog backup_dialog(this);
	if (backup_dialog.exec() == QDialog::Accepted)
	{
		QString backup_path = filepath_info.absolutePath() + QDir::separator() +
			QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm") + "_" +
			filepath_info.fileName();
		QFile::copy(filepath, backup_path);
	}

	return true;
}

/**
	Ajoute un projetmoveDiagramUp(
	@param project projet a ajouter
	@param update_panel Whether the elements panel should be warned this
	project has been added. Defaults to true.
*/
bool QETDiagramEditor::addProject(QETProject *project, bool update_panel)
{
	// enregistre le projet
	QETApp::registerProject(project);

	// cree un ProjectView pour visualiser le projet
	ProjectView *project_view = new ProjectView(project);
	addProjectView(project_view);

	undo_group.addStack(project -> undoStack());

	connect(project, &QETProject::projectModified, this, [this](QETProject *modified_project, bool) {
		if (modified_project == currentProject()) {
			updateWindowModifiedState();
		}
	});

	m_element_collection_widget->addProject(project);

	// met a jour le panel d'elements
	if (update_panel) {
		pa -> elementsPanel().projectWasOpened(project);
		if (currentDiagramView() != nullptr)
		m_autonumbering_dock->setProject(project, project_view);
	}

	return(true);
}

/**
	@return la liste des projets ouverts dans cette fenetre
*/
QList<ProjectView *> QETDiagramEditor::openedProjects() const
{
	QList<ProjectView *> result;
	QList<QMdiSubWindow *> window_list(m_workspace.subWindowList());
	foreach(QMdiSubWindow *window, window_list) {
		if (ProjectView *project_view = qobject_cast<ProjectView *>(window -> widget())) {
			result << project_view;
		}
	}
	return(result);
}

/**
	@return Le projet actuellement edite (= qui a le focus dans l'interface
	MDI) ou 0 s'il n'y en a pas
*/
ProjectView *QETDiagramEditor::currentProjectView() const
{
	QMdiSubWindow *current_window = m_workspace.activeSubWindow();
	if (!current_window) return(nullptr);

	QWidget *current_widget = current_window -> widget();
	if (!current_widget) return(nullptr);

	if (ProjectView *project_view = qobject_cast<ProjectView *>(current_widget)) {
		return(project_view);
	}
	return(nullptr);
}

/**
	@brief QETDiagramEditor::currentProject
	@return the current edited project.
	This function can return nullptr.
*/
QETProject *QETDiagramEditor::currentProject() const
{
	ProjectView *view = currentProjectView();
	if (view) {
		return view->project();
	}
	else {
		return nullptr;
	}
}

/**
	@return Le schema actuellement edite (= l'onglet ouvert dans le projet
	courant) ou 0 s'il n'y en a pas
*/
DiagramView *QETDiagramEditor::currentDiagramView() const
{
	if (ProjectView *project_view = currentProjectView()) {
		return(project_view -> currentDiagram());
	}
	return(nullptr);
}

/**
	@return the selected element in the current diagram view, or 0 if:
	  * no diagram is being viewed in this editor.
	  * no element is selected
	  * more than one element is selected
*/
Element *QETDiagramEditor::currentElement() const
{
	DiagramView *dv = currentDiagramView();
	if (!dv)
		return(nullptr);

	QList<Element *> selected_elements = DiagramContent(dv->diagram()).m_elements;
	if (selected_elements.count() != 1)
		return(nullptr);

	return(selected_elements.first());
}

/**
	Cette methode permet de retrouver le projet contenant un schema donne.
	@param diagram_view Schema dont il faut retrouver
	@return la vue sur le projet contenant ce schema ou 0 s'il n'y en a pas
*/
ProjectView *QETDiagramEditor::findProject(DiagramView *diagram_view) const
{
	foreach(ProjectView *project_view, openedProjects()) {
		if (project_view -> diagram_views().contains(diagram_view)) {
			return(project_view);
		}
	}
	return(nullptr);
}

/**
	Cette methode permet de retrouver le projet contenant un schema donne.
	@param diagram Schema dont il faut retrouver
	@return la vue sur le projet contenant ce schema ou 0 s'il n'y en a pas
*/
ProjectView *QETDiagramEditor::findProject(Diagram *diagram) const
{
	foreach(ProjectView *project_view, openedProjects()) {
		foreach(DiagramView *diagram_view, project_view -> diagram_views()) {
			if (diagram_view -> diagram() == diagram) {
				return(project_view);
			}
		}
	}
	return(nullptr);
}

/**
	@param project Projet dont il faut trouver la vue
	@return la vue du projet passe en parametre
*/
ProjectView *QETDiagramEditor::findProject(QETProject *project) const
{
	foreach(ProjectView *opened_project, openedProjects()) {
		if (opened_project -> project() == project) {
			return(opened_project);
		}
	}
	return(nullptr);
}

/**
	@param filepath Chemin de fichier d'un projet
	@return le ProjectView correspondant au chemin passe en parametre, ou 0 si
	celui-ci n'a pas ete trouve
*/
ProjectView *QETDiagramEditor::findProject(const QString &filepath) const
{
	foreach(ProjectView *opened_project, openedProjects()) {
		if (QETProject *project = opened_project -> project()) {
			if (project -> filePath() == filepath) {
				return(opened_project);
			}
		}
	}
	return(nullptr);
}

/**
	@param widget Widget a rechercher dans la zone MDI
	@return La sous-fenetre accueillant le widget passe en parametre, ou 0 si
	celui-ci n'a pas ete trouve.
*/
QMdiSubWindow *QETDiagramEditor::subWindowForWidget(QWidget *widget) const
{
	foreach(QMdiSubWindow *sub_window, m_workspace.subWindowList()) {
		if (sub_window -> widget() == widget) {
			return(sub_window);
		}
	}
	return(nullptr);
}

/**
	@param widget Widget a activer
*/
void QETDiagramEditor::activateWidget(QWidget *widget) {
	QMdiSubWindow *sub_window = subWindowForWidget(widget);
	if (sub_window) {
		m_workspace.setActiveSubWindow(sub_window);
	}
}

void QETDiagramEditor::zoomGroupTriggered(QAction *action)
{
	QString value = action->data().toString();
	DiagramView *dv = currentDiagramView();

	if (!dv || value.isEmpty()) return;

	if (value == "zoom_in")
		dv->zoom(1.15);
	else if (value == "zoom_out")
		dv->zoom(0.85);
	else if (value == "zoom_content")
		dv->zoomContent();
	else if (value == "zoom_fit")
		dv->zoomFit();
	else if (value == "zoom_reset")
		dv->zoomReset();
}

/**
	@brief QETDiagramEditor::selectGroupTriggered
	This slot is called when selection need to change.
	@param action : Action that describes what to do.
*/
void QETDiagramEditor::selectGroupTriggered(QAction *action)
{
	if (!currentDiagramView() || !currentDiagramView()->diagram())
		return;

	auto value = action->data().toString();
	if (value.isEmpty())
		return;

	auto diagram = currentDiagramView()->diagram();

	if (value == "select_all")
		diagram->selectAll();
	else if (value == "deselect")
		diagram->deselectAll();
	else if (value == "invert_selection")
		diagram->invertSelection();
}

/**
	@brief QETDiagramEditor::addItemGroupTriggered
	This slot is called when an item must be added to the current diagram,
	this slot use the DVEventInterface to add item
	@param action : Action that describe the item to add.
*/
void QETDiagramEditor::addItemGroupTriggered(QAction *action)
{
	QString value = action->data().toString();

	if (Q_UNLIKELY (!currentDiagramView() || !currentDiagramView()->diagram() || value.isEmpty())) return;

	Diagram *d = currentDiagramView()->diagram();
	DiagramEventInterface *diagram_event = nullptr;

	if (value == "line")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Line);
	else if (value == "rectangle")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Rectangle);
	else if (value == "ellipse")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Ellipse);
	else if (value == "polyline")
	{
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Polygon);
		statusBar()-> showMessage(tr("Double-click pour terminer la forme, Click droit pour annuler le dernier point"));
		connect(diagram_event, &DiagramEventInterface::destroyed, [this]() {
		statusBar()->clearMessage();
		});
	}
	else if (value == "image")
	{
		DiagramEventAddImage *deai = new DiagramEventAddImage(d);
		if (deai->isNull())
		{
			delete deai;
			return;
		}
		else
			diagram_event = deai;
	}
	else if (value == "text")
	{
		diagram_event = new DiagramEventAddText(d);
	}
	else if (value == QLatin1String("terminal_strip"))
	{
		const auto diagram_view{currentDiagramView()};
		if (diagram_view)
		{
			AddTerminalStripItemDialog::openDialog(diagram_view->diagram(), this);
		}
	}

	if (diagram_event)
	{
		d->setEventInterface(diagram_event);
		connect(diagram_event, &DiagramEventInterface::destroyed, [action]() {action->setChecked(false);});
	}
}

/**
	@brief QETDiagramEditor::selectionGroupTriggered
	This slot is called when an action should be made on the current selection
	@param action : Action that describe the action to do.
*/
void QETDiagramEditor::selectionGroupTriggered(QAction *action)
{
	QString value = action->data().toString();
	DiagramView *dv = currentDiagramView();
	Diagram *diagram = dv->diagram();
	DiagramContent dc(diagram);

	if (!dv || value.isEmpty()) return;

        if (value == "delete_selection")
        {
            if (DeleteQGraphicsItemCommand::hasNonDeletableTerminal(dc)) {
                QET::QetMessageBox::information(this,
                                                tr("Suppression de borne impossible"),
                                                tr("La suppression ne peut être effectué car la selection "
												   "possède une ou plusieurs bornes ponté et/ou appartenant à une borne à niveau multiple.\n"
                                                   "Déponter et/ou supprimer les niveaux des bornes concerné "
                                                   "afin de pouvoir les supprimer"));
            } else {
                diagram->clearSelection();
                diagram->undoStack().push(new DeleteQGraphicsItemCommand(diagram, dc));
                dv->adjustSceneRect();
            }
        }
	else if (value == "rotate_selection")
	{
		RotateSelectionCommand *c = new RotateSelectionCommand(diagram);
		if(c->isValid())
			diagram->undoStack().push(c);
	}
	else if (value == "rotate_group_selection")
	{
		RotateSelectionCommand *c = new RotateSelectionCommand(diagram, 90, nullptr, true);
		if(c->isValid())
			diagram->undoStack().push(c);
	}
	else if (value == "rotate_selected_text")
		diagram->undoStack().push(new RotateTextsCommand(diagram));
	else if (value == "find_selected_element" && currentElement())
		findElementInPanel(currentElement()->location());
	else if (value == "edit_selected_element")
		dv->editSelection();
	else if (value == "group_selected_texts")
	{
		QList<DynamicElementTextItem *> deti_list = dc.m_element_texts.values();
		if(deti_list.size() <= 1)
			return;

		diagram->undoStack().push(new AddTextsGroupCommand(deti_list.first()->parentElement(), tr("Groupe"), deti_list));
	}
}

void QETDiagramEditor::rowColumnGroupTriggered(QAction *action)
{
	QString value = action->data().toString();
	DiagramView *dv = currentDiagramView();

	if (!dv || value.isEmpty() || dv->diagram()->isReadOnly()) return;

	Diagram *d = dv->diagram();
	BorderProperties old_bp = d->border_and_titleblock.exportBorder();
	BorderProperties new_bp = d->border_and_titleblock.exportBorder();

	if (value == "add_column")
		new_bp.columns_count += 1;
	else if (value == "remove_column")
		new_bp.columns_count -= 1;
	else if (value == "add_row")
		new_bp.rows_count += 1;
	else if (value == "remove_row")
		new_bp.rows_count -= 1;

	d->undoStack().push(new ChangeBorderCommand(d, old_bp, new_bp));
}

/**
	@brief QETDiagramEditor::slot_updateActions
	Manage actions
*/
void QETDiagramEditor::slot_updateActions()
{
	DiagramView *dv = currentDiagramView();
	ProjectView *pv = currentProjectView();

	bool opened_project = pv;
	bool opened_diagram = dv;
	bool editable_project = (pv && !pv -> project() -> isReadOnly());

	m_close_file->                  setEnabled(opened_project);
	m_save_file->                   setEnabled(opened_project);
	m_save_file_as->                setEnabled(opened_project);
	m_rotate_texts->                setEnabled(editable_project);
	m_export_to_images->            setEnabled(opened_diagram);
	m_print->                       setEnabled(opened_diagram);
	m_export_to_pdf->               setEnabled(opened_diagram);
	m_edit_diagram_properties->     setEnabled(opened_diagram);
	m_zoom_actions_group.           setEnabled(opened_diagram);
	m_select_actions_group.         setEnabled(opened_diagram);
	m_add_item_actions_group.       setEnabled(editable_project);
	m_row_column_actions_group.     setEnabled(editable_project);
	m_grey_background->             setEnabled(opened_diagram);
	m_draw_grid->                   setEnabled(opened_diagram);
	m_draw_guides->                 setEnabled(opened_diagram);

		//Project menu
	m_project_edit_properties     -> setEnabled(opened_project);
	m_project_add_diagram         -> setEnabled(editable_project);
	m_remove_diagram_from_project -> setEnabled(editable_project);
	m_clean_project               -> setEnabled(editable_project);
	m_add_summary                 -> setEnabled(editable_project);
	m_add_nomenclature            -> setEnabled(editable_project);
	m_csv_export                  -> setEnabled(editable_project);
	m_project_export_conductor_num-> setEnabled(opened_project);
	m_terminal_strip_dialog       -> setEnabled(editable_project);
	m_project_export_wiring_list  -> setEnabled(opened_project);
	m_terminal_numbering          -> setEnabled(editable_project);
	m_renumber_components         -> setEnabled(editable_project);
	m_iec_structure               -> setEnabled(editable_project);
	m_create_symbol               -> setEnabled(editable_project);
	m_explode_element             -> setEnabled(editable_project);
	m_show_conductor_text         -> setEnabled(editable_project);
	m_hide_conductor_text         -> setEnabled(editable_project);
	m_align_conductor_text        -> setEnabled(editable_project);
	m_replace_part                -> setEnabled(editable_project);
	m_save_group                  -> setEnabled(editable_project);
	m_insert_group                -> setEnabled(editable_project);
#ifdef QET_EXPORT_PROJECT_DB
	m_export_project_db           -> setEnabled(editable_project);
#endif
	m_project_terminalBloc        -> setEnabled(editable_project);

		//Catalog menu. Browsing and managing the catalog do not need a
		//project: the catalog belongs to the office, not to one drawing.
	m_catalog_assign              -> setEnabled(editable_project);
	m_link_accessory              -> setEnabled(editable_project);
	m_catalog_register            -> setEnabled(editable_project);
	m_catalog_missing             -> setEnabled(opened_project);
		//The environment belongs to the station, not to a project.
	m_environment                 -> setEnabled(true);
	m_catalog_import              -> setEnabled(true);
	m_catalog_repository          -> setEnabled(true);


	slot_updateUndoStack();
	slot_updateModeActions();
	slot_updatePasteAction();
	slot_updateComplexActions();
	slot_updateAutoNumDock();
}

/**
	@brief QETDiagramEditor::slot_updateAutoNumDock
	Update Auto Num Dock Widget when changing Project
*/
void QETDiagramEditor::slot_updateAutoNumDock()
{
	if ( m_workspace.subWindowList().indexOf(m_workspace.activeSubWindow()) != activeSubWindowIndex) {
			activeSubWindowIndex = m_workspace.subWindowList().indexOf(m_workspace.activeSubWindow());
			if (currentProjectView() != nullptr && currentDiagramView() != nullptr) {
				m_autonumbering_dock->setProject(currentProjectView()->project(),currentProjectView());
			}
	}
}

/**
	@brief QETDiagramEditor::slot_updateUndoStack
	Update the undo stack view
*/
void QETDiagramEditor::slot_updateUndoStack()
{
	if(currentProjectView())
		undo_group.setActiveStack(currentProjectView()->project()->undoStack());
}

/**
	@brief QETDiagramEditor::slot_updateComplexActions
	Manage the actions that need some conditions to be enabled or not.
	This method does nothing if there is no project opened
*/
void QETDiagramEditor::slot_updateComplexActions()
{
	DiagramView *dv = currentDiagramView();
	if(!dv)
	{
		QList <QAction *> action_list;
		action_list << m_conductor_reset
			    << m_find_element
			    << m_cut
			    << m_copy
			    << m_delete_selection
			    << m_rotate_selection
			    << m_rotate_group_selection
			    << m_edit_selection
			    << m_group_selected_texts;
		for(QAction *action : action_list)
			action->setEnabled(false);

		return;
	}

	Diagram *diagram_ = dv->diagram();
	DiagramContent dc(diagram_);
	bool ro = diagram_->isReadOnly();


	//Number of selected conductors
	int selected_conductors_count = diagram_->selectedConductors().count();
	m_conductor_reset->setEnabled(!ro && selected_conductors_count);

	// number of selected elements
	int selected_elements_count = dc.count(DiagramContent::Elements);
	m_find_element->setEnabled(selected_elements_count == 1);

	//Actions that need items (elements, conductors, texts...) selected, to be enabled
	bool copiable_items  = dc.hasCopiableItems();
	bool deletable_items = dc.hasDeletableItems();
	m_cut              -> setEnabled(!ro && copiable_items);
	m_copy             -> setEnabled(copiable_items);
	m_delete_selection -> setEnabled(!ro && deletable_items);
	m_rotate_selection -> setEnabled(!ro && diagram_->canRotateSelection());
	m_rotate_group_selection -> setEnabled(!ro && diagram_->canRotateSelection());

		//Action that need selected texts or texts group
	QList<DiagramTextItem *> texts = DiagramContent(diagram_).selectedTexts();
	QList<ElementTextItemGroup *> groups = DiagramContent(diagram_).selectedTextsGroup();
	int selected_texts = texts.count();
	int selected_conductor_texts   = 0;
	for(DiagramTextItem *dti : texts)
	{
		if(dti->type() == ConductorTextItem::Type)
			selected_conductor_texts++;
	}
	int selected_dynamic_elmt_text = 0;
	for(DiagramTextItem *dti : texts)
	{
		if(dti->type() == DynamicElementTextItem::Type)
			selected_dynamic_elmt_text++;
	}
	m_rotate_texts->setEnabled(!ro && (selected_texts || groups.size()));

	//Action that need only element text selected
	QList<DynamicElementTextItem *> deti_list = dc.m_element_texts.values();
	if(deti_list.size() > 1 && dc.count() == deti_list.count())
	{
		Element *elmt = deti_list.first()->parentElement();
		bool ok = true;
		for(DynamicElementTextItem *deti : deti_list)
		{
			if(elmt != deti->parentElement())
				ok = false;
		}
		m_group_selected_texts->setEnabled(!ro && ok);
	}
	else
		m_group_selected_texts->setDisabled(true);

	// actions need only one editable item
	int selected_image = dc.count(DiagramContent::Images);

	int selected_shape = dc.count(DiagramContent::Shapes);
	int selected_editable = selected_elements_count
			+ (selected_texts
			   - selected_conductor_texts
			   - selected_dynamic_elmt_text)
			+ selected_image
			+ selected_shape
			+ selected_conductors_count;

	if (selected_editable == 1)
	{
		m_edit_selection -> setEnabled(true);
		//edit element
		if (selected_elements_count)
		{
			m_edit_selection -> setText(tr("Éditer l'élement",
						       "edit element"));
			m_edit_selection -> setIcon(QET::Icons::ElementEdit);
		}
		//edit text field
		else if (selected_texts)
		{
			m_edit_selection -> setText(tr("Éditer le champ de texte",
						       "edit text field"));
			m_edit_selection -> setIcon(QET::Icons::EditText);
		}
		//edit image
		else if (selected_image)
		{
			m_edit_selection -> setText(tr("Éditer l'image",
						       "edit image"));
			m_edit_selection -> setIcon(QET::Icons::resize_image);
		}
		//edit conductor
		else if (selected_conductors_count)
		{
			m_edit_selection -> setText(tr("Éditer le conducteur",
						       "edit conductor"));
			m_edit_selection -> setIcon(QET::Icons::ConductorEdit);
		}
	}
	//not an editable item
	else
	{
		m_edit_selection -> setText(tr("Éditer l'objet sélectionné",
					       "edit selected item"));
		m_edit_selection -> setIcon(QET::Icons::ElementEdit);
		m_edit_selection -> setEnabled(false);
	}

	//Actions for edit Z value
	QList<QGraphicsItem *> list = dc.items(
				DiagramContent::SelectedOnly
				| DiagramContent::Elements
				| DiagramContent::Shapes
				| DiagramContent::Images);
	m_depth_action_group->setEnabled(list.isEmpty()? false : true);
}

/**
	@brief QETDiagramEditor::slot_updateModeActions
	Manage action who need an opened diagram or project to be updated
*/
void QETDiagramEditor::slot_updateModeActions()
{
	DiagramView *dv = currentDiagramView();

	if (!dv)
		grp_visu_sel -> setEnabled(false);
	else
	{
		switch((int)(dv -> dragMode()))
		{
			case QGraphicsView::NoDrag:
				grp_visu_sel -> setEnabled(false);
				break;
			case QGraphicsView::ScrollHandDrag:
				grp_visu_sel -> setEnabled(true);
				m_mode_visualise -> setChecked(true);
				break;
			case QGraphicsView::RubberBandDrag:
				grp_visu_sel -> setEnabled(true);
				m_mode_selection -> setChecked(true);
				break;
		}
	}

	if (ProjectView *pv = currentProjectView())
	{
		m_auto_conductor -> setEnabled (true);
		m_auto_conductor -> setChecked (pv -> project() -> autoConductor());
		m_auto_break_conductor -> setEnabled (true);
		m_auto_break_conductor -> setChecked (pv -> project() -> autoBreakConductor());
	}
	else
	{
		m_auto_conductor -> setDisabled(true);
		m_auto_break_conductor -> setDisabled(true);
	}
}

/**
	@brief QETDiagramEditor::slot_updatePasteAction
	Gere les actions ayant besoin du presse-papier
*/
void QETDiagramEditor::slot_updatePasteAction()
{
	DiagramView *dv = currentDiagramView();
	bool editable_diagram = (dv && !dv -> diagram() -> isReadOnly());

	// pour coller, il faut un schema ouvert et un schema dans le presse-papier
	m_paste -> setEnabled(editable_diagram && Diagram::clipboardMayContainDiagram());
}

/**
	@brief QETDiagramEditor::addProjectView
	Add a new project view to workspace and build the connection between
	the projectview / project and this QETDiagramEditor.
	@param project_view : project view to add
*/
void QETDiagramEditor::addProjectView(ProjectView *project_view)
{
	if (!project_view) return;

	foreach(DiagramView *dv, project_view -> diagram_views())
		diagramWasAdded(dv);

	//Manage the close event of project
	connect(project_view, &ProjectView::projectClosed, this, &QETDiagramEditor::projectWasClosed);
	//Manage the adding  of diagram
	connect(project_view, qOverload<DiagramView*>(&ProjectView::diagramAdded), this, &QETDiagramEditor::diagramWasAdded);

	if (QETProject *project = project_view -> project())
		connect(project, &QETProject::readOnlyChanged, this, &QETDiagramEditor::slot_updateActions);

	//Manage request for edit or find element and titleblock
	connect (project_view, &ProjectView::findElementRequired,
		 this, &QETDiagramEditor::findElementInPanel);

	// display error messages sent by the project view
	connect(project_view, &ProjectView::errorEncountered, this, qOverload<const QString&>(&QETDiagramEditor::showError));

	//Highlight the current page
	connect(project_view, &ProjectView::diagramActivated, this, [this](DiagramView *dv) {
		if (dv && dv->diagram() && pa) {
			// 1. Find the item in the tree that corresponds to this diagram
			QTreeWidgetItem *item = pa->elementsPanel().getItemForDiagram(dv->diagram());

				   // 2. If you find it, select it
			if (item) {
				pa->elementsPanel().setCurrentItem(item);
			}
		}
	});

		//Highlight the current page in projectView on project activation
	connect(this, &QETDiagramEditor::syncElementsPanel, this, [this]() {
		if (pa && currentDiagramView()) {
				// In the tree, find the element that corresponds to the diagram of the selected project.
			QTreeWidgetItem *item = pa->elementsPanel().getItemForDiagram(currentDiagramView()->diagram());
			if (item) {
					// select the diagram
				pa->elementsPanel().setCurrentItem(item);
			}
		}
	});

	//We maximise the new window if the current window is inexistent or maximized
	QWidget *current_window = m_workspace.activeSubWindow();
	bool maximise = ((!current_window)
			 || (current_window -> windowState()
			     & Qt::WindowMaximized));

		//Add the new window
	QMdiSubWindow *sub_window = m_workspace.addSubWindow(project_view);
	sub_window -> setWindowIcon(project_view -> windowIcon());
	sub_window -> systemMenu() -> clear();

	//By default QMdiSubWindow have a QAction "close" with shortcut QKeySequence::Close
	//But the QAction m_close_file of this class have the same shortcut too.
	//We remove the shortcut of the QAction of QMdiSubWindow for avoid conflic
	for(QAction *act : sub_window->actions())
	{
		if(act->shortcut() == QKeySequence::Close)
			act->setShortcut(QKeySequence());
	}

		//Display the new window
	if (maximise) project_view -> showMaximized();
	else          project_view -> show();
}

/**
	@return la liste des fichiers edites par cet editeur de schemas
*/
QList<QString> QETDiagramEditor::editedFiles() const
{
	QList<QString> edited_files_list;
	foreach (ProjectView *project_view, openedProjects()) {
		QString diagram_file(project_view -> project() -> filePath());
		if (!diagram_file.isEmpty()) {
			edited_files_list << QFileInfo(diagram_file).canonicalFilePath();
		}
	}
	return(edited_files_list);
}

/**
	@param filepath Un chemin de fichier
	Note : si filepath est une chaine vide, cette methode retourne 0.
	@return le ProjectView editant le fichier filepath, ou 0 si ce fichier n'est
	pas edite par cet editeur de schemas.
*/
ProjectView *QETDiagramEditor::viewForFile(const QString &filepath) const
{
	if (filepath.isEmpty()) return(nullptr);

	QString searched_can_file_path = QFileInfo(filepath).canonicalFilePath();
	if (searched_can_file_path.isEmpty()) {
		// QFileInfo returns an empty path for non-existent files
		return(nullptr);
	}
	foreach (ProjectView *project_view, openedProjects()) {
		QString project_can_file_path = QFileInfo(project_view -> project() -> filePath()).canonicalFilePath();
		if (project_can_file_path == searched_can_file_path) {
			return(project_view);
		}
	}
	return(nullptr);
}

/**
	@brief QETDiagramEditor::drawGrid
	@return true if the grid of folio must be displayed
*/
bool QETDiagramEditor::drawGrid() const
{
	return m_draw_grid->isChecked();
}

/**
	@brief QETDiagramEditor::openBackupFiles
	@param backup_files
*/
void QETDiagramEditor::openBackupFiles(QList<KAutoSaveFile *> backup_files)
{
	for (KAutoSaveFile *file : backup_files)
	{
			//Create the project
		DialogWaiting::instance(this);

		QETProject *project = new QETProject(file, this);
		if (project->state() != QETProject::Ok)
		{
			if (project -> state() != QETProject::FileOpenDiscard)
			{
				QET::QetMessageBox::warning(
					this,
					tr("Échec de l'ouverture du projet", "message box title"),
					QString(tr(
						"Une erreur est survenue lors de l'ouverture du fichier %1.",
						"message box content")).arg(file->managedFile().fileName()));
			}
			delete project;
			DialogWaiting::dropInstance();
		}
		addProject(project);
		DialogWaiting::dropInstance();
	}
}
/**
	met a jour le menu "Fenetres"
*/
void QETDiagramEditor::slot_updateWindowsMenu()
{
	// nettoyage du menu
	foreach(QAction *a, windows_menu -> actions()) windows_menu -> removeAction(a);

	// actions de fermeture
	windows_menu -> addAction(m_close_file);
	//windows_menu -> addAction(closeAllAct);

	// actions de reorganisation des fenetres
	windows_menu -> addSeparator();
	windows_menu -> addAction(m_tile_window);
	windows_menu -> addAction(m_cascade_window);

	// actions de deplacement entre les fenetres
	windows_menu -> addSeparator();
	windows_menu -> addAction(m_next_window);
	windows_menu -> addAction(m_previous_window);

	// liste des fenetres
	QList<ProjectView *> windows = openedProjects();

	m_tile_window    -> setEnabled(!windows.isEmpty() && m_workspace.viewMode() == QMdiArea::SubWindowView);
	m_cascade_window -> setEnabled(!windows.isEmpty() && m_workspace.viewMode() == QMdiArea::SubWindowView);
	m_next_window    -> setEnabled(windows.count() > 1);
	m_previous_window    -> setEnabled(windows.count() > 1);

	if (!windows.isEmpty()) windows_menu -> addSeparator();
	QActionGroup *windows_actions = new QActionGroup(this);
	foreach(ProjectView *project_view, windows) {
		QString pv_title = project_view -> windowTitle();
		QAction *action  = windows_menu -> addAction(pv_title);
		windows_actions -> addAction(action);
		action -> setStatusTip(QString(tr("Active le projet « %1 »")).arg(pv_title));
		action -> setCheckable(true);
		action -> setChecked(project_view == currentProjectView());
		connect(action, &QAction::triggered, &windowMapper, qOverload<>(&QSignalMapper::map));		
		windowMapper.setMapping(action, project_view);
	}
}

/**
	Edite les proprietes du schema diagram
	@param diagram_view schema dont il faut editer les proprietes
*/
void QETDiagramEditor::editDiagramProperties(DiagramView *diagram_view)
{
	if (ProjectView *project_view = findProject(diagram_view)) {
		activateProject(project_view);
		project_view -> editDiagramProperties(diagram_view);
	}
}

/**
	Edite les proprietes du schema diagram
	@param diagram schema dont il faut editer les proprietes
*/
void QETDiagramEditor::editDiagramProperties(Diagram *diagram)
{
	if (ProjectView *project_view = findProject(diagram)) {
		activateProject(project_view);
		project_view -> editDiagramProperties(diagram);
	}
}

/**
	Affiche les projets dans des fenetres.
*/
void QETDiagramEditor::setWindowedMode()
{
	m_workspace.setViewMode(QMdiArea::SubWindowView);
	m_windowed_view_mode -> setChecked(true);
	slot_updateWindowsMenu();
}

/**
	Affiche les projets dans des onglets.
*/
void QETDiagramEditor::setTabbedMode()
{
	m_workspace.setViewMode(QMdiArea::TabbedView);
	m_tabbed_view_mode -> setChecked(true);
	slot_updateWindowsMenu();
}

/**
	@brief QETDiagramEditor::readSettings
	Read the settings
*/
void QETDiagramEditor::readSettings()
{
	QSettings settings;

	// dimensions et position de la fenetre
	QVariant geometry = settings.value("diagrameditor/geometry");
	if (geometry.isValid()) restoreGeometry(geometry.toByteArray());

	// etat de la fenetre (barres d'outils, docks...)
	QVariant state = settings.value("diagrameditor/state");
	if (state.isValid()) restoreState(state.toByteArray());

	// gestion des projets (onglets ou fenetres)
	bool tabbed = settings.value("diagrameditor/viewmode", "tabbed") == "tabbed";
	if (tabbed) {
		setTabbedMode();
	} else {
		setWindowedMode();
	}
}

/**
	@brief QETDiagramEditor::writeSettings
	Write the settings
*/
void QETDiagramEditor::writeSettings()
{
	QSettings settings;
	settings.setValue("diagrameditor/geometry", saveGeometry());
	settings.setValue("diagrameditor/state", saveState());
}

/**
	Active le projet passe en parametre
	@param project Projet a activer
*/
void QETDiagramEditor::activateProject(QETProject *project)
{
	activateProject(findProject(project));
}

/**
	Active le projet passe en parametre
	@param project_view Projet a activer
*/
void QETDiagramEditor::activateProject(ProjectView *project_view)
{
	if (!project_view) return;
	activateWidget(project_view);
}

/**
	@brief QETDiagramEditor::projectWasClosed
	Manage the close of a project.
	@param project_view
*/
void QETDiagramEditor::projectWasClosed(ProjectView *project_view)
{
	QETProject *project = project_view -> project();
	if (project)
	{
		pa -> elementsPanel().projectWasClosed(project);
		m_element_collection_widget->removeProject(project);
		undo_group.removeStack(project -> undoStack());
		QETApp::unregisterProject(project);
	}
	//When project is closed, a lot of signals are emitted, notably if there is an item selected in a diagram.
	//In some special case, since signal/slot connection can be direct or queued, some signals are handled after QObject is deleted, and crash qet
	//notably in the function Diagram::elements when it calls items() (I don't know exactly why).
	//set nullptr to "m_selection_properties_editor->setDiagram()" fixes this crash
	m_selection_properties_editor->setDiagram(nullptr);
	project_view -> deleteLater();
	project -> deleteLater();
}

/**
	Edite les proprietes du projet project_view.
	@param project_view Vue sur le projet dont il faut editer les proprietes
*/
void QETDiagramEditor::editProjectProperties(ProjectView *project_view)
{
	if (!project_view) return;
	activateProject(project_view);
	project_view -> editProjectProperties();
}

/**
	Edite les proprietes du projet project.
	@param project Projet dont il faut editer les proprietes
*/
void QETDiagramEditor::editProjectProperties(QETProject *project)
{
	editProjectProperties(findProject(project));
}

/**
	@brief QETDiagramEditor::addDiagramToProject
	Add a diagram to project
	@param project
*/
void QETDiagramEditor::addDiagramToProject(QETProject *project)
{
	if (!project) {
		return;
	}

	if (ProjectView *project_view = findProject(project))
	{
		activateProject(project);
		project_view->project()->addNewDiagram();
	}
}

/**
	@brief QETDiagramEditor::addDiagramToProjectAt
	Add a diagram to project, inserted at a specific position.
	@param project
	@param pos
*/
void QETDiagramEditor::addDiagramToProjectAt(QETProject *project, int pos)
{
	if (!project) {
		return;
	}

	if (ProjectView *project_view = findProject(project))
	{
		activateProject(project);
		project_view->project()->addNewDiagram(pos);
	}
}
/**
 * @brief QETDiagramEditor::removeDiagram
 * Wrapper für einzelne Diagramme, um Abwärtskompatibilität zu erhalten.
 */
void QETDiagramEditor::removeDiagram(Diagram *diagram)
{
	if (!diagram) return;
	QList<Diagram *> list;
	list << diagram;
	removeDiagrams(list);
}

/**
 * @brief QETDiagramEditor::removeDiagrams
 * Deletes a list of folios with a single query.
 */
void QETDiagramEditor::removeDiagrams(const QList<Diagram *> &diagrams)
{
	if (diagrams.isEmpty()) return;

	if (diagrams.count() == 1) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, tr("Supprimer le folio"),
									  tr("Êtes-vous sûr de vouloir supprimer ce folio ?"),
									  QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::No) return;
	} else {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, tr("Supprimer les folios"),
									  tr("Êtes-vous sûr de vouloir supprimer les %1 folios sélectionnés ?").arg(diagrams.count()),
									  QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::No) return;
	}

	ProjectView *project_view = nullptr;
	QETProject *project = diagrams.first()->project();
	if (project) {
		project_view = findProject(project);
	}

	if (project_view) project_view->setUpdatesEnabled(false);
	if (pa) pa->setUpdatesEnabled(false);

	if (project) {
		project->undoStack()->beginMacro(diagrams.count() == 1
			? tr("Supprimer le folio")
			: tr("Supprimer %1 folios").arg(diagrams.count()));
	}

	foreach (Diagram *diagram, diagrams) {
		removeDiagramSilent(diagram);
	}

	if (project) project->undoStack()->endMacro();

	if (pa) pa->setUpdatesEnabled(true);
	if (project_view) project_view->setUpdatesEnabled(true);

	emit syncElementsPanel();
}

/**
	Supprime un schema de son projet
	@param diagram Schema a supprimer
*/
void QETDiagramEditor::removeDiagramSilent(Diagram *diagram)
{
	if (!diagram) return;

	if (QETProject *diagram_project = diagram -> project()) {
		if (ProjectView *project_view = findProject(diagram_project)) {

			// supprime le schema
			project_view -> removeDiagram(diagram, true);
		}
	}
}
void QETDiagramEditor::moveDiagramUp(const QList<Diagram *> &diagrams) {
	if (diagrams.isEmpty()) return;
	QList<Diagram *> safeDiagrams = diagrams;
	if (QETProject *diagram_project = safeDiagrams.first()->project()) {
		if (!diagram_project->isReadOnly()) {
			if (ProjectView *project_view = findProject(diagram_project)) {
				// Forward loop for moving up
				diagram_project->undoStack()->beginMacro(tr("Déplacer les folios"));
				for (int i = 0; i < safeDiagrams.size(); ++i) {
					project_view->moveDiagramUp(safeDiagrams.at(i));
					QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				diagram_project->undoStack()->endMacro();
			}
		}
	}
}

void QETDiagramEditor::moveDiagramDown(const QList<Diagram *> &diagrams) {
	if (diagrams.isEmpty()) return;
	QList<Diagram *> safeDiagrams = diagrams;
	if (QETProject *diagram_project = safeDiagrams.first()->project()) {
		if (!diagram_project->isReadOnly()) {
			if (ProjectView *project_view = findProject(diagram_project)) {
				// Backward loop for moving down
				diagram_project->undoStack()->beginMacro(tr("Déplacer les folios"));
				for (int i = safeDiagrams.size() - 1; i >= 0; --i) {
					project_view->moveDiagramDown(safeDiagrams.at(i));
					QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				diagram_project->undoStack()->endMacro();
			}
		}
	}
}

void QETDiagramEditor::moveDiagramUpTop(const QList<Diagram *> &diagrams) {
	if (diagrams.isEmpty()) return;
	QList<Diagram *> safeDiagrams = diagrams;
	if (QETProject *diagram_project = safeDiagrams.first()->project()) {
		if (!diagram_project->isReadOnly()) {
			if (ProjectView *project_view = findProject(diagram_project)) {
				// Backward loop to preserve relative order of the selected items when moving to top
				diagram_project->undoStack()->beginMacro(tr("Déplacer les folios"));
				for (int i = safeDiagrams.size() - 1; i >= 0; --i) {
					project_view->moveDiagramUpTop(safeDiagrams.at(i));
					QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				diagram_project->undoStack()->endMacro();
			}
		}
	}
}

void QETDiagramEditor::moveDiagramUpx10(const QList<Diagram *> &diagrams) {
	if (diagrams.isEmpty()) return;
	QList<Diagram *> safeDiagrams = diagrams;
	if (QETProject *diagram_project = safeDiagrams.first()->project()) {
		if (!diagram_project->isReadOnly()) {
			if (ProjectView *project_view = findProject(diagram_project)) {
				// Forward loop for moving up
				diagram_project->undoStack()->beginMacro(tr("Déplacer les folios"));
				for (int i = 0; i < safeDiagrams.size(); ++i) {
					project_view->moveDiagramUpx10(safeDiagrams.at(i));
					QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				diagram_project->undoStack()->endMacro();
			}
		}
	}
}

void QETDiagramEditor::moveDiagramDownx10(const QList<Diagram *> &diagrams) {
	if (diagrams.isEmpty()) return;
	QList<Diagram *> safeDiagrams = diagrams;
	if (QETProject *diagram_project = safeDiagrams.first()->project()) {
		if (!diagram_project->isReadOnly()) {
			if (ProjectView *project_view = findProject(diagram_project)) {
				// Backward loop for moving down
				diagram_project->undoStack()->beginMacro(tr("Déplacer les folios"));
				for (int i = safeDiagrams.size() - 1; i >= 0; --i) {
					project_view->moveDiagramDownx10(safeDiagrams.at(i));
					QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				diagram_project->undoStack()->endMacro();
			}
		}
	}
}

void QETDiagramEditor::moveDiagramUpx100(const QList<Diagram *> &diagrams) {
	if (diagrams.isEmpty()) return;
	QList<Diagram *> safeDiagrams = diagrams;
	if (QETProject *diagram_project = safeDiagrams.first()->project()) {
		if (!diagram_project->isReadOnly()) {
			if (ProjectView *project_view = findProject(diagram_project)) {
				// Forward loop for moving up
				diagram_project->undoStack()->beginMacro(tr("Déplacer les folios"));
				for (int i = 0; i < safeDiagrams.size(); ++i) {
					project_view->moveDiagramUpx100(safeDiagrams.at(i));
					QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				diagram_project->undoStack()->endMacro();
			}
		}
	}
}

void QETDiagramEditor::moveDiagramDownx100(const QList<Diagram *> &diagrams) {
	if (diagrams.isEmpty()) return;
	QList<Diagram *> safeDiagrams = diagrams;
	if (QETProject *diagram_project = safeDiagrams.first()->project()) {
		if (!diagram_project->isReadOnly()) {
			if (ProjectView *project_view = findProject(diagram_project)) {
				// Backward loop for moving down
				diagram_project->undoStack()->beginMacro(tr("Déplacer les folios"));
				for (int i = safeDiagrams.size() - 1; i >= 0; --i) {
					project_view->moveDiagramDownx100(safeDiagrams.at(i));
					QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				diagram_project->undoStack()->endMacro();
			}
		}
	}
}

void QETDiagramEditor::reloadOldElementPanel()
{
	pa->reloadAndFilter();
}

/**
	Supprime le schema courant du projet courant
*/
void QETDiagramEditor::removeDiagramFromProject()
{
	if (ProjectView *current_project = currentProjectView()) {
		if (DiagramView *current_diagram = current_project -> currentDiagram()) {
			current_project -> removeDiagram(current_diagram);
		}
	}
}

/**
	@brief QETDiagramEditor::diagramWasAdded
	Manage the adding of diagram view in a project
	@param dv : added diagram view
*/
void QETDiagramEditor::diagramWasAdded(DiagramView *dv)
{
	connect(dv->diagram(),
		&QGraphicsScene::selectionChanged,
		this,
		&QETDiagramEditor::selectionChanged,
		Qt::DirectConnection);
	connect(dv, &DiagramView::modeChanged, this, &QETDiagramEditor::slot_updateModeActions);
}

/**
	@brief QETDiagramEditor::findElementInPanel
	Find the item for location in the element panel
	@param location
*/
void QETDiagramEditor::findElementInPanel(const ElementsLocation &location)
{
	m_element_collection_widget->setCurrentLocation(location);
}

/**
	Show the error message contained in \a result.
*/
void QETDiagramEditor::showError(const QETResult &result)
{
	if (result.isOk()) return;
	showError(result.errorMessage());
}

/**
	Show the \a error message.
*/
void QETDiagramEditor::showError(const QString &error)
{
	if (error.isEmpty()) return;
	QET::QetMessageBox::critical(this, tr("Erreur", "message box title"), error);
}

/**
	@brief QETDiagramEditor::subWindowActivated
	Slot used to update menu and undo stack when subwindows of MDIarea was activated
	@param subWindows
*/
void QETDiagramEditor::subWindowActivated(QMdiSubWindow *subWindows)
{
	Q_UNUSED(subWindows)

	slot_updateActions();
	slot_updateWindowsMenu();
	emit syncElementsPanel();
	updateUsageTrackersActiveState();
	updateWindowModifiedState();
}

/**
	@brief QETDiagramEditor::updateUsageTrackersActiveState
	Mark the currently active project's usage tracker (time spent on this
	project) as active, and every other opened project's tracker as
	inactive. Called whenever the current MDI subwindow changes.

	Known limitation: this only accounts for tab switches within this
	QETDiagramEditor window. If the same project were ever shown as the
	active tab in two different windows at once, its tracked time could be
	double-counted -- today QETApp only ever gives a project one ProjectView,
	so this doesn't happen in practice.
*/
void QETDiagramEditor::updateUsageTrackersActiveState()
{
	QETProject *active_project = currentProject();
	const QList<ProjectView *> project_views = openedProjects();
	for (ProjectView *project_view : project_views) {
		if (QETProject *project = project_view->project()) {
			project->projectPropertiesHandler().usageTracker().setActive(project == active_project);
		}
	}
}

/**
	@brief QETDiagramEditor::updateWindowModifiedState
	Reflect the currently active project's unsaved-changes state in the
	main window's title and native "document modified" indicator (e.g.
	the dot in the close button on macOS). Called whenever the active
	project changes, or whenever the active project's own modified state
	changes.

	The window title's "[*]" placeholder is Qt's own convention: combined
	with setWindowModified(), it lets each platform render the modified
	indicator its own way (or not at all, on platforms without one)
	without QET having to draw anything itself.
*/
void QETDiagramEditor::updateWindowModifiedState()
{
	if (QETProject *project = currentProject()) {
		setWindowTitle(QString("%1[*] - %2").arg(
			project->pathNameTitle(),
			tr("QElectroTech", "window title")));
		setWindowModified(project->projectOptionsWereModified());
	} else {
		setWindowTitle(tr("QElectroTech", "window title"));
		setWindowModified(false);
	}
}

/**
	@brief QETDiagramEditor::selectionChanged
	This slot is called when a diagram selection was changed.
*/
void QETDiagramEditor::selectionChanged()
{
	slot_updateComplexActions();

	DiagramView *dv = currentDiagramView();
	if (dv && dv->diagram())
		m_selection_properties_editor->setDiagram(dv->diagram());
}


/**
	@brief QETDiagramEditor::generateTerminalBlock
*/
void QETDiagramEditor::generateTerminalBlock()
{
#ifdef TODO_LIST
#	pragma message("@TODO Merge 'qet_tb_generator' code in to Qet")
#	pragma message("https://github.com/qelectrotech/qet_tb_generator")
#endif

	bool success = false;
	QList<QString> exeList;
	QProcess *process = new QProcess(qApp);

#if defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
	exeList << (QETApp::dataDir() + "/binary/qet_tb_generator.exe")
			<< (QDir::currentPath() + "/qet_tb_generator.exe")
			<< QStandardPaths::findExecutable("qet_tb_generator.exe")
			<< "qet_tb_generator.exe"
			<< "qet_tb_generator";    // from original code: missing ".exe" ???
#elif  defined(Q_OS_MACOS)
	exeList << (QETApp::dataDir() + "/binary/qet_tb_generator")
			<< (QDir::currentPath() + "/qet_tb_generator")
			<< QStandardPaths::findExecutable("qet_tb_generator")
			<< (QDir::homePath() + "/.qet/qet_tb_generator.app")
			<< "/Library/Frameworks/Python.framework/Versions/3.11/bin/qet_tb_generator";
#else
	exeList << (QETApp::dataDir() + "/binary/qet_tb_generator")
			<< (QDir::currentPath() + "/qet_tb_generator")
			<< (QDir::homePath() + "/.qet/qet_tb_generator")
			<< QStandardPaths::findExecutable("qet_tb_generator")
			<< "qet_tb_generator";
#endif

		// If launched under control:
		//connect(process, SIGNAL(errorOcurred(int error)), this, SLOT(slot_generateTerminalBlock_error()));
		//process->start("qet_tb_generator");

	qInfo() << " project to use for qet_tb_generator: "
			<< (QETDiagramEditor::currentProjectView()->project()->filePath());

	if (openedProjects().count()) {
		foreach(QString exe, exeList) {
			if ((success == false) && exe.length() && QFile::exists(exe)) {
				success = process->startDetached(exe, {(QETDiagramEditor::currentProjectView()->project()->filePath())});
			}
			if (success == true) {
				qInfo() << " qet_tb_generator found here:" << exe;
				break;
			} else {
				qInfo() << " qet_tb_generator not found :" << exe;
			}
		}
	} else {
		qInfo() << "No project loaded - no need to start \"qet_tb_generator\"";
	}
	process->close();

#if defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
	QString message=QObject::tr(
		"To install the plugin qet_tb_generator"
		"<br>Visit :"
		"<br>"
		"<a href='https://pypi.python.org/pypi/qet-tb-generator'>qet-tb-generator</a>"
		"<br>Requires python 3.5 or above."
		"<br><B><U> First install on Windows</B></U>"
		"<br>1. Install, if required, python 3.5 or above"
		"<br> Visit :"
		"<br>"
		"<a href='https://www.python.org/downloads/'>python.org</a>"
		"<br>2. pip install qet_tb_generator"
		"<br><B><U> Update on Windows</B></U>"
		"<br>python -m pip install --upgrade qet_tb_generator"
		"<br>"
		">>user could launch in a terminal this script in this directory"
		"<br>"
		" C:\\users\\XXXX\\AppData\\Local\\Programs\\Python\\Python36-32\\Scripts   "
		"<br>");
#elif defined(Q_OS_MACOS)
	QString message=QObject::tr(
		"To install the plugin qet_tb_generator"
		"<br>Visit  :"
		"<br>"
		"<a href='https://pypi.python.org/pypi/qet-tb-generator'>qet-tb-generator</a>"
		"<br><B><U> First install on macOSX</B></U>"
		"<br>1. Install, if required, python 3.11 bundle only, "
		"<a href='https://www.python.org/ftp/python/3.11.2/python-3.11.2-macos11.pkg'>python-3.11.2-macos11.pkg</a>"
		"<br>2 Run Profile.command script"
		"<br>"
		"because program use hardcoded PATH for localise qet-tb-generator plugin "
		"<br> Visit :"
		"<br>"
		"<a href='https://qelectrotech.org/forum/viewtopic.php?pid=5674#p5674'>howto</a>"
		"<br>2. pip3 install qet_tb_generator"
		"<br><B><U> Update on macOSX</B></U>"
		"<br> pip3 install --upgrade qet_tb_generator"
		"<br>");
#else
	QString message=QObject::tr(
		"To install the plugin qet_tb_generator"
		"<br>Visit :"
		"<br>"
		"<a href='https://pypi.python.org/pypi/qet-tb-generator'>qet-tb-generator</a>"
		"<br>"
		"<br>Requires python 3.5 or above."
		"<br>"
		"<br><B><U> First install on Linux</B>""</U>"
		"<br>1. check you have pip3 installed: pip3 --version"
		"<br>If not install with: sudo apt-get install python3-pip"
		"<br>2. Install the program: sudo pip3 install qet_tb_generator"
		"<br>3. Run the program: qet_tb_generator"
		"<br>"
		"<br><B>""<U> Update on Linux</B>""</U>"
		"<br>sudo pip3 install --upgrade qet_tb_generator"
		"<br>");
#endif
	if ( !success ) {
		QMessageBox::warning(nullptr,
							 QObject::tr("Error launching qet_tb_generator plugin"),
							 message);
	}
}

/**
 * @brief QETDiagramEditor::slot_terminalNumbering
 * Opens the dialog for automatic terminal numbering and applies the generated undo command.
 */
void QETDiagramEditor::slot_terminalNumbering() {
	QETProject *project = currentProject();
	if (!project) return;

	TerminalNumberingDialog dialog(this, project);
	if (dialog.exec() == QDialog::Accepted) {
		// Fetch the generated undo command from the dialog logic
		QUndoCommand *macro = dialog.getUndoCommand(project);

		// If changes were made, push them to the global undo stack
		if (macro) {
			undo_group.activeStack()->push(macro);
		}
	}
}
