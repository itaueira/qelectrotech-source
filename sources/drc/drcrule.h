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
#ifndef DRCRULE_H
#define DRCRULE_H

#include <QString>

/**
	@brief How serious a finding is.

	Ordered on purpose, and the order is the point: a caller that wants
	"errors only" asks isAtLeast(severity, DrcSeverity::Error), and the
	command line turns "is there anything at least as serious as an error"
	into its exit code. Never compare the underlying numbers by hand - the
	names below are what gets written to the project file, and the numbers
	are free to change.
*/
enum class DrcSeverity
{
	Information = 0,
	Warning = 1,
	Error = 2
};

/**
	@brief Where a rule finds the data it needs.

	Two routes, and a rule declares which one it uses instead of the engine
	guessing. The reason is measured, not stylistic: the terminal table of
	the project data base is filled only along the conductor path, so a
	terminal with no wire never reaches it. A rule about unconnected
	terminals written as SQL would run, return no row, and report a clean
	project - which is worse than not having the rule at all.

	- Sql: the answer is a query over the tables the project data base
	  derives from the XML (diagram_info, element_info, element,
	  conductor);
	- Scan: the answer needs the objects in memory - connectivity,
	  potential, links, cross references, and anything else the schema
	  does not hold.

	This enumeration carries no payload. What an Sql rule runs and what a
	Scan rule walks have nothing in common, so they belong to the two
	engines and not here; a single check() shared by both would be the very
	guess this declaration exists to prevent.
*/
enum class DrcRoute
{
	Sql,
	Scan
};

/**
	@brief What every design rule check declares about itself.

	Identifier, sentence, severity, whether it is on, and the route it
	needs. Nothing else, and no rule of its own: this is the contract the
	rules are written against, dozens of times over, and the measure of
	whether the contract is right is how long writing one takes.

	The two pairs of values deserve a word, because they are what the
	project file is made of. A rule is born with a severity and an enabled
	state, and those two are its **default** - the judgement of whoever
	wrote it. The draughtsman may lower an error to a warning or switch a
	rule off, and that is the **current** value. isModified() is the
	difference between the two, and the reason it exists is the project
	file: only what somebody changed on purpose gets written, so a project
	is not filled with a copy of the factory settings that would then
	freeze the day a rule changes its own default.

	The identifier is what survives everything else. It goes into the
	project file as the key of the choice above, so it is machine text -
	never translated, never shown - while the description is the sentence
	the panel prints, and it should say what to do, not merely what is
	wrong.

	The route is fixed at construction and cannot be changed afterwards.
	The data lives where it lives; a rule that could switch routes would be
	a rule nobody could tell was looking in the wrong place.
*/
class DrcRule
{
	public:
		DrcRule() {}
		DrcRule(const QString &identifier,
			const QString &description,
			DrcSeverity severity,
			DrcRoute route,
			bool enabled = true);

		/// Machine key, never translated: the project file writes it.
		QString identifier() const {return m_identifier;}
		/// The sentence the panel prints. Translated by whoever writes it.
		QString description() const {return m_description;}
		/// Which engine runs this rule. Fixed at construction.
		DrcRoute route() const {return m_route;}

		DrcSeverity severity() const {return m_severity;}
		void setSeverity(DrcSeverity severity) {m_severity = severity;}
		/// The severity the rule was written with.
		DrcSeverity defaultSeverity() const {return m_default_severity;}

		bool isEnabled() const {return m_enabled;}
		void setEnabled(bool enabled) {m_enabled = enabled;}
		/// Whether the rule was written switched on.
		bool defaultEnabled() const {return m_default_enabled;}

		bool isModified() const;
		void resetToDefault();

		/// A rule with no identifier has no key, so it is not a rule.
		bool isValid() const {return !m_identifier.isEmpty();}

		bool operator==(const DrcRule &other) const;
		bool operator!=(const DrcRule &other) const;

		/// Whether this severity is at least as serious as the threshold.
		static bool isAtLeast(DrcSeverity severity, DrcSeverity threshold);

		static QString severityToString(DrcSeverity severity);
		static DrcSeverity severityFromString(
				const QString &string,
				DrcSeverity fallback = DrcSeverity::Warning);
		static QString translatedSeverity(DrcSeverity severity);

		static QString routeToString(DrcRoute route);
		static DrcRoute routeFromString(const QString &string,
						DrcRoute fallback = DrcRoute::Sql);

	private:
		QString m_identifier;
		QString m_description;
		DrcRoute m_route = DrcRoute::Sql;
		DrcSeverity m_severity = DrcSeverity::Warning;
		DrcSeverity m_default_severity = DrcSeverity::Warning;
		bool m_enabled = true;
		bool m_default_enabled = true;
};

#endif // DRCRULE_H
