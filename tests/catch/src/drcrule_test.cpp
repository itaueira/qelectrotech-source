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
#include <catch2/catch.hpp>

#include "../../../sources/drc/drcrule.h"
#include "../../../sources/drc/drcruleset.h"

/*
	The contract every design rule check is written against, and the
	register that holds them.

	Nothing runs a rule here, and that is the shape of the step: the
	contract has no engine yet. What can already be got wrong is silent in
	every case, which is why it is worth nailing before dozens of rules are
	written against it:

	- a severity comparison written by hand, so that "errors only" quietly
	  includes warnings, or quietly excludes errors;
	- the translated word written into the project file, which makes every
	  project saved that day unreadable in another language;
	- an unknown severity name throwing the rule away instead of falling
	  back, which is what happens the first time an older version opens a
	  newer project;
	- two rules sharing one identifier, so that the severity lowered for
	  one applies to the other and nobody can see it;
	- a route filter that lets an Sql rule reach the walk over the objects,
	  or the other way round - the very confusion DrcRoute exists to stop;
	- a switched-off route still doing work, which is the performance trap
	  waiting for the day the rule list is long.

	Labelled T23 and not CU-23.n: not one of the use cases of that
	specification is closed by this file. CU-23.1 measures how long writing
	a rule against this contract takes, and that is measured by a person
	writing one; the rest need a project open and live in C_uitests.
*/

TEST_CASE("T23 — a rule is born unmodified, and what is changed is remembered apart", "[t23][drc]")
{
	DrcRule rule(QStringLiteral("folio-titleblock-empty"),
		     QStringLiteral("Renseigner le cartouche du folio"),
		     DrcSeverity::Error,
		     DrcRoute::Sql);

	REQUIRE(rule.isValid());
	REQUIRE(rule.identifier() == QStringLiteral("folio-titleblock-empty"));
	REQUIRE(rule.route() == DrcRoute::Sql);
	REQUIRE(rule.severity() == DrcSeverity::Error);
	REQUIRE(rule.defaultSeverity() == DrcSeverity::Error);
	REQUIRE(rule.isEnabled());
	REQUIRE(rule.defaultEnabled());

		//Nothing was touched, so the project file has nothing to write
		//about this rule
	REQUIRE_FALSE(rule.isModified());

		//The draughtsman lowers it. The current value moves, the
		//judgement of whoever wrote the rule does not - it is what
		//resetToDefault and "write only what changed" both rest on
	rule.setSeverity(DrcSeverity::Warning);
	REQUIRE(rule.severity() == DrcSeverity::Warning);
	REQUIRE(rule.defaultSeverity() == DrcSeverity::Error);
	REQUIRE(rule.isModified());

	rule.resetToDefault();
	REQUIRE(rule.severity() == DrcSeverity::Error);
	REQUIRE_FALSE(rule.isModified());

		//Switching it off counts as a change too, and on its own: a rule
		//left at its own severity but turned off has to survive a save
	rule.setEnabled(false);
	REQUIRE(rule.isModified());
	REQUIRE_FALSE(rule.isEnabled());
	REQUIRE(rule.defaultEnabled());

	rule.resetToDefault();
	REQUIRE(rule.isEnabled());
	REQUIRE_FALSE(rule.isModified());

		//A rule written switched off is unmodified as well - the default
		//is what it was born with, not "on"
	DrcRule off(QStringLiteral("experimental"),
		    QStringLiteral("Règle en essai"),
		    DrcSeverity::Information,
		    DrcRoute::Scan,
		    false);
	REQUIRE_FALSE(off.isEnabled());
	REQUIRE_FALSE(off.defaultEnabled());
	REQUIRE_FALSE(off.isModified());

		//And no identifier is no rule: the register has nothing to key it
		//by, so it must not be storable
	DrcRule nameless;
	REQUIRE_FALSE(nameless.isValid());
}

