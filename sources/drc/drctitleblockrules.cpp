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
#include "drctitleblockrules.h"

#include "drcruleset.h"
#include "drcscanengine.h"

#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetproject.h"

#include <QCoreApplication>

#include <algorithm>

/**
	@brief DrcTitleBlockRules::emptyFieldIdentifier
	@return the machine key of the rule. Never translated: the project file
	writes it.
*/
QString DrcTitleBlockRules::emptyFieldIdentifier()
{
	return QStringLiteral("empty_title_block_field");
}

/**
	@brief DrcTitleBlockRules::emptyFields
	@param project : the project to walk
	@param rule : the rule as it stands
	@param required : the fields that must be filled in
	@return one finding per folio leaving one of them blank

	The context is read off the folio, and not rebuilt from its title
	block properties here: it is what the renderer draws the cells with and
	what diagram_info is filled from, so the finding, the table and the
	printed sheet are three readings of one value.
*/
QList<DrcFinding> DrcTitleBlockRules::emptyFields(QETProject *project,
						  const DrcRule &rule,
						  const QStringList &required)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

		//diagrams() builds every folio, so that a folio nobody has opened
		//yet is read like the others - and it hands them out in folio
		//order, which is the order the findings are listed in.
	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
	{
		const QStringList empty = DrcTitleBlock::emptyKeys(
				diagram->border_and_titleblock.titleblockInformation(),
				required);
		const QString sentence = DrcTitleBlock::emptyFieldsText(empty);
		if (sentence.isEmpty()) {
			continue;
		}

		findings.append(DrcFinding::onDiagram(rule.identifier(),
						      rule.severity(),
						      sentence,
						      diagram));
	}

	return findings;
}

/**
	@brief DrcTitleBlockRules::registerRules
	@param set : the register the rules join
	@param engine : the engine that will walk them
	@param required : the fields every folio must fill in
	@return how many were taken

	Born a warning and switched on: see the header for why.

	A list that names no field registers nothing. The rule would be walked,
	find nothing on any project, and count as a rule run - and a check
	whose only rule it was would then call the project clean, having
	looked at nothing. Not asking for any field is switching the rule off,
	and it is said that way.
*/
int DrcTitleBlockRules::registerRules(DrcRuleSet &set,
				      DrcScanEngine &engine,
				      const QStringList &required)
{
	const bool names_a_field =
			std::any_of(required.cbegin(), required.cend(),
				    [](const QString &key) {
				return !key.isEmpty();
			});
	if (!names_a_field) {
		return 0;
	}

	int taken = 0;

		//The list is copied into the walk, so that the rule keeps asking
		//for what it was registered with whatever happens to the caller's.
	const QStringList fields = required;
	if (engine.addRule(set,
			   DrcRule(emptyFieldIdentifier(),
				   QCoreApplication::translate(
					   "DrcTitleBlockRules",
					   "Folio with an empty title block field"),
				   DrcSeverity::Warning,
				   DrcRoute::Scan),
			   [fields](QETProject *project, const DrcRule &rule) {
				   return DrcTitleBlockRules::emptyFields(
						   project, rule, fields);
			   })) {
		++taken;
	}

	return taken;
}
