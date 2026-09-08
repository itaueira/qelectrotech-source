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
	@brief Renumberer::connectorKey
	@param connector
	@return what makes two connector names the same connector
*/
QString Renumberer::connectorKey(const QString &connector)
{
	return connector.trimmed().toCaseFolded();
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

	// Which spelling of a connector name goes into the labels, worked out
	// before anything is numbered because the answer has to be the same for
	// every pin of the connector - including the ones met later.
	//
	// The first pin in reading order names the connector. Deterministic, and
	// explainable in one line to whoever asks why the label says CN1 when the
	// field says cn1; the spellings met are kept beside it so the answer is
	// available instead of having to be guessed.
	QHash<QString, QString> connector_names;
	for (const RenumberInput &input : ordered)
	{
		if (input.format.scope != NumberingScope::Connector) {
			continue;
		}
		const QString key = connectorKey(input.connector);
		if (key.isEmpty()) {
			continue;
		}

		const QString spelling = input.connector.trimmed();
		if (!connector_names.contains(key)) {
			connector_names.insert(key, spelling);
		}
		QStringList &seen = result.connector_spellings[connector_names.value(key)];
		if (!seen.contains(spelling)) {
			seen.append(spelling);
		}
	}

	// One counter per scope bucket. A project scope has a single bucket, a
	// folio scope one per folio, and so on - which is the whole difference
	// between M1, M2, M3 and M201, M202.
	QHash<QString, int> counters;

	for (const RenumberInput &input : ordered)
	{
		RenumberEntry entry;
		entry.uuid = input.uuid;
		entry.from = input.current;
		if (input.format.scope == NumberingScope::Connector)
		{
			// Set before the two branches below, so that a way numbered by hand
			// and a way passed over are compared inside their own connector as
			// well - a hand written 3 colliding with a computed 3 is a real
			// double, and it is one only within the connector.
			entry.group = connectorKey(input.connector);
		}

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

		if (input.format.scope == NumberingScope::Connector
		    && connectorKey(input.connector).isEmpty())
		{
			// Numbering by connector has nothing to say about a component that
			// belongs to no connector. It keeps its tag and says in the preview
			// why it was passed over - which is also what keeps this format
			// from renaming a whole project to bare numbers when somebody picks
			// it as the default for everything.
			entry.to = input.current;
			entry.skipped = true;
			entry.changed = false;
			result.entries.append(entry);
			continue;
		}

		QString bucket;
		bool root_in_bucket = true;
		switch (input.format.scope)
		{
			case NumberingScope::Project:  bucket = QStringLiteral("*"); break;
			case NumberingScope::Folio:    bucket = QStringLiteral("f:") + input.folio; break;
			case NumberingScope::Rung:     bucket = QStringLiteral("r:") + input.folio
								  + QLatin1Char('/') + input.rung; break;
			case NumberingScope::Location: bucket = QStringLiteral("l:") + input.location; break;
			case NumberingScope::Connector:
				bucket = QStringLiteral("c:") + connectorKey(input.connector);
				// The root stays out of this bucket, and that is a decision and
				// not an oversight. A connector has one series of ways, and the
				// symbol a way was drawn with is not what decides its number: a
				// root in the bucket would open a second series for a pin drawn
				// with another symbol and hand two ways the same number.
				//
				// The two sides of a harness are the case this leaves open, and
				// it is left open on purpose: a plug and its socket are two sides
				// of one connector, and nothing on a pin says which side it is
				// on yet. Written with one name, they share one series and run 1
				// to 2n; the way out today is the one the norm already uses -
				// naming the two sides apart.
				root_in_bucket = false;
				break;
		}
		// The root joins the bucket: contactors and motors each count from one,
		// which is what makes K1, K2 and M1, M2 instead of K1, M2. The format
		// name joins it too, so that two classes numbered by different rules
		// do not share a counter and collide.
		bucket += QLatin1Char('|') + (root_in_bucket ? input.root : QString())
			  + QLatin1Char('|') + input.format.name;

		QHash<QString, QString> context;
		context.insert(QStringLiteral("folio"), input.folio);
		context.insert(QStringLiteral("rung"), input.rung);
		context.insert(QStringLiteral("location"), input.location);
		context.insert(QStringLiteral("connector"),
			       connector_names.value(connectorKey(input.connector),
						     input.connector.trimmed()));

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
	@brief RenumberPlan::skippedCount
	@return how many objects the format had nothing to say about
*/
int RenumberPlan::skippedCount() const
{
	int count = 0;
	for (const RenumberEntry &entry : entries)
	{
		if (entry.skipped) {
			++count;
		}
	}
	return count;
}

/**
	@brief RenumberPlan::inconsistentConnectors
	@return the connector names that were written more than one way
*/
QStringList RenumberPlan::inconsistentConnectors() const
{
	QStringList names;
	for (auto it = connector_spellings.cbegin() ; it != connector_spellings.cend() ; ++it)
	{
		if (it.value().size() > 1) {
			names.append(it.key());
		}
	}
	return names;
}

/**
	@brief RenumberPlan::duplicates
	@return the labels the plan would give to more than one object
*/
QStringList RenumberPlan::duplicates() const
{
	// Counted inside the identity space of each entry, which is the whole
	// project for everything but the ways of a connector - see
	// RenumberEntry::group.
	QHash<QString, int> counts;
	QHash<QString, QString> labels_by_key;
	for (const RenumberEntry &entry : entries)
	{
		if (entry.to.isEmpty()) {
			continue;
		}
		const QString key = entry.group + QLatin1Char('|') + entry.to;
		counts.insert(key, counts.value(key, 0) + 1);
		labels_by_key.insert(key, entry.to);
	}

	QStringList repeated;
	const QStringList keys = counts.keys();
	for (const QString &key : keys)
	{
		if (counts.value(key) > 1) {
			const QString label = labels_by_key.value(key);
			if (!repeated.contains(label)) {
				repeated.append(label);
			}
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
