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
#include "renumberplan.h"

#include <algorithm>

namespace
{
	/**
		How far apart two coordinates have to be to count as different rows or
		columns. A symbol dropped a couple of pixels above its neighbour is on
		the same row as far as a person reading the drawing is concerned, and
		comparing raw coordinates would order those two by an accident of the
		mouse.
	*/
	const qreal ORDERING_TOLERANCE = 10.0;

	bool before(qreal first, qreal second)
	{
		return first < second - ORDERING_TOLERANCE;
	}

	bool sameBand(qreal first, qreal second)
	{
		return qAbs(first - second) <= ORDERING_TOLERANCE;
	}
}

/**
	@brief Renumberer::readingOrderLessThan
	@param first
	@param second
	@param columns_first
	@return true when @a first comes before @a second in reading order
*/
bool Renumberer::readingOrderLessThan(const RenumberInput &first,
				      const RenumberInput &second,
				      bool columns_first)
{
	if (first.folio_index != second.folio_index) {
		return first.folio_index < second.folio_index;
	}

	if (columns_first)
	{
		// Left to right, then top to bottom.
		if (!sameBand(first.position.x(), second.position.x())) {
			return before(first.position.x(), second.position.x());
		}
		if (!sameBand(first.position.y(), second.position.y())) {
			return before(first.position.y(), second.position.y());
		}
	}
	else
	{
		// Top to bottom, then left to right - the default, and the way the
		// drawings here are read.
		if (!sameBand(first.position.y(), second.position.y())) {
			return before(first.position.y(), second.position.y());
		}
		if (!sameBand(first.position.x(), second.position.x())) {
			return before(first.position.x(), second.position.x());
		}
	}

	// Two objects at the same place, within tolerance: order by uuid so that
	// the answer is the same on every station and on every run. An unstable
	// order here would make two people renumbering the same project disagree.
	return first.uuid < second.uuid;
}

/**
	@brief Renumberer::sorted
	@param inputs
	@param columns_first
	@return @a inputs in reading order
*/
QList<RenumberInput> Renumberer::sorted(const QList<RenumberInput> &inputs, bool columns_first)
{
	QList<RenumberInput> ordered = inputs;
	std::stable_sort(ordered.begin(), ordered.end(),
			 [columns_first](const RenumberInput &first, const RenumberInput &second)
	{
		return readingOrderLessThan(first, second, columns_first);
	});
	return ordered;
}

/**
	@brief Renumberer::plan
	@param inputs
	@param format
	@param columns_first
	@return what the renumbering would do
*/
RenumberPlan Renumberer::plan(const QList<RenumberInput> &inputs,
			      const NumberingFormat &format,
			      bool columns_first)
{
	QList<RenumberInput> stamped = inputs;
	for (RenumberInput &input : stamped) {
		input.format = format;
	}
	return plan(stamped, columns_first);
}

/**
	@brief Renumberer::plan
	@param inputs : each one carrying the format of its own class
	@param columns_first
	@return what the renumbering would do
*/
RenumberPlan Renumberer::plan(const QList<RenumberInput> &inputs, bool columns_first)
{
	RenumberPlan result;
	const QList<RenumberInput> ordered = sorted(inputs, columns_first);

	// One counter per scope bucket. A project scope has a single bucket, a
	// folio scope one per folio, and so on - which is the whole difference
	// between M1, M2, M3 and M201, M202.
	QHash<QString, int> counters;

	for (const RenumberInput &input : ordered)
	{
		RenumberEntry entry;
		entry.uuid = input.uuid;
		entry.from = input.current;

		if (input.frozen)
		{
			// Numbered by hand: left exactly as it is, and shown in the plan
			// so the user sees it was skipped instead of wondering why it did
			// not change.
			entry.to = input.current;
			entry.frozen = true;
			entry.changed = false;
			result.entries.append(entry);
			continue;
		}

		QString bucket;
		switch (input.format.scope)
		{
			case NumberingScope::Project:  bucket = QStringLiteral("*"); break;
			case NumberingScope::Folio:    bucket = QStringLiteral("f:") + input.folio; break;
			case NumberingScope::Rung:     bucket = QStringLiteral("r:") + input.folio
								  + QLatin1Char('/') + input.rung; break;
			case NumberingScope::Location: bucket = QStringLiteral("l:") + input.location; break;
		}
		// The root joins the bucket: contactors and motors each count from one,
		// which is what makes K1, K2 and M1, M2 instead of K1, M2. The format
		// name joins it too, so that two classes numbered by different rules
		// do not share a counter and collide.
		bucket += QLatin1Char('|') + input.root
			  + QLatin1Char('|') + input.format.name;

		QHash<QString, QString> context;
		context.insert(QStringLiteral("folio"), input.folio);
		context.insert(QStringLiteral("rung"), input.rung);
		context.insert(QStringLiteral("location"), input.location);

		const int counter = counters.value(bucket, 0);
		counters.insert(bucket, counter + 1);

		entry.to = input.format.render(input.root, counter, context);
		entry.changed = entry.to != entry.from;
		result.entries.append(entry);
	}

	return result;
}

