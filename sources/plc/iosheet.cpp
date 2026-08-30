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
#include "iosheet.h"

namespace
{
		/// One field, and the column names that feed it.
	struct HeaderNames
	{
		IoField field;
		const char *names;
	};

		/**
			The names are separated by a vertical bar and are already folded:
			lower case, no accent, single spaces. What a person types in that
			cell in Portuguese, in French and in English, which is what the
			three sheets this fork has seen actually use.
		*/
	const HeaderNames header_names[] = {
		{IoTypeField,
		 "tipo|tipo de e/s|tipo es|tipo io|tipo de sinal|type|type e/s|"
		 "type d'e/s|io|e/s|es|i/o|io type|signal|sinal"},
		{IoTagField,
		 "tag|etiqueta|identificacao|identificador|mnemonico|nome|"
		 "repere|nom|name|label"},
		{IoDescriptionField,
		 "descricao|descritivo|designacao|funcao|texto|"
		 "description|designation|fonction|libelle|"
		 "function|text"},
		{IoAddressField,
		 "endereco|endereco sugerido|enderecamento|address|adresse|"
		 "adresse suggeree|endereco clp|tag clp"},
		{IoCardField,
		 "cartao|cartao de destino|modulo|destino|placa|"
		 "carte|module|card|slot"},
		{IoConnectField,
		 "ligar a|ligar em|ligacao|conectar a|elemento|elemento ligado|"
		 "agrupamento|circuito|simbolo|"
		 "connecter a|element|schema|"
		 "connect to|connect|macro"},
		{IoTerminalField,
		 "exige borne|necessita borne|borne|bornes|borne de campo|"
		 "borne exigee|bornier|borne requise|"
		 "terminal|needs terminal"},
		{IoCommentField,
		 "comentario|comentarios|observacao|observacoes|obs|nota|notas|"
		 "commentaire|remarque|"
		 "comment|note|notes"}
	};

		/// @return the field @a folded names, NoIoField when it names none
	IoField fieldOfName(const QString &folded)
	{
		if (folded.isEmpty()) {
			return NoIoField;
		}
		for (const HeaderNames &candidate : header_names)
		{
			const QStringList names =
				QString::fromLatin1(candidate.names)
					.split(QLatin1Char('|'));
			if (names.contains(folded)) {
				return candidate.field;
			}
		}
		return NoIoField;
	}

		/// @return the cell of @a row at @a column, empty when there is none
	QString cellAt(const QStringList &row, int column)
	{
		if (column < 0 || column >= row.count()) {
			return QString();
		}
		return row.at(column).trimmed();
	}

		/// @return the row numbers as "25, 40 et 41", ready to be read
	QString rowNumbers(const QList<int> &rows)
	{
		QStringList numbers;
		for (const int row : rows) {
			numbers << QString::number(row);
		}
		return numbers.join(QStringLiteral(", "));
	}
}

/**
	@brief IoSheet::Mapping::columnOf
	@param field
	@return the column that feeds field, -1 when the sheet has none
*/
int IoSheet::Mapping::columnOf(IoField field) const
{
	return m_columns.value(int(field), -1);
}

/**
	@brief IoSheet::Mapping::setColumn
	@param field
	@param column

	A column feeds one field and a field is fed by one column: pointing a
	second field at a column takes it away from the first, which is what the
	combo boxes of the import dialogue expect to happen.
*/
void IoSheet::Mapping::setColumn(IoField field, int column)
{
	if (field == NoIoField) {
		return;
	}
	if (column < 0)
	{
		unsetField(field);
		return;
	}

	const QList<int> keys = m_columns.keys();
	for (const int key : keys)
	{
		if (m_columns.value(key) == column) {
			m_columns.remove(key);
		}
	}
	m_columns.insert(int(field), column);
}

/**
	@brief IoSheet::Mapping::unsetField
	@param field
*/
void IoSheet::Mapping::unsetField(IoField field)
{
	m_columns.remove(int(field));
}

bool IoSheet::Mapping::isEmpty() const
{
	return m_columns.isEmpty();
}

/**
	@brief IoSheet::Mapping::fields
	@return the fields this sheet carries
*/
IoFields IoSheet::Mapping::fields() const
{
	IoFields result = NoIoField;
	const QList<int> keys = m_columns.keys();
	for (const int key : keys) {
		result |= IoField(key);
	}
	return result;
}

