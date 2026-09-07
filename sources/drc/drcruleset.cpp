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
#include "drcruleset.h"

/**
	@brief DrcRuleSet::add
	@param rule : the rule to register
	@return false, and changes nothing, when the rule has no identifier or
	when that identifier is already taken. Refusing is the point: replacing
	the first rule quietly would hand it the severity the draughtsman
	lowered for the other one.
*/
bool DrcRuleSet::add(const DrcRule &rule)
{
	if (!rule.isValid() || m_index.contains(rule.identifier())) {
		return false;
	}
	m_index.insert(rule.identifier(), m_rules.count());
	m_rules.append(rule);
	return true;
}

bool DrcRuleSet::contains(const QString &identifier) const
{
	return m_index.contains(identifier);
}

/**
	@brief DrcRuleSet::rule
	@param identifier : the key
	@return the rule, or a default-constructed one - whose isValid() is
	false - when nothing answers to that identifier. The caller asks
	isValid(), and the absent rule is not an empty rule that would then
	run.
*/
DrcRule DrcRuleSet::rule(const QString &identifier) const
{
	const auto it = m_index.constFind(identifier);
	if (it == m_index.constEnd()) {
		return DrcRule();
	}
	return m_rules.at(it.value());
}

/**
	@brief DrcRuleSet::enabledRules
	@return the rules that are switched on, in the order they were added,
	both routes together.
*/
QList<DrcRule> DrcRuleSet::enabledRules() const
{
	QList<DrcRule> list;
	for (const DrcRule &rule : m_rules) {
		if (rule.isEnabled()) {
			list.append(rule);
		}
	}
	return list;
}

/**
	@brief DrcRuleSet::enabledRules
	@param route : the engine asking
	@return only the rules of that route that are switched on. An engine
	never sees a rule of the other route, which is what keeps a rule from
	being run by whichever engine happens to come first.
*/
QList<DrcRule> DrcRuleSet::enabledRules(DrcRoute route) const
{
	QList<DrcRule> list;
	for (const DrcRule &rule : m_rules) {
		if (rule.isEnabled() && rule.route() == route) {
			list.append(rule);
		}
	}
	return list;
}

/**
	@brief DrcRuleSet::hasEnabledRule
	The guard both engines ask before doing any work. With no enabled rule
	of its route, an engine must not update the data base nor walk a
	single folio - that is what makes a switched-off rule cost nothing
	instead of costing everything but the report.
	@param route : the engine asking
*/
bool DrcRuleSet::hasEnabledRule(DrcRoute route) const
{
	for (const DrcRule &rule : m_rules) {
		if (rule.isEnabled() && rule.route() == route) {
			return true;
		}
	}
	return false;
}

/**
	@brief DrcRuleSet::modifiedRules
	@return the rules somebody moved away from what they were written
	with. This is the list the project file gets: what nobody touched is
	not written, so a project does not carry a frozen copy of the factory
	settings.
*/
QList<DrcRule> DrcRuleSet::modifiedRules() const
{
	QList<DrcRule> list;
	for (const DrcRule &rule : m_rules) {
		if (rule.isModified()) {
			list.append(rule);
		}
	}
	return list;
}

/**
	@brief DrcRuleSet::setSeverity
	@param identifier : the key
	@param severity : the severity to give it
	@return false when no rule answers to that identifier. A choice made
	about a rule that is not there has to be visible to the caller,
	because it comes from a project file written by another version.
*/
bool DrcRuleSet::setSeverity(const QString &identifier, DrcSeverity severity)
{
	const auto it = m_index.constFind(identifier);
	if (it == m_index.constEnd()) {
		return false;
	}
	m_rules[it.value()].setSeverity(severity);
	return true;
}

/**
	@brief DrcRuleSet::setEnabled
	@param identifier : the key
	@param enabled : whether the rule runs
	@return false when no rule answers to that identifier.
*/
bool DrcRuleSet::setEnabled(const QString &identifier, bool enabled)
{
	const auto it = m_index.constFind(identifier);
	if (it == m_index.constEnd()) {
		return false;
	}
	m_rules[it.value()].setEnabled(enabled);
	return true;
}

/**
	@brief DrcRuleSet::resetToDefault
	Puts every rule back to what it was written with. No rule is added or
	removed - the register is the same, only the choices are dropped.
*/
void DrcRuleSet::resetToDefault()
{
	for (DrcRule &rule : m_rules) {
		rule.resetToDefault();
	}
}
