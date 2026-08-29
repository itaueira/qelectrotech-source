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
#include "macroparameterset.h"

#include <QDebug>
#include <QDomDocument>
#include <QDomElement>

/**
	@brief MacroValueSet::MacroValueSet
*/
MacroValueSet::MacroValueSet()
{}

/**
	@brief MacroValueSet::MacroValueSet
	@param name : user visible name of the set
*/
MacroValueSet::MacroValueSet(const QString &name) :
	name(name)
{}

/**
	@brief MacroValueSet::isNull
	@return true when the set has no name to be picked by
*/
bool MacroValueSet::isNull() const
{
	return name.isEmpty();
}

/**
	@brief MacroValueSet::fromXml
	@param element : a <valueset> block
	@return false when @a element does not describe a value set

	A <value> with no name is skipped rather than stored under an empty key,
	because an empty key answers to no marker and would only be dead weight
	carried into the next save.
*/
bool MacroValueSet::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}
	const QString read_name = element.attribute(QStringLiteral("name")).trimmed();
	if (read_name.isEmpty()) {
		return false;
	}

	name = read_name;
	values.clear();
	for (QDomElement child = element.firstChildElement(valueTagName()) ;
	     !child.isNull() ;
	     child = child.nextSiblingElement(valueTagName()))
	{
		const QString value_name = child.attribute(QStringLiteral("name")).trimmed();
		if (value_name.isEmpty()) {
			continue;
		}
		values.insert(value_name, child.text());
	}
	return true;
}

/**
	@return the tag name of a value set block
*/
QString MacroValueSet::tagName()
{
	return QStringLiteral("valueset");
}

/**
	@return the tag name of one value inside a set
*/
QString MacroValueSet::valueTagName()
{
	return QStringLiteral("value");
}

/**
	@brief MacroParameterSet::MacroParameterSet
	Builds an empty set, which is what every macro written before this task
	amounts to: no parameter, nothing to substitute, insertion unchanged.
*/
MacroParameterSet::MacroParameterSet()
{}

/**
	@brief MacroParameterSet::isEmpty
	@return true when no parameter is declared
*/
bool MacroParameterSet::isEmpty() const
{
	return m_parameters.isEmpty();
}

/**
	@brief MacroParameterSet::count
	@return how many parameters are declared
*/
int MacroParameterSet::count() const
{
	return m_parameters.count();
}

/**
	@brief MacroParameterSet::parameters
	@return the parameters, in the order the macro declares them
*/
QList<MacroParameter> MacroParameterSet::parameters() const
{
	return m_parameters;
}

/**
	@brief MacroParameterSet::names
	@return the parameter names, in declaration order
*/
QStringList MacroParameterSet::names() const
{
	QStringList list;
	list.reserve(m_parameters.count());
	for (const MacroParameter &parameter : m_parameters) {
		list << parameter.name;
	}
	return list;
}

/**
	@brief MacroParameterSet::contains
	@param name
	@return whether a parameter of that name is declared
*/
bool MacroParameterSet::contains(const QString &name) const
{
	for (const MacroParameter &parameter : m_parameters)
	{
		if (parameter.name == name) {
			return true;
		}
	}
	return false;
}

/**
	@brief MacroParameterSet::parameter
	@param name
	@return the parameter of that name, a null parameter when there is none
*/
MacroParameter MacroParameterSet::parameter(const QString &name) const
{
	for (const MacroParameter &parameter : m_parameters)
	{
		if (parameter.name == name) {
			return parameter;
		}
	}
	return MacroParameter();
}

/**
	@brief MacroParameterSet::append
	@param parameter
	@return false when the parameter is nameless or the name is taken

	The first declaration of a name wins. A .qetmak that declares TAG twice
	is a file someone edited by hand, and keeping the first keeps the order
	the rest of the file was written against.
*/
bool MacroParameterSet::append(const MacroParameter &parameter)
{
	if (parameter.name.isEmpty()) {
		return false;
	}
	if (contains(parameter.name))
	{
		qWarning() << "MacroParameterSet: parameter declared twice, keeping the first:"
			   << parameter.name;
		return false;
	}
	m_parameters << parameter;
	return true;
}

/**
	@brief MacroParameterSet::clear
*/
void MacroParameterSet::clear()
{
	m_parameters.clear();
	m_value_sets.clear();
}

