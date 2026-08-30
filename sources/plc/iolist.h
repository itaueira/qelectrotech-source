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
#ifndef IOLIST_H
#define IOLIST_H

#include "iopoint.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

class QDomDocument;
class QDomElement;

/**
	@brief Every I/O point of a project, drawn or not.

	The list belongs to the project and not to a dialogue, because it
	outlives the dialogue: a sheet is imported on Monday, half of it is
	assigned to cards on Tuesday, and the revised sheet arrives on Friday.
	Nothing of that works if the points only exist while a window is open.

	It is deliberately a list and not a set. Order is what the person who
	typed the sheet chose, and a reimport must not shuffle it.
*/
class IoList
{
	Q_DECLARE_TR_FUNCTIONS(IoList)

	public:
		/**
			@brief What one import did to the list.

			Everything here is an id, not an index: an index means nothing
			once the next import has run, and the dialogue that shows this
			report has to be able to jump to the point afterwards.
		*/
	struct MergeReport
	{
			/// points that entered the list
		QStringList added;
			/// points an incoming line changed
		QStringList updated;
			/// points an incoming line matched without changing anything
		QStringList unchanged;
			/**
				Points already in the list that no incoming line matched.
				They are not removed - see the decision in the task file -
				they are reported, so that a person can see what the revised
				sheet stopped mentioning and decide themselves.
			*/
		QStringList missing;
			/**
				Points that entered the list because their key matched more
				than one point already there, so no single one of them could
				be said to be it. Every id here is also in added: the import
				never guesses which of the two it meant.
			*/
		QStringList ambiguous;

		bool isEmpty() const;
			/// @return the whole thing in one paragraph, ready to be shown
		QString text() const;
	};

		IoList();

		int count() const;
		bool isEmpty() const;

		const IoPoint &at(int index) const;
		IoPoint point(int index) const;
		bool setPoint(int index, const IoPoint &point);

		int indexOfId(const QString &id) const;
		/**
			@brief Find the point an incoming line is talking about.
			@param other the incoming point
			@param ambiguous set to true when the key matched more than one
			@return the index, -1 when nothing matched and -1 when several did

			The key is a cascade and not a fixed field: the tag if the sheet
			gives one, failing that the address, failing that the description
			with its case, its accents and its double spaces folded away. That
			order is what lets a two column sheet be reimported at all, and
			what lets a fuller sheet later add a tag and an address to points
			that were imported by description alone.
		*/
		int indexOfKey(const IoPoint &other, bool *ambiguous = nullptr) const;

		QString append(IoPoint point);
		bool removeAt(int index);
		void clear();

		QList<int> unassigned() const;

		/**
			@brief Bring a sheet in, without losing what is already drawn.
			@param incoming the points the sheet described
			@param fields the fields this sheet is allowed to write
			@return what was added, changed, left alone and not mentioned

			Three things this never touches, whatever is in fields: the id,
			the card a point was assigned to, and the channel it took. Those
			are the drawing, and the drawing is not the sheet's business.

			Within fields, an empty incoming value never overwrites a value
			that is there. Blanking a column is a thing a person does on
			purpose, one point at a time - not something a sheet with a
			half filled column does to ninety-six points at once.
		*/
		MergeReport merge(const QList<IoPoint> &incoming,
				  IoFields fields = AllIoFields);

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString newId();

		bool operator==(const IoList &other) const;
		bool operator!=(const IoList &other) const;

	private:
		QVector<IoPoint> m_points;
};

#endif // IOLIST_H
