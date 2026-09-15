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
#include "drclinkrules.h"

#include "drcruleset.h"
#include "drcscanengine.h"

#include "../elementprovider.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "../qetproject.h"

#include <QCoreApplication>
#include <QPointer>
#include <QVector>

namespace
{
	/**
		What to call a component that has no label.

		A row that names nothing is a row nobody can act on, and the
		missing-part report of the catalogue already answers this the same
		way. Saying it in one place keeps the three rules from each
		inventing their own word for it.
	*/
	QString labelOf(Element *element)
	{
		if (!element) {
			return QString();
		}

		const QString label = element->actualLabel();
		if (!label.isEmpty()) {
			return label;
		}
		return QCoreApplication::translate("DrcLinkRules",
						   "(sans repère)");
	}

	/**
		The findings of one direction of a cross reference.

		The two directions differ by the filter and by the sentence, and
		by nothing else - so they are one function with two arguments
		rather than two functions that will drift apart.

		@param project the project to walk
		@param rule the rule as it stands
		@param filter ElementData::Master or ElementData::Slave
		@param text the sentence, carrying one %1 for the label
	*/
	QList<DrcFinding> unlinkedOf(QETProject *project,
				     const DrcRule &rule,
				     ElementData::Types filter,
				     const QString &text)
	{
		QList<DrcFinding> findings;
		if (!project) {
			return findings;
		}

			//The second argument is the folio to leave out of the search,
			//and there is none: the checker looks at the whole project.
		const ElementProvider provider(project);
		const QVector<QPointer<Element>> unlinked =
				provider.freeElement(filter);

		for (const QPointer<Element> &element : unlinked)
		{
			if (element.isNull()) {
				continue;
			}

			findings.append(
				DrcFinding::onElement(rule.identifier(),
						      rule.severity(),
						      text.arg(labelOf(element)),
						      element));
		}

		return findings;
	}
}

/**
	@brief DrcLinkRules::masterWithoutSlaveIdentifier
	@return the machine key of the rule. Never translated: the project
	file writes it, and a key in the language of whoever saved the file is
	a key nobody else can read.
*/
QString DrcLinkRules::masterWithoutSlaveIdentifier()
{
	return QStringLiteral("master_without_slave");
}

/**
	@brief DrcLinkRules::slaveWithoutMasterIdentifier
	@return the machine key of the rule
*/
QString DrcLinkRules::slaveWithoutMasterIdentifier()
{
	return QStringLiteral("slave_without_master");
}

/**
	@brief DrcLinkRules::terminalOutsideStripIdentifier
	@return the machine key of the rule
*/
QString DrcLinkRules::terminalOutsideStripIdentifier()
{
	return QStringLiteral("terminal_outside_strip");
}

/**
	@brief DrcLinkRules::masterWithoutSlave
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per master element linked to no contact
*/
QList<DrcFinding> DrcLinkRules::masterWithoutSlave(QETProject *project,
						   const DrcRule &rule)
{
	return unlinkedOf(project, rule, ElementData::Master,
			  QCoreApplication::translate(
				  "DrcLinkRules",
				  "L'élément maître %1 n'a aucun contact "
				  "esclave : sa référence croisée ne renvoie "
				  "nulle part."));
}

/**
	@brief DrcLinkRules::slaveWithoutMaster
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per slave contact linked to no master element
*/
QList<DrcFinding> DrcLinkRules::slaveWithoutMaster(QETProject *project,
						   const DrcRule &rule)
{
	return unlinkedOf(project, rule, ElementData::Slave,
			  QCoreApplication::translate(
				  "DrcLinkRules",
				  "Le contact esclave %1 n'est rattaché à "
				  "aucun élément maître : il n'apparaît sur "
				  "la référence croisée d'aucune bobine."));
}

/**
	@brief DrcLinkRules::terminalOutsideStrip
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per terminal that belongs to no strip
*/
QList<DrcFinding> DrcLinkRules::terminalOutsideStrip(QETProject *project,
						     const DrcRule &rule)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

	const ElementProvider provider(project);
	const QVector<TerminalElement *> loose = provider.freeTerminal();

	const QString text = QCoreApplication::translate(
				"DrcLinkRules",
				"La borne %1 n'appartient à aucun bornier : "
				"elle ne figurera sur aucun plan de bornier.");

	for (TerminalElement *terminal : loose)
	{
		if (!terminal) {
			continue;
		}

		findings.append(
			DrcFinding::onElement(rule.identifier(),
					      rule.severity(),
					      text.arg(labelOf(terminal)),
					      terminal));
	}

	return findings;
}

/**
	@brief DrcLinkRules::registerRules
	@param set : the register the three rules join
	@param engine : the engine that will walk them
	@return how many were taken

	The severities are the ones the rules are born with, and all three are
	warnings. Two reasons, and neither is taste: an error stops the
	command line of a checker with a non-zero exit code, and a drawing
	still being made would then fail the check for not being finished; and
	nobody has yet counted how many terminals of a delivered project sit
	outside a strip, so a rule that turned out to fire on every one of
	them would be an error nobody could clear. Raising any of the three
	afterwards is one line of the profile the project file carries.
*/
int DrcLinkRules::registerRules(DrcRuleSet &set, DrcScanEngine &engine)
{
	int taken = 0;

	if (engine.addRule(set,
			   DrcRule(masterWithoutSlaveIdentifier(),
				   QCoreApplication::translate(
					   "DrcLinkRules",
					   "Élément maître sans aucun contact "
					   "esclave"),
				   DrcSeverity::Warning,
				   DrcRoute::Scan),
			   &DrcLinkRules::masterWithoutSlave)) {
		++taken;
	}

	if (engine.addRule(set,
			   DrcRule(slaveWithoutMasterIdentifier(),
				   QCoreApplication::translate(
					   "DrcLinkRules",
					   "Contact esclave rattaché à aucun "
					   "élément maître"),
				   DrcSeverity::Warning,
				   DrcRoute::Scan),
			   &DrcLinkRules::slaveWithoutMaster)) {
		++taken;
	}

	if (engine.addRule(set,
			   DrcRule(terminalOutsideStripIdentifier(),
				   QCoreApplication::translate(
					   "DrcLinkRules",
					   "Borne dessinée hors de tout "
					   "bornier"),
				   DrcSeverity::Warning,
				   DrcRoute::Scan),
			   &DrcLinkRules::terminalOutsideStrip)) {
		++taken;
	}

	return taken;
}
