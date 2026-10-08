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
#include "refreshembeddedelementscommand.h"

#include "../ElementsCollection/elementorigin.h"
#include "../ElementsCollection/elementslocation.h"
#include "../ElementsCollection/embeddedelementrefresh.h"
#include "../ElementsCollection/terminaluuids.h"
#include "../ElementsCollection/xmlelementcollection.h"
#include "../factory/elementpicturefactory.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QObject>

namespace {

	//A document holding a deep copy of @p definition, and nothing else
QDomDocument copyOf(const QDomElement &definition)
{
	QDomDocument document;
	document.appendChild(document.importNode(definition, true));
	return document;
}

}

/**
	@brief RefreshEmbeddedElementsCommand::RefreshEmbeddedElementsCommand
	@param project : the project whose collection changes
	@param changes : the new <definition> of each symbol. A path the
	project does not hold is skipped.
	@param parent
*/
RefreshEmbeddedElementsCommand::RefreshEmbeddedElementsCommand(
		QETProject *project,
		const QList<Change> &changes,
		QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project)
{
	for (const Change &change : changes)
	{
		const ElementsLocation copy(change.embedded_path, project);
		const QDomElement current = copy.xml();
		if (current.isNull() || change.definition.isNull()) {
			continue;
		}

		QDomDocument fresh;
		QDomElement fresh_element = fresh.createElement(QStringLiteral("element"));
		fresh.appendChild(fresh_element);
		fresh_element.appendChild(fresh.importNode(change.definition.documentElement(), true));
		TerminalUuids::keep(current.parentNode().toElement(), fresh_element);
		const QDomElement fresh_definition =
				fresh_element.firstChildElement(QStringLiteral("definition"));
		TerminalUuids::fillMissingInDefinition(fresh_definition);

		m_old << Change{change.embedded_path, copyOf(current)};
		m_new << Change{change.embedded_path, copyOf(fresh_definition)};
	}

	setText(QObject::tr("Mettre à jour %n élément(s) depuis la collection",
			    "", m_new.size()));
}

/**
	@brief RefreshEmbeddedElementsCommand::undo
	Put the previous definitions back and redraw from them
*/
void RefreshEmbeddedElementsCommand::undo()
{
	apply(m_old);
}

/**
	@brief RefreshEmbeddedElementsCommand::redo
	Put the new definitions in and redraw from them
*/
void RefreshEmbeddedElementsCommand::redo()
{
	apply(m_new);
}

/**
	@brief RefreshEmbeddedElementsCommand::apply
	Write @p definitions into the project's collection, forget the drawings
	cached for them, and redraw every element placed from them.

	Each element whose terminals the definition has is given its size and
	hotspot first: the symbol editor recomputes them on every save, and
	the redraw refuses a box that differs from the element's. The old box
	comes back on undo, with the old definition. An element whose terminals
	differ is left as it is: it is only brought here when the project is
	about to be reloaded, which builds it again anyway.
*/
void RefreshEmbeddedElementsCommand::apply(const QList<Change> &definitions)
{
	if (!m_project) {
		return;
	}

	XmlElementCollection *collection = m_project->embeddedElementCollection();
	for (const Change &change : definitions)
	{
		const ElementsLocation copy(change.embedded_path, m_project);
		const QDomElement current = copy.xml();
		if (current.isNull()) {
			continue;
		}

		QDomNode element_node = current.parentNode();
		element_node.replaceChild(
					current.ownerDocument().importNode(change.definition.documentElement(), true),
					current);
			//The copy is now the version it was taken from: a copy is only
			//ever brought when it had not been saved again since, so on undo
			//the old definition is its own origin too
		ElementOrigin::set(element_node.toElement(),
						   ElementOrigin::definitionUuid(change.definition.documentElement()));

			//The panel of collections shows the project's copy too
		emit collection->elementChanged(change.embedded_path);

		ElementPictureFactory::instance()->dropCache(copy);
		const QDomElement applied = change.definition.documentElement();
		for (Element *element :
			 EmbeddedElementRefresh::placedElements(m_project, change.embedded_path))
		{
			if (element->definitionTerminalsMatch(applied)) {
				element->adoptDefinitionBox(applied);
			}
			element->reloadPicture();
		}
	}
}
