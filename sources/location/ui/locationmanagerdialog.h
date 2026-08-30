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
#ifndef LOCATIONMANAGERDIALOG_H
#define LOCATIONMANAGERDIALOG_H

#include "../locationtree.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>

class Element;
class QETProject;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The LocationManagerDialog class
	Where the enclosures of the project are made, named, nested and taken
	apart, and where a handful of components is put into one of them in a
	single gesture.

	The window is the tree and nothing else. It does not draw a rectangle
	on a folio and it does not print a bill of material - those are their
	own steps of the task - because the thing a person needs first is
	somewhere the enclosure exists at all. A project of ours has fourteen
	folios and one cabinet; the cabinet is on all fourteen and is none of
	them, so it has to live in a list of its own.

	Not modal, and for the same reason the I/O list is not: the selection
	being assigned is on the folio behind. Closing the window to see what
	it points at would make it a report instead of a manager. Which also
	means the tree can change under it - somebody undoes on the folio - so
	the window listens to the undo stack rather than to a signal of its
	own, the way IoListDialog does, and rebuilds from the project every
	time the index moves.

	Every mutation goes through EditLocationTreeCommand, including the ones
	that look like they need no command. Renaming a code is the interesting
	case: it rewrites the path of every component standing below it, and
	that half has to be in the same undo as the rename or the project ends
	up with a tree saying QCP1 and eighty components saying QCM1. So the
	dialogue never writes the tree itself; it hands the command the tree as
	the edit left it plus the map the tree reported, and lets the command
	drag the components.

	The counts are the point of the window as much as the rows are. Every
	location says how many components name it, and the header says how many
	name nothing at all - which is the number the closure of CU-32.8 has to
	drive to zero, visible from the first day rather than only in a report
	written later.
*/
class LocationManagerDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit LocationManagerDialog(QETProject *project,
					       QWidget *parent = nullptr);

		/// @return the project this window is editing
		QETProject *project() const;

	private slots:
		void reload();
		void selectionChanged();
		void itemEdited(QTreeWidgetItem *item, int column);
		void stackChanged();

		void addLocation();
		void addChildLocation();
		void removeLocation();
		void moveLocation();
		void linkPart();
		void assignSelection();
		void unassignSelection();

	private:
		void buildWidgets();
		void fill(const LocationTree &tree);
		void addRow(QTreeWidgetItem *parent, const LocationTree &tree,
			    const QString &uuid);
		void say(const QString &message, bool problem = false);
		void push(const LocationTree &tree,
			  const QMap<QString, QString> &changed,
			  const QString &label);

		/// @return how many components stand on each path of the project
		QHash<QString, int> componentCounts(int *unassigned) const;
		/// @return what the person has selected on the folios, if anything
		QList<Element *> selectedElements() const;
		/// @return the uuid of the selected row, empty when none
		QString selectedUuid() const;
		/// @return a code no sibling of @a parent_uuid answers to yet
		QString freeCode(const LocationTree &tree,
				 const QString &parent_uuid) const;
		void createUnder(const QString &parent_uuid);
		void assignTo(const QString &path);

	private:
		QPointer<QETProject> m_project;
		/// how many components stand on each path, refreshed by reload
		QHash<QString, int> m_counts;
		bool m_loading = false;

		QTreeWidget *m_tree = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_add = nullptr;
		QPushButton *m_add_child = nullptr;
		QPushButton *m_remove = nullptr;
		QPushButton *m_move = nullptr;
		QPushButton *m_link = nullptr;
		QPushButton *m_assign = nullptr;
		QPushButton *m_unassign = nullptr;
};

#endif // LOCATIONMANAGERDIALOG_H
