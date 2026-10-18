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
#include "assemblychanges.h"

#include "../utils/csvwriter.h"

#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <climits>

namespace
{
	/// A line made from an item drawn now.
	AssemblyChange changeFrom(const AssemblyItem &item,
				  AssemblyItemKind kind,
				  AssemblyAction action,
				  int source)
	{
		AssemblyChange change;
		change.kind = kind;
		change.action = action;
		change.uuid = item.uuid;
		change.new_label = item.label;
		change.folio = item.folio;
		change.position = item.position;
		change.description = item.description;
		change.part_code = item.part_code;
		change.source = source;
		return change;
	}

	/**
		The label the shop finds on the panel: the one printed when it was
		wired, and the new one only for an item that was not there.
	*/
	QString labelOnThePanel(const AssemblyChange &change)
	{
		return change.old_label.isEmpty() ? change.new_label
						  : change.old_label;
	}

	/// -1, 0 or 1, the way QString::compare answers.
	int compareText(const QString &a, const QString &b)
	{
		const int folded = QString::compare(a, b, Qt::CaseInsensitive);
		if (folded != 0) {
			return folded;
		}
			//Case aside they are equal; case decides, so that "k1" and
			//"K1" still come out in the same order on every run.
		return QString::compare(a, b, Qt::CaseSensitive);
	}

	/**
		Additions and relabellings: by folio, then by the label on the
		panel, then by what the item is, then by its identity - the last one
		only so that two lines alike in everything else keep one order.
	*/
	bool drawnBefore(const AssemblyChange &a, const AssemblyChange &b)
	{
			//An item whose folio is not known goes last rather than first:
			//0 is "not said", and it is not the folio before the first.
		const int folio_a = a.folio > 0 ? a.folio : INT_MAX;
		const int folio_b = b.folio > 0 ? b.folio : INT_MAX;
		if (folio_a != folio_b) {
			return folio_a < folio_b;
		}
		if (const int by_label = compareText(labelOnThePanel(a),
						     labelOnThePanel(b))) {
			return by_label < 0;
		}
		if (const int by_description = compareText(a.description,
							   b.description)) {
			return by_description < 0;
		}
		return a.uuid < b.uuid;
	}

	/// Removals: drawn nowhere, so by the label they carried.
	bool removedBefore(const AssemblyChange &a, const AssemblyChange &b)
	{
		if (const int by_label = compareText(a.old_label, b.old_label)) {
			return by_label < 0;
		}
		return a.uuid < b.uuid;
	}

	/// A number of a line as a cell: empty when it does not apply.
	QString numberCell(int value)
	{
			//0 is how a line says "drawn nowhere", and a 0 in a column a
			//person reads as a folio would read as a folio.
		return value > 0 ? QString::number(value) : QString();
	}

	/// The end of a row of the file.
	QString rowEnd()
	{
			//Written out rather than left to a text-mode device, so that the
			//file is the same bytes on every system it is exported from.
		return QStringLiteral("\r\n");
	}
}

int AssemblyChangeLists::count() const
{
	return int(to_add.size() + to_remove.size() + to_relabel.size());
}

bool AssemblyChangeLists::isEmpty() const
{
	return to_add.isEmpty() && to_remove.isEmpty() && to_relabel.isEmpty();
}

QList<AssemblyChange> AssemblyChangeLists::all() const
{
	return to_add + to_remove + to_relabel;
}

bool AssemblyChanges::isMarked() const
{
	return stage != AssemblyStage::InProject;
}

bool AssemblyChanges::isEmpty() const
{
	return components.isEmpty() && wires.isEmpty();
}

int AssemblyChanges::count() const
{
	return components.count() + wires.count();
}

QList<AssemblyChange> AssemblyChanges::all() const
{
	return components.all() + wires.all();
}

