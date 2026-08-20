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
#ifndef CATALOGTABLEREADER_H
#define CATALOGTABLEREADER_H

#include "catalogtable.h"

#include <QChar>
#include <QString>

/**
	@brief The CatalogTableReader class
	Turn a spreadsheet file into a CatalogTable.

	The delimiter is guessed rather than asked for: a supplier's list arrives
	with commas, semicolons or tabs depending on which locale exported it, and
	asking the user which one it is means asking a question the file already
	answers.

	Quoting follows the usual convention - a field may be quoted, a quote
	inside a quoted field is doubled, and a quoted field may contain the
	delimiter and even a line break. That last one is why this is a character
	parser and not a QString::split: a description with a comma in it split
	every row of the first real list tried.
*/
class CatalogTableReader
{
	public:
		static CatalogTable readCsv(const QString &file_path,
					    QChar delimiter = QChar(),
					    QString *error = nullptr);

		/**
			@param text : the whole file
			@param delimiter : null to guess it from the first line
		*/
		static CatalogTable parseCsv(const QString &text, QChar delimiter = QChar());

		/// @return the delimiter that appears most often outside quotes
		static QChar detectDelimiter(const QString &text);

		/**
			Read the first sheet of an xlsx workbook.

			An xlsx is a zip of XML, and Qt's zip reader lives in the private
			API of QtGui - the same private API this build already uses for
			the PDF internal links. When it is not available, this returns an
			empty table and an error saying to export the sheet as CSV, which
			is a message rather than a compile failure.
		*/
		static CatalogTable readXlsx(const QString &file_path, QString *error = nullptr);

		/// true when this build can read xlsx
		static bool xlsxSupported();

		/// Read by extension: xlsx through readXlsx, everything else as CSV
		static CatalogTable read(const QString &file_path,
					 QChar delimiter = QChar(),
					 QString *error = nullptr);

		static bool writeCsv(const QString &file_path,
				     const CatalogTable &table,
				     QChar delimiter = QLatin1Char(';'),
				     QString *error = nullptr);

		/// @return the field, quoted only when it has to be
		static QString quoteField(const QString &field, QChar delimiter);

		/// The delimiters the guess considers, in the order they are tried
		static QList<QChar> candidateDelimiters();
};

#endif // CATALOGTABLEREADER_H
