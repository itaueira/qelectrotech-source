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

#include "ElementsCollection/embeddedelementrefresh.h"
#include "ElementsCollection/sheetsymbolextractor.h"
#include "ElementsCollection/ui/createsymboldialog.h"
#include "ElementsCollection/ui/pinoutgeneratordialog.h"
#include "ElementsCollection/ui/symbolgroupdialog.h"

#include "autoNum/ui/iecstructuredialog.h"
#include "autoNum/ui/renumberdialog.h"
#include "catalog/ui/catalogbrowserdialog.h"
#include "undocommand/addgraphicsobjectcommand.h"
#include "undocommand/explodeelementcommand.h"
#include "undocommand/swapelementcommand.h"
#include "undocommand/scaleelementscommand.h"
#include "undocommand/exchangeelementscommand.h"
#include "undocommand/syncsymboltextscommand.h"
#include "undocommand/conductortextcommand.h"
#include "catalog/ui/catalogreplacedialog.h"
#include "catalog/ui/catalogimportdialog.h"
#include "drc/ui/drcprojectactions.h"
#include "catalog/ui/catalogrepositorydialog.h"
#include "crashrecovery.h"
#include "environment/ui/environmentdialog.h"
#include "catalog/ui/catalogmanagerdialog.h"
#include "catalog/ui/catalogpartdialog.h"
#include "catalog/ui/catalogprojectactions.h"
#include "connector/ui/connectorreportdialog.h"
#include "container/qetcontainer.h"
#include "container/qetzip.h"
#include "utils/previousversions.h"
#include "shownkinds.h"
#include "symboltextplacement.h"
#ifdef QET_HAS_SCRIPTING
#include "scripting/qetscripting.h"
#include "scripting/scriptlibrary.h"
#include "scripting/scriptmanagerdialog.h"
#include "scripting/liveserver.h"
#include "scripting/macrorecorder.h"
#include "scripting/assistantinfo.h"
#endif
#include <QDomDocument>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QToolButton>
#include <QWidgetAction>
#include <QGridLayout>
#include "ElementsCollection/elementscollectionwidget.h"
#include "ElementsCollection/elementpickerpopup.h"
#include "shortcutbarsettings.h"
#include "diagramtoolbarsettings.h"
#include "qetgraphicsitem/conductor.h"
#include "itemgroups.h"
#include "commandsearchpopup.h"
#include "QWidgetAnimation/qwidgetanimation.h"
#include "autoNum/ui/autonumberingdockwidget.h"
#include "conductornumexport.h"
#include "diagramcommands.h"
#include "diagramcontent.h"
#include "diagramevent/diagrameventaddimage.h"
#include "diagramevent/diagrameventaddlocationarea.h"
#ifdef QET_HAS_QTPDF
#include "diagramevent/diagrameventaddpdf.h"
#endif
#include "diagramevent/diagrameventaddshape.h"
#include "diagramevent/diagrameventaddpath.h"
#include "diagramevent/diagrameventaddcable.h"
#include "diagramevent/diagrameventfillet.h"
#include "diagramevent/diagrameventaddtext.h"
#include "cable/cablepart.h"
#include "cable/editcablecommand.h"
#include "diagramevent/diagrameventaddpaste.h"
#include "diagramview.h"
#include "elementtextpattern.h"
#include "elementspanelwidget.h"
#include "factory/elementpicturefactory.h"
#include "factory/qetgraphicstablefactory.h"
#include "folionavigationbuttons.h"
#include "location/layout/mountinglayouteditor.h"
#include "location/mountinglayout.h"
#include "location/ui/locationbomdialog.h"
#include "location/ui/locationmanagerdialog.h"
#include "location/ui/locationreportdialog.h"
#include "macro/ui/circuittabledialog.h"
#include "plc/ui/ioassigndialog.h"
#include "plc/ui/ioimportdialog.h"
#include "plc/ui/iolistdialog.h"
#include "print/projectprintwindow.h"
#include "project/projectpropertieshandler.h"
#include "project/projecttemplate.h"
#include "ui/multielementinfowidget.h"
#include "ui/projectpropertiesdialog.h"
#include "projectview.h"
#include "qetproject.h"
#include "qetpalette.h"
#include "qetgraphicsitem/ViewItem/mountinglayoutviewitem.h"
#include "qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/elementtextitemgroup.h"
#include "qetgraphicsitem/qetgraphicsitem.h"
#include "qetgraphicsitem/stripframeitem.h"
#include "qetgraphicsitem/terminalbusitem.h"
#include "qeticons.h"
#include "qetmessagebox.h"
#include "recentfiles.h"
#include "textgrid.h"
#include "shortcutmanager.h"
#include "cablelist/cableexportdialog.h"
#include "ui/bomexportdialog.h"
#include "ui/conductorcolortoolbutton.h"
#include "ui/diagrambgcolorbutton.h"
#include "ui/duplicateoffsetdialog.h"
#include "ui/jumptoelementdialog.h"
#include "ui/navigatechoicedialog.h"
#include "ui/assemblystatedialog.h"
#include "ui/diagrampropertieseditordockwidget.h"
#include "ui/dialogwaiting.h"
#include "undocommand/addelementtextcommand.h"
#include "utils/apppreferences.h"
#include "undocommand/changeelementinformationcommand.h"
#include "qetinformation.h"
#include "utils/qetsettings.h"
#include "utils/qetutils.h"
#include "undocommand/groupitemscommand.h"
#include "undocommand/alignselectioncommand.h"
#include "undocommand/mirrorselectioncommand.h"
#include "undocommand/rotateselectioncommand.h"
#include "undocommand/rotatetextscommand.h"
#include "diagram.h"
#include "TerminalStrip/ui/terminalstripeditorwindow.h"
#include "ui/diagrameditorhandlersizewidget.h"
#include "TerminalStrip/ui/addterminalstripitemdialog.h"
#include "wiringlistexport.h"
#include "ui/wiringlistdialog.h"
#include "ui/terminalnumberingdialog.h"
#include "elementprovider.h"
#include "plc/iowiring.h"
#include "qetinformation.h"
#include "undocommand/changeelementinformationcommand.h"
#include "ui/cablenumberingdialog.h"
#include "ui/conductorpropertieseditorwidget.h"
#include "toolbarsettings.h"
#include "wirearrangement.h"
#include "wirecolorname.h"
#include "wirenames.h"
#include "wireremoval.h"
#include "wiresections.h"
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLocale>
#include <utility>

#include <QDesktopServices>
#include <QTimer>
#include <QGuiApplication>
#include <QPushButton>
#include <QMessageBox>
#include <QSpinBox>
#include <QClipboard>
#include <QPainter>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QListWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QDockWidget>
#include <QJsonObject>
#include <QTime>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QLabel>
#include <QUrl>
#include <algorithm>
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
	m_align_actions_group      (this),
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

	readSettings();  // restoreGeometry before show()
	show();
	readSettingsState();  // restoreState() must be called after show() in Qt6
	ToolbarSettings::applyTo(this);
#ifdef QET_HAS_SCRIPTING
	setUpLiveIndicator();
	setUpMacroRecorder();
		//Live mode asks once per run, from the first window to open, and
		//only once that window is on screen to anchor its warning.
	QTimer::singleShot(0, this, [this]() { LiveServer::instance().askAndStart(this); });
		//A toolbar saved as shown while there were scripts stays out of
		//the way while there are none.
	if (m_script_actions.isEmpty()) m_scripts_tool_bar->hide();
#endif

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
	qdw_pa = new QDockWidget(tr("Projects", "dock title"), this);

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

		//The widget does not know which view should receive the element -- it
		//is also used by the picker popup, which has no editor ancestor -- so
		//the host decides.
	connect(m_element_collection_widget,
		&ElementsCollectionWidget::insertElementRequested,
		this, &QETDiagramEditor::insertElementFromCollection);

	addDockWidget(Qt::RightDockWidgetArea, m_qdw_elmt_collection);
}

