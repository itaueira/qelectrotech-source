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
#include "collectionthreadbudget.h"

const char *const CollectionThreadBudget::kOverrideVariable =
		"QET_COLLECTION_THREADS";

/**
	@brief CollectionThreadBudget::threadCount
	@param machine_cores : how many cores the machine reports.
	QThread::idealThreadCount() answers -1 when it cannot tell, which is why
	nothing here assumes the number is sane.
	@return how many worker threads the collection scan may use

	The floor of one carries the whole defensive weight of this function,
	and it carries it twice over: a machine with fewer cores than this class
	reserves, and a core count the operating system refused to give. There
	used to be a separate guard above for the second case; planting a defect
	in it showed it could not fail - the floor had already answered - so it
	is gone rather than sitting there looking like it protects something.
*/
int CollectionThreadBudget::threadCount(int machine_cores)
{
	const int count = machine_cores - kCoresLeftFree;
	return count < 1 ? 1 : count;
}

/**
	@brief CollectionThreadBudget::threadCount
	Same, with the environment override applied.
	@param machine_cores : how many cores the machine reports
	@param requested : the raw value read from the environment. Anything
	that is not a whole number of one or more - empty, absent, a word, zero,
	a negative - leaves the rule in charge rather than stopping the scan or
	guessing what was meant.
	@return how many worker threads the collection scan may use

	The override is deliberately not clamped to machine_cores: asking for
	more threads than there are cores is how the uncapped behaviour is
	reproduced for comparison, and the pool never starts more threads than
	there is work for anyway.
*/
int CollectionThreadBudget::threadCount(int machine_cores,
					const QString &requested)
{
	bool is_number = false;
	const int wanted = requested.toInt(&is_number);

	if (is_number && wanted >= 1) {
		return wanted;
	}
	return threadCount(machine_cores);
}
