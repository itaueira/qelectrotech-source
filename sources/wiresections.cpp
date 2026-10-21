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
#include "wiresections.h"

#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "conductorautonumerotation.h"
#include "diagram.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "wiregauge.h"

#include <QObject>
#include <QSet>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVariant>

namespace
{
		/**
			@return true when @p potential holds a wire of a panel already
			built: the point is then left alone, whole (see the header)
		*/
	bool holdsFrozen(const QList<Conductor *> &potential)
	{
		for (const Conductor *conductor : potential)
		{
			if (ConductorAutoNumerotation::isFrozen(conductor)) {
				return true;
			}
		}
		return false;
	}

		/// @p section on @p conductor at once, with no undo command
	void setSection(Conductor *conductor, const QString &section)
	{
			//Left out of the numbering: not written (T77)
		if (!conductor || section.isEmpty()
		    || ConductorAutoNumerotation::isIgnored(conductor)) {
			return;
		}
		ConductorProperties properties = conductor->properties();
		if (properties.m_wire_section == section) {
			return;
		}
		properties.m_wire_section = section;
		conductor->setProperties(properties);
	}

		/**
			The conductors of @p potential whose section is not
			@p section, and what each one gets; none when the point holds
			a wire of a panel already built.
			@param potential : the point, or the part of it to change
			@param frozen_check : the whole point, the one asked whether
			it holds such a wire
		*/
	QList<WireSections::Change> changesInPotential(
			const QList<Conductor *> &potential,
			const QString &section,
			const QList<Conductor *> &frozen_check)
	{
		QList<WireSections::Change> changes;
		if (section.isEmpty() || holdsFrozen(frozen_check)) {
			return changes;
		}
		for (Conductor *conductor : potential)
		{
				//Left out of the numbering: not written (T77)
			if (!conductor || ConductorAutoNumerotation::isIgnored(conductor)) {
				continue;
			}
			const ConductorProperties before = conductor->properties();
			if (before.m_wire_section == section) {
				continue;
			}
			WireSections::Change change;
			change.conductor = conductor;
			change.before = before;
			change.after = before;
			change.after.m_wire_section = section;
			changes << change;
		}
		return changes;
	}

	QString undoCaption()
	{
		return QObject::tr("Update the wire cross-sections", "undo caption");
	}
}

/**
	@brief WireSections::componentSection
	See the header.
*/
QString WireSections::componentSection(const Element *element)
{
	if (!element) {
		return QString();
	}
	return element->elementInformations()
			.value(QETInformation::ELMT_WIRE_SECTION).toString();
}

/**
	@brief WireSections::potentialOf
	See the header.
*/
QList<Conductor *> WireSections::potentialOf(Conductor *conductor)
{
	QList<Conductor *> potential;
	if (!conductor) {
		return potential;
	}
	potential << conductor;
		//Across folio reports, as the numbering goes: two ends of a report
		//are one point, and one number
	const QSet<Conductor *> related = conductor->relatedPotentialConductors();
	for (Conductor *other : related)
	{
		if (other && other != conductor) {
			potential << other;
		}
	}
	return potential;
}

/**
	@brief WireSections::sectionOf
	See the header.
*/
QString WireSections::sectionOf(const QList<Conductor *> &potential)
{
		//A component at both ends of a conductor, or on several conductors
		//of the point, is read once: what is asked is the largest, and a
		//text read twice changes nothing but the time it takes
	QSet<const Element *> read;
	QStringList texts;
	for (const Conductor *conductor : potential)
	{
		if (!conductor) {
			continue;
		}
		for (const Terminal *terminal : {conductor->terminal1, conductor->terminal2})
		{
			const Element *element = terminal ? terminal->parentElement() : nullptr;
			if (!element || read.contains(element)) {
				continue;
			}
			read.insert(element);
			texts << componentSection(element);
		}
	}
	return WireGauge::largest(texts);
}

/**
	@brief WireSections::sectionOfPotential
	See the header.
*/
QString WireSections::sectionOfPotential(Conductor *conductor)
{
	return sectionOf(potentialOf(conductor));
}