/**
	@brief QETDiagramEditor::setUpUndoStack
	Setup the undostack and undo stack widget
*/
void QETDiagramEditor::setUpUndoStack()
{

	QUndoView *undo_view = new QUndoView(&undo_group, this);

	undo_view -> setEmptyLabel (tr("No modification"));
	undo_view -> setStatusTip  (tr("Click an action to get back in the changes you made to your diagram", "Status tip"));
	undo_view -> setWhatsThis  (tr("This panel lists the various changes applied to the current sheet. Clicking a change gets the sheet back to its state right after it was applied.", "\"What's this\" tip"));

	qdw_undo  = new QDockWidget(tr("Undo", "dock title"), this);
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
	m_export_to_images = new QAction(QET::Icons::DocumentExport,  tr("&Export"), this);
	ShortcutManager::instance().registerAction(m_export_to_images, "diagrameditor.export_to_images", tr("Diagram editors"), Qt::CTRL | Qt::SHIFT | Qt::Key_X);
	m_export_to_images->setStatusTip(tr("Exports the current sheet to another format", "status bar tip"));
	connect(m_export_to_images, &QAction::triggered, [this]() {
		ProjectView *current_project = currentProjectView();
		if (current_project) {
			current_project -> exportProject();
		}
	});

		//Print
	m_print = new QAction(QET::Icons::DocumentPrint,   tr("Print"),  this);
	ShortcutManager::instance().registerAction(m_print, "diagrameditor.print", tr("Diagram editors"), QKeySequence::Print);
	m_print->setStatusTip(tr("Prints one or more sheet of the current project", "status bar tip"));
	connect(m_print, &QAction::triggered, [this]() {
		auto project = currentProject();
		if (project) {
			ProjectPrintWindow::launchDialog(project, QPrinter::NativeFormat ,this);
		}
	});

		//export to pdf
	m_export_to_pdf = new QAction(QET::Icons::PDF, tr("Export in pdf"), this);
	ShortcutManager::instance().registerAction(m_export_to_pdf, "diagrameditor.export_to_pdf", tr("Diagram editors"), QKeySequence());
	m_export_to_pdf->setStatusTip(tr("Export one or more sheets of the current project", "status bar tip"));
	connect(m_export_to_pdf, &QAction::triggered, [this] () {
		auto project = currentProject();
		if (project) {
			ProjectPrintWindow::launchDialog(project, QPrinter::PdfFormat, this);
		}
	});

		//Quit editor
	m_quit_editor = new QAction(QET::Icons::ApplicationExit, tr("&Quit"),  this);
	ShortcutManager::instance().registerAction(m_quit_editor, "diagrameditor.quit", tr("Diagram editors"), Qt::CTRL | Qt::Key_Q);
	m_quit_editor->setStatusTip(tr("Closes QElectroTech", "status bar tip"));
	connect(m_quit_editor, &QAction::triggered, this, &QETDiagramEditor::close);

		//Undo
	undo = undo_group.createUndoAction(this, tr("Undo"));
	undo->setIcon(QET::Icons::EditUndo);
	ShortcutManager::instance().registerAction(undo, "diagrameditor.undo", tr("Diagram editors"), QKeySequence::Undo);
	undo->setStatusTip(tr("Undoes the previous action", "status bar tip"));
		//Redo
	redo = undo_group.createRedoAction(this, tr("Redo"));
	redo->setIcon(QET::Icons::EditRedo);
	ShortcutManager::instance().registerAction(redo, "diagrameditor.redo", tr("Diagram editors"), QKeySequence::Redo);
	redo->setStatusTip(tr("Restores the undone action", "status bar tip"));

		//cut copy past
	m_cut   = new QAction(QET::Icons::EditCut,   tr("Cu&t"), this);
	m_copy  = new QAction(QET::Icons::EditCopy,  tr("&Copy"), this);
	m_paste = new QAction(QET::Icons::EditPaste, tr("&Paste"), this);
	m_paste_origin = new QAction(QET::Icons::EditPaste, tr("Paste at the original position"), this);
	m_paste_element_info = new QAction(QET::Icons::EditPaste, tr("Paste element information"), this);

	ShortcutManager::instance().registerAction(m_cut, "diagrameditor.cut", tr("Diagram editors"), QKeySequence::Cut);
	ShortcutManager::instance().registerAction(m_copy, "diagrameditor.copy", tr("Diagram editors"), QKeySequence::Copy);
	ShortcutManager::instance().registerAction(m_paste, "diagrameditor.paste", tr("Diagram editors"), QKeySequence::Paste);
	ShortcutManager::instance().registerAction(m_paste_origin, "diagrameditor.paste_origin", tr("Diagram editors"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
		//No default shortcut: Ctrl+Shift+V is taken by m_paste_origin.
	ShortcutManager::instance().registerAction(m_paste_element_info, "diagrameditor.paste_element_info", tr("Diagram editors"), QKeySequence());

	m_cut   -> setStatusTip(tr("Puts selected elements into the clipboard", "status bar tip"));
	m_copy  -> setStatusTip(tr("Copies selected elements", "status bar tip"));
	m_paste -> setStatusTip(tr("Pastes elements from the clipboard into the sheet", "status bar tip"));
	m_paste_origin -> setStatusTip(tr("Places the clipboard's items at their original position and moves the cursor to that point", "status bar tip"));
	m_paste_element_info -> setStatusTip(tr("Copies the filled-in information of the element on the clipboard to the selected elements", "status bar tip"));
	m_paste_element_info -> setEnabled(false);

	connect(m_cut, &QAction::triggered, [this]() {
		if (currentDiagramView())
			currentDiagramView()->cut();
	});
	connect(m_copy, &QAction::triggered, [this]() {
		if (currentDiagramView())
			currentDiagramView()->copy();
	});

		//Both paste shortcuts share one starter; they differ only in the
		//placement mode handed to DiagramEventAddPaste.
	const auto start_paste = [this](DiagramEventAddPaste::PastePlacement placement) {
		auto *dv = currentDiagramView();
		if (!dv || !dv->diagram()) return;

			//Paste as a placement rather than dropping the items straight
			//down. Pasting in place put the copy exactly on top of the
			//original, where it was easy to miss entirely; now it appears
			//under the cursor and follows it until a click, Return, or Escape
			//to cancel -- the same interaction as placing a new element.
			//
			//dv->lastMousePos() (an ordinary Qt mouse-move position), not
			//QCursor::pos() (a global, OS-level cursor query): several
			//window managers and compositors -- Wayland in particular --
			//silently refuse that query, returning a stale or wrong
			//position, which is exactly what made the pasted content land
			//far from the cursor instead of under it.
		const QPoint last_pos = dv->lastMousePos();
		const QPoint view_pos = (last_pos.x() >= 0 && dv->viewport()->rect().contains(last_pos))
				? last_pos
				: dv->viewport()->rect().center();
		const QPointF start_pos = dv->mapToScene(view_pos);

		dv->diagram()->setEventInterface(
					new DiagramEventAddPaste(dv->diagram(), start_pos, placement));
	};
	connect(m_paste, &QAction::triggered, [start_paste]() {
		start_paste(DiagramEventAddPaste::UnderCursor);
	});
	connect(m_paste_origin, &QAction::triggered, [start_paste]() {
		start_paste(DiagramEventAddPaste::AtOrigin);
	});
	connect(m_paste_element_info, &QAction::triggered,
			this, &QETDiagramEditor::pasteElementInformations);

	//Format painter actions
	m_copy_text_format  = new QAction(QET::Icons::CopyElementTextFormat,  tr("Copy text format"), this);
	m_apply_text_format = new QAction(QET::Icons::ApplyElementTextFormat, tr("Apply text format"), this);

	ShortcutManager::instance().registerAction(m_copy_text_format, "diagrameditor.copy_text_format", tr("Diagram editors"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
	ShortcutManager::instance().registerAction(m_apply_text_format, "diagrameditor.apply_text_format", tr("Diagram editors"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_V));

	m_copy_text_format  -> setStatusTip(tr("Copy the text format of the selected element as a template", "status bar tip"));
	m_apply_text_format -> setStatusTip(tr("Apply the copied text format to the selected elements", "status bar tip"));

	connect(m_copy_text_format,  &QAction::triggered, this, &QETDiagramEditor::copyTextFormat);
	connect(m_apply_text_format, &QAction::triggered, this, &QETDiagramEditor::applyTextFormat);

		//The texts of the selected symbols, label and notes, as one block at
		//one of nine points around the symbol: the nine points of the text
		//alignment dialog. A click places them at the point chosen last.
	const auto chosen_point = []() {
		return Qt::Alignment(QSettings().value(QStringLiteral("diagrameditor/place_texts_point"),
											   int(Qt::AlignLeft | Qt::AlignVCenter)).toInt());
	};
		//The button shows where its click will put the texts
	m_place_texts = new QAction(SymbolTextPlacement::icon(chosen_point()), tr("Place symbol texts"), this);
	m_place_texts->setStatusTip(tr("Place the label and the notes of the selected symbols as one block beside them", "status bar tip"));
	ShortcutManager::instance().registerAction(m_place_texts, "diagrameditor.place_symbol_texts", tr("Diagram editors"), QKeySequence());
	{
		auto *menu = new QMenu(this);
		auto *grid_widget = new QWidget(menu);
		auto *grid = new QGridLayout(grid_widget);
		grid->setContentsMargins(6, 6, 6, 6);
		grid->setSpacing(2);
		const struct { Qt::Alignment where; const char *tip; } points[] = {
			{Qt::AlignLeft | Qt::AlignTop,        QT_TR_NOOP("Above, left")},
			{Qt::AlignHCenter | Qt::AlignTop,     QT_TR_NOOP("Above")},
			{Qt::AlignRight | Qt::AlignTop,       QT_TR_NOOP("Above, right")},
			{Qt::AlignLeft | Qt::AlignVCenter,    QT_TR_NOOP("Left")},
			{Qt::AlignCenter,                     QT_TR_NOOP("On the symbol")},
			{Qt::AlignRight | Qt::AlignVCenter,   QT_TR_NOOP("Right")},
			{Qt::AlignLeft | Qt::AlignBottom,     QT_TR_NOOP("Below, left")},
			{Qt::AlignHCenter | Qt::AlignBottom,  QT_TR_NOOP("Below")},
			{Qt::AlignRight | Qt::AlignBottom,    QT_TR_NOOP("Below, right")}};
		int n = 0;
		for (const auto &point : points)
		{
			auto *button = new QToolButton(grid_widget);
			button->setIcon(SymbolTextPlacement::icon(point.where));
			button->setIconSize(QSize(32, 32));
			button->setToolTip(tr(point.tip));
			button->setObjectName(QStringLiteral("place_texts_%1").arg(n));
			button->setFixedSize(38, 38);
			button->setAutoRaise(true);
			const Qt::Alignment where = point.where;
			connect(button, &QToolButton::clicked, this, [this, menu, where]() {
				menu->close();
				QSettings().setValue(QStringLiteral("diagrameditor/place_texts_point"), int(where));
				m_place_texts->setIcon(SymbolTextPlacement::icon(where));
				placeSymbolTexts(where);
			});
			grid->addWidget(button, n / 3, n % 3);
			++n;
		}
		auto *grid_action = new QWidgetAction(menu);
		grid_action->setDefaultWidget(grid_widget);
		menu->addAction(grid_action);
		m_place_texts->setMenu(menu);
	}
	connect(m_place_texts, &QAction::triggered, this, [this, chosen_point]() {
		placeSymbolTexts(chosen_point());
	});

		//The scale of the selected elements on the folio, these elements
		//only and not their symbol (T83): every scale, the one they all
		//have checked
	m_element_scale = new QAction(tr("Element scale"), this);
	m_element_scale->setStatusTip(tr("Draw the selected elements smaller or larger on the folio, without changing their symbol", "status bar tip"));
	{
		auto *menu = new QMenu(this);
		m_element_scale->setMenu(menu);
		const auto selected = [this]() {
			QList<Element *> elements;
			if (DiagramView *view = currentDiagramView(); view && view->diagram())
			{
				const QList<QGraphicsItem *> items = view->diagram()->selectedItems();
				for (QGraphicsItem *item : items) {
					if (auto *element = qgraphicsitem_cast<Element *>(item)) elements << element;
				}
			}
			return elements;
		};
			//The entries are made here, once, and not when the submenu
			//opens: the folio's context menu leaves out a submenu with no
			//enabled entry (DiagramView::contextMenuActions()), and one
			//still empty when that menu is built was always left out of it.
			//Each entry scales what is selected when it is chosen
		for (const qreal scale : Element::symbolScaleChoices())
		{
			QAction *action = menu->addAction(QStringLiteral("%1 %").arg(qRound(scale * 100)));
			action->setCheckable(true);
			action->setData(scale);
			connect(action, &QAction::triggered, this, [this, selected, scale]() {
				scaleElements(selected(), scale);
			});
		}
		connect(menu, &QMenu::aboutToShow, this, [menu, selected]() {
			const QList<Element *> elements = selected();
			const QList<QAction *> actions = menu->actions();
			for (QAction *action : actions)
			{
				bool all_at = !elements.isEmpty();
				for (const Element *element : elements)
				{
					if (!qFuzzyCompare(element->symbolScale(), action->data().toReal())) all_at = false;
				}
				action->setChecked(all_at);
			}
		});
	}

		//The frame of a strip drawn round the selected terminals, with its
		//name, and the terminals taken out of it (T85)
	m_strip_frame = new QAction(tr("Drawn terminal strip"), this);
	m_strip_frame->setStatusTip(tr("Draw the frame of a strip round the selected terminals, with its name", "status bar tip"));
	{
		auto *menu = new QMenu(this);
		m_strip_frame->setMenu(menu);
		const auto selected = [this]() {
			DiagramView *view = currentDiagramView();
			return view && view->diagram()
					? StripFrameItem::stripTerminals(view->diagram()->selectedItems())
					: QList<Element *>();
		};
		QAction *draw = menu->addAction(tr("Draw round the selected terminals..."));
		connect(draw, &QAction::triggered, this, [this, selected]() {
			DiagramView *view = currentDiagramView();
			const QList<Element *> terminals = selected();
			if (!view || terminals.isEmpty()) return;
			bool ok = false;
			const QString name = StripFrameItem::askName(this, StripFrameItem::suggestedName(terminals), &ok);
			if (ok) {
				StripFrameItem::drawAround(view->diagram(), terminals, name);
			}
		});
		QAction *take_out = menu->addAction(tr("Take the selected terminals out of their strip"));
		connect(take_out, &QAction::triggered, this, [this, selected]() {
			if (DiagramView *view = currentDiagramView()) {
				StripFrameItem::takeOut(view->diagram(), selected());
			}
		});
			//The selected terminals joined by a bridge of their strip, a
			//comb the fitter puts on them, and the wires it makes
			//redundant taken out (T87)
		menu->addSeparator();
		QAction *bus = menu->addAction(tr("Join into a bus bar"));
		bus->setStatusTip(tr("Join the selected terminals with a bridge of their strip, fed by one wire", "status bar tip"));
		connect(bus, &QAction::triggered, this, [this, selected]() {
			DiagramView *view = currentDiagramView();
			const QList<Element *> terminals = selected();
			if (!view || terminals.isEmpty()) return;
			QString name;
			if (TerminalBusItem::needsStripName(terminals))
			{
				bool ok = false;
				name = StripFrameItem::askName(this, QString(), &ok);
				if (!ok) return;
			}
			QString why;
			if (!TerminalBusItem::join(view->diagram(), terminals, name, &why) && !why.isEmpty()) {
				QET::QetMessageBox::information(this, tr("Join into a bus bar"), why);
			}
		});
	}

		//Duplicate: copy the selection and place it at a configured,
		//grid-step offset immediately -- no interactive follow-the-
		//cursor step, unlike Ctrl+V above. That is deliberate (#991):
		//the point of a duplicate shortcut is repeatable, unattended
		//stamping (configure the offset once, then tap Ctrl+D to lay
		//out a row), which an interactive placement would interrupt on
		//every press.
	m_duplicate = new QAction(QET::Icons::EditCopy, tr("&Duplicate"), this);
	ShortcutManager::instance().registerAction(m_duplicate, "diagrameditor.duplicate", tr("Diagram editors"), Qt::CTRL | Qt::Key_D);
	m_duplicate->setStatusTip(tr("Copies the selection, offset by the configured spacing", "status bar tip"));
	connect(m_duplicate, &QAction::triggered, [this]() {
		auto *dv = currentDiagramView();
		if (!dv || !dv->diagram()) return;

			//Ask the first time only -- every later press reuses whatever
			//was confirmed then, so the shortcut can be tapped repeatedly
			//without an interruption each time. m_configure_duplicate
			//below is the deliberate way back into this dialog.
		if (!DuplicateOffsetDialog::hasSavedStepOffset()) {
			DuplicateOffsetDialog dialog(this);
			if (dialog.exec() != QDialog::Accepted) return;
			DuplicateOffsetDialog::saveStepOffset(dialog.stepOffset());
		}
		dv->duplicate(DuplicateOffsetDialog::savedStepOffset());
	});

		//Reopens the dialog above on demand, to change the spacing or
		//direction a later Ctrl+D should use. Enabled unconditionally
		//(see slot_updateComplexActions()): it only ever writes a
		//setting, so it does not need a diagram open or anything
		//selected the way m_duplicate itself does.
	m_configure_duplicate = new QAction(tr("Configure duplication..."), this);
	ShortcutManager::instance().registerAction(m_configure_duplicate, "diagrameditor.configure_duplicate", tr("Diagram editors"), QKeySequence());
	m_configure_duplicate->setStatusTip(tr("Choose the spacing and direction used by Duplicate", "status bar tip"));
	connect(m_configure_duplicate, &QAction::triggered, [this]() {
		DuplicateOffsetDialog dialog(this);
		if (dialog.exec() == QDialog::Accepted) {
			DuplicateOffsetDialog::saveStepOffset(dialog.stepOffset());
		}
	});

		//Reset conductor path
	m_conductor_reset = new QAction(QET::Icons::ConductorSettings,     tr("Reset conductors"),        this);
	ShortcutManager::instance().registerAction(m_conductor_reset, "diagrameditor.conductor_reset", tr("Diagram editors"), Qt::CTRL | Qt::Key_K);
	m_conductor_reset->setStatusTip(tr("Reset the conductors path ignoring the user changes", "status bar tip"));
	connect(m_conductor_reset, &QAction::triggered, [this]() {
		if (DiagramView *dv = currentDiagramView())
			dv->resetConductors();
	});

		//Arrange conductors: the bend in the free space, one lane each (T67)
	m_conductor_arrange = new QAction(QET::Icons::ConductorSettings, tr("Arrange conductors"), this);
	ShortcutManager::instance().registerAction(m_conductor_arrange, "diagrameditor.conductor_arrange", tr("Diagram editors"), QKeySequence());
	m_conductor_arrange->setStatusTip(tr("Put the bend of each selected conductor in the middle of the free space, on its own lane", "status bar tip"));
	connect(m_conductor_arrange, &QAction::triggered, this, [this]() {arrangeConductors(false);});
	m_conductor_arrange_folio = new QAction(QET::Icons::ConductorSettings, tr("Arrange the folio's conductors"), this);
	ShortcutManager::instance().registerAction(m_conductor_arrange_folio, "diagrameditor.conductor_arrange_folio", tr("Diagram editors"), QKeySequence());
	m_conductor_arrange_folio->setStatusTip(tr("Arrange every conductor of the folio not dragged by hand", "status bar tip"));
	connect(m_conductor_arrange_folio, &QAction::triggered, this, [this]() {arrangeConductors(true);});

		//The cross-section of each point from its components, for a
		//project drawn before they said one (T70)
	m_update_wire_sections = new QAction(QET::Icons::ConductorSettings, tr("Update the wire cross-sections"), this);
	ShortcutManager::instance().registerAction(m_update_wire_sections, "diagrameditor.update_wire_sections", tr("Diagram editors"), QKeySequence());
	m_update_wire_sections->setStatusTip(tr("Give every conductor of the project the largest cross-section of the components of its point", "status bar tip"));
	connect(m_update_wire_sections, &QAction::triggered, this, &QETDiagramEditor::updateWireSections);

		//The names typed by hand, "F5-2 (1mm²)", taken to the numbering
		//F<folio>-<number> (colour cross-section), each wire keeping its
		//number (T70)
	m_convert_wire_names = new QAction(QET::Icons::ConductorSettings, tr("Convert the wire names"), this);
	ShortcutManager::instance().registerAction(m_convert_wire_names, "diagrameditor.convert_wire_names", tr("Diagram editors"), QKeySequence());
	m_convert_wire_names->setStatusTip(tr("Name every wire of the project F<folio>-<number> (colour cross-section), keeping the number it has", "status bar tip"));
	connect(m_convert_wire_names, &QAction::triggered, this, &QETDiagramEditor::convertWireNames);
	m_renumber_wires = new QAction(QET::Icons::ConductorSettings, tr("Renumber the wires…"), this);
	ShortcutManager::instance().registerAction(m_renumber_wires, "diagrameditor.renumber_wires", tr("Diagram editors"), QKeySequence());
	m_renumber_wires->setStatusTip(tr("Number the wires again from their place on the folio, from 1 on each folio", "status bar tip"));
	connect(m_renumber_wires, &QAction::triggered, this, &QETDiagramEditor::renumberWires);
	m_remove_wires = new QAction(QET::Icons::EditDelete, tr("Remove the wires…"), this);
	ShortcutManager::instance().registerAction(m_remove_wires, "diagrameditor.remove_wires", tr("Diagram editors"), QKeySequence());
	m_remove_wires->setStatusTip(tr("Remove every wire of the project or of this folio, but those left out of automatic numbering and those of a panel marked as built", "status bar tip"));
	connect(m_remove_wires, &QAction::triggered, this, &QETDiagramEditor::removeWires);
		//The box of the wire panel, for every selected wire at once (T77):
		//ticked when they all are left out
	m_leave_out_wires = new QAction(tr("Leave out of automatic numbering"), this);
	m_leave_out_wires->setCheckable(true);
	ShortcutManager::instance().registerAction(m_leave_out_wires, "diagrameditor.leave_out_wires", tr("Diagram editors"), QKeySequence());
	m_leave_out_wires->setStatusTip(tr("Automatic numbering does not change the selected wires, and does not count their numbers", "status bar tip"));
	connect(m_leave_out_wires, &QAction::triggered, this, &QETDiagramEditor::leaveOutSelectedWires);

		//AutoConductor
	m_auto_conductor = new QAction   (QET::Icons::Autoconnect, tr("Automatic creation conductor (s)","Tool tip of auto conductor"), this);
	m_auto_conductor->setStatusTip (tr("Using the automatic creation of conductor (s) when possible", "Status tip of auto conductor"));
	m_auto_conductor->setCheckable (true);
	connect(m_auto_conductor, &QAction::triggered, [this](bool ac) {
			//Remembered, so the next project and the next session start
			//the way it was left
		QETProject::setDefaultAutoConductor(ac);
		if (ProjectView *pv = currentProjectView())
			pv->project()->setAutoConductor(ac);
	});
		//Registered with no default sequence on purpose. This is a
		//setting some people toggle constantly and others never touch,
		//so it earns a place in the Shortcuts page rather than a key of
		//its own taken from the ones still free. Asked for on the forum
		//(viewtopic.php?pid=23296): "est il possible dans les raccourcis
		//d'ajouter un pour création automatique de conducteur ?"
	ShortcutManager::instance().registerAction(m_auto_conductor, "diagrameditor.auto_conductor", tr("Diagram editors"), QKeySequence());

		//AutoBreakConductor
	m_auto_break_conductor = new QAction   (QET::Icons::Conductor, tr("Automatic disconnection of conductor(s)","Tool tip of auto break conductor"), this);
	ShortcutManager::instance().registerAction(m_auto_break_conductor, "diagrameditor.auto_break_conductor", tr("Diagram editors"), QKeySequence());
	m_auto_break_conductor->setStatusTip (tr("Automatically cut existing conductors when placing an element", "Status tip of auto break conductor"));
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

		//Diagram background color picker
	m_background_color_button = new DiagramBgColorToolButton(this, this);

		//Draw or not the background grid
	m_draw_grid = new QAction ( QET::Icons::Grid, tr("Display the grid"), this);
	ShortcutManager::instance().registerAction(m_draw_grid, "diagrameditor.draw_grid", tr("Diagram editors"), QKeySequence());
	m_draw_grid->setStatusTip(tr("Display or hide the grid of sheet"));
	QSettings settings;
	m_draw_grid->setCheckable(true);
	m_draw_grid->setChecked(settings.value("diagrameditor/grid_display_startup", true).toBool());
	connect(m_draw_grid, &QAction::triggered, [this](bool checked) {
			//The same setting as "Afficher la grille par défaut" in the
			//preferences: the folios opened later, in this session or the
			//next, show the grid the way it was left
		QSettings().setValue(QStringLiteral("diagrameditor/grid_display_startup"), checked);
			//The folio's own setting: folios() builds nothing
		foreach (ProjectView *prjv, this->openedProjects())
			foreach (Diagram *d, prjv->project()->folios()) {
				d->setDisplayGrid(checked);
				d->update();
			}
	});

		//A crosshair across the whole folio under the mouse, to see
		//whether symbols and terminals are in line
	m_crosshair = new QAction(tr("Folio-wide crosshair"), this);
	ShortcutManager::instance().registerAction(m_crosshair, "diagrameditor.crosshair", tr("Diagram editors"), QKeySequence());
	m_crosshair->setStatusTip(tr("Show two lines across the whole folio under the mouse, to see whether parts are in line"));
	m_crosshair->setCheckable(true);
	m_crosshair->setChecked(DiagramView::crosshairShown());
	connect(m_crosshair, &QAction::triggered, [this](bool checked) {
		DiagramView::setCrosshairShown(checked);
		for (DiagramView *view : findChildren<DiagramView *>())
			view->viewport()->update();
	});

		//Snap step for dragged texts, as a fraction of the folio grid
	m_text_grid_menu = new QMenu(tr("Text grid"), this);
	m_text_grid_menu->setIcon(QET::Icons::Grid);
	m_text_grid_menu->setToolTipsVisible(true);
	m_text_grid_button = new QToolButton(this);
	m_text_grid_button->setMenu(m_text_grid_menu);
	m_text_grid_button->setPopupMode(QToolButton::InstantPopup);
	m_text_grid_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_text_grid_button->setToolTip(tr("Snap grid for texts moved with the mouse.\n"
									  "Hold Ctrl while moving to place freely."));
	auto text_grid_group = new QActionGroup(this);
	for (const qreal divisor : TextGrid::divisors)
	{
		QAction *action = m_text_grid_menu->addAction(
					divisor > 0 ? TextGrid::ratioLabel(divisor) : tr("Disabled"));
		action->setCheckable(true);
		action->setData(divisor);
		text_grid_group->addAction(action);
	}
	connect(text_grid_group, &QActionGroup::triggered, this, [](QAction *action) {
		QSettings().setValue(TextGrid::settings_key, action->data());
		emit QETApp::instance()->textGridChanged();
	});
	connect(QETApp::instance(), &QETApp::textGridChanged,
			this, &QETDiagramEditor::updateTextGridButton);
	updateTextGridButton();

	// Draw or not the custom guides
	m_draw_guides = new QAction ( QIcon::fromTheme("guides"), tr("View the guides"), this);
	ShortcutManager::instance().registerAction(m_draw_guides, "diagrameditor.draw_guides", tr("Diagram editors"), QKeySequence());
	m_draw_guides->setStatusTip(tr("Shows or hides the guides"));
	m_draw_guides->setCheckable(true);
	m_draw_guides->setChecked(settings.value("diagrameditor/guides_display_startup", false).toBool());
	connect(m_draw_guides, &QAction::triggered, [this](bool checked) {
			//Remembered like the grid, in the setting of the preferences
		QSettings().setValue(QStringLiteral("diagrameditor/guides_display_startup"), checked);
			//The folio's own setting: folios() builds nothing
		foreach (ProjectView *prjv, this->openedProjects())
			foreach (Diagram *d, prjv->project()->folios()) {
				d->setDisplayGuides(checked);
			}
	});

		//Keep the column numbers and row letters of the folio in sight
	m_cell_rulers = new QAction(tr("Keep headers visible"), this);
	ShortcutManager::instance().registerAction(m_cell_rulers, "diagrameditor.cell_rulers", tr("Diagram editors"), QKeySequence());
	m_cell_rulers->setStatusTip(tr("Keeps the sheet's column numbers and row letters visible at the edge of the view"));
	m_cell_rulers->setCheckable(true);
	m_cell_rulers->setChecked(settings.value("diagrameditor/cell_rulers", false).toBool());
	connect(m_cell_rulers, &QAction::triggered, [this](bool checked) {
		QSettings().setValue("diagrameditor/cell_rulers", checked);
		foreach (ProjectView *prjv, this->openedProjects())
			foreach (DiagramView *dv, prjv->diagram_views())
				dv->setCellRulersShown(checked);
	});

		//Draw the limits of the folio columns and rows across the drawing
	m_cell_lines = new QAction(tr("Show cell boundaries"), this);
	ShortcutManager::instance().registerAction(m_cell_lines, "diagrameditor.cell_lines", tr("Diagram editors"), QKeySequence());
	m_cell_lines->setStatusTip(tr("Draws the boundaries of the sheet's columns and rows on the diagram, on screen only"));
	m_cell_lines->setCheckable(true);
	m_cell_lines->setChecked(settings.value("diagrameditor/cell_lines", false).toBool());
	connect(m_cell_lines, &QAction::triggered, [this](bool checked) {
		QSettings().setValue("diagrameditor/cell_lines", checked);
		foreach (ProjectView *prjv, this->openedProjects())
			foreach (DiagramView *dv, prjv->diagram_views())
				dv->setCellLinesShown(checked);
	});

		//Show or hide whole kinds of items on every folio (bugtracker #301)
	m_shown_kinds_menu = new QMenu(tr("Show"), this);
	const QList<QPair<ShownKinds::Kind, QString>> kinds {
		{ShownKinds::SymbolTexts,     tr("Element texts")},
		{ShownKinds::WireNumbers,     tr("Conductor texts")},
		{ShownKinds::FreeTexts,       tr("Text fields")},
		{ShownKinds::Shapes,          tr("Shapes")},
		{ShownKinds::Pictures,        tr("Pictures")},
		{ShownKinds::Tables,          tr("Tables")},
		{ShownKinds::CrossReferences, tr("Cross-references")}};
	for (const auto &kind : kinds)
	{
		QAction *action = m_shown_kinds_menu->addAction(kind.second);
		action->setCheckable(true);
		action->setData(int(kind.first));
		connect(action, &QAction::triggered, this, [kind](bool checked) {
			ShownKinds::setShown(kind.first, checked);
				//Items built later are shown or hidden as they are made
				//(ShownKinds::tag())
			for (QETProject *project : QETApp::registeredProjects())
				for (Diagram *diagram : project->builtFolios())
					ShownKinds::apply(diagram, kind.first);
			emit QETApp::instance()->shownKindsChanged();
		});
	}
	m_hidden_kinds_label = new QLabel(this);
	m_hidden_kinds_label->setToolTip(tr("See Display > Show"));
	statusBar()->addPermanentWidget(m_hidden_kinds_label);
	connect(QETApp::instance(), &QETApp::shownKindsChanged,
			this, &QETDiagramEditor::updateShownKinds);
	updateShownKinds();

		//Edit current diagram properties
	m_edit_diagram_properties = new QAction(QET::Icons::DialogInformation, tr("Sheet properties"), this);
	ShortcutManager::instance().registerAction(m_edit_diagram_properties, "diagrameditor.edit_diagram_properties", tr("Diagram editors"), Qt::CTRL | Qt::Key_L);
	m_edit_diagram_properties     -> setStatusTip(tr("Edits the properties of the sheet (size, title block information, conductor properties...)", "status bar tip"));
	connect(m_edit_diagram_properties, &QAction::triggered, [this]() {
		if (ProjectView *project_view = currentProjectView())
		{
			activateProject(project_view);
			project_view->editCurrentDiagramProperties();
		}
	});

		//Edit current project properties
	m_project_edit_properties = new QAction(QET::Icons::ProjectProperties, tr("Project properties"), this);
	ShortcutManager::instance().registerAction(m_project_edit_properties, "diagrameditor.project_edit_properties", tr("Diagram editors"), QKeySequence());
	connect(m_project_edit_properties, &QAction::triggered, [this]() {
		editProjectProperties(currentProjectView());
	});

		//Add new folio to current project
	m_project_add_diagram = new QAction(QET::Icons::DiagramAdd, tr("Add a sheet"), this);
	ShortcutManager::instance().registerAction(m_project_add_diagram, "diagrameditor.project_add_diagram", tr("Diagram editors"), Qt::CTRL | Qt::Key_T);
	connect(m_project_add_diagram, &QAction::triggered, [this]() {
		if (ProjectView *current_project = currentProjectView()) {
			current_project->project()->addNewDiagram();
		}
	});

		//Remove current folio from current project
	m_remove_diagram_from_project = new QAction(QET::Icons::DiagramDelete, tr("Delete this sheet"), this);
	ShortcutManager::instance().registerAction(m_remove_diagram_from_project, "diagrameditor.remove_diagram_from_project", tr("Diagram editors"), QKeySequence());
	connect(m_remove_diagram_from_project, &QAction::triggered, this, &QETDiagramEditor::removeDiagramFromProject);

		//Clean the current project
	m_clean_project         = new QAction(QET::Icons::EditClear,             tr("Clean project"),                   this);
	ShortcutManager::instance().registerAction(m_clean_project, "diagrameditor.clean_project", tr("Diagram editors"), QKeySequence());
	connect(m_clean_project, &QAction::triggered, [this]() {
		if (ProjectView *current_project = currentProjectView()) {
			if (current_project->cleanProject()) {
				pa -> reloadAndFilter();
			}
		}
	});

		//Export nomenclature to CSV
	m_csv_export = new QAction(QET::Icons::DocumentSpreadsheet, tr("Export to CSV"), this);
	ShortcutManager::instance().registerAction(m_csv_export, "diagrameditor.csv_export", tr("Diagram editors"), QKeySequence());
	connect(m_csv_export, &QAction::triggered, [this]() {
		BOMExportDialog bom(currentProjectView()->project(), this);
		bom.exec();
	});

		//Add a nomenclature item
	m_add_nomenclature = new QAction(QET::Icons::TableOfContent, tr("Add a nomenclature"), this);
	ShortcutManager::instance().registerAction(m_add_nomenclature, "diagrameditor.add_nomenclature", tr("Diagram editors"), QKeySequence());
	connect(m_add_nomenclature, &QAction::triggered, this, [this]() {
		if(this->currentDiagramView()) {
			QetGraphicsTableFactory::createAndAddNomenclature(this->currentDiagramView()->diagram());
		}
	});

		//Add a summary item
	m_add_summary = new QAction(QET::Icons::TableOfContent, tr("Add a summary"), this);
	ShortcutManager::instance().registerAction(m_add_summary, "diagrameditor.add_summary", tr("Diagram editors"), QKeySequence());
	connect(m_add_summary, &QAction::triggered, this, [this]() {
		if(this->currentDiagramView()) {
			QetGraphicsTableFactory::createAndAddSummary(this->currentDiagramView()->diagram());
		}
	});

	m_terminal_strip_dialog = new QAction(QET::Icons::TerminalStrip, tr("Terminal block manager (DEV)"), this);
	ShortcutManager::instance().registerAction(m_terminal_strip_dialog, "diagrameditor.terminal_strip_dialog", tr("Diagram editors"), QKeySequence());
	connect(m_terminal_strip_dialog, &QAction::triggered, this, [this]()
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
	m_catalog_assign = new QAction(QET::Icons::TableOfContent, tr("Attribuer une pièce"), this);
	m_catalog_assign->setToolTip(tr(
				   "Attribue une pièce du catalogue aux composants sélectionnés : "
				   "informations, numéros de bornes et accessoires suivent."));
	m_catalog_assign->setStatusTip(m_catalog_assign->toolTip());
	ShortcutManager::instance().registerAction(m_catalog_assign, "diagrameditor.catalog_assign",
						   tr("Éditeur de schémas"),
						   Qt::CTRL | Qt::SHIFT | Qt::Key_P);
	connect(m_catalog_assign, &QAction::triggered, this, &QETDiagramEditor::assignCatalogPart);

		//The way back from the folio to the plate. Offered by the context
		//menu only for an element that is mounted somewhere (see
		//DiagramView::contextMenuActions()).
	m_show_on_plate = new QAction(tr("Montrer sur la plaque"), this);
	m_show_on_plate->setStatusTip(tr("Ouvre l'implantation de la plaque où ce "
					   "composant est monté, avec la pièce sélectionnée."));
	connect(m_show_on_plate, &QAction::triggered, this, &QETDiagramEditor::showSelectionOnPlate);

	m_catalog_register = new QAction(QET::Icons::TableOfContent, tr("Enregistrer une pièce"), this);
	m_catalog_register->setToolTip(tr(
				   "Crée une pièce du catalogue à partir des composants "
				   "sélectionnés, avec une broche par borne."));
	m_catalog_register->setStatusTip(m_catalog_register->toolTip());
	connect(m_catalog_register, &QAction::triggered, this, &QETDiagramEditor::registerCatalogPart);

	m_catalog_missing = new QAction(QET::Icons::TableOfContent, tr("Composants sans pièce"), this);
	connect(m_catalog_missing, &QAction::triggered, this, [this]()
	{
		CatalogProjectActions::showMissingPartReport(this->currentProject(), this);
	});

		//Beside its sibling and not inside it: one asks who has nothing to
		//buy, the other asks which of the bought products nobody has
		//measured. Same menu, same list of components, two questions, and
		//an answer to one of them is no answer to the other.
	m_catalog_no_physical_view = new QAction(QET::Icons::TableOfContent,
						 tr("Pièces sans vue physique"), this);
	m_catalog_no_physical_view->setToolTip(tr(
				   "Liste les composants dont la pièce n'a ni largeur ni "
				   "hauteur au catalogue : ce qui reste à mesurer avant "
				   "de dessiner une implantation."));
	m_catalog_no_physical_view->setStatusTip(m_catalog_no_physical_view->toolTip());
	connect(m_catalog_no_physical_view, &QAction::triggered, this, [this]()
	{
		CatalogProjectActions::showMissingPhysicalViewReport(this->currentProject(), this);
	});

		//The third of the end of project checks, and the one that asks
		//about the wiring rather than about the buying: which connectors
		//nobody can count the ways of, and which pins belong to no
		//connector at all.
	m_connector_check = new QAction(QET::Icons::TableOfContent,
					tr("Contrôle des connecteurs"), this);
	m_connector_check->setToolTip(tr(
				  "Liste les connecteurs dont le nombre de voies ne peut "
				  "pas être établi et les broches qui n'appartiennent à "
				  "aucun connecteur, et permet d'y remédier depuis la "
				  "liste."));
	m_connector_check->setStatusTip(m_connector_check->toolTip());
	connect(m_connector_check, &QAction::triggered, this, [this]()
	{
		ConnectorCheck::showReport(this->currentProject(), this);
	});

	m_drc_check = new QAction(QET::Icons::TableOfContent,
				  tr("Contrôle du projet"), this);
	m_drc_check->setToolTip(tr(
				  "Vérifie le projet : bobine sans contact, contact sans "
				  "bobine, borne hors bornier. La liste reste affichée et "
				  "mène au composant."));
	m_drc_check->setStatusTip(m_drc_check->toolTip());
	connect(m_drc_check, &QAction::triggered, this, [this]()
	{
		DrcProjectActions::showCheckReport(this->currentProject(), this);
	});

	m_environment = new QAction(QET::Icons::Configure, tr("Environnement de travail"), this);
	connect(m_environment, &QAction::triggered, this, [this]()
	{
		EnvironmentDialog dialog(this);
		dialog.exec();
	});

	m_catalog_import = new QAction(QET::Icons::DocumentSpreadsheet, tr("Importer des pièces…"), this);
	m_catalog_import->setToolTip(tr(
				   "Importe des pièces depuis un fichier .csv ou .xlsx, avec un "
				   "rapport de ce qui est entré et de ce qui a été refusé."));
	m_catalog_import->setStatusTip(m_catalog_import->toolTip());
	connect(m_catalog_import, &QAction::triggered, this, [this]()
	{
		CatalogImportDialog dialog(QETApp::catalog(), this);
		dialog.exec();
	});

	m_catalog_repository = new QAction(QET::Icons::FolderShowAll, tr("Répertoire partagé…"), this);
	m_catalog_repository->setToolTip(tr(
				   "Cherche et publie des pièces dans le répertoire partagé du "
				   "bureau."));
	m_catalog_repository->setStatusTip(m_catalog_repository->toolTip());
	connect(m_catalog_repository, &QAction::triggered, this, [this]()
	{
		CatalogRepositoryDialog dialog(QETApp::catalog(), this);
		dialog.exec();
	});

	m_renumber_components = new QAction(tr("Renuméroter…"), this);
	m_renumber_components->setToolTip(tr(
				   "Renumérote les composants du projet, avec la table « de → vers "
				   "» affichée avant d'appliquer."));
	m_renumber_components->setStatusTip(m_renumber_components->toolTip());
	connect(m_renumber_components, &QAction::triggered,
		this, &QETDiagramEditor::renumberComponents);

		//Making a symbol on the sheet, with the drawing tools everybody
		//already uses, instead of in a separate program. The separate program
		//is why nobody makes symbols: a similar one gets used and disguised,
		//and the library stops representing what is actually built.
	m_create_symbol = new QAction(QET::Icons::ElementNew,
				      tr("Créer un symbole…"),
				      this);
	m_create_symbol->setToolTip(tr(
				   "Transforme le dessin sélectionné en symbole de la bibliothèque "
				   ": les points de raccordement sont déduits des bouts libres du "
				   "dessin."));
	m_create_symbol->setStatusTip(m_create_symbol->toolTip());
	connect(m_create_symbol, &QAction::triggered,
		this, &QETDiagramEditor::createSymbolFromSelection);

		//The other way round: the drawing of a card is already written down
		//in the catalogue, pin by pin, and drawing it again by hand is how a
		//card of thirty two points ends up with thirty one on the folio.
	m_generate_pinout = new QAction(QET::Icons::Terminal,
					tr("Générer des blocs de brochage…"),
					this);
	m_generate_pinout->setToolTip(tr(
				   "Dessine le symbole d'une pièce à partir du brochage "
				   "que le catalogue connaît, en un bloc ou en plusieurs."));
	m_generate_pinout->setStatusTip(m_generate_pinout->toolTip());
	connect(m_generate_pinout, &QAction::triggered,
		this, &QETDiagramEditor::generatePinoutBlocks);

		//Twenty starters typed in twenty times is where one tag ends up
		//repeated and one rating ends up wrong. Written once as a table,
		//they are drawn once - and the table is what gets read at the
		//review, instead of twenty drawings.
	m_generate_circuits = new QAction(QET::Icons::DocumentSpreadsheet,
					  tr("Générer des circuits par table…"),
					  this);
	m_generate_circuits->setToolTip(tr(
					"Dessine d'un coup les circuits d'une table : "
					"une ligne par circuit, un macro et ses valeurs."));
	m_generate_circuits->setStatusTip(m_generate_circuits->toolTip());
	connect(m_generate_circuits, &QAction::triggered,
		this, &QETDiagramEditor::generateCircuits);

		//The list of points comes from the automation department and comes
		//again, revised, several times over a project. So this sits in the
		//menu next to the generators and not inside a wizard: it is opened
		//as often as a folio is.
	m_import_io_sheet = new QAction(QET::Icons::DocumentImport,
					tr("Importer une liste d'E/S…"),
					this);
	m_import_io_sheet->setToolTip(tr(
					"Lit une feuille de calcul d'entrées / sorties et la fond avec "
					"celle du projet, sans rien effacer de ce qui est déjà placé."));
	m_import_io_sheet->setStatusTip(m_import_io_sheet->toolTip());
	connect(m_import_io_sheet, &QAction::triggered,
		this, &QETDiagramEditor::importIoSheet);

		//Taking them out of the list and into a card is the step right
		//after the import, so it sits right after it in the menu.
	m_assign_io_points = new QAction(tr("Affecter des E/S à une carte…"),
									this);
	m_assign_io_points->setToolTip(tr(
									"Place les points d'entrées / sorties déjà importés dans "
									"les voies libres d'une carte d'automate, et les en retire."));
	m_assign_io_points->setStatusTip(m_assign_io_points->toolTip());
	connect(m_assign_io_points, &QAction::triggered,
		this, &QETDiagramEditor::assignIoPoints);

		//Reading what was placed is the step after placing it, so it
		//closes the run of I/O actions.
	m_io_list = new QAction(tr("Liste des E/S…"),
									this);
	m_io_list->setToolTip(tr(
									"Montre les entrées / sorties du projet, les automates et "
									"leurs cartes au-dessus, et laisse les corriger sur place."));
	m_io_list->setStatusTip(m_io_list->toolTip());
	connect(m_io_list, &QAction::triggered,
		this, &QETDiagramEditor::showIoList);

		//Marking a bar and wiring the communs are one gesture in two
		//halves, so they close the run of I/O actions together.
	m_mark_io_bus = new QAction(tr("Marquer comme barre par défaut…"),
									this);
	m_mark_io_bus->setToolTip(tr(
									"Dit que les éléments sélectionnés servent de barre "
									"d'alimentation ou de retour pour les communs."));
	m_mark_io_bus->setStatusTip(m_mark_io_bus->toolTip());
	connect(m_mark_io_bus, &QAction::triggered,
		this, &QETDiagramEditor::markIoBus);

	m_wire_io_commons = new QAction(tr("Raccorder les communs…"),
									this);
	m_wire_io_commons->setToolTip(tr(
									"Tire d'un coup les fils entre les communs des cartes et "
									"les barres marquées de leur folio."));
	m_wire_io_commons->setStatusTip(m_wire_io_commons->toolTip());
	connect(m_wire_io_commons, &QAction::triggered,
		this, &QETDiagramEditor::wireIoCommons);

	m_save_group = new QAction(tr("Enregistrer un groupement…"),
				   this);
	m_save_group->setToolTip(tr(
				   "Garde le morceau de schéma sélectionné dans la bibliothèque, "
				   "avec les pièces déjà attribuées."));
	m_save_group->setStatusTip(m_save_group->toolTip());
	connect(m_save_group, &QAction::triggered,
		this, &QETDiagramEditor::saveSelectionAsGroup);

	m_insert_group = new QAction(tr("Insérer un groupement…"), this);
	m_insert_group->setToolTip(tr(
				   "Insère un morceau de schéma de la bibliothèque, pièces "
				   "comprises."));
	m_insert_group->setStatusTip(m_insert_group->toolTip());
	connect(m_insert_group, &QAction::triggered,
		this, &QETDiagramEditor::insertGroup);

		//The norm is a property of the project, not of the program: two
		//customers, two requirements, the same workstation.
		//The other half of creating a symbol: draw, make a block, and when the
		//block turns out to need a line moved, explode it and make it again.
	m_explode_element = new QAction(
				tr("Éclater le symbole"), this);
	m_explode_element->setToolTip(tr(
				   "Rend le composant sélectionné à ses lignes et ses textes, pour "
				   "les reprendre et refaire le bloc."));
	m_explode_element->setStatusTip(m_explode_element->toolTip());
	connect(m_explode_element, &QAction::triggered,
		this, &QETDiagramEditor::explodeSelection);

		//Another element in the place of the selected ones, with their wires:
		//the picker opens as Insert opens it, and what is chosen there
		//replaces the selection instead of being placed (T54). Alt+S: Alt+Q
		//is the swap, as in the schematic editors it was asked to match.
	m_replace_element = new QAction(tr("Remplacer l'élément…"), this);
	m_replace_element->setStatusTip(tr(
		"Met un autre élément à la place des éléments sélectionnés, en "
		"gardant leurs fils", "status bar tip"));
	m_replace_element->setData("replace_element");
	ShortcutManager::instance().registerAction(
		m_replace_element, "diagrameditor.replace_element",
		tr("Éditeur de schémas"), QKeySequence(Qt::ALT | Qt::Key_S));
	connect(m_replace_element, &QAction::triggered,
		this, &QETDiagramEditor::replaceSelectedElements);
	addAction(m_replace_element);

		//Each of two selected elements in the place of the other: position,
		//rotation and mirrors, the wires staying where they are (T66)
	m_exchange_elements = new QAction(tr("Swap elements"), this);
	m_exchange_elements->setStatusTip(tr(
		"Puts each of the two selected elements in the place of the other; "
		"the wires stay, and each one takes those of its new place", "status bar tip"));
	m_exchange_elements->setData("exchange_elements");
	ShortcutManager::instance().registerAction(
		m_exchange_elements, "diagrameditor.exchange_elements",
		tr("Éditeur de schémas"), QKeySequence(Qt::ALT | Qt::Key_Q));
	connect(m_exchange_elements, &QAction::triggered,
		this, &QETDiagramEditor::exchangeSelectedElements);
	addAction(m_exchange_elements);

		//The texts of the selected elements as their symbol has them: the
		//update of the drawings keeps what was changed on a folio, and this
		//is the way back to the symbol (T58)
	m_reset_symbol_texts = new QAction(tr("Rétablir les textes du symbole"), this);
	m_reset_symbol_texts->setStatusTip(tr(
		"Remet les textes des éléments sélectionnés comme leur symbole les a,"
		" en gardant leurs valeurs", "status bar tip"));
	m_reset_symbol_texts->setData("reset_symbol_texts");
	connect(m_reset_symbol_texts, &QAction::triggered,
		this, &QETDiagramEditor::resetSymbolTexts);
	addAction(m_reset_symbol_texts);

	m_replace_part = new QAction(
				tr("Remplacer une pièce…"), this);
	m_replace_part->setToolTip(tr(
				   "Échange une pièce contre une autre sur tous les composants du "
				   "projet, en une seule fois annulable."));
	m_replace_part->setStatusTip(m_replace_part->toolTip());
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
	m_show_fine_grid = new QAction(tr("Grille fine"), this);
	m_show_fine_grid->setToolTip(tr(
				   "Affiche la grille fine sous la principale. Elle sert au dessin "
				   "; les points de raccordement, eux, doivent rester sur la "
				   "principale."));
	m_show_fine_grid->setStatusTip(m_show_fine_grid->toolTip());
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
				tr("Points de raccordement"), this);
	m_show_terminals->setToolTip(tr(
				   "Garde tous les points de raccordement visibles."));
	m_show_terminals->setStatusTip(m_show_terminals->toolTip());
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
				tr("Attributs vides"), this);
	m_show_empty_fields->setToolTip(tr(
				   "Montre en gris les champs de texte qui n'ont pas encore de "
				   "valeur, pour pouvoir les placer."));
	m_show_empty_fields->setStatusTip(m_show_empty_fields->toolTip());
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
				tr("Afficher le numéro"), this);
	m_show_conductor_text->setToolTip(tr(
				   "Réaffiche le numéro sur les conducteurs sélectionnés. Le "
				   "numéro lui-même ne change pas."));
	m_show_conductor_text->setStatusTip(m_show_conductor_text->toolTip());
	connect(m_show_conductor_text, &QAction::triggered,
		this, [this]() { this->setConductorTextVisible(true); });

	m_hide_conductor_text = new QAction(
				tr("Masquer le numéro"), this);
	m_hide_conductor_text->setToolTip(tr(
				   "Cache le numéro sur les conducteurs sélectionnés. Le numéro "
				   "lui-même ne change pas : il reste dans la liste de câblage."));
	m_hide_conductor_text->setStatusTip(m_hide_conductor_text->toolTip());
	connect(m_hide_conductor_text, &QAction::triggered,
		this, [this]() { this->setConductorTextVisible(false); });

	m_align_conductor_text = new QAction(
				tr("Aligner les numéros"), this);
	m_align_conductor_text->setToolTip(tr(
				   "Aligne sur un même axe le numéro des conducteurs sélectionnés. "
				   "Remplace le fait de déplacer chaque texte à la main."));
	m_align_conductor_text->setStatusTip(m_align_conductor_text->toolTip());
	m_align_conductor_text->setStatusTip(
				tr("Remplace le fait de déplacer chaque texte à la main."));
	connect(m_align_conductor_text, &QAction::triggered,
		this, &QETDiagramEditor::alignConductorTexts);

		//An accessory drawn on the folio, in its own location, saying whose it
		//is. Reached from the menu on the selected element rather than asked
		//when the symbol is inserted - see the T13 task file for why.
	m_link_accessory = new QAction(
				tr("Lier un accessoire…"), this);
	m_link_accessory->setToolTip(tr(
				   "Dit à quel composant appartient l'accessoire sélectionné."));
	m_link_accessory->setStatusTip(m_link_accessory->toolTip());
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
				tr("Structure CEI 81346…"), this);
	m_iec_structure->setToolTip(tr(
				   "Active la structure d'identification de la norme pour ce "
				   "projet, et choisit ce qui est écrit sur le dessin."));
	m_iec_structure->setStatusTip(m_iec_structure->toolTip());
	connect(m_iec_structure, &QAction::triggered, this, [this]()
	{
		if (QETProject *project = this->currentProject())
		{
			IecStructureDialog dialog(project, this);
			dialog.exec();
		}
	});

	m_assembly_state = new QAction(
				tr("État de montage du projet…"), this);
	m_assembly_state->setToolTip(tr(
				   "Fige ce qui est déjà câblé et étiqueté : "
				   "l'automatisation ne numérote plus que ce qui sera "
				   "dessiné après."));
	m_assembly_state->setStatusTip(m_assembly_state->toolTip());
	connect(m_assembly_state, &QAction::triggered, this, [this]()
	{
		if (QETProject *project = this->currentProject())
		{
			AssemblyStateDialog dialog(project, this);
			dialog.exec();
		}
	});

	m_location_manager = new QAction(
				tr("Armoires et localisations…"), this);
	m_location_manager->setToolTip(tr(
				   "Fait les armoires du projet, les emboîte, et met la "
				   "sélection dans l'une d'elles en un geste."));
	m_location_manager->setStatusTip(m_location_manager->toolTip());
	connect(m_location_manager, &QAction::triggered,
		this, &QETDiagramEditor::showLocationManager);

	m_location_report = new QAction(
				tr("Composants sans localisation…"), this);
	m_location_report->setToolTip(tr(
				  "Liste ce qui n'est encore dans aucune armoire, folio "
				  "par folio, et permet de les y mettre."));
	m_location_report->setStatusTip(m_location_report->toolTip());
	connect(m_location_report, &QAction::triggered,
		this, &QETDiagramEditor::showLocationReport);

	m_location_bom = new QAction(
				tr("Liste de matériel par localisation…"), this);
	m_location_bom->setToolTip(tr(
			       "Ce qu'il faut sortir du magasin pour une armoire, "
			       "sous-localisations comprises. S'exporte, et se pose "
			       "sur le folio."));
	m_location_bom->setStatusTip(m_location_bom->toolTip());
	connect(m_location_bom, &QAction::triggered,
		this, &QETDiagramEditor::showLocationBom);

	m_mounting_layout = new QAction(
				tr("Calepinage de l'armoire…"), this);
	m_mounting_layout->setToolTip(tr(
				  "Implante la platine en millimètre : ce qui est "
				  "vissé dessus, et où. Se glisse à la souris."));
	m_mounting_layout->setStatusTip(m_mounting_layout->toolTip());
	connect(m_mounting_layout, &QAction::triggered,
		this, &QETDiagramEditor::showMountingLayout);

	m_put_mounting_layout = new QAction(
				tr("Poser le calepinage sur le folio…"), this);
	m_put_mounting_layout->setToolTip(tr(
				  "Dessine une platine du projet sur le folio courant, "
				  "à l'échelle choisie. Le dessin suit la platine."));
	m_put_mounting_layout->setStatusTip(m_put_mounting_layout->toolTip());
	connect(m_put_mounting_layout, &QAction::triggered,
		this, &QETDiagramEditor::putMountingLayoutOnFolio);


		//Launch the plugin of terminal generator
	m_project_terminalBloc = new QAction(QET::Icons::TerminalStrip, tr("Launch the terminal block creation plugin"), this);
	ShortcutManager::instance().registerAction(m_project_terminalBloc, "diagrameditor.terminal_strip_plugin", tr("Diagram editors"), QKeySequence());
	connect(m_project_terminalBloc, &QAction::triggered, this, &QETDiagramEditor::generateTerminalBlock);

		//Add a cable list
	m_add_cable_list = new QAction(QET::Icons::TableOfContent, tr("Add a cable list"), this);
	connect(m_add_cable_list, &QAction::triggered, this, [this]() {
		if (this->currentDiagramView()) {
			QetGraphicsTableFactory::createAndAddCableList(this->currentDiagramView()->diagram());
		}
	});

		//Export the whole cable list to CSV, every column of the list
	m_export_cable_list = new QAction(QET::Icons::DocumentSpreadsheet, tr("Export the cable list to CSV format"), this);
	connect(m_export_cable_list, &QAction::triggered, [this]() {
		QETProject *project = this->currentProject();
		if (!project) {
			return;
		}
			//The same dialog the material list is exported with, only
			//with the columns of the cable list
		CableExportDialog dialog(project, this);
		dialog.exec();
	});

	//Export conductor num to csv
	m_project_export_conductor_num = new QAction(QET::Icons::DocumentSpreadsheet, tr("Export the list of names of wires"), this);
	ShortcutManager::instance().registerAction(m_project_export_conductor_num, "diagrameditor.export_conductor_names", tr("Diagram editors"), QKeySequence());
	connect(m_project_export_conductor_num, &QAction::triggered, [this]() {
		QETProject *project = this->currentProject();
		if (project)
		{
			ConductorNumExport wne(project, this);
			wne.toCsv();
		}
	});
	// Export wiring list to CSV
	m_project_export_wiring_list = new QAction(QET::Icons::DocumentSpreadsheet, tr("Export the wiring diagram"), this);
	ShortcutManager::instance().registerAction(m_project_export_wiring_list, "diagrameditor.export_wiring_list", tr("Diagram editors"), QKeySequence());
	connect(m_project_export_wiring_list, &QAction::triggered, [this]() {
		QETProject *project = this->currentProject();
		if (project)
		{
			WiringListExport wle(project, this);
			wle.toCsv();
		}
	});

	// Show the wiring list read from the project database
	m_project_wiring_list_view = new QAction(QET::Icons::DocumentSpreadsheet, tr("Wiring list (database)"), this);
	ShortcutManager::instance().registerAction(m_project_wiring_list_view, "diagrameditor.wiring_list_view", tr("Diagram editors"), QKeySequence());
	connect(m_project_wiring_list_view, &QAction::triggered, [this]() {
		QETProject *project = this->currentProject();
		if (project)
		{
			WiringListDialog dialog(project, this);
			dialog.exec();
		}
	});

	// Terminal Numbering
	m_terminal_numbering = new QAction(QET::Icons::TerminalStrip, tr("Automatic numbering of terminals"), this);
	ShortcutManager::instance().registerAction(m_terminal_numbering, "diagrameditor.terminal_numbering", tr("Diagram editors"), QKeySequence());
	connect(m_terminal_numbering, &QAction::triggered, this, &QETDiagramEditor::slot_terminalNumbering);

	// Cable numbering: the rule the project numbers its cables with, and
	// the numbering of every cable of that project at once
	m_cable_numbering = new QAction(QET::Icons::AutoNum, tr("Cable numbering"), this);
	m_cable_numbering->setStatusTip(
		tr("Shows the cable numbering rule of the project and "
		   "renumbers its cables with it"));
	connect(m_cable_numbering, &QAction::triggered, this, &QETDiagramEditor::slot_cableNumbering);

	// Reload element drawings from their current definition (bugtracker #802)
	m_reload_element_drawings = new QAction(QET::Icons::ViewRefresh, tr("Reload element drawings"), this);
	ShortcutManager::instance().registerAction(m_reload_element_drawings, "diagrameditor.reload_element_drawings", tr("Diagram editors"), QKeySequence());
	m_reload_element_drawings->setStatusTip(
		tr("Applique les éléments modifiés dans la collection et les redessine"));
	connect(m_reload_element_drawings, &QAction::triggered, this, &QETDiagramEditor::slot_reloadElementDrawings);

		//The terminals of the project's symbols on whole numbers, so that a
		//wire between two terminals in line runs straight (T72)
	m_whole_terminals = new QAction(tr("Round the terminals of the symbols"), this);
	ShortcutManager::instance().registerAction(m_whole_terminals, "diagrameditor.whole_terminals", tr("Diagram editors"), QKeySequence());
	m_whole_terminals->setStatusTip(tr("Move each terminal of the project's symbols to the nearest whole number, so that the wires on it can run straight", "status bar tip"));
	connect(m_whole_terminals, &QAction::triggered, this, &QETDiagramEditor::putTerminalsOnWholeNumbers);

#ifdef QET_HAS_SCRIPTING
	// Run a JavaScript macro against the current project (bugtracker #162).
	m_run_script = new QAction(tr("Run a script..."), this);
	ShortcutManager::instance().registerAction(m_run_script, "diagrameditor.run_script", tr("Diagram editors"), QKeySequence());
	m_run_script->setStatusTip(
		tr("Runs a JavaScript script on the current project (see qet.* in "
		   "the script for the available API)"));
	connect(m_run_script, &QAction::triggered, this, &QETDiagramEditor::slot_runScript);

		//Record what the user does, for an assistant to turn into a script
	m_record_macro = new QAction(QET::Icons::RecordMacro, tr("Record a macro"), this);
	m_record_macro->setCheckable(true);
	m_record_macro->setStatusTip(tr("Records what you do on the project, for an AI assistant "
					"to make a script of; click again to stop"));
	connect(m_record_macro, &QAction::triggered, this, [this](bool checked) {
		if (checked) {
			if (!MacroRecorder::instance().start(this)) m_record_macro->setChecked(false);
		} else {
			MacroRecorder::instance().stop();
		}
	});
	ShortcutManager::instance().registerAction(m_record_macro, "diagrameditor.record_macro",
						   tr("Diagram editors"), QKeySequence());

		//Write, try, give an icon to and delete stored scripts
	m_manage_scripts = new QAction(tr("Manage scripts…"), this);
	m_manage_scripts->setStatusTip(tr("Write a script and make it a button with an icon"));
	connect(m_manage_scripts, &QAction::triggered, this, [this]() {
		if (!m_script_manager) {
			m_script_manager = new ScriptManagerDialog(
				[this](const QString &path, const QString &name) {
					runStoredScript(path, name);
				}, this);
		}
		m_script_manager->show();
		m_script_manager->raise();
		m_script_manager->activateWindow();
	});
	ShortcutManager::instance().registerAction(m_manage_scripts, "diagrameditor.manage_scripts",
						   tr("Diagram editors"), QKeySequence());

		//Stored scripts are files in a folder, written by hand, by the
		//script manager or by an assistant: open it to add one.
	m_open_scripts_folder = new QAction(QET::Icons::FolderOpen, tr("Open the scripts folder"), this);
	ShortcutManager::instance().registerAction(m_open_scripts_folder, "diagrameditor.open_scripts_folder", tr("Diagram editors"), QKeySequence());
	m_open_scripts_folder->setStatusTip(
		tr("Every .js file in this folder that starts with a // "
		   "==QETScript== header becomes a button"));
	connect(m_open_scripts_folder, &QAction::triggered, this, []() {
		QDir().mkpath(ScriptLibrary::folder());
		QDesktopServices::openUrl(QUrl::fromLocalFile(ScriptLibrary::folder()));
	});
#endif

	#ifdef QET_EXPORT_PROJECT_DB
		m_export_project_db = new QAction(QET::Icons::DocumentSpreadsheet, tr("Export the internal project database"), this);
		ShortcutManager::instance().registerAction(m_export_project_db, "diagrameditor.export_project_db", tr("Diagram editors"), QKeySequence());
		connect(m_export_project_db, &QAction::triggered, [this]() {
			projectDataBase::exportDb(this->currentProject()->dataBase(), this);
		});
	#endif

		//MDI view style
	m_tabbed_view_mode = new QAction(tr("using tabs"), this);
	ShortcutManager::instance().registerAction(m_tabbed_view_mode, "diagrameditor.tabbed_view_mode", tr("Diagram editors"), QKeySequence());
	m_tabbed_view_mode->setStatusTip(tr("Shows the various opened projects in tabs", "status bar tip"));
	m_tabbed_view_mode->setCheckable(true);
	connect(m_tabbed_view_mode, &QAction::triggered, this, &QETDiagramEditor::setTabbedMode);

	m_windowed_view_mode = new QAction(tr("using windows"), this);
	ShortcutManager::instance().registerAction(m_windowed_view_mode, "diagrameditor.windowed_view_mode", tr("Diagram editors"), QKeySequence());
	m_windowed_view_mode->setStatusTip(tr("Shows the various opened projects in windows", "status bar tip"));
	m_windowed_view_mode->setCheckable(true);
	connect(m_windowed_view_mode, &QAction::triggered, this, &QETDiagramEditor::setWindowedMode);

	m_group_view_mode = new QActionGroup(this);
	m_group_view_mode -> addAction(m_windowed_view_mode);
	m_group_view_mode -> addAction(m_tabbed_view_mode);
	m_group_view_mode -> setExclusive(true);

	m_tile_window = new QAction(tr("&Tile"), this);
	ShortcutManager::instance().registerAction(m_tile_window, "diagrameditor.tile_windows", tr("Diagram editors"), QKeySequence());
	m_tile_window->setStatusTip(tr("Arranges windows in a tile pattern", "status bar tip"));
	connect(m_tile_window, &QAction::triggered, &m_workspace, &QMdiArea::tileSubWindows);

	m_cascade_window = new QAction(tr("&Cascade"), this);
	ShortcutManager::instance().registerAction(m_cascade_window, "diagrameditor.cascade_windows", tr("Diagram editors"), QKeySequence());
	m_cascade_window->setStatusTip(tr("Arranges windows in a cascade pattern", "status bar tip"));
	connect(m_cascade_window, &QAction::triggered, &m_workspace, &QMdiArea::cascadeSubWindows);

		//Switch selection/view mode
	m_mode_selection = new QAction(QET::Icons::PartSelect, tr("Select"), this);
	ShortcutManager::instance().registerAction(m_mode_selection, "diagrameditor.mode_selection", tr("Diagram editors"), QKeySequence());
	m_mode_selection->setStatusTip(tr("Allows to select elements", "status bar tip"));
	m_mode_selection->setCheckable(true);
	m_mode_selection->setChecked(true);
	connect(m_mode_selection, &QAction::triggered, [this]() {
		if (ProjectView *pv = currentProjectView()) {
			for (DiagramView *dv : pv->diagram_views()) {
				dv->setSelectionMode();
			}
		}
	});

	m_mode_visualise = new QAction(QET::Icons::ViewMove, tr("Move"), this);
	ShortcutManager::instance().registerAction(m_mode_visualise, "diagrameditor.mode_visualise", tr("Diagram editors"), QKeySequence());
	m_mode_visualise->setStatusTip(tr("Allows to view the sheet without modifying it", "status bar tip"));
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
	m_next_window = new QAction(tr("Next project"), this);
	ShortcutManager::instance().registerAction(m_next_window, "diagrameditor.next_window", tr("Diagram editors"), QKeySequence::NextChild);
	m_next_window->setStatusTip(tr("Activates the next project", "status bar tip"));
	connect(m_next_window, &QAction::triggered, &m_workspace, &QMdiArea::activateNextSubWindow);

	m_previous_window = new QAction(tr("Previous project"), this);
	ShortcutManager::instance().registerAction(m_previous_window, "diagrameditor.previous_window", tr("Diagram editors"), QKeySequence::PreviousChild);
	m_previous_window->setStatusTip(tr("Activates the previous project", "status bar tip"));
	connect(m_previous_window, &QAction::triggered, &m_workspace, &QMdiArea::activatePreviousSubWindow);

		//Files action
	QAction *new_file  = m_file_actions_group.addAction(QET::Icons::ProjectNew,     tr("&New"));
	QAction *open_file = m_file_actions_group.addAction(QET::Icons::DocumentOpen,   tr("&Open"));
	m_save_file        = m_file_actions_group.addAction(QET::Icons::DocumentSave,   tr("&Save"));
	m_save_file_as     = m_file_actions_group.addAction(QET::Icons::DocumentSaveAs, tr("Save as"));
	m_save_copy_for_older = m_file_actions_group.addAction(QET::Icons::DocumentSaveAs,
														   tr("Save a copy for an older QElectroTech"));
	m_close_file       = m_file_actions_group.addAction(QET::Icons::ProjectClose,   tr("&Close"));

	ShortcutManager::instance().registerAction(new_file, "diagrameditor.new_file", tr("Diagram editors"), QKeySequence::New);
	ShortcutManager::instance().registerAction(open_file, "diagrameditor.open_file", tr("Diagram editors"), QKeySequence::Open);
	ShortcutManager::instance().registerAction(m_close_file, "diagrameditor.close_file", tr("Diagram editors"), QKeySequence::Close);
	ShortcutManager::instance().registerAction(m_save_file, "diagrameditor.save_file", tr("Diagram editors"), QKeySequence::Save);
	ShortcutManager::instance().registerAction(m_save_file_as, "diagrameditor.save_file_as", tr("Diagram editors"), Qt::CTRL | Qt::SHIFT | Qt::Key_S);

	new_file     ->setStatusTip( tr("Creates a new project", "status bar tip") );
	open_file    ->setStatusTip( tr("Opens an existing project", "status bar tip") );
	m_close_file ->setStatusTip( tr("Closes the current project", "status bar tip") );
	m_save_file    ->setStatusTip( tr("Saves the current project and all its sheets", "status bar tip") );
	m_save_file_as ->setStatusTip( tr("Saves the current project with a different file name", "status bar tip") );
	m_save_copy_for_older->setStatusTip(tr("Writes a copy of the current project as a .qet file, which "
										   "versions of QElectroTech that cannot open a .qetz open",
										   "status bar tip"));
	ShortcutManager::instance().registerAction(m_save_copy_for_older, "diagrameditor.save_copy_for_older",
											   tr("Diagram editors"), QKeySequence());
	connect(m_save_copy_for_older, &QAction::triggered, this, [this]() {
		if (ProjectView *project_view = currentProjectView()) {
			const QETResult result = project_view->saveCopyForOlderVersions();
			if (!result.isOk()) {
				showError(result);
			}
		}
	});

		//Starting a project from a template project. Kept out of
		//m_file_actions_group on purpose: that group is also what fills the
		//main toolbar, and this is a once-per-project action which would
		//only take a button away from what is used all day. It is placed in
		//the File menu by setUpMenu(), right under "New".
	m_new_from_template = new QAction(QET::Icons::ProjectNew,
					  tr("Nouveau à partir d'un modèle…"), this);
	m_new_from_template->setStatusTip(
				tr("Crée un projet à partir d'un projet modèle, sans toucher au modèle",
				   "status bar tip"));

	connect(m_save_file_as, &QAction::triggered, this, &QETDiagramEditor::saveAs);
	connect(m_save_file,    &QAction::triggered, this, &QETDiagramEditor::save);
	connect(new_file,       &QAction::triggered, this, &QETDiagramEditor::newProject);
	connect(m_new_from_template, &QAction::triggered, this, &QETDiagramEditor::newProjectFromTemplate);
	connect(open_file,      &QAction::triggered, this, &QETDiagramEditor::openProject);
	connect(m_close_file,   &QAction::triggered, [this]() {
		if (ProjectView *project_view = currentProjectView()) {
			closeProject(project_view);
		}
	});

		//Rows and Columns
	QAction *add_column    = m_row_column_actions_group.addAction( QET::Icons::EditTableInsertColumnRight, tr("Add a column") );
	QAction *remove_column = m_row_column_actions_group.addAction( QET::Icons::EditTableDeleteColumn,      tr("Remove a column") );
	QAction *add_row       = m_row_column_actions_group.addAction( QET::Icons::EditTableInsertRowUnder,    tr("Add row", "Add row") );
	QAction *remove_row    = m_row_column_actions_group.addAction( QET::Icons::EditTableDeleteRow,         tr("Remove a row","Remove row") );

	add_column    -> setStatusTip( tr("Add a column to the sheet", "status bar tip"));
	remove_column -> setStatusTip( tr("Remove a column from the sheet", "status bar tip"));
	add_row       -> setStatusTip( tr("Expands the sheet height", "status bar tip"));
	remove_row    -> setStatusTip( tr("Shrinks the sheet height", "status bar tip"));

	add_column   ->setData("add_column");
	remove_column->setData("remove_column");
	add_row      ->setData("add_row");
	remove_row   ->setData("remove_row");

	for (QAction *action : m_row_column_actions_group.actions()) {
		ShortcutManager::instance().registerAction(
			action, "diagrameditor." + action->data().toString(),
			tr("Diagram editors"), QKeySequence());
	}

	connect(&m_row_column_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::rowColumnGroupTriggered);

		//Selections Actions (related to a selected item)
	m_delete_selection     = m_selection_actions_group.addAction( QET::Icons::EditDelete,        tr("Delete")                 );
	m_rotate_selection     = m_selection_actions_group.addAction( QET::Icons::TransformRotate,   tr("Rotate")                   );
	m_rotate_group_selection = m_selection_actions_group.addAction( QET::Icons::TransformRotate, tr("Rotate the group")         );
	m_rotate_texts         = m_selection_actions_group.addAction( QET::Icons::ObjectRotateRight, tr("Choose texts orientation")       );
	m_mirror_horizontal    = m_selection_actions_group.addAction( QET::Icons::ImageFlipHorizontal, tr("Horizontal mirror")       );
	m_mirror_vertical      = m_selection_actions_group.addAction( QET::Icons::ImageFlipVertical,   tr("Vertical mirror")         );
	m_find_element         = m_selection_actions_group.addAction( QET::Icons::ZoomDraw,          tr("Find in the panel")   );
	m_edit_selection       = m_selection_actions_group.addAction( QET::Icons::ElementEdit,       tr("Edit the selected item") );
	m_group_selected_texts = m_selection_actions_group.addAction( QET::Icons::textGroup,         tr("Group selected texts"));
	m_group_selection      = m_selection_actions_group.addAction( tr("Group", "fr: Grouper") );
	m_ungroup_selection    = m_selection_actions_group.addAction( tr("Ungroup") );

	ShortcutManager::instance().registerAction(m_delete_selection, "diagrameditor.delete_selection", tr("Diagram editors"), Qt::Key_Delete);
	ShortcutManager::instance().registerAction(m_rotate_selection, "diagrameditor.rotate_selection", tr("Diagram editors"), Qt::Key_Space);
	ShortcutManager::instance().registerAction(m_rotate_group_selection, "diagrameditor.rotate_group_selection", tr("Diagram editors"), Qt::SHIFT | Qt::Key_Space);
	ShortcutManager::instance().registerAction(m_rotate_texts, "diagrameditor.rotate_texts", tr("Diagram editors"), Qt::CTRL | Qt::Key_Space);
		//M and F, the keys of the same two actions in the element editor
	ShortcutManager::instance().registerAction(m_mirror_horizontal, "diagrameditor.mirror_horizontal", tr("Diagram editors"), Qt::Key_M);
	ShortcutManager::instance().registerAction(m_mirror_vertical, "diagrameditor.mirror_vertical", tr("Diagram editors"), Qt::Key_F);
	ShortcutManager::instance().registerAction(m_find_element, "diagrameditor.find_element", tr("Diagram editors"), QKeySequence());
	ShortcutManager::instance().registerAction(m_group_selected_texts, "diagrameditor.group_selected_texts", tr("Diagram editors"), QKeySequence());
	ShortcutManager::instance().registerAction(m_edit_selection, "diagrameditor.edit_selection", tr("Diagram editors"), Qt::CTRL | Qt::Key_E);

		//Re-enter placement mode with the element placed last. Bare A rather
		//than Space: Space already rotates the pending element *inside*
		//placement mode (diagrameventaddelement.cpp), and is taken three times
		//over in this editor besides. A matches KiCad's add-symbol key and
		//reads correctly in the source language ("Ajouter"). ShortcutManager
		//makes it a default, not a commitment -- it appears in the Shortcuts
		//preference page like every other binding.
	m_insert_last_element = new QAction(QET::Icons::ElementNew,
					    tr("Insert the last element"), this);
	m_insert_last_element->setStatusTip(
		tr("Places the last inserted element again", "status bar tip"));
	m_insert_last_element->setData("insert_last_element");
	m_insert_last_element->setEnabled(false);
	ShortcutManager::instance().registerAction(
		m_insert_last_element, "diagrameditor.insert_last_element",
		tr("Diagram editors"), Qt::Key_A);
	connect(m_insert_last_element, &QAction::triggered,
		this, &QETDiagramEditor::insertLastElement);
	addAction(m_insert_last_element);
		//Type to find and run any command, as SolidWorks' "Search Commands"
		//and the command palette of many editors. Ctrl+Shift+P, the key those
		//editors use, is taken by the autonumbering dock; M for "menu".
	m_command_search = new QAction(tr("Search for a command…"), this);
	m_command_search->setStatusTip(
		tr("Type part of a command name and press Enter to launch it",
		   "status bar tip"));
	ShortcutManager::instance().registerAction(
		m_command_search, "diagrameditor.command_search",
		tr("Diagram editors"), Qt::CTRL | Qt::SHIFT | Qt::Key_M);
	connect(m_command_search, &QAction::triggered, this, [this]() {
		if (!m_command_search_popup) {
			m_command_search_popup = new CommandSearchPopup(this);
		}
		const QRect area = geometry();
		m_command_search_popup->popUpAt(
			area.contains(QCursor::pos()) ? QCursor::pos()
						      : area.center());
	});
	addAction(m_command_search);

		//Cursor-anchored picker. Insert is unbound anywhere in the tree and
		//reads correctly for the action, which keeps A free for the far more
		//frequent "place the same symbol again".
	m_show_element_picker = new QAction(QET::Icons::Add,
					    tr("Insert an element…"), this);
	m_show_element_picker->setStatusTip(
		tr("Opens the element picker at the cursor position",
		   "status bar tip"));
	m_show_element_picker->setData("show_element_picker");
	ShortcutManager::instance().registerAction(
		m_show_element_picker, "diagrameditor.show_element_picker",
		tr("Diagram editors"), Qt::Key_Insert);
	connect(m_show_element_picker, &QAction::triggered,
		this, &QETDiagramEditor::showElementPicker);
	addAction(m_show_element_picker);

		//The picker with a row of commands above it, chosen by what is
		//selected -- the SolidWorks "S" shortcut bar. S is unbound in this
		//editor.
	m_show_shortcut_bar = new QAction(tr("Shortcut bar"), this);
	m_show_shortcut_bar->setStatusTip(
		tr("Opens, at the cursor position, the commands useful for the selection, and the element picker",
		   "status bar tip"));
	ShortcutManager::instance().registerAction(
		m_show_shortcut_bar, "diagrameditor.show_shortcut_bar",
		tr("Diagram editors"), Qt::Key_S);
	connect(m_show_shortcut_bar, &QAction::triggered,
		this, &QETDiagramEditor::showShortcutBar);
	addAction(m_show_shortcut_bar);

		//Enter on the folio repeats it (DiagramView::keyPressEvent). That is
		//handled by the view rather than bound here, so Enter keeps working
		//in the search fields, the collection tree and every dialog.
	m_repeat_last_command = new QAction(tr("Repeat the last command"), this);
	m_repeat_last_command->setStatusTip(
		tr("Relaunches the last drawing tool or element insertion (Enter on the sheet)",
		   "status bar tip"));
	m_repeat_last_command->setEnabled(false);
	ShortcutManager::instance().registerAction(
		m_repeat_last_command, "diagrameditor.repeat_last_command",
		tr("Diagram editors"), QKeySequence());
	connect(m_repeat_last_command, &QAction::triggered,
		this, &QETDiagramEditor::repeatLastCommand);
	addAction(m_repeat_last_command);

	m_delete_selection->setStatusTip( tr("Removes selected elements from the sheet", "status bar tip"));
	m_rotate_selection->setStatusTip( tr("Rotate selected elements and texts", "status bar tip"));
	m_rotate_group_selection->setStatusTip( tr("Rotates the selection as a group around its centre, rather than each element in place", "status bar tip"));
	m_rotate_texts    ->setStatusTip( tr("Rotate selected texts to a specific angle", "status bar tip"));
	m_mirror_horizontal->setStatusTip( tr("Flips the selected elements from left to right", "status bar tip"));
	m_mirror_vertical ->setStatusTip( tr("Flips the selected elements from top to bottom", "status bar tip"));
	m_find_element    ->setStatusTip( tr("Find the selected item in the panel", "status bar tip"));

	m_delete_selection    ->setData("delete_selection");
	m_rotate_selection    ->setData("rotate_selection");
	m_rotate_group_selection->setData("rotate_group_selection");
	m_rotate_texts        ->setData("rotate_selected_text");
	m_mirror_horizontal   ->setData("mirror_horizontal");
	m_mirror_vertical     ->setData("mirror_vertical");
	m_find_element        ->setData("find_selected_element");
	m_edit_selection      ->setData("edit_selected_element");
	m_group_selected_texts->setData("group_selected_texts");
	m_group_selection     ->setData("group_selection");
	m_ungroup_selection   ->setData("ungroup_selection");

		//No default key: Ctrl+G is "jump to element". A user can bind one.
	ShortcutManager::instance().registerAction(m_group_selection, "diagrameditor.group_selection", tr("Diagram editors"), QKeySequence());
	ShortcutManager::instance().registerAction(m_ungroup_selection, "diagrameditor.ungroup_selection", tr("Diagram editors"), QKeySequence());
	m_group_selection  ->setStatusTip(tr("Groups the selected elements, texts, shapes and images: they are selected, moved and copied together", "status bar tip"));
	m_ungroup_selection->setStatusTip(tr("Breaks up the selected groups", "status bar tip"));

	connect(&m_selection_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::selectionGroupTriggered);

		//Align actions. No default shortcut: they are reached from the
		//Edit menu, the selection's context menu and the command search,
		//and a user can bind one if they want.
	QAction *snap_to_grid = m_align_actions_group.addAction(QET::Icons::SnapToGrid, tr("Align to grid"));
	ShortcutManager::instance().registerAction(snap_to_grid, "diagrameditor.snap_selection_to_grid", tr("Diagram editors"), QKeySequence());
	snap_to_grid->setStatusTip(tr("Puts the selected elements, images and texts back on the grid", "status bar tip"));
	snap_to_grid->setData("snap_selection_to_grid");

	const struct {
		const char *id;
		const QIcon &icon;
		QString text;
		QString tip;
	} align_actions[] = {
		{"align_left",    QET::Icons::AlignLeft,    tr("Align left"),        tr("Aligns the left edges of the selected items", "status bar tip")},
		{"align_hcenter", QET::Icons::AlignHCenter, tr("Centre horizontally"), tr("Aligns the selected items on one vertical line, by their origin point for elements", "status bar tip")},
		{"align_right",   QET::Icons::AlignRight,   tr("Align right"),        tr("Aligns the right edges of the selected items", "status bar tip")},
		{"align_top",     QET::Icons::AlignTop,     tr("Align top"),         tr("Aligns the top edges of the selected items", "status bar tip")},
		{"align_vcenter", QET::Icons::AlignVCenter, tr("Centre vertically"),   tr("Aligns the selected items on one horizontal line, by their origin point for elements", "status bar tip")},
		{"align_bottom",  QET::Icons::AlignBottom,  tr("Align bottom"),          tr("Aligns the bottom edges of the selected items", "status bar tip")}
	};
	for (const auto &a : align_actions)
	{
		QAction *action = m_align_actions_group.addAction(a.icon, a.text);
		ShortcutManager::instance().registerAction(action, QStringLiteral("diagrameditor.") + QLatin1String(a.id), tr("Diagram editors"), QKeySequence());
		action->setStatusTip(a.tip);
		action->setData(QString::fromLatin1(a.id));
	}
		//Distribute: the first item stays and the others follow it at one
		//pitch, asked first (alignGroupTriggered())
	const struct {
		const char *id;
		QString text;
		QString tip;
	} distribute_actions[] = {
		{"distribute_vertical",   tr("Distribute vertically…"),   tr("Spaces the selected items evenly downwards, the top one staying in place", "status bar tip")},
		{"distribute_horizontal", tr("Distribute horizontally…"), tr("Spaces the selected items evenly to the right, the left one staying in place", "status bar tip")}
	};
	for (const auto &a : distribute_actions)
	{
		QAction *action = m_align_actions_group.addAction(a.text);
		ShortcutManager::instance().registerAction(action, QStringLiteral("diagrameditor.") + QLatin1String(a.id), tr("Diagram editors"), QKeySequence());
		action->setStatusTip(a.tip);
		action->setData(QString::fromLatin1(a.id));
	}
	connect(&m_align_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::alignGroupTriggered);

		//Select Action
	QAction *select_all     = m_select_actions_group.addAction( QET::Icons::EditSelectAll,      tr("Select All") );
	QAction *select_nothing = m_select_actions_group.addAction( QET::Icons::EditSelectNone,     tr("Select none") );
	QAction *select_invert  = m_select_actions_group.addAction( QET::Icons::EditSelectInvert,   tr("Invert selection") );
	QAction *select_all_conductors  = m_select_actions_group.addAction( QET::Icons::Conductor,      tr("Select all conductors") );
	QAction *select_all_text_fields = m_select_actions_group.addAction( QET::Icons::PartTextField,  tr("Select all text fields") );

	ShortcutManager::instance().registerAction(select_all, "diagrameditor.select_all", tr("Diagram editors"), QKeySequence::SelectAll);
	ShortcutManager::instance().registerAction(select_nothing, "diagrameditor.select_nothing", tr("Diagram editors"), QKeySequence::Deselect);
	ShortcutManager::instance().registerAction(select_invert, "diagrameditor.select_invert", tr("Diagram editors"), Qt::CTRL | Qt::Key_I);
		//No default sequence for these two: they are menu actions, and the
		//point of registering them is so a user can bind one if they want.
	ShortcutManager::instance().registerAction(select_all_conductors, "diagrameditor.select_all_conductors", tr("Diagram editors"), QKeySequence());
	ShortcutManager::instance().registerAction(select_all_text_fields, "diagrameditor.select_all_text_fields", tr("Diagram editors"), QKeySequence());

	select_all    ->setStatusTip( tr("Selects all elements on the sheet", "status bar tip") );
	select_nothing->setStatusTip( tr("Deselect all elements on the sheet", "status bar tip") );
	select_invert ->setStatusTip( tr("Inverts selection of elements", "status bar tip") );
	select_all_conductors ->setStatusTip( tr("Selects all conductors on the sheet, deselects the rest", "status bar tip") );
	select_all_text_fields->setStatusTip( tr("Selects all text fields on the sheet, deselects the rest", "status bar tip") );

	select_all    ->setData("select_all");
	select_nothing->setData("deselect");
	select_invert ->setData("invert_selection");
	select_all_conductors ->setData("select_all_conductors");
	select_all_text_fields->setData("select_all_text_fields");

	connect(&m_select_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::selectGroupTriggered);

		//Zoom actions
	QAction *zoom_in      = m_zoom_actions_group.addAction( QET::Icons::ZoomIn,       tr("Zoom In"));
	QAction *zoom_out     = m_zoom_actions_group.addAction( QET::Icons::ZoomOut,      tr("Zoom Out"));
	QAction *zoom_content = m_zoom_actions_group.addAction( QET::Icons::ZoomDraw,     tr("Zoom content"));
	QAction *zoom_fit     = m_zoom_actions_group.addAction( QET::Icons::ZoomFitBest,  tr("Fit in view"));
	QAction *zoom_reset   = m_zoom_actions_group.addAction( QET::Icons::ZoomOriginal, tr("Reset zoom"));
	QAction *center_on_cursor = m_zoom_actions_group.addAction(tr("Centre on the cursor"));
	m_zoom_action_toolBar << zoom_content << zoom_fit << zoom_reset;

	ShortcutManager::instance().registerAction(zoom_in, "diagrameditor.zoom_in", tr("Diagram editors"), QKeySequence::ZoomIn);
	ShortcutManager::instance().registerAction(zoom_out, "diagrameditor.zoom_out", tr("Diagram editors"), QKeySequence::ZoomOut);
	ShortcutManager::instance().registerAction(zoom_content, "diagrameditor.zoom_content", tr("Diagram editors"), Qt::CTRL | Qt::Key_8);
	ShortcutManager::instance().registerAction(zoom_fit, "diagrameditor.zoom_fit", tr("Diagram editors"), Qt::CTRL | Qt::Key_9);
	ShortcutManager::instance().registerAction(zoom_reset, "diagrameditor.zoom_reset", tr("Diagram editors"), Qt::CTRL | Qt::Key_0);
	ShortcutManager::instance().registerAction(center_on_cursor, "diagrameditor.center_on_cursor", tr("Diagram editors"), QKeySequence());

	zoom_in     ->setStatusTip(tr("Expand the sheet", "status bar tip"));
	zoom_out    ->setStatusTip(tr("Shrinks the sheet", "status bar tip"));
	zoom_content->setStatusTip(tr("Adjusts the zoom to display all the contents of sheet regardless of context"));
	zoom_fit    ->setStatusTip(tr("Adjusts the zoom on exactly the part of the sheet", "status bar tip"));
	zoom_reset  ->setStatusTip(tr("Restores default zoom level", "status bar tip"));
	center_on_cursor->setStatusTip(tr("Centres the sheet on the point under the mouse cursor, without changing the zoom", "status bar tip"));

	zoom_in     ->setData("zoom_in");
	zoom_out    ->setData("zoom_out");
	zoom_content->setData("zoom_content");
	zoom_fit    ->setData("zoom_fit");
	zoom_reset  ->setData("zoom_reset");
	center_on_cursor->setData("center_on_cursor");

	connect(&m_zoom_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::zoomGroupTriggered);

		//Adding action (add text, image, shape...)
	// Exclusive (the default) prevents the active action from ever being
	// unchecked by clicking it again -- Qt only lets you switch to a
	// different one. That's exactly why clicking an already-active
	// tool's own icon never actually deactivated it: confirmed by
	// logging every addItemGroupTriggered() call and finding
	// isChecked()==true on *every* click, including the one meant to
	// turn the tool off -- the click-handling code's own "was this an
	// uncheck?" check was correct, its precondition just could never
	// occur. ExclusiveOptional allows exactly one more transition:
	// clicking the currently-checked action unchecks it, leaving none
	// checked, which is required for "click the active tool to turn it
	// off" to mean anything at the QAction level at all.
	m_add_item_actions_group.setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
	QAction *add_text      = m_add_item_actions_group.addAction(QET::Icons::PartTextField, tr("Add a textfield"));
	QAction *add_image	   = m_add_item_actions_group.addAction(QET::Icons::adding_image,  tr("Add a picture"));
#ifdef QET_HAS_QTPDF
	QAction *add_pdf	   = m_add_item_actions_group.addAction(QET::Icons::adding_pdf,   tr("Add a PDF"));
#endif
	QAction *add_line	   = m_add_item_actions_group.addAction(QET::Icons::PartLine,      tr("Add line", "Draw line"));
	QAction *add_rectangle = m_add_item_actions_group.addAction(QET::Icons::PartRectangle, tr("Add a rectangle"));
	QAction *add_ellipse   = m_add_item_actions_group.addAction(QET::Icons::PartEllipse,   tr("Add an ellipse"));
	QAction *add_arc       = m_add_item_actions_group.addAction(QET::Icons::PartArc,       tr("Add an arc"));
	QAction *add_polyline  = m_add_item_actions_group.addAction(QET::Icons::PartPolygon,   tr("Add a polygon"));
	QAction *add_location_area = m_add_item_actions_group.addAction(QET::Icons::LocationArea, tr("Ajouter une zone de localisation"));
	QAction *add_path      = m_add_item_actions_group.addAction(QET::Icons::PartBezier,   tr("Add a curve"));
	QAction *add_fillet    = m_add_item_actions_group.addAction(QET::Icons::DrawFillet,   tr("Add a fillet"));
	QAction *add_terminal_strip = m_add_item_actions_group.addAction(QET::Icons::TerminalStrip, tr("Add a terminal plan"));
	QAction *add_generic_device = m_add_item_actions_group.addAction(QET::Icons::GenericDevice, tr("Add a generic device…"));
		//Not an element of the drawing but a line drawn across the
		//conductors: what it crosses becomes the cores of a cable.
	m_add_cable		   = m_add_item_actions_group.addAction(QET::Icons::Cable,      tr("Draw a cable"));

	add_text     ->setStatusTip(tr("Adds a text field to the current sheet"));
	add_image    ->setStatusTip(tr("Add an image to the current sheet"));
#ifdef QET_HAS_QTPDF
	add_pdf      ->setStatusTip(tr("Adds a PDF page to the current sheet"));
#endif
	add_line     ->setStatusTip(tr("Add a line to the current sheet"));
	add_rectangle->setStatusTip(tr("Adds a rectangle on the current sheet"));
	add_ellipse  ->setStatusTip(tr("Add an ellipse to the current sheet"));
	add_arc      ->setStatusTip(tr("Adds an arc on the current sheet"));
	add_polyline ->setStatusTip(tr("Adds a polyline to the current sheet"));
	add_location_area->setStatusTip(tr("Dessine sur le folio actuel une zone qui affecte les composants qu'elle contient"));
	add_path     ->setStatusTip(tr("Adds a Bézier curve to the current sheet"));
	add_fillet   ->setStatusTip(tr("Rounds the corner between two lines of the current sheet"));
	add_terminal_strip->setStatusTip(tr("Adds a terminal plan to the current sheet"));
	add_generic_device->setStatusTip(tr("Makes a box symbol with terminals on any side and places it on the current sheet"));
	m_add_cable     ->setStatusTip(tr("Draws the main line of a cable: every conductor crossed becomes one of its cores"));

	add_text     ->setData(QStringLiteral("text"));
	add_image    ->setData(QStringLiteral("image"));
#ifdef QET_HAS_QTPDF
	add_pdf      ->setData(QStringLiteral("pdf"));
#endif
	add_line     ->setData(QStringLiteral("line"));
	add_rectangle->setData(QStringLiteral("rectangle"));
	add_ellipse  ->setData(QStringLiteral("ellipse"));
	add_arc      ->setData(QStringLiteral("arc"));
	add_polyline ->setData(QStringLiteral("polyline"));
	add_location_area->setData(QStringLiteral("location_area"));
	add_path     ->setData(QStringLiteral("path"));
	add_fillet   ->setData(QStringLiteral("fillet"));
	add_terminal_strip->setData(QStringLiteral("terminal_strip"));
	add_generic_device->setData(QStringLiteral("generic_device"));
	m_add_cable     ->setData(QStringLiteral("cable"));

	add_text->setCheckable(true);
	add_line->setCheckable(true);
	add_rectangle->setCheckable(true);
	add_ellipse->setCheckable(true);
	add_arc->setCheckable(true);
	add_polyline->setCheckable(true);
	add_location_area->setCheckable(true);
	add_path->setCheckable(true);
	m_add_cable->setCheckable(true);
	add_fillet->setCheckable(true);

	connect(&m_add_item_actions_group, &QActionGroup::triggered, this, &QETDiagramEditor::addItemGroupTriggered);
		//No default key, but an id: they can then be found by the command
		//search, bound in the Shortcuts page and placed on the shortcut bar,
		//like every other command.
	for (QAction *action : m_add_item_actions_group.actions()) {
		ShortcutManager::instance().registerAction(
			action, "diagrameditor.add_" + action->data().toString(),
			tr("Diagram editors"), QKeySequence());
	}

		//Depth action
	m_depth_action_group = QET::depthActionGroup(this);
	m_depth_action_group->setDisabled(true);

	connect(m_depth_action_group, &QActionGroup::triggered, [this](QAction *action) {
		this->currentDiagramView()->diagram()->changeZValue(action->data().value<QET::DepthOption>());
	});

	m_find = new QAction(tr("Search / Replace"), this);
	ShortcutManager::instance().registerAction(m_find, "diagrameditor.find", tr("Diagram editors"), QKeySequence::Find);
	connect(m_find, &QAction::triggered, [this]()
	{
		if (auto animator = m_search_and_replace_widget.findChild<QWidgetAnimation *>("search and replace animator")) {
			animator->setHidden(!m_search_and_replace_widget.isHidden());
		} else {
			this->m_search_and_replace_widget.setHidden(!m_search_and_replace_widget.isHidden());
		}
	});

	m_jump_to_element = new QAction(tr("Navigate to an element"), this);
	ShortcutManager::instance().registerAction(m_jump_to_element, "diagrameditor.jump_to_element", tr("Diagram editors"), Qt::CTRL | Qt::Key_G);
	m_jump_to_element->setStatusTip(tr("Quickly search for and select an element in the sheet", "status bar tip"));
	connect(m_jump_to_element, &QAction::triggered, [this]()
	{
		DiagramView *diagram_view = this->currentDiagramView();
		if (!diagram_view || !diagram_view->diagram()) {
			return;
		}
		JumpToElementDialog dialog(diagram_view->diagram(), this);
		dialog.exec();
	});

		//Navigate is a third way of doing what a double click on a cross
		//reference already does, and that is the whole point: the double
		//click only exists on the label itself, which the reader has to find
		//first. The menu, the right click and the key work on the component,
		//and they are what a project of eighty folios is read with.
	m_navigate = new QAction(QET::Icons::FolioXrefComing, tr("Naviguer vers la référence"), this);
	ShortcutManager::instance().registerAction(m_navigate, "diagrameditor.navigate", tr("Éditeur de schémas"), Qt::Key_F12);
	m_navigate->setStatusTip(tr("Atteint l'autre représentation de l'objet sélectionné", "status bar tip"));
	connect(m_navigate, &QAction::triggered, this, &QETDiagramEditor::navigateToReference);

		//Back and Forward retrace the folios the reader went through, as the
		//buttons of a web browser do with pages - not the order of the
		//folios in the project, which PageUp and PageDown already walk. Each
		//project keeps its own way (ProjectView). Alt with an arrow is the
		//fine keyboard move of a selection, hence Ctrl+Alt.
	m_folio_back = new QAction(tr("Revenir dans l'historique des folios"), this);
	ShortcutManager::instance().registerAction(m_folio_back, "diagrameditor.folio_back", tr("Éditeur de schémas"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));
	m_folio_back->setStatusTip(tr("Revient au folio vu avant celui-ci", "status bar tip"));
	connect(m_folio_back, &QAction::triggered, this, [this]() {
		if (ProjectView *project_view = currentProjectView()) {
			project_view->goBack();
		}
	});

	m_folio_forward = new QAction(tr("Avancer dans l'historique des folios"), this);
	ShortcutManager::instance().registerAction(m_folio_forward, "diagrameditor.folio_forward", tr("Éditeur de schémas"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right));
	m_folio_forward->setStatusTip(tr("Retourne au folio quitté en revenant", "status bar tip"));
	connect(m_folio_forward, &QAction::triggered, this, [this]() {
		if (ProjectView *project_view = currentProjectView()) {
			project_view->goForward();
		}
	});

		//And the two side buttons of a mouse, anywhere in this window.
	new FolioNavigationButtons(this, m_folio_back, m_folio_forward);
}

/**
	@brief QETDiagramEditor::setUpToolBar
*/
void QETDiagramEditor::setUpToolBar()
{
	main_tool_bar = new QToolBar(tr("Tools"), this);
	main_tool_bar -> setObjectName("toolbar");

	view_tool_bar = new QToolBar(tr("Display"), this);
	view_tool_bar -> setObjectName("display");

	diagram_tool_bar = new QToolBar(tr("Diagram"), this);
	diagram_tool_bar -> setObjectName("diagram");

	m_add_item_tool_bar = new QToolBar(tr("Add"), this);
	m_add_item_tool_bar->setObjectName("adding");

	m_depth_tool_bar = new QToolBar(tr("Depth", "toolbar title"));
	m_depth_tool_bar->setObjectName("diagram_depth_toolbar");

		//The toolbar buttons that are widgets, not commands. Each is held by
		//an action of this window, so it survives being taken off a toolbar.
	auto add_widget = [this](const QString &id, QWidget *widget) {
		auto *action = new QWidgetAction(this);
		action->setDefaultWidget(widget);
		action->setText(DiagramToolbarSettings::widgetTitle(id));
		m_toolbar_widgets.insert(id, action);
	};
	add_widget(QStringLiteral("widget:handler_size"), new DiagramEditorHandlerSizeWidget(this));
	add_widget(QStringLiteral("widget:text_grid"), m_text_grid_button);
	add_widget(QStringLiteral("widget:background_color"), m_background_color_button);
		//Sits with the conductor actions it works alongside: it colours
		//the selected conductors and sets the colour of the next one drawn.
	m_conductor_color_button = new ConductorColorToolButton(this, this);
	add_widget(QStringLiteral("widget:conductor_color"), m_conductor_color_button);

	addToolBar(Qt::TopToolBarArea, main_tool_bar);
	addToolBar(Qt::TopToolBarArea, view_tool_bar);
	addToolBar(Qt::TopToolBarArea, diagram_tool_bar);
	addToolBar(Qt::TopToolBarArea, m_add_item_tool_bar);
	addToolBar(Qt::TopToolBarArea, m_depth_tool_bar);

	DiagramToolbarSettings::markWindow(this);
	rebuildToolBars();

	m_scripts_tool_bar = new QToolBar(tr("Scripts", "toolbar title"), this);
	m_scripts_tool_bar->setObjectName("scripts");
	addToolBar(Qt::TopToolBarArea, m_scripts_tool_bar);
#ifndef QET_HAS_SCRIPTING
	m_scripts_tool_bar->toggleViewAction()->setVisible(false);
	m_scripts_tool_bar->hide();
#endif

		//The selected tool has to be recognisable, which means the icon on it
		//has to stay visible. See QETApp::toolBarStyleSheet(). Again here,
		//after the scripts toolbar, which rebuildToolBars() does not build.
	QETApp::styleToolBars(this);
}

/**
	@brief QETDiagramEditor::rebuildToolBars
	Fill every toolbar from DiagramToolbarSettings, adding the user's own
	toolbars that this window does not have yet and removing those that
	were deleted. Called when the window is built, when the settings
	change, and when the stored scripts change (a script can be on any
	toolbar).
*/
void QETDiagramEditor::rebuildToolBars()
{
	const QList<DiagramToolbarSettings::Toolbar> custom = DiagramToolbarSettings::customToolbars();
	QStringList custom_names;
	for (const DiagramToolbarSettings::Toolbar &toolbar : custom) {
		custom_names << toolbar.name;
	}
	for (QToolBar *toolbar : m_custom_tool_bars) {
		if (!custom_names.contains(toolbar->objectName())) {
			removeToolBar(toolbar);
			toolbar->deleteLater();
		}
	}
	m_custom_tool_bars.erase(std::remove_if(m_custom_tool_bars.begin(), m_custom_tool_bars.end(),
		[&custom_names](QToolBar *toolbar) { return !custom_names.contains(toolbar->objectName()); }),
		m_custom_tool_bars.end());

	for (const DiagramToolbarSettings::Toolbar &toolbar : custom)
	{
		auto existing = std::find_if(m_custom_tool_bars.cbegin(), m_custom_tool_bars.cend(),
			[&toolbar](QToolBar *t) { return t->objectName() == toolbar.name; });
		if (existing != m_custom_tool_bars.cend()) {
			(*existing)->setWindowTitle(toolbar.title);
			continue;
		}
		auto *new_tool_bar = new QToolBar(toolbar.title, this);
		new_tool_bar->setObjectName(toolbar.name);
		new_tool_bar->setMovable(!ToolbarSettings::locked());
		addToolBar(Qt::TopToolBarArea, new_tool_bar);
		m_custom_tool_bars << new_tool_bar;
	}

	auto resolve = [this](const QString &id) -> QAction * {
		if (DiagramToolbarSettings::isWidget(id)) {
			return m_toolbar_widgets.value(id);
		}
		return ShortcutManager::instance().action(id, this);
	};
	const QList<QToolBar *> built_in {main_tool_bar, view_tool_bar, diagram_tool_bar,
					   m_add_item_tool_bar, m_depth_tool_bar};
	for (QToolBar *toolbar : built_in + m_custom_tool_bars) {
		DiagramToolbarSettings::fill(toolbar, DiagramToolbarSettings::ids(toolbar->objectName()), resolve);
	}

		//The toolbars this call has just created, a user one included, get
		//the same style as the others. See QETApp::toolBarStyleSheet().
	QETApp::styleToolBars(this);
}

/**
	@brief QETDiagramEditor::assignCatalogPart
	Assign one catalog part to every component selected on the current folio.

	The most repeated action of the day, which is why it is one dialog and one
	confirmation and nothing else.

	Assigning a part takes away the accessories the part it replaces had put
	on the component, and puts its own in their place - the auxiliary blocks
	of the component are where both live. What somebody typed into a block by
	hand is not touched, by the same rule that guards every other field, and
	that is why there is no warning here: nothing a person wrote is at stake.
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
	if (!catalog) {
		return;
	}

		//The selection is right here, so « New part » inside the browser can be
		//born with one pin per terminal of the symbol and with what the
		//designer already typed while drawing. Opening it empty asks for all
		//of that a second time, from someone who has the component on screen.
	const CatalogPart part = CatalogBrowserDialog::choosePart(
				catalog, this,
				CatalogProjectActions::partFromElements(*catalog, selected));
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
	@brief Draw the symbol of a part from the pinout the catalogue holds.

	No selection is needed and no sheet either: what is drawn comes from the
	catalogue. The open project is passed all the same, because a terminal
	already drawn somewhere in it is a terminal this block may not draw a
	second time, and that is checked before the block exists.
*/
void QETDiagramEditor::generatePinoutBlocks()
{
	PinoutGeneratorDialog dialog(QETApp::catalog(), currentProject(), this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

		//The collection is reloaded so the new blocks are in the panel at
		//once: making a symbol and then not finding it is the same as not
		//having made it.
	if (m_element_collection_widget) {
		m_element_collection_widget->reload();
	}
	statusBar()->showMessage(
				tr("%n bloc(s) enregistré(s) dans la bibliothèque.",
				   "", dialog.savedPaths().size()),
				8000);
}

/**
	@brief QETDiagramEditor::generateCircuits
	Draw a whole table of circuits at once: one row per circuit, a
	parameterised macro and its answers.
*/
void QETDiagramEditor::generateCircuits()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

	CircuitTableDialog dialog(project, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

		//The generator pushed its own undo macro on the project stack, and
		//the folios it made are already open: nothing to refresh here, only
		//the count to say out loud.
	statusBar()->showMessage(dialog.report().text(), 8000);
}

/**
	@brief QETDiagramEditor::importIoSheet
	Bring a sheet of I/O points into the project: the points exist before
	any of them is on a folio, which is the state this program did not have.
*/
void QETDiagramEditor::importIoSheet()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

	IoImportDialog dialog(project, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

		//The dialogue pushed its own command on the project stack. Nothing is
		//drawn yet, so there is no folio to refresh - only the count of what
		//moved, which is what tells the person the second import did what they
		//read in the summary.
	statusBar()->showMessage(dialog.report().text(), 8000);
}

/**
	@brief QETDiagramEditor::assignIoPoints
	Put the I/O points already in the project into the channels of a PLC
	card, or take them back out.
*/
void QETDiagramEditor::assignIoPoints()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

		//The window stays open across several cards, so it closes on Close
		//and never on Accepted: what it did is read from the report and
		//not from the result code.
	IoAssignDialog dialog(project, this);
	dialog.exec();

	const QString report = dialog.report();
	if (!report.isEmpty()) {
		statusBar()->showMessage(report, 8000);
	}
}

/**
	@brief QETDiagramEditor::showIoList
	Read the entries and exits of the project, the automates and their
	cards above them, and correct them on the spot.
*/
void QETDiagramEditor::showIoList()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

		//The window is not modal, and that is the whole point of it: the
		//jump it offers opens a folio behind it, and a list that has to
		//be closed to see what it points at is a report, not a list. One
		//window per project is enough, so an already open one is brought
		//back rather than doubled.
	const QList<IoListDialog *> opened = findChildren<IoListDialog *>();
	for (IoListDialog *opened_dialog : opened) {
		if (opened_dialog->project() != project) {
			continue;
		}
		opened_dialog->show();
		opened_dialog->raise();
		opened_dialog->activateWindow();
		return;
	}

	IoListDialog *dialog = new IoListDialog(project, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
}

/**
	@brief QETDiagramEditor::showLocationManager
	Open the window where the enclosures of the project are made, named,
	nested and taken apart, and where a handful of components is put into
	one of them in a single gesture.
*/
void QETDiagramEditor::showLocationManager()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

		//Not modal, and for the same reason the I/O list is not: what is
		//being assigned is the selection on the folio behind, so a window
		//that has to be closed to see what it points at would be a report
		//instead of a manager. One window per project is enough, so an
		//already open one is brought back rather than doubled.
	const QList<LocationManagerDialog *> opened =
			findChildren<LocationManagerDialog *>();
	for (LocationManagerDialog *opened_dialog : opened) {
		if (opened_dialog->project() != project) {
			continue;
		}
		opened_dialog->show();
		opened_dialog->raise();
		opened_dialog->activateWindow();
		return;
	}

	LocationManagerDialog *dialog = new LocationManagerDialog(project, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
}

/**
	@brief QETDiagramEditor::showLocationReport
	Open the worklist of the components that say they are nowhere.
*/
void QETDiagramEditor::showLocationReport()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

		//Not modal, and here it matters more than anywhere else: the whole
		//point of the window is that a double click sends the person to a
		//component on its folio, and a modal report could not let them look.
	const QList<LocationReportDialog *> opened =
			findChildren<LocationReportDialog *>();
	for (LocationReportDialog *opened_dialog : opened) {
		if (opened_dialog->project() != project) {
			continue;
		}
		opened_dialog->show();
		opened_dialog->raise();
		opened_dialog->activateWindow();
		return;
	}

	LocationReportDialog *dialog = new LocationReportDialog(project, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(dialog, &LocationReportDialog::goToElement,
		this, &QETDiagramEditor::goToElement);
	dialog->show();
}

/**
	@brief QETDiagramEditor::showLocationBom
	Open the list of what has to be picked in the storeroom for one
	enclosure.
*/
void QETDiagramEditor::showLocationBom()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

	const QList<LocationBomDialog *> opened =
			findChildren<LocationBomDialog *>();
	for (LocationBomDialog *opened_dialog : opened) {
		if (opened_dialog->project() != project) {
			continue;
		}
		opened_dialog->show();
		opened_dialog->raise();
		opened_dialog->activateWindow();
		return;
	}

	LocationBomDialog *dialog = new LocationBomDialog(project, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(dialog, &LocationBomDialog::insertTable,
		this, &QETDiagramEditor::insertNomenclature);
	dialog->show();
}

/**
	@brief QETDiagramEditor::showMountingLayout
	Open the window a panel is laid out in: one face of one enclosure, drawn
	in millimetre, with what is screwed to it.
*/
void QETDiagramEditor::showMountingLayout()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

		//One window per project, and here it is not a convenience: two
		//windows over the same layout would both write it back, and the
		//last drag to happen anywhere would become the whole truth. The
		//list is asked of the application and not of this widget,
		//because a layout editor has no parent - that is what puts it
		//in the window list of the program.
	const QList<MountingLayoutEditor *> opened =
			QETApp::mountingLayoutEditors(project);
	if (!opened.isEmpty())
	{
		MountingLayoutEditor *editor = opened.first();
		editor->show();
		editor->raise();
		editor->activateWindow();
		return;
	}

	MountingLayoutEditor *editor = new MountingLayoutEditor(project);
	editor->setAttribute(Qt::WA_DeleteOnClose);
		//From a part to its symbol: the plate asks, this window knows
		//where the folios are.
	connect(editor, &MountingLayoutEditor::goToElement, this, [this](Element *element) {
		goToElement(element);
			//In front: the plate window is above this one, and a folio
			//changed behind it is a change nobody sees
		raise();
		activateWindow();
	});
	editor->show();
}

