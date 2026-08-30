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
#include "iotree.h"

#include "iopoint.h"

#include <QHash>

#include <algorithm>

/**
	@brief IoTree::CardGroup::total
	@return how many points named this card
*/
int IoTree::CardGroup::total() const
{
	return int(points.count());
}

/**
	@brief IoTree::CardGroup::assigned
	@param list the list the indexes point into
	@return how many of those points actually took a channel

	Naming a card and occupying a channel in it are two different
	things, and the difference is what the person is looking for when
	they open the list: a card with twelve points and eight assigned
	has four still to place.
*/
int IoTree::CardGroup::assigned(const IoList &list) const
{
	int count = 0;
	for (int index : points)
	{
		if (index >= 0 && index < list.count() && list.at(index).isAssigned()) {
			++ count;
		}
	}
	return count;
}

/**
	@brief IoTree::UnitGroup::total
	@return every point of every card of this controller
*/
int IoTree::UnitGroup::total() const
{
	int count = 0;
	for (const CardGroup &card : cards) {
		count += card.total();
	}
	return count;
}

/**
	@brief IoTree::UnitGroup::assigned
	@param list the list the indexes point into
	@return every assigned point of every card of this controller
*/
int IoTree::UnitGroup::assigned(const IoList &list) const
{
	int count = 0;
	for (const CardGroup &card : cards) {
		count += card.assigned(list);
	}
	return count;
}

/**
	@brief IoTree::Tree::total
	@return every point of the list, counted exactly once

	This is the invariant the whole file exists for: the project total is
	the controllers, plus the cards nobody can find, plus the points
	that name no card - and nothing falls between them.
*/
int IoTree::Tree::total() const
{
	int count = int(cardless.count());
	for (const UnitGroup &unit : units) {
		count += unit.total();
	}
	for (const CardGroup &card : missing) {
		count += card.total();
	}
	return count;
}

/**
	@brief IoTree::Tree::assigned
	@param list the list the indexes point into
	@return how many points of the whole project took a channel
*/
int IoTree::Tree::assigned(const IoList &list) const
{
	int count = 0;
	for (const UnitGroup &unit : units) {
		count += unit.assigned(list);
	}
	for (const CardGroup &card : missing) {
		count += card.assigned(list);
	}
	return count;
}

/**
	@brief IoTree::Tree::isEmpty
	@return true when there is neither a point nor a card to show
*/
bool IoTree::Tree::isEmpty() const
{
	return units.isEmpty() && missing.isEmpty() && cardless.isEmpty();
}

/**
	@brief IoTree::unitLabel
	@param name the plc_unit value
	@return what to show for it

	A project where nobody named a controller still has one, and it has
	to be called something. Calling it "the controller" is what makes a
	single-controller project need no naming at all.
*/
QString IoTree::unitLabel(const QString &name)
{
	const QString trimmed = name.trimmed();
	return trimmed.isEmpty() ? tr("Automate") : trimmed;
}

/**
	@brief IoTree::missingLabel
	@param uuid the uuid the points name
	@return what to show for a card the project does not have
*/
QString IoTree::missingLabel(const QString &uuid)
{
	return tr("Carte absente (%1)").arg(uuid);
}

/**
	@brief IoTree::build
	@param list the project's I/O list
	@param cards every card of the project
	@return the three levels

	Every card of the project gets a group even when no point named it:
	an empty card is a fact the person needs to see, not an absence. And
	every point of the list lands somewhere - in its card, in a card the
	project cannot find, or in the bucket of points that name no card -
	so that the three counts add up to the list's own count.
*/
IoTree::Tree IoTree::build(const IoList &list, const QVector<Card> &cards)
{
	Tree tree;

		//bucket the points by the card they name
	QHash<QString, QList<int>> points_of;
	for (int i = 0 ; i < list.count() ; ++ i)
	{
		const QString uuid = list.at(i).master_uuid.trimmed();
		if (uuid.isEmpty()) {
			tree.cardless.append(i);
		} else {
			points_of[uuid].append(i);
		}
	}

		//the channel order is what the person reads the card in; a point
		//that named the card without taking a channel goes last, and ties
		//break on the identifier so that two runs give the same list
	auto by_channel = [&list](int a, int b)
	{
		const IoPoint &pa = list.at(a);
		const IoPoint &pb = list.at(b);
		if (pa.io_index != pb.io_index)
		{
			if (pa.io_index < 0) { return false; }
			if (pb.io_index < 0) { return true; }
			return pa.io_index < pb.io_index;
		}
		return pa.id < pb.id;
	};

		//one group per card of the project, points or no points
	QHash<QString, int> unit_index;
	for (const Card &card : cards)
	{
		CardGroup group;
		group.uuid = card.uuid;
		group.label = card.label;
		group.folio = card.folio;
		group.channels = card.channels;
		group.points = points_of.take(card.uuid);
		std::sort(group.points.begin(), group.points.end(), by_channel);

			//two spellings of the same name are the same controller: a
			//person who typed "CLP1" once and "clp1" once meant one
		const QString key = card.unit.trimmed().toLower();
		if (!unit_index.contains(key))
		{
			UnitGroup unit;
			unit.name = card.unit.trimmed();
			unit.label = unitLabel(unit.name);
			unit_index.insert(key, int(tree.units.count()));
			tree.units.append(unit);
		}
		tree.units[unit_index.value(key)].cards.append(group);
	}

		//whatever is left named a card the project does not have
	const QList<QString> orphan_uuids = points_of.keys();
	for (const QString &uuid : orphan_uuids)
	{
		CardGroup group;
		group.uuid = uuid;
		group.label = missingLabel(uuid);
		group.missing = true;
		group.points = points_of.value(uuid);
		std::sort(group.points.begin(), group.points.end(), by_channel);
		tree.missing.append(group);
	}

		//deterministic order: the unnamed controller first, because it is
		//the one a project that never named anything has
	std::sort(tree.units.begin(), tree.units.end(),
			  [](const UnitGroup &a, const UnitGroup &b)
	{
		if (a.name.isEmpty() != b.name.isEmpty()) { return a.name.isEmpty(); }
		const int cmp = QString::compare(a.label, b.label, Qt::CaseInsensitive);
		return cmp == 0 ? a.label < b.label : cmp < 0;
	});

	for (UnitGroup &unit : tree.units)
	{
		std::sort(unit.cards.begin(), unit.cards.end(),
				  [](const CardGroup &a, const CardGroup &b)
		{
			const int cmp = QString::compare(a.label, b.label, Qt::CaseInsensitive);
			return cmp == 0 ? a.uuid < b.uuid : cmp < 0;
		});
	}

	std::sort(tree.missing.begin(), tree.missing.end(),
			  [](const CardGroup &a, const CardGroup &b)
	{
		return a.uuid < b.uuid;
	});

	return tree;
}
