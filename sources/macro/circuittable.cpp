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
#include "circuittable.h"

#include "circuitclipboard.h"
#include "macroparameter.h"
#include "macrosequence.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QLatin1Char>
#include <QSet>
#include <QStringList>
#include <QUuid>

/**
	@brief CircuitRow::CircuitRow
*/
CircuitRow::CircuitRow()
{}

/**
	@brief CircuitRow::CircuitRow
	@param macro_path
*/
CircuitRow::CircuitRow(const QString &macro_path) :
	macro_path(macro_path)
{}

/**
	@brief CircuitRow::isNull
	@return true when the row names no macro and holds no value
*/
bool CircuitRow::isNull() const
{
	return macro_path.isEmpty() && values.isEmpty();
}

/**
	@brief CircuitRow::toXml
	@param document
	@return the row as a <circuit> element

	The values are written in the order of their names, and not in the order
	a QHash happens to walk them: a project saved twice without being edited
	has to give the same file, or every save shows up as a change.
*/
QDomElement CircuitRow::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	if (!id.isEmpty()) {
		element.setAttribute(QStringLiteral("id"), id);
	}
	element.setAttribute(QStringLiteral("macro"), macro_path);

	QStringList names = values.keys();
	names.sort();
	for (const QString &name : names)
	{
		QDomElement value_element = document.createElement(valueTagName());
		value_element.setAttribute(QStringLiteral("name"), name);
		value_element.appendChild(document.createTextNode(values.value(name)));
		element.appendChild(value_element);
	}
	return element;
}

/**
	@brief CircuitRow::fromXml
	@param element
	@return true when the row was read
*/
bool CircuitRow::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	id = element.attribute(QStringLiteral("id")).trimmed();
	macro_path = element.attribute(QStringLiteral("macro")).trimmed();
	values.clear();
	for (QDomElement child = element.firstChildElement(valueTagName()) ;
	     !child.isNull() ;
	     child = child.nextSiblingElement(valueTagName()))
	{
		const QString name = child.attribute(QStringLiteral("name")).trimmed();
		if (name.isEmpty()) {
			continue;
		}
		values.insert(name, child.text());
	}
	return true;
}

/**
	@brief CircuitRow::tagName
	@return "circuit"
*/
QString CircuitRow::tagName()
{
	return QStringLiteral("circuit");
}

/**
	@brief CircuitRow::valueTagName
	@return "value"
*/
QString CircuitRow::valueTagName()
{
	return QStringLiteral("value");
}

/**
	@brief CircuitTable::Problem::text
	@return the refusal, naming the row by its number on screen

	One sentence per row and not one per field: being told one missing value
	at a time is the slowest way to fill a table, and a generator is meant to
	save time. The rule is MacroParameterSet::missingRequired()'s, said again
	here because the row has to name itself.
*/
QString CircuitTable::Problem::text() const
{
	const int shown = row + 1;

	if (no_macro) {
		return QCoreApplication::translate("CircuitTable",
						   "Ligne %1 : aucun macro choisi.")
				.arg(shown);
	}

	QStringList sentences;
	if (missing.count() == 1)
	{
		sentences << QCoreApplication::translate("CircuitTable",
							 "Ligne %1 : la variable %2 est "
							 "obligatoire et n'a pas de valeur.")
				     .arg(shown)
				     .arg(missing.first());
	}
	else if (missing.count() > 1)
	{
		sentences << QCoreApplication::translate("CircuitTable",
							 "Ligne %1 : ces variables sont "
							 "obligatoires et n'ont pas de "
							 "valeur : %2.")
				     .arg(shown)
				     .arg(missing.join(QStringLiteral(", ")));
	}
	if (!refused.isEmpty())
	{
		sentences << QCoreApplication::translate("CircuitTable",
							 "Ligne %1 : la valeur de %2 n'est "
							 "pas acceptée par le type du "
							 "paramètre.")
				     .arg(shown)
				     .arg(refused.join(QStringLiteral(", ")));
	}
	return sentences.join(QLatin1Char(' '));
}

/**
	@brief CircuitTable::CircuitTable
*/
CircuitTable::CircuitTable()
{}

