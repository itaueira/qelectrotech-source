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
#include "drcfinding.h"

#include <QCoreApplication>

/*
	No element.h, no conductor.h and no diagram.h here, and that is not an
	omission: this file only ever stores and hands back the pointers, so
	the forward declarations of the header are enough. It is what lets the
	finding - including the case that has no target at all - be compiled
	and proved in the pure suite, which does not link the program.
*/

/**
	@brief DrcFinding::onElement
	@param rule_identifier : the machine key of the rule that found it
	@param severity : the severity the rule carried at that moment
	@param text : what is wrong, translated, without the place
	@param element : what to select on the sheet
	@return the finding. The sheet is **not** read off the element here -
	that would need its class, and with it the whole program - so whoever
	builds the finding calls setDiagram() as well. The scan engine does it
	for every finding a rule hands back, in one place.
*/
DrcFinding DrcFinding::onElement(const QString &rule_identifier,
				 DrcSeverity severity,
				 const QString &text,
				 Element *element)
{
	DrcFinding finding;
	finding.m_rule_identifier = rule_identifier;
	finding.m_severity = severity;
	finding.m_text = text;
	finding.m_target_kind = DrcTargetKind::Element;
	finding.m_element = element;
	return finding;
}

/**
	@brief DrcFinding::onConductor
	@param rule_identifier : the machine key of the rule that found it
	@param severity : the severity the rule carried at that moment
	@param text : what is wrong, translated, without the place
	@param conductor : what to select on the sheet
	@return the finding
*/
DrcFinding DrcFinding::onConductor(const QString &rule_identifier,
				   DrcSeverity severity,
				   const QString &text,
				   Conductor *conductor)
{
	DrcFinding finding;
	finding.m_rule_identifier = rule_identifier;
	finding.m_severity = severity;
	finding.m_text = text;
	finding.m_target_kind = DrcTargetKind::Conductor;
	finding.m_conductor = conductor;
	return finding;
}

/**
	@brief DrcFinding::onDiagram
	@param rule_identifier : the machine key of the rule that found it
	@param severity : the severity the rule carried at that moment
	@param text : what is wrong, translated, without the place
	@param diagram : the sheet to open
	@return the finding
*/
DrcFinding DrcFinding::onDiagram(const QString &rule_identifier,
				 DrcSeverity severity,
				 const QString &text,
				 Diagram *diagram)
{
	DrcFinding finding;
	finding.m_rule_identifier = rule_identifier;
	finding.m_severity = severity;
	finding.m_text = text;
	finding.m_target_kind = DrcTargetKind::Diagram;
	finding.m_diagram = diagram;
	return finding;
}

/**
	@brief DrcFinding::onProject
	@param rule_identifier : the machine key of the rule that found it
	@param severity : the severity the rule carried at that moment
	@param text : what is wrong, translated
	@return a finding with nothing to navigate to, said out loud
*/
DrcFinding DrcFinding::onProject(const QString &rule_identifier,
				 DrcSeverity severity,
				 const QString &text)
{
	DrcFinding finding;
	finding.m_rule_identifier = rule_identifier;
	finding.m_severity = severity;
	finding.m_text = text;
	finding.m_target_kind = DrcTargetKind::None;
	return finding;
}

/**
	@brief DrcFinding::isValid
	@return whether this finding says something. A default built one does
	not, and neither does one whose rule forgot its sentence.
*/
bool DrcFinding::isValid() const
{
	return !m_rule_identifier.isEmpty() && !m_text.isEmpty();
}

/**
	@brief DrcFinding::hasTarget
	@return whether there is an object to go to. The kind and the pointer
	are tested together so that no caller has to test the second one after
	having read the first.
*/
bool DrcFinding::hasTarget() const
{
	switch (m_target_kind)
	{
		case DrcTargetKind::Element:
			return m_element != nullptr;
		case DrcTargetKind::Conductor:
			return m_conductor != nullptr;
		case DrcTargetKind::Diagram:
			return m_diagram != nullptr;
		case DrcTargetKind::None:
			return false;
	}
	return false;
}

/**
	@brief DrcFinding::setFolio
	@param folio : the number printed on the sheet, 1-based

	A number below one is not a folio, and it is kept out rather than
	stored: hasFolio() then answers the question once, and the panel never
	has to decide what a zero meant.
*/
void DrcFinding::setFolio(int folio)
{
	m_folio = folio > 0 ? folio : 0;
}

/**
	@brief DrcFinding::location
	@return where the finding is, in words, or an empty string when
	nothing is known about where it is - which is the honest answer for a
	finding about the project as a whole.
*/
QString DrcFinding::location() const
{
	if (hasFolio() && !m_position.isEmpty())
	{
		return QCoreApplication::translate("DrcFinding",
						   "folio %1, repère %2")
		       .arg(m_folio).arg(m_position);
	}
	if (hasFolio())
	{
		return QCoreApplication::translate("DrcFinding", "folio %1")
		       .arg(m_folio);
	}
	if (!m_position.isEmpty())
	{
		return QCoreApplication::translate("DrcFinding", "repère %1")
		       .arg(m_position);
	}
	return QString();
}

/**
	@brief DrcFinding::describe
	@return the sentence and the place in one line, for a log or for the
	command line. The panel does not use it: it has a column for each
	piece, and repeating the place inside the sentence is what makes such
	a table unreadable.
*/
QString DrcFinding::describe() const
{
	const QString where = location();
	if (where.isEmpty()) {
		return m_text;
	}
		//Not a translated string: "%1 (%2)" carries no words, and a
		//translator shown it out of context has nothing to decide and
		//every chance of dropping one of the two markers. The words are
		//in text() and in location(), which are translated where they are
		//written.
	return QStringLiteral("%1 (%2)").arg(m_text, where);
}

/**
	@brief DrcFinding::countAtLeast
	@param findings : the findings of a run
	@param threshold : the severity to count from
	@return how many of them are at least that serious
*/
int DrcFinding::countAtLeast(const QList<DrcFinding> &findings,
			     DrcSeverity threshold)
{
	int count = 0;
	for (const DrcFinding &finding : findings)
	{
		if (DrcRule::isAtLeast(finding.severity(), threshold)) {
			++count;
		}
	}
	return count;
}

/**
	@brief DrcFinding::hasAtLeast
	@param findings : the findings of a run
	@param threshold : the severity to test against
	@return whether any of them is at least that serious
*/
bool DrcFinding::hasAtLeast(const QList<DrcFinding> &findings,
			    DrcSeverity threshold)
{
	for (const DrcFinding &finding : findings)
	{
		if (DrcRule::isAtLeast(finding.severity(), threshold)) {
			return true;
		}
	}
	return false;
}
