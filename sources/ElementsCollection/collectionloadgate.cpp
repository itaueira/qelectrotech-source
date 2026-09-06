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
#include "collectionloadgate.h"

/**
	@brief CollectionLoadGate::windowActivated
	The window became active. With no project being read this is what starts
	the collection load, which is what happened unconditionally before this
	class existed - a program started without a file must not wait for a
	project that is never going to come.
	@return true when the caller must start the load now
*/
bool CollectionLoadGate::windowActivated()
{
	if (m_projects_loading > 0)
	{
			//Remembered rather than dropped: the activation is the reason
			//the load exists at all, and projectLoadFinished() below is
			//where it is honoured.
		m_activation_pending = true;
		return false;
	}
	return release();
}

/**
	@brief CollectionLoadGate::projectLoadStarted
	A project started being read. Counted rather than flagged: opening a
	project pumps the event loop (the waiting dialog does, and so does every
	modal box on that path), so a second read can begin before the first one
	is over.
*/
void CollectionLoadGate::projectLoadStarted()
{
	++ m_projects_loading;
}

/**
	@brief CollectionLoadGate::projectLoadFinished
	A project finished being read, whether it loaded or failed: a project
	that failed to open is no longer competing either, so there is nothing
	left to wait for.
	@return true when the caller must start the load now
*/
bool CollectionLoadGate::projectLoadFinished()
{
	if (m_projects_loading > 0) {
		-- m_projects_loading;
	}
	if (m_projects_loading > 0) {
		return false;
	}
	return release();
}

/**
	@brief CollectionLoadGate::release
	@return true the first time it is called, false ever after - so that the
	collection is loaded once and not once per event that would have done it.
*/
bool CollectionLoadGate::release()
{
	if (m_released) {
		return false;
	}
	m_released = true;
	m_activation_pending = false;
	return true;
}