/**
	@brief QETDiagramEditor::showSelectionOnPlate
	Open the mounting layout of the project with the part of the selected
	element selected and in view. One element, and one that is mounted:
	the context menu offers nothing else.
*/
void QETDiagramEditor::showSelectionOnPlate()
{
	DiagramView *view = currentDiagramView();
	QETProject *project = currentProject();
	if (!view || !view->diagram() || !project) {
		return;
	}
	const QList<QGraphicsItem *> selected = view->diagram()->selectedItems();
	if (selected.count() != 1) {
		return;
	}
	const Element *element = qgraphicsitem_cast<Element *>(selected.first());
	if (!element) {
		return;
	}
	const QString item = project->mountingLayout()
			     .itemOfElement(element->uuid().toString());
	if (item.isEmpty()) {
		return;
	}

	showMountingLayout();
	const QList<MountingLayoutEditor *> opened = QETApp::mountingLayoutEditors(project);
	if (!opened.isEmpty()) {
		opened.first()->pointAtItem(item);
	}
}

/**
	@brief QETDiagramEditor::putMountingLayoutOnFolio
	Pose a view of one mounting plate on the folio in front, at a scale the
	person chooses.

	Two questions and no dialog of its own, on purpose. Which face, because a
	project has several and the program must not pick; and at which scale,
	because that decision has not been taken anywhere yet - the workshop has
	not said what scale it wants its drawings at, nor whether they go on a
	sheet of their own. Until it does, the scale is a property of the item,
	typed in by hand, and the second question is where the hand goes.

	What is posed is a view and not a copy: it holds the identifier of the
	face, reads everything else out of the project, and redraws itself when
	the plate changes. Nothing it draws can be written back.
*/
void QETDiagramEditor::putMountingLayoutOnFolio()
{
	DiagramView *diagram_view = currentDiagramView();
	if (!diagram_view || !diagram_view->diagram()) {
		return;
	}

	Diagram *diagram_ = diagram_view->diagram();
	QETProject *project = diagram_->project();
	if (!project || project->isReadOnly()) {
		return;
	}

	const MountingLayout layout = project->mountingLayout();
	const QStringList faces = layout.surfaceUuids();

	if (faces.isEmpty())
	{
		QET::QetMessageBox::information(
				this,
				tr("Poser le calepinage sur le folio"),
				tr("Ce projet n'a encore aucune platine. Ouvrez "
				   "« Calepinage de l'armoire… » pour en créer une."));
		return;
	}

		//The person chooses by the name of the face and never by its
		//identifier : designation() is what the layout window shows, so
		//the two lists read the same.
	QStringList labels;
	for (const QString &face : faces) {
		labels << layout.surface(face).designation();
	}

	bool chosen = false;
	const QString picked = QInputDialog::getItem(
				this,
				tr("Poser le calepinage sur le folio"),
				tr("Quelle platine dessiner sur ce folio ?"),
				labels, 0, false, &chosen);
	if (!chosen) {
		return;
	}

	const int index = labels.indexOf(picked);
	if (index < 0) {
		return;
	}

	const MountingSurface face = layout.surface(faces.at(index));

		//What the plate would measure on the sheet at the value the box
		//opens on, said out loud rather than left to be discovered after
		//the drawing overruns the folio. The number is read from the item
		//and never written here twice: a sentence quoting a default that
		//has since changed is worse than no sentence.
		//
		//The box opens on the largest step at which this plate fits on
		//this folio, and not on a constant. A constant is right for one
		//plate size and shrinks every other one for nothing, and a plate
		//shrunk for nothing prints labels too small to read - measured
		//on 15/09/2026, a 4 mm label prints about 0.8 mm on A3 at 0.5
		//and about 1.9 mm at 1.2.
	const QRectF folio_area =
			diagram_->border_and_titleblock.insideBorderRect();
	const qreal proposed =
			face.area.isValid()
			? MountingLayoutViewItem::scaleThatFits(face.area.width,
								face.area.height,
								folio_area)
			: MountingLayoutViewItem::defaultDrawingScale();
	const QString scale_hint =
			face.area.isValid()
			? tr("Unités de folio par millimètre. À %1, une platine "
			     "de %2 × %3 mm est dessinée %4 × %5.")
			  .arg(QString::number(proposed, 'g', 4),
			       QString::number(face.area.width, 'g', 6),
			       QString::number(face.area.height, 'g', 6),
			       QString::number(face.area.width * proposed, 'g', 6),
			       QString::number(face.area.height * proposed, 'g', 6))
			: tr("Unités de folio par millimètre. Cette platine n'est "
			     "pas mesurée : seuls ses composants seront dessinés.");

	bool accepted = false;
	const double scale_ = QInputDialog::getDouble(
				this,
				tr("Échelle du calepinage"),
				scale_hint,
				proposed,
				MountingLayoutViewItem::minimumDrawingScale(),
				MountingLayoutViewItem::maximumDrawingScale(),
				4,
				&accepted);
	if (!accepted) {
		return;
	}

	auto *view_item = new MountingLayoutViewItem();
	view_item->setProject(project);
	view_item->setSurfaceUuid(face.uuid);
	view_item->setDrawingScale(qreal(scale_));

		//Undoable, because posing a plate on a folio is a change to the
		//folio like any other. The command owns the item until it is
		//undone away, which is what the generic one already does for
		//every other object added to a folio.
	diagram_->undoStack().push(
				new AddGraphicsObjectCommand(view_item,
							     diagram_,
							     QPointF(50, 50)));
}

