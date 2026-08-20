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
#include "catalogtablereader.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QXmlStreamReader>

// Qt ships a zip reader, but only in the private API of QtGui. This build
// already depends on that private API (Qt::GuiPrivate, for the PDF internal
// links), so using it here adds no new kind of risk - and the guard means a Qt
// that moved the header degrades to a message instead of a build failure.
#if defined(__has_include)
#	if __has_include(<private/qzipreader_p.h>)
#		include <private/qzipreader_p.h>
#		define QET_HAS_ZIP_READER 1
#	endif
#endif

/**
	@brief CatalogTableReader::candidateDelimiters
	@return the delimiters the guess considers
*/
QList<QChar> CatalogTableReader::candidateDelimiters()
{
	// Semicolon first: a spreadsheet exported by a machine set to Portuguese
	// or French writes semicolons, because the comma is the decimal separator
	// there. Guessing comma first would split every price in half.
	return { QLatin1Char(';'), QLatin1Char(','), QLatin1Char('\t'), QLatin1Char('|') };
}

/**
	@brief CatalogTableReader::detectDelimiter
	@param text
	@return the delimiter that appears most often outside quotes
*/
QChar CatalogTableReader::detectDelimiter(const QString &text)
{
	// Only the first line matters: it is the header, and a header has no
	// line breaks inside a field in any spreadsheet worth importing.
	const int end = text.indexOf(QLatin1Char('\n'));
	const QString first_line = end < 0 ? text : text.left(end);

	QChar best = candidateDelimiters().first();
	int best_count = 0;

	const QList<QChar> candidates = candidateDelimiters();
	for (const QChar candidate : candidates)
	{
		int count = 0;
		bool inside_quotes = false;
		for (const QChar character : first_line)
		{
			if (character == QLatin1Char('"')) {
				inside_quotes = !inside_quotes;
			} else if (character == candidate && !inside_quotes) {
				++count;
			}
		}
		if (count > best_count)
		{
			best_count = count;
			best = candidate;
		}
	}
	return best;
}

/**
	@brief CatalogTableReader::parseCsv
	@param text
	@param delimiter : null to guess it
	@return the table
*/
CatalogTable CatalogTableReader::parseCsv(const QString &text, QChar delimiter)
{
	CatalogTable table;
	if (text.isEmpty()) {
		return table;
	}

	const QChar separator = delimiter.isNull() ? detectDelimiter(text) : delimiter;

	QStringList current_row;
	QString field;
	bool inside_quotes = false;

	const int length = text.length();
	for (int index = 0 ; index < length ; ++index)
	{
		const QChar character = text.at(index);

		if (inside_quotes)
		{
			if (character == QLatin1Char('"'))
			{
				// A doubled quote inside a quoted field is one quote.
				if (index + 1 < length && text.at(index + 1) == QLatin1Char('"'))
				{
					field.append(QLatin1Char('"'));
					++index;
				}
				else
				{
					inside_quotes = false;
				}
			}
			else
			{
				field.append(character);
			}
			continue;
		}

		if (character == QLatin1Char('"'))
		{
			inside_quotes = true;
		}
		else if (character == separator)
		{
			current_row.append(field);
			field.clear();
		}
		else if (character == QLatin1Char('\n') || character == QLatin1Char('\r'))
		{
			// Swallow the \n of a \r\n so an empty row is not invented.
			if (character == QLatin1Char('\r')
			    && index + 1 < length && text.at(index + 1) == QLatin1Char('\n')) {
				++index;
			}
			current_row.append(field);
			field.clear();

			// A row of nothing but empty fields is a blank line, and a
			// spreadsheet export ends with several.
			bool row_is_empty = true;
			for (const QString &cell : current_row)
			{
				if (!cell.trimmed().isEmpty())
				{
					row_is_empty = false;
					break;
				}
			}
			if (!row_is_empty)
			{
				if (table.headers.isEmpty()) {
					table.headers = current_row;
				} else {
					table.rows.append(current_row);
				}
			}
			current_row.clear();
		}
		else
		{
			field.append(character);
		}
	}

	// Whatever is left when the file ends without a final line break.
	if (!field.isEmpty() || !current_row.isEmpty())
	{
		current_row.append(field);
		bool row_is_empty = true;
		for (const QString &cell : current_row)
		{
			if (!cell.trimmed().isEmpty())
			{
				row_is_empty = false;
				break;
			}
		}
		if (!row_is_empty)
		{
			if (table.headers.isEmpty()) {
				table.headers = current_row;
			} else {
				table.rows.append(current_row);
			}
		}
	}

	// Trim the headers once, here, so that every column lookup afterwards
	// does not have to.
	for (QString &header : table.headers) {
		header = header.trimmed();
	}

	return table;
}

