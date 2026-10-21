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
#ifndef QETELEMENTEDITOR_H
#define QETELEMENTEDITOR_H

#include "../../ElementsCollection/elementslocation.h"

#include <QCloseEvent>
#include <QMainWindow>

class ElementScene;
class QActionGroup;
class ElementItemEditor;
class ElementView;
class QListWidget;
class QStackedWidget;
class QLabel;
class QUndoGroup;
class QUndoView;
class PartImage;

namespace Ui {
	class QETElementEditor;
}

class QETElementEditor : public QMainWindow
{
		Q_OBJECT
	signals:
		void saveToLocation(ElementsLocation loc);
			/// "Enregistrer" in part drawing mode: the drawing as it stands,
			/// a <layout_drawing> block as text, empty when nothing is drawn
		void partDrawingSaved(const QString &block);

	public:
		explicit QETElementEditor(QWidget *parent = nullptr);
		~QETElementEditor() override;

		void contextMenu(QPoint p, QList<QAction *> actions = QList<QAction *>());
		void setNames(const NamesList &name_list);
		void setLocation(const ElementsLocation &location);
		ElementsLocation location() const;
		void setFileName(const QString &file_name);
		QString fileName() const;
		void setReadOnly(bool ro);
		bool isReadOnly() const;
		void fromFile(const QString &filepath);
		bool toFile(const QString &filepath);
		void fromLocation(const ElementsLocation &location);
		bool toLocation(const ElementsLocation &location);
		bool isEditing(const ElementsLocation &location);
		bool isEditing(const QString &filepath);
		ElementScene *elementScene() const;
		ElementView *elementView() const;
		static QPointF pasteOffset();
		static QString getOpenElementFileName(QWidget *parent = nullptr, const QString &dir = QString());
		void updateTitle();
		void fillPartsList();
		void UncheckAddPrimitive();
		void updateCurrentPartEditor();
		void updateInformations();
		void updatePartsList();
		void updateSelectionFromPartsList();
		void openElement(const QString &filepath);
		bool checkElement();

			/// The two scenes: the symbol drawn on folios, and the 2D
			/// drawing of a part of the catalogue, in millimetre, that the
			/// mounting plate shows. elementScene() and elementView()
			/// answer for the one shown, because every part, part editor
			/// and tool reaches the scene through them.
		bool isLayoutDrawingActive() const;
		ElementScene *layoutDrawingScene() const;
			/// The element as it would be saved
		QDomDocument elementXml();

		/**
			@brief Open on the 2D drawing of a part of the catalogue, alone.
			@param part_code the code of the part, for the title and the
			questions
			@param block the drawing, a <layout_drawing> block as text;
			empty for a part that has none yet

			No symbol, and none of the actions of a symbol file. Saving
			hands the drawing to partDrawingSaved(), and whoever opened
			the editor decides where it goes: the drawing belongs to the
			part, and the part is not this window's to write.
		*/
		void editPartDrawing(const QString &part_code, const QString &block);
		bool isPartDrawingMode() const;
			/// @return the drawing as it stands, a <layout_drawing> block
			/// as text, empty when nothing is drawn
		QString partDrawing() const;
			/// An image on the 2D tab, at @a size_mm, centred on the
			/// origin, as one step of undo. @return the image, or nullptr
			/// when the bytes are not a file of @a format this program
			/// can draw
		PartImage *addLayoutImage(const QByteArray &data, const QString &format,
					  const QSizeF &size_mm);
			/// What "Insérer une image" does once the file is chosen: ask
			/// the real size, then addLayoutImage(). @return true when an
			/// image was added
		bool insertImageFile(const QString &path);

	protected:
		bool event(QEvent *event) override;
		void closeEvent(QCloseEvent *) override;