TEST_CASE("T23 — the severity order is asked, never compared by hand", "[t23][drc]")
{
		//An error is at least an error, and at least a warning, and at
		//least an information. This is the question the command line asks
		//before returning a non-zero exit code
	REQUIRE(DrcRule::isAtLeast(DrcSeverity::Error, DrcSeverity::Error));
	REQUIRE(DrcRule::isAtLeast(DrcSeverity::Error, DrcSeverity::Warning));
	REQUIRE(DrcRule::isAtLeast(DrcSeverity::Error, DrcSeverity::Information));

		//A warning is not an error. Getting this one backwards is how a
		//project with nothing but warnings starts failing the CI
	REQUIRE_FALSE(DrcRule::isAtLeast(DrcSeverity::Warning, DrcSeverity::Error));
	REQUIRE(DrcRule::isAtLeast(DrcSeverity::Warning, DrcSeverity::Warning));
	REQUIRE(DrcRule::isAtLeast(DrcSeverity::Warning, DrcSeverity::Information));

		//And an information is not a warning. Getting this one backwards
		//is how a project full of notes stops being exportable
	REQUIRE_FALSE(DrcRule::isAtLeast(DrcSeverity::Information, DrcSeverity::Error));
	REQUIRE_FALSE(DrcRule::isAtLeast(DrcSeverity::Information, DrcSeverity::Warning));
	REQUIRE(DrcRule::isAtLeast(DrcSeverity::Information, DrcSeverity::Information));
}

TEST_CASE("T23 — severity and route survive the project file, and reading is tolerant", "[t23][drc]")
{
	const QList<DrcSeverity> severities{DrcSeverity::Information,
					    DrcSeverity::Warning,
					    DrcSeverity::Error};
	for (const DrcSeverity severity : severities) {
		const QString written = DrcRule::severityToString(severity);
		REQUIRE_FALSE(written.isEmpty());
		REQUIRE(DrcRule::severityFromString(written) == severity);
	}

	const QList<DrcRoute> routes{DrcRoute::Sql, DrcRoute::Scan};
	for (const DrcRoute route : routes) {
		const QString written = DrcRule::routeToString(route);
		REQUIRE_FALSE(written.isEmpty());
		REQUIRE(DrcRule::routeFromString(written) == route);
	}

		//A name this version does not know falls back instead of being
		//thrown away. It is what an older version reads out of a project
		//saved by a newer one, and the rule has to keep running
	REQUIRE(DrcRule::severityFromString(QStringLiteral("catastrophe"))
		== DrcSeverity::Warning);
	REQUIRE(DrcRule::severityFromString(QString()) == DrcSeverity::Warning);
	REQUIRE(DrcRule::severityFromString(QStringLiteral("catastrophe"),
					    DrcSeverity::Error)
		== DrcSeverity::Error);
	REQUIRE(DrcRule::routeFromString(QStringLiteral("telepathy"),
					 DrcRoute::Scan)
		== DrcRoute::Scan);

		//Reading is tolerant, and it is not sloppy: the machine name is
		//exact, so a severity written in another case is not silently a
		//different severity than the one meant
	REQUIRE(DrcRule::severityFromString(QStringLiteral("Error"),
					    DrcSeverity::Information)
		== DrcSeverity::Information);

		//The word shown and the word written are not the same word. If
		//they ever were, translating the interface would rewrite the
		//project file, and a project saved in Portuguese would not open
		//in French
	REQUIRE(DrcRule::translatedSeverity(DrcSeverity::Error)
		!= DrcRule::severityToString(DrcSeverity::Error));
	REQUIRE_FALSE(DrcRule::translatedSeverity(DrcSeverity::Error).isEmpty());
	REQUIRE_FALSE(DrcRule::translatedSeverity(DrcSeverity::Warning).isEmpty());
	REQUIRE_FALSE(DrcRule::translatedSeverity(DrcSeverity::Information).isEmpty());
	REQUIRE(DrcRule::translatedSeverity(DrcSeverity::Error)
		!= DrcRule::translatedSeverity(DrcSeverity::Warning));
}

