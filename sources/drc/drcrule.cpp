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
#include "drcrule.h"

#include <QCoreApplication>

/**
	@brief DrcRule::DrcRule
	The severity and the enabled state given here are both the current
	value and the default: a rule is born unmodified, and what the
	draughtsman changes afterwards is what the project file has to keep.
	@param identifier : machine key, mandatory
	@param description : the sentence shown to the user, already translated
	@param severity : how serious the finding is, by default
	@param route : where the rule finds its data
	@param enabled : whether the rule runs, by default
*/
DrcRule::DrcRule(const QString &identifier,
		 const QString &description,
		 DrcSeverity severity,
		 DrcRoute route,
		 bool enabled) :
	m_identifier(identifier),
	m_description(description),
	m_route(route),
	m_severity(severity),
	m_default_severity(severity),
	m_enabled(enabled),
	m_default_enabled(enabled)
{
}

/**
	@brief DrcRule::isModified
	@return whether somebody moved this rule away from what it was written
	with. This is what decides that a rule gets written to the project
	file: the ones nobody touched are not written, so a project does not
	carry a frozen copy of the factory settings.
*/
bool DrcRule::isModified() const
{
	return m_severity != m_default_severity || m_enabled != m_default_enabled;
}

/**
	@brief DrcRule::resetToDefault
	Puts the rule back to what it was written with. The identifier, the
	description and the route are not touched - they are not choices.
*/
void DrcRule::resetToDefault()
{
	m_severity = m_default_severity;
	m_enabled = m_default_enabled;
}

/**
	@brief DrcRule::operator==
	Two rules are the same when they say the same thing **and** stand in
	the same state, defaults included. The defaults are part of it on
	purpose: a round trip through the project file that lost them would
	silently start writing rules that nobody changed.
*/
bool DrcRule::operator==(const DrcRule &other) const
{
	return m_identifier == other.m_identifier
	       && m_description == other.m_description
	       && m_route == other.m_route
	       && m_severity == other.m_severity
	       && m_default_severity == other.m_default_severity
	       && m_enabled == other.m_enabled
	       && m_default_enabled == other.m_default_enabled;
}

bool DrcRule::operator!=(const DrcRule &other) const
{
	return !(*this == other);
}

/**
	@brief DrcRule::isAtLeast
	The single place where severities are compared. It exists so that the
	ordering is written once: the command line asks it whether anything
	reached Error before returning a non-zero exit code, and the export
	warning asks it the same question to count.
	@param severity : the severity of a finding
	@param threshold : the severity being asked about
	@return true when severity is as serious as threshold, or worse
*/
bool DrcRule::isAtLeast(DrcSeverity severity, DrcSeverity threshold)
{
	return static_cast<int>(severity) >= static_cast<int>(threshold);
}

/**
	@brief DrcRule::severityToString
	@return the machine name, which is what the project file stores. Not
	translated, and not the number: a project saved today has to keep
	meaning the same thing after the enumeration is reordered.
*/
QString DrcRule::severityToString(DrcSeverity severity)
{
	switch (severity) {
		case DrcSeverity::Information:
			return QStringLiteral("information");
		case DrcSeverity::Warning:
			return QStringLiteral("warning");
		case DrcSeverity::Error:
			return QStringLiteral("error");
	}
	return QStringLiteral("warning");
}

/**
	@brief DrcRule::severityFromString
	Reading is tolerant, as everywhere else in the project file: a name
	this version does not know - a severity added by a later one, or a
	typed-in file - falls back instead of throwing the rule away.
	@param string : what the project file held
	@param fallback : what an unknown name means
*/
DrcSeverity DrcRule::severityFromString(const QString &string,
					DrcSeverity fallback)
{
	if (string == QLatin1String("information")) {
		return DrcSeverity::Information;
	}
	if (string == QLatin1String("warning")) {
		return DrcSeverity::Warning;
	}
	if (string == QLatin1String("error")) {
		return DrcSeverity::Error;
	}
	return fallback;
}

/**
	@brief DrcRule::translatedSeverity
	@return the word the panel and the export warning show. Separate from
	severityToString on purpose: the day somebody translates one of these
	into the project file, every project written that day becomes
	unreadable by a machine in another language.
*/
QString DrcRule::translatedSeverity(DrcSeverity severity)
{
	switch (severity) {
		case DrcSeverity::Information:
			return QCoreApplication::translate("DrcRule",
				"Information");
		case DrcSeverity::Warning:
			return QCoreApplication::translate("DrcRule",
				"Avertissement");
		case DrcSeverity::Error:
			return QCoreApplication::translate("DrcRule",
				"Erreur");
	}
	return QString();
}

/**
	@brief DrcRule::routeToString
	@return the machine name of the route. Written to the project file
	along with the rest, and read back by routeFromString.
*/
QString DrcRule::routeToString(DrcRoute route)
{
	switch (route) {
		case DrcRoute::Sql:
			return QStringLiteral("sql");
		case DrcRoute::Scan:
			return QStringLiteral("scan");
	}
	return QStringLiteral("sql");
}

/**
	@brief DrcRule::routeFromString
	@param string : what was read
	@param fallback : what an unknown name means
*/
DrcRoute DrcRule::routeFromString(const QString &string, DrcRoute fallback)
{
	if (string == QLatin1String("sql")) {
		return DrcRoute::Sql;
	}
	if (string == QLatin1String("scan")) {
		return DrcRoute::Scan;
	}
	return fallback;
}
