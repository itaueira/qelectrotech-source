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
#include "macroparameter.h"

#include "macrosubstitution.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QRegularExpression>

/**
	@brief MacroParameter::MacroParameter
	Default constructor. Builds a free text parameter with no name, which is
	not valid on purpose: a parameter has to be named before it can answer to
	a marker.
*/
MacroParameter::MacroParameter()
{}

/**
	@brief MacroParameter::MacroParameter
	@param name : the marker name, without the braces
	@param label : user visible label
	@param type : type of the parameter
*/
MacroParameter::MacroParameter(const QString &name,
			       const QString &label,
			       MacroParameterType type) :
	name(name),
	label(label),
	type(type)
{}

/**
	@brief MacroParameter::isNull
	@return true when this parameter answers to no marker
*/
bool MacroParameter::isNull() const
{
	return name.isEmpty();
}

/**
	@brief MacroParameter::isValid
	@param error : when not nullptr, receives why the parameter is refused
	@return whether this parameter can be declared as it stands
*/
bool MacroParameter::isValid(QString *error) const
{
	if (name.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("MacroParameter",
							     "Le paramètre est sans nom.");
		}
		return false;
	}
	if (!isValidName(name))
	{
		if (error) {
			*error = QCoreApplication::translate("MacroParameter",
							     "Nom de paramètre invalide : %1. Une lettre ou un "
							     "souligné, puis des lettres, des chiffres et des "
							     "soulignés.").arg(name);
		}
		return false;
	}
	if (type == MacroParameterType::List && choices.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("MacroParameter",
							     "Le paramètre %1 est une liste sans valeur.").arg(name);
		}
		return false;
	}
		//A default value the parameter itself would refuse is a trap: the
		//insertion would fail before the user had touched anything.
	if (!default_value.isEmpty() && !acceptsValue(default_value))
	{
		if (error) {
			*error = QCoreApplication::translate("MacroParameter",
							     "La valeur par défaut %1 est absente de la liste du "
							     "paramètre %2.").arg(default_value, name);
		}
		return false;
	}
	return true;
}

/**
	@brief MacroParameter::marker
	@return the marker this parameter answers to, "${TAG}" for TAG
*/
QString MacroParameter::marker()  const
{
	return MacroSubstitution::marker(name);
}

/**
	@brief MacroParameter::acceptsValue
	@param value
	@return whether @a value may be stored in this parameter
*/
bool MacroParameter::acceptsValue(const QString &value) const
{
	if (type == MacroParameterType::List) {
		return choices.contains(value);
	}
	return true;
}

/**
	@brief MacroParameter::toXml
	@param document : the document the element is created in
	@param element : receives the <parameter> block
	@return false when the parameter is not in a state worth writing
*/
bool MacroParameter::toXml(QDomDocument &document, QDomElement &element) const
{
	if (name.isEmpty()) {
		return false;
	}
	element = document.createElement(tagName());
	element.setAttribute(QStringLiteral("name"), name);
	if (!label.isEmpty()) {
		element.setAttribute(QStringLiteral("label"), label);
	}
	element.setAttribute(QStringLiteral("type"), typeToString(type));
	element.setAttribute(QStringLiteral("default"), default_value);
	if (!unit.isEmpty()) {
		element.setAttribute(QStringLiteral("unit"), unit);
	}
	element.setAttribute(QStringLiteral("required"),
			     required ? QStringLiteral("true") : QStringLiteral("false"));
	if (!description.isEmpty()) {
		element.setAttribute(QStringLiteral("description"), description);
	}
	for (const QString &choice : choices)
	{
		QDomElement choice_element = document.createElement(choiceTagName());
		choice_element.appendChild(document.createTextNode(choice));
		element.appendChild(choice_element);
	}
	return true;
}

/**
	@brief MacroParameter::toXml
	@param document
	@return the <parameter> block, a null element when there is nothing to write
*/
QDomElement MacroParameter::toXml(QDomDocument &document) const
{
	QDomElement element;
	if (!toXml(document, element)) {
		return QDomElement();
	}
	return element;
}

