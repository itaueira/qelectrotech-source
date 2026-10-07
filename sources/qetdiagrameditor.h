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
#ifndef QET_DIAGRAM_EDITOR_H
#define QET_DIAGRAM_EDITOR_H

#include "ElementsCollection/collectionloadgate.h"
#include "SearchAndReplace/ui/searchandreplacewidget.h"
#include "diagramcontext.h"
#include "qetmainwindow.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QMdiArea>
#include <QPointer>
#include <QSignalMapper>
#include <QUndoGroup>

class QToolButton;
class QLabel;

class QMdiSubWindow;
class QETProject;
class QETResult;
class ProjectView;
class ConductorColorToolButton;
class CustomElement;
class DiagramBgColorToolButton;
class Diagram;
class DiagramView;
class Element;
class ElementsPanelWidget;
class ElementsLocation;
class RecentFiles;
class DiagramPropertiesEditorDockWidget;
class ElementsCollectionWidget;
class ElementPickerPopup;
class CommandSearchPopup;
class AutoNumberingDockWidget;
class TerminalNumberingDialog;

class KAutoSaveFile;
/**
	This class represents the main window of the QElectroTech diagram editor and,
	ipso facto, the most important part of the QElectroTech user interface.
*/
class QETDiagramEditor : public QETMainWindow
{
	Q_OBJECT

        friend class TerminalStripEditorWindow;
        friend class LiveServer;
        friend class MacroRecorder;
	
	public:
		QETDiagramEditor(
				const QStringList & = QStringList(),
				QWidget * = nullptr);
		~QETDiagramEditor() override;
		void                 closeEvent        (QCloseEvent *) override;
		QList<ProjectView *> openedProjects    () const;
		void                 addProjectView    (ProjectView *);
		bool                 openAndAddProject (const QString &, bool = true);
		QList<QString>       editedFiles       () const;
		ProjectView         *viewForFile       (const QString &) const;
		ProjectView *currentProjectView() const;
		QETProject *currentProject() const;
		void templateSaved(const ElementsLocation &location);
		bool drawGrid() const;

		/**
			The components a Navigate over the selection of @a diagram would
			take the reader to, each named once and never the selection itself.

			Static, and asking nothing of a window, because what is navigable
			is a question about a folio: the editor only decides what to do
			with the answer. It is also what lets the rule be read without
			opening a main window, which no test can do.

			A component answers with the components it is linked to - a master
			with its contacts, a contact with its coil, a folio arrow with the
			arrow at the other end. A selected label answers for the component
			carrying it: clicking a cross reference selects the text and not
			the component under it, so refusing there would mean the command
			works everywhere except on the very thing it is named after.
		*/
		static QList<Element *> navigationTargets(Diagram *diagram);

	  protected:
		bool event(QEvent *) override;
	private:
		// Declared first so it is initialised before any member whose
		// constructor may dispatch a Qt event calling event() (e.g. the
		// QActionGroup members below trigger QObject::setParent events).
		bool m_first_show = true;
		// Same reason, and the same place: event() reads it too. It decides
		// when the elements collection may start loading -- see T42, and the
		// comment on startElementsCollectionLoad().
		CollectionLoadGate m_collection_load_gate;

		QETDiagramEditor(const QETDiagramEditor &);
		void setUpElementsPanel ();
		void setUpElementsCollectionWidget();
		void setUpUndoStack     ();
		void setUpSelectionPropertiesEditor();
		void setUpAutonumberingWidget();
		void setUpActions       ();
		void setUpToolBar       ();
		void setUpMenu          ();
		
		bool addProject(QETProject *, bool = true);
		void startElementsCollectionLoad();
		DiagramView *currentDiagramView() const;
		Element *currentElement() const;
		ProjectView *findProject(DiagramView *) const;
		ProjectView *findProject(Diagram *) const;
		ProjectView *findProject(QETProject *) const;
		ProjectView *findProject(const QString &) const;
		QMdiSubWindow *subWindowForWidget(QWidget *) const;
		void updateUsageTrackersActiveState();
		void updateWindowModifiedState();

	signals:
		void syncElementsPanel();