/**
	@brief CatalogTableReader::readCsv
	@param file_path
	@param delimiter
	@param error
	@return the table, empty on failure
*/
CatalogTable CatalogTableReader::readCsv(const QString &file_path,
					 QChar delimiter,
					 QString *error)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogTableReader",
							     "Impossible de lire %1.").arg(file_path);
		}
		return CatalogTable();
	}

	QByteArray raw = file.readAll();
	file.close();

	// A spreadsheet exported as "CSV UTF-8" starts with a byte order mark, and
	// left in place it becomes part of the first header - which then matches no
	// column mapping and the whole import silently maps nothing.
	static const QByteArray utf8_bom("\xEF\xBB\xBF", 3);
	if (raw.startsWith(utf8_bom)) {
		raw.remove(0, utf8_bom.size());
	}

	return parseCsv(QString::fromUtf8(raw), delimiter);
}

namespace
{
	/**
		Turn the column part of a cell reference into a zero based index:
		A is 0, Z is 25, AA is 26. Needed because a sheet omits empty cells,
		so a row is not "the cells in order" but "the cells that exist".
	*/
	int columnIndexOf(const QString &cell_reference)
	{
		int index = 0;
		bool any = false;
		for (const QChar character : cell_reference)
		{
			if (!character.isLetter()) {
				break;
			}
			index = index * 26 + (character.toUpper().unicode() - 'A' + 1);
			any = true;
		}
		return any ? index - 1 : -1;
	}

#ifdef QET_HAS_ZIP_READER
	/// The shared strings of a workbook, in index order.
	QStringList readSharedStrings(const QByteArray &xml)
	{
		QStringList strings;
		QXmlStreamReader reader(xml);
		while (!reader.atEnd())
		{
			reader.readNext();
			if (!reader.isStartElement() || reader.name() != QLatin1String("si")) {
				continue;
			}
			// One shared string may be several runs of text; the cell shows
			// them concatenated, so that is what is stored.
			QString text;
			while (!reader.atEnd())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == QLatin1String("si")) {
					break;
				}
				if (reader.isStartElement() && reader.name() == QLatin1String("t")) {
					text += reader.readElementText();
				}
			}
			strings.append(text);
		}
		return strings;
	}

	/// The path of the first worksheet inside the archive.
	QString firstSheetPath(const QZipReader &zip)
	{
		QString fallback;
		const QVector<QZipReader::FileInfo> entries = zip.fileInfoList();
		for (const QZipReader::FileInfo &entry : entries)
		{
			if (!entry.filePath.startsWith(QLatin1String("xl/worksheets/"))
			    || !entry.filePath.endsWith(QLatin1String(".xml"))) {
				continue;
			}
			if (entry.filePath == QLatin1String("xl/worksheets/sheet1.xml")) {
				return entry.filePath;
			}
			if (fallback.isEmpty() || entry.filePath < fallback) {
				fallback = entry.filePath;
			}
		}
		return fallback;
	}
#endif
}

/**
	@brief CatalogTableReader::xlsxSupported
	@return true when this build can read xlsx
*/
bool CatalogTableReader::xlsxSupported()
{
#ifdef QET_HAS_ZIP_READER
	return true;
#else
	return false;
#endif
}