TEST_CASE("T23 — an identifier belongs to one rule, and the register refuses the second", "[t23][drc]")
{
	DrcRuleSet set;
	REQUIRE(set.isEmpty());
	REQUIRE(set.count() == 0);

	REQUIRE(set.add(DrcRule(QStringLiteral("folio-titleblock-empty"),
				QStringLiteral("Renseigner le cartouche du folio"),
				DrcSeverity::Warning,
				DrcRoute::Sql)));
	REQUIRE(set.count() == 1);
	REQUIRE(set.contains(QStringLiteral("folio-titleblock-empty")));

		//The same identifier again, with a different sentence and a
		//different route. Accepting it would hand the second rule the
		//severity somebody lowered for the first, and the project file
		//has no way of telling the two apart
	REQUIRE_FALSE(set.add(DrcRule(QStringLiteral("folio-titleblock-empty"),
				      QStringLiteral("Autre chose"),
				      DrcSeverity::Error,
				      DrcRoute::Scan)));
	REQUIRE(set.count() == 1);
		//Refused means unchanged, not overwritten
	REQUIRE(set.rule(QStringLiteral("folio-titleblock-empty")).route()
		== DrcRoute::Sql);
	REQUIRE(set.rule(QStringLiteral("folio-titleblock-empty")).severity()
		== DrcSeverity::Warning);

		//A rule with no identifier has no key at all
	REQUIRE_FALSE(set.add(DrcRule()));
	REQUIRE(set.count() == 1);

		//An identifier nobody registered answers with an invalid rule,
		//and not with an empty one that would then be run
	const DrcRule absent = set.rule(QStringLiteral("nowhere"));
	REQUIRE_FALSE(absent.isValid());
	REQUIRE_FALSE(set.contains(QStringLiteral("nowhere")));

		//And a choice made about a rule that is not there is visible to
		//the caller: that is what comes out of a project file written by
		//another version
	REQUIRE_FALSE(set.setSeverity(QStringLiteral("nowhere"), DrcSeverity::Error));
	REQUIRE_FALSE(set.setEnabled(QStringLiteral("nowhere"), false));

		//The order is the order they were added, because that is the
		//order the panel lists them in
	REQUIRE(set.add(DrcRule(QStringLiteral("terminal-unconnected"),
				QStringLiteral("Borne sans raccordement"),
				DrcSeverity::Error,
				DrcRoute::Scan)));
	REQUIRE(set.add(DrcRule(QStringLiteral("element-label-missing"),
				QStringLiteral("Composant sans étiquette"),
				DrcSeverity::Warning,
				DrcRoute::Sql)));
	REQUIRE(set.count() == 3);
	REQUIRE(set.rules().at(0).identifier()
		== QStringLiteral("folio-titleblock-empty"));
	REQUIRE(set.rules().at(1).identifier()
		== QStringLiteral("terminal-unconnected"));
	REQUIRE(set.rules().at(2).identifier()
		== QStringLiteral("element-label-missing"));
}

TEST_CASE("T23 — each engine sees only its own route, and never the other one", "[t23][drc]")
{
	DrcRuleSet set;
	set.add(DrcRule(QStringLiteral("folio-titleblock-empty"),
			QStringLiteral("Renseigner le cartouche du folio"),
			DrcSeverity::Warning,
			DrcRoute::Sql));
	set.add(DrcRule(QStringLiteral("element-label-missing"),
			QStringLiteral("Composant sans étiquette"),
			DrcSeverity::Warning,
			DrcRoute::Sql));
	set.add(DrcRule(QStringLiteral("terminal-unconnected"),
			QStringLiteral("Borne sans raccordement"),
			DrcSeverity::Error,
			DrcRoute::Scan));

	REQUIRE(set.enabledRules().count() == 3);
	REQUIRE(set.enabledRules(DrcRoute::Sql).count() == 2);
	REQUIRE(set.enabledRules(DrcRoute::Scan).count() == 1);

		//The one rule the schema cannot answer must reach the walk over
		//the objects and nothing else. An Sql engine that got hold of it
		//would run a query, find no row, and report a clean project
	REQUIRE(set.enabledRules(DrcRoute::Scan).at(0).identifier()
		== QStringLiteral("terminal-unconnected"));
	for (const DrcRule &rule : set.enabledRules(DrcRoute::Sql)) {
		REQUIRE(rule.identifier() != QStringLiteral("terminal-unconnected"));
	}

	REQUIRE(set.hasEnabledRule(DrcRoute::Sql));
	REQUIRE(set.hasEnabledRule(DrcRoute::Scan));
}

