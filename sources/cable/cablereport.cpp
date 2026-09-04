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
#include "cablereport.h"

/**
	@return true when no cable was looked at and nothing was counted
*/
bool CableReport::isEmpty() const
{
	return cables == 0
		&& wires_occupied == 0
		&& wires_reserved == 0
		&& wires_orphan == 0
		&& duplicated_conductors == 0
		&& orphans.isEmpty();
}

/**
	@return true when at least one wire names a conductor that is not there
*/
bool CableReport::hasOrphan() const
{
	return wires_orphan > 0;
}

void CableReport::clear()
{
	cables = 0;
	wires_occupied = 0;
	wires_reserved = 0;
	wires_orphan = 0;
	duplicated_conductors = 0;
	orphans.clear();
}

/**
	@return the counts and then the orphan lines, one per line

	Empty when there is nothing to say, so that a caller can show the text
	only when it is not empty instead of testing the fields one by one.
*/
QString CableReport::toText() const
{
	if (isEmpty()) {
		return QString();
	}

	QStringList lines;

	lines << tr("%n cable(s) read.", "", cables);
	lines << tr("%n wire(s) linked to a conductor.", "", wires_occupied);

	if (wires_reserved) {
		lines << tr("%n spare wire(s).", "", wires_reserved);
	}

	if (wires_orphan)
	{
		lines << tr("%n wire(s) lost their conductor and are kept as marked spares:",
			    "", wires_orphan);
		for (const QString &line : orphans) {
			lines << QStringLiteral("    ") + line;
		}
	}

	if (duplicated_conductors)
	{
		lines << tr("%n conductor(s) share their identifier with another one; "
			    "a wire pointing at such an identifier may have been linked "
			    "to the wrong conductor.", "", duplicated_conductors);
	}

	return lines.join(QLatin1Char('\n'));
}