/**
	@brief WireSections::changesFor
	See the header.
*/
QList<WireSections::Change> WireSections::changesFor(const QList<Conductor *> &conductors)
{
	QList<Change> changes;
	QSet<Conductor *> seen;
	for (Conductor *conductor : conductors)
	{
		if (!conductor || seen.contains(conductor)) {
			continue;
		}
		const QList<Conductor *> potential = potentialOf(conductor);
		for (Conductor *member : potential) {
			seen.insert(member);
		}
		changes << changesInPotential(potential, sectionOf(potential), potential);
	}
	return changes;
}

/**
	@brief WireSections::changesForElements
	See the header.
*/
QList<WireSections::Change> WireSections::changesForElements(const QList<Element *> &elements)
{
	QList<Conductor *> conductors;
	for (const Element *element : elements)
	{
		if (element) {
			conductors << element->conductors();
		}
	}
	return changesFor(conductors);
}

/**
	@brief WireSections::apply
	See the header.
*/
void WireSections::apply(const QList<Change> &changes, bool forward)
{
	if (forward)
	{
		for (const Change &change : changes)
		{
			if (change.conductor) {
				change.conductor->setProperties(change.after);
			}
		}
		return;
	}
	for (int i = changes.size() - 1 ; i >= 0 ; --i)
	{
		const Change &change = changes.at(i);
		if (change.conductor) {
			change.conductor->setProperties(change.before);
		}
	}
}

/**
	@brief WireSections::addCommands
	See the header.
*/
int WireSections::addCommands(const QList<Change> &changes, QUndoCommand *parent)
{
	if (!parent) {
		return 0;
	}
	int added = 0;
	for (const Change &change : changes)
	{
		if (!change.conductor) {
			continue;
		}
			//The command every edit of conductor properties goes through,
			//so that a section given here undoes the way a section typed in
			//the conductor's dialog does
		QVariant old_value, new_value;
		old_value.setValue(change.before);
		new_value.setValue(change.after);
		new QPropertyUndoCommand(change.conductor.data(), "properties",
					 old_value, new_value, parent);
		++added;
	}
	return added;
}

/**
	@brief WireSections::giveOwnSection
	See the header.
*/
void WireSections::giveOwnSection(Conductor *conductor)
{
	const QList<Conductor *> potential = potentialOf(conductor);
	if (holdsFrozen(potential)) {
		return;
	}
	setSection(conductor, sectionOf(potential));
}

/**
	@brief WireSections::giveNewConductorItsSection
	See the header.
*/
int WireSections::giveNewConductorItsSection(Conductor *conductor, Diagram *diagram,
					     QUndoCommand *parent_undo)
{
	if (!conductor) {
		return 0;
	}
	const QList<Conductor *> potential = potentialOf(conductor);
	const QString section = sectionOf(potential);
	if (section.isEmpty() || holdsFrozen(potential)) {
		return 0;
	}
	setSection(conductor, section);

		//The others of the point, the new conductor aside: it is first in
		//the list, and has just been given the section
	const QList<Change> changes = changesInPotential(potential.mid(1), section,
							 potential);
	if (changes.isEmpty()) {
		return 0;
	}
	if (parent_undo) {
		return addCommands(changes, parent_undo);
	}
	if (!diagram) {
		return 0;
	}
	auto *undo = new QUndoCommand(undoCaption());
	const int added = addCommands(changes, undo);
	diagram->undoStack().push(undo);
	return added;
}

/**
	@brief WireSections::updateProject
	See the header.
*/
int WireSections::updateProject(QETProject *project)
{
	if (!project || project->isReadOnly()) {
		return 0;
	}

	int changed = 0;
		//diagrams() builds the folios still waiting (lazy folios): a point
		//crosses folio reports, and its far end has to be there to count
	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
			//Worked out folio by folio, after the folio before was pushed:
			//a point that crosses to a later folio was given its section
			//there already, and is not counted twice
		const QList<Change> changes = changesFor(diagram->conductors());
		if (changes.isEmpty()) {
			continue;
		}
		auto *undo = new QUndoCommand(undoCaption());
		changed += addCommands(changes, undo);
		project->undoStack()->push(undo);
	}
	return changed;
}