/**
	@brief QETDiagramEditor::goToElement
	@param element
	Bring forward the folio the component is drawn on, then select it and
	scroll to it. Three windows away from where the person clicked, so it is
	the editor that does it: a report that knew how to switch folios would
	be a report that knows what a project view is.
*/
void QETDiagramEditor::goToElement(Element *element)
{
	if (!element) {
		return;
	}

	Diagram *diagram = element->diagram();
	if (!diagram) {
		return;
	}

	ProjectView *project_view = findProject(diagram);
	if (!project_view) {
		return;
	}

	activateProject(project_view);
	project_view->showDiagram(diagram);

	diagram->clearSelection();
	element->setSelected(true);
	element->ensureVisible();
}

/**
	@brief QETDiagramEditor::navigationTargets
	@param diagram the folio whose selection is being asked about
	@return the components a Navigate would take the reader to

	Written once, read twice: the command uses it to know where to go, and
	DiagramView::contextMenuActions() uses it - through the enabled state of
	the action - to know whether to offer the entry at all. Two ways of
	answering "is there anywhere to go from here" would drift apart, and the
	drift shows up as a menu entry that does nothing when clicked.

	Nothing is chosen here. A component that is linked to three others
	answers with three, and it is the caller that decides what to do with a
	list of more than one.
*/
QList<Element *> QETDiagramEditor::navigationTargets(Diagram *diagram)
{
	QList<Element *> targets;
	if (!diagram) {
		return targets;
	}

	DiagramContent content(diagram);
	QList<Element *> asked = content.m_elements;

		//A label is a legitimate place to ask from, and the usual one:
		//clicking a cross reference selects the text, never the component
		//drawn under it. Both kinds are taken, because a label of a component
		//is either a text of its own or a member of a group of them.
	const QSet<DynamicElementTextItem *> texts = content.m_element_texts;
	for (DynamicElementTextItem *text : texts)
	{
		if (Element *owner = text->parentElement()) {
			asked << owner;
		}
	}

	const QSet<ElementTextItemGroup *> groups = content.m_texts_groups;
	for (ElementTextItemGroup *group : groups)
	{
		if (Element *owner = group->parentElement()) {
			asked << owner;
		}
	}

	for (Element *element : std::as_const(asked))
	{
			//Sorted by position, and the order matters: it is what makes the
			//answer the same on two runs of the same project.
		const QList<Element *> linked = element->linkedElements();
		for (Element *target : linked)
		{
			if (target && target != element && !targets.contains(target)) {
				targets << target;
			}
		}
	}

	return targets;
}

/**
	@brief QETDiagramEditor::navigateToReference
	Go to the other representation of what is selected.

	One destination goes straight there; several ask. Silently picking one of
	several would be a reference that points somewhere the reader did not
	choose, which is the one thing this task exists to make impossible - but
	refusing to move at all was not better, and that is what this used to do:
	a coil answered by three contacts is the ordinary shape of a real project,
	not an exception, so the command was greyed out on exactly the drawings it
	was written for.

	The count is asked again here rather than trusted from the enabled state,
	because a shortcut fires whatever the menu happens to be showing.
*/
void QETDiagramEditor::navigateToReference()
{
	DiagramView *dv = currentDiagramView();
	if (!dv) {
		return;
	}

	const QList<Element *> targets = navigationTargets(dv->diagram());
	if (targets.isEmpty()) {
		return;
	}

		//The other end of a folio report is shown as a double click on the
		//arrow shows it (Element::jumpTo()): the whole folio, the arrow
		//shaded for a moment and the pointer on it - one framing for one
		//destination, whichever gesture asked (decision of 20/10/2026).
		//Any other destination is zoomed onto, as before.
	const auto show = [](Element *target) {
		if (target->linkType() & Element::AllReport) {
			Element::jumpTo(target);
		} else {
			QetGraphicsItem::showItem(target);
		}
	};

	if (targets.count() == 1) {
		show(targets.first());
		return;
	}

	NavigateChoiceDialog dialog(targets, this);
	if (dialog.exec() != QDialog::Accepted) {
			//Escape leaves the reader where they were, folio and selection
			//untouched: nothing was shown yet, so there is nothing to undo.
		return;
	}

	if (Element *chosen = dialog.chosenTarget()) {
		show(chosen);
	}
}

/**
	@brief QETDiagramEditor::insertNomenclature
	@param query the nomenclature query to start the dialog from
	Pose a nomenclature table on the folio in front of the person, opened on
	a query somebody else has already narrowed down. It is the ordinary
	dialog, so what lands on the folio is an ordinary table: it follows the
	project, it prints, and it can still be edited afterwards.
*/
void QETDiagramEditor::insertNomenclature(const QString &query)
{
	DiagramView *diagram_view = currentDiagramView();
	if (!diagram_view || !diagram_view->diagram()) {
		return;
	}

	QetGraphicsTableFactory::createAndAddNomenclature(diagram_view->diagram(),
							  query);
}

/**
	@brief QETDiagramEditor::markIoBus
	Say that the selected elements are the supply bar or the return bar of
	their folio, which is what the batch wiring of the communs aims at.
*/
void QETDiagramEditor::markIoBus()
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
		QET::QetMessageBox::information(this, tr("Aucun élément sélectionné"),
						tr("Sélectionnez le ou les éléments qui font "
						   "office de barre sur leur folio."));
		return;
	}

	QStringList choices;
	choices << IoCommon::busName(IoCommon::SupplyBus)
		<< IoCommon::busName(IoCommon::ReturnBus)
		<< tr("Aucune");

		//A folio drawn to IEC repeats the same rail from folio to folio,
		//so the mark is put folio by folio, and taking it back off has to
		//be as easy as putting it on : that is the third choice.
	int start = 2;
	const IoCommon::BusKind already = IoWiring::busOf(selected.first());
	if (already == IoCommon::SupplyBus) {
		start = 0;
	} else if (already == IoCommon::ReturnBus) {
		start = 1;
	}

	bool chosen = false;
	const QString picked = QInputDialog::getItem(this,
			tr("Barre par défaut"),
			tr("Ces éléments servent de barre pour :"),
			choices, start, false, &chosen);
	if (!chosen) {
		return;
	}

	IoCommon::BusKind kind = IoCommon::NoBus;
	if (picked == choices.at(0)) {
		kind = IoCommon::SupplyBus;
	} else if (picked == choices.at(1)) {
		kind = IoCommon::ReturnBus;
	}

		//One command for the whole selection : marking the two rails of a
		//folio is one gesture on the designer's side, so it has to be
		//one step on the stack too.
	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> changes;
	for (Element *element : selected)
	{
		const DiagramContext before = element->elementInformations();
		DiagramContext after = before;
		if (kind == IoCommon::NoBus) {
			after.remove(QETInformation::ELMT_PLC_BUS);
		} else {
			after.addValue(QETInformation::ELMT_PLC_BUS,
				       IoCommon::busToString(kind));
		}
		if (after != before) {
			changes.insert(element, qMakePair(before, after));
		}
	}

	if (changes.isEmpty())
	{
		statusBar()->showMessage(
					tr("Ces éléments portaient déjà cette marque."), 8000);
		return;
	}

	diagram_view->diagram()->undoStack().push(
				new ChangeElementInformationCommand(changes));

	const QString said = (kind == IoCommon::NoBus)
			? tr("%1 élément(s) ne servent plus de barre.")
					.arg(int(changes.count()))
			: tr("%1 élément(s) marqués comme barre « %2 ».")
					.arg(int(changes.count()))
					.arg(IoCommon::busName(kind));
	statusBar()->showMessage(said, 8000);
}