/**
	@brief MacroParameterSet::defaults
	@return every declared parameter mapped to its default value
*/
QHash<QString, QString> MacroParameterSet::defaults() const
{
	QHash<QString, QString> values;
	for (const MacroParameter &parameter : m_parameters) {
		values.insert(parameter.name, parameter.default_value);
	}
	return values;
}

/**
	@brief MacroParameterSet::missingRequired
	@param values
	@return the required parameters @a values leaves blank, in declaration order
*/
QStringList MacroParameterSet::missingRequired(const QHash<QString, QString> &values) const
{
	QStringList missing;
	for (const MacroParameter &parameter : m_parameters)
	{
		if (!parameter.required) {
			continue;
		}
			//A value of nothing but spaces is blank to the person looking at
			//the drawing, so it is blank here too.
		if (values.value(parameter.name).trimmed().isEmpty()) {
			missing << parameter.name;
		}
	}
	return missing;
}

/**
	@brief MacroParameterSet::undeclared
	@param values
	@return the names in @a values that no parameter declares, sorted
*/
QStringList MacroParameterSet::undeclared(const QHash<QString, QString> &values) const
{
	QStringList unknown;
	for (auto it = values.constBegin() ; it != values.constEnd() ; ++it)
	{
		if (!contains(it.key())) {
			unknown << it.key();
		}
	}
		//Sorted, because a QHash hands its keys over in whatever order it
		//likes and an error message that changes wording between two runs of
		//the same file is an error message nobody trusts.
	unknown.sort();
	return unknown;
}

/**
	@brief MacroParameterSet::valueSetNames
	@return the names of the value sets, in declaration order
*/
QStringList MacroParameterSet::valueSetNames() const
{
	QStringList list;
	list.reserve(m_value_sets.count());
	for (const MacroValueSet &value_set : m_value_sets) {
		list << value_set.name;
	}
	return list;
}

/**
	@brief MacroParameterSet::hasValueSet
	@param name
	@return whether a value set of that name is declared
*/
bool MacroParameterSet::hasValueSet(const QString &name) const
{
	for (const MacroValueSet &value_set : m_value_sets)
	{
		if (value_set.name == name) {
			return true;
		}
	}
	return false;
}

/**
	@brief MacroParameterSet::valueSet
	@param name
	@return the value set of that name, a null set when there is none
*/
MacroValueSet MacroParameterSet::valueSet(const QString &name) const
{
	for (const MacroValueSet &value_set : m_value_sets)
	{
		if (value_set.name == name) {
			return value_set;
		}
	}
	return MacroValueSet();
}

/**
	@brief MacroParameterSet::appendValueSet
	@param value_set
	@return false when the set is nameless or the name is taken
*/
bool MacroParameterSet::appendValueSet(const MacroValueSet &value_set)
{
	if (value_set.isNull()) {
		return false;
	}
	if (hasValueSet(value_set.name))
	{
		qWarning() << "MacroParameterSet: value set declared twice, keeping the first:"
			   << value_set.name;
		return false;
	}
	m_value_sets << value_set;
	return true;
}

/**
	@brief MacroParameterSet::applyValueSet
	@param name : the value set to lay over @a current
	@param current : the values as they stand
	@return the merged values
*/
QHash<QString, QString> MacroParameterSet::applyValueSet(const QString &name,
							 const QHash<QString, QString> &current) const
{
	QHash<QString, QString> merged = current;
	const MacroValueSet value_set = valueSet(name);
	if (value_set.isNull()) {
		return merged;
	}
	for (auto it = value_set.values.constBegin() ; it != value_set.values.constEnd() ; ++it) {
		merged.insert(it.key(), it.value());
	}
	return merged;
}

