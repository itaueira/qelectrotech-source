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
#ifndef COLLECTIONLOADGATE_H
#define COLLECTIONLOADGATE_H

/**
	@brief Decides when the elements collection may start loading.

	Loading the collection walks every .elmt of every collection directory
	and parses each one, spread over the global thread pool. Reading a
	project does the same kind of work on the same cores at the same moment,
	and the two together take far longer than one after the other: the sheet
	the user is waiting for queues behind thousands of element files it does
	not need yet.

	So the collection waits for the project. Not for the window, not for a
	click: only for the project that is already being read, and only when
	there is one. This class holds that rule alone - no widget, no timer, no
	Qt - so that the rule can be checked without a window, and so that the
	two call sites cannot disagree about it.

	The rule, in full:

	- the load is handed out at most once; every later event is a no-op;
	- while a project is being read, the window becoming active does not
	  hand it out - it is remembered and the load waits;
	- when the last project being read is done, the load is handed out;
	- with no project at all, the window becoming active hands it out
	  immediately, exactly as before this class existed. **That case is the
	  one to keep in mind**: a gate that waits for a project when the
	  program was started empty waits forever, and the panel stays blank.

	Both windowActivated() and projectLoadFinished() answer the same
	question - "must the caller start the load now?" - and exactly one call
	over the life of the gate can answer true.
*/
class CollectionLoadGate
{
	public:
		CollectionLoadGate() {}

		bool windowActivated();
		void projectLoadStarted();
		bool projectLoadFinished();

		/// Whether the load has already been handed out.
		bool loadReleased() const {return m_released;}
		/// How many project reads are in flight right now.
		int projectsLoading() const {return m_projects_loading;}
		/// Whether the window became active while a project was being read.
		bool activationPending() const {return m_activation_pending;}

	private:
		bool release();

		bool m_released = false;
		bool m_activation_pending = false;
		int m_projects_loading = 0;
};

#endif // COLLECTIONLOADGATE_H
