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
#ifndef IOTREE_H
#define IOTREE_H

#include "iolist.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QVector>

/**
	@brief The three levels an I/O list is read at: the card, the
	programmable controller, and the whole project.

	A card knows its own channels and nothing else. A person checking a
	panel needs the other two: everything one controller carries, cards
	included, and everything the project carries, controllers included -
	and the numbers have to add up between them, because that is what
	tells them nothing was forgotten.

	QElectroTech has the bottom level and the top one. A card is an
	element whose data says Master, PLC, and whose table of channels is
	not empty; the project is the list of every point. The middle level
	does not exist upstream: nothing says two cards belong to the same
	controller. The fork writes that in the card itself, in the plc_unit
	element information - see the task file, decision F - so that the
	link travels with the card when it is copied, and so that a project
	saved here still opens in stock QElectroTech.

	This file is the arithmetic of those three levels and nothing else:
	no element, no project, no widget. It is given the list and a plain
	description of the cards, and it gives back who is under whom. That
	is what makes the invariant testable on a bench - and the invariant
	is the point of the whole exercise.
*/
class IoTree
{
	Q_DECLARE_TR_FUNCTIONS(IoTree)

	public:
		/**
			@brief A card, as this file needs to see it.

			Built by the caller from the project's master elements, or by a
			test from nothing at all. Keeping the element out of here is what
			lets the tree be tested.
		*/
		struct Card
		{
				/// element uuid, the same string the points carry in master_uuid
			QString uuid;
				/// what to show: the card label, its name when it has no label
			QString label;
				/// the controller this card belongs to, empty when nobody said
			QString unit;
				/// folio this card is drawn on, shown but never grouped on
			QString folio;
				/// how many channels the card has, table rows included
			int channels = 0;
		};

		/**
			@brief One card and the points that named it.
		*/
		struct CardGroup
		{
			QString uuid;
			QString label;
			QString folio;
			int channels = 0;
				/**
					@brief true when points name this uuid but no card in the
					project answers to it.

					The card was deleted, or the project was opened without
					the folio that carries it. The points are shown all the
					same, under the uuid, because a point that vanishes from
					the count is exactly the failure this tree exists to make
					impossible.
				*/
			bool missing = false;
				/// indexes into the IoList, in channel order
			QList<int> points;

			int total() const;
			int assigned(const IoList &list) const;
		};

		/**
			@brief One controller and its cards.
		*/
		struct UnitGroup
		{
				/// the plc_unit value, empty for the controller nobody named
			QString name;
				/// what to show
			QString label;
			QList<CardGroup> cards;

			int total() const;
			int assigned(const IoList &list) const;
		};

		/**
			@brief The whole project, in three levels.
		*/
		struct Tree
		{
			QList<UnitGroup> units;
				/// cards named by a point but absent from the project
			QList<CardGroup> missing;
				/**
					@brief indexes of the points that name no card at all.

					Not the same set as IoList::unassigned(): that one is the
					points with no channel, and a point can name a card
					without having taken a channel in it. Here the question
					is only whether the point knows which card it belongs to.
				*/
			QList<int> cardless;

			int total() const;
			int assigned(const IoList &list) const;
			bool isEmpty() const;
		};

		/**
			@brief Sort the points of @a list under the cards of @a cards.
			@param list the project's I/O list
			@param cards every card of the project
			@return the three levels, with every point of the list in exactly
			one of them
		*/
		static Tree build(const IoList &list, const QVector<Card> &cards);

		/// @return what to show for a controller whose name is @a name
		static QString unitLabel(const QString &name);

		/// @return what to show for a card that is not in the project
		static QString missingLabel(const QString &uuid);
};

#endif // IOTREE_H