/**
	@brief CircuitTable::setParameters
	@param macro_path
	@param parameters : what the .qetmak at @a macro_path declares

	The table does not read the file: whoever opened it says what it holds.
*/
void CircuitTable::setParameters(const QString &macro_path,
				 const MacroParameterSet &parameters)
{
	if (macro_path.isEmpty()) {
		return;
	}
	m_parameters.insert(macro_path, parameters);
}

/**
	@brief CircuitTable::hasParameters
	@param macro_path
	@return whether the parameters of @a macro_path are known
*/
bool CircuitTable::hasParameters(const QString &macro_path) const
{
	return m_parameters.contains(macro_path);
}

/**
	@brief CircuitTable::parameters
	@param macro_path
	@return what @a macro_path declares, empty when nobody said
*/
MacroParameterSet CircuitTable::parameters(const QString &macro_path) const
{
	return m_parameters.value(macro_path);
}

/**
	@brief CircuitTable::macroPaths
	@return the macros the rows use, each once, in order of first appearance

	Order of first appearance and not alphabetical order: it is what decides
	the order of the columns, and the person who filled the first row expects
	to see its variables first.
*/
QStringList CircuitTable::macroPaths() const
{
	QStringList paths;
	for (const CircuitRow &row : m_rows)
	{
		if (row.macro_path.isEmpty() || paths.contains(row.macro_path)) {
			continue;
		}
		paths << row.macro_path;
	}
	return paths;
}

/**
	@brief CircuitTable::isEmpty
	@return whether there is no row at all
*/
bool CircuitTable::isEmpty() const
{
	return m_rows.isEmpty();
}

/**
	@brief CircuitTable::rowCount
	@return how many rows
*/
int CircuitTable::rowCount() const
{
	return m_rows.count();
}

/**
	@brief CircuitTable::rows
	@return every row, in table order
*/
QList<CircuitRow> CircuitTable::rows() const
{
	return m_rows;
}

/**
	@brief CircuitTable::row
	@param index
	@return the row, null when @a index is out of the table
*/
CircuitRow CircuitTable::row(int index) const
{
	if (index < 0 || index >= m_rows.count()) {
		return CircuitRow();
	}
	return m_rows.at(index);
}

/**
	@brief CircuitTable::indexOfId
	@param id
	@return where the row is now, -1 when it is not here any more
*/
int CircuitTable::indexOfId(const QString &id) const
{
	if (id.isEmpty()) {
		return -1;
	}
	for (int i = 0 ; i < m_rows.count() ; ++ i)
	{
		if (m_rows.at(i).id == id) {
			return i;
		}
	}
	return -1;
}

/**
	@brief CircuitTable::appendRow
	@param row
	@return the index of the new row

	A row that comes in without an id is given one, because the id is what
	regenerating a single row later finds it by, and a row that lost its id
	is a row that can only be regenerated with all the others.
*/
int CircuitTable::appendRow(const CircuitRow &row)
{
	CircuitRow appended = row;
	if (appended.id.isEmpty()) {
		appended.id = newId();
	}
	m_rows << appended;
	return m_rows.count() - 1;
}

/**
	@brief CircuitTable::appendRow
	@param macro_path
	@return the index of the new row, filled with the defaults of the macro
*/
int CircuitTable::appendRow(const QString &macro_path)
{
	CircuitRow row(macro_path);
	if (m_parameters.contains(macro_path)) {
		row.values = m_parameters.value(macro_path).defaults();
	}
	return appendRow(row);
}

/**
	@brief CircuitTable::insertRow
	@param index
	@param row
	@return true when it was inserted
*/
bool CircuitTable::insertRow(int index, const CircuitRow &row)
{
	if (index < 0 || index > m_rows.count()) {
		return false;
	}
	CircuitRow inserted = row;
	if (inserted.id.isEmpty()) {
		inserted.id = newId();
	}
	m_rows.insert(index, inserted);
	return true;
}

/**
	@brief CircuitTable::removeRow
	@param index
	@return true when it was removed
*/
bool CircuitTable::removeRow(int index)
{
	if (index < 0 || index >= m_rows.count()) {
		return false;
	}
	m_rows.removeAt(index);
	return true;
}

