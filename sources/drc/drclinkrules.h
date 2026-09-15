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
#ifndef DRCLINKRULES_H
#define DRCLINKRULES_H

#include "drcfinding.h"
#include "drcrule.h"

#include <QList>
#include <QString>

class DrcRuleSet;
class DrcScanEngine;
class QETProject;

/**
	@brief The three rules a cross reference and a terminal strip answer in
	one call.

	They are the first rules written against the contract, and they were
	chosen for that because the project already knows the answer to all
	three: ElementProvider::freeElement() hands back every component of a
	given kind that is linked to nothing, and freeTerminal() hands back
	every terminal drawn on a folio that belongs to no strip. There is no
	measurement to invent and no data that is missing.

	They are all of the Scan route, and not by preference. A coil with no
	contact is a question about a link, and a link is nowhere in the
	project data base; a slave contact has no row in any table at all,
	because populateElementTable() filters the components by kind and
	leaves it out; and a terminal only reaches the terminal table once a
	conductor is drawn to it. Each of the three, written as a SELECT,
	would come back with nothing and report a clean project.

	@par What they do not judge
	A coil with no contact is a warning and never an error, and so is a
	contact with no coil: the legitimate case is a drawing still being
	made, where the contact is going on the next folio. A terminal outside
	a strip is a warning for the same kind of reason - nobody has counted
	yet how many terminals of a finished project are in that state, and a
	rule that turned out to fire on all of them would have taught the
	office to ignore the panel before anybody had the number.

	@par The wording names the component and says where
	Each finding carries the label of what is wrong; the folio and the
	position are filled by the engine, once, for every rule. A component
	with no label reads as such rather than as an empty sentence - a row
	that names nothing is a row nobody can act on.
*/
namespace DrcLinkRules
{
	/// Machine key of "a master element with no slave contact".
	QString masterWithoutSlaveIdentifier();
	/// Machine key of "a slave contact with no master element".
	QString slaveWithoutMasterIdentifier();
	/// Machine key of "a terminal drawn outside any terminal strip".
	QString terminalOutsideStripIdentifier();

	/**
		Every master element of @a project that is linked to no contact.

		@param project the project to walk
		@param rule the rule as it stands, whose current severity every
		finding copies
		@return one finding per master element with an empty link list
	*/
	QList<DrcFinding> masterWithoutSlave(QETProject *project,
					     const DrcRule &rule);

	/**
		Every slave contact of @a project that is linked to no master.

		The same call as above with the other filter, and that is the
		point: the two directions of a cross reference are two arguments,
		not two implementations.

		@param project the project to walk
		@param rule the rule as it stands
		@return one finding per slave contact with an empty link list
	*/
	QList<DrcFinding> slaveWithoutMaster(QETProject *project,
					     const DrcRule &rule);

	/**
		Every terminal of @a project that belongs to no terminal strip.

		@param project the project to walk
		@param rule the rule as it stands
		@return one finding per terminal with no parent strip
	*/
	QList<DrcFinding> terminalOutsideStrip(QETProject *project,
					       const DrcRule &rule);

	/**
		Put the three of them into a register and an engine.

		@param set the register they join
		@param engine the engine that will walk them
		@return how many were taken. Three on a fresh register; less when
		one of the identifiers was already there, which the register
		refuses rather than replaces.
	*/
	int registerRules(DrcRuleSet &set, DrcScanEngine &engine);
}

#endif // DRCLINKRULES_H