	public slots:
		void save();
		void saveAs();
		bool newProject();
		/**
			Create a new project out of a template project file, chosen by
			the user. The new project is not attached to the template: see
			ProjectTemplate::detachFromFile().
		*/
		bool newProjectFromTemplate();
		bool openProject();
		bool openRecentFile(const QString &);
		bool closeProject(ProjectView *);
		bool closeProject(QETProject *);
		void zoomGroupTriggered      (QAction *action);
		void selectGroupTriggered    (QAction *action);
		void addItemGroupTriggered   (QAction *action);
		void selectionGroupTriggered (QAction *action);
		void alignGroupTriggered     (QAction *action);
		void rowColumnGroupTriggered (QAction *action);
		void slot_updateActions();
		void slot_updateUndoStack();
		void slot_updateModeActions();
		void slot_updateComplexActions();
		void slot_updatePasteAction();
		void pasteElementInformations();
		void updatePasteElementInfoAction();
		void slot_updateWindowsMenu();
		void slot_updateAutoNumDock();
		void insertElementFromCollection(const ElementsLocation &location);
		void insertLastElement();
		void rememberPlacedElement(const ElementsLocation &location);
		void showElementPicker();
		void showShortcutBar();
		void rebuildToolBars();
		bool repeatLastCommand();
		void generateTerminalBlock();
		void setWindowedMode();
		void setTabbedMode();
		void readSettings();
		void readSettingsState();
		void writeSettings();
		void activateProject(QETProject *);
		void activateProject(ProjectView *);
		void activateWidget(QWidget *);
		void projectWasClosed(ProjectView *);
		void editProjectProperties(ProjectView *);
		void editProjectProperties(QETProject *);
		void slot_terminalNumbering();
		void slot_reloadElementDrawings();
#ifdef QET_HAS_SCRIPTING
		void slot_runScript();
		void rebuildScriptActions();
		void updateScriptActions();
		bool ensureScriptingEnabled(const QString &title);
		void runStoredScript(const QString &path, const QString &name);
		void setUpLiveIndicator();
		void setUpMacroRecorder();
		void macroRecorded(const QJsonObject &recording);
#endif
		void editDiagramProperties(DiagramView *);
		void editDiagramProperties(Diagram *);
		void addDiagramToProject(QETProject *);
		void addDiagramToProjectAt(QETProject *, int);
		void removeDiagram(Diagram *);
		void removeDiagrams(const QList<Diagram *> &diagrams);
		void removeDiagramFromProject();
		void moveDiagramUp(const QList<Diagram *> &diagrams);
		void moveDiagramDown(const QList<Diagram *> &diagrams);
		void moveDiagramUpTop(const QList<Diagram *> &diagrams);
		void moveDiagramUpx10(const QList<Diagram *> &diagrams);
		void moveDiagramDownx10(const QList<Diagram *> &diagrams);
		void moveDiagramUpx100(const QList<Diagram *> &diagrams);
		void moveDiagramDownx100(const QList<Diagram *> &diagrams);
		void reloadOldElementPanel();
		void diagramWasAdded(DiagramView *);
		void findElementInPanel(const ElementsLocation &);
		void showError(const QETResult &);
		void showError(const QString &);
		void subWindowActivated(QMdiSubWindow *subWindows);

	private slots:
		void updateTextGridButton();
		void updateShownKinds();
		void selectionChanged();
		void assignCatalogPart();
		void registerCatalogPart();
		void renumberComponents();
		void createSymbolFromSelection();
		void generatePinoutBlocks();
		void generateCircuits();
		void importIoSheet();
		void assignIoPoints();
		void showIoList();
		void showLocationManager();
		void showLocationReport();
		void showLocationBom();
		void showMountingLayout();
		void showSelectionOnPlate();
		void putMountingLayoutOnFolio();
		void goToElement(Element *element);
		void insertNomenclature(const QString &query);
		void markIoBus();
		void wireIoCommons();
		void saveSelectionAsGroup();
		void insertGroup();
		void explodeSelection();
		void setConductorTextVisible(bool visible);
		void alignConductorTexts();
		void navigateToReference();