/**
	@brief CircuitTable::clear
	Empties the rows. The parameter sets stay: they describe the macros, not
	the table.
*/
void CircuitTable::clear()
{
	m_rows.clear();
}

/**
	@brief CircuitTable::macroPath
	@param index
	@return the macro of that row
*/
QString CircuitTable::macroPath(int index) const
{
	return row(index).macro_path;
}

/**
	@brief CircuitTable::setMacroPath
	@param index
	@param macro_path
	@return true when the row changed macro

	The values already typed are kept, all of them. The ones the new macro
	declares go on being asked for; the ones it does not become inert and
	wait, which is what makes changing one's mind twice cost nothing. Only
	the variables the new macro declares and the row has never heard of are
	added, at their default.
*/
bool CircuitTable::setMacroPath(int index, const QString &macro_path)
{
	if (index < 0 || index >= m_rows.count()) {
		return false;
	}

	m_rows[index].macro_path = macro_path;
	if (!m_parameters.contains(macro_path)) {
		return true;
	}

	const QHash<QString, QString> defaults =
			m_parameters.value(macro_path).defaults();
	for (auto it = defaults.constBegin() ; it != defaults.constEnd() ; ++ it)
	{
		if (!m_rows.at(index).values.contains(it.key())) {
			m_rows[index].values.insert(it.key(), it.value());
		}
	}
	return true;
}

/**
	@brief CircuitTable::columns
	@return the union of the parameters of the macros in use, in declaration
	order, first macro first

	Union and not intersection: a table that mixes a direct starter and a
	reversing one shows the columns of both, and the direct rows leave the
	reversing columns inert. That is CU-08.4, and it is what lets one table
	hold a whole switchboard instead of one table per kind of feeder.
*/
QStringList CircuitTable::columns() const
{
	QStringList names;
	const QStringList paths = macroPaths();
	for (const QString &path : paths)
	{
		if (!m_parameters.contains(path)) {
			continue;
		}
		const QStringList declared = m_parameters.value(path).names();
		for (const QString &name : declared)
		{
			if (!names.contains(name)) {
				names << name;
			}
		}
	}
	return names;
}

/**
	@brief CircuitTable::columnCount
	@return how many columns
*/
int CircuitTable::columnCount() const
{
	return columns().count();
}

/**
	@brief CircuitTable::columnLabel
	@param column
	@return the label of the first macro that declares it, the name itself
	when no macro does or when the declaration has no label
*/
QString CircuitTable::columnLabel(const QString &column) const
{
	const QStringList paths = macroPaths();
	for (const QString &path : paths)
	{
		const MacroParameterSet set = m_parameters.value(path);
		if (!set.contains(column)) {
			continue;
		}
		const QString label = set.parameter(column).label.trimmed();
		return label.isEmpty() ? column : label;
	}
	return column;
}

/**
	@brief CircuitTable::parameterFor
	@param index
	@param column
	@return what the macro of that row declares under that name, a null
	parameter when it declares nothing
*/
MacroParameter CircuitTable::parameterFor(int index, const QString &column) const
{
	const QString path = macroPath(index);
	if (path.isEmpty() || !m_parameters.contains(path)) {
		return MacroParameter();
	}
	const MacroParameterSet set = m_parameters.value(path);
	if (!set.contains(column)) {
		return MacroParameter();
	}
	return set.parameter(column);
}

/**
	@brief CircuitTable::isInert
	@param index
	@param column
	@return whether that cell asks for nothing

	Inert is not empty. The value stays where it is, and comes back the
	moment the row goes back to a macro that declares the variable.
*/
bool CircuitTable::isInert(int index, const QString &column) const
{
	return parameterFor(index, column).isNull();
}

/**
	@brief CircuitTable::value
	@param index
	@param column
	@return what the cell holds, inert or not
*/
QString CircuitTable::value(int index, const QString &column) const
{
	return row(index).values.value(column);
}

