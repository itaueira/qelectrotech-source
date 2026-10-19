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
#include "terminalmatch.h"

#include <QHash>
#include <QtMath>

namespace
{
		/// Two docking points closer than this are the same point. They are
		/// worked out through each element's transform, and a quarter turn
		/// can leave a rounding error behind, never a whole unit.
	constexpr qreal kSamePoint = 0.5;

	bool samePoint(const QPointF &a, const QPointF &b)
	{
		return qAbs(a.x() - b.x()) < kSamePoint
				&& qAbs(a.y() - b.y()) < kSamePoint;
	}

		/// How many terminals of @a spots carry each name
	QHash<QString, int> nameCounts(const QVector<TerminalMatch::Spot> &spots)
	{
		QHash<QString, int> counts;
		for (const TerminalMatch::Spot &spot : spots) {
			if (!spot.name.isEmpty()) {
				++counts[spot.name];
			}
		}
		return counts;
	}

		/// The pairing being built: for each old terminal its match, and
		/// which new terminals are taken
	struct Pairing
	{
		QVector<int> result;
		QVector<bool> taken;

		Pairing(int old_count, int new_count) :
			result(old_count, -1), taken(new_count, false) {}
		void take(int i, int j) { result[i] = j; taken[j] = true; }
	};

		/// The same printed name, when it is carried by one terminal on each side
	void pairByName(const QVector<TerminalMatch::Spot> &old_spots,
			const QVector<TerminalMatch::Spot> &new_spots, Pairing &pairing)
	{
		const QHash<QString, int> old_names = nameCounts(old_spots);
		const QHash<QString, int> new_names = nameCounts(new_spots);
		for (int i = 0 ; i < old_spots.size() ; ++i)
		{
			const QString &name = old_spots.at(i).name;
			if (pairing.result.at(i) >= 0
			    || old_names.value(name) != 1
			    || new_names.value(name) != 1) {
				continue;
			}
			for (int j = 0 ; j < new_spots.size() ; ++j)
			{
				if (!pairing.taken.at(j) && new_spots.at(j).name == name) {
					pairing.take(i, j);
					break;
				}
			}
		}
	}

		/// The same point, facing the same way when @a same_orientation
	void pairByPoint(const QVector<TerminalMatch::Spot> &old_spots,
			 const QVector<TerminalMatch::Spot> &new_spots, Pairing &pairing,
			 bool same_orientation)
	{
		for (int i = 0 ; i < old_spots.size() ; ++i)
		{
			if (pairing.result.at(i) >= 0) {
				continue;
			}
			for (int j = 0 ; j < new_spots.size() ; ++j)
			{
				if (pairing.taken.at(j)
				    || !samePoint(new_spots.at(j).point, old_spots.at(i).point)) {
					continue;
				}
				if (same_orientation
				    && new_spots.at(j).orientation != old_spots.at(i).orientation) {
					continue;
				}
				pairing.take(i, j);
				break;
			}
		}
	}
}

/**
	@brief TerminalMatch::match
	For each terminal of @a old_spots, the terminal of @a new_spots that is
	the same one. Four passes, each one only over what the passes before it
	left unmatched, and each terminal of @a new_spots taken once at most:

	1. the same uuid. It is how a symbol made from another one keeps saying
	   "this is the same terminal", even moved - the rule a project already
	   follows when it is opened against a changed symbol (TerminalUuids);
	2. the same name, when that name is carried by one terminal on each side.
	   The name is the number printed by the terminal (A1, 13, 14), and a
	   name that repeats on either side would only pick one at random;
	3. the same point with the same orientation;
	4. the same point.

	@param old_spots : the terminals of the element being replaced
	@param new_spots : the terminals of the element replacing it
	@return for each of @a old_spots, the index of its match in @a new_spots,
	or -1 when it has none
*/
QVector<int> TerminalMatch::match(const QVector<Spot> &old_spots,
				  const QVector<Spot> &new_spots)
{
	Pairing pairing(old_spots.size(), new_spots.size());

		//1. The same terminal, by its uuid
	for (int i = 0 ; i < old_spots.size() ; ++i)
	{
		if (old_spots.at(i).uuid.isNull()) {
			continue;
		}
		for (int j = 0 ; j < new_spots.size() ; ++j)
		{
			if (!pairing.taken.at(j) && new_spots.at(j).uuid == old_spots.at(i).uuid) {
				pairing.take(i, j);
				break;
			}
		}
	}
		//2. The same printed name, one on each side
	pairByName(old_spots, new_spots, pairing);
		//3. and 4. The same point, facing the same way first
	pairByPoint(old_spots, new_spots, pairing, true);
	pairByPoint(old_spots, new_spots, pairing, false);
	return pairing.result;
}

/**
	@brief TerminalMatch::matchByPlace
	For each terminal of @a old_spots, the terminal of @a new_spots that
	takes its wires when another element comes to its place, the wires
	staying where they are. Two passes, each terminal of @a new_spots taken
	once at most:

	1. the same point: the wire ends where it ended, now on the terminal
	   that lies there;
	2. the same name, when that name is carried by one terminal on each
	   side, for what the first pass left.

	The uuid says nothing here: the two elements are different components,
	and a terminal of one is not "the same terminal" as one of the other.
	@param old_spots : the terminals of the element leaving the place
	@param new_spots : the terminals of the element coming to it, where
	they will be once there
	@return for each of @a old_spots, the index of its match in @a new_spots,
	or -1 when it has none
*/
QVector<int> TerminalMatch::matchByPlace(const QVector<Spot> &old_spots,
					 const QVector<Spot> &new_spots)
{
	Pairing pairing(old_spots.size(), new_spots.size());
	pairByPoint(old_spots, new_spots, pairing, false);
	pairByName(old_spots, new_spots, pairing);
	return pairing.result;
}
