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
#ifndef DRCRULESET_H
#define DRCRULESET_H

#include "drcrule.h"

#include <QHash>
#include <QList>
#include <QString>

/**
	@brief The rules a verification run knows about, keyed by identifier.

	It holds the one invariant the contract has, and it holds it in a
	single place so that the call sites cannot disagree about it: **an
	identifier belongs to one rule**. The identifier is the key the project
	file writes the lowered severity under, so two rules sharing one would
	make a choice made about the first silently apply to the second, and
	the draughtsman would have no way of seeing it. add() refuses the
	duplicate and answers false rather than replacing quietly.

	It also holds the answer to "does this engine have anything to do".
	Both engines - the query over the project data base and the walk over
	the objects - ask hasEnabledRule() for their own route before doing any
	work at all, so a route whose rules are all switched off costs nothing:
	no query built, no data base update, no folio walked. Written once
	here instead of twice there, because a guard duplicated is a guard that
	will disagree with itself.

	No rule of its own lives here, and no execution: this is the register,
	not the engine. What an Sql rule runs and what a Scan rule walks belong
	to the two engines, for the reason DrcRoute states.

	The order is the order the rules were added. That is the order the
	panel lists them in, and it is worth being stable: a list that
	reshuffles between runs reads as if the project had changed.
*/
class DrcRuleSet
{
	public:
		DrcRuleSet() {}

		bool add(const DrcRule &rule);

		int count() const {return m_rules.count();}
		bool isEmpty() const {return m_rules.isEmpty();}
		bool contains(const QString &identifier) const;

		DrcRule rule(const QString &identifier) const;
		QList<DrcRule> rules() const {return m_rules;}

		QList<DrcRule> enabledRules() const;
		QList<DrcRule> enabledRules(DrcRoute route) const;
		bool hasEnabledRule(DrcRoute route) const;

		QList<DrcRule> modifiedRules() const;

		bool setSeverity(const QString &identifier, DrcSeverity severity);
		bool setEnabled(const QString &identifier, bool enabled);
		void resetToDefault();

	private:
		QList<DrcRule> m_rules;
		/// identifier -> position in m_rules, so lookup does not scan
		QHash<QString, int> m_index;
};

#endif // DRCRULESET_H