/**
	@brief QETDiagramEditor::wireIoCommons
	Draw in one go the wires between the communs of the cards and the bars
	marked on their folio (CU-11.8).
*/
void QETDiagramEditor::wireIoCommons()
{
	QETProject *project = currentProject();
	if (!project) {
		return;
	}

		//What was pointed at is what was meant. Nothing pointed at means
		//the whole project, and there « a card » has to mean something :
		//the same set the assignment window offers, so that what could
		//not be done stays a short list about cards.
	QList<Element *> cards;
	DiagramView *diagram_view = currentDiagramView();
	if (diagram_view && diagram_view->diagram())
	{
		const QList<QGraphicsItem *> items =
				diagram_view->diagram()->selectedItems();
		for (QGraphicsItem *item : items)
		{
			if (Element *element = qgraphicsitem_cast<Element *>(item)) {
				cards.append(element);
			}
		}
	}

	if (cards.isEmpty())
	{
		ElementProvider provider(project);
		const QVector<QPointer<Element>> masters =
				provider.find(ElementData::Master);
		for (Element *element : masters)
		{
			if (!element) {
				continue;
			}
			if (element->elementData().m_master_type != ElementData::PLC) {
				continue;
			}
			if (element->elementData().plcMasterData().ios.isEmpty()) {
				continue;
			}
			cards.append(element);
		}
	}

	if (cards.isEmpty())
	{
		QET::QetMessageBox::information(this, tr("Aucune carte"),
						tr("Ce projet ne contient aucune carte d'automate. "
						   "Sélectionnez les éléments dont les communs sont "
						   "à raccorder."));
		return;
	}

	IoWiring wiring(project);
	QStringList problems;
	const IoCommon::Plan plan = wiring.plan(cards, &problems);

	QString said = plan.text();
	if (!problems.isEmpty()) {
		said += QLatin1Char('\n') + problems.join(QLatin1Char('\n'));
	}

		//Nothing to draw is not a failure, and the reasons are the whole
		//answer : they are shown, rather than a silent no.
	if (plan.isEmpty())
	{
		QET::QetMessageBox::information(this, tr("Raccorder les communs"),
						said);
		return;
	}

	if (QET::QetMessageBox::question(this, tr("Raccorder les communs"), said,
					 QMessageBox::Ok | QMessageBox::Cancel,
					 QMessageBox::Ok) != QMessageBox::Ok) {
		return;
	}

	const IoWiring::Report report = wiring.wire(plan);
	QString done = report.text();
	done.replace(QLatin1Char('\n'), QStringLiteral(" "));
	statusBar()->showMessage(done, 8000);
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
		//Where the designer is looking, not at the corner of the sheet: a
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

namespace
{
	/**
		The elements selected on @a diagram, top to bottom then left to
		right: the first one is the one whose name starts the search, and
		the same selection always gives the same first one.
	*/
	QList<Element *> selectedElementsInOrder(Diagram *diagram)
	{
		QList<Element *> elements;
		const QList<QGraphicsItem *> items = diagram->selectedItems();
		for (QGraphicsItem *item : items)
		{
			if (Element *element = qgraphicsitem_cast<Element *>(item)) {
				elements << element;
			}
		}
		std::stable_sort(elements.begin(), elements.end(),
				 [](const Element *a, const Element *b) {
			if (!qFuzzyCompare(a->pos().y(), b->pos().y())) {
				return a->pos().y() < b->pos().y();
			}
			return a->pos().x() < b->pos().x();
		});
		return elements;
	}

	/// How an element is called in a message: its label, or its name
	QString elementLabel(Element *element)
	{
		const QString label = element->actualLabel();
		return label.isEmpty() ? element->name() : label;
	}
}

/**
	@brief QETDiagramEditor::replaceSelectedElements
	Open the element picker where the mouse is, to choose the element that
	replaces the selected ones (T54). The search starts with the name of the
	first of them, so that its look-alikes are listed straight away.
*/
void QETDiagramEditor::replaceSelectedElements()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}

	const QList<Element *> elements = selectedElementsInOrder(view->diagram());
	if (elements.isEmpty()) {
		return;
	}

	elementPicker()->popUpToReplace(QCursor::pos(), elements.first()->name());
}

/**
	@brief QETDiagramEditor::replaceSelectionWith
	Put the element at @a location in the place of each selected element,
	with its wires, in one undoable step (T54). When a wire would have no
	terminal to go to, or a cross-reference link would be lost, the user is
	asked first, with the list.
	@param location : the element chosen in the picker
*/
void QETDiagramEditor::replaceSelectionWith(const ElementsLocation &location)
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}
	Diagram *diagram = view->diagram();

	const QList<Element *> elements = selectedElementsInOrder(diagram);
	if (elements.isEmpty()) {
		return;
	}

		//Into the project once, as placing the element would do: the
		//project keeps its own copy of every symbol it uses
	ElementsLocation source(location);
	const ElementsLocation in_project = diagram->project()->importElement(source);
	if (!in_project.exist())
	{
		QMessageBox::warning(this, tr("Remplacer l'élément"),
			tr("L'élément choisi n'a pas pu être ajouté au projet."));
		return;
	}
	const ElementsLocation project_location(in_project.projectCollectionPath());

	QList<SwapElementCommand::Plan> plans;
	QStringList refusals;
	for (Element *element : elements)
	{
		SwapElementCommand::Plan plan =
				SwapElementCommand::plan(element, project_location);
		if (plan.error.isEmpty()) {
			plans << plan;
		} else {
			refusals << plan.error;
		}
	}

		//The same wire can be lost on both of its ends, when both are
		//replaced: it is counted once
	QSet<Conductor *> kept;
	QSet<Conductor *> lost;
	QStringList details;
	int lost_links = 0;
	for (const SwapElementCommand::Plan &plan : std::as_const(plans))
	{
		for (Conductor *conductor : plan.moved_conductors) {
			kept.insert(conductor);
		}
		for (Conductor *conductor : plan.lost_conductors)
		{
			if (lost.contains(conductor)) {
				continue;
			}
			lost.insert(conductor);
			const QString number = WireColorName::withoutMarker(conductor->properties().text);
			details << (number.isEmpty()
				    ? tr("Fil sans numéro, sur %1")
				      .arg(elementLabel(plan.old_element))
				    : tr("Fil « %1 », sur %2")
				      .arg(number, elementLabel(plan.old_element)));
		}
		for (Element *other : plan.lost_links)
		{
			++lost_links;
			details << tr("Renvoi de %1 vers %2")
				   .arg(elementLabel(plan.old_element), elementLabel(other));
		}
	}
	kept.subtract(lost);

	if (!plans.isEmpty() && (!lost.isEmpty() || lost_links))
	{
		QStringList text;
		if (!lost.isEmpty()) {
			text << tr("%n fil(s) sans borne dans le nouvel élément : "
				   "il(s) sera(ont) supprimé(s).", "", lost.size());
		}
		if (lost_links) {
			text << tr("%n renvoi(s) sera(ont) perdu(s).", "", lost_links);
		}
		text << tr("Ctrl+Z annule le remplacement.");

		QMessageBox box(QMessageBox::Question, tr("Remplacer l'élément"),
				text.join(QStringLiteral("\n")),
				QMessageBox::NoButton, this);
		box.setDetailedText(details.join(QStringLiteral("\n")));
		QPushButton *replace = box.addButton(tr("Remplacer"),
						     QMessageBox::AcceptRole);
		box.addButton(QMessageBox::Cancel);
		box.setDefaultButton(replace);
		box.exec();
		if (box.clickedButton() != replace)
		{
			for (SwapElementCommand::Plan &plan : plans) {
				SwapElementCommand::discard(plan);
			}
			return;
		}
	}

	if (!plans.isEmpty())
	{
		QUndoCommand *command = nullptr;
		if (plans.size() == 1) {
			command = new SwapElementCommand(plans.first());
		} else {
			command = new QUndoCommand(tr("remplacer %n élément(s)",
						      "undo caption", plans.size()));
			for (const SwapElementCommand::Plan &plan : std::as_const(plans)) {
				new SwapElementCommand(plan, command);
			}
		}
		diagram->undoStack().push(command);

		statusBar()->showMessage(
			tr("%n élément(s) remplacé(s)", "", plans.size())
			+ QStringLiteral(" · ")
			+ tr("%n fil(s) gardé(s)", "", kept.size()),
			10000);
	}

	if (!refusals.isEmpty()) {
		QMessageBox::information(this, tr("Remplacer l'élément"),
					 refusals.join(QStringLiteral("\n\n")));
	}
}

/**
	@brief QETDiagramEditor::arrangeConductors
	Arrange the selected conductors, or every conductor of the folio the
	user did not drag by hand (T67), and say in the status bar how many
	changed.
*/
void QETDiagramEditor::arrangeConductors(bool whole_folio)
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}
	Diagram *diagram = view->diagram();
	const QSet<Conductor *> selected = diagram->selectedConductors();
	const int changed = WireArrangement::arrange(
				diagram, QList<Conductor *>(selected.cbegin(), selected.cend()),
				whole_folio ? WireArrangement::Scope::Folio
					    : WireArrangement::Scope::Selection);
	const QString message = changed
			? tr("%n conductor(s) arranged", "status bar message", changed)
			: tr("Nothing to arrange: the conductors are already in place", "status bar message");
		//Queued, as for the align actions: closing the menu the action came
		//from would clear a message shown right now
	QTimer::singleShot(0, this, [this, message]() {
		statusBar()->showMessage(message, 8000);
	});
}

/**
	@brief QETDiagramEditor::updateWireSections
	Give every conductor of the current project the cross-section of its
	point, the largest its components ask for (T70) - one undo step per
	folio - and say in the status bar how many changed.
*/
void QETDiagramEditor::updateWireSections()
{
	ProjectView *project_view = currentProjectView();
	QETProject *project = project_view ? project_view->project() : nullptr;
	if (!project || project->isReadOnly()) {
		return;
	}
	const int changed = WireSections::updateProject(project);
	const QString message = changed
			? tr("Cross-section updated on %n conductor(s)", "status bar message", changed)
			: tr("The conductors already have the cross-section of their components", "status bar message");
		//Queued, as for the align actions: closing the menu the action came
		//from would clear a message shown right now
	QTimer::singleShot(0, this, [this, message]() {
		statusBar()->showMessage(message, 8000);
	});
}

/**
	@brief QETDiagramEditor::convertWireNames
	Name every wire of the current project F<folio>-<number> (colour
	cross-section), each one keeping the number its text carries (T70) - one
	undo step - and say in the status bar how many changed.
*/
void QETDiagramEditor::convertWireNames()
{
	ProjectView *project_view = currentProjectView();
	QETProject *project = project_view ? project_view->project() : nullptr;
	if (!project || project->isReadOnly()) {
		return;
	}
		//The folio on screen, so that the next wire drawn on it goes on
		//from its highest number without a change of folio
	DiagramView *view = currentDiagramView();
	const int changed = WireNames::convertProject(project, view ? view->diagram() : nullptr);
	const QString message = changed
			? tr("%n wire name(s) converted", "status bar message", changed)
			: tr("The wires already carry the standard name", "status bar message");
		//Queued, as for the cross-sections: closing the menu the action
		//came from would clear a message shown right now
	QTimer::singleShot(0, this, [this, message]() {
		statusBar()->showMessage(message, 8000);
	});
}

/**
	@brief QETDiagramEditor::renumberWires
	Ask for the scope and the reading order, and number the wires again
	from their place on the folio (T78) - one undo step - saying in the
	status bar how many changed. The reading order is the setting the
	renumbering of components keeps, so that both read a folio alike.
*/
void QETDiagramEditor::renumberWires()
{
	ProjectView *project_view = currentProjectView();
	QETProject *project = project_view ? project_view->project() : nullptr;
	if (!project || project->isReadOnly()) {
		return;
	}
	DiagramView *view = currentDiagramView();
	Diagram *shown = view ? view->diagram() : nullptr;
	const QString order_key = QStringLiteral("renumbering/columns-first");

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Renumber the wires"));
	auto *scope = new QComboBox(&dialog);
	scope->addItem(tr("Whole project"), true);
	if (shown) {
		scope->addItem(tr("This folio only"), false);
	}
	auto *order = new QComboBox(&dialog);
	order->addItem(tr("Top to bottom, then left to right"), false);
	order->addItem(tr("Left to right, then top to bottom"), true);
	QSettings settings;
	order->setCurrentIndex(settings.value(order_key, false).toBool() ? 1 : 0);
	auto *note = new QLabel(tr("Every wire gets a new number from its place, from 1 on each folio. "
				   "A wire left out of automatic numbering loses the name the numbering gave it, "
				   "and keeps a name typed by hand; those of a panel marked as built keep theirs."),
				&dialog);
	note->setWordWrap(true);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Renumber"));
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	auto *form = new QFormLayout(&dialog);
	form->addRow(tr("Scope"), scope);
	form->addRow(tr("Reading order"), order);
	form->addRow(note);
	form->addRow(buttons);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	const bool columns_first = order->currentData().toBool();
	settings.setValue(order_key, columns_first);
	const QList<Diagram *> folios = scope->currentData().toBool()
			? project->diagrams() : QList<Diagram *>{shown};
	const int changed = WireNames::renumberProject(project, folios, columns_first, shown);
	const QString message = changed
			? tr("%n wire(s) renumbered", "status bar message", changed)
			: tr("The wires are already numbered in this order", "status bar message");
	QTimer::singleShot(0, this, [this, message]() {
		statusBar()->showMessage(message, 8000);
	});
}

/**
	@brief QETDiagramEditor::removeWires
	Ask for the scope, say how many wires go and how many stay before any
	goes, and remove them in one undo step (T79), saying in the status bar
	how many went.
*/
void QETDiagramEditor::removeWires()
{
	ProjectView *project_view = currentProjectView();
	QETProject *project = project_view ? project_view->project() : nullptr;
	if (!project || project->isReadOnly()) {
		return;
	}
	DiagramView *view = currentDiagramView();
	Diagram *shown = view ? view->diagram() : nullptr;

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Remove the wires"));
	auto *scope = new QComboBox(&dialog);
	scope->addItem(tr("Whole project"), true);
	if (shown) {
		scope->addItem(tr("This folio only"), false);
	}
	auto *note = new QLabel(&dialog);
	note->setWordWrap(true);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Remove"));
	const auto folios = [project, shown, scope]() {
		return scope->currentData().toBool() ? project->diagrams() : QList<Diagram *>{shown};
	};
	const auto update = [folios, note, buttons]() {
		const WireRemoval::Count count = WireRemoval::count(folios());
		note->setText(tr("%1 wire(s) will be removed. Kept: %2 left out of automatic numbering, "
				 "%3 of a panel marked as built. One undo puts them all back.")
			      .arg(count.removed).arg(count.left_out).arg(count.frozen));
		buttons->button(QDialogButtonBox::Ok)->setEnabled(count.removed > 0);
	};
	connect(scope, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, update);
	update();
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	auto *form = new QFormLayout(&dialog);
	form->addRow(tr("Scope"), scope);
	form->addRow(note);
	form->addRow(buttons);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	const WireRemoval::Count count = WireRemoval::removeWires(project, folios());
	const QString message = tr("%n wire(s) removed", "status bar message", count.removed);
	QTimer::singleShot(0, this, [this, message]() {
		statusBar()->showMessage(message, 8000);
	});
}

/**
	@brief QETDiagramEditor::leaveOutSelectedWires
	Leave the selected wires out of the automatic numbering, or bring them
	back, in one undo step: the step the box of the wire panel pushes (T77).
	The other things selected are not touched.
	@param ignored : the state the entry was clicked to
*/
void QETDiagramEditor::leaveOutSelectedWires(bool ignored)
{
	DiagramView *view = currentDiagramView();
	Diagram *diagram = view ? view->diagram() : nullptr;
	if (!diagram || diagram->isReadOnly()) {
		return;
	}
	const QSet<Conductor *> selected = diagram->selectedConductors();
	if (QUndoCommand *undo = ConductorPropertiesEditorWidget::leaveOutCommand(
			QList<Conductor *>(selected.cbegin(), selected.cend()), ignored)) {
		diagram->undoStack().push(undo);
	}
	updateLeaveOutWires();
}

/**
	@brief QETDiagramEditor::updateLeaveOutWires
	The entry that leaves the selected wires out of the numbering: usable
	when wires are selected on a folio that can be changed, and ticked when
	every one of them is left out already (T77).
*/
void QETDiagramEditor::updateLeaveOutWires()
{
	DiagramView *view = currentDiagramView();
	Diagram *diagram = view ? view->diagram() : nullptr;
	const QSet<Conductor *> selected = diagram ? diagram->selectedConductors()
						   : QSet<Conductor *>();
	m_leave_out_wires->setEnabled(diagram && !diagram->isReadOnly() && !selected.isEmpty());
	m_leave_out_wires->setChecked(!selected.isEmpty()
				      && std::all_of(selected.cbegin(), selected.cend(),
						     [](const Conductor *c) {return c->isFreezeLabel();}));
}

/**
	@brief QETDiagramEditor::exchangeSelectedElements
	Put each of the two selected elements in the place of the other, in one
	undoable step (T66). The wires stay where they are and each element takes
	those of its new place; when a wire would end on no terminal, the user is
	asked first, with the list. With other than two elements selected the
	status bar says so.
*/
void QETDiagramEditor::exchangeSelectedElements()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}
	Diagram *diagram = view->diagram();

	auto say = [this](const QString &message) {
			//Queued, as for the align actions: closing the menu the action
			//came from would clear a message shown right now
		QTimer::singleShot(0, this, [this, message]() {
			statusBar()->showMessage(message, 8000);
		});
	};

	const QList<Element *> elements = selectedElementsInOrder(diagram);
	if (elements.size() != 2) {
		say(tr("Select two elements to swap them (%n selected)", "", elements.size()));
		return;
	}

	ExchangeElementsCommand::Plan plan =
			ExchangeElementsCommand::plan(elements.at(0), elements.at(1));
	if (!plan.error.isEmpty()) {
		say(plan.error);
		return;
	}

	if (!plan.lost_conductors.isEmpty())
	{
		QStringList details;
		for (Conductor *conductor : std::as_const(plan.lost_conductors))
		{
			const QString number = WireColorName::withoutMarker(conductor->properties().text);
			details << (number.isEmpty() ? tr("Wire without a number")
						     : tr("Wire « %1 »").arg(number));
		}
		QMessageBox box(QMessageBox::Question, tr("Swap elements"),
				tr("%n wire(s) would end on no terminal of the other element, "
				   "and will be deleted.", "", plan.lost_conductors.size())
				+ QStringLiteral("\n") + tr("Ctrl+Z undoes the swap."),
				QMessageBox::NoButton, this);
		box.setDetailedText(details.join(QStringLiteral("\n")));
		QPushButton *swap = box.addButton(tr("Swap"), QMessageBox::AcceptRole);
		box.addButton(QMessageBox::Cancel);
		box.setDefaultButton(swap);
		box.exec();
		if (box.clickedButton() != swap) {
			return;
		}
	}

	diagram->undoStack().push(new ExchangeElementsCommand(plan));
	say(tr("%1 and %2 swapped", "the labels of the two elements")
		.arg(elementLabel(elements.at(0)), elementLabel(elements.at(1)))
	    + QStringLiteral(" · ")
	    + tr("%n wire(s) handed over", "", plan.moved_conductors.size()));
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
		//alignment moves everything the least it can: the designer who put
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

	QMenu* menu_fichier	  = new QMenu(tr("&File"), this);
	QMenu* menu_edition	  = new QMenu(tr("&Edit"), this);
	QMenu* menu_project	  = new QMenu(tr("&Project"), this);
		//Numbering of everything the project numbers, kept together
		//and sitting right behind the project it belongs to
	QMenu* menu_numbering = new QMenu(tr("Numbering"), this);
	QMenu* menu_lists     = new QMenu(tr("Lists"), this);
	QMenu* menu_catalogue = new QMenu(tr("&Catalogue"), this);
	QMenu* menu_affichage = new QMenu(tr("Displ&ay"), this);
	// QMenu *menu_outils    = new QMenu(tr("O&utils"), this);
	windows_menu = new QMenu(tr("Wi&ndows"), this);

	insertMenu(settings_menu_, menu_fichier);
		//Short label in the menu, full text in the tooltip. Without this Qt shows
		//no tooltip on a menu item, and shortening the label alone would hide the
		//information instead of moving it elsewhere.
	for (QMenu *menu : {menu_fichier, menu_edition, menu_project,
			    menu_catalogue, menu_affichage}) {
		menu->setToolTipsVisible(true);
	}

	insertMenu(settings_menu_, menu_edition);
	insertMenu(settings_menu_, menu_project);
	insertMenu(settings_menu_, menu_numbering);
	insertMenu(settings_menu_, menu_lists);
	insertMenu(settings_menu_, menu_catalogue);
	insertMenu(settings_menu_, menu_affichage);
	insertMenu(help_menu_, windows_menu);

	// File menu
		// Add the RecentFiles menu itself, not a copy of the actions it
		// happened to hold at construction time. RecentFiles::buildMenu()
		// clears that menu and creates fresh QActions every time a file is
		// opened, so a snapshot taken here never gained an entry again and
		// the list only looked right after a restart (bugtracker #97).
		//
		// The menu is an application-wide singleton and several editor
		// windows may exist. QMenu::addMenu() adds the submenu's menuAction()
		// rather than reparenting it, so every window shows the same live
		// menu, which is the intent.
	QMenu *recentfile = QETApp::projectsRecentFiles()->menu();
	recentfile->setTitle(tr("&Latest files"));
	recentfile->setIcon(QET::Icons::DocumentOpenRecent);
	menu_fichier->addMenu(recentfile);
	connect(QETApp::projectsRecentFiles(), &RecentFiles::fileOpeningRequested, this, &QETDiagramEditor::openRecentFile);
	const QList<QAction *> file_actions = m_file_actions_group.actions();
	menu_fichier -> addActions(file_actions);
		//Right under "New", which is where somebody starting a project
		//looks. Inserted rather than appended because the actions above come
		//in as a group, and "New from a template" belongs beside the one it
		//is an alternative to, not after "Close".
	if (m_new_from_template)
	{
		if (file_actions.size() > 1) {
			menu_fichier -> insertAction(file_actions.at(1), m_new_from_template);
		} else {
			menu_fichier -> addAction(m_new_from_template);
		}
	}
	menu_fichier -> addSeparator();
	//menu_fichier -> addAction(import_diagram);
	menu_fichier -> addAction(m_export_to_images);
	menu_fichier -> addAction(m_export_to_pdf);
	menu_fichier -> addAction(m_print);
	menu_fichier -> addSeparator();
	menu_fichier -> addAction(m_quit_editor);

	// menu Edition
		//What is used all day stays in the first column, and the families
		//used less often sit one level down, each under its subject: the
		//commands added since issue #1336 had made the menu spill into a
		//second column. Every action keeps its shortcut, and the command
		//search, the gestures and the toolbars read the shortcut register,
		//not this menu.
	menu_edition -> addAction(undo);
	menu_edition -> addAction(redo);
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_cut);
	menu_edition -> addAction(m_copy);
	menu_edition -> addAction(m_paste);
	menu_edition -> addAction(m_duplicate);
	QMenu *paste_menu = menu_edition -> addMenu(QET::Icons::EditPaste, tr("Collage spécial"));
	paste_menu -> addAction(m_paste_origin);
	paste_menu -> addAction(m_paste_element_info);
	paste_menu -> addSeparator();
	paste_menu -> addAction(m_copy_text_format);
	paste_menu -> addAction(m_apply_text_format);
	paste_menu -> addSeparator();
	paste_menu -> addAction(m_configure_duplicate);
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_show_element_picker);
	menu_edition -> addAction(m_insert_last_element);
		//The same actions the "Ajouter" toolbar holds. They were toolbar-only,
		//which left them unreachable for anyone working without a mouse: a
		//toolbar button has no key, so text fields, images and every drawing
		//shape simply could not be added. m_depth_action_group below has
		//always been in both places; this brings these into line with it.
	m_add_item_menu = menu_edition -> addMenu(tr("&Add"));
	m_add_item_menu -> setIcon(QET::Icons::Add);
	m_add_item_menu -> addActions(m_add_item_actions_group.actions());
	menu_edition -> addSeparator();
		//The menu had grown to over forty entries and no longer fitted on a
		//laptop screen (issue #1336). Whole families now sit one level down,
		//as Ajouter and Aligner already did; every action and its shortcut
		//is unchanged.
	QMenu *select_menu = menu_edition -> addMenu(QET::Icons::EditSelectAll, tr("Selection"));
	select_menu -> addActions(m_select_actions_group.actions());
	menu_edition -> addAction(m_delete_selection);
	menu_edition -> addAction(m_rotate_selection);
	menu_edition -> addAction(m_edit_selection);
	m_align_menu = menu_edition -> addMenu(tr("Align"));
		//Snap to grid, then the horizontal three, then the vertical three,
		//in the order setUpActions() adds them
	m_align_menu -> addAction(m_align_actions_group.actions().first());
	m_align_menu -> addSeparator();
	m_align_menu -> addActions(m_align_actions_group.actions().mid(1, 3));
	m_align_menu -> addSeparator();
	m_align_menu -> addActions(m_align_actions_group.actions().mid(4, 3));
	m_align_menu -> addSeparator();
		//And the two distributions
	m_align_menu -> addActions(m_align_actions_group.actions().mid(7));
	QMenu *arrange_menu = menu_edition -> addMenu(tr("Organiser"));
	arrange_menu -> addAction(m_rotate_group_selection);
	arrange_menu -> addAction(m_rotate_texts);
	arrange_menu -> addAction(m_place_texts);
	arrange_menu -> addAction(m_mirror_horizontal);
	arrange_menu -> addAction(m_mirror_vertical);
	arrange_menu -> addAction(m_element_scale);
	arrange_menu -> addAction(m_strip_frame);
	arrange_menu -> addSeparator();
	QMenu *group_menu = arrange_menu -> addMenu(QET::Icons::textGroup, tr("Group", "fr: Grouper"));
	group_menu -> addAction(m_group_selection);
	group_menu -> addAction(m_ungroup_selection);
	group_menu -> addSeparator();
	group_menu -> addAction(m_group_selected_texts);
	QMenu *depth_menu = arrange_menu -> addMenu(QET::Icons::BringForward, tr("Depth"));
	depth_menu -> addActions(m_depth_action_group->actions());
	menu_edition -> addSeparator();
	QMenu *conductor_menu = menu_edition -> addMenu(tr("Conducteurs"));
	conductor_menu -> addAction(m_conductor_reset);
	conductor_menu -> addAction(m_conductor_arrange);
	conductor_menu -> addAction(m_conductor_arrange_folio);
	conductor_menu -> addAction(m_update_wire_sections);
	conductor_menu -> addAction(m_convert_wire_names);
	conductor_menu -> addAction(m_renumber_wires);
	conductor_menu -> addAction(m_leave_out_wires);
	conductor_menu -> addAction(m_remove_wires);
	conductor_menu -> addSeparator();
	conductor_menu -> addAction(m_show_conductor_text);
	conductor_menu -> addAction(m_hide_conductor_text);
	conductor_menu -> addAction(m_align_conductor_text);
	menu_edition -> addAction(m_edit_diagram_properties);
		//Shared with the folio's context menu (see
		//DiagramView::contextMenuActions()).
	m_row_column_menu = menu_edition -> addMenu(tr("Rows and columns"));
	m_row_column_menu -> setIcon(QET::Icons::EditTableInsertColumnRight);
	m_row_column_menu -> addActions(m_row_column_actions_group.actions());
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_find);
	menu_edition -> addAction(m_jump_to_element);
	menu_edition -> addAction(m_navigate);
	menu_edition -> addAction(m_folio_back);
	menu_edition -> addAction(m_folio_forward);
	menu_edition -> addSeparator();
	QMenu *symbol_menu = menu_edition -> addMenu(tr("Symboles et circuits"));
	symbol_menu -> addAction(m_create_symbol);
	symbol_menu -> addAction(m_explode_element);
	symbol_menu -> addAction(m_replace_element);
	symbol_menu -> addAction(m_exchange_elements);
	symbol_menu -> addAction(m_reset_symbol_texts);
	symbol_menu -> addSeparator();
	symbol_menu -> addAction(m_generate_pinout);
	symbol_menu -> addAction(m_generate_circuits);
	symbol_menu -> addSeparator();
	symbol_menu -> addAction(m_save_group);
	symbol_menu -> addAction(m_insert_group);
	QMenu *io_menu = menu_edition -> addMenu(tr("Entrées et sorties"));
	io_menu -> addAction(m_import_io_sheet);
	io_menu -> addAction(m_assign_io_points);
	io_menu -> addAction(m_io_list);
	io_menu -> addAction(m_mark_io_bus);
	io_menu -> addAction(m_wire_io_commons);
	menu_edition -> addSeparator();
	menu_edition -> addAction(m_command_search);
	menu_edition -> addAction(m_repeat_last_command);
	menu_edition -> addAction(m_show_shortcut_bar);
	menu_edition -> addAction(m_find_element);

	// menu Projet
	menu_project -> addAction(m_project_edit_properties);
	menu_project -> addAction(m_auto_conductor);
		//Sits beside m_auto_conductor, the setting it pairs with. It was
		//toolbar-only and so had no keyboard route at all.
	menu_project -> addAction(m_auto_break_conductor);
	menu_project -> addSeparator();
	menu_project -> addAction(m_project_add_diagram);
	menu_project -> addAction(m_remove_diagram_from_project);
	menu_project -> addAction(m_clean_project);
	menu_project -> addSeparator();
	menu_project -> addAction(m_csv_export);
	menu_project -> addAction(m_export_cable_list);
	menu_project -> addAction(m_project_export_conductor_num);
	menu_project -> addAction(m_project_export_wiring_list);
	menu_project -> addAction(m_project_wiring_list_view);
	menu_project -> addAction(m_renumber_components);
	menu_project -> addAction(m_iec_structure);
	menu_project -> addAction(m_assembly_state);
	menu_project -> addAction(m_location_manager);
	menu_project -> addAction(m_location_report);
	menu_project -> addAction(m_location_bom);
	menu_project -> addAction(m_mounting_layout);
	menu_project -> addAction(m_put_mounting_layout);
	menu_project -> addAction(m_replace_part);
	menu_project -> addAction(m_drc_check);
	menu_project -> addAction(m_reload_element_drawings);
	menu_project -> addAction(m_whole_terminals);
#ifdef QET_HAS_SCRIPTING
	m_scripts_menu = menu_project -> addMenu(tr("Scripts"));
	rebuildScriptActions();
	connect(&ScriptLibrary::instance(), &ScriptLibrary::changed,
		this, &QETDiagramEditor::rebuildScriptActions);
	AssistantInfo::watch();
#endif
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
	menu_catalogue -> addAction(m_catalog_no_physical_view);
	menu_catalogue -> addAction(m_connector_check);
	menu_catalogue -> addSeparator();
	menu_catalogue -> addAction(m_catalog_import);
	menu_catalogue -> addAction(m_catalog_repository);
	menu_catalogue -> addSeparator();
	menu_catalogue -> addAction(m_catalog_manager);
	menu_catalogue -> addSeparator();
	menu_catalogue -> addAction(m_environment);

	// menu Numbering
		//Everything the project numbers sits here together: the
		//terminals have come out of the project menu, the cables are
		//the new one
	menu_numbering -> addAction(m_terminal_numbering);
	menu_numbering -> addAction(m_cable_numbering);

	// menu Listes
		//Everything which drops a table or a list onto the sheets has
		//come out of the project menu and sits here together: the
		//summary, the material list, the terminal strip tools and the
		//cable list. The CSV exports stay with the other exports in the
		//project menu, the cable list's among them.
	menu_lists -> addAction(m_add_summary);
	menu_lists -> addAction(m_add_nomenclature);
	menu_lists -> addSeparator();
	menu_lists -> addAction(m_terminal_strip_dialog);
	menu_lists -> addAction(m_project_terminalBloc);
	menu_lists -> addSeparator();
	menu_lists -> addAction(m_add_cable_list);

	main_tool_bar         -> toggleViewAction() -> setStatusTip(tr("Display or hide the main toolbar"));
	view_tool_bar         -> toggleViewAction() -> setStatusTip(tr("Display or hide the Display toolbar"));
	diagram_tool_bar      -> toggleViewAction() -> setStatusTip(tr("Display or hide the Diagram toolbar"));
	qdw_pa           -> toggleViewAction() -> setStatusTip(tr("Show or hide the elements panel"));
	qdw_undo         -> toggleViewAction() -> setStatusTip(tr("Display or hide the undo list"));


	// menu Affichage
	QMenu *projects_view_mode = menu_affichage -> addMenu(QET::Icons::ConfigureToolbars, tr("Display projects"));
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
	menu_affichage -> addAction(m_crosshair);
	menu_affichage -> addMenu(m_text_grid_menu);
	menu_affichage -> addAction(m_draw_guides);
	menu_affichage -> addAction(m_cell_rulers);
	menu_affichage -> addAction(m_cell_lines);
	menu_affichage -> addMenu(m_shown_kinds_menu);
	menu_affichage -> addMenu(m_background_color_button->menu());
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
			//A project given on the command line is still being read at
			//this point: the waiting dialog pumps the event loop while it
			//loads, which is how this activation reaches us in the middle
			//of the load. The gate holds the collection back until that
			//read is over, and answers false here; with no project being
			//read it answers true and nothing changes.
		if (m_collection_load_gate.windowActivated()) {
			startElementsCollectionLoad();
		}
	}
	return(QETMainWindow::event(e));
}