/**
	@brief CatalogTableReader::readXlsx
	@param file_path
	@param error
	@return the first sheet as a table
*/
CatalogTable CatalogTableReader::readXlsx(const QString &file_path, QString *error)
{
	CatalogTable table;

#ifndef QET_HAS_ZIP_READER
	Q_UNUSED(file_path)
	if (error) {
		*error = QCoreApplication::translate("CatalogTableReader",
						     "Cette version du programme ne sait pas lire les "
						     "classeurs xlsx. Exportez la feuille en CSV depuis le "
						     "tableur, puis importez le CSV.");
	}
	return table;
#else
	QZipReader zip(file_path);
	if (!zip.isReadable() || zip.fileInfoList().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogTableReader",
							     "%1 ne s'ouvre pas comme un classeur xlsx.")
				 .arg(file_path);
		}
		return table;
	}

	const QStringList shared =
		readSharedStrings(zip.fileData(QStringLiteral("xl/sharedStrings.xml")));

	const QString sheet_path = firstSheetPath(zip);
	if (sheet_path.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogTableReader",
							     "Le classeur %1 ne contient aucune feuille.")
				 .arg(file_path);
		}
		return table;
	}

	QXmlStreamReader reader(zip.fileData(sheet_path));
	QStringList row;
	int widest = 0;

	while (!reader.atEnd())
	{
		reader.readNext();

		if (reader.isStartElement() && reader.name() == QLatin1String("row"))
		{
			row.clear();
			continue;
		}

		if (reader.isStartElement() && reader.name() == QLatin1String("c"))
		{
			const QString reference =
				reader.attributes().value(QStringLiteral("r")).toString();
			const QString type = reader.attributes().value(QStringLiteral("t")).toString();

			// Pad up to this cell: a sheet writes only the cells that exist,
			// so a gap in the middle of a row would shift every column after
			// it if it were not filled in.
			const int column = columnIndexOf(reference);
			if (column >= 0)
			{
				while (row.size() < column) {
					row.append(QString());
				}
			}

			QString value;
			while (!reader.atEnd())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == QLatin1String("c")) {
					break;
				}
				if (!reader.isStartElement()) {
					continue;
				}
				if (reader.name() == QLatin1String("v"))
				{
					const QString raw = reader.readElementText();
					if (type == QLatin1String("s"))
					{
						bool ok = false;
						const int index = raw.toInt(&ok);
						value = (ok && index >= 0 && index < shared.size())
							? shared.at(index)
							: raw;
					}
					else
					{
						// A date arrives as a serial number, and turning it
						// into a date needs the workbook's epoch. Left raw
						// on purpose: a wrong date is worse than a number
						// the user can see is a number.
						value = raw;
					}
				}
				else if (reader.name() == QLatin1String("t"))
				{
					value = reader.readElementText();
				}
			}
			row.append(value.trimmed());
			continue;
		}

		if (reader.isEndElement() && reader.name() == QLatin1String("row"))
		{
			bool row_is_empty = true;
			for (const QString &cell : row)
			{
				if (!cell.isEmpty())
				{
					row_is_empty = false;
					break;
				}
			}
			if (!row_is_empty)
			{
				widest = qMax(widest, row.size());
				if (table.headers.isEmpty()) {
					table.headers = row;
				} else {
					table.rows.append(row);
				}
			}
			row.clear();
		}
	}

	if (reader.hasError() && error)
	{
		*error = QCoreApplication::translate("CatalogTableReader",
						     "La feuille de %1 n'a pas pu être lue : %2")
			 .arg(file_path, reader.errorString());
	}

	for (QString &header : table.headers) {
		header = header.trimmed();
	}
	return table;
#endif
}

/**
	@brief CatalogTableReader::read
	@param file_path
	@param delimiter
	@param error
	@return the table, read according to the extension
*/
CatalogTable CatalogTableReader::read(const QString &file_path,
				      QChar delimiter,
				      QString *error)
{
	if (QFileInfo(file_path).suffix().compare(QLatin1String("xlsx"),
						  Qt::CaseInsensitive) == 0) {
		return readXlsx(file_path, error);
	}
	return readCsv(file_path, delimiter, error);
}

/**
	@brief CatalogTableReader::quoteField
	@param field
	@param delimiter
	@return the field, quoted only when it has to be
*/
QString CatalogTableReader::quoteField(const QString &field, QChar delimiter)
{
	const bool needs_quotes = field.contains(delimiter)
				  || field.contains(QLatin1Char('"'))
				  || field.contains(QLatin1Char('\n'))
				  || field.contains(QLatin1Char('\r'));
	if (!needs_quotes) {
		return field;
	}

	QString quoted = field;
	quoted.replace(QLatin1Char('"'), QStringLiteral("\"\""));
	return QLatin1Char('"') + quoted + QLatin1Char('"');
}

/**
	@brief CatalogTableReader::writeCsv
	@param file_path
	@param table
	@param delimiter
	@param error
	@return true on success
*/
bool CatalogTableReader::writeCsv(const QString &file_path,
				  const CatalogTable &table,
				  QChar delimiter,
				  QString *error)
{
	QFile file(file_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogTableReader",
							     "Impossible d'écrire %1.").arg(file_path);
		}
		return false;
	}

	// The byte order mark on the way out: without it a spreadsheet opens the
	// file in the local eight-bit encoding and every accent is broken, which
	// is the first thing purchasing notices.
	file.write(QByteArray("\xEF\xBB\xBF", 3));

	QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	stream.setEncoding(QStringConverter::Utf8);
#else
	stream.setCodec("UTF-8");
#endif

	QStringList quoted_headers;
	for (const QString &header : table.headers) {
		quoted_headers.append(quoteField(header, delimiter));
	}
	stream << quoted_headers.join(delimiter) << "\r\n";

	for (const QStringList &row : table.rows)
	{
		QStringList quoted_cells;
		for (const QString &cell : row) {
			quoted_cells.append(quoteField(cell, delimiter));
		}
		stream << quoted_cells.join(delimiter) << "\r\n";
	}

	stream.flush();
	file.close();
	return true;
}