/**
	@brief MacroParameterSet::fromXml
	@param qet_macro_root : the <qet_macro> root of a macro file
	@return false only when there is no root to read

	No <parameters> block is not an error: it is every macro written before
	this task, and it has to keep inserting exactly as it does today. The
	caller asks isEmpty() to know which case it is in.
*/
bool MacroParameterSet::fromXml(const QDomElement &qet_macro_root)
{
	clear();
	if (qet_macro_root.isNull()) {
		return false;
	}

	const QDomElement parameters_element =
		qet_macro_root.firstChildElement(parametersTagName());
	if (!parameters_element.isNull())
	{
		for (QDomElement child = parameters_element.firstChildElement(MacroParameter::tagName()) ;
		     !child.isNull() ;
		     child = child.nextSiblingElement(MacroParameter::tagName()))
		{
			MacroParameter parameter;
			if (!parameter.fromXml(child))
			{
				qWarning() << "MacroParameterSet: a <parameter> without a name was ignored.";
				continue;
			}
			append(parameter);
		}
	}

	const QDomElement value_sets_element =
		qet_macro_root.firstChildElement(valueSetsTagName());
	if (!value_sets_element.isNull())
	{
		for (QDomElement child = value_sets_element.firstChildElement(MacroValueSet::tagName()) ;
		     !child.isNull() ;
		     child = child.nextSiblingElement(MacroValueSet::tagName()))
		{
			MacroValueSet value_set;
			if (!value_set.fromXml(child))
			{
				qWarning() << "MacroParameterSet: a <valueset> without a name was ignored.";
				continue;
			}
			appendValueSet(value_set);
		}
	}
	return true;
}

/**
	@brief MacroParameterSet::appendToXml
	@param document
	@param qet_macro_root : receives the blocks

	Nothing is written for an empty set, which is what keeps a macro without
	parameters byte for byte the file it was.
*/
void MacroParameterSet::appendToXml(QDomDocument &document, QDomElement &qet_macro_root) const
{
	if (qet_macro_root.isNull()) {
		return;
	}
	const QDomElement parameters_element = parametersToXml(document);
	if (!parameters_element.isNull()) {
		qet_macro_root.appendChild(parameters_element);
	}
	const QDomElement value_sets_element = valueSetsToXml(document);
	if (!value_sets_element.isNull()) {
		qet_macro_root.appendChild(value_sets_element);
	}
}

/**
	@brief MacroParameterSet::parametersToXml
	@param document
	@return the <parameters> block, a null element when nothing is declared
*/
QDomElement MacroParameterSet::parametersToXml(QDomDocument &document) const
{
	if (m_parameters.isEmpty()) {
		return QDomElement();
	}
	QDomElement element = document.createElement(parametersTagName());
	for (const MacroParameter &parameter : m_parameters)
	{
		QDomElement child;
		if (parameter.toXml(document, child)) {
			element.appendChild(child);
		}
	}
	return element;
}

/**
	@brief MacroParameterSet::valueSetsToXml
	@param document
	@return the <valuesets> block, a null element when no set is declared

	The values of a set are written in the order the parameters are declared,
	and anything the set names that no parameter declares comes after, sorted.
	A QHash hands its keys over in whatever order it likes, and a file that
	comes out shuffled every time it is saved cannot be diffed - which is the
	first thing anyone does when a macro starts behaving oddly.
*/
QDomElement MacroParameterSet::valueSetsToXml(QDomDocument &document) const
{
	if (m_value_sets.isEmpty()) {
		return QDomElement();
	}
	QDomElement element = document.createElement(valueSetsTagName());
	for (const MacroValueSet &value_set : m_value_sets)
	{
		QDomElement set_element = document.createElement(MacroValueSet::tagName());
		set_element.setAttribute(QStringLiteral("name"), value_set.name);

		QStringList written;
		for (const MacroParameter &parameter : m_parameters)
		{
			if (!value_set.values.contains(parameter.name)) {
				continue;
			}
			written << parameter.name;
		}
		QStringList strays;
		for (auto it = value_set.values.constBegin() ; it != value_set.values.constEnd() ; ++it)
		{
			if (!written.contains(it.key())) {
				strays << it.key();
			}
		}
		strays.sort();
		written << strays;

		for (const QString &value_name : written)
		{
			QDomElement value_element = document.createElement(MacroValueSet::valueTagName());
			value_element.setAttribute(QStringLiteral("name"), value_name);
			value_element.appendChild(document.createTextNode(value_set.values.value(value_name)));
			set_element.appendChild(value_element);
		}
		element.appendChild(set_element);
	}
	return element;
}

/**
	@return the tag name of the block that declares the parameters
*/
QString MacroParameterSet::parametersTagName()
{
	return QStringLiteral("parameters");
}

/**
	@return the tag name of the block that holds the value sets
*/
QString MacroParameterSet::valueSetsTagName()
{
	return QStringLiteral("valuesets");
}
