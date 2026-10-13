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
#ifndef NAVIGATIONHISTORY_H
#define NAVIGATIONHISTORY_H

#include <QList>
#include <QtGlobal>

/**
	@brief The NavigationHistory class
	The places a reader went through, in the order they went, and where in
	that walk they stand now: what the Back and Forward buttons of a web
	browser retrace. It is a rule and nothing else. It knows no widget, so a
	place is whatever its owner compares by identity - a folio view, for the
	project view that keeps one of these.

	Going somewhere new after going back drops the places that were ahead,
	as a browser does. Visiting the place one already stands on records
	nothing, and Back and Forward move inside the walk without recording.
*/
template <typename Place>
class NavigationHistory
{
	public:
		/**
			@param limit : how many places are kept; past it the oldest
			one goes. Never less than one.
		*/
		explicit NavigationHistory(int limit = 100) :
			m_limit(qMax(1, limit))
		{}

		/**
			@brief visit
			The reader reached @a place by any way other than Back and
			Forward.
		*/
		void visit(const Place &place)
		{
			if (m_index >= 0 && m_places.at(m_index) == place) {
				return;
			}
			while (m_places.size() > m_index + 1) {
				m_places.removeLast();
			}
			m_places.append(place);
			while (m_places.size() > m_limit) {
				m_places.removeFirst();
			}
			m_index = static_cast<int>(m_places.size()) - 1;
		}

		bool canGoBack() const {
			return m_index > 0;
		}

		bool canGoForward() const {
			return m_index >= 0 && m_index < m_places.size() - 1;
		}

		/**
			@brief back
			@return the place before the current one, which becomes the
			current one. The current place when there is none before it.
		*/
		Place back()
		{
			if (canGoBack()) {
				--m_index;
			}
			return current();
		}

		/**
			@brief forward
			@return the place one went back from, which becomes the
			current one. The current place when there is none ahead.
		*/
		Place forward()
		{
			if (canGoForward()) {
				++m_index;
			}
			return current();
		}

		/**
			@return where the reader stands, or a default-built place
			when nothing was visited yet.
		*/
		Place current() const {
			return m_index >= 0 ? m_places.at(m_index) : Place();
		}

		/**
			@brief remove
			@a place no longer exists: every visit to it goes, and two
			visits it separated become one. When it was the current place,
			the one visited before it becomes current, or the next one when
			there was none before.
		*/
		void remove(const Place &place)
		{
			QList<Place> kept;
			int index = -1;
			for (int i = 0 ; i < m_places.size() ; ++i)
			{
				const Place &visited = m_places.at(i);
				if (!(visited == place)
				    && (kept.isEmpty() || !(kept.last() == visited))) {
					kept.append(visited);
				}
				if (i <= m_index && !kept.isEmpty()) {
					index = static_cast<int>(kept.size()) - 1;
				}
			}
			m_places = kept;
			m_index = m_places.isEmpty() ? -1 : qMax(index, 0);
		}

		void clear()
		{
			m_places.clear();
			m_index = -1;
		}

		/**
			@return every place kept, oldest first.
		*/
		QList<Place> places() const {
			return m_places;
		}

		/**
			@return the position of the current place in places(), or -1
			when nothing was visited yet.
		*/
		int currentIndex() const {
			return m_index;
		}

	private:
		QList<Place> m_places;
		int m_index = -1;
		int m_limit;
};

#endif // NAVIGATIONHISTORY_H