TEST_CASE("T23 — a switched-off route gives its engine nothing to do", "[t23][drc]")
{
	DrcRuleSet set;
	set.add(DrcRule(QStringLiteral("folio-titleblock-empty"),
			QStringLiteral("Renseigner le cartouche du folio"),
			DrcSeverity::Warning,
			DrcRoute::Sql));
	set.add(DrcRule(QStringLiteral("element-label-missing"),
			QStringLiteral("Composant sans étiquette"),
			DrcSeverity::Warning,
			DrcRoute::Sql));
	set.add(DrcRule(QStringLiteral("terminal-unconnected"),
			QStringLiteral("Borne sans raccordement"),
			DrcSeverity::Error,
			DrcRoute::Scan));

		//Half the rules off. The ones that are off leave the list at
		//once - a rule filtered after running is a rule that cost its
		//whole price and produced no report
	REQUIRE(set.setEnabled(QStringLiteral("element-label-missing"), false));
	REQUIRE(set.enabledRules().count() == 2);
	REQUIRE(set.enabledRules(DrcRoute::Sql).count() == 1);
	REQUIRE(set.hasEnabledRule(DrcRoute::Sql));

		//The last Sql rule off, and the Sql engine has nothing to do at
		//all: no query to build, and no reason to update the data base
	REQUIRE(set.setEnabled(QStringLiteral("folio-titleblock-empty"), false));
	REQUIRE(set.enabledRules(DrcRoute::Sql).isEmpty());
	REQUIRE_FALSE(set.hasEnabledRule(DrcRoute::Sql));

		//The other route is untouched: switching off every rule of one
		//engine must not stop the other
	REQUIRE(set.hasEnabledRule(DrcRoute::Scan));
	REQUIRE(set.enabledRules(DrcRoute::Scan).count() == 1);

		//The register did not shrink. The rules are still there, still
		//listable in the panel, still switchable back on
	REQUIRE(set.count() == 3);
	REQUIRE(set.contains(QStringLiteral("element-label-missing")));
}

TEST_CASE("T23 — only what somebody changed is what the project file gets", "[t23][drc]")
{
	DrcRuleSet set;
	set.add(DrcRule(QStringLiteral("folio-titleblock-empty"),
			QStringLiteral("Renseigner le cartouche du folio"),
			DrcSeverity::Warning,
			DrcRoute::Sql));
	set.add(DrcRule(QStringLiteral("terminal-unconnected"),
			QStringLiteral("Borne sans raccordement"),
			DrcSeverity::Error,
			DrcRoute::Scan));
	set.add(DrcRule(QStringLiteral("element-label-missing"),
			QStringLiteral("Composant sans étiquette"),
			DrcSeverity::Warning,
			DrcRoute::Sql));

		//A project nobody configured writes nothing. This is what keeps
		//a saved project from freezing a copy of the factory settings
	REQUIRE(set.modifiedRules().isEmpty());

		//Two choices, of the two kinds there are
	REQUIRE(set.setSeverity(QStringLiteral("terminal-unconnected"),
				DrcSeverity::Warning));
	REQUIRE(set.setEnabled(QStringLiteral("element-label-missing"), false));

	const QList<DrcRule> modified = set.modifiedRules();
	REQUIRE(modified.count() == 2);
	REQUIRE(modified.at(0).identifier() == QStringLiteral("terminal-unconnected"));
	REQUIRE(modified.at(0).severity() == DrcSeverity::Warning);
	REQUIRE(modified.at(0).defaultSeverity() == DrcSeverity::Error);
	REQUIRE(modified.at(1).identifier() == QStringLiteral("element-label-missing"));
	REQUIRE_FALSE(modified.at(1).isEnabled());

		//Setting a rule back to the value it was written with removes it
		//from the list: an undone choice is not a choice to preserve
	REQUIRE(set.setSeverity(QStringLiteral("terminal-unconnected"),
				DrcSeverity::Error));
	REQUIRE(set.modifiedRules().count() == 1);

		//And dropping every choice empties it, without losing a rule
	set.resetToDefault();
	REQUIRE(set.modifiedRules().isEmpty());
	REQUIRE(set.count() == 3);
	REQUIRE(set.enabledRules().count() == 3);
}

TEST_CASE("T23 — two rules that say the same thing in the same state are the same rule", "[t23][drc]")
{
	const DrcRule a(QStringLiteral("folio-titleblock-empty"),
			QStringLiteral("Renseigner le cartouche du folio"),
			DrcSeverity::Error,
			DrcRoute::Sql);
	DrcRule b(QStringLiteral("folio-titleblock-empty"),
		  QStringLiteral("Renseigner le cartouche du folio"),
		  DrcSeverity::Error,
		  DrcRoute::Sql);

	REQUIRE(a == b);
	REQUIRE_FALSE(a != b);

	b.setSeverity(DrcSeverity::Warning);
	REQUIRE(a != b);

		//Back to the same current value, and equal again - because the
		//default came along unchanged. Had the round trip through the
		//project file lost the default, this would stay different, and
		//the rule would be written to every project from then on
	b.setSeverity(DrcSeverity::Error);
	REQUIRE(a == b);
	REQUIRE(a.isModified() == b.isModified());
}
