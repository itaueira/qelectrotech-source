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
#ifndef OPTIONTREE_H
#define OPTIONTREE_H

#include "projectoption.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

class QDomDocument;
class QDomElement;

/**
	@brief Every configuration a template project can be built in, and
	which of them is switched on right now.

	The tree belongs to the project, for the same reason the tree of
	locations does: a configuration outlives every panel that ever showed
	it. It is stored flat, each option naming its parent, because that is
	what survives a file round trip and what makes moving a branch one
	assignment instead of a splice.

	Two questions this class answers and which are deliberately not the
	same one:

	- isSwitchedOn() is what the person clicked. It is stored.
	- isActive() is whether the option applies: switched on, and every
	  option above it switched on too. It is worked out, never stored.

	Keeping both is what lets a sub-option survive its parent being turned
	off. "Bidirectionnel" carries "avec frein"; turning the first off must
	take the second with it on the drawing, and must give it back untouched
	when the first comes back - which is the third line of the task: ligar
	de novo, tudo volta na hora, sem regenerar nada. Deducing the stored
	flag from the effective answer would lose that choice for good.

	What an option is worth - the difference it makes to the drawing - is
	not here. This file only says which configurations exist and which of
	them the project is in; the difference itself is a block of its own,
	beside this one, so that a program that knows nothing of options still
	opens the file and draws the base state.

	This file is arithmetic and nothing else. No project, no element, no
	widget - which is what lets the invariants be tested on a bench.
*/
class OptionTree
{
	Q_DECLARE_TR_FUNCTIONS(OptionTree)

	public:
		OptionTree();

		int count() const;
		bool isEmpty() const;
		void clear();

		const ProjectOption &at(int index) const;
		ProjectOption option(const QString &uuid) const;
		int indexOfUuid(const QString &uuid) const;

		/**
			@brief Put an option in the tree.
			@param option the option, its uuid given here when it has none
			@param error filled with why nothing was added
			@return the uuid of the option added, empty when it was refused

			Refuses an empty name, a parent that is not in the tree, and a
			name a sibling already answers to - the third because two
			options of one level sharing a sentence is a tree nobody can
			read, and this feature is useless the moment the person cannot
			tell which switch does what.
		*/
		QString append(ProjectOption option, QString *error = nullptr);

		/**
			@brief Write an option back, however it was changed.
			@param option the option as it should now be, matched by uuid
			@param error filled with why nothing was written
			@return true when the tree was changed

			One entry point for the four things a person does to an
			option - rename it, describe it, nest it elsewhere, switch it -
			because an undo command wants one operation and not four.

			False with an empty error means the option handed in was the
			one already there: not a failure, but the guard that keeps a
			dialogue opened and closed from marking a project modified.
		*/
		bool update(const ProjectOption &option, QString *error = nullptr);

		/**
			@brief Take an option, and everything under it, out.
			@param uuid what to remove
			@param removed filled with the uuid of every option that went
			@return true when something was removed

			The branch goes with it: a sub-option of an option that no
			longer exists refines nothing. What becomes of the differences
			recorded against those uuids is not decided here - the caller
			is handed the list and answers for it.
		*/
		bool remove(const QString &uuid, QStringList *removed = nullptr);

		/**
			@brief Turn one option on or off.
			@param uuid the option
			@param on what it should now be
			@return true when it changed

			Its sub-options are not touched, and that is the whole point:
			see isActive().
		*/
		bool setSwitchedOn(const QString &uuid, bool on);

		QStringList rootUuids() const;
		QStringList childUuids(const QString &parent_uuid) const;
		QStringList descendantUuids(const QString &uuid) const;
			/// @return the options above this one, the nearest first
		QStringList ancestorUuids(const QString &uuid) const;
		int depth(const QString &uuid) const;

			/// @return the names down to this option, for a person to read
		QString displayPath(const QString &uuid) const;

			/// @return what the person clicked, stored as it was clicked
		bool isSwitchedOn(const QString &uuid) const;

		/**
			@brief Whether this option applies to the drawing.
			@param uuid the option
			@return true when it is switched on and so is every option
			above it
		*/
		bool isActive(const QString &uuid) const;

		/**
			@brief The set the drawing is resolved against.
			@return the uuid of every active option, parents before
			children

			This is what a stored difference is matched against, and why
			the order is fixed rather than incidental: two runs of the
			same project have to produce the same set, in the same order,
			or a comparison made on it would answer differently for no
			reason a person could see.
		*/
		QStringList activeUuids() const;

			/// @return the name of every active option, in the same order
		QStringList activeNames() const;

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString newId();

		bool operator==(const OptionTree &other) const;
		bool operator!=(const OptionTree &other) const;

	private:
		int indexOfSiblingName(const QString &parent_uuid,
				       const QString &name,
				       const QString &except_uuid = QString()) const;
		bool isDescendantOf(const QString &uuid,
				    const QString &ancestor_uuid) const;

		QVector<ProjectOption> m_options;
};

#endif // OPTIONTREE_H
