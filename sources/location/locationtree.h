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
#ifndef LOCATIONTREE_H
#define LOCATIONTREE_H

#include "projectlocation.h"

#include <QCoreApplication>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

class QDomDocument;
class QDomElement;

/**
	@brief Every place a project has to mount something in, and who is
	inside whom.

	The tree belongs to the project, for the same reason the I/O list does:
	an enclosure outlives every dialogue that ever showed it. It is stored
	flat, each location naming its parent, because that is what survives a
	file round trip and what makes moving a branch one assignment instead of
	a splice.

	A component does not hold a location. It holds the path of codes down to
	one - QCM1/PORTE - as an ordinary element information, so that it reads
	as itself in the nomenclature the storeroom is handed, and so that a
	project of ours opened in stock QElectroTech keeps it. The price of that
	choice is here: renaming a code rewrites the components below it, which
	is why setCode and move hand back exactly which paths changed into which.

	This file is arithmetic and nothing else. No project, no element, no
	widget - which is what lets the invariants be tested on a bench.
*/
class LocationTree
{
	Q_DECLARE_TR_FUNCTIONS(LocationTree)

	public:
		/**
			@brief One line of the bill of material the locations produce.

			The storeroom does not want to read fourteen doors: it wants to
			read one door, quantity fourteen, and to be told which fourteen
			when it asks. Hence the paths.
		*/
		struct BomLine
		{
				/// the catalogue part these locations were bought as
			QString part_code;
				/// which revision of it, 0 when whatever is current will do
			int part_revision = 0;
				/// the name of the first location that used this part
			QString name;
				/// how many locations of the project use it
			int quantity = 0;
				/// which ones, by path, in the order the tree gives them
			QStringList paths;
		};

		LocationTree();

		int count() const;
		bool isEmpty() const;
		void clear();

		const ProjectLocation &at(int index) const;
		ProjectLocation location(const QString &uuid) const;
		int indexOfUuid(const QString &uuid) const;
		int indexOfPath(const QString &path) const;
		QString uuidOfPath(const QString &path) const;

		/**
			@brief Put a location in the tree.
			@param location the location, its uuid given here when it has none
			@param error filled with why nothing was added
			@return the uuid of the location added, empty when it was refused

			Refuses an unusable code, a parent that is not in the tree, and a
			code a sibling already answers to - the three things that would
			leave the tree with a path that means two places at once.
		*/
		QString append(ProjectLocation location, QString *error = nullptr);

		/**
			@brief Write a location back, wherever it moved to.
			@param location the location as it should now be, matched by uuid
			@param moved filled with the paths that changed, old to new
			@param error filled with why nothing was written
			@return true when the tree was changed

			One entry point for the three things a person does to a location -
			rename it, describe it, move it - because an undo command wants
			one operation and not three, and because only the operation that
			did the change can say which components have to follow it.

			moved holds one entry per path that actually changed, the branch
			below included. Changing only the name leaves it empty.
		*/
		bool update(const ProjectLocation &location,
			    QMap<QString, QString> *moved = nullptr,
			    QString *error = nullptr);

		/**
			@brief Move a location, and everything under it, elsewhere.
			@param uuid what to move
			@param new_parent_uuid where to, empty for the top level
			@param moved filled with the paths that changed, old to new
			@param error filled with why nothing was moved
			@return true when the tree was changed
		*/
		bool move(const QString &uuid,
			  const QString &new_parent_uuid,
			  QMap<QString, QString> *moved = nullptr,
			  QString *error = nullptr);

		/**
			@brief Take a location, and everything under it, out.
			@param uuid what to remove
			@param removed filled with every path that stopped existing
			@return true when something was removed

			The branch goes with it. What the components that pointed at those
			paths become is not decided here - the caller is handed the list
			and answers for it, because the answer is a question for a person.
		*/
		bool remove(const QString &uuid, QStringList *removed = nullptr);

		QStringList rootUuids() const;
		QStringList childUuids(const QString &parent_uuid) const;
		QStringList descendantUuids(const QString &uuid) const;
		int depth(const QString &uuid) const;

			/// @return the codes down to this location, joined: QCM1/PORTE
		QString path(const QString &uuid) const;
			/// @return the names down to this location, for a person to read
		QString displayPath(const QString &uuid) const;
			/// @return every path of the tree, parents before children
		QStringList paths() const;

		/**
			@brief Turn a path into the designation the drawing carries.
			@param path a path of codes, QCM1/PORTE
			@return +QCM1+PORTE, the repeated prefix form of IEC 81346

			The norm writes a lower level inside a higher one by repeating the
			prefix rather than by nesting brackets, which is why the path
			converts by replacing a separator and not by parsing.
		*/
		static QString iecTag(const QString &path);
		static QStringList splitPath(const QString &path);
		static QString joinPath(const QStringList &codes);

		/**
			@brief LocationTree::rewrittenPath
			The path a component should carry after the tree moved under
			it.
			@param path the path the component carries today
			@param changed old path to new path, the way update and move
			report it
			@return the path to write, empty when the location stopped
			existing

			update and move fill their map with one entry per path that
			really changed, for the whole branch and not only for the
			location the caller named - see pathsOf. A component sitting
			three levels down is therefore found by a plain lookup, and no
			prefix matching is needed nor wanted: renaming QCM1 must not
			rewrite a component of QCM10.

			remove reports a list instead of a map, because nothing
			replaces what is gone. lostPaths turns that list into the same
			shape, mapping every path to an empty one - and empty is what
			not assigned means here.
		*/
		static QString rewrittenPath(const QString &path,
					     const QMap<QString, QString> &changed);
		static QMap<QString, QString> lostPaths(const QStringList &removed);

		/**
			@brief What the locations themselves cost.
			@return one line per part, quantities added up

			Only what is not virtual - see ProjectLocation::isVirtual. The
			components mounted inside are somebody else's list; this one is
			the enclosures, the plates and the boxes.
		*/
		QList<BomLine> bomLines() const;

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString newId();

		bool operator==(const LocationTree &other) const;
		bool operator!=(const LocationTree &other) const;

	private:
		int indexOfSiblingCode(const QString &parent_uuid,
				       const QString &code,
				       const QString &except_uuid = QString()) const;
		bool isDescendantOf(const QString &uuid,
				    const QString &ancestor_uuid) const;
		QMap<QString, QString> pathsOf(const QStringList &uuids) const;

		QVector<ProjectLocation> m_locations;
};

#endif // LOCATIONTREE_H