/**
	@brief CircuitTable::values
	@param index
	@return every value of the row, inert ones included

	The generator does not want this one: MacroSubstitution would report the
	inert names as undeclared. It wants the values of the declared columns,
	which is what problems() checks and what the generator will ask for.
*/
QHash<QString, QString> CircuitTable::values(int index) const
{
	return row(index).values;
}

/**
	@brief CircuitTable::setValue
	@param index
	@param column
	@param value
	@param error : when not nullptr, receives the refusal
	@return true when the cell took the value
*/
bool CircuitTable::setValue(int index,
			    const QString &column,
			    const QString &value,
			    QString *error)
{
	if (index < 0 || index >= m_rows.count())
	{
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "La ligne %1 n'existe pas.")
					 .arg(index + 1);
		}
		return false;
	}

	const MacroParameter parameter = parameterFor(index, column);
	if (parameter.isNull())
	{
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "Le macro de la ligne %1 ne "
							     "déclare pas la variable %2.")
					 .arg(index + 1)
					 .arg(column);
		}
		return false;
	}

	if (!value.isEmpty() && !parameter.acceptsValue(value))
	{
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "%1 n'est pas une valeur de la "
							     "liste du paramètre %2.")
					 .arg(value)
					 .arg(column);
		}
		return false;
	}

	m_rows[index].values.insert(column, value);
	return true;
}

/**
	@brief CircuitTable::problems
	@return one Problem per row the generator cannot draw, in table order

	The nineteen good rows are not this function's business: it names what is
	wrong, and the generator draws everything it does not name. Aborting the
	twenty because of one is what CU-08.6 exists to forbid.
*/
QList<CircuitTable::Problem> CircuitTable::problems() const
{
	QList<Problem> found;
	for (int i = 0 ; i < m_rows.count() ; ++ i)
	{
		const CircuitRow &row = m_rows.at(i);
		Problem problem;
		problem.row = i;
		problem.id = row.id;

		if (row.macro_path.isEmpty())
		{
			problem.no_macro = true;
			found << problem;
			continue;
		}
		if (!m_parameters.contains(row.macro_path)) {
			continue;
		}

		const MacroParameterSet set = m_parameters.value(row.macro_path);
		const QStringList declared = set.names();

			//The inert values are left out on purpose: a value kept for a
			//macro this row no longer uses is not a value this row passes.
		QHash<QString, QString> passed;
		for (const QString &name : declared)
		{
			if (row.values.contains(name)) {
				passed.insert(name, row.values.value(name));
			}
		}

		problem.missing = set.missingRequired(passed);
		for (const QString &name : declared)
		{
			const QString value = passed.value(name);
			if (value.isEmpty()) {
				continue;
			}
			if (!set.parameter(name).acceptsValue(value)) {
				problem.refused << name;
			}
		}

		if (!problem.missing.isEmpty() || !problem.refused.isEmpty()) {
			found << problem;
		}
	}
	return found;
}

/**
	@brief CircuitTable::Refusal::text
	@return the refusal, ready to be shown next to the table
*/
QString CircuitTable::Refusal::text() const
{
	return QCoreApplication::translate("CircuitTable",
					   "Ligne %1, %2 : %3")
			.arg(QString::number(row + 1), column, reason);
}

/**
	@brief CircuitTable::PasteReport::text
	@return what the paste did, in one message

	One line for the count, one for the columns that landed nowhere, then one
	line per refused cell. A paste that went through without a word gives a
	single line, which is what twenty good rows should cost the person to
	read.
*/
QString CircuitTable::PasteReport::text() const
{
	if (!ok) {
		return error;
	}

	QStringList lines;
	lines << QCoreApplication::translate("CircuitTable",
					     "%1 ligne(s) lue(s), %2 ajoutée(s), %3 cellule(s) écrite(s).")
			.arg(QString::number(rows_read),
			     QString::number(rows_added),
			     QString::number(cells_written));

	if (!unmatched.isEmpty()) {
		lines << QCoreApplication::translate("CircuitTable",
						     "Colonne(s) laissée(s) de côté, aucun macro ne les déclare : %1.")
				.arg(unmatched.join(QStringLiteral(", ")));
	}

	for (const Refusal &refusal : refused) {
		lines << refusal.text();
	}

	return lines.join(QStringLiteral("\n"));
}

