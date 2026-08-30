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
#ifndef IOLISTDIALOG_H
#define IOLISTDIALOG_H

#include "../iotree.h"

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QVector>

class Element;
class QComboBox;
class QETProject;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The IoListDialog class
	The I/O list of the project, of one controller and of one card, all
	three at once, editable in place.

	Three levels, because that is how the automation department reads it:
	the point belongs to a card, the card belongs to a controller, and the
	project is the controllers. The middle level is the one QElectroTech
	does not have - a card is a PLC master with a channel table, and
	nothing upstream says two cards are the same rack - so it is read from
	the plc_unit information of the card itself. A card that names none
	falls into a single unnamed controller, which is what makes a project
	with one PLC need no naming at all.

	The counts are the point of the exercise. Every card says how many of
	its points took a channel out of how many it has, every controller is
	the sum of its cards, and the project is the sum of the controllers
	plus what belongs to no card at all. The arithmetic is IoTree's, and a
	point can only ever be in one node of it: a point naming a card the
	project does not have gets a node of its own, with the uuid in plain
	sight, rather than disappearing from a total.

	The column decides between editing and jumping, and that is deliberate.
	The four columns a person actually corrects - the tag, the description,
	the address and the comment - open an editor on the second click. Every
	other column jumps: the folio of the card comes up, the drawn symbol of
	that channel lights up, and the list stays open beside it. So the
	window is not modal: a list that has to be closed to see what it points
	at is not a list, it is a report.

	Editing writes in two places at once. The point of the project list
	carries the text, the row of the card carries what the folio shows, and
	only the cell that was typed into is carried across - a comment typed
	here does not overwrite a function text somebody wrote in the element
	properties.
*/
class IoListDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit IoListDialog(QETProject *project, QWidget *parent = nullptr);

		/// @return the project this window is reading
		QETProject *project() const;

	private slots:
		void reload();
		void scopeChanged();
		void selectionChanged();
		void itemEdited(QTreeWidgetItem *item, int column);
		void itemJump(QTreeWidgetItem *item, int column);
		void stackChanged();
		void showSelected();

	private:
		void buildWidgets();
		void reloadCards();
		void reloadScopes(const IoTree::Tree &tree);
		void fill(const IoTree::Tree &tree, const IoList &list);
		void addUnit(const IoTree::UnitGroup &unit, const IoList &list);
		void addCard(QTreeWidgetItem *parent, const IoTree::CardGroup &card,
			     const IoList &list);
		void addPoint(QTreeWidgetItem *parent, int index, const IoList &list);
		void say(const QString &message, bool problem = false);
		void showItem(QTreeWidgetItem *item);
		void showElement(Element *element);

		/// @return the card element of the project that answers to @a uuid
		Element *cardByUuid(const QString &uuid) const;
		/// @return the drawn element linked to row @a io_index of @a master
		Element *slaveOf(Element *master, int io_index) const;

	private:
		QPointer<QETProject> m_project;
		/// the PLC cards of the project, index-aligned with m_card_data
		QVector<QPointer<Element>> m_cards;
		QVector<IoTree::Card> m_card_data;
		/// the element the last jump lit up, so the next one can put it out
		QPointer<Element> m_showed;
		bool m_loading = false;

		QComboBox *m_scope = nullptr;
		QTreeWidget *m_tree = nullptr;
		QLabel *m_counts = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_show = nullptr;
};

#endif // IOLISTDIALOG_H
