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
#include "labelcsvwriter.h"

#include "../utils/csvwriter.h"

#include <QSaveFile>

namespace LabelCsvWriter
{

namespace {

/// A number of the entry as a cell: empty when it does not apply.
QString numberCell(int value)
{
		//-1 is how LabelEntry says "not a terminal", and a -1 in a column a
		//person reads as a place on the rail would read as a place.
	return value < 0 ? QString() : QString::number(value);
}

/// The end of a row of the file.
QString rowEnd()
{
		//Written out rather than left to a text-mode device, so that the
		//file is the same bytes on every system it is exported from.
	return QStringLiteral("\r\n");
}

} // namespace

QString separator()
{
	return QStringLiteral(";");
}

QStringList header()
{
		//The label first - its kind, its tag, the lines under the tag - and
		//then what it is checked against: the rail and the place on it, the
		//level of the terminal, the location, the sheet and its revision,
		//and whether the item was kept off the purchase list.
	static const QStringList list {
		QStringLiteral("kind"),
		QStringLiteral("text"),
		QStringLiteral("lines"),
		QStringLiteral("strip"),
		QStringLiteral("position"),
		QStringLiteral("level"),
		QStringLiteral("location"),
		QStringLiteral("folio"),
		QStringLiteral("revision"),
		QStringLiteral("excluded_from_bom")
	};
	return list;
}

QString kindKey(LabelSource source)
{
	switch (source)
	{
		case LabelSource::Component:
			return QStringLiteral("component");
		case LabelSource::Terminal:
			return QStringLiteral("terminal");
		case LabelSource::Wire:
			return QStringLiteral("wire");
		case LabelSource::Cable:
			return QStringLiteral("cable");
		case LabelSource::CabinetDoor:
			return QStringLiteral("door");
	}
	return QStringLiteral("unknown");
}

QStringList cells(const LabelEntry &entry)
{
		//In the order of header(), one for one. The lines under the tag are
		//one cell: their number changes from one label to the next, and a
		//row whose width changed with it would shift every column after it.
		//A line feed between them, which QETCsv quotes, and which a
		//spreadsheet shows as one cell on several lines.
	return QStringList {
		kindKey(entry.source),
		entry.primary_text,
		entry.secondary_texts.join(QStringLiteral("\n")),
		entry.strip,
		numberCell(entry.strip_position),
		numberCell(entry.level),
		entry.location,
		entry.folio,
		entry.revision,
		entry.excluded_from_bom ? QStringLiteral("true")
					: QStringLiteral("false")
	};
}

QString text(const LabelEntryList &entries)
{
	QString csv = QETCsv::row(header(), separator()) + rowEnd();
	for (const LabelEntry &entry : entries) {
		csv += QETCsv::row(cells(entry), separator()) + rowEnd();
	}
	return csv;
}

QByteArray bytes(const LabelEntryList &entries)
{
	QByteArray data("\xEF\xBB\xBF", 3);
	data += text(entries).toUtf8();
	return data;
}

QString write(const LabelEntryList &entries, const QString &path)
{
		//The whole text is built before the file is opened, so that
		//nothing about the list can fail once the file is half written.
	const QByteArray data = bytes(entries);

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

} // namespace LabelCsvWriter
