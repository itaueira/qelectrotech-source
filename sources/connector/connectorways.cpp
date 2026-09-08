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
#include "connectorways.h"

#include <QSet>

/**
	@brief ConnectorWays::fromPinout
	@param part_ways : the labels of the pins of the catalogue part, in
	part order, as CatalogPart::pinLabels() gives them
	@param drawn_labels : the pin labels the folios carry on this one
	connector, in any order
	@return what is used, what is spare, and what the part has no way for
*/
ConnectorWays ConnectorWays::fromPinout(const QStringList &part_ways,
					const QStringList &drawn_labels)
{
	ConnectorWays answer;

	QSet<QString> way_set;
	for (const QString &way : part_ways)
	{
		if (way.trimmed().isEmpty()) {
			++answer.m_blank_ways;
			continue;
		}
		if (way_set.contains(way)) {
			if (!answer.m_declared_twice.contains(way)) {
				answer.m_declared_twice.append(way);
			}
			continue;
		}
		way_set.insert(way);
		answer.m_ways.append(way);
	}

	QSet<QString> drawn_set;
	for (const QString &label : drawn_labels)
	{
		if (label.trimmed().isEmpty()) {
			++answer.m_blank_drawn;
			continue;
		}
		if (drawn_set.contains(label)) {
			if (!answer.m_drawn_twice.contains(label)) {
				answer.m_drawn_twice.append(label);
			}
			continue;
		}
		drawn_set.insert(label);
		answer.m_drawn.append(label);
	}

	answer.m_known = !answer.m_ways.isEmpty();
	if (!answer.m_known) {
		/*
			Nothing is said about the part, and that includes not
			saying that the drawn labels are absent from it: with no
			pinout written down, "this way does not exist" is not a
			finding, it is a guess. What the folios carry has been
			read all the same, and is answered for.
		*/
		return answer;
	}

	for (const QString &way : answer.m_ways)
	{
		if (drawn_set.contains(way)) {
			answer.m_used.append(way);
		} else {
			answer.m_reserve.append(way);
		}
	}

	for (const QString &label : answer.m_drawn)
	{
		if (!way_set.contains(label)) {
			answer.m_not_on_part.append(label);
		}
	}

	return answer;
}

/**
	@brief ConnectorWays::unknownCount
	@return the reading of a count that has no pinout behind it
*/
int ConnectorWays::unknownCount()
{
	return -1;
}

/**
	@brief ConnectorWays::isKnown
	@return true when the part declares a pinout
*/
bool ConnectorWays::isKnown() const
{
	return m_known;
}

/**
	@brief ConnectorWays::ways
	@return every distinct way of the part, in part order
*/
QStringList ConnectorWays::ways() const
{
	return m_ways;
}

/**
	@brief ConnectorWays::used
	@return the ways the folios draw, in part order
*/
QStringList ConnectorWays::used() const
{
	return m_used;
}

/**
	@brief ConnectorWays::reserve
	@return the ways nothing draws, in part order
*/
QStringList ConnectorWays::reserve() const
{
	return m_reserve;
}

/**
	@brief ConnectorWays::notOnPart
	@return the drawn labels the part has no way for
*/
QStringList ConnectorWays::notOnPart() const
{
	return m_not_on_part;
}

/**
	@brief ConnectorWays::drawn
	@return every distinct label the folios draw
*/
QStringList ConnectorWays::drawn() const
{
	return m_drawn;
}

/**
	@brief ConnectorWays::drawnTwice
	@return the labels drawn more than once
*/
QStringList ConnectorWays::drawnTwice() const
{
	return m_drawn_twice;
}

/**
	@brief ConnectorWays::declaredTwice
	@return the labels the part declares more than once
*/
QStringList ConnectorWays::declaredTwice() const
{
	return m_declared_twice;
}

/**
	@brief ConnectorWays::wayCount
	@return how many ways the part has, unknownCount() when it declares none
*/
int ConnectorWays::wayCount() const
{
	return m_known ? m_ways.size() : unknownCount();
}

/**
	@brief ConnectorWays::usedCount
	@return how many ways are drawn, unknownCount() when there is no pinout
*/
int ConnectorWays::usedCount() const
{
	return m_known ? m_used.size() : unknownCount();
}

/**
	@brief ConnectorWays::reserveCount
	@return how many ways are spare, unknownCount() when there is no pinout
*/
int ConnectorWays::reserveCount() const
{
	return m_known ? m_reserve.size() : unknownCount();
}

/**
	@brief ConnectorWays::drawnCount
	@return how many distinct labels the folios draw
*/
int ConnectorWays::drawnCount() const
{
	return m_drawn.size();
}

/**
	@brief ConnectorWays::blankWayCount
	@return how many pins of the part carry no label at all
*/
int ConnectorWays::blankWayCount() const
{
	return m_blank_ways;
}

/**
	@brief ConnectorWays::blankDrawnCount
	@return how many drawn pins carry no label at all
*/
int ConnectorWays::blankDrawnCount() const
{
	return m_blank_drawn;
}