/**
	@brief IoSheet::Mapping::mappedFields
	@return the mapped fields, leftmost column first
*/
QList<IoField> IoSheet::Mapping::mappedFields() const
{
	QMap<int, IoField> by_column;
	const QList<int> keys = m_columns.keys();
	for (const int key : keys) {
		by_column.insert(m_columns.value(key), IoField(key));
	}
	return by_column.values();
}

bool IoSheet::Report::isEmpty() const
{
	return points.isEmpty()
		&& blank_rows.isEmpty()
		&& unknown_type_rows.isEmpty()
		&& nameless_rows.isEmpty();
}

/**
	@brief IoSheet::Report::isClean
	@return true when nothing had to be skipped or guessed
*/
bool IoSheet::Report::isClean() const
{
	return blank_rows.isEmpty()
		&& unknown_type_rows.isEmpty()
		&& nameless_rows.isEmpty();
}

/**
	@brief IoSheet::Report::text
	@return the whole reading in one paragraph
*/
QString IoSheet::Report::text() const
{
	QStringList lines;

	lines << tr("%1 point(s) d'E/S lu(s).").arg(points.count());

	if (!blank_rows.isEmpty()) {
		lines << tr("%1 ligne(s) vide(s) ignorée(s) : %2.")
				 .arg(blank_rows.count())
				 .arg(rowNumbers(blank_rows));
	}
	if (!nameless_rows.isEmpty()) {
		lines << tr("%1 ligne(s) sans repère, sans adresse et sans "
			    "description ignorée(s) : %2.")
				 .arg(nameless_rows.count())
				 .arg(rowNumbers(nameless_rows));
	}
	if (!unknown_type_rows.isEmpty()) {
		lines << tr("%1 ligne(s) dont le type n'a pas été reconnu, lues "
			    "comme entrée numérique : %2.")
				 .arg(unknown_type_rows.count())
				 .arg(rowNumbers(unknown_type_rows));
	}

	return lines.join(QLatin1Char('\n'));
}

/**
	@brief IoSheet::basic
	@return type in the first column, description in the second
*/
IoSheet::Mapping IoSheet::basic()
{
	Mapping mapping;
	mapping.setColumn(IoTypeField, 0);
	mapping.setColumn(IoDescriptionField, 1);
	return mapping;
}

/**
	@brief IoSheet::guess
	@param header
	@return what each column name was understood to be
*/
IoSheet::Mapping IoSheet::guess(const QStringList &header)
{
	Mapping mapping;
	int recognised = 0;

	for (int column = 0 ; column < header.count() ; ++column)
	{
		const IoField field =
			fieldOfName(IoPoint::normalize(header.at(column)));
		if (field == NoIoField) {
			continue;
		}
			//The leftmost column wins: a sheet with a "description" and a
			//"description longue" gives the first one to the field, not
			//the last one read.
		if (mapping.columnOf(field) >= 0) {
			continue;
		}
		mapping.setColumn(field, column);
		++recognised;
	}

	mapping.has_header = recognised >= 2;
	return mapping;
}

/**
	@brief IoSheet::mappingFor
	@param grid
	@return the guessed mapping, or the basic one when the grid has no header
*/
IoSheet::Mapping IoSheet::mappingFor(const QList<QStringList> &grid)
{
	if (grid.isEmpty()) {
		return basic();
	}

	const Mapping guessed = guess(grid.first());
	if (guessed.has_header) {
		return guessed;
	}
	return basic();
}

