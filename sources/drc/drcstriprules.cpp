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
#include "drcstriprules.h"

#include "drcruleset.h"
#include "drcscanengine.h"
#include "drcstripnumbers.h"

#include "../TerminalStrip/GraphicsItem/terminalstripdrawerhelpers.h"
#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QCoreApplication>
#include <QSharedPointer>
#include <QStringList>
#include <QVector>

namespace
{
	/**
		The symbol @a level is drawn as, when it is drawn on a folio.

		Null for a level with no symbol, and for one whose symbol was
		deleted from its folio: see the description of DrcStripRules for
		why neither takes part in anything.
	*/
	Element *drawnSymbolOf(const QSharedPointer<RealTerminal> &level)
	{
		if (level.isNull()) {
			return nullptr;
		}
		Element *element = level->element();
		if (!element || !element->diagram()) {
			return nullptr;
		}
		return element;
	}

	/// Whether any terminal of @a strip is drawn on a folio.
	bool hasDrawnTerminal(const TerminalStrip *strip)
	{
		const QVector<QSharedPointer<RealTerminal>> levels =
				strip->realTerminals();
		for (const QSharedPointer<RealTerminal> &level : levels)
		{
			if (drawnSymbolOf(level)) {
				return true;
			}
		}
		return false;
	}

	/// @a strip as a sentence names it.
	QString nameOf(const TerminalStrip *strip)
	{
		return DrcStripNumbers::stripName(strip->installation(),
						  strip->location(),
						  strip->name());
	}
}

/**
	@brief DrcStripRules::duplicateTerminalNumberIdentifier
	@return the machine key of the rule. Never translated: the project file
	writes it.
*/
QString DrcStripRules::duplicateTerminalNumberIdentifier()
{
	return QStringLiteral("duplicate_terminal_number");
}

/**
	@brief DrcStripRules::emptyStripIdentifier
	@return the machine key of the rule
*/
QString DrcStripRules::emptyStripIdentifier()
{
	return QStringLiteral("empty_terminal_strip");
}

/**
	@brief DrcStripRules::duplicateStripNameIdentifier
	@return the machine key of the rule
*/
QString DrcStripRules::duplicateStripNameIdentifier()
{
	return QStringLiteral("duplicate_terminal_strip_name");
}

/**
	@brief DrcStripRules::duplicateTerminalNumbers
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per number carried by more than one terminal of a
	strip
*/
QList<DrcFinding> DrcStripRules::duplicateTerminalNumbers(QETProject *project,
							  const DrcRule &rule)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

		//The strips in the order the project holds them, which is the
		//order the file saves them in and the order the tree lists them.
	const QVector<TerminalStrip *> strips = project->terminalStrip();
	for (TerminalStrip *strip : strips)
	{
		if (!strip) {
			continue;
		}

		QList<Element *> drawn;
		QList<DrcStripNumbers::Mark> marks;

			//The rail as it stands: the order of the vector is the order
			//of the plan, so the first member of a group is the terminal
			//nearest the start of the rail.
		const QVector<QSharedPointer<PhysicalTerminal>> rail =
				strip->physicalTerminal();
		for (int place = 0 ; place < rail.count() ; ++place)
		{
			const QSharedPointer<PhysicalTerminal> &physical = rail.at(place);
			if (physical.isNull()) {
				continue;
			}

				//Back to front, the order of the levels: realTerminals()
				//is indexed by level.
			const QVector<QSharedPointer<RealTerminal>> levels =
					physical->realTerminals();
			for (const QSharedPointer<RealTerminal> &level : levels)
			{
				Element *symbol = drawnSymbolOf(level);
				if (!symbol) {
					continue;
				}

				DrcStripNumbers::Mark mark;
					//The index in the rail, which is what
					//PhysicalTerminal::pos() answers - asked here without
					//a search of the vector for every terminal.
				mark.physical = place;
					//The very function the plan draws the number with, so
					//that the number compared is the number printed.
				mark.number = TerminalStripDrawer::Helpers::terminalNumber(
						      level->label());
				if (DrcStripNumbers::key(mark.number).isEmpty()) {
					continue;
				}

				drawn << symbol;
				marks << mark;
			}
		}

		const QList<QList<int>> groups = DrcStripNumbers::groups(marks);
		if (groups.isEmpty()) {
			continue;
		}

		const QString strip_name = nameOf(strip);
		for (const QList<int> &group : groups)
		{
			QList<DrcStripNumbers::Mark> members;
			for (int index : group)
			{
					//The folio and the cell are written by the function
					//that addresses every finding of the checker, and only
					//for the terminals that end up in a sentence: the
					//sentence and the columns of the panel cannot disagree.
				DrcStripNumbers::Mark member = marks.at(index);
				DrcFinding probe = DrcFinding::onElement(rule.identifier(),
									 rule.severity(),
									 QString(),
									 drawn.at(index));
				DrcScanEngine::locate(project, probe);
				member.folio = probe.folio();
				member.cell = probe.position();
				members << member;
			}

			const QString sentence =
					DrcStripNumbers::duplicateText(strip_name, members);
			if (sentence.isEmpty()) {
				continue;
			}

			findings.append(DrcFinding::onElement(rule.identifier(),
							      rule.severity(),
							      sentence,
							      drawn.at(group.first())));
		}
	}

	return findings;
}