/**
	@brief QETDiagramEditor::startElementsCollectionLoad
	Starts the load of the elements collection, shortly.

	The delay is the one this has always had: the window is given a moment to
	finish showing itself before thousands of element files are parsed on
	every core. What is new is *when* this is called - see CollectionLoadGate,
	which owns that decision and lets it through exactly once.
*/
void QETDiagramEditor::startElementsCollectionLoad()
{
	QTimer::singleShot(250, m_element_collection_widget, &ElementsCollectionWidget::reload);
}

/**
	@brief QETDiagramEditor::save
	Ask the current active project to save
*/
void QETDiagramEditor::save()
{
	if (ProjectView *project_view = currentProjectView()) {
		QETResult saved = project_view -> save();

		if (saved.isOk()) {
			//save_file -> setDisabled(true);
			QETApp::projectsRecentFiles() -> fileWasOpened(project_view -> project() -> filePath());

			QString title = (project_view -> project() -> title ());
			if (title.isEmpty()) title = "QElectroTech ";
			QString filePath = (project_view -> project() -> filePath ());
			statusBar()-> showMessage(tr("Project %1 registered in the directory: %2.").arg(title).arg (filePath), 2000);
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
			statusBar()->showMessage(tr("Project %1 registered in the directory: %2.").arg(title).arg (filePath), 2000);
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
	@brief QETDiagramEditor::newProjectFromTemplate
	Create a new project out of a template project file.

	The template is a whole .qet: the cover sheet, the index and the folios
	the house always starts from, already framed and with their title blocks
	filled in. It is read and never written - the project that comes out of
	it carries no file path at all, so the first "Save" asks where to put it
	instead of writing over the template.
	@return true when a project was created
*/
bool QETDiagramEditor::newProjectFromTemplate()
{
	const QString filepath = QFileDialog::getOpenFileName(
				this,
				tr("Nouveau projet à partir d'un modèle", "dialog title"),
				ProjectTemplate::templatesDir(),
				ProjectTemplate::nameFilter());
	if (filepath.isEmpty()) {
		return(false);
	}

		//Same pairing as openAndAddProject(): reading a template is reading
		//a project, so it gets the waiting dialog, and the elements
		//collection is held back from walking the same cores meanwhile.
	m_collection_load_gate.projectLoadStarted();
	DialogWaiting::instance(this);

	QString error;
	QETProject *project = ProjectTemplate::openAsNewProject(filepath, this, &error);
	if (project) {
		addProject(project);
	}

	DialogWaiting::dropInstance();
	if (m_collection_load_gate.projectLoadFinished()) {
		startElementsCollectionLoad();
	}

	if (!project)
	{
		QET::QetMessageBox::critical(
					this,
					tr("Impossible d'ouvrir le modèle", "message box title"),
					error);
		return(false);
	}

		//The template is deliberately not added to the recently opened
		//files: what is offered there is opened for editing, and this one is
		//never to be edited by accident.
	statusBar()->showMessage(
				tr("Nouveau projet créé à partir du modèle %1. "
				   "Il n'a pas encore de fichier : « Enregistrer » demandera où le ranger.")
				.arg(QFileInfo(filepath).fileName()),
				10000);
	return(true);
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
		tr("Open a file"),
		open_dialog_dir.absolutePath(),
		QETProject::openFileFilter() + ";;" + tr("XML Files (*.xml);;All files (*)")
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
	@brief QETDiagramEditor::askAboutLock
	Tell the user who has the project @p filepath open and let them choose.
	Read-only is the default: opening anyway can lose the other person's
	work, and a lock left by a crash on another machine cannot be told from
	a live one, which is why the age of the last sign of life is shown.
*/
QETDiagramEditor::LockChoice QETDiagramEditor::askAboutLock(
		const QString &filepath,
		const ProjectLock::Holder &holder)
{
	const QString name = QFileInfo(filepath).fileName();
	QString text;
	if (holder.session == QLatin1String("?")) {
		text = tr("%1 is being opened by another session of QElectroTech.").arg(name);
	} else {
		const QString since = QLocale::system().toString(holder.since.toLocalTime(), QLocale::ShortFormat);
		text = tr("%1 is open by %2 on %3 since %4").arg(name, holder.user, holder.host, since);
		const qint64 minutes = holder.heartbeat.isValid()
				? holder.heartbeat.secsTo(QDateTime::currentDateTimeUtc()) / 60
				: -1;
		if (minutes >= 0) {
			text += minutes < 1
					? tr(" (active just now).")
					: tr(" (active %n minute(s) ago).", nullptr, static_cast<int>(minutes));
		} else {
			text += QLatin1Char('.');
		}
	}

	QMessageBox box(QMessageBox::Warning, tr("Project in use", "message box title"), text, QMessageBox::NoButton, this);
	box.setInformativeText(tr("Open it read-only to look at it without changing it. "
							  "Open it anyway only if you know that session is not "
							  "using it any more: both of you could then save over each other."));
	QPushButton *read_only = box.addButton(tr("Open read-only"), QMessageBox::AcceptRole);
	QPushButton *anyway = box.addButton(tr("Open anyway"), QMessageBox::DestructiveRole);
	box.addButton(QMessageBox::Cancel);
	box.setDefaultButton(read_only);
	box.setEscapeButton(box.button(QMessageBox::Cancel));
	box.exec();
	if (box.clickedButton() == read_only) return LockChoice::ReadOnly;
	if (box.clickedButton() == anyway) return LockChoice::OpenAnyway;
	return LockChoice::Cancel;
}

/**
	@brief QETDiagramEditor::offerToRecover
	@p filepath could not be read (@p reason): say why, and offer its newest
	previous version (PreviousVersions) and, for a .qetz, what can be read
	of it (QetContainer::recover()). Either opens as a new project,
	"<name> (recovered)"; @p filepath is never moved or written.
	@return true if a project was opened
*/
bool QETDiagramEditor::offerToRecover(const QString &filepath, const QString &reason)
{
	const QFileInfo info(filepath);
	QFile file(filepath);
	const bool zip = file.open(QIODevice::ReadOnly) && QetContainer::isZip(file.peek(4));
	file.close();
	const QStringList versions = PreviousVersions::kept(filepath);

	QMessageBox box(QMessageBox::Warning, tr("Cannot open %1").arg(info.fileName()),
					reason.isEmpty()
					? tr("The file %1 cannot be read as a QElectroTech project.").arg(info.fileName())
					: tr("The project file is damaged:\n%1").arg(reason),
					QMessageBox::Cancel, this);
	QAbstractButton *previous = nullptr, *recover = nullptr;
	if (!versions.isEmpty()) {
		const QDateTime saved = QFileInfo(versions.first()).lastModified();
		previous = box.addButton(tr("Open the version saved %1")
								 .arg(QLocale().toString(saved, QLocale::ShortFormat)),
								 QMessageBox::AcceptRole);
	}
	if (zip)
		recover = box.addButton(tr("Recover what can be read"), QMessageBox::AcceptRole);
	if (previous || recover)
		box.setInformativeText(tr("Either opens as a new project, \"%1\"; %2 is left as it is.")
							   .arg(recoveredPath(filepath, info.suffix()).section(QLatin1Char('/'), -1),
									info.fileName()));
	box.exec();

	if (previous && box.clickedButton() == previous)
		return openAsRecovered(versions.first(), filepath);
	if (recover && box.clickedButton() == recover) {
		file.open(QIODevice::ReadOnly);
		QDomDocument document;
		QStringList lost;
		QString error;
		if (!QetContainer::recover(file.readAll(), &document, &lost, &error)) {
			QET::QetMessageBox::warning(this, tr("Cannot open %1").arg(info.fileName()),
										tr("Nothing can be recovered from it:\n%1").arg(error));
			return false;
		}
			//Opened from a .qetz written of it, as the project it replaces
		QTemporaryDir dir;
		const QString read = dir.filePath(info.completeBaseName() + QStringLiteral(".qetz"));
		const QList<QetZip::Entry> parts = QetContainer::split(document, QString(), &error);
		if (!dir.isValid() || parts.isEmpty() || !QetZip::write(read, parts, &error)
				|| !openAsRecovered(read, filepath)) {
			if (!error.isEmpty())
				QET::QetMessageBox::warning(this, tr("Cannot open %1").arg(info.fileName()), error);
			return false;
		}
		if (!lost.isEmpty()) {
			QMessageBox done(QMessageBox::Information, tr("Project recovered"),
							 tr("What could be read of %1 is open as a new project. "
								"Some parts could not be read and were left out: see the details.")
							 .arg(info.fileName()),
							 QMessageBox::Ok, this);
			done.setDetailedText(lost.join(QLatin1Char('\n')));
			done.exec();
		}
		return true;
	}
	return false;
}

/**
	@brief QETDiagramEditor::recoveredPath
	@return where a project recovered from @p filepath is saved: beside it,
	"<name> (recovered).<suffix>", numbered if that is taken
*/
QString QETDiagramEditor::recoveredPath(const QString &filepath, const QString &suffix)
{
	const QFileInfo info(filepath);
	QString path = info.absoluteDir().filePath(tr("%1 (recovered)").arg(info.completeBaseName())
											   + QLatin1Char('.') + suffix);
	for (int n = 2 ; QFileInfo::exists(path) ; ++n)
		path = info.absoluteDir().filePath(tr("%1 (recovered %2)").arg(info.completeBaseName()).arg(n)
										   + QLatin1Char('.') + suffix);
	return path;
}

/**
	@brief QETDiagramEditor::openAsRecovered
	Open @p source as a new project saved beside @p original, of its type
	(recoveredPath()), not saved yet.
	@return true if it opened
*/
bool QETDiagramEditor::openAsRecovered(const QString &source, const QString &original)
{
	QETProject *project = new QETProject(source);
	if (project->state() != QETProject::Ok) {
		QET::QetMessageBox::warning(this, tr("Cannot open %1").arg(QFileInfo(source).fileName()),
									project->openError());
		delete project;
		return false;
	}
	project->setFilePath(recoveredPath(original, QFileInfo(original).suffix()));
	project->setModified(true);
	addProject(project);
	return true;
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
				tr("Unable to open file", "message box title"),
				QString(
					tr("It appears the %1 file which you are trying to open does "
					"not exist or does not exist anymore.")
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
				tr("Unable to open file", "message box title"),
				tr("The file you try to open does not seem readable and can "
				"not be opened. Please check the file permissions.")
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
				tr("Opening the file read-only", "message box title"),
				tr("The project you try to open does not seem writable. It "
				"will be opened read-only.")
			);
		}
	}

		//Is someone else working on this file? Taken before the project is
		//read, so a cancel costs nothing. Never blocks opening: a folder
		//the lock cannot be made in, or the setting off, opens as before.
	std::unique_ptr<ProjectLock> lock;
	bool opened_read_only_by_lock = false;
	if (ProjectLock::mayTake() && filepath_info.isWritable())
	{
		lock = std::make_unique<ProjectLock>();
		if (lock->acquire(filepath) == ProjectLock::Result::Held)
		{
			const ProjectLock::Holder holder = lock->blockingHolder();
			bool take_over = false;
			if (interactive) {
				switch (askAboutLock(filepath, holder)) {
				case LockChoice::Cancel:
					return(false);
				case LockChoice::OpenAnyway:
					take_over = true;
					break;
				case LockChoice::ReadOnly:
					break;
				}
			}
				//Not interactive (an assistant opening it): read-only, which
				//is the safe answer to a question nobody is there to ask
			if (take_over && lock->acquire(filepath, true) == ProjectLock::Result::Acquired) {
				// ours now
			} else if (!take_over) {
				lock.reset();
				opened_read_only_by_lock = true;
			}
		}
	}

		//Rule 2 of the recovery specification: this is where a recovery
		//copy is offered - when this project is opened, and never at the
		//start of the program. Rules 1, 3 and 4 are decided by
		//CrashRecovery: only this project, only its newest copy, and only
		//when the previous session ended abruptly.
		//
		//Not offered for a project somebody else has open: it opens
		//read-only, so there is nothing here to restore into.
	KAutoSaveFile *recovery = nullptr;
	if (interactive && !opened_read_only_by_lock)
	{
		recovery = CrashRecovery::candidateFor(filepath);
		if (recovery)
		{
			const QDateTime written =
				QFileInfo(recovery->fileName()).lastModified();
			const QMessageBox::StandardButton answer =
				QET::QetMessageBox::question(
					this,
					tr("Fichier de restauration", "message box title"),
					tr("QElectroTech s'est arrêté pendant que ce projet était "
					   "ouvert. Ouvrir la copie de restauration du %1 au lieu "
					   "du fichier enregistré ?", "message box content")
						.arg(QLocale::system().toString(
							     written, QLocale::ShortFormat)),
					QMessageBox::Ok | QMessageBox::Cancel,
					QMessageBox::Ok);
			if (answer != QMessageBox::Ok)
			{
					//Never opened, so destroying it leaves the copy on
					//the disk; the copies are thrown away explicitly,
					//so the same question does not come back.
				delete recovery;
				recovery = nullptr;
				CrashRecovery::discardCopiesOf(filepath);
			}
		}
	}

		//Reading this project and walking the elements collection are the
		//same work on the same cores, and doing them at once costs far more
		//than doing them one after the other. So the collection is held back
		//from here to each DialogWaiting::dropInstance() below - paired with
		//dropInstance() on purpose, since both exits already carry it.
	m_collection_load_gate.projectLoadStarted();

	//Create the project
	DialogWaiting::instance(this);

		//Per-project window for the font counters reported below; the folios
		//(and with them the stored font descriptions) are built between here
		//and the end of addProject(). RAII, because DialogWaiting pumps the
		//event loop during the load: a nested openAndAddProject() gets its
		//own window and this one resumes unharmed.
	QETUtils::FontRestorationScope font_scope;

		//QETProject takes ownership of the recovery copy, and falls back to
		//the file on the disk by itself when the copy turns out unreadable.
	QETProject *project = recovery ? new QETProject(recovery)
				       : new QETProject(filepath);
	if (project -> state() != QETProject::Ok)
	{
			//A damaged project: its previous version or what can be read
			//of it, as a new project; the file itself is left as it is
		if (interactive && project->state() == QETProject::XmlParsingFailed
				&& offerToRecover(filepath, project->openError()))
		{
			delete project;
			DialogWaiting::dropInstance();
			return(true);
		}
		if (interactive && project -> state() != QETProject::FileOpenDiscard
				&& project -> state() != QETProject::XmlParsingFailed)
		{
			QET::QetMessageBox::warning(
				this,
				tr("Unable to open project", "message box title"),
				QString(
					tr(
						"The file %1 does not appear to be a QElectroTech "
						"project file. Thus it cannot be opened.",
						"message box content"
					)
				).arg(filepath)
			);
		}
		delete project;
		DialogWaiting::dropInstance();
		if (m_collection_load_gate.projectLoadFinished()) {
			startElementsCollectionLoad();
		}
		return(false);
	}

	if (opened_read_only_by_lock) project->setOpenedReadOnlyByLock(true);
	if (lock && lock->isHeld()) {
		connect(lock.get(), &ProjectLock::lockTaken, this, [this, project](const ProjectLock::Holder &by) {
			statusBar()->showMessage(
				tr("%1 on %2 has taken over %3. Save your changes with Save As to keep them.")
					.arg(by.user, by.host, QFileInfo(project->filePath()).fileName()),
				30000);
		});
		project->setLock(std::move(lock));
	}

	QETApp::projectsRecentFiles() -> fileWasOpened(filepath);
	addProject(project);
	DialogWaiting::dropInstance();

		//The project is in and the window is usable: the collection may now
		//have the machine. Nothing was skipped - it starts here instead of
		//in the middle of the load above.
	if (m_collection_load_gate.projectLoadFinished()) {
		startElementsCollectionLoad();
	}

	if (interactive && project->isFromLaterVersion()) {
		QET::QetMessageBox::warning(
			this,
			tr("Opening the file read-only", "message box title"),
			tr("This project was saved by a later version of QElectroTech. "
			   "It is opened read-only, so that saving it cannot lose what "
			   "this version does not know. Use Save As to keep a copy "
			   "in this version's format."));
	}

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
			details << tr("%n font descriptions written in a foreign or "
					  "corrupted format have been restored. They will be "
					  "rewritten in a stable format the next time the "
					  "project is saved.",
					  "message box content",
					  salvaged_fonts);
		}
		if (unreadable_fonts) {
			details << tr("%n font descriptions could not be read; the "
					  "default font will be used for these texts.",
					  "message box content",
					  unreadable_fonts);
		}
		QET::QetMessageBox::information(
			this,
			tr("Project fonts", "message box title"),
			details.join("\n\n")
		);
	}

		//Report the elements and wires left out of the load: an element
		//whose definition is missing, a wire whose terminal was not found.
		//They are kept in the file as they were (F127, F128), but are not
		//shown.
	QStringList lost_elements, lost_wires;
	for (Diagram *diagram : project->folios()) {
		if (!diagram->isLoaded()) continue;
		for (const QString &element : diagram->symbolsNotLoaded())
			lost_elements << tr("Sheet %1: %2").arg(diagram->folioIndex() + 1).arg(element);
	}
	for (Diagram *diagram : project->foliosWithWiresNotReconnected()) {
		for (const QString &wire : diagram->wiresNotReconnected()) {
			lost_wires << tr("Sheet %1: %2").arg(diagram->folioIndex() + 1).arg(wire);
		}
	}
	if (interactive && !lost_elements.isEmpty())
	{
		QMessageBox box(QMessageBox::Warning,
						tr("Elements not loaded", "message box title"),
						tr("%n elements could not be loaded: their definition is "
						   "missing from the project and from the collections.\n"
						   "\n"
						   "They are not shown, but they are kept in the file as they "
						   "are, with their conductors, and come back once their "
						   "definition is available again.",
						   "message box content",
						   lost_elements.size()),
						QMessageBox::Ok,
						this);
		box.setDetailedText(lost_elements.join(QLatin1Char('\n')));
		box.exec();
	}
	if (interactive && !lost_wires.isEmpty())
	{
		QMessageBox box(QMessageBox::Warning,
						tr("Conductors not loaded", "message box title"),
						tr("%n conductors could not be joined to their terminals and "
						   "were not loaded. The element's definition is missing, or "
						   "was replaced by another one with different terminals.\n"
						   "\n"
						   "They are not shown, but they are kept in the file as they "
						   "are, and join their terminals again once the definition "
						   "has them where they were.",
						   "message box content",
						   lost_wires.size()),
						QMessageBox::Ok,
						this);
		box.setDetailedText(lost_wires.join(QLatin1Char('\n')));
		box.exec();
	}

		//No copy of the file is made on opening it: the versions kept when the
		//project is saved (PreviousVersions) protect it from a bad save, in a
		//folder of the user's, and a copy beside the project at every opening
		//filled the folder people share with files nobody asked for (T40).

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

		//Diagnosis: with QET_SHOW_FOLIO=n, the folio n is shown two seconds
		//after the project opens, as a click on its tab shows it - for
		//measuring how long a folio takes to come up (the watchdog and its
		//stall sampler say where the time went), with no hand on the mouse
	bool show_folio_ok = false;
	const int show_folio = qEnvironmentVariableIntValue("QET_SHOW_FOLIO", &show_folio_ok);
	if (show_folio_ok && show_folio > 0)
	{
		QPointer<ProjectView> view = project_view;
		QTimer::singleShot(2000, this, [view, project, show_folio]() {
				//folios(), and not diagrams(), which builds every folio
			const QList<Diagram *> diagrams = project->folios();
			if (view && show_folio <= diagrams.size()) {
				view->showDiagram(diagrams.at(show_folio - 1));
			}
		});
	}

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
	@brief QETDiagramEditor::templateSaved
	List a template saved from a folio in this editor's templates tab.
	@param location : the saved .qetmak file
*/
void QETDiagramEditor::templateSaved(const ElementsLocation &location)
{
	if (m_element_collection_widget)
		m_element_collection_widget->addTemplate(location);
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
	else if (value == "center_on_cursor")
		dv->centerOnCursor();
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
	else if (value == "select_all_conductors")
		diagram->selectAllConductors();
	else if (value == "select_all_text_fields")
		diagram->selectAllTextFields();
}

/**
	@brief QETDiagramEditor::cableNumberingGate
	Decide whether the cable tool may start, asking about the numbering
	rule when the project numbers its cables by no rule at all.

	A project which does have one is asked nothing: the tool starts and
	that rule gives the numbers. A project which has none is asked once
	whether one should be defined now:

	- Oui opens the window where the rule is written, straight onto the
	  cable page of its automatic numbering; whatever he does in there,
	  the tool only starts if a rule really exists by the time that
	  window is closed.
	- Non says no, and says it for good: it is remembered in the program
	  settings so the question does not come up on the next start
	  either. Drawing goes on all the same, with W as the cable's name:
	  no number is worked out where none has been defined.
	- Annuler leaves everything as it was, the tool does not start, and
	  the question comes again the next time he picks the tool up.

	@param diagram the folio the cable would be drawn on
	@return true when the cable tool may start
*/
bool QETDiagramEditor::cableNumberingGate(Diagram *diagram)
{
	if (!diagram || !diagram->project()) return true;
	QETProject *project = diagram->project();
	if (project->hasCableAutoNum()) return true;

		//A read-only project is left alone without a question: there is
		//nowhere to define a rule in it, so asking whether he wants to
		//would be a way to nowhere. Its cables are drawn called W, the
		//same as any project without a rule.
	if (project->isReadOnly()) return true;

		//Whether he has once answered no in THIS project. The answer is
		//kept with the project rather than in the program settings, so
		//one project being answered with no does not stop every other
		//project from being asked.
	if (!project->cableAskNumbering()) {
		return true;
	}

	QMessageBox box(QMessageBox::Question,
					tr("Automatic numbering", "window title"),
					tr("No automatic cable numbering is defined.",
					   "shown when a project has no cable numbering rule"),
					QMessageBox::NoButton,
					this);
	box.setInformativeText(tr(
		"Do you want to define one now?\n\n"
		"You can also number the cables automatically later: the rule "
		"is set at any time in the project properties, on the "
		"\"Auto Numbering\" page.",
		"ask whether a cable numbering rule should be defined now"));
	QPushButton *define_button = box.addButton(tr("Yes", "yes"), QMessageBox::YesRole);
	QPushButton *later_button  = box.addButton(tr("No", "no"),  QMessageBox::NoRole);
	box.addButton(tr("Cancel", "cancel"), QMessageBox::RejectRole);
	box.setDefaultButton(later_button);
	box.exec();

	if (box.clickedButton() == later_button) {
			//With this project only: he has answered once here and
			//should not be asked again every time he picks the tool
			//up in this project -- another project is free to ask.
		project->setCableAskNumbering(false);
		project->setModified(true);
		return true;
	}
	if (box.clickedButton() != define_button) {
		return false;
	}

		//Oui: the window itself, opened where the rule is written. It
		//has to really be there by the time that window is closed --
		//a Cancel in it must not leave the tool running all the same.
	ProjectPropertiesDialog dialog(diagram->project(), this);
	dialog.setCurrentPage(ProjectPropertiesDialog::Autonum);
	dialog.changeToCable();
	dialog.exec();

	return diagram->project()->hasCableAutoNum();
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

	// This action group allows deselecting the currently-active tool by
	// clicking its own icon again, not just switching between tools --
	// that click still fires this slot (QActionGroup::triggered fires on
	// every click in the group, checked-state-changing or not), so
	// without this check it would unconditionally construct *another*
	// instance of the same tool and reactivate it, leaving the tool
	// fully active in the canvas while its own toolbar button shows
	// unchecked -- exactly backwards from what the click was for.
	// Scoped to checkable actions specifically: image/pdf/terminal_strip
	// are one-shot actions (pick a file, open a dialog) with no
	// persistent "active tool" state at all, and aren't even checkable
	// -- isChecked() on them is always false, so without this guard
	// they'd hit the branch above on every single click and never
	// reach their own handling below.
	if (action->isCheckable() && !action->isChecked())
	{
		d->clearEventInterface();
		return;
	}

	setLastCommand(action);

	DiagramEventInterface *diagram_event = nullptr;

	if (value == "line")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Line);
	else if (value == "rectangle")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Rectangle);
	else if (value == "ellipse")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Ellipse);
	else if (value == "arc")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Ellipse, true);
	else if (value == "polyline")
		diagram_event = new DiagramEventAddShape (d, QetShapeItem::Polygon);
	else if (value == "path")
	{
		diagram_event = new DiagramEventAddPath (d);
	}
	else if (value == QLatin1String("location_area"))
	{
		diagram_event = new DiagramEventAddLocationArea(d);
		statusBar()-> showMessage(tr("Cliquez-glissez, ou cliquez les deux coins, pour dessiner la zone. Click droit pour annuler"));
		connect(diagram_event, &DiagramEventInterface::destroyed, [this]() {
		statusBar()->clearMessage();
		});
	}
	else if (value == QLatin1String("cable"))
	{
			//The rule the number comes from -- or the word about there
			//being no rule yet -- comes before anything is drawn.
		if (!cableNumberingGate(d))
		{
				//Nothing is drawn, so the button must not go on looking
				//like a tool which is running, and whichever tool was
				//active before this click must not keep going with its
				//own button unchecked either.
			action->setChecked(false);
			d->clearEventInterface();
			return;
		}
		diagram_event = new DiagramEventAddCable(d);
	}
	else if (value == "fillet")
		diagram_event = new DiagramEventFillet (d);
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
#ifdef QET_HAS_QTPDF
	else if (value == "pdf")
	{
		DiagramEventAddPdf *deap = new DiagramEventAddPdf(d);
		if (deap->isNull())
		{
			delete deap;
			return;
		}
		else
			diagram_event = deap;
	}
#endif
	else if (value == "text")
	{
		diagram_event = new DiagramEventAddText(d);
	}
	else if (value == QLatin1String("generic_device"))
	{
		currentDiagramView()->addGenericDevice();
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
		// Defensive: on this style/theme, the toolbar button bound to an
		// exclusive-group action doesn't reliably repaint its checked
		// appearance after the group briefly had *no* action checked at
		// all (the gap between unchecking one tool and checking one
		// again, even the same one) -- confirmed by tracing a real
		// recording frame by frame: the status bar correctly showed this
		// tool's "before first click" hint (which can only appear from
		// inside a freshly-constructed tool's own constructor, so the
		// action's checked state and the tool's activation were both
		// genuinely correct) while the button itself stayed visually
		// unchecked for a sustained period. Forcing an explicit repaint
		// here makes the button's appearance match its actual state
		// regardless of whether Qt's own change notification fired
		// correctly.
		
		if (DiagramView *dv = currentDiagramView())
			dv->setFocus();  // so the view (and the active tool) actually receives Escape etc

		if (QWidget *button = m_add_item_tool_bar->widgetForAction(action))
			button->update();
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
                                                tr("Unable to delete the terminal"),
                                                tr("The deletion cannot be performed because the selection contains one or more terminals "
												   "that are bridged and/or belong to a multi-level terminal.\n"
												   "Unbridge and/or remove the levels from the affected terminals so that they can be "
												   "deleted"));
            } else {
					//A cable line is not held by DiagramContent: it is
					//removed by its own command, which also lets the cable
					//it belongs to go when no line of it is left anywhere.
				QList<CablePart *> cable_parts;
				for (QGraphicsItem *item : diagram->selectedItems()) {
					if (auto *part = qgraphicsitem_cast<CablePart *>(item)) {
						cable_parts << part;
					}
				}
				const bool other_items = dc.hasDeletableItems();
					//Nor is the frame drawn round a strip: Delete takes it
					//off the folio and leaves its terminals on it (T85)
				const QList<StripFrameItem *> strip_frames =
						StripFrameItem::stripFrames(diagram->selectedItems());

                diagram->clearSelection();

				if (!strip_frames.isEmpty()) {
					QList<QUndoCommand *> others;
					if (other_items) {
						others << new DeleteQGraphicsItemCommand(diagram, dc);
					}
					if (!cable_parts.isEmpty()) {
						others << new RemoveCableCommand(diagram, cable_parts);
					}
					if (QUndoCommand *step = StripFrameItem::deleteCommand(strip_frames, others)) {
						diagram->undoStack().push(step);
					}
				} else if (cable_parts.isEmpty()) {
					diagram->undoStack().push(new DeleteQGraphicsItemCommand(diagram, dc));
				} else if (!other_items) {
					diagram->undoStack().push(new RemoveCableCommand(diagram, cable_parts));
				} else {
						//One single step, so a selection holding both a
						//cable and ordinary drawings comes back whole.
					diagram->undoStack().push(new BatchCommand(
						new DeleteQGraphicsItemCommand(diagram, dc),
						new RemoveCableCommand(diagram, cable_parts)));
				}
                dv->adjustSceneRect();
            }
        }
	else if (value == "rotate_selection")
	{
			//The frame drawn round a strip turns as one piece, its
			//terminals round their middle and its name upright (T85)
		if (StripFrameItem::rotateSelection(diagram, 90))
			return;
			//A selection that is exactly one whole group turns as one piece,
			//as "Pivoter le groupe" does, rather than each member in place
			//(discussion #1070). Wires follow their symbols either way.
		QList<QGraphicsItem *> members;
		for (QGraphicsItem *item : diagram->selectedItems()) {
			if (item->type() != Conductor::Type) {
				members << item;
			}
		}
		const bool whole_group = !ItemGroups::soleWholeGroup(members).isNull();
		RotateSelectionCommand *c = new RotateSelectionCommand(diagram, 90, nullptr, whole_group);
		if(c->isValid())
			diagram->undoStack().push(c);
	}
	else if (value == "mirror_horizontal" || value == "mirror_vertical")
	{
		auto *c = new MirrorSelectionCommand(
					diagram,
					value == "mirror_horizontal" ? Qt::Horizontal : Qt::Vertical);
		if (c->isValid())
			diagram->undoStack().push(c);
		else
			delete c;
	}
	else if (value == "rotate_group_selection")
	{
		RotateSelectionCommand *c = new RotateSelectionCommand(diagram, 90, nullptr, true);
		if(c->isValid())
			diagram->undoStack().push(c);
	}
	else if (value == "rotate_selected_text")
	{
			//Ask for the angle first, then build the command: the command
			//itself no longer opens a dialog. Guarding on the selection keeps
			//the previous behaviour of showing no dialog when there is
			//nothing to rotate.
		if (RotateTextsCommand::hasSelectedTexts(diagram))
		{
			qreal rotation = RotateTextsCommand::currentRotation(diagram);
			if (RotateTextsCommand::askRotation(rotation))
				diagram->undoStack().push(new RotateTextsCommand(diagram, rotation));
		}
	}
	else if (value == "find_selected_element" && currentElement())
		findElementInPanel(currentElement()->location());
	else if (value == "edit_selected_element")
		dv->editSelection();
	else if (value == "group_selection" || value == "ungroup_selection")
	{
		GroupItemsCommand *command = value == "group_selection"
				? GroupItemsCommand::group(diagram)
				: GroupItemsCommand::ungroup(diagram);
		if (command) {
			diagram->undoStack().push(command);
		}
	}
	else if (value == "group_selected_texts")
	{
		QList<DynamicElementTextItem *> deti_list = dc.m_element_texts.values();
		if(deti_list.size() <= 1)
			return;

		diagram->undoStack().push(new AddTextsGroupCommand(deti_list.first()->parentElement(), tr("Group"), deti_list));
	}
}

namespace
{
	/**
		Ask the pitch of a distribution, in grid steps, with @a automatic
		filled in: the smallest at which no text overlaps the next one. A
		smaller one is taken too, and the box says texts will overlap.
		@return the pitch, or 0 when the box is cancelled
	*/
	int askDistributionPitch(QWidget *parent, const QString &title, int automatic)
	{
		QDialog dialog(parent);
		dialog.setWindowTitle(title);

		auto *pitch = new QSpinBox(&dialog);
		pitch->setRange(1, 999);
		pitch->setValue(automatic);
		pitch->setSuffix(QCoreApplication::translate("QETDiagramEditor", " × grid"));
		pitch->setToolTip(QCoreApplication::translate("QETDiagramEditor",
			"From the centre of one item to the centre of the next, in grid steps"));
		auto *automatic_button = new QPushButton(
			QCoreApplication::translate("QETDiagramEditor", "Automatic"), &dialog);
		automatic_button->setToolTip(QCoreApplication::translate("QETDiagramEditor",
			"The smallest pitch at which no text overlaps the next one"));
		auto *warning = new QLabel(QCoreApplication::translate("QETDiagramEditor",
			"Smaller than the automatic pitch: some texts will overlap."), &dialog);
		warning->setWordWrap(true);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

		auto *row = new QHBoxLayout;
		row->addWidget(new QLabel(QCoreApplication::translate("QETDiagramEditor", "Pitch:"), &dialog));
		row->addWidget(pitch, 1);
		row->addWidget(automatic_button);
		auto *layout = new QVBoxLayout(&dialog);
		layout->addLayout(row);
		layout->addWidget(warning);
		layout->addWidget(buttons);

		auto update = [pitch, warning, automatic]() {
			warning->setVisible(pitch->value() < automatic);
		};
		QObject::connect(pitch, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, update);
		QObject::connect(automatic_button, &QPushButton::clicked, &dialog,
						 [pitch, automatic]() { pitch->setValue(automatic); });
		QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		update();
		pitch->selectAll();
		pitch->setFocus();
		return dialog.exec() == QDialog::Accepted ? pitch->value() : 0;
	}
}