/**
	@brief CircuitTable::landingColumns
	@param macro_for_new_rows : the macro the paste would give to the rows it
	adds, empty when the rows it lands on already have one
	@return the columns a paste can land in, in order

	The columns in use, plus the ones the macro of the rows to come declares
	and nobody has yet. Without the second half, choosing a macro and pasting
	twenty lines into an empty table would land nowhere: the table has no
	rows, so it has no columns, so the paste has nothing to fill.
*/
QStringList CircuitTable::landingColumns(const QString &macro_for_new_rows) const
{
	QStringList names = columns();
	if (macro_for_new_rows.isEmpty() || !m_parameters.contains(macro_for_new_rows)) {
		return names;
	}

	const QStringList declared = m_parameters.value(macro_for_new_rows).names();
	for (const QString &name : declared)
	{
		if (!names.contains(name)) {
			names << name;
		}
	}
	return names;
}

/**
	@brief CircuitTable::columnNamed
	@param text : one cell of the first line of a paste
	@param among : the columns it may name
	@return the column it names, empty when it names none

	A person copying a range out of their spreadsheet copies the heading with
	it, and the heading is written the way the heading of the table is
	written - the label, translated - and not the way the parameter is named
	in the macro. Both are accepted, and so is a difference of case or a
	trailing space, because a heading typed by hand is never exactly the same
	twice.

	The label is looked for in every parameter set the table knows, not only
	in the ones its rows use, so that the first paste into an empty table
	finds the labels of the macro that is about to be inserted.
*/
QString CircuitTable::columnNamed(const QString &text, const QStringList &among) const
{
	const QString wanted = text.trimmed();
	if (wanted.isEmpty()) {
		return QString();
	}

	for (const QString &name : among)
	{
		if (name.compare(wanted, Qt::CaseInsensitive) == 0) {
			return name;
		}
	}

	for (const QString &name : among)
	{
		for (auto it = m_parameters.constBegin() ; it != m_parameters.constEnd() ; ++ it)
		{
			if (!it.value().contains(name)) {
				continue;
			}
			const QString label = it.value().parameter(name).label.trimmed();
			if (!label.isEmpty() && label.compare(wanted, Qt::CaseInsensitive) == 0) {
				return name;
			}
		}
	}

	return QString();
}