/**
	@brief IoSheet::read
	@param grid
	@param mapping
	@return the points and everything that was not plain sailing
*/
IoSheet::Report IoSheet::read(const QList<QStringList> &grid,
			      const Mapping &mapping)
{
	Report report;

	const int type_column        = mapping.columnOf(IoTypeField);
	const int tag_column         = mapping.columnOf(IoTagField);
	const int description_column = mapping.columnOf(IoDescriptionField);
	const int address_column     = mapping.columnOf(IoAddressField);
	const int card_column        = mapping.columnOf(IoCardField);
	const int connect_column     = mapping.columnOf(IoConnectField);
	const int terminal_column    = mapping.columnOf(IoTerminalField);
	const int comment_column     = mapping.columnOf(IoCommentField);

	const int first = mapping.has_header ? 1 : 0;

	for (int index = first ; index < grid.count() ; ++index)
	{
			//The number a person reads in the leftmost margin of their
			//spreadsheet, header included. It is the only number that is
			//any use in a message telling them which row to go and look at.
		const int number = index + 1;
		const QStringList &row = grid.at(index);

		const QString type_cell        = cellAt(row, type_column);
		const QString tag_cell         = cellAt(row, tag_column);
		const QString description_cell = cellAt(row, description_column);
		const QString address_cell     = cellAt(row, address_column);
		const QString card_cell        = cellAt(row, card_column);
		const QString connect_cell     = cellAt(row, connect_column);
		const QString terminal_cell    = cellAt(row, terminal_column);
		const QString comment_cell     = cellAt(row, comment_column);

		if (type_cell.isEmpty()
		    && tag_cell.isEmpty()
		    && description_cell.isEmpty()
		    && address_cell.isEmpty()
		    && card_cell.isEmpty()
		    && connect_cell.isEmpty()
		    && terminal_cell.isEmpty()
		    && comment_cell.isEmpty())
		{
			report.blank_rows << number;
			continue;
		}

		IoPoint point;
		point.tag         = tag_cell;
		point.description = description_cell;
		point.address     = address_cell;
		point.card        = card_cell;
		point.connect_to  = connect_cell;
		point.comment     = comment_cell;

		if (!type_cell.isEmpty())
		{
			bool understood = false;
			point.type = IoPoint::typeFromString(type_cell, &understood);
			if (!understood) {
				report.unknown_type_rows << number;
			}
		}

		if (terminal_column >= 0) {
			point.needs_terminal = isYes(terminal_cell);
		}

			//A row that says a card, a comment or a terminal but never
			//says which point it is talking about. It is not blank, so
			//saying nothing about it would be the silent half import the
			//task forbids.
		if (point.isNull())
		{
			report.nameless_rows << number;
			continue;
		}

		report.points << point;
	}

	return report;
}

/**
	@brief IoSheet::fieldName
	@param field
	@return the name of that field, as a dialogue shows it
*/
QString IoSheet::fieldName(IoField field)
{
	switch (field)
	{
		case IoTypeField:        return tr("Type");
		case IoTagField:         return tr("Repère");
		case IoDescriptionField: return tr("Description");
		case IoAddressField:     return tr("Adresse");
		case IoCardField:        return tr("Carte");
		case IoConnectField:     return tr("Connecter à");
		case IoTerminalField:    return tr("Borne exigée");
		case IoCommentField:     return tr("Commentaire");
		case NoIoField:
		case AllIoFields:
			break;
	}
	return tr("(ignorée)");
}

/**
	@brief IoSheet::mappableFields
	@return every field a column can feed, leftmost first in a full sheet
*/
QList<IoField> IoSheet::mappableFields()
{
	QList<IoField> fields;
	fields << IoTypeField
	       << IoTagField
	       << IoDescriptionField
	       << IoAddressField
	       << IoCardField
	       << IoConnectField
	       << IoTerminalField
	       << IoCommentField;
	return fields;
}

/**
	@brief IoSheet::isYes
	@param text
	@return true when the cell says yes

	Everything a person actually types in a yes-or-no column of a spreadsheet,
	in the three languages, plus the cross and the digit. Anything else is no,
	because a column meaning "create a field terminal" must never say yes by
	accident.
*/
bool IoSheet::isYes(const QString &text)
{
	const QString folded = IoPoint::normalize(text);
	if (folded.isEmpty()) {
		return false;
	}

	return folded == QLatin1String("sim")
		|| folded == QLatin1String("s")
		|| folded == QLatin1String("x")
		|| folded == QLatin1String("yes")
		|| folded == QLatin1String("y")
		|| folded == QLatin1String("oui")
		|| folded == QLatin1String("o")
		|| folded == QLatin1String("true")
		|| folded == QLatin1String("vrai")
		|| folded == QLatin1String("verdadeiro")
		|| folded == QLatin1String("1");
}