/**
	@brief DrcStripRules::emptyStrips
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per strip with no terminal drawn on a folio
*/
QList<DrcFinding> DrcStripRules::emptyStrips(QETProject *project,
					     const DrcRule &rule)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

	const QVector<TerminalStrip *> strips = project->terminalStrip();
	for (TerminalStrip *strip : strips)
	{
		if (!strip || hasDrawnTerminal(strip)) {
			continue;
		}

			//About the project, and not about a folio: a strip is drawn on
			//none, and sending the reader to folio 1 would be the lie
			//DrcTargetKind::None exists to avoid.
		findings.append(DrcFinding::onProject(
					rule.identifier(),
					rule.severity(),
					DrcStripNumbers::emptyStripText(nameOf(strip))));
	}

	return findings;
}

/**
	@brief DrcStripRules::duplicateStripNames
	@param project : the project to walk
	@param rule : the rule as it stands
	@return one finding per name carried by more than one strip
*/
QList<DrcFinding> DrcStripRules::duplicateStripNames(QETProject *project,
						     const DrcRule &rule)
{
	QList<DrcFinding> findings;
	if (!project) {
		return findings;
	}

	QList<TerminalStrip *> named;
	QStringList keys;
	const QVector<TerminalStrip *> strips = project->terminalStrip();
	for (TerminalStrip *strip : strips)
	{
		if (!strip) {
			continue;
		}
		named << strip;
		keys << DrcStripNumbers::stripKey(strip->installation(),
						  strip->location(),
						  strip->name());
	}

	const QList<QList<int>> groups = DrcStripNumbers::sameNameGroups(keys);
	for (const QList<int> &group : groups)
	{
		const QString sentence = DrcStripNumbers::sameNameText(
						 nameOf(named.at(group.first())),
						 int(group.count()));
		if (sentence.isEmpty()) {
			continue;
		}

		findings.append(DrcFinding::onProject(rule.identifier(),
						      rule.severity(),
						      sentence));
	}

	return findings;
}

/**
	@brief DrcStripRules::registerRules
	@param set : the register the rules join
	@param engine : the engine that will walk them
	@return how many were taken

	All three born switched on. The repeated number is an error and the
	other two are warnings: see the header for why.
*/
int DrcStripRules::registerRules(DrcRuleSet &set, DrcScanEngine &engine)
{
	int taken = 0;

	if (engine.addRule(set,
			   DrcRule(duplicateTerminalNumberIdentifier(),
				   QCoreApplication::translate(
					   "DrcStripRules",
					   "Two terminals of a strip with the "
					   "same number"),
				   DrcSeverity::Error,
				   DrcRoute::Scan),
			   &DrcStripRules::duplicateTerminalNumbers)) {
		++taken;
	}

	if (engine.addRule(set,
			   DrcRule(emptyStripIdentifier(),
				   QCoreApplication::translate(
					   "DrcStripRules",
					   "Terminal strip with no terminal"),
				   DrcSeverity::Warning,
				   DrcRoute::Scan),
			   &DrcStripRules::emptyStrips)) {
		++taken;
	}

	if (engine.addRule(set,
			   DrcRule(duplicateStripNameIdentifier(),
				   QCoreApplication::translate(
					   "DrcStripRules",
					   "Two terminal strips with the same "
					   "name"),
				   DrcSeverity::Warning,
				   DrcRoute::Scan),
			   &DrcStripRules::duplicateStripNames)) {
		++taken;
	}

	return taken;
}