/**
	@brief CircuitTable::pasteTsv
	@param text : what the spreadsheet put on the clipboard
	@param start_row : the row the first line lands in, -1 for after the last
	@param start_column : the column the first cell lands in, empty for the
	first one; ignored when the paste carries a header
	@param macro_for_new_rows : the macro to give the rows the paste adds,
	empty to give them the macro of the row above
	@return what was done, and what was not

	The paste *grows* the table. A range of twenty lines dropped into an
	empty table gives twenty circuits; refusing it because the table has no
	room is refusing the one thing CU-08.2 is for. The rows it adds inherit
	the macro of the row above them, so the ordinary case - twenty feeders of
	the same kind - needs the macro chosen once.

	Two kinds of refusal, and the difference matters. A paste that cannot be
	placed at all - no columns, a starting row that does not exist, more
	columns than the table has left - is refused whole: ok is false, error
	says why, and *nothing* is written. A cell the parameter refuses is
	reported and the paste goes on: the value stays as it was and the other
	nineteen rows are pasted. The first would cost the person their work; the
	second costs them one cell.
*/
CircuitTable::PasteReport CircuitTable::pasteTsv(const QString &text,
						 int start_row,
						 const QString &start_column,
						 const QString &macro_for_new_rows)
{
	PasteReport report;

	QList<QStringList> grid = CircuitClipboard::parse(text);
	bool anything = false;
	for (const QStringList &line : grid)
	{
		for (const QString &cell : line)
		{
			if (!cell.trimmed().isEmpty())
			{
				anything = true;
				break;
			}
		}
		if (anything) {
			break;
		}
	}
	if (!anything)
	{
		report.error = QCoreApplication::translate("CircuitTable",
							  "Le presse-papiers ne contient aucune cellule.");
		return report;
	}

	if (!macro_for_new_rows.isEmpty() && !hasParameters(macro_for_new_rows))
	{
		report.error = QCoreApplication::translate("CircuitTable",
							  "Les paramètres du macro %1 ne sont pas connus de la table.")
				.arg(macro_for_new_rows);
		return report;
	}

	const QStringList landing = landingColumns(macro_for_new_rows);
	if (landing.isEmpty())
	{
		report.error = QCoreApplication::translate("CircuitTable",
							  "La table n'a aucune colonne : choisissez d'abord le macro des lignes.");
		return report;
	}

	if (start_row < 0) {
		start_row = m_rows.count();
	}
	if (start_row > m_rows.count())
	{
		report.error = QCoreApplication::translate("CircuitTable",
							  "La ligne %1 n'existe pas.")
				.arg(start_row + 1);
		return report;
	}

	int width = 0;
	for (const QStringList &line : grid)
	{
		if (line.count() > width) {
			width = line.count();
		}
	}

		//Is the first line a header? It is when more of its cells name a
		//column than do not. Asking for every cell to match would lose the
		//heading the moment the person copies an "Obs" column with it;
		//asking for one would read a data line whose first value happens to
		//read like a name as a heading, and silently drop that circuit.
		//Counting the two sides is the clause that survives both.
	const QStringList first = grid.first();
	QStringList by_header;
	QStringList strays;
	int matched = 0;
	for (const QString &cell : first)
	{
		const QString name = columnNamed(cell, landing);
		if (name.isEmpty())
		{
			by_header << QString();
			if (!cell.trimmed().isEmpty()) {
				strays << cell.trimmed();
			}
		}
		else
		{
			by_header << name;
			++ matched;
		}
	}

	QStringList target;
	if (grid.count() > 1 && matched > 0 && matched > strays.count())
	{
		target = by_header;
		report.header_used = true;
		report.unmatched = strays;
		grid.removeFirst();
	}
	else
	{
			//No header: the cells land side by side from where the person
			//put the cursor, the way a spreadsheet pastes.
		int start = 0;
		if (!start_column.isEmpty())
		{
			start = landing.indexOf(start_column);
			if (start < 0)
			{
				report.error = QCoreApplication::translate("CircuitTable",
									  "La table n'a pas de colonne %1.")
						.arg(start_column);
				return report;
			}
		}
		if (start + width > landing.count())
		{
			report.error = QCoreApplication::translate("CircuitTable",
								  "Le collage a %1 colonne(s), mais il n'en reste que %2 à partir de %3.")
					.arg(QString::number(width),
					     QString::number(landing.count() - start),
					     landing.at(start));
			return report;
		}
		for (int c = 0 ; c < width ; ++ c) {
			target << landing.at(start + c);
		}
	}

	report.rows_read = grid.count();
	report.landed = target;

		//Nothing has been written up to here, and every whole-paste refusal
		//is behind us. From here the paste applies.
	for (int line = 0 ; line < grid.count() ; ++ line)
	{
		const int index = start_row + line;
		if (index >= m_rows.count())
		{
			QString macro = macro_for_new_rows;
			if (macro.isEmpty() && !m_rows.isEmpty()) {
				macro = m_rows.last().macro_path;
			}
			appendRow(macro);
			++ report.rows_added;
		}

		const QStringList cells = grid.at(line);
		bool took = false;
		for (int c = 0 ; c < cells.count() && c < target.count() ; ++ c)
		{
			const QString column = target.at(c);
			if (column.isEmpty()) {
				continue;
			}

			const QString value = cells.at(c);
				//An empty cell landing on a column this row's macro does
				//not have is not a refusal, it is a table where the rows
				//are not all the same kind. Saying so twenty times would
				//bury the one cell that really was refused.
			if (value.isEmpty() && isInert(index, column)) {
				continue;
			}

			QString why;
			if (setValue(index, column, value, &why))
			{
				++ report.cells_written;
				took = true;
			}
			else
			{
				Refusal refusal;
				refusal.row = index;
				refusal.column = column;
				refusal.value = value;
				refusal.reason = why;
				report.refused << refusal;
			}
		}
		if (took) {
			++ report.rows_changed;
		}
	}

	report.ok = true;
	return report;
}

