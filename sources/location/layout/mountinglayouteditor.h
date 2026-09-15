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
#ifndef MOUNTINGLAYOUTEDITOR_H
#define MOUNTINGLAYOUTEDITOR_H

#include <QMainWindow>
#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QString>

class MountingScene;
class MountingView;
class QAction;
class QComboBox;
class QETProject;
class QLabel;

/**
	@brief The window a panel is laid out in: one face of one enclosure,
	drawn in millimetre, with what is screwed to it.

	It is a window of its own and not a tab of the project, which was
	decided rather than assumed: the program already has a second editor
	built that way - its own scene, its own view, its own window, its own
	undo stack - and a surface counted in millimetre, with no sheet border
	and no title block, is not a folio. Making it one would have meant the
	list of folio views returning two kinds of thing, and every caller of
	that list being revisited.

	@par The wire between the drawing and the file

	The scene of the previous half edits a value and knows nothing about
	any project; this window is what ties the two ends together, and the
	tie is the whole point of it. It reads one face out of
	QETProject::mountingLayout() when it opens, and it writes the face back
	after every step of its undo stack - after a drag, and after every undo
	and redo of one.

	@par Why after every step, and not when the window closes

	Because of what a backup is. QETProject::writeBackup() serialises the
	project out of memory every twenty minutes, and the recovery copy the
	crash handler leaves behind is that same serialisation. A layout that
	only reaches the project when this window is closed is a layout no
	backup has ever seen: the afternoon somebody spent placing forty
	breakers dies with the process, and the recovery copy comes back
	cheerfully holding the empty plate of the morning. Writing after each
	step costs a copy of a list of rectangles, and it puts the work where
	everything else that saves the project can find it.

	@par Why it does not write the file

	The file belongs to the project and the save belongs to the person. This
	window writes into the project in memory and marks it modified, exactly
	like every other editor of the program; Ctrl+S on the project is what
	reaches the disc. A window that wrote the .qet by itself would be a
	window saving half-finished drawings on somebody else's behalf.

	@par Why opening it does not make the project ask to be saved

	QETProject::setMountingLayout compares before it writes, and the
	comparison allows a nanometre of slack on every length. Opening this
	window, looking at a plate and closing it hands the project back the
	layout it already had, the guard returns early, and nothing is marked
	modified. That is not a courtesy: a program that asks to save a project
	nobody edited teaches people to answer "no" to that question.

	@par What is not on the undo stack, and why

	The moves are; adding a face and mounting a part are not. The stack
	belongs to the scene and the scene drops it whenever another face is
	shown - which it must, because a step that moves a part of the previous
	face has nothing left to move. So a step that added a face would
	disappear from the stack the first time somebody looked at another face,
	and an undo that is there sometimes is worse than an undo that is never
	there. Making the composition of a face undoable needs a scene that can
	mount and unmount one part without rebuilding itself, which it cannot
	do yet. Until then the refusal happens before the fact: a face with no
	location or no kind is refused with the reason, and a face added by
	mistake is an empty face in a list rather than anything lost.
*/
class MountingLayoutEditor : public QMainWindow
{
	Q_OBJECT

	public:
		explicit MountingLayoutEditor(QETProject *project,
					      QWidget *parent = nullptr);
		~MountingLayoutEditor() override;

			/// @return the project this window lays out
		QETProject *project() const;
			/// @return the scene the face is drawn on
		MountingScene *scene() const;
			/// @return the view the face is looked at through
		MountingView *view() const;

			/// @return the identifier of the face being shown, empty when none
		QString shownSurface() const;

		/**
			@brief Draw the face of @a surface_uuid.
			@param surface_uuid which face, empty for none
			@return true when there was such a face in the project

			Reading the layout again on the way, so that what is
			drawn is what the project holds and not what this window
			remembers.
		*/
		bool showSurface(const QString &surface_uuid);

		/**
			@brief Write the face being drawn back into the project.
			@return true when the project was given something new

			False for the two ordinary cases - nothing changed, and
			there is no face being drawn - as well as for the face
			that has gone from the project while this window was
			open. None of the three is an error.
		*/
		bool commitToProject();

		/**
			@brief Add a face to the project and show it.
			@param location_path the location it belongs to
			@param kind which face of it: plate, door, side
			@param name what a person calls it, may be empty
			@param area how much room it has, an unusable one meaning
			nobody has measured it yet
			@param error filled with why nothing was added
			@return the identifier of the face, empty when refused
		*/
		QString addSurface(const QString &location_path,
				   const QString &kind,
				   const QString &name,
				   const QSizeF &area,
				   QString *error = nullptr);

	protected:
		void closeEvent(QCloseEvent *event) override;

	private slots:
		void stepApplied();
		void surfaceChosen(int index);
		void newSurface();
		void addPart();
		void pointedAt(const QPointF &position_mm);
		void zoomTold(qreal pixels_per_millimetre);

	private:
		void buildActions();
		void buildWidgets();
		void refreshSurfaceList();
		void updateTitle();
		void updateActions();
		void say(const QString &message, bool problem = false);
		QPointF freePosition() const;
		bool isEditable() const;
		void readSettings();
		void writeSettings() const;

		QPointer<QETProject> m_project;
		MountingScene *m_scene = nullptr;
		MountingView *m_view = nullptr;
		QComboBox *m_surface_box = nullptr;
		QLabel *m_position_label = nullptr;
		QLabel *m_zoom_label = nullptr;

		/// which face is drawn, so that a rebuilt list can find it again
		QString m_shown;
		/// true while the list is being filled, so filling it chooses nothing
		bool m_filling = false;

		QAction *m_new_surface = nullptr;
		QAction *m_add_part = nullptr;
		QAction *m_undo = nullptr;
		QAction *m_redo = nullptr;
		QAction *m_zoom_in = nullptr;
		QAction *m_zoom_out = nullptr;
		QAction *m_zoom_fit = nullptr;
		QAction *m_zoom_actual = nullptr;
		QAction *m_close = nullptr;
};

#endif // MOUNTINGLAYOUTEDITOR_H
