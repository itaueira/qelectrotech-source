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
#ifndef UPDATECOALESCER_H
#define UPDATECOALESCER_H

/**
	@brief Decides when a change to the project data base is announced.

	Every announcement makes each list drawn on a folio re-run its query,
	so the count of announcements is the cost. One change means one
	announcement, and that is right; but a single gesture of the draughtsman
	can change dozens of rows - renaming a wire renames every conductor of
	its potential, and a renumbering renames every conductor of the project
	in one pass - and there the answer has to be one announcement for the
	whole gesture, not one per row.

	This class holds that rule alone - no signal, no timer, no Qt - so that
	it can be checked without a project open, and so that the call sites
	cannot disagree about it. What emits the signal is projectDataBase; what
	opens and closes an operation is whoever knows a gesture has begun.

	The rule, in full:

	- with no operation open, notify() answers true: the change is announced
	  on the spot, which is what a single edit needs;
	- while an operation is open, notify() answers false and remembers that
	  something happened;
	- closing the operation announces once, and only if something did
	  happen. An operation that changed nothing announces nothing - a
	  bracket that turned out to be a no-op must not redraw the folio;
	- operations nest, and only the outermost close announces. An operation
	  that calls another one would otherwise announce twice, which is the
	  storm this exists to stop, one level deeper;
	- a close with no operation open does nothing and answers false. It
	  neither announces - there was nothing held back - nor drives the depth
	  below zero, which would silently swallow the announcement of the next
	  real operation.
*/
class UpdateCoalescer
{
	public:
		UpdateCoalescer() {}

		void beginOperation();
		bool endOperation();

		/// @return true when the caller must announce the change now
		bool notify();

		/// Whether a gesture is being grouped right now.
		bool isOperationOpen() const {return m_depth > 0;}
		/// How many nested operations are open.
		int depth() const {return m_depth;}
		/// Whether an announcement is being held back.
		bool isPending() const {return m_pending;}

	private:
		int m_depth = 0;
		bool m_pending = false;
};

#endif // UPDATECOALESCER_H
