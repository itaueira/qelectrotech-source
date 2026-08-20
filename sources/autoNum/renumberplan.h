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
#ifndef RENUMBERPLAN_H
#define RENUMBERPLAN_H

#include "numberingformat.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

/**
	@brief The RenumberInput class
	One object as the renumbering sees it. Everything already resolved: the
	position on its folio, the tag root of its class, the format to use, and
	whether it was numbered by hand.

	Resolved on purpose. Working out a tag root means asking the catalog, and
	working out a rung means asking the folio for its coordinate system; doing
	either in here would mean the ordering rules could not be tested without a
	project open, and the ordering rules are the part that goes wrong.
*/
class RenumberInput
{
	public:
		QString uuid;         ///< identity, so the plan can be applied later
		QString current;      ///< the label the object has now
		QString root;         ///< tag root of its class
		int folio_index = 0;  ///< 0-based position of the folio in the project
		QPointF position;     ///< position on the folio, scene coordinates
		bool frozen = false;  ///< numbered by hand: never touched again
		QString folio;        ///< what %{folio} becomes
		QString rung;         ///< what %{rung} becomes
		QString location;     ///< what %{location} becomes
		/**
			The format to number this object with, taken from its class.

			Per object and not per run, because the registered decision of
			T07 is that the rule lives on the class: contactors may be
			numbered sequentially while wires are numbered by folio, in the
			same project and in the same command.
		*/
		NumberingFormat format;
};

/**
	@brief One line of the "from → to" preview.
*/
class RenumberEntry
{
	public:
		QString uuid;
		QString from;
		QString to;
		bool frozen = false;    ///< left alone because it was set by hand
		bool changed = false;
};

/**
	@brief The RenumberPlan class
	What a renumbering would do, before it does it.

	Renumbering blind is worse than not renumbering, so nothing is applied
	until this has been shown. It also carries the answer to the question
	that matters: does the result contain a duplicate?
*/
class RenumberPlan
{
	public:
		QList<RenumberEntry> entries;

		int changeCount() const;
		int frozenCount() const;
		/// The labels that would appear more than once, if any
		QStringList duplicates() const;
		bool hasDuplicates() const;
		/// The new label of @a uuid, empty when it is not in the plan
		QString labelFor(const QString &uuid) const;
};

/**
	@brief The Renumberer class
	Turns a list of objects into a plan.
*/
class Renumberer
{
	public:
		/**
			@brief Reading order.
			@param columns_first : false for the default, top to bottom then
			left to right; true for left to right then top to bottom.

			The default is the one the drawings here are read in. It is a
			setting of the environment and not of the command, because two
			people renumbering the same project have to get the same answer.
		*/
		static bool readingOrderLessThan(const RenumberInput &first,
						 const RenumberInput &second,
						 bool columns_first);

		/// @a inputs sorted into reading order, folio by folio
		static QList<RenumberInput> sorted(const QList<RenumberInput> &inputs,
						   bool columns_first);

		/**
			@param inputs : each one carrying the format of its own class
			@param columns_first
			@return what the renumbering would do.

			An object that was numbered by hand keeps its label and appears in
			the plan marked frozen: the user has to see that it was skipped,
			not wonder why it did not change.
		*/
		static RenumberPlan plan(const QList<RenumberInput> &inputs,
					 bool columns_first = false);

		/**
			The same, with one format for everything. For the default the
			dialog offers, and for a project whose classes declare no format
			of their own.
		*/
		static RenumberPlan plan(const QList<RenumberInput> &inputs,
					 const NumberingFormat &format,
					 bool columns_first = false);

		/**
			@param label
			@param inputs
			@return the uuid of the object already carrying @a label, empty
			when none does. This is what "go to" needs in order to show the
			user where the collision is, instead of only refusing.
		*/
		static QString holderOf(const QString &label,
					const QList<RenumberInput> &inputs,
					const QString &except_uuid = QString());

		/**
			@return true when @a label may be given to @a uuid.
			The same label in two different locations is legitimate - two
			cabinets may each have their own -Q1 - so the location is part of
			the comparison and not ignored.
		*/
		static bool isLabelFree(const QString &label,
					const QString &location,
					const QList<RenumberInput> &inputs,
					const QString &except_uuid = QString());
};

#endif // RENUMBERPLAN_H