/**
	@brief CircuitTable::copyTsv
	@param top_row : first row, -1 for the first of the table
	@param bottom_row : last row, -1 for the last of the table
	@param column_names : the columns to write, empty for all of them
	@param with_header : whether to write the names on a first line
	@return the text to put on the clipboard

	The header is written with the parameter *names* and not with their
	labels, so that copying a range out of the table and pasting it back
	lands in the same columns whatever language the program is showing.
	columnNamed() accepts both, so a person is free to edit the heading in
	their spreadsheet.

	An inert cell is written empty: the value it is holding belongs to
	another macro, and putting it in a column of this one would be telling
	the spreadsheet something that is not true.
*/
QString CircuitTable::copyTsv(int top_row,
			      int bottom_row,
			      const QStringList &column_names,
			      bool with_header) const
{
	QStringList wanted = column_names;
	if (wanted.isEmpty()) {
		wanted = columns();
	}
	if (wanted.isEmpty() || m_rows.isEmpty()) {
		return QString();
	}

	if (top_row < 0) {
		top_row = 0;
	}
	if (bottom_row < 0 || bottom_row >= m_rows.count()) {
		bottom_row = m_rows.count() - 1;
	}
	if (top_row > bottom_row) {
		return QString();
	}

	QList<QStringList> grid;
	if (with_header) {
		grid << wanted;
	}
	for (int index = top_row ; index <= bottom_row ; ++ index)
	{
		QStringList line;
		for (const QString &column : wanted) {
			line << (isInert(index, column) ? QString() : value(index, column));
		}
		grid << line;
	}

	return CircuitClipboard::compose(grid);
}

/**
	@brief CircuitTable::canFillDown
	@param top_row : the row the value is taken from
	@param bottom_row : the last row it is written to
	@param column
	@param error : filled with the reason when the answer is false
	@return whether the value of @a top_row can be written down the range
*/
bool CircuitTable::canFillDown(int top_row,
			       int bottom_row,
			       const QString &column,
			       QString *error) const
{
	if (top_row < 0 || bottom_row >= m_rows.count() || top_row >= bottom_row)
	{
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "Choisissez une plage d'au moins deux lignes.");
		}
		return false;
	}

	if (column.isEmpty() || !columns().contains(column))
	{
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "La table n'a pas de colonne %1.")
					.arg(column);
		}
		return false;
	}

	if (isInert(top_row, column))
	{
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "Le macro de la ligne %1 ne déclare pas la variable %2.")
					.arg(QString::number(top_row + 1), column);
		}
		return false;
	}

	if (error) {
		error->clear();
	}
	return true;
}

/**
	@brief CircuitTable::fillDown
	@param top_row : the row the value is taken from
	@param bottom_row : the last row it is written to
	@param column
	@param error : filled with the reason when nothing was written
	@return how many cells took the value

	The plain copy down: twenty motors of the same power are typed once. A
	row of the range whose macro does not declare the variable is stepped
	over without a word - a table mixing a direct starter and a reversing one
	is the ordinary case, and filling a column of the reversing one has
	nothing to say to the others.
*/
int CircuitTable::fillDown(int top_row,
			   int bottom_row,
			   const QString &column,
			   QString *error)
{
	if (!canFillDown(top_row, bottom_row, column, error)) {
		return 0;
	}

	const QString source = value(top_row, column);
	int written = 0;
	for (int index = top_row + 1 ; index <= bottom_row ; ++ index)
	{
		if (isInert(index, column)) {
			continue;
		}
		if (setValue(index, column, source)) {
			++ written;
		}
	}
	return written;
}