	public:
		QAction
		*m_edit_diagram_properties, ///< Show a dialog to edit diagram properties
		*m_conductor_reset,         ///< Reset paths of selected conductors
		*m_cut,                     ///< Cut selection to clipboard
		*m_copy,                    ///< Copy selection to clipboard
			//Public for the same reason as the four above, and for no other:
			//DiagramView::contextMenuActions() offers it on the folio. This
			//short public block is the list of actions the context menu is
			//allowed to reach - everything else stays private.
		*m_catalog_assign = nullptr,///< Assign a catalog part to the selected components
		*m_show_on_plate = nullptr, ///< Show the selected element on its mounting plate
		*m_navigate = nullptr,      ///< Go to the other representation of the selected object
		*m_paste_element_info,      ///< Copy the information of the copied element onto the selected elements (#1375)
		*m_insert_last_element = nullptr; ///< Place the last placed element again
		
		QActionGroup
		m_row_column_actions_group, /// Action related to add/remove rows/column in diagram
		m_selection_actions_group,  ///Action related to edit a selected item
		m_align_actions_group,      ///Action related to align the selected items
		*m_depth_action_group = nullptr;

		QMenu
		*m_add_item_menu = nullptr,   ///< Submenu of m_add_item_actions_group
		*m_align_menu = nullptr,      ///< Submenu of m_align_actions_group
		*m_row_column_menu = nullptr; ///< Submenu of m_row_column_actions_group
	
	private:
		QActionGroup
		*grp_visu_sel,            ///< Action group for visualisation vs edition mode
		*m_group_view_mode,       ///< Action group for project
		m_add_item_actions_group, ///Action related to adding (add text image shape...)
		m_zoom_actions_group,     ///Action related to zoom for diagram
		m_select_actions_group,   ///Action related to global selections
		m_file_actions_group;     ///Actions related to file (open, close, save...)
		