/**
	@brief Renumberer::holderOf
	@param label
	@param inputs
	@param except_uuid
	@return the uuid of whoever already carries @a label
*/
QString Renumberer::holderOf(const QString &label,
			     const QList<RenumberInput> &inputs,
			     const QString &except_uuid)
{
	for (const RenumberInput &input : inputs)
	{
		if (input.uuid == except_uuid) {
			continue;
		}
		if (input.current == label) {
			return input.uuid;
		}
	}
	return QString();
}

/**
	@brief Renumberer::isLabelFree
	@param label
	@param location
	@param inputs
	@param except_uuid
	@return true when @a label may be used
*/
bool Renumberer::isLabelFree(const QString &label,
			     const QString &location,
			     const QList<RenumberInput> &inputs,
			     const QString &except_uuid)
{
	for (const RenumberInput &input : inputs)
	{
		if (input.uuid == except_uuid) {
			continue;
		}
		if (input.current != label) {
			continue;
		}
		// The same label in two different locations is legitimate: two panels
		// may each have their own -Q1. Same location, same label is not.
		if (input.location == location) {
			return false;
		}
	}
	return true;
}

/**
	@brief RenumberPlan::changeCount
	@return how many labels would actually change
*/
int RenumberPlan::changeCount() const
{
	int count = 0;
	for (const RenumberEntry &entry : entries)
	{
		if (entry.changed) {
			++count;
		}
	}
	return count;
}

/**
	@brief RenumberPlan::frozenCount
	@return how many objects were left alone
*/
int RenumberPlan::frozenCount() const
{
	int count = 0;
	for (const RenumberEntry &entry : entries)
	{
		if (entry.frozen) {
			++count;
		}
	}
	return count;
}

/**
	@brief RenumberPlan::duplicates
	@return the labels the plan would give to more than one object
*/
QStringList RenumberPlan::duplicates() const
{
	QHash<QString, int> counts;
	for (const RenumberEntry &entry : entries)
	{
		if (!entry.to.isEmpty()) {
			counts.insert(entry.to, counts.value(entry.to, 0) + 1);
		}
	}

	QStringList repeated;
	const QStringList labels = counts.keys();
	for (const QString &label : labels)
	{
		if (counts.value(label) > 1) {
			repeated.append(label);
		}
	}
	repeated.sort();
	return repeated;
}

/**
	@brief RenumberPlan::hasDuplicates
	@return true when the plan would produce a duplicate
*/
bool RenumberPlan::hasDuplicates() const
{
	return !duplicates().isEmpty();
}

/**
	@brief RenumberPlan::labelFor
	@param uuid
	@return the new label of @a uuid
*/
QString RenumberPlan::labelFor(const QString &uuid) const
{
	for (const RenumberEntry &entry : entries)
	{
		if (entry.uuid == uuid) {
			return entry.to;
		}
	}
	return QString();
}
