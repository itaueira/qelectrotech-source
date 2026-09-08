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
#include <QMap>
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
			The connector this pin belongs to, as the draughtsman wrote it on
			the pin: what %{connector} becomes, and what the counter of a
			connector scoped format restarts on (T34).

			Free text, and empty for anything that is not a pin. Nothing
			normalises it on the way in, so "CN1", "cn1" and "CN1 " are three
			different strings for one connector; Renumberer::connectorKey is
			where they are made to count as one.
		*/
		QString connector;
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
		/**
			The identity space this tag lives in, empty for almost everything.

			A tag is normally unique in the whole project, and two objects
			carrying the same one is the mistake a renumbering must never make.
			The way of a connector is the exception, and it is one by design
			(T34): the pin carries the way number alone, and the connector it
			sits in is the rest of its identity - so way 1 of XS1 and way 1 of
			XS2 are two different things that both read "1".

			Without this, numbering two connectors at once would report every
			way as a double and ask the user to confirm each time, which teaches
			them to click through the one warning that matters.
		*/
		QString group;
		bool frozen = false;    ///< left alone because it was set by hand
		/**
			Left alone because the format had nothing to say about it: numbering
			by connector met a component that belongs to no connector (T34).

			A different thing from frozen, and shown as a different thing: the
			user did not protect this one, the rule passed it over. Handing it
			the first free number of a nameless group would be an answer made up
			out of nothing, and dropping it from the plan would leave the doubt
			of whether it was renumbered unseen.
		*/
		bool skipped = false;
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
		/**
			One entry per connector the plan numbered: the name that went into
			the labels, and every spelling that name was written with on a pin.

			Two spellings in one list is the draughtsman having typed the same
			connector two ways. They were counted as one connector - counting
			them apart is what would restart the numbering halfway and hand two
			ways the same number - but they are reported rather than merged in
			silence, so the field can be put right.
		*/
		QMap<QString, QStringList> connector_spellings;

		int changeCount() const;
		int frozenCount() const;
		int skippedCount() const;
		/// The connector names that were written more than one way
		QStringList inconsistentConnectors() const;
		/// The labels that would appear more than once inside one group
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
		/**
			@param connector : a connector name as it was typed on a pin
			@return what makes two of those names the same connector.

			The ends are trimmed and the case is folded, and nothing else: "CN1",
			"cn1" and "CN1 " are one connector, while "CN 1" is another one,
			because a space in the middle of a name is a name and not a slip.

			The normalising happens here, on the way in, and never on the way
			out: what the pin carries stays what the user typed. Renumbering is
			allowed to change a tag, and it is not allowed to quietly rewrite a
			field the user filled in by hand. The day a connector management
			window becomes the only thing that writes the field, it is this same
			function it has to agree with.
		*/
		static QString connectorKey(const QString &connector);

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
