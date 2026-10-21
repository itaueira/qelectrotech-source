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
#ifndef ASSEMBLYCHANGEMARKS_H
#define ASSEMBLYCHANGEMARKS_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QETProject;
class QGraphicsItem;
class QGraphicsObject;
class QPainter;
class QPainterPath;
class QRectF;

/**
	@brief The mark, on the folio, of what changed since the panel was marked
	as wired (T29, step 5).
	The components and the wires the order of service lists to mount, to
	label again or to number again (AssemblyChanges) wear a mark of their
	own, in a colour apart from the blue of the highlight the mouse already
	draws: what was removed is drawn nowhere, and has nothing to wear it.

	Worked out, never written: the mark is a flag on the item
	(QGraphicsItem::data()), set from the comparison of the photograph with
	the drawing as it is now, and taken off when the project is no longer
	marked - nothing in the project file, nothing to clean.

	Worked out again a moment after every change of the project (its undo
	stack), and only for a project marked as wired: reading the drawing
	builds every folio still waiting to be built, which a project that was
	never marked never needs.

	On the screen only: the folio printed or exported is the folio, and the
	order of service for the shop is the file of the changes.
*/
class AssemblyChangeMarks : public QObject
{
		Q_OBJECT

	public:
			/// The key of the flag on an item (QGraphicsItem::data())
		static constexpr int MarkKey = 0x7429;

			/// The marks of @a project, made the first time they are asked for
		static AssemblyChangeMarks *of(QETProject *project);
			/// Whether @a item wears the mark
		static bool isMarked(const QGraphicsItem *item);
			/// The mark, drawn around @a box, or along @a path for a wire
		static void drawAround(QPainter *painter, const QRectF &box);
		static void drawAlong(QPainter *painter, const QPainterPath &path);

			/// Work the marks out now, and not a moment after
		void refresh();

	private:
		explicit AssemblyChangeMarks(QETProject *project);

		QPointer<QETProject> m_project;
		QTimer m_timer;
		QList<QPointer<QGraphicsObject>> m_marked;
};

#endif // ASSEMBLYCHANGEMARKS_H