	private slots:
		bool on_m_save_action_triggered();
		bool on_m_save_as_action_triggered();
		void on_m_select_all_act_triggered();
		void on_m_edit_element_properties_action_triggered();
		void on_m_new_action_triggered();
		void on_m_open_action_triggered();
		void on_m_open_from_file_action_triggered();
		bool on_m_save_as_file_action_triggered();
		bool on_m_export_svg_action_triggered();
		void on_m_reload_action_triggered();
		void on_m_quit_action_triggered();
		void on_m_deselect_all_action_triggered();
		void on_m_cut_action_triggered();
		void on_m_copy_action_triggered();
		void on_m_paste_action_triggered();
		void on_m_paste_in_area_action_triggered();
		void on_m_paste_from_file_action_triggered();
		void on_m_paste_from_element_action_triggered();
		void on_m_revert_selection_action_triggered();
		void on_m_delete_action_triggered();
		void on_m_edit_names_action_triggered();
		void on_m_edit_author_action_triggered();
		void on_m_zoom_in_action_triggered();
		void on_m_zoom_out_action_triggered();
		void on_m_zoom_fit_best_action_triggered();
		void on_m_zoom_original_action_triggered();
		void on_m_about_qet_action_triggered();
		void on_m_online_manual_triggered();
		void on_m_youtube_action_triggered();
		void on_m_donate_action_triggered();
		void on_m_about_qt_action_triggered();
		void on_m_import_dxf_triggered();
		void on_m_import_scaled_element_triggered();
		void scaleElement();

	private:
		bool canClose();
		void readSettings();
		void readSettingsState();
		void writeSettings() const;
		void setupActions();
		void updateAction();
		void setupConnection();
		void initGui();
		QWidget *clearToolsDock();
		void copyAndPasteXml(const QDomDocument &xml_document);
		bool isModified() const;
		void setClean();
		void setLayoutDrawingActive(bool active);
		void loadPartDrawing(const QString &block);
		QList<QAction *> symbolFileActions() const;
		void updateLengthUnit();
		void insertImage();
		bool askImageSize(QSizeF &size);
		void noticePlacedElements(const ElementsLocation &location);
		static ElementsLocation fileLocation(const QString &file_path);

	private:
		Ui::QETElementEditor *ui;

		bool
			m_read_only        = false,
			m_opened_from_file = false,
			m_first_activation = true;

		ElementScene *m_elmt_scene = nullptr;

		QActionGroup
			*m_add_part_action_grp = nullptr,
			*m_depth_action_group  = nullptr;

		QList<QAction *> m_context_menu_action_list;

		QAction
			*m_scale_element_action = nullptr,
			*m_undo_action = nullptr,
			*m_redo_action = nullptr;

			/// Align, distribute and centre on the axis (T73), each with
			/// how many parts it needs to be enabled
		QList<QPair<QAction *, int>> m_align_actions;


			/// Hash associating primitive names with their matching edition widget
		QHash<QString, ElementItemEditor *> m_editors;

		ElementsLocation m_location;

		QString
			m_file_name,
			m_min_title;

		ElementView *m_view = nullptr;

		QListWidget *m_parts_list = nullptr;

		QStackedWidget *m_tools_dock_stack = nullptr;

		QLabel *m_default_informations = nullptr;

		QLabel *m_position_label = nullptr;

			/// m_elmt_scene and m_view stay the symbol's: the element
			/// data, the checks before saving and the title are the
			/// symbol's whichever tab is shown. What edits the content -
			/// tools, selection, parts list, edit menu, zoom, undo -
			/// goes through the active pair.
		ElementScene
			*m_drawing_scene = nullptr,
			*m_active_scene  = nullptr;
		ElementView
			*m_drawing_view = nullptr,
			*m_active_view  = nullptr;
		QStackedWidget *m_view_stack = nullptr;
		bool m_part_drawing_mode = false;
		QString m_part_code;
		QUndoGroup *m_undo_group = nullptr;
		QUndoView *m_undo_view = nullptr;
		QAction
			*m_add_terminal_action     = nullptr,
			*m_add_dynamic_text_action = nullptr,
			*m_insert_image_action     = nullptr;

};

#endif // QETELEMENTEDITOR_H