/**
	@brief QETDiagramEditor::alignGroupTriggered
	Run the align action @a action on the selection of the current diagram,
	and say in the status bar what it did, including when there was nothing
	to do or when locked items were left in place. A distribution asks its
	pitch first.
	@param action
*/
void QETDiagramEditor::alignGroupTriggered(QAction *action)
{
	DiagramView *dv = currentDiagramView();
	if (!dv)
		return;

	static const QHash<QString, AlignSelectionCommand::Mode> modes {
		{QStringLiteral("snap_selection_to_grid"), AlignSelectionCommand::SnapToGrid},
		{QStringLiteral("align_left"),    AlignSelectionCommand::AlignLeft},
		{QStringLiteral("align_hcenter"), AlignSelectionCommand::AlignHCenter},
		{QStringLiteral("align_right"),   AlignSelectionCommand::AlignRight},
		{QStringLiteral("align_top"),     AlignSelectionCommand::AlignTop},
		{QStringLiteral("align_vcenter"), AlignSelectionCommand::AlignVCenter},
		{QStringLiteral("align_bottom"),  AlignSelectionCommand::AlignBottom},
		{QStringLiteral("distribute_vertical"),   AlignSelectionCommand::DistributeVertical},
		{QStringLiteral("distribute_horizontal"), AlignSelectionCommand::DistributeHorizontal}
	};
	const QString id = action->data().toString();
	if (!modes.contains(id))
		return;
	const AlignSelectionCommand::Mode mode = modes.value(id);
	const bool distribution = mode == AlignSelectionCommand::DistributeVertical
			|| mode == AlignSelectionCommand::DistributeHorizontal;

	Diagram *diagram = dv->diagram();
		//The pitch, asked with the automatic one filled in; with fewer than
		//two pieces there is none, and the command says why below
	int pitch_steps = 0;
	if (distribution)
	{
		const int automatic = AlignSelectionCommand::automaticPitch(diagram, mode);
		if (automatic > 0)
		{
			pitch_steps = askDistributionPitch(this,
				action->text().remove(QStringLiteral("…")), automatic);
			if (pitch_steps <= 0)
				return;
		}
	}
	auto *command = new AlignSelectionCommand(diagram, mode, pitch_steps);
	const int locked = command->lockedCount();

	QString message;
	if (command->isValid())
	{
		if (mode == AlignSelectionCommand::SnapToGrid)
			message = tr("%n items put back on the grid", "", command->movedCount());
		else if (distribution)
			message = tr("%n items distributed", "", command->movedCount());
		else
			message = tr("%n items aligned", "", command->movedCount());
		diagram->undoStack().push(command);
	}
	else
	{
		if (mode == AlignSelectionCommand::SnapToGrid)
			message = tr("The selection is already on the grid");
		else if (command->itemCount() < 2)
			message = tr("Select at least two unlocked elements, images, texts, shapes or groups");
		else if (distribution)
			message = tr("The selection is already distributed at this pitch");
		else
			message = tr("The selection is already aligned, to the nearest grid step");
		delete command;
	}
	if (locked)
		message += QLatin1Char(' ') + tr("(%n locked objects left in place)", "", locked);

		//Queued, not shown at once: when the action comes from a menu or the
		//command search, closing it queues events that would clear a message
		//shown right now.
	QTimer::singleShot(0, this, [this, message]() {
		statusBar()->showMessage(message, 5000);
	});
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
	m_save_copy_for_older->         setEnabled(opened_project);
	m_rotate_texts->                setEnabled(editable_project);
	m_export_to_images->            setEnabled(opened_diagram);
	m_print->                       setEnabled(opened_diagram);
	m_export_to_pdf->               setEnabled(opened_diagram);
	m_edit_diagram_properties->     setEnabled(opened_diagram);
	m_zoom_actions_group.           setEnabled(opened_diagram);
	m_select_actions_group.         setEnabled(opened_diagram);
	m_add_item_actions_group.       setEnabled(editable_project);
	m_insert_last_element->         setEnabled(opened_diagram && editable_project && !m_last_inserted_element.isNull());
	m_show_element_picker->         setEnabled(opened_diagram && editable_project);
	m_show_shortcut_bar->           setEnabled(opened_diagram && editable_project);
	m_repeat_last_command->         setEnabled(opened_diagram && editable_project
						   && m_last_command && m_last_command->isEnabled());
	m_row_column_actions_group.     setEnabled(editable_project);
	m_background_color_button->    setEnabled(opened_diagram);
	m_draw_grid->                   setEnabled(opened_diagram);
	m_draw_guides->                 setEnabled(opened_diagram);
	m_cell_rulers->                 setEnabled(opened_diagram);
	m_cell_lines->                  setEnabled(opened_diagram);

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
	m_project_wiring_list_view    -> setEnabled(opened_project);
	m_terminal_numbering          -> setEnabled(editable_project);
	m_renumber_components         -> setEnabled(editable_project);
	m_iec_structure               -> setEnabled(editable_project);
		//Marking is a change to the project like any other - it goes on the
		//undo stack and is written to the file - so a project opened read
		//only has no business being marked.
	m_assembly_state              -> setEnabled(editable_project);
	m_location_manager            -> setEnabled(editable_project);
	m_location_report             -> setEnabled(editable_project);
	m_location_bom                -> setEnabled(opened_project);
	m_mounting_layout             -> setEnabled(editable_project);
		//Posing a plate writes on a folio, so it needs one open on top of
		//an editable project - which is one condition more than opening the
		//layout window needs.
	m_put_mounting_layout         -> setEnabled(editable_project && opened_diagram);
	m_create_symbol               -> setEnabled(editable_project);
	m_generate_pinout             -> setEnabled(editable_project);
	m_generate_circuits           -> setEnabled(editable_project);
	m_import_io_sheet             -> setEnabled(editable_project);
	m_assign_io_points            -> setEnabled(editable_project);
	m_io_list                     -> setEnabled(editable_project);
	m_mark_io_bus                 -> setEnabled(editable_project);
	m_wire_io_commons             -> setEnabled(editable_project);
	m_explode_element             -> setEnabled(editable_project);
	m_show_conductor_text         -> setEnabled(editable_project);
	m_hide_conductor_text         -> setEnabled(editable_project);
	m_align_conductor_text        -> setEnabled(editable_project);
	m_convert_wire_names          -> setEnabled(editable_project);
	m_renumber_wires              -> setEnabled(editable_project);
	m_remove_wires                -> setEnabled(editable_project);
	m_replace_part                -> setEnabled(editable_project);
	m_save_group                  -> setEnabled(editable_project);
	m_insert_group                -> setEnabled(editable_project);
	m_cable_numbering             -> setEnabled(editable_project);
	m_add_cable_list              -> setEnabled(editable_project);
	m_export_cable_list           -> setEnabled(editable_project);
	m_reload_element_drawings     -> setEnabled(opened_project);
		//It changes the project's symbols and reloads it: not on a project
		//nobody can write to
	m_whole_terminals             -> setEnabled(editable_project);
	updateFolioHistoryActions();
#ifdef QET_HAS_SCRIPTING
	m_run_script                  -> setEnabled(opened_project);
#endif
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
	m_catalog_no_physical_view    -> setEnabled(opened_project);
		//Opened and not editable, the same as the two reports above it:
		//reading what a project still needs is worth doing on a project
		//nobody can write to.
	m_connector_check             -> setEnabled(opened_project);
	m_drc_check                   -> setEnabled(opened_project);
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
	@brief QETDiagramEditor::updateFolioHistoryActions
	Back and Forward are offered when the project in front has a folio to
	go back, or forward, to.
*/
void QETDiagramEditor::updateFolioHistoryActions()
{
	ProjectView *project_view = currentProjectView();
	m_folio_back   ->setEnabled(project_view && project_view->canGoBack());
	m_folio_forward->setEnabled(project_view && project_view->canGoForward());
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

namespace {
/**
	@return the elements of @p diagram's selection which take information
*/
QList<Element *> selectedInformationElements(Diagram *diagram)
{
	QList<Element *> list;
	for (Element *element : DiagramContent(diagram).m_elements) {
		if (MultiElementInfoWidget::takesInformations(element))
			list << element;
	}
	return list;
}
} // namespace

/**
	@brief QETDiagramEditor::slot_updateComplexActions
	Manage the actions that need some conditions to be enabled or not.
	This method does nothing if there is no project opened
*/
void QETDiagramEditor::slot_updateComplexActions()
{
#ifdef QET_HAS_SCRIPTING
	updateScriptActions();
#endif
	DiagramView *dv = currentDiagramView();
	if(!dv)
	{
		QList <QAction *> action_list;
		action_list << m_conductor_reset
			    << m_conductor_arrange
			    << m_conductor_arrange_folio
			    << m_update_wire_sections
			    << m_leave_out_wires
			    << m_find_element
			    << m_navigate
			    << m_cut
			    << m_copy
			    << m_paste_element_info
			    << m_duplicate
			    << m_delete_selection
			    << m_rotate_selection
			    << m_rotate_group_selection
			    << m_mirror_horizontal
			    << m_mirror_vertical
			    << m_element_scale
			    << m_strip_frame
			    << m_replace_element
			    << m_exchange_elements
			    << m_reset_symbol_texts
			    << m_edit_selection
			    << m_group_selected_texts
			    << m_group_selection
			    << m_ungroup_selection;
		for(QAction *action : action_list)
			action->setEnabled(false);
		m_align_actions_group.setEnabled(false);

		return;
	}

	Diagram *diagram_ = dv->diagram();
	DiagramContent dc(diagram_);
	bool ro = diagram_->isReadOnly();


	//Number of selected conductors
	int selected_conductors_count = diagram_->selectedConductors().count();
	m_conductor_reset->setEnabled(!ro && selected_conductors_count);
	m_conductor_arrange->setEnabled(!ro && selected_conductors_count);
	updateLeaveOutWires();
	m_conductor_arrange_folio->setEnabled(!ro && !diagram_->conductors().isEmpty());
		//The whole project, and not this folio: its folio may hold no
		//conductor while the others do (T70)
	m_update_wire_sections->setEnabled(!ro);

	// number of selected elements
	int selected_elements_count = dc.count(DiagramContent::Elements);
	m_find_element->setEnabled(selected_elements_count == 1);
	m_copy_text_format->setEnabled(selected_elements_count >= 1);
	m_apply_text_format->setEnabled(!ro && selected_elements_count >= 1 && ElementTextFormatPattern::hasPattern());
	m_place_texts->setEnabled(!ro && selected_elements_count >= 1);
	m_element_scale->setEnabled(!ro && selected_elements_count >= 1);
	m_strip_frame->setEnabled(!ro && !StripFrameItem::stripTerminals(diagram_->selectedItems()).isEmpty());
	m_mirror_horizontal->setEnabled(!ro && selected_elements_count);
	m_mirror_vertical->setEnabled(!ro && selected_elements_count);
	m_replace_element->setEnabled(!ro && selected_elements_count);
		//On as soon as there is an element: with other than two, the swap
		//says so instead of keeping quiet (T66)
	m_exchange_elements->setEnabled(!ro && selected_elements_count);
	m_reset_symbol_texts->setEnabled(!ro && selected_elements_count);

		//Navigating answers « where is the other one », so it is offered as
		//soon as there is one. Several used to keep the entry away - the
		//command could not choose for the reader - and that made it greyed
		//out on the ordinary case, a coil answered by three contacts; the
		//choice list now asks instead, so the count only has to be non zero.
		//Read-only has no say: going somewhere writes nothing.
	m_navigate->setEnabled(!navigationTargets(diagram_).isEmpty());

	//Actions that need items (elements, conductors, texts...) selected, to be enabled
		//A cable line is drawn on the folio but held by the cable, not
		//by DiagramContent: it counts on its own for deleting and editing.
	int selected_cables = 0;
	for (QGraphicsItem *item : diagram_->selectedItems()) {
		if (item && item->type() == CablePart::Type) {
			selected_cables++;
		}
	}

		//A drawn cable line is copyable, cuttable and duplicatable on
		//its own, although it belongs to its cable rather than to the
		//folio and DiagramContent never sees it.
	bool copiable_items  = dc.hasCopiableItems() || selected_cables > 0;
	bool deletable_items = dc.hasDeletableItems() || selected_cables > 0;
		//The frame drawn round a strip is not copied, but Delete takes it
		//off and Rotate turns it with its terminals (T85)
	const bool strip_frames = !StripFrameItem::stripFrames(diagram_->selectedItems()).isEmpty();
	m_cut              -> setEnabled(!ro && copiable_items);
	m_copy             -> setEnabled(copiable_items);
	m_duplicate        -> setEnabled(!ro && copiable_items);
	updatePasteElementInfoAction();
	m_delete_selection -> setEnabled(!ro && (deletable_items || strip_frames));
	m_rotate_selection -> setEnabled(!ro && (diagram_->canRotateSelection() || strip_frames));
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

	m_group_selection  ->setEnabled(GroupItemsCommand::canGroup(diagram_));
	m_ungroup_selection->setEnabled(GroupItemsCommand::canUngroup(diagram_));

	// actions need only one editable item
	int selected_image = dc.count(DiagramContent::Images);

	int selected_shape = dc.count(DiagramContent::Shapes);
	int selected_editable = selected_elements_count
			+ (selected_texts
			   - selected_conductor_texts
			   - selected_dynamic_elmt_text)
			+ selected_image
			+ selected_shape
			+ selected_conductors_count
			+ selected_cables;

		//Several elements and nothing else: edit their information together (#1439)
	if (!ro && selected_editable > 1 && selected_editable == selected_elements_count
			&& selectedInformationElements(diagram_).size() > 1)
	{
		m_edit_selection -> setEnabled(true);
		m_edit_selection -> setText(tr("Edit the information of the elements",
					       "edit several elements"));
		m_edit_selection -> setIcon(QET::Icons::ElementEdit);
	}
	else if (selected_editable == 1)
	{
		m_edit_selection -> setEnabled(true);
		//edit element
		if (selected_elements_count)
		{
			m_edit_selection -> setText(tr("Edit the element",
						       "edit element"));
			m_edit_selection -> setIcon(QET::Icons::ElementEdit);
		}
		//edit text field
		else if (selected_texts)
		{
			m_edit_selection -> setText(tr("Edit the text field",
						       "edit text field"));
			m_edit_selection -> setIcon(QET::Icons::EditText);
		}
		//edit image
		else if (selected_image)
		{
			m_edit_selection -> setText(tr("Edit the image",
						       "edit image"));
			m_edit_selection -> setIcon(QET::Icons::resize_image);
		}
		//edit cable
		else if (selected_cables)
		{
			m_edit_selection -> setText(tr("Edit the cable",
						       "edit cable"));
			m_edit_selection -> setIcon(QET::Icons::Cable);
		}
		//edit conductor
		else if (selected_conductors_count)
		{
			m_edit_selection -> setText(tr("Edit conductor",
						       "edit conductor"));
			m_edit_selection -> setIcon(QET::Icons::ConductorEdit);
		}
	}
	//not an editable item
	else
	{
		m_edit_selection -> setText(tr("Edit the selected object",
					       "edit selected item"));
		m_edit_selection -> setIcon(QET::Icons::ElementEdit);
		m_edit_selection -> setEnabled(false);
	}

	//Actions for edit Z value
	QList<QGraphicsItem *> list = dc.items(
				DiagramContent::SelectedOnly
				| DiagramContent::Elements
				| DiagramContent::Shapes
				| DiagramContent::LocationAreas
				| DiagramContent::Images);
	m_depth_action_group->setEnabled(list.isEmpty()? false : true);

		//Align actions: symbols, pictures, free texts and shapes take part,
		//counted the way the command counts them, a group as one.
		//Snapping needs one of them, lining them up needs two.
	const int alignable = AlignSelectionCommand::unitCount(dc);
	m_align_actions_group.setEnabled(!ro && alignable);
	const QList<QAction *> align_actions = m_align_actions_group.actions();
	for (QAction *action : align_actions.mid(1))
		action->setEnabled(!ro && alignable >= 2);
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

	if (m_conductor_color_button) {
		m_conductor_color_button->updateEnabledState();
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
	const bool may_be_diagram = Diagram::clipboardMayContainDiagram();
	m_paste -> setEnabled(editable_diagram && may_be_diagram);
	m_paste_origin -> setEnabled(editable_diagram && may_be_diagram);

		//Read the clipboard once here rather than on every selection
		//change: it can hold a whole folio.
	m_clipboard_element_info.clear();
	m_clipboard_has_element = false;
	if (may_be_diagram)
	{
		QDomDocument document;
		if (document.setContent(QApplication::clipboard()->text()))
		{
			const QList<QDomElement> elements = QET::findInDomElement(
						document.documentElement(),
						QStringLiteral("elements"),
						QStringLiteral("element"));
			if (elements.size() == 1)
			{
				m_clipboard_element_info.fromXml(
							elements.first().firstChildElement(QStringLiteral("elementInformations")),
							QStringLiteral("elementInformation"));
				m_clipboard_has_element = true;
			}
		}
	}
	updatePasteElementInfoAction();
}

/**
	@brief QETDiagramEditor::updatePasteElementInfoAction
	Enable "paste element information" when the clipboard holds exactly one
	element and the selection of an editable folio has an element to give
	its information to.
*/
void QETDiagramEditor::updatePasteElementInfoAction()
{
	DiagramView *dv = currentDiagramView();
	m_paste_element_info->setEnabled(
				m_clipboard_has_element
				&& dv && dv->diagram() && !dv->diagram()->isReadOnly()
				&& !selectedInformationElements(dv->diagram()).isEmpty());
}

/**
	@brief QETDiagramEditor::pasteElementInformations
	Copy the information of the element on the clipboard onto the selected
	elements, in one undo step (#1375). Every field filled in on the copied
	element replaces the same field; a field it leaves empty is left alone,
	since an empty field is not written to the clipboard and cannot be told
	from one the copied element does not have.
	The label and its formula travel together: one without the other would
	have the formula rebuild a label the user did not copy.
*/
void QETDiagramEditor::pasteElementInformations()
{
	DiagramView *dv = currentDiagramView();
	if (!m_clipboard_has_element || !dv || !dv->diagram()
			|| dv->diagram()->isReadOnly())
		return;

	const DiagramContext &source = m_clipboard_element_info;
	const bool takes_label = source.contains(QETInformation::ELMT_LABEL)
			|| source.contains(QETInformation::ELMT_FORMULA);
		//A numbering scheme id is only kept if it names a scheme of this
		//project; one copied from another project would name nothing.
	QETProject *project = dv->diagram()->project();
	const QUuid scheme_id(source.value(QETInformation::ELMT_FORMULA_ID).toString());
	const bool keep_scheme_id = project && !scheme_id.isNull()
			&& !project->elementAutoNumTitle(scheme_id).isEmpty();

	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> changes;
	for (Element *element : selectedInformationElements(dv->diagram()))
	{
		const DiagramContext old_info = element->elementInformations();
		DiagramContext new_info = old_info;
		if (takes_label) {
			new_info.remove(QETInformation::ELMT_LABEL);
			new_info.remove(QETInformation::ELMT_FORMULA);
			new_info.remove(QETInformation::ELMT_FORMULA_ID);
		}
		for (const QString &key : source.keys()) {
			if (key == QETInformation::ELMT_FORMULA_ID && !keep_scheme_id
					|| key == "exclude_from_bom")
				continue;
			new_info.addValue(key, source.value(key), source.keyMustShow(key));
		}
		if (new_info != old_info)
			changes.insert(element, qMakePair(old_info, new_info));
	}
	if (changes.isEmpty())
		return;

	auto *undo = new ChangeElementInformationCommand(changes);
	undo->setText(tr("Paste element information"));
	dv->diagram()->undoStack().push(undo);
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

		//Back and Forward follow the way walked in the project in front
	connect(project_view, &ProjectView::historyChanged, this, &QETDiagramEditor::updateFolioHistoryActions);

	//Highlight the current page
	connect(project_view, &ProjectView::diagramActivated, this, [this](DiagramView *dv) {
			//The actions that depend on the folio, for the folio now shown:
			//going to another folio of the same project changes no
			//selection, and "Arrange the folio's conductors" kept the state
			//of the folio left - disabled on a folio full of wires, coming
			//from one with none (T80, step 2)
		slot_updateComplexActions();
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
		action -> setStatusTip(QString(tr("Activates the project « %1 »")).arg(pv_title));
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

	// gestion des projets (onglets ou fenetres)
	bool tabbed = settings.value("diagrameditor/viewmode", "tabbed") == "tabbed";
	if (tabbed) {
		setTabbedMode();
	} else {
		setWindowedMode();
	}
}

/**
	@brief QETDiagramEditor::readSettingsState
	Restore the window state (docks, toolbars).
	Must be called AFTER show() in Qt6 for restoreState() to work correctly.
*/
void QETDiagramEditor::readSettingsState()
{
	QSettings settings;

	// etat de la fenetre (barres d'outils, docks...)
	QVariant state = settings.value("diagrameditor/state");
	if (state.isValid()) {
		if (!restoreState(state.toByteArray())) {
			settings.remove("diagrameditor/state");
		}
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
		reply = QMessageBox::question(this, tr("Delete this sheet"),
									  tr("Are you sure you want to delete this sheet?"),
									  QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::No) return;
	} else {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, tr("Delete the sheets"),
									  tr("Are you sure you want to delete the selected %1 sheets ?").arg(diagrams.count()),
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
			? tr("Delete this sheet")
			: tr("Delete %1 sheets").arg(diagrams.count()));
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
				diagram_project->undoStack()->beginMacro(tr("Move the sheets"));
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
				diagram_project->undoStack()->beginMacro(tr("Move the sheets"));
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
				diagram_project->undoStack()->beginMacro(tr("Move the sheets"));
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
				diagram_project->undoStack()->beginMacro(tr("Move the sheets"));
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
				diagram_project->undoStack()->beginMacro(tr("Move the sheets"));
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
				diagram_project->undoStack()->beginMacro(tr("Move the sheets"));
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
				diagram_project->undoStack()->beginMacro(tr("Move the sheets"));
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
		//Grouping leaves the selection as it is, so without this Group
		//and Ungroup would keep the state from before (#1144)
	connect(dv->diagram(),
		&Diagram::itemGroupChanged,
		this,
		&QETDiagramEditor::slot_updateComplexActions);
	connect(dv, &DiagramView::modeChanged, this, &QETDiagramEditor::slot_updateModeActions);
	connect(dv, &DiagramView::elementPlacementStarted, this, &QETDiagramEditor::rememberPlacedElement);
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
	QET::QetMessageBox::critical(this, tr("Error", "message box title"), error);
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
		setWindowModified(project->projectWasModified());
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
	@brief QETDiagramEditor::insertElementFromCollection
	Place @a location on the current folio using the interactive placement
	mode -- the same mode a drag and drop ends in, entered without the drag.
	@param location
*/
void QETDiagramEditor::insertElementFromCollection(const ElementsLocation &location)
{
	DiagramView *dv = currentDiagramView();
	if (dv && dv->startElementPlacement(location, dv->defaultPlacementPos())) {
		return;
	}

		//Nowhere to place it: no folio open, or a read-only project. Do what
		//a double click did before it placed, rather than nothing.
	m_element_collection_widget->editLocation(location);
}

/**
	@brief QETDiagramEditor::rememberPlacedElement
	Remember @a location for "insert last element". Connected to every view,
	so an element placed by drag and drop counts as well as one placed from
	the collection without a drag. Macros never reach here: the view does not
	report them, since re-entering their placement mode from a shortcut has
	not been thought through.
	@param location
*/
void QETDiagramEditor::rememberPlacedElement(const ElementsLocation &location)
{
	m_last_inserted_element = location;
	m_insert_last_element->setEnabled(true);
	setLastCommand(m_insert_last_element);
}

/**
	@brief QETDiagramEditor::setLastCommand
	Remember @a action as the command Enter repeats, and name it in the
	Édition menu entry so it is clear what will happen.
*/
void QETDiagramEditor::setLastCommand(QAction *action)
{
	m_last_command = action;
	m_repeat_last_command->setText(
		action == m_insert_last_element
		? tr("Repeat: insert \"%1\"").arg(m_last_inserted_element.name())
		: tr("Repeat: %1").arg(action->text().remove(QLatin1Char('&'))));
	slot_updateActions();
}

/**
	@brief QETDiagramEditor::repeatLastCommand
	Run the last drawing or placing command again -- a line, a text field,
	the last element -- as Enter does in SolidWorks.
	@return true if a command was run
*/
bool QETDiagramEditor::repeatLastCommand()
{
	if (!m_last_command || !m_repeat_last_command->isEnabled()) {
		return false;
	}
		//A tool that is still on would be switched off by triggering it
	if (m_last_command->isCheckable() && m_last_command->isChecked()) {
		return false;
	}
	m_last_command->trigger();
	return true;
}

/**
	@brief QETDiagramEditor::insertLastElement
	Re-enter placement mode with the element placed most recently, so a run of
	the same symbol can be dropped without returning to the collection.
*/
void QETDiagramEditor::insertLastElement()
{
	if (!m_last_inserted_element.isElement()
	    || !m_last_inserted_element.exist()) {
		return;
	}
	insertElementFromCollection(m_last_inserted_element);
}

/**
	@brief QETDiagramEditor::elementPicker
	@return the element picker, built on first use: most sessions never open
	it, and it holds a list view and a model of its own.
*/
ElementPickerPopup *QETDiagramEditor::elementPicker()
{
	if (!m_element_picker)
	{
		m_element_picker = new ElementPickerPopup(m_element_collection_widget,
							  this);
		connect(m_element_picker, &ElementPickerPopup::elementChosen,
			this, &QETDiagramEditor::insertElementFromCollection);
		connect(m_element_picker, &ElementPickerPopup::replacementChosen,
			this, &QETDiagramEditor::replaceSelectionWith);
	}
	return m_element_picker;
}

/**
	@brief QETDiagramEditor::showElementPicker
	Open the element picker where the mouse is.
*/
void QETDiagramEditor::showElementPicker()
{
	if (!currentDiagramView()) {
		return;
	}

	elementPicker()->popUpAt(QCursor::pos());
}

/**
	@brief QETDiagramEditor::showShortcutBar
	Open the element picker where the mouse is, with a row of commands above
	it chosen by what is selected: the folio's commands with nothing
	selected, conductor commands when only conductors are, and selection
	commands otherwise. The commands are ShortcutManager ids, listed in
	ShortcutBarSettings and editable in the configuration dialog.
*/
void QETDiagramEditor::showShortcutBar()
{
	DiagramView *dv = currentDiagramView();
	if (!dv) {
		return;
	}

	const ShortcutBarSettings::Context context =
		ShortcutBarSettings::contextFor(dv->diagram()->selectedItems());

	elementPicker()->popUpShortcutBar(QCursor::pos(), context);
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

/**
 * @brief QETDiagramEditor::slot_cableNumbering
 * Opens the window holding the numbering rule of the project's cables
 * and the button which numbers every one of them again. The window
 * writes the project and pushes its own undo step itself, so nothing
 * is done with its result here beyond letting it work.
 */
void QETDiagramEditor::slot_cableNumbering() {
	QETProject *project = currentProject();
	if (!project) return;

	CableNumberingDialog dialog(project, this);
	dialog.exec();
}

/**
	@brief QETDiagramEditor::slot_reloadElementDrawings
	Redraw every placed element of the current project from its current
	definition (bugtracker #802): a symbol edited and saved after being
	placed keeps showing its old drawing otherwise, until the whole project
	is closed and reopened.

	Purely visual and not undoable, the way pressing a "refresh" button
	would be: nothing is pushed on the undo stack and the project is not
	marked as modified. Elements whose size, hotspot or terminals changed
	are skipped and listed: they must be removed and re-inserted, which
	deletes the conductors already connected to them.

	Before redrawing, the symbols changed in the company or user collection
	are brought into the project's own copies - see
	EmbeddedElementRefresh::bringFromCollections(). That part does change
	the project, so it is undoable and marks it as modified; the redraw that
	follows stays as described above. The elements whose copy has another
	size or hotspot but their terminals take that box first, so that the
	redraw applies it instead of listing them.

	And when some terminal was added, removed or moved, the user is offered
	to reload the project the way opening it does, which applies all of it
	without removing anything - see reloadProjectFromMemory().
*/
void QETDiagramEditor::slot_reloadElementDrawings() {
	reloadElementDrawings(currentProject());
}

/**
	@brief QETDiagramEditor::reloadElementDrawings
	"Recharger les dessins des éléments" for @p project, which needs not be
	the current one: the editor of symbols runs it from the notice it shows
	once a symbol placed in a project is saved (T62).
	@param project : a project open in this editor
*/
void QETDiagramEditor::reloadElementDrawings(QETProject *project)
{
	reloadElementDrawings(project, EmbeddedElementRefresh::editableCollectionDirs(),
						  QString());
}

/**
	@brief QETDiagramEditor::reloadElementDrawings
	The same, limited to the copies at @p only_path and the elements placed
	from them, and to the collections @p collection_dirs: what the notice of
	the editor of symbols runs for the symbol just saved, and nothing else
	(EmbeddedElementRefresh::savedScope()).
	@param project : a project open in this editor
	@param collection_dirs : the collections to bring a newer version from
	@param only_path : the copies to look at; every copy when empty
*/
void QETDiagramEditor::reloadElementDrawings(QETProject *project,
											 const QStringList &collection_dirs,
											 const QString &only_path)
{
	if (!project) return;

		//A symbol changed in the project and in a collection: which version
		//stays is the user's call, and nothing here can make it for them
	QSet<QString> taken_from_collection;
	const QStringList changed_in_both =
			EmbeddedElementRefresh::changedInBoth(project, collection_dirs, only_path);
	if (!changed_in_both.isEmpty())
	{
		QStringList paths;
		for (const QString &path : changed_in_both) {
			paths << EmbeddedElementRefresh::collectionRelativePath(path);
		}
		QMessageBox question(QMessageBox::Question,
							 tr("Recharger les dessins des éléments"),
							 tr("%n élément(s) ont été modifiés dans le projet et dans la"
								" collection. Utiliser la version de la collection ?",
								"", changed_in_both.size()),
							 QMessageBox::NoButton,
							 this);
		QPushButton *collection = question.addButton(tr("Utiliser la collection"),
													  QMessageBox::AcceptRole);
		question.addButton(tr("Garder le projet"), QMessageBox::RejectRole);
		question.setDefaultButton(collection);
		question.setDetailedText(paths.join(QLatin1Char('\n')));
		question.exec();
		if (question.clickedButton() == collection) {
			taken_from_collection = QSet<QString>(changed_in_both.cbegin(),
												  changed_in_both.cend());
		}
	}

		//A terminal added, removed, moved, renamed or turned, or another kind
		//of element, can only be applied by building the elements again, so
		//the project is reloaded - if the user agrees
	const QStringList needing_reload = EmbeddedElementRefresh::needingReload(
				project, collection_dirs, taken_from_collection, only_path);
	if (!needing_reload.isEmpty())
	{
		QMessageBox question(QMessageBox::Question,
							 tr("Recharger les dessins des éléments"),
							 EmbeddedElementRefresh::reloadQuestion(needing_reload.size()),
							 QMessageBox::NoButton,
							 this);
		QPushButton *reload = question.addButton(tr("Recharger"), QMessageBox::AcceptRole);
		question.addButton(tr("Pas maintenant"), QMessageBox::RejectRole);
		question.setDefaultButton(reload);
		question.setDetailedText(needing_reload.join(QLatin1Char('\n')));
		question.exec();
		if (question.clickedButton() == reload) {
			reloadProjectFromMemory(project, collection_dirs, taken_from_collection,
									only_path);
			return;
		}
	}

		//The newer versions and the texts of the placed elements, in one
		//Ctrl+Z (T58)
	EmbeddedElementRefresh::Report brought;
	if (QUndoCommand *update = EmbeddedElementRefresh::updateCommand(
				project, collection_dirs, taken_from_collection, &brought, only_path)) {
		project->undoStack()->push(update);
	}
		//A copy corrected from a folio may come with another size or hotspot,
		//which the redraw below would refuse: the elements take it first
	EmbeddedElementRefresh::adoptBoxes(project, only_path);

	QList<Element *> elements;
	QSet<QString> dropped_locations;
	for (Diagram *diagram : project->diagrams())
	{
		DiagramContent content(diagram, false);
		for (Element *elmt : content.m_elements)
		{
			if (!only_path.isEmpty()
					&& elmt->location().collectionPath(false) != only_path) {
				continue;
			}
			elements << elmt;
			const QString key = elmt->location().toString();
			if (!dropped_locations.contains(key))
			{
				ElementPictureFactory::instance()->dropCache(elmt->location());
				dropped_locations.insert(key);
			}
		}
	}

	int reloaded = 0;
	int unavailable = 0;
	QStringList geometry_changed;
	for (Element *elmt : elements)
	{
		switch (elmt->reloadPicture())
		{
			case Element::ReloadPictureResult::Reloaded:
				++reloaded;
				break;
			case Element::ReloadPictureResult::Unavailable:
				++unavailable;
				break;
			case Element::ReloadPictureResult::GeometryChanged:
			{
				const Diagram *diagram = elmt->diagram();
				const QString folio = diagram
						? tr("sheet %1").arg(project->folioIndex(diagram) + 1)
						: QString();
				geometry_changed << QStringLiteral("%1 (%2)").arg(elmt->name(), folio);
				break;
			}
		}
	}

	QString message = tr("%n elements redrawn.", "", reloaded);

	const QString brought_text = EmbeddedElementRefresh::reportText(brought);
	if (!brought_text.isEmpty()) {
		message = brought_text % QStringLiteral("\n\n") % message;
	}

	if (unavailable) {
		message += QStringLiteral("\n\n")
				% tr("%n elements for which no definition could be found or which "
					  "were illegible: their current design has been retained.", "", unavailable);
	}

	const QStringList brought_details = EmbeddedElementRefresh::reportDetails(brought);

		//From the notice of the editor of symbols: the elements of the symbol
		//saved whose texts were arranged on the folio stayed as they were,
		//on purpose, and the user is offered to bring them too (T62)
	const QList<Element *> kept = only_path.isEmpty()
			? QList<Element *>()
			: EmbeddedElementRefresh::keptOnFolio(project, only_path);
	if (!kept.isEmpty())
	{
		message += QStringLiteral("\n\n")
				% tr("%n element(s) keep texts arranged on the folio, which stayed where they are.",
					 "", kept.size());
		if (EmbeddedElementRefresh::turnsTextsByCorner(kept)) {
			message += QStringLiteral("\n\n")
					% tr("Some are rotated: tick “Rotate about its own centre” on the text"
						 " in the symbol editor, so that the text stays next to them.");
		}
		QStringList lines;
		for (const Element *element : kept) {
			lines << EmbeddedElementRefresh::describe(element);
		}
		QMessageBox box(QMessageBox::Question, tr("Reload element drawings"), message,
						QMessageBox::NoButton, this);
		QPushButton *bring = box.addButton(tr("Bring these too"), QMessageBox::AcceptRole);
		box.addButton(tr("Keep them"), QMessageBox::RejectRole);
		box.setDefaultButton(bring);
		box.setDetailedText((brought_details + geometry_changed + lines).join(QLatin1Char('\n')));
		box.exec();
		if (box.clickedButton() == bring) {
			if (QUndoCommand *command = EmbeddedElementRefresh::bringKeptCommand(project, kept)) {
				project->undoStack()->push(command);
			}
		}
		return;
	}

	if (geometry_changed.isEmpty() && brought_details.isEmpty())
	{
		QET::QetMessageBox::information(
			this, tr("Reload element drawings"), message);
		return;
	}

	if (!geometry_changed.isEmpty())
	{
		message += QStringLiteral("\n\n")
				% tr("%n élément(s) non redessiné(s) : leurs bornes ont changé.",
					  "", geometry_changed.size())
				% QStringLiteral(" ")
				% tr("Relancez la commande et acceptez de recharger le projet.");
	}

		//The full list goes in the expandable, scrollable details area
		//so the dialog stays readable on large projects.
	QMessageBox box(QMessageBox::Warning,
					tr("Reload element drawings"),
					message,
					QMessageBox::Ok,
					this);
	box.setDetailedText((brought_details + geometry_changed).join(QLatin1Char('\n')));
	box.exec();
}

/**
	@brief QETDiagramEditor::resetSymbolTexts
	Give the selected elements the texts of their symbol, as an element just
	placed has them: what was moved, rewritten or restyled on the folio goes
	back to the symbol, and so does a text of the symbol removed from them.
	Their values - label, informations - stay, and so do the texts added on
	the folio. One Ctrl+Z. See SymbolTextSync, in Reset.
*/
void QETDiagramEditor::resetSymbolTexts()
{
	DiagramView *view = currentDiagramView();
	if (!view || !view->diagram() || view->diagram()->isReadOnly()) {
		return;
	}
	QETProject *project = view->diagram()->project();
	const QList<Element *> elements = DiagramContent(view->diagram()).m_elements;
	if (!project || elements.isEmpty()) {
		return;
	}

	auto *texts = new SyncSymbolTextsCommand(EmbeddedElementRefresh::textPlans(
				project, SymbolTextSync::Mode::Reset, elements));
	if (!texts->elements())
	{
		delete texts;
		statusBar()->showMessage(tr("Les textes sont déjà ceux du symbole."), 4000);
		return;
	}
	texts->setText(tr("Rétablir les textes du symbole"));
	project->undoStack()->push(texts);
}

/**
	@brief QETDiagramEditor::reloadProjectFromMemory
	Bring every newer version of the editable collections into @p project,
	the ones whose terminals changed included, and reload it the way opening
	it does - from what it holds in memory, so nothing has to be saved first
	and the file on the disk is untouched. See EmbeddedElementRefresh::
	reopened() for what happens to the conductors.

	The order matters. The symbol editors open on the project are closed
	first, because they may ask to save and the user may cancel: past that
	point nothing can be cancelled. The old view is closed without asking
	and without writing, since its usual closing saves the file according to
	the answer. The reloaded project then takes its place, on the folio that
	was shown. Reopening has the price of reopening: the undo history and
	the windows tied to the old project are gone.
	@param project : the current project
	@param collection_dirs : the editable collections
	@param taken_from_collection : the copies changed in the project and in a
	collection whose collection version the user chose
	@param only_path : the copies to bring and whose elements' texts follow
	their symbol; every copy when empty
*/
void QETDiagramEditor::reloadProjectFromMemory(QETProject *project,
											   const QStringList &collection_dirs,
											   const QSet<QString> &taken_from_collection,
											   const QString &only_path)
{
	ProjectView *view = findProject(project);
	if (!view || !view->closeElementEditors()) {
		return;
	}

	EmbeddedElementRefresh::Report brought = EmbeddedElementRefresh::bringFromCollections(
				project, collection_dirs, true, taken_from_collection, only_path);
		//The texts before the reload: opening a project puts back the texts
		//saved with each element, not those of its symbol (T58)
	const QList<Element *> following = only_path.isEmpty()
			? QList<Element *>()
			: EmbeddedElementRefresh::placedElements(project, only_path);
	auto *texts = new SyncSymbolTextsCommand(
				only_path.isEmpty() || !following.isEmpty()
				? EmbeddedElementRefresh::textPlans(project, SymbolTextSync::Mode::Merge,
													following)
				: QList<SymbolTextSync::Plan>());
	brought.text_elements = texts->elements();
	brought.added_texts = texts->addedTexts();
	brought.changed_texts = texts->changedTexts();
	brought.removed_texts = texts->removedTexts();
	if (texts->isEmpty()) {
		delete texts;
	} else {
		project->undoStack()->push(texts);
	}

	QETProject *reloaded = replaceWithReopened(project, &EmbeddedElementRefresh::reopened,
											   tr("Recharger les dessins des éléments"));
	if (!reloaded) {
		return;
	}

	const QStringList left_out = EmbeddedElementRefresh::wiresLeftOut(reloaded);
	QMessageBox box(left_out.isEmpty() ? QMessageBox::Information : QMessageBox::Warning,
					tr("Recharger les dessins des éléments"),
					EmbeddedElementRefresh::reloadReportText(brought, left_out.size()),
					QMessageBox::Ok,
					this);
	if (!left_out.isEmpty()) {
		box.setDetailedText(left_out.join(QLatin1Char('\n')));
	}
	box.exec();
}

/**
	@brief QETDiagramEditor::replaceWithReopened
	Reload @p project with @p reopen and put the project reloaded in its
	place, on the folio that was shown. What reloadProjectFromMemory() and
	putTerminalsOnWholeNumbers() share; the symbol editors open on the
	project are the caller's to close first (ProjectView::
	closeElementEditors()), since one may ask to save and the user may
	cancel.

	The old view is closed without asking and without writing, since its
	usual closing saves the file according to the answer. Reopening has the
	price of reopening: the undo history and the windows tied to the old
	project are gone, and @p project with them.
	@param project : a project open in this editor
	@param reopen : what reloads it, EmbeddedElementRefresh::reopened() or
	one like it
	@param title : the title of the message shown when it cannot be reloaded
	@return the project reloaded, shown; nullptr when it could not be
	reloaded - @p project then stays open as it was, and the user is told why
*/
QETProject *QETDiagramEditor::replaceWithReopened(QETProject *project,
												  QETProject *(*reopen)(QETProject *, QString *),
												  const QString &title)
{
	ProjectView *view = findProject(project);
	if (!view) {
		return nullptr;
	}
	DiagramView *shown = view->currentDiagram();
	const int folio = shown ? project->diagrams().indexOf(shown->diagram()) : -1;

		//Hidden before it is dropped: dropping it only schedules its
		//deletion, which the message boxes below would never let happen
	DialogWaiting *waiting = DialogWaiting::instance(this);
	const auto drop_waiting = [waiting]() {
		waiting->hide();
		DialogWaiting::dropInstance();
	};
	QString error;
	QETProject *reloaded = reopen(project, &error);
	if (!reloaded)
	{
		drop_waiting();
		QET::QetMessageBox::critical(
					this,
					title,
					tr("Le projet n'a pas pu être rechargé ; il reste ouvert.\n\n%1")
					.arg(error));
		return nullptr;
	}

		//The old view goes first, through the same door as any project
		//closing, so that the panels and QETApp let go of it. The projects
		//panel never disconnects from a project it removes, and any signal
		//the old project sends while it is being let go of puts it back:
		//seen on screen, a second entry for a project about to be deleted
	disconnect(project, nullptr, &pa->elementsPanel(), nullptr);
	view->skipClosingQuestions();
	closeProject(view);
	pa->elementsPanel().projectWasClosed(project);
	addProject(reloaded);
	if (ProjectView *reloaded_view = findProject(reloaded))
	{
		if (Diagram *diagram = reloaded->diagrams().value(folio)) {
			reloaded_view->showDiagram(diagram);
		}
		activateProject(reloaded_view);
	}
	drop_waiting();
	return reloaded;
}

/**
	@brief QETDiagramEditor::putTerminalsOnWholeNumbers
	Put the terminals of the symbols of the current project on whole
	numbers (T72): a terminal at a fraction in its symbol puts the end of
	every wire on it off the grid, and a wire from a terminal in line with
	it runs with a step no grid takes away.

	Every copy of the project's own collection with a terminal off is
	changed, and the elements placed from them follow by reloading the
	project the way opening it does - from memory, with nothing saved and
	nothing to insert again; the wires stay on their terminals and follow
	them (EmbeddedElementRefresh::reopenedOnWholeNumbers()). The user is
	asked first, since the reload clears the undo history, and told
	afterwards how many terminals and symbols changed.
*/
void QETDiagramEditor::putTerminalsOnWholeNumbers()
{
	QETProject *project = currentProject();
	if (!project || project->isReadOnly()) {
		return;
	}
	const QString title = tr("Round the terminals of the symbols");
	const EmbeddedElementRefresh::OffWholeNumbers off =
			EmbeddedElementRefresh::offWholeNumbers(project);

		//A symbol two of whose terminals would meet is left whole, and named
	QStringList merged;
	if (!off.merged.isEmpty())
	{
		merged << tr("Left as they are, two of their terminals would meet:");
		for (const QString &path : off.merged) {
			merged << EmbeddedElementRefresh::collectionRelativePath(path);
		}
	}

	if (off.copies.isEmpty())
	{
		if (merged.isEmpty())
		{
			const QString message = tr("The terminals of the symbols are already on whole numbers",
									   "status bar message");
				//Queued, as for the wire names: closing the menu the action
				//came from would clear a message shown right now
			QTimer::singleShot(0, this, [this, message]() {
				statusBar()->showMessage(message, 8000);
			});
			return;
		}
		QMessageBox box(QMessageBox::Information, title,
						tr("%n symbol(s) left as they are: two of their terminals would meet.",
						   "", off.merged.size()),
						QMessageBox::Ok, this);
		box.setDetailedText(merged.join(QLatin1Char('\n')));
		box.exec();
		return;
	}

	QStringList details;
	for (const QString &path : off.copies) {
		details << EmbeddedElementRefresh::collectionRelativePath(path);
	}
	if (!merged.isEmpty()) {
		details << QString() << merged;
	}
	QMessageBox question(QMessageBox::Question, title,
						 tr("%n terminal(s) of the symbols are off whole numbers. Put them on"
							" whole numbers?", "", off.terminals)
						 % QStringLiteral("\n\n")
						 % tr("The project is reloaded: the undo history is cleared. Nothing is"
							  " saved on the disk."),
						 QMessageBox::NoButton, this);
	QPushButton *put = question.addButton(tr("Put on whole numbers"), QMessageBox::AcceptRole);
	question.addButton(QMessageBox::Cancel);
	question.setDefaultButton(put);
	question.setDetailedText(details.join(QLatin1Char('\n')));
	question.exec();
	if (question.clickedButton() != put) {
		return;
	}

	ProjectView *view = findProject(project);
	if (!view || !view->closeElementEditors()) {
		return;
	}
	QETProject *reloaded = replaceWithReopened(
				project, &EmbeddedElementRefresh::reopenedOnWholeNumbers, title);
	if (!reloaded) {
		return;
	}

	QString message = tr("%n terminal(s) put on whole numbers, in %1 symbol(s).", "", off.terminals)
			.arg(off.copies.size());
	const QStringList left_out = EmbeddedElementRefresh::wiresLeftOut(reloaded);
	if (!left_out.isEmpty()) {
		message += QStringLiteral("\n\n")
				% tr("%n conductor(s) left out: their terminal was not found.", "", left_out.size());
	}
	QMessageBox box(left_out.isEmpty() ? QMessageBox::Information : QMessageBox::Warning,
					title, message, QMessageBox::Ok, this);
	if (!left_out.isEmpty()) {
		box.setDetailedText(left_out.join(QLatin1Char('\n')));
	}
	box.exec();
}

#ifdef QET_HAS_SCRIPTING
/**
	@brief QETDiagramEditor::slot_runScript
	Run a JavaScript macro against the current project (bugtracker #162).
	See QetScriptApi for what a script can do -- read-mostly: folio/element/
	conductor counts and the same export operations the --export-* CLI
	flags provide. Export/save calls inside the script act on this
	project's file on disk, so unsaved edits in the open editor are not
	visible to the script; save first if that matters.
*/
void QETDiagramEditor::slot_runScript() {
	QETProject *project = currentProject();
	if (!project) return;

	if (!ensureScriptingEnabled(tr("Run a script"))) return;

	const QString script_path = QFileDialog::getOpenFileName(
		this,
		tr("Run a script"),
		QString(),
		tr("JavaScript scripts (*.js);;All files (*)")
	);
	if (script_path.isEmpty()) return;

	QetScripting::runOnProject(script_path, project, currentDiagramView());
}

/**
	@brief QETDiagramEditor::ensureScriptingEnabled
	@return true if scripts may run, asking to switch them on if they are
	off. Shared by "Run a script..." and the stored script buttons, so
	there is one prompt to keep right.
*/
bool QETDiagramEditor::ensureScriptingEnabled(const QString &title)
{
	// Scripting is off until somebody says otherwise, so the first use has
	// to ask. Asking here rather than greying the action out keeps the
	// feature discoverable: a disabled menu entry tells a user that
	// something exists and nothing about how to have it.
	if (!QetSettings::scriptingEnabled()) {
		const QMessageBox::StandardButton answer = QET::QetMessageBox::question(
			this,
			title,
			tr("Scripts are disabled.\n"
			   "\n"
			   "A script runs with your permissions: it can read and "
			   "modify the open project and write files. Only run "
			   "scripts whose origin you know.\n"
			   "\n"
			   "Enable scripts? This setting can be changed in "
			   "Configure QElectroTech > General > Projects."),
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (answer != QMessageBox::Yes) {
			return false;
		}
		QetSettings::setScriptingEnabled(true);
	}
	return true;
}

/**
	@brief QETDiagramEditor::runStoredScript
	Run the stored script at @a path on the current project, as one undo
	step named after the script. The file is read again on every run, so
	an edit to it takes effect on the next click.
*/
void QETDiagramEditor::runStoredScript(const QString &path, const QString &name)
{
	QETProject *project = currentProject();
	if (!project) return;
	if (!ensureScriptingEnabled(name)) return;
	QetScripting::runOnProject(path, project, currentDiagramView(), name);
}

/**
	@brief QETDiagramEditor::rebuildScriptActions
	One action per stored script, in the Projet > Scripts menu and on the
	Scripts toolbar, registered with ShortcutManager under
	diagrameditor.script.<file name>: that one registration is what lists
	it in the shortcut settings, the shortcut bar (S) and command search.
	Rebuilt from scratch whenever the scripts folder changes.
*/
void QETDiagramEditor::rebuildScriptActions()
{
	for (QAction *action : std::as_const(m_script_actions)) {
		ShortcutManager::instance().unregisterAction(
			action, action->property("qet_script_action_id").toString());
		delete action;
	}
	m_script_actions.clear();
	m_scripts_menu->clear();
	m_scripts_tool_bar->clear();

	const ScriptLibrary &library = ScriptLibrary::instance();
	for (const ScriptLibrary::Script &script : library.scripts()) {
		const ScriptHeader &h = script.header;
		auto *action = new QAction(ScriptLibrary::icon(script), h.name, this);
		action->setStatusTip(h.tooltip);
		action->setToolTip(h.tooltip.isEmpty() ? h.name : h.name + QLatin1Char('\n') + h.tooltip);
		action->setProperty("qet_script_action_id", ScriptLibrary::actionId(h.id));
		action->setProperty("qet_script_context", h.context);
		connect(action, &QAction::triggered, this, [this, path = script.path, name = h.name]() {
			runStoredScript(path, name);
		});
		ShortcutManager::instance().registerAction(action, ScriptLibrary::actionId(h.id),
							   tr("Scripts"),
							   QKeySequence::fromString(h.shortcut));
		m_scripts_menu->addAction(action);
		m_scripts_tool_bar->addAction(action);
		m_script_actions << action;
	}

		//A script with a header that cannot be used gets no button: say
		//which file and why, where its author will look for the button.
	const QStringList errors = library.errors();
	if (!errors.isEmpty()) {
		m_scripts_menu->addSeparator();
		for (const QString &error : errors) {
			QAction *ignored = m_scripts_menu->addAction(QET::Icons::DialogInformation,
								     tr("Ignored: %1").arg(error));
			ignored->setEnabled(false);
		}
	}

	m_scripts_menu->addSeparator();
	m_scripts_menu->addAction(m_record_macro);
	m_scripts_menu->addAction(m_manage_scripts);
	m_scripts_menu->addAction(m_run_script);
	m_scripts_menu->addAction(m_open_scripts_folder);

		//Show the toolbar when the first script arrives, hide it when the
		//last one goes; in between it is the user's to show or hide.
	const bool has_scripts = !m_script_actions.isEmpty();
	if (has_scripts != m_had_scripts) m_scripts_tool_bar->setVisible(has_scripts);
	m_had_scripts = has_scripts;

	rebuildToolBars();
	updateScriptActions();
}

/**
	@brief QETDiagramEditor::setUpMacroRecorder
	While recording: the action checked, and "● Enregistrement : N étapes"
	with Arrêter on the status bar. At the end, where it was saved and the
	request to paste into the assistant.
*/
void QETDiagramEditor::setUpMacroRecorder()
{
	auto *box = new QWidget(this);
	auto *layout = new QHBoxLayout(box);
	layout->setContentsMargins(0, 0, 0, 0);
	auto *label = new QLabel(box);
	label->setStyleSheet(QStringLiteral("color: %1; font-weight: bold")
		.arg(QET::Palette::readable(QColor(0xd0, 0x30, 0x30), palette()).name()));
	auto *stop = new QToolButton(box);
	stop->setText(tr("Stop"));
	stop->setToolTip(tr("Stop recording the macro"));
	layout->addWidget(label);
	layout->addWidget(stop);
	statusBar()->addPermanentWidget(box);
	box->hide();
	connect(stop, &QToolButton::clicked, this, []() { MacroRecorder::instance().stop(); });

	connect(&MacroRecorder::instance(), &MacroRecorder::stateChanged, box,
		[this, box, label](bool recording, int steps) {
		box->setVisible(recording);
		label->setText(tr("● Recording: %n steps", nullptr, steps));
		const QSignalBlocker blocker(m_record_macro);
		m_record_macro->setChecked(recording);
		updateScriptActions();
	});
	connect(&MacroRecorder::instance(), &MacroRecorder::finished, this,
		[this](const QJsonObject &recording, QETDiagramEditor *editor) {
		if (editor == this) macroRecorded(recording);
	});
}

void QETDiagramEditor::macroRecorded(const QJsonObject &recording)
{
	const int steps = recording.value(QStringLiteral("steps")).toArray().size();
	const QString dir = recording.value(QStringLiteral("folder")).toString();
	QMessageBox box(QMessageBox::Information, tr("Macro recorded"),
			tr("“%1”: %n steps.\n"
			   "\n"
			   "To make a script of it, ask your AI assistant: the button below "
			   "copies the request, just paste it into its window.", nullptr, steps)
			.arg(recording.value(QStringLiteral("name")).toString()),
			QMessageBox::NoButton, this);
	box.setInformativeText(QDir::toNativeSeparators(dir)
			       + (recording.value(QStringLiteral("complete")).toBool()
				  ? QString() : QStringLiteral("\n") + recording.value(QStringLiteral("note")).toString()));
	QPushButton *copy = box.addButton(tr("&Copy the request for the assistant"), QMessageBox::AcceptRole);
	QPushButton *open = box.addButton(tr("&Open the folder"), QMessageBox::ActionRole);
	box.addButton(tr("&Close"), QMessageBox::RejectRole);
	box.setDefaultButton(copy);
	box.exec();
	if (box.clickedButton() == copy) {
		QGuiApplication::clipboard()->setText(MacroRecorder::assistantRequest(recording));
		statusBar()->showMessage(tr("Request copied: paste it into the assistant's window"), 8000);
	} else if (box.clickedButton() == open) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
	}
}

/**
	@brief QETDiagramEditor::setUpLiveIndicator
	"Assistant connecté" and a Stop button on the status bar, shown
	whenever live mode is open, so it is never on without being seen.
*/
void QETDiagramEditor::setUpLiveIndicator()
{
	auto *box = new QWidget(this);
	auto *layout = new QHBoxLayout(box);
	layout->setContentsMargins(0, 0, 0, 0);
	auto *label = new QLabel(box);
	auto *stop = new QToolButton(box);
	stop->setText(tr("Stop"));
	stop->setToolTip(tr("Close the assistant's connection for the rest of the session"));
	layout->addWidget(label);
	layout->addWidget(stop);
	statusBar()->addPermanentWidget(box);
	connect(stop, &QToolButton::clicked, this, []() { LiveServer::instance().stop(); });

	auto update = [box, label](LiveServer::State state) {
		box->setVisible(state != LiveServer::Off);
		label->setText(state == LiveServer::Connected
			       ? tr("Live mode: assistant connected")
			       : tr("Live mode: waiting for an assistant"));
		label->setStyleSheet(state == LiveServer::Connected
				     ? QStringLiteral("font-weight: bold") : QString());
	};
	connect(&LiveServer::instance(), &LiveServer::stateChanged, box, update);
	update(LiveServer::instance().state());

		//The Assistant dock: every action, and whether to be asked first
	auto *dock = new QDockWidget(tr("Assistant"), this);
	dock->setObjectName(QStringLiteral("assistant_dock"));
	auto *content = new QWidget(dock);
	auto *dock_layout = new QVBoxLayout(content);
	auto *ask = new QCheckBox(tr("Ask before running a script the assistant wrote"), content);
	ask->setChecked(LiveServer::instance().askFirst());
	ask->setToolTip(tr("Remembered after a restart"));
	auto *log = new QListWidget(content);
	log->setWordWrap(true);
	dock_layout->addWidget(ask);
	dock_layout->addWidget(log);
	dock->setWidget(content);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	dock->hide();
	connect(ask, &QCheckBox::toggled, &LiveServer::instance(), &LiveServer::setAskFirst);
	connect(&LiveServer::instance(), &LiveServer::askFirstChanged, ask, &QCheckBox::setChecked);
	connect(&LiveServer::instance(), &LiveServer::stateChanged, dock,
		[dock](LiveServer::State state) {
		if (state == LiveServer::Waiting) dock->show();
		else if (state == LiveServer::Off) dock->hide();
	});
	connect(&LiveServer::instance(), &LiveServer::handled, log,
		[log](const QJsonObject &request, const QJsonObject &answer) {
		const QString cmd = request.value(QStringLiteral("cmd")).toString();
		if (cmd == QLatin1String("status")) return;
		const bool ok = answer.value(QStringLiteral("ok")).toBool();
		QString what = answer.value(QStringLiteral("undo")).toString();
		for (const char *key : {"name", "script", "action"})
			if (what.isEmpty()) what = request.value(QLatin1String(key)).toString();
		if (what.isEmpty()) what = cmd;
		auto *item = new QListWidgetItem(QStringLiteral("%1  %2  %3")
			.arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
			     ok ? QStringLiteral("✓") : QStringLiteral("✗"), what));
		QStringList details;
		if (!ok) details << answer.value(QStringLiteral("error")).toString();
		const QJsonArray lines = answer.value(QStringLiteral("log")).toArray();
		for (const QJsonValue &l : lines) details << l.toString();
		const QString source = request.value(QStringLiteral("source")).toString();
		if (!source.isEmpty()) details << QString() << source;
		item->setToolTip(details.join(QLatin1Char('\n')));
		item->setData(Qt::UserRole, details.join(QLatin1Char('\n')));
		log->insertItem(0, item);
	});
	connect(log, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
		QET::QetMessageBox::information(this, tr("Assistant"),
						item->data(Qt::UserRole).toString());
	});
		//What the assistant just did, where the user is already looking
	connect(&LiveServer::instance(), &LiveServer::handled, box,
		[label](const QJsonObject &request, const QJsonObject &answer) {
		const QString cmd = request.value(QStringLiteral("cmd")).toString();
		if (cmd == QLatin1String("status")) return;
		QString what = answer.value(QStringLiteral("undo")).toString();
		if (what.isEmpty())
			what = request.value(QStringLiteral("name")).toString();
		if (what.isEmpty())
			what = request.value(QStringLiteral("script")).toString();
		if (what.isEmpty())
			what = request.value(QStringLiteral("action")).toString();
		if (what.isEmpty())
			what = cmd;
		label->setText(tr("Live mode: %1 %2 at %3")
			       .arg(answer.value(QStringLiteral("ok")).toBool() ? QStringLiteral("✓")
										  : QStringLiteral("✗"),
				    what, QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
		label->setToolTip(answer.value(QStringLiteral("error")).toString());
	});
}

/**
	@brief QETDiagramEditor::updateScriptActions
	Enable each script for what its header's @context asks for: always
	(canvas), with something selected (selection), or with a conductor
	selected (conductor). None without an open project.
*/
void QETDiagramEditor::updateScriptActions()
{
	DiagramView *dv = currentDiagramView();
	Diagram *diagram = dv ? dv->diagram() : nullptr;
	const bool selection = diagram && !diagram->selectedItems().isEmpty();
	const bool conductor = diagram && !diagram->selectedConductors().isEmpty();
	m_record_macro->setEnabled(currentProject() || MacroRecorder::instance().isRecording());
	for (QAction *action : std::as_const(m_script_actions)) {
		const QString context = action->property("qet_script_context").toString();
		bool enabled = currentProject() != nullptr;
		if (context == QLatin1String("selection")) enabled = enabled && selection;
		else if (context == QLatin1String("conductor")) enabled = enabled && conductor;
		action->setEnabled(enabled);
	}
}
#endif

/**
	@brief QETDiagramEditor::updateShownKinds
	Tick the kinds shown in View > Show, and say in the status bar how many
	are hidden, so that a hidden kind is never forgotten.
*/
void QETDiagramEditor::updateShownKinds()
{
	for (QAction *action : m_shown_kinds_menu->actions())
		action->setChecked(ShownKinds::isShown(ShownKinds::Kind(action->data().toInt())));

	const int hidden = ShownKinds::hiddenCount();
	m_hidden_kinds_label->setText(tr("%n kinds of items hidden", "", hidden));
	m_hidden_kinds_label->setVisible(hidden > 0);
}

/**
	@brief QETDiagramEditor::updateTextGridButton
	Show the current text grid on its toolbar button and check it in its menu.
*/
void QETDiagramEditor::updateTextGridButton()
{
	const qreal divisor = QSettings().value(TextGrid::settings_key, 1).toReal();
	for (QAction *action : m_text_grid_menu->actions())
	{
		if (qFuzzyCompare(action->data().toReal() + 1, divisor + 1))
		{
			action->setChecked(true);
			m_text_grid_button->setText(tr("Texts %1").arg(action->text()));
			return;
		}
	}
		//A divisor the menu does not offer, set by hand in the config file
	m_text_grid_button->setText(tr("Texts %1").arg(TextGrid::ratioLabel(divisor)));
}
/**
 * @brief QETDiagramEditor::copyTextFormat
 * Copies the text/label layout and formatting from the currently selected element as a template.
 */
void QETDiagramEditor::copyTextFormat()
{
	DiagramView *dv = currentDiagramView();
	if (!dv || !dv->diagram()) return;

	Element *sourceElmt = nullptr;
	for (QGraphicsItem *item : dv->diagram()->selectedItems()) {
		if (Element *elmt = qgraphicsitem_cast<Element *>(item)) {
			sourceElmt = elmt;
			break;
		}
	}

	if (!sourceElmt) return;

	ElementTextFormatPattern pattern = ElementTextFormatPattern::fromElement(sourceElmt);
	if (pattern.isValid()) {
		ElementTextFormatPattern::setCurrentPattern(pattern);
		slot_updateComplexActions();
		statusBar()->showMessage(tr("Text format of \"%1\" copied").arg(sourceElmt->name()), 3000);
	}
}

/**
 * @brief QETDiagramEditor::applyTextFormat
 * Applies the stored text formatting template to all selected elements in the active diagram.
 */
/**
	@brief QETDiagramEditor::placeSymbolTexts
	Place the texts of the selected symbols as one block at @a where,
	one of nine points around each symbol, in one undo step.
	@param where : see SymbolTextPlacement::place()
*/
void QETDiagramEditor::placeSymbolTexts(Qt::Alignment where)
{
	DiagramView *dv = currentDiagramView();
	if (!dv || !dv->diagram() || dv->diagram()->isReadOnly()) return;

	QList<Element *> elements;
	for (QGraphicsItem *item : dv->diagram()->selectedItems())
		if (Element *element = qgraphicsitem_cast<Element *>(item))
			elements.append(element);
	if (elements.isEmpty()) return;

	if (QUndoCommand *undo = SymbolTextPlacement::place(elements, where))
		dv->diagram()->undoStack().push(undo);
}

/**
	@brief QETDiagramEditor::scaleElements
	Draw @a elements at @a scale on the folio, their symbol as it is, in
	one undo step (T83). See ScaleElementsCommand.
	@param elements : elements of the current folio
	@param scale : 1 for the symbol as it is drawn
*/
void QETDiagramEditor::scaleElements(const QList<Element *> &elements, qreal scale)
{
	DiagramView *dv = currentDiagramView();
	if (!dv || !dv->diagram() || dv->diagram()->isReadOnly() || elements.isEmpty()) return;

	auto *undo = new ScaleElementsCommand(elements, scale);
	if (undo->isValid()) {
		dv->diagram()->undoStack().push(undo);
	} else {
		delete undo;
	}
}

void QETDiagramEditor::applyTextFormat()
{
	if (!ElementTextFormatPattern::hasPattern()) return;

	DiagramView *dv = currentDiagramView();
	if (!dv || !dv->diagram()) return;
	if (dv->diagram()->isReadOnly()) return;

	QList<Element *> targetElements;
	for (QGraphicsItem *item : dv->diagram()->selectedItems()) {
		if (Element *elmt = qgraphicsitem_cast<Element *>(item)) {
			targetElements.append(elmt);
		}
	}

	if (targetElements.isEmpty()) return;

	const ElementTextFormatPattern &pattern = ElementTextFormatPattern::currentPattern();
	dv->diagram()->undoStack().push(new ApplyElementTextFormatCommand(targetElements, pattern));

	statusBar()->showMessage(tr("Text format applied to %n element(s)", "", targetElements.size()), 3000);
}
