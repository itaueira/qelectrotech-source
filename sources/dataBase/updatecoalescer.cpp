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
#include "updatecoalescer.h"

/**
	@brief UpdateCoalescer::beginOperation
	Open an operation. Nested opens count, and only the matching outermost
	close announces.
*/
void UpdateCoalescer::beginOperation()
{
	++m_depth;
}

/**
	@brief UpdateCoalescer::endOperation
	Close an operation.
	@return true when this close is the one that must announce
*/
bool UpdateCoalescer::endOperation()
{
	if (m_depth == 0) {
			//Unbalanced close. Nothing was held back, so there is nothing to
			//announce; and letting the depth go below zero would make the
			//next real operation close one level short of announcing.
		return false;
	}

	--m_depth;
	if (m_depth > 0) {
		return false;
	}

	const bool announce = m_pending;
	m_pending = false;
	return announce;
}

/**
	@brief UpdateCoalescer::notify
	@return true when the caller must announce the change now
*/
bool UpdateCoalescer::notify()
{
	if (m_depth == 0) {
		return true;
	}

	m_pending = true;
	return false;
}
