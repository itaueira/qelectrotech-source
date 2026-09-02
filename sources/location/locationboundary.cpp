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
#include "locationboundary.h"

#include "locationtree.h"

#include <QStringList>

/**
	@brief crossesLocationBoundary
	See the header for the rule and for the three codings it rejects.

	Splitting is delegated rather than repeated: LocationTree::splitPath
	already drops empty segments and simplifies each code, so a path typed
	as "QCM1 / PORTE" and one stored as "QCM1/PORTE" arrive here as the same
	pair of codes, and a path of nothing but separators arrives as no codes
	at all. Writing a second splitter here would be a second answer to the
	same question, and the two would drift.
*/
bool crossesLocationBoundary(const QString &path_a, const QString &path_b)
{
	const QStringList a{LocationTree::splitPath(path_a)};
	const QStringList b{LocationTree::splitPath(path_b)};

		//Conservatism: an unknown end asserts nothing, so there is nothing
		//to cross. See the header.
	if (a.isEmpty() || b.isEmpty()) {
		return false;
	}

		//Ancestry is a prefix relation on segments, so only the shorter of
		//the two can be the ancestor, and the comparison stops at its end.
	const QStringList &shorter{a.size() <= b.size() ? a : b};
	const QStringList &longer {a.size() <= b.size() ? b : a};

	const int depth = shorter.size();
	for (int i = 0 ; i < depth ; ++ i)
	{
		if (shorter.at(i).compare(longer.at(i), Qt::CaseInsensitive) != 0) {
			return true;
		}
	}

		//The shorter path is a prefix of the longer one, segment for
		//segment: one contains the other, or they are the same place.
	return false;
}