/**
	@brief MacroParameter::fromXml
	@param element : a <parameter> block
	@return false when @a element does not describe a parameter

	Reading is deliberately tolerant, because a .qetmak is a file someone
	edits by hand: an unknown type falls back to text, a missing "required"
	means false, and the order of the children does not matter. The one
	thing refused is a parameter with no name, because a nameless parameter
	answers to no marker and could only be a silent passenger.
*/
bool MacroParameter::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}
	const QString read_name = element.attribute(QStringLiteral("name")).trimmed();
	if (read_name.isEmpty()) {
		return false;
	}

	name = read_name;
	label = element.attribute(QStringLiteral("label"));
	type = typeFromString(element.attribute(QStringLiteral("type")).trimmed().toLower());
	default_value = element.attribute(QStringLiteral("default"));
	unit = element.attribute(QStringLiteral("unit"));
	description = element.attribute(QStringLiteral("description"));

		//"true", "1" and "yes" all mean the same thing to someone writing
		//this by hand, and refusing two of the three would be a riddle.
	const QString read_required = element.attribute(QStringLiteral("required")).trimmed().toLower();
	required = (read_required == QLatin1String("true")
		    || read_required == QLatin1String("1")
		    || read_required == QLatin1String("yes"));

	choices.clear();
	for (QDomElement child = element.firstChildElement(choiceTagName()) ;
	     !child.isNull() ;
	     child = child.nextSiblingElement(choiceTagName()))
	{
		choices << child.text();
	}
	return true;
}

/**
	@return the tag name of a parameter block
*/
QString MacroParameter::tagName()
{
	return QStringLiteral("parameter");
}

/**
	@return the tag name of one value offered by a list parameter
*/
QString MacroParameter::choiceTagName()
{
	return QStringLiteral("choice");
}

/**
	@brief MacroParameter::typeToString
	@param type
	@return the stable string written in the .qetmak
*/
QString MacroParameter::typeToString(MacroParameterType type)
{
	switch (type)
	{
		case MacroParameterType::Text:   return QStringLiteral("text");
		case MacroParameterType::Number: return QStringLiteral("number");
		case MacroParameterType::List:   return QStringLiteral("list");
		case MacroParameterType::Part:   return QStringLiteral("part");
	}
	return QStringLiteral("text");
}

/**
	@brief MacroParameter::typeFromString
	@param string
	@param ok : when not nullptr, set to false for an unknown string
	@return the type @a string names, Text when it names nothing known.
	A macro written by a newer version that knows a type this build does not
	is read as text rather than refused: the drawing still comes in, and the
	value is still substituted.
*/
MacroParameterType MacroParameter::typeFromString(const QString &string, bool *ok)
{
	if (ok) {
		*ok = true;
	}
	const QList<MacroParameterType> types = allTypes();
	for (const MacroParameterType type : types)
	{
		if (typeToString(type) == string) {
			return type;
		}
	}
	if (ok) {
		*ok = false;
	}
	return MacroParameterType::Text;
}

/**
	@brief MacroParameter::translatedTypeName
	@param type
	@return the name of @a type in the user language
*/
QString MacroParameter::translatedTypeName(MacroParameterType type)
{
	switch (type)
	{
		case MacroParameterType::Text:
			return QCoreApplication::translate("MacroParameter", "Texte");
		case MacroParameterType::Number:
			return QCoreApplication::translate("MacroParameter", "Nombre");
		case MacroParameterType::List:
			return QCoreApplication::translate("MacroParameter", "Liste");
		case MacroParameterType::Part:
			return QCoreApplication::translate("MacroParameter", "Pièce");
	}
	return QCoreApplication::translate("MacroParameter", "Texte");
}

/**
	@return every type, in the order they are offered to the user
*/
QList<MacroParameterType> MacroParameter::allTypes()
{
	return QList<MacroParameterType>{MacroParameterType::Text,
					 MacroParameterType::Number,
					 MacroParameterType::List,
					 MacroParameterType::Part};
}

/**
	@brief MacroParameter::isValidName
	@param name
	@return whether @a name may be declared as a parameter name
*/
bool MacroParameter::isValidName(const QString &name)
{
	static const QRegularExpression allowed(QStringLiteral("\\A[A-Za-z_][A-Za-z0-9_]*\\z"));
	return allowed.match(name).hasMatch();
}