		QAction
		*m_tabbed_view_mode,		///< Display projects as tabs
		*m_windowed_view_mode,		///< Display projects as windows
		*m_mode_selection,		///< Set edition mode
		*m_mode_visualise,		///< Set visualisation mode
		*m_export_to_images,		///< Export diagrams of the current project as imagess
		*m_export_to_pdf = nullptr, ///< Export project to pdf.
		*m_print,			///< Print diagrams of the current project
		*m_quit_editor,			///< Quit the diagram editor
		*undo,				///< Cancel the latest action
		*redo,				///< Redo the latest cancelled operation
		*m_paste,			///< Paste clipboard content on the current diagram
		*m_paste_origin,		///< Same, at the copied position, cursor warped to the origin (Ctrl+Shift+V)
		*m_duplicate,			///< Copy selection, offset by the configured step (#991)
		*m_configure_duplicate,		///< Reopen the duplicate offset/direction dialog (#991)
		*m_auto_conductor,		///< Enable/Disable the use of auto conductor
		*m_auto_break_conductor,	///< Enable/Disable the use of auto break conductor
		*m_draw_grid,			///< Switch the background grid display or not
		*m_draw_guides = nullptr,	///< Switch the custom guides display or not
		*m_cell_rulers = nullptr,	///< Keep the folio column/row headers in sight or not
		*m_cell_lines = nullptr,	///< Draw the folio column/row limits across the drawing or not
		*m_project_edit_properties,	///< Edit the properties of the current project.
		*m_project_add_diagram,		///< Add a diagram to the current project.
		*m_remove_diagram_from_project,	///< Delete a diagram from the current project
		*m_clean_project,		///< Clean the content of the current project by removing useless items
		*m_csv_export,			///< generate nomenclature
		*m_add_nomenclature,		///< Add nomenclature graphics item;
		*m_add_summary,			///<Add summary graphics item
		*m_terminal_strip_dialog = nullptr, ///<Launch terminal strip dialog
		*m_project_terminalBloc,	///< generate terminal block
		*m_project_export_conductor_num,///<Export the wire num to csv
		*m_project_export_wiring_list, ///< Action to export the wiring list
		*m_project_wiring_list_view,   ///< Action to show the wiring list read from the project database
		*m_terminal_numbering,         ///< Action to launch terminal numbering
		*m_catalog_manager = nullptr,   ///< Action to open the catalog classes and properties
		*m_catalog_browse = nullptr,    ///< Action to browse the catalog parts
		*m_catalog_register = nullptr,  ///< Action to save the selected components as a catalog part
		*m_catalog_missing = nullptr,   ///< Action to report the components with no part
		*m_catalog_no_physical_view = nullptr, ///< Action to report the parts nobody measured
		*m_connector_check = nullptr,   ///< Action to check the connectors of the project
		*m_drc_check = nullptr,         ///< Action to run the design rule check on the project
		*m_environment = nullptr,       ///< Action to see and change the work environment
		*m_catalog_import = nullptr,    ///< Action to import parts from a spreadsheet
		*m_catalog_repository = nullptr,///< Action to browse the shared part repository
		*m_renumber_components = nullptr,///< Action to renumber the components, with a preview
		*m_create_symbol = nullptr,     ///< Action to turn the selected drawing into a symbol
		*m_generate_pinout = nullptr,  ///< Action to draw the blocks of a part from its pinout
		*m_generate_circuits = nullptr, ///< Action to draw a whole table of circuits in one go
		*m_import_io_sheet = nullptr,   ///< Action to bring a sheet of I/O points into the project
		*m_assign_io_points = nullptr,  ///< Action to place I/O points in the channels of a card
		*m_io_list = nullptr,           ///< Action to read the I/O of the project and of its cards
		*m_mark_io_bus = nullptr,      ///< Action to mark the selection as a default bar for the commons
		*m_wire_io_commons = nullptr,  ///< Action to wire the commons of the cards onto those bars
		*m_save_group = nullptr,        ///< Action to file the selection as a graphic grouping
		*m_insert_group = nullptr,      ///< Action to insert a graphic grouping
		*m_iec_structure = nullptr,     ///< Action to turn the IEC 81346 structure on for this project
		*m_assembly_state = nullptr,    ///< Action to say the panel is wired, and to take that back
	*m_location_manager = nullptr,  ///< Action to manage the enclosures of the project
		*m_location_report = nullptr,   ///< Action to list the components that are nowhere
		*m_location_bom = nullptr,      ///< Action to list what to pick for one enclosure
		*m_mounting_layout = nullptr,   ///< Action to lay the panel of an enclosure out, in millimetre
		*m_put_mounting_layout = nullptr, ///< Action to pose a view of a mounting plate on the current folio
		*m_explode_element = nullptr,   ///< Action to turn a placed symbol back into drawing
		*m_replace_part = nullptr,      ///< Action to swap a catalog part across the project
		*m_show_fine_grid = nullptr,    ///< Action to draw the finer grid
		*m_show_terminals = nullptr,    ///< Action to keep the connection points visible
		*m_show_empty_fields = nullptr, ///< Action to show the text fields that have no value
		*m_show_conductor_text = nullptr,///< Action to show the number of the selected conductors
		*m_hide_conductor_text = nullptr,///< Action to hide it
		*m_align_conductor_text = nullptr,///< Action to line the conductor numbers up on one axis
		*m_link_accessory = nullptr,    ///< Action to say which component an accessory belongs to
		*m_reload_element_drawings,    ///< Action to redraw every placed element from its current definition
#ifdef QET_HAS_SCRIPTING
		*m_run_script,                 ///< Action to run a JavaScript macro against the current project
		*m_open_scripts_folder,        ///< Action to open the folder stored scripts are read from
		*m_manage_scripts,             ///< Action to open the script manager
		*m_record_macro,               ///< Action to start / stop recording a macro
#endif
		*m_export_project_db,		///Export to file the internal database of the current project
		*m_tile_window,			///< Show MDI subwindows as tile
		*m_cascade_window,		///< Show MDI subwindows as cascade
		*m_previous_window,		///< Switch to the previous document
		*m_next_window,			///< Switch to the next document
		*m_edit_selection,		///< To edit selected item
		*m_delete_selection,		///< Delete selection
		*m_rotate_selection,		///< Rotate selected elements and text items by 90 degrees
		*m_rotate_group_selection = nullptr, ///< Rotate the selection as a whole around its shared center, instead of each item in place
		*m_rotate_texts,		///< Direct selected text items to a specific angle
		*m_mirror_horizontal = nullptr, ///< Mirror the selected elements, left and right swap (#1335)
		*m_mirror_vertical = nullptr,   ///< Mirror the selected elements, top and bottom swap (#1335)
		*m_find_element,		///< Find the selected element in the panel
		*m_group_selected_texts = nullptr,
		*m_group_selection = nullptr,   ///< Group the selected items (#1070)
		*m_ungroup_selection = nullptr, ///< Ungroup the selected groups
		*m_close_file,			///< Close current project file
		*m_save_file,			///< Save current project
		*m_save_file_as,		///< Save current project as a specific file
		*m_find = nullptr,
		*m_new_from_template = nullptr,	///< Start a new project from a template project
		*m_jump_to_element = nullptr;	///< Open the "jump to element" quick-open popup

