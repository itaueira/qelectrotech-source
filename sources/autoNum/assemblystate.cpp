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
#include "assemblystate.h"

#include <QCoreApplication>
#include <QDomDocument>

namespace
{
	/// The child node one frozen component is written as.
	const char *component_tag = "component";
	/// The child node one frozen conductor is written as.
	const char *conductor_tag = "conductor";
	const char *uuid_attribute = "uuid";
	const char *label_attribute = "label";

	/**
		@param parent the node the entries hang under
		@param document
		@param tag_name the child node name
		@param entries uuid -> label

		Written in the order of the map, which is the order of the uuid: the
		same photograph produces the same bytes every time it is saved.
	*/
	void writeEntries(QDomElement &parent, QDomDocument &document,
			  const char *tag_name,
			  const QMap<QString, QString> &entries)
	{
		for (auto iterator = entries.constBegin() ;
		     iterator != entries.constEnd() ; ++iterator)
		{
			QDomElement entry = document.createElement(QLatin1String(tag_name));
			entry.setAttribute(QLatin1String(uuid_attribute), iterator.key());
			entry.setAttribute(QLatin1String(label_attribute), iterator.value());
			parent.appendChild(entry);
		}
	}

	/**
		@param parent
		@param tag_name
		@return the entries @a parent holds under @a tag_name

		An entry with no uuid is dropped rather than stored under an empty
		key: a null identity matches every component that has none, and the
		freezing would spread to whatever the next reading finds.
	*/
	QMap<QString, QString> readEntries(const QDomElement &parent,
					   const char *tag_name)
	{
		QMap<QString, QString> entries;
		for (QDomElement child = parent.firstChildElement(QLatin1String(tag_name)) ;
		     !child.isNull() ;
		     child = child.nextSiblingElement(QLatin1String(tag_name)))
		{
			const QString uuid = child.attribute(QLatin1String(uuid_attribute));
			if (uuid.isEmpty()) {
				continue;
			}
			entries.insert(uuid, child.attribute(QLatin1String(label_attribute)));
		}
		return entries;
	}
}

/**
	@brief AssemblyState::AssemblyState
*/
AssemblyState::AssemblyState()
{}

/**
	@brief AssemblyState::isFrozen
	@return whether the automation has to leave the photograph alone
*/
bool AssemblyState::isFrozen() const
{
	return stage != AssemblyStage::InProject;
}

/**
	@brief AssemblyState::isEmpty
	@return whether this state has nothing to say
*/
bool AssemblyState::isEmpty() const
{
	return stage == AssemblyStage::InProject
	       && components.isEmpty()
	       && conductors.isEmpty()
	       && marked_by.isEmpty()
	       && marked_at.isEmpty();
}

/**
	@brief AssemblyState::frozenCount
	@return how many items the photograph holds
*/
int AssemblyState::frozenCount() const
{
	return components.size() + conductors.size();
}

/**
	@brief AssemblyState::holdsComponent
	@param uuid
	@return whether this component existed when the project was marked
*/
bool AssemblyState::holdsComponent(const QString &uuid) const
{
	return !uuid.isEmpty() && components.contains(uuid);
}

/**
	@brief AssemblyState::holdsConductor
	@param uuid
	@return whether this conductor existed when the project was marked
*/
bool AssemblyState::holdsConductor(const QString &uuid) const
{
	return !uuid.isEmpty() && conductors.contains(uuid);
}

/**
	@brief AssemblyState::componentLabel
	@param uuid
	@return the tag this component carried when the project was marked
*/
QString AssemblyState::componentLabel(const QString &uuid) const
{
	return components.value(uuid);
}

/**
	@brief AssemblyState::conductorLabel
	@param uuid
	@return the text this conductor carried when the project was marked
*/
QString AssemblyState::conductorLabel(const QString &uuid) const
{
	return conductors.value(uuid);
}

bool AssemblyState::operator==(const AssemblyState &other) const
{
	return stage == other.stage
	       && components == other.components
	       && conductors == other.conductors
	       && marked_by == other.marked_by
	       && marked_at == other.marked_at;
}

bool AssemblyState::operator!=(const AssemblyState &other) const
{
	return !(*this == other);
}

/**
	@brief AssemblyState::toXml
	@param document
	@return the node to hang under the project root
*/
QDomElement AssemblyState::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());
	element.setAttribute(QStringLiteral("stage"), stageToString(stage));

		//Written only when filled, so that the day T25 starts filling them
		//the file of a project that predates it does not grow two empty
		//attributes it never had.
	if (!marked_by.isEmpty()) {
		element.setAttribute(QStringLiteral("marked_by"), marked_by);
	}
	if (!marked_at.isEmpty()) {
		element.setAttribute(QStringLiteral("marked_at"), marked_at);
	}

	writeEntries(element, document, component_tag, components);
	writeEntries(element, document, conductor_tag, conductors);
	return element;
}

/**
	@brief AssemblyState::fromXml
	@param element
	@return whether the node was one of ours
*/
bool AssemblyState::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
			//A project saved before this existed says nothing, and silence
			//means in project. Reading is tolerant; that is the rule.
		return false;
	}

	stage = stageFromString(element.attribute(QStringLiteral("stage")));
	marked_by = element.attribute(QStringLiteral("marked_by"));
	marked_at = element.attribute(QStringLiteral("marked_at"));
	components = readEntries(element, component_tag);
	conductors = readEntries(element, conductor_tag);
	return true;
}

QString AssemblyState::tagName()
{
	return QStringLiteral("assembly_state");
}

QString AssemblyState::stageToString(AssemblyStage stage)
{
	switch (stage) {
		case AssemblyStage::Assembled:
			return QStringLiteral("assembled");
		case AssemblyStage::InField:
			return QStringLiteral("in_field");
		case AssemblyStage::InProject:
			break;
	}
	return QStringLiteral("in_project");
}

AssemblyStage AssemblyState::stageFromString(const QString &string)
{
		//Anything unknown reads as in project, which is the stage that
		//restricts nothing: a project written by a later version that invents
		//a fourth stage still opens, and opens without silently refusing to
		//renumber what the drawer asked it to renumber.
	if (string == QLatin1String("assembled")) {
		return AssemblyStage::Assembled;
	}
	if (string == QLatin1String("in_field")) {
		return AssemblyStage::InField;
	}
	return AssemblyStage::InProject;
}

QString AssemblyState::translatedStage(AssemblyStage stage)
{
	switch (stage) {
		case AssemblyStage::InProject:
			return QCoreApplication::translate("AssemblyState",
				"En étude");
		case AssemblyStage::Assembled:
			return QCoreApplication::translate("AssemblyState",
				"Monté");
		case AssemblyStage::InField:
			return QCoreApplication::translate("AssemblyState",
				"En service");
	}
	return QString();
}