/**
	@brief AssemblyChanges::compareItems
	@param photographed
	@param now
	@param kind
	@return what to add, remove and relabel among the items of one kind
*/
AssemblyChangeLists AssemblyChanges::compareItems(
		const QMap<QString, QString> &photographed,
		const QList<AssemblyItem> &now,
		AssemblyItemKind kind)
{
	AssemblyChangeLists lists;
	QSet<QString> seen;

	for (int index = 0 ; index < now.size() ; ++index)
	{
		const AssemblyItem &item = now.at(index);

			//The first item under a uuid is the one the photograph knew; a
			//second one under the same uuid is a second piece to mount, and
			//an item with no uuid at all cannot be in the photograph.
		const bool known = !item.uuid.isEmpty()
				   && photographed.contains(item.uuid)
				   && !seen.contains(item.uuid);
		if (!item.uuid.isEmpty()) {
			seen.insert(item.uuid);
		}

		if (!known)
		{
			lists.to_add << changeFrom(item, kind, AssemblyAction::Add,
						   index);
			continue;
		}

		const QString before = photographed.value(item.uuid);
			//Compared exactly: "k1" and "K1" are two different labels on a
			//printed tag, and the shop has to print the second one.
		if (before != item.label)
		{
			AssemblyChange change = changeFrom(item, kind,
							   AssemblyAction::Relabel,
							   index);
			change.old_label = before;
			lists.to_relabel << change;
		}
	}

	for (auto iterator = photographed.constBegin() ;
	     iterator != photographed.constEnd() ; ++iterator)
	{
			//An empty key names nothing - AssemblyState never stores one -
			//and listing it would send the shop looking for a part that
			//never had an identity to lose.
		if (iterator.key().isEmpty() || seen.contains(iterator.key())) {
			continue;
		}

		AssemblyChange change;
		change.kind = kind;
		change.action = AssemblyAction::Remove;
		change.uuid = iterator.key();
		change.old_label = iterator.value();
		lists.to_remove << change;
	}

		//stable_sort: two lines the order cannot tell apart keep the order
		//they were found in, rather than one the sort happened to pick.
	std::stable_sort(lists.to_add.begin(), lists.to_add.end(), drawnBefore);
	std::stable_sort(lists.to_relabel.begin(), lists.to_relabel.end(),
			 drawnBefore);
	std::stable_sort(lists.to_remove.begin(), lists.to_remove.end(),
			 removedBefore);
	return lists;
}

/**
	@brief AssemblyChanges::compare
	@param photograph
	@param components
	@param wires
	@return what changed since @a photograph was taken
*/
AssemblyChanges AssemblyChanges::compare(const AssemblyState &photograph,
					 const QList<AssemblyItem> &components,
					 const QList<AssemblyItem> &wires)
{
	AssemblyChanges changes;
	changes.stage = photograph.stage;

	if (!photograph.isFrozen()) {
			//Not marked, so there is no "since": comparing against an empty
			//photograph would list every item of the drawing as one to
			//mount, and comparing against one left over from a marking
			//that was taken back would answer a question nobody is asking.
		return changes;
	}

	changes.components = compareItems(photograph.components, components,
					  AssemblyItemKind::Component);
	changes.wires = compareItems(photograph.conductors, wires,
				     AssemblyItemKind::Wire);
	return changes;
}

QString AssemblyChanges::separator()
{
	return QStringLiteral(";");
}

QStringList AssemblyChanges::csvHeader()
{
		//What to do and to what first, then the two labels - the one on the
		//panel and the one to print -, then where it is drawn and what it
		//is: the order a person reads a line of an order of service in.
	static const QStringList list {
		QStringLiteral("action"),
		QStringLiteral("kind"),
		QStringLiteral("old_label"),
		QStringLiteral("new_label"),
		QStringLiteral("folio"),
		QStringLiteral("position"),
		QStringLiteral("description"),
		QStringLiteral("part_code")
	};
	return list;
}

QString AssemblyChanges::actionKey(AssemblyAction action)
{
	switch (action)
	{
		case AssemblyAction::Add:
			return QStringLiteral("add");
		case AssemblyAction::Remove:
			return QStringLiteral("remove");
		case AssemblyAction::Relabel:
			return QStringLiteral("relabel");
	}
	return QStringLiteral("unknown");
}

QString AssemblyChanges::kindKey(AssemblyItemKind kind)
{
	switch (kind)
	{
		case AssemblyItemKind::Component:
			return QStringLiteral("component");
		case AssemblyItemKind::Wire:
			return QStringLiteral("wire");
	}
	return QStringLiteral("unknown");
}

QStringList AssemblyChanges::csvCells(const AssemblyChange &change)
{
	return QStringList {
		actionKey(change.action),
		kindKey(change.kind),
		change.old_label,
		change.new_label,
		numberCell(change.folio),
		change.position,
		change.description,
		change.part_code
	};
}

QString AssemblyChanges::csvText() const
{
	QString csv = QETCsv::row(csvHeader(), separator()) + rowEnd();
	const QList<AssemblyChange> lines = all();
	for (const AssemblyChange &change : lines) {
		csv += QETCsv::row(csvCells(change), separator()) + rowEnd();
	}
	return csv;
}

QByteArray AssemblyChanges::csvBytes() const
{
	QByteArray data("\xEF\xBB\xBF", 3);
	data += csvText().toUtf8();
	return data;
}

QString AssemblyChanges::writeCsv(const QString &path) const
{
		//The whole text is built before the file is opened, so that nothing
		//about the list can fail once the file is half written.
	const QByteArray data = csvBytes();

	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return file.errorString();
	}
	if (file.write(data) != data.size())
	{
		const QString reason = file.errorString();
		file.cancelWriting();
		return reason;
	}
	if (!file.commit()) {
		return file.errorString();
	}
	return QString();
}