		///< One-click conductor colour, in the "Schéma" toolbar
		ConductorColorToolButton *m_conductor_color_button = nullptr;
		///< Diagram background color picker, in the "Affichage" toolbar
		DiagramBgColorToolButton *m_background_color_button = nullptr;
		QMenu *m_text_grid_menu = nullptr;		///< Snap step used when dragging texts
		QToolButton *m_text_grid_button = nullptr;
		QMenu *m_shown_kinds_menu = nullptr;		///< View > Show, kinds of items (#301)
		QLabel *m_hidden_kinds_label = nullptr;	///< Status bar: how many kinds are hidden

		QList <QAction *> m_zoom_action_toolBar; ///Only zoom action must displayed in the toolbar

		void removeDiagramSilent(Diagram *diagram);

		QMdiArea m_workspace;
		QSignalMapper windowMapper;
		QDir open_dialog_dir; /// Directory to use for file dialogs such as File > save

		QDockWidget
		*qdw_pa, /// Dock for the elements panel
		*m_qdw_elmt_collection,
		*qdw_undo; /// Dock for the undo list

		ElementPickerPopup *elementPicker();
		QAction *m_show_element_picker = nullptr;
		QAction *m_show_shortcut_bar = nullptr;
		QAction *m_repeat_last_command = nullptr;
			/// What Enter on the folio repeats
		QPointer<QAction> m_last_command;
		void setLastCommand(QAction *action);
		ElementPickerPopup *m_element_picker = nullptr; ///< Built on first use
		QAction *m_command_search = nullptr;
		CommandSearchPopup *m_command_search_popup = nullptr; ///< Built on first use
		ElementsCollectionWidget *m_element_collection_widget;
			/// Last element placed from the collection, for "insert last"
		ElementsLocation m_last_inserted_element;
			
		DiagramPropertiesEditorDockWidget *m_selection_properties_editor;
			/// Information of the one element on the clipboard, read when
			/// the clipboard changes; see slot_updatePasteAction()
		DiagramContext m_clipboard_element_info;
		bool m_clipboard_has_element = false;
			/// Elements panel
		ElementsPanelWidget *pa;
		QMenu *windows_menu;
		
		QToolBar
		*main_tool_bar       = nullptr,
		*view_tool_bar       = nullptr,
		*diagram_tool_bar    = nullptr,
		*m_add_item_tool_bar = nullptr,
		*m_depth_tool_bar    = nullptr,
		*m_scripts_tool_bar  = nullptr;	///< One button per stored script
			/// Toolbars the user added (DiagramToolbarSettings)
		QList<QToolBar *> m_custom_tool_bars;
			/// The toolbar buttons that are widgets, by DiagramToolbarSettings id
		QHash<QString, QAction *> m_toolbar_widgets;
#ifdef QET_HAS_SCRIPTING
		QMenu *m_scripts_menu = nullptr;
		QList<QAction *> m_script_actions;	///< One per stored script, rebuilt when the folder changes
		bool m_had_scripts = false;
		QPointer<QDialog> m_script_manager;
#endif
		
		QUndoGroup undo_group;
		AutoNumberingDockWidget *m_autonumbering_dock;
		int activeSubWindowIndex;
		SearchAndReplaceWidget m_search_and_replace_widget;
};
#endif
