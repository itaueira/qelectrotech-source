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

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
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
