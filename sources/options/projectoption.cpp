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
#include "projectoption.h"

#include <QDomDocument>
#include <QDomElement>

ProjectOption::ProjectOption()
{}

/**
	@brief ProjectOption::ProjectOption
	@param option_name the sentence the tree shows
	@param option_description what the name has no room for
*/
ProjectOption::ProjectOption(const QString &option_name,
			     const QString &option_description) :
	name(sanitizeName(option_name)),
	description(option_description)
{}

/**
	@brief ProjectOption::isNull
	@return true when there is no name

	An option without a name cannot be shown, and an option nobody can see
	in the tree is an option nobody can turn off - which is the one state
	this feature must never produce.
*/
bool ProjectOption::isNull() const
{
	return name.isEmpty();
}

/**
	@brief ProjectOption::toXml
	@param document
	@return the option as one element, empty fields left out
*/
QDomElement ProjectOption::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());

	if (!uuid.isEmpty()) {
		element.setAttribute(QStringLiteral("uuid"), uuid);
	}
	if (!parent_uuid.isEmpty()) {
		element.setAttribute(QStringLiteral("parent"), parent_uuid);
	}
	element.setAttribute(QStringLiteral("name"), name);
	if (!description.isEmpty()) {
		element.setAttribute(QStringLiteral("description"), description);
	}

		//Written only when it is on, so that a template whose options are
		//all off reads as the base project it is - and so that the file of
		//a project nobody configured yet carries no noise.
	if (switched_on) {
		element.setAttribute(QStringLiteral("on"),
				     QStringLiteral("true"));
	}

	return element;
}

/**
	@brief ProjectOption::fromXml
	@param element
	@return true when the element was one of ours

	Tolerant on purpose, as every read in this program is: a name that came
	back with the spaces somebody left around it is folded, and anything but
	"true" in the switch means off. What the tree does with a name that is
	still empty afterwards is the tree's business, not this one's.
*/
bool ProjectOption::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	uuid        = element.attribute(QStringLiteral("uuid"));
	parent_uuid = element.attribute(QStringLiteral("parent"));
	name        = sanitizeName(element.attribute(QStringLiteral("name")));
	description = element.attribute(QStringLiteral("description"));

	switched_on = element.attribute(QStringLiteral("on"))
		      == QLatin1String("true");

	return true;
}

/**
	@brief ProjectOption::tagName
	@return the name of the element that holds one option
*/
QString ProjectOption::tagName()
{
	return QStringLiteral("option");
}

/**
	@brief ProjectOption::isValidName
	@param option_name
	@param error
	@return true when the name can be given to an option
*/
bool ProjectOption::isValidName(const QString &option_name, QString *error)
{
	if (sanitizeName(option_name).isEmpty())
	{
		if (error) {
			*error = tr("Le nom d'une option ne peut pas être vide.");
		}
		return false;
	}

	return true;
}

/**
	@brief ProjectOption::sanitizeName
	@param option_name
	@return the name without the spaces a person leaves behind
*/
QString ProjectOption::sanitizeName(const QString &option_name)
{
	return option_name.simplified();
}

bool ProjectOption::operator==(const ProjectOption &other) const
{
	return uuid == other.uuid
		&& parent_uuid == other.parent_uuid
		&& name == other.name
		&& description == other.description
		&& switched_on == other.switched_on;
}

bool ProjectOption::operator!=(const ProjectOption &other) const
{
	return !(*this == other);
}