/**
	@brief CircuitTable::canFillSeries
	@param top_row : the row the series starts from
	@param bottom_row : the last row it is written to
	@param column
	@param error : filled with the reason when the answer is false
	@return whether the value of @a top_row has a next one
*/
bool CircuitTable::canFillSeries(int top_row,
				 int bottom_row,
				 const QString &column,
				 QString *error) const
{
	if (!canFillDown(top_row, bottom_row, column, error)) {
		return false;
	}

	const MacroParameter parameter = parameterFor(top_row, column);
	if (parameter.type != MacroParameterType::Text)
	{
			//A number is a quantity and not a counter: 7,5 kW followed by
			//7,6 kW would be an engineering decision taken by a drag of the
			//mouse. A list may only hold what it declares, and a part code
			//names a part in the catalogue. For those three, copying the
			//first value down is the operation that means something, and
			//that is what the message says to do.
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "%1 est du type %2 : une valeur n'y a pas de suivante. Recopiez la première vers le bas.")
					.arg(column,
					     MacroParameter::translatedTypeName(parameter.type));
		}
		return false;
	}

	if (value(top_row, column).isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CircuitTable",
							     "La ligne %1 n'a pas de valeur dans %2 : la série n'a pas de départ.")
					.arg(QString::number(top_row + 1), column);
		}
		return false;
	}

	if (error) {
		error->clear();
	}
	return true;
}

/**
	@brief CircuitTable::fillSeries
	@param top_row : the row the series starts from
	@param bottom_row : the last row it is written to
	@param column
	@param error : filled with the reason when nothing was written
	@return how many cells took a value

	CU-08.3: -M1 in the first row and the drag gives -M2, -M3, -M4. The
	digits that count are the ones MacroSequence::stemOf() finds, so
	"PS1 - NO BREAK" counts on the 1 and keeps the rest, and a value with no
	number at all starts at two.

	A value the column already spends is stepped over rather than issued
	twice: the same tag on two circuits of one switchboard is a defect, a
	hole in the numbering is not. Only the rows outside the range are counted
	as spending, so a series written over itself does not run away from its
	own first value.
*/
int CircuitTable::fillSeries(int top_row,
			     int bottom_row,
			     const QString &column,
			     QString *error)
{
	if (!canFillSeries(top_row, bottom_row, column, error)) {
		return 0;
	}

	int number = 1;
	int width = 0;
	QString tail;
	const QString stem = MacroSequence::stemOf(value(top_row, column),
						   &number, &width, &tail);

	QSet<QString> taken;
	for (int index = 0 ; index < m_rows.count() ; ++ index)
	{
		if (index > top_row && index <= bottom_row) {
			continue;
		}
		if (isInert(index, column)) {
			continue;
		}
		const QString held = value(index, column);
		if (!held.isEmpty()) {
			taken << held;
		}
	}

	int written = 0;
	int step = 0;
	for (int index = top_row + 1 ; index <= bottom_row ; ++ index)
	{
		if (isInert(index, column)) {
			continue;
		}
		++ step;
		const QString proposal = stem
				+ QString::number(number + step).rightJustified(width, QLatin1Char('0'))
				+ tail;
		const QString free = MacroSequence::nextFree(proposal, taken);
		if (!setValue(index, column, free)) {
			continue;
		}
		taken << free;
		++ written;
	}
	return written;
}

/**
	@brief CircuitTable::toXml
	@param document
	@return the table as a <circuit_table> element
*/
QDomElement CircuitTable::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	for (const CircuitRow &row : m_rows) {
		element.appendChild(row.toXml(document));
	}
	return element;
}

/**
	@brief CircuitTable::fromXml
	@param element
	@return true when the table was read

	Tolerant, as reading always is here: a row without a macro is kept, so
	that a project saved half filled comes back half filled instead of coming
	back short of a row nobody can name.
*/
bool CircuitTable::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	m_rows.clear();
	for (QDomElement child = element.firstChildElement(CircuitRow::tagName()) ;
	     !child.isNull() ;
	     child = child.nextSiblingElement(CircuitRow::tagName()))
	{
		CircuitRow row;
		if (!row.fromXml(child)) {
			continue;
		}
		if (row.id.isEmpty()) {
			row.id = newId();
		}
		m_rows << row;
	}
	return true;
}

/**
	@brief CircuitTable::tagName
	@return "circuit_table"
*/
QString CircuitTable::tagName()
{
	return QStringLiteral("circuit_table");
}

/**
	@brief CircuitTable::newId
	@return an id no other row has
*/
QString CircuitTable::newId()
{
	return QUuid::createUuid().toString();
}
